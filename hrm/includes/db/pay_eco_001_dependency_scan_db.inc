<?php
/** PAY-ECO-001 deterministic dependency-integrity scan writer. */
include_once($path_to_root.'/includes/pay_eco_001_contract.inc');
include_once(dirname(__FILE__).'/pay_eco_001_sandbox_db.inc');

function pay_eco_001_dependency_lock_inventory($pack_version_id,&$error)
{
    $error=null;
    $lock=pay_eco_001_onboarding_exact_row('SELECT dependency_lock_id,pack_version_id,lock_sha256 FROM '.TB_PREF.'pay_ctry_dependency_locks WHERE pack_version_id='.(int)$pack_version_id.' LIMIT 2',$error,false);
    if($lock===false){if($error==='onboarding_state_missing')$error='dependency_scan_lock_required';return false;}
    if(!pay_eco_001_sha((string)$lock['lock_sha256']))return pay_eco_001_fail($error,'dependency_scan_lock_digest_invalid');
    $q=db_query('SELECT m.ordinal_no,m.dependency_pack_code,m.required_version,m.resolved_pack_version_id,m.member_sha256,p.pack_code AS resolved_pack_code,v.version AS resolved_version,v.publisher_code FROM '.TB_PREF.'pay_ctry_dependency_lock_members m INNER JOIN '.TB_PREF.'pay_ctry_pack_versions v ON v.pack_version_id=m.resolved_pack_version_id INNER JOIN '.TB_PREF.'pay_ctry_packs p ON p.pack_id=v.pack_id WHERE m.dependency_lock_id='.(int)$lock['dependency_lock_id'].' ORDER BY m.ordinal_no,m.dependency_lock_member_id');
    if(!$q)return pay_eco_001_fail($error,'dependency_scan_member_query_failed');
    $members=array();$lockPayload=array();$ord=0;
    while($r=db_fetch_assoc($q)){
        $ord++;if((int)$r['ordinal_no']!==$ord)return pay_eco_001_fail($error,'dependency_scan_member_ordinal_invalid');
        if(!pay_eco_001_token((string)$r['dependency_pack_code'],64)||!pay_eco_001_token((string)$r['required_version'],64)||!pay_eco_001_positive_id($r['resolved_pack_version_id'])||!pay_eco_001_sha((string)$r['member_sha256']))return pay_eco_001_fail($error,'dependency_scan_member_invalid');
        if((string)$r['dependency_pack_code']!==(string)$r['resolved_pack_code']||(string)$r['required_version']!==(string)$r['resolved_version'])return pay_eco_001_fail($error,'dependency_scan_resolution_mismatch');
        $payload=array('pack_id'=>(string)$r['dependency_pack_code'],'version'=>(string)$r['required_version'],'resolved_pack_version_id'=>(int)$r['resolved_pack_version_id']);
        if(!hash_equals(pay_eco_001_hash($payload),(string)$r['member_sha256']))return pay_eco_001_fail($error,'dependency_scan_member_digest_mismatch');
        $lockPayload[]=$payload;$members[]=array('ordinal_no'=>$ord,'pack_code'=>(string)$r['dependency_pack_code'],'version'=>(string)$r['required_version'],'resolved_pack_version_id'=>(int)$r['resolved_pack_version_id'],'publisher_code'=>(string)$r['publisher_code'],'member_sha256'=>(string)$r['member_sha256']);
    }
    $expected=pay_eco_001_hash(array('lock_version'=>'dependency-lock-v1','pack_version_id'=>(int)$pack_version_id,'members'=>$lockPayload));
    if(!hash_equals($expected,(string)$lock['lock_sha256']))return pay_eco_001_fail($error,'dependency_scan_lock_integrity_invalid');
    return array('dependency_lock_id'=>(int)$lock['dependency_lock_id'],'lock_sha256'=>(string)$lock['lock_sha256'],'members'=>$members,'inventory_sha256'=>pay_eco_001_hash($members));
}

function pay_eco_001_dependency_member_findings($inventory,$now_sql,&$error)
{
    $error=null;$codes=array();$accepted=0;$revoked=0;$not_effective=0;$trust_members=array();$now_epoch=strtotime((string)$now_sql.' UTC');
    if($now_epoch===false)return pay_eco_001_fail($error,'dependency_scan_time_invalid');
    foreach($inventory['members'] as $member){
        $vr=pay_eco_001_onboarding_exact_row('SELECT verification_report_id,report_sha256 FROM '.TB_PREF.'pay_ctry_verification_reports WHERE pack_version_id='.(int)$member['resolved_pack_version_id']." AND final_status='accepted' ORDER BY verification_report_id DESC LIMIT 1",$error,true);
        if($vr===false)return false;
        $verification_id=null;$verification_sha=null;
        if(!is_array($vr)||!pay_eco_001_sha((string)$vr['report_sha256'])){$codes[]='resolved_dependency_verification_missing';}
        else{$accepted++;$verification_id=(int)$vr['verification_report_id'];$verification_sha=(string)$vr['report_sha256'];}
        $pub=pay_eco_001_onboarding_exact_row('SELECT publisher_id,identity_sha256,valid_from,valid_to FROM '.TB_PREF.'pay_ctry_publishers WHERE publisher_code='.pay_ctry_001_sql_hex($member['publisher_code']).' LIMIT 2',$error,true);if($pub===false)return false;
        $publisher_id=null;$publisher_identity=null;$publisher_valid_from=null;$publisher_valid_to=null;$publisher_validity_state='missing';$revocation_id=null;$revocation_sha=null;
        if(!is_array($pub)||!pay_eco_001_sha((string)$pub['identity_sha256'])){$codes[]='resolved_dependency_publisher_missing';}
        else{
            $publisher_id=(int)$pub['publisher_id'];$publisher_identity=(string)$pub['identity_sha256'];$publisher_valid_from=(string)$pub['valid_from'];$publisher_valid_to=$pub['valid_to']===null?null:(string)$pub['valid_to'];
            $publisher_validity_state=pay_eco_001_dependency_publisher_validity_state($publisher_valid_from,$publisher_valid_to,$now_epoch);
            if($publisher_validity_state!=='effective'){$not_effective++;$codes[]='resolved_dependency_publisher_not_effective';}
            $rev=pay_eco_001_onboarding_exact_row('SELECT publisher_revocation_id,revocation_sha256 FROM '.TB_PREF.'pay_ctry_publisher_revocations WHERE publisher_id='.$publisher_id.' AND effective_at<='.pay_ctry_001_sql_hex($now_sql).' ORDER BY publisher_revocation_id DESC LIMIT 1',$error,true);if($rev===false)return false;
            if(is_array($rev)){
                if(!pay_eco_001_sha((string)$rev['revocation_sha256']))return pay_eco_001_fail($error,'dependency_scan_revocation_digest_invalid');
                $revoked++;$codes[]='resolved_dependency_publisher_revoked';$revocation_id=(int)$rev['publisher_revocation_id'];$revocation_sha=(string)$rev['revocation_sha256'];
            }
        }
        $trust_members[]=array(
            'ordinal_no'=>(int)$member['ordinal_no'],'member_sha256'=>(string)$member['member_sha256'],'resolved_pack_version_id'=>(int)$member['resolved_pack_version_id'],
            'accepted_verification_report_id'=>$verification_id,'accepted_verification_report_sha256'=>$verification_sha,
            'publisher_id'=>$publisher_id,'publisher_identity_sha256'=>$publisher_identity,'publisher_valid_from'=>$publisher_valid_from,'publisher_valid_to'=>$publisher_valid_to,'publisher_validity_state'=>$publisher_validity_state,
            'effective_revocation_id'=>$revocation_id,'effective_revocation_sha256'=>$revocation_sha
        );
    }
    sort($codes,SORT_STRING);
    return array('dependency_result'=>count($codes)?'failed':'passed','finding_codes'=>$codes,'dependency_count'=>count($inventory['members']),'accepted_dependency_count'=>$accepted,'revoked_dependency_count'=>$revoked,'not_effective_dependency_publisher_count'=>$not_effective,'trust_members'=>$trust_members,'vulnerability_result'=>'incomplete','vulnerability_finding'=>'vulnerability_intelligence_unconfigured');
}

function pay_eco_001_write_dependency_scan($request,&$error,$now_epoch=null)
{
    $error=null;if(!pay_eco_001_validate_dependency_scan_request($request,$error))return false;
    $actor=pay_eco_001_onboarding_actor_id();$now_epoch=$now_epoch===null?time():(int)$now_epoch;
    if($actor<=0||$now_epoch<=0||!pay_eco_001_onboarding_access(PAY_ECO_001_DEPENDENCY_SCAN_PERMISSION))return pay_eco_001_fail($error,'dependency_scan_authority_denied');
    if(!pay_eco_001_onboarding_owned_transaction($error))return false;$ok=false;$out=false;
    do{
        $candidate=pay_eco_001_onboarding_exact_row('SELECT c.catalog_candidate_id,c.publisher_profile_id,c.pack_version_id,c.candidate_code,c.compatibility_sha256,c.certification_sha256,c.support_sha256,c.vulnerability_sha256,c.revocation_state_sha256,c.sandbox_profile_sha256,c.submitted_by,c.submitted_at,c.candidate_sha256 FROM '.TB_PREF.'pay_eco_001_pack_candidates c WHERE c.catalog_candidate_id='.(int)$request['catalog_candidate_id'].' LIMIT 2 FOR UPDATE',$error,false);if($candidate===false)break;
        $candidatePayload=$candidate;$candidatePayload['publisher_profile_id']=(int)$candidatePayload['publisher_profile_id'];$candidatePayload['pack_version_id']=(int)$candidatePayload['pack_version_id'];$candidatePayload['submitted_by']=(int)$candidatePayload['submitted_by'];$candidatePayload['submitted_at']=gmdate('Y-m-d\\TH:i:s\\Z',strtotime((string)$candidatePayload['submitted_at'].' UTC'));
        if(pay_eco_001_time($candidatePayload['submitted_at'])===false||!pay_eco_001_validate_pack_candidate($candidatePayload,$error)){$error='dependency_scan_candidate_integrity_invalid';break;}
        $verification=pay_eco_001_onboarding_exact_row('SELECT verification_report_id,report_sha256 FROM '.TB_PREF.'pay_ctry_verification_reports WHERE pack_version_id='.(int)$candidate['pack_version_id']." AND final_status='accepted' ORDER BY verification_report_id DESC LIMIT 1",$error,false);if($verification===false){if($error==='onboarding_state_missing')$error='dependency_scan_accepted_verification_required';break;}if(!pay_eco_001_sha((string)$verification['report_sha256'])){$error='dependency_scan_accepted_verification_invalid';break;}
        $sandbox=pay_eco_001_onboarding_exact_row('SELECT sandbox_evaluation_id,catalog_candidate_id,sandbox_profile_sha256,conformance_suite_sha256,manifest_sha256,semantic_root,evidence_root,artifact_inventory_sha256,result,findings_sha256,evaluated_by,evaluated_at,evaluation_sha256 FROM '.TB_PREF.'pay_eco_001_sandbox_evaluations WHERE catalog_candidate_id='.(int)$candidate['catalog_candidate_id'].' AND sandbox_profile_sha256='.pay_ctry_001_sql_hex(pay_eco_001_sandbox_profile_sha256())." AND result='passed' ORDER BY sandbox_evaluation_id DESC LIMIT 1",$error,false);if($sandbox===false){if($error==='onboarding_state_missing')$error='dependency_scan_passed_sandbox_required';break;}
        $sandboxPayload=$sandbox;unset($sandboxPayload['sandbox_evaluation_id']);$sandboxPayload['catalog_candidate_id']=(int)$sandboxPayload['catalog_candidate_id'];$sandboxPayload['evaluated_by']=(int)$sandboxPayload['evaluated_by'];$sandboxPayload['evaluated_at']=gmdate('Y-m-d\TH:i:s\Z',strtotime((string)$sandboxPayload['evaluated_at'].' UTC'));if(!pay_eco_001_validate_sandbox_evaluation($sandboxPayload,$error)||!hash_equals(pay_eco_001_sandbox_suite_sha256(),(string)$sandboxPayload['conformance_suite_sha256'])){$error='dependency_scan_sandbox_integrity_invalid';break;}
        $inventory=pay_eco_001_dependency_lock_inventory((int)$candidate['pack_version_id'],$error);if($inventory===false)break;
        $now=gmdate('Y-m-d H:i:s',$now_epoch);$findings=pay_eco_001_dependency_member_findings($inventory,$now,$error);if($findings===false)break;
        $trustSnapshot=pay_eco_001_hash(array('candidate_verification_report_id'=>(int)$verification['verification_report_id'],'candidate_verification_report_sha256'=>(string)$verification['report_sha256'],'sandbox_evaluation_id'=>(int)$sandbox['sandbox_evaluation_id'],'sandbox_evaluation_sha256'=>(string)$sandbox['evaluation_sha256'],'dependency_members'=>$findings['trust_members']));
        $iso=pay_eco_001_onboarding_now_iso($now_epoch);$overall=$findings['dependency_result']==='failed'?'failed':'incomplete';
        $scan=array('catalog_candidate_id'=>(int)$candidate['catalog_candidate_id'],'sandbox_evaluation_id'=>(int)$sandbox['sandbox_evaluation_id'],'dependency_lock_id'=>(int)$inventory['dependency_lock_id'],'scan_profile_sha256'=>pay_eco_001_dependency_scan_profile_sha256(),'scan_suite_sha256'=>pay_eco_001_dependency_scan_suite_sha256(),'dependency_lock_sha256'=>(string)$inventory['lock_sha256'],'dependency_inventory_sha256'=>(string)$inventory['inventory_sha256'],'dependency_trust_snapshot_sha256'=>$trustSnapshot,'publisher_vulnerability_evidence_sha256'=>(string)$candidate['vulnerability_sha256'],'dependency_result'=>(string)$findings['dependency_result'],'vulnerability_result'=>'incomplete','result'=>$overall,'findings_sha256'=>pay_eco_001_hash($findings),'scanned_by'=>$actor,'scanned_at'=>$iso,'scan_sha256'=>'');
        $scan['scan_sha256']=pay_eco_001_dependency_scan_sha256($scan);if(!pay_eco_001_validate_dependency_scan($scan,$error))break;
        $existing=pay_eco_001_onboarding_exact_row('SELECT * FROM '.TB_PREF.'pay_eco_001_dependency_scans WHERE catalog_candidate_id='.(int)$scan['catalog_candidate_id'].' AND scan_profile_sha256='.pay_ctry_001_sql_hex($scan['scan_profile_sha256']).' AND dependency_trust_snapshot_sha256='.pay_ctry_001_sql_hex($scan['dependency_trust_snapshot_sha256']).' LIMIT 2 FOR UPDATE',$error,true);if($existing===false)break;
        if(is_array($existing)){foreach(array('sandbox_evaluation_id','dependency_lock_id','scan_suite_sha256','dependency_lock_sha256','dependency_inventory_sha256','dependency_trust_snapshot_sha256','publisher_vulnerability_evidence_sha256','dependency_result','vulnerability_result','result','findings_sha256') as $field)if((string)$existing[$field]!== (string)$scan[$field]){$error='dependency_scan_replay_conflict';break 2;}$ok=true;$out=array('dependency_scan_id'=>(int)$existing['dependency_scan_id'],'result'=>(string)$existing['result'],'scan_sha256'=>(string)$existing['scan_sha256'],'replayed'=>true);break;}
        $sql='INSERT INTO '.TB_PREF.'pay_eco_001_dependency_scans (catalog_candidate_id,sandbox_evaluation_id,dependency_lock_id,scan_profile_sha256,scan_suite_sha256,dependency_lock_sha256,dependency_inventory_sha256,dependency_trust_snapshot_sha256,publisher_vulnerability_evidence_sha256,dependency_result,vulnerability_result,result,findings_sha256,scanned_by,scanned_at,scan_sha256,created_at) VALUES ('.(int)$scan['catalog_candidate_id'].','.(int)$scan['sandbox_evaluation_id'].','.(int)$scan['dependency_lock_id'].','.pay_ctry_001_sql_hex($scan['scan_profile_sha256']).','.pay_ctry_001_sql_hex($scan['scan_suite_sha256']).','.pay_ctry_001_sql_hex($scan['dependency_lock_sha256']).','.pay_ctry_001_sql_hex($scan['dependency_inventory_sha256']).','.pay_ctry_001_sql_hex($scan['dependency_trust_snapshot_sha256']).','.pay_ctry_001_sql_hex($scan['publisher_vulnerability_evidence_sha256']).','.pay_ctry_001_sql_hex($scan['dependency_result']).','.pay_ctry_001_sql_hex($scan['vulnerability_result']).','.pay_ctry_001_sql_hex($scan['result']).','.pay_ctry_001_sql_hex($scan['findings_sha256']).','.$actor.','.pay_ctry_001_sql_hex($now).','.pay_ctry_001_sql_hex($scan['scan_sha256']).','.pay_ctry_001_sql_hex($now).')';
        if(!db_query($sql)){$error='dependency_scan_insert_failed';break;}$id=pay_ctry_001_insert_id();if($id<=0){$error='dependency_scan_identity_missing';break;}
        if(!pay_eco_001_onboarding_audit('pay_eco_dependency_scan',array('dependency_scan_id'=>$id),'pay.eco.dependency_scan.appended','dependency_integrity_scanned',array('catalog_candidate_id'=>(int)$scan['catalog_candidate_id'],'result'=>$scan['result'],'dependency_result'=>$scan['dependency_result'],'vulnerability_result'=>'incomplete','vulnerability_intelligence_unconfigured'=>true,'dependency_vulnerability_scanning'=>false,'signed_reproducible_artifact_admission'=>false,'marketplace_publication'=>false,'pack_installation'=>false,'company_activation'=>false,'payroll_execution'=>false,'historical_payroll_accounting_rows_rewritten'=>0),$actor,$error))break;
        $ok=true;$out=array('dependency_scan_id'=>$id,'result'=>$scan['result'],'scan_sha256'=>$scan['scan_sha256'],'replayed'=>false);
    }while(false);
    if($ok)commit_transaction();else cancel_transaction();return $ok?$out:false;
}
?>
