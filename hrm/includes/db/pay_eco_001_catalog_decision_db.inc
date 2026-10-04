<?php
/** PAY-ECO-001 catalog decision governance writer. */
include_once($path_to_root.'/includes/pay_eco_001_contract.inc');
include_once(dirname(__FILE__).'/pay_eco_001_onboarding_db.inc');

function pay_eco_001_catalog_decision_iso($sql_value,&$error)
{
    $epoch=strtotime((string)$sql_value.' UTC');
    if($epoch===false)return pay_eco_001_fail($error,'catalog_decision_time_state_invalid');
    return gmdate('Y-m-d\TH:i:s\Z',$epoch);
}

/**
 * Append one immutable catalog-review decision. The candidate submitter is the maker,
 * the current trusted operator is the checker, and accepted artifact/vulnerability
 * evidence is revalidated server-side. This function never publishes or installs.
 */
function pay_eco_001_write_catalog_decision($request,&$error,$now_epoch=null)
{
    $error=null;if(!pay_eco_001_validate_catalog_decision_request($request,$error))return false;
    $checker=pay_eco_001_onboarding_actor_id();$now_epoch=$now_epoch===null?time():(int)$now_epoch;
    if($checker<=0||$now_epoch<=0||!pay_eco_001_onboarding_access(PAY_ECO_001_CATALOG_DECISION_PERMISSION))return pay_eco_001_fail($error,'catalog_decision_authority_denied');
    if(!pay_eco_001_onboarding_owned_transaction($error))return false;$ok=false;$out=false;
    do{
        $candidate=pay_eco_001_onboarding_exact_row(
            'SELECT c.catalog_candidate_id,c.publisher_profile_id,c.pack_version_id,c.candidate_code,c.compatibility_sha256,c.certification_sha256,c.support_sha256,c.vulnerability_sha256,c.revocation_state_sha256,c.sandbox_profile_sha256,c.submitted_by,c.submitted_at,c.candidate_sha256,ep.publisher_id,p.identity_sha256 AS publisher_identity_sha256,p.valid_from,p.valid_to,v.signer_code,v.key_id,v.signed_at '.
            'FROM '.TB_PREF.'pay_eco_001_pack_candidates c INNER JOIN '.TB_PREF.'pay_eco_001_publisher_profiles ep ON ep.publisher_profile_id=c.publisher_profile_id INNER JOIN '.TB_PREF.'pay_ctry_publishers p ON p.publisher_id=ep.publisher_id INNER JOIN '.TB_PREF.'pay_ctry_pack_versions v ON v.pack_version_id=c.pack_version_id WHERE c.catalog_candidate_id='.(int)$request['catalog_candidate_id'].' LIMIT 2 FOR UPDATE',$error,false);
        if($candidate===false)break;
        $maker=(int)$candidate['submitted_by'];if($maker<=0||$maker===$checker){$error='catalog_decision_separation_required';break;}
        $candidatePayload=array('publisher_profile_id'=>(int)$candidate['publisher_profile_id'],'pack_version_id'=>(int)$candidate['pack_version_id'],'candidate_code'=>(string)$candidate['candidate_code'],'compatibility_sha256'=>(string)$candidate['compatibility_sha256'],'certification_sha256'=>(string)$candidate['certification_sha256'],'support_sha256'=>(string)$candidate['support_sha256'],'vulnerability_sha256'=>(string)$candidate['vulnerability_sha256'],'revocation_state_sha256'=>(string)$candidate['revocation_state_sha256'],'sandbox_profile_sha256'=>(string)$candidate['sandbox_profile_sha256'],'submitted_by'=>$maker,'submitted_at'=>pay_eco_001_catalog_decision_iso($candidate['submitted_at'],$error),'candidate_sha256'=>(string)$candidate['candidate_sha256']);
        if($candidatePayload['submitted_at']===false||!pay_eco_001_validate_pack_candidate($candidatePayload,$error)){$error='catalog_decision_candidate_integrity_invalid';break;}
        $from=strtotime((string)$candidate['valid_from'].' UTC');$to=$candidate['valid_to']===null?null:strtotime((string)$candidate['valid_to'].' UTC');
        if($from===false||($candidate['valid_to']!==null&&$to===false)||$from>$now_epoch||($to!==null&&$to<$now_epoch)){$error='catalog_decision_publisher_not_effective';break;}
        $revocation=pay_eco_001_onboarding_exact_row('SELECT publisher_revocation_id FROM '.TB_PREF.'pay_ctry_publisher_revocations WHERE publisher_id='.(int)$candidate['publisher_id'].' LIMIT 2 FOR UPDATE',$error,true);if($revocation===false)break;if(is_array($revocation)){$error='catalog_decision_publisher_revoked';break;}
        $signing=pay_eco_001_onboarding_exact_row('SELECT s.signer_id,k.trust_key_id,k.valid_from,k.valid_to,k.key_sha256 FROM '.TB_PREF.'pay_ctry_signers s INNER JOIN '.TB_PREF.'pay_ctry_trust_keys k ON k.signer_id=s.signer_id WHERE s.publisher_id='.(int)$candidate['publisher_id'].' AND s.signer_code='.pay_ctry_001_sql_hex((string)$candidate['signer_code']).' AND k.key_id='.pay_ctry_001_sql_hex((string)$candidate['key_id']).' LIMIT 2 FOR UPDATE',$error,false);if($signing===false)break;
        if(!pay_eco_001_sha((string)$candidate['publisher_identity_sha256'])||!pay_eco_001_sha((string)$signing['key_sha256'])){$error='catalog_decision_signing_lineage_invalid';break;}
        $signedEpoch=strtotime((string)$candidate['signed_at'].' UTC');$keyFrom=strtotime((string)$signing['valid_from'].' UTC');$keyTo=$signing['valid_to']===null?null:strtotime((string)$signing['valid_to'].' UTC');if($signedEpoch===false||$keyFrom===false||($signing['valid_to']!==null&&$keyTo===false)||$keyFrom>$signedEpoch||($keyTo!==null&&$keyTo<$signedEpoch)){$error='catalog_decision_signing_key_time_invalid';break;}
        $keyRevocation=pay_eco_001_onboarding_exact_row('SELECT key_revocation_id FROM '.TB_PREF.'pay_ctry_trust_key_revocations WHERE trust_key_id='.(int)$signing['trust_key_id'].' LIMIT 2 FOR UPDATE',$error,true);if($keyRevocation===false)break;if(is_array($keyRevocation)){$error='catalog_decision_signing_key_revoked';break;}
        $currentTrustSnapshot=pay_eco_001_hash(array('publisher_id'=>(int)$candidate['publisher_id'],'publisher_identity_sha256'=>(string)$candidate['publisher_identity_sha256'],'publisher_valid_from'=>(string)$candidate['valid_from'],'publisher_valid_to'=>$candidate['valid_to']===null?null:(string)$candidate['valid_to'],'publisher_effective'=>true,'publisher_revoked'=>false,'trust_key_id'=>(int)$signing['trust_key_id'],'key_sha256'=>(string)$signing['key_sha256'],'key_revoked'=>false));
        $admission=pay_eco_001_onboarding_exact_row(
            'SELECT aa.artifact_admission_id,aa.trust_snapshot_sha256,aa.vulnerability_correlation_id,aa.result,aa.admission_sha256,vc.vulnerability_snapshot_id,vc.result AS vulnerability_result,vc.correlation_sha256,vs.snapshot_sha256,vs.issued_at,vs.expires_at '.
            'FROM '.TB_PREF.'pay_eco_001_artifact_admissions aa INNER JOIN '.TB_PREF.'pay_eco_001_vulnerability_correlations vc ON vc.vulnerability_correlation_id=aa.vulnerability_correlation_id INNER JOIN '.TB_PREF.'pay_eco_001_vulnerability_snapshots vs ON vs.vulnerability_snapshot_id=vc.vulnerability_snapshot_id WHERE aa.catalog_candidate_id='.(int)$candidate['catalog_candidate_id']." AND aa.result='admitted' AND vc.result='passed' ORDER BY aa.artifact_admission_id DESC LIMIT 1 FOR UPDATE",$error,false);
        if($admission===false){if($error==='onboarding_state_missing')$error='catalog_decision_artifact_admission_required';break;}
        if(!pay_eco_001_sha((string)$admission['trust_snapshot_sha256'])||!pay_eco_001_sha((string)$admission['snapshot_sha256'])||!pay_eco_001_sha((string)$admission['admission_sha256'])||!pay_eco_001_sha((string)$admission['correlation_sha256'])){$error='catalog_decision_admission_integrity_invalid';break;}
        if(!hash_equals((string)$admission['trust_snapshot_sha256'],(string)$currentTrustSnapshot)){$error='catalog_decision_trust_snapshot_drift';break;}
        $issued=strtotime((string)$admission['issued_at'].' UTC');$expires=strtotime((string)$admission['expires_at'].' UTC');if($issued===false||$expires===false||$issued>$now_epoch||$expires<$now_epoch){$error='catalog_decision_vulnerability_snapshot_not_current';break;}
        $nowSql=gmdate('Y-m-d H:i:s',$now_epoch);$currentSnapshot=pay_eco_001_onboarding_exact_row('SELECT vulnerability_snapshot_id,snapshot_sha256 FROM '.TB_PREF.'pay_eco_001_vulnerability_snapshots WHERE issued_at<='.pay_ctry_001_sql_hex($nowSql).' AND expires_at>='.pay_ctry_001_sql_hex($nowSql).' ORDER BY issued_at DESC,vulnerability_snapshot_id DESC LIMIT 1 FOR UPDATE',$error,false);if($currentSnapshot===false){if($error==='onboarding_state_missing')$error='catalog_decision_current_vulnerability_snapshot_required';break;}
        if((int)$currentSnapshot['vulnerability_snapshot_id']!==(int)$admission['vulnerability_snapshot_id']||!hash_equals((string)$currentSnapshot['snapshot_sha256'],(string)$admission['snapshot_sha256'])){$error='catalog_decision_stale_vulnerability_correlation';break;}
        $existing=pay_eco_001_onboarding_exact_row('SELECT * FROM '.TB_PREF.'pay_eco_001_catalog_decisions WHERE catalog_candidate_id='.(int)$candidate['catalog_candidate_id'].' LIMIT 2 FOR UPDATE',$error,true);if($existing===false)break;
        if(is_array($existing)){
            if((string)$existing['decision']!==(string)$request['decision']||(string)$existing['decision_reason_sha256']!==(string)$request['decision_reason_sha256']||(int)$existing['maker_id']!==$maker||(int)$existing['checker_id']!==$checker||(string)$existing['trust_snapshot_sha256']!==(string)$admission['trust_snapshot_sha256']||(string)$existing['certification_snapshot_sha256']!==(string)$candidate['certification_sha256']||(string)$existing['compatibility_snapshot_sha256']!==(string)$candidate['compatibility_sha256']||(string)$existing['vulnerability_snapshot_sha256']!==(string)$admission['snapshot_sha256']){$error='catalog_decision_replay_conflict';break;}
            $ok=true;$out=array('catalog_decision_id'=>(int)$existing['catalog_decision_id'],'decision_sha256'=>(string)$existing['decision_sha256'],'replayed'=>true);break;
        }
        $iso=pay_eco_001_onboarding_now_iso($now_epoch);$decision=array('catalog_candidate_id'=>(int)$candidate['catalog_candidate_id'],'decision'=>(string)$request['decision'],'trust_snapshot_sha256'=>(string)$admission['trust_snapshot_sha256'],'certification_snapshot_sha256'=>(string)$candidate['certification_sha256'],'compatibility_snapshot_sha256'=>(string)$candidate['compatibility_sha256'],'vulnerability_snapshot_sha256'=>(string)$admission['snapshot_sha256'],'decision_reason_sha256'=>(string)$request['decision_reason_sha256'],'maker_id'=>$maker,'checker_id'=>$checker,'decided_at'=>$iso,'decision_sha256'=>'');$decision['decision_sha256']=pay_eco_001_catalog_decision_sha256($decision);if(!pay_eco_001_validate_catalog_decision($decision,$error))break;
        $now=gmdate('Y-m-d H:i:s',$now_epoch);$sql='INSERT INTO '.TB_PREF.'pay_eco_001_catalog_decisions (catalog_candidate_id,decision,trust_snapshot_sha256,certification_snapshot_sha256,compatibility_snapshot_sha256,vulnerability_snapshot_sha256,decision_reason_sha256,maker_id,checker_id,decided_at,decision_sha256,created_at) VALUES ('.(int)$decision['catalog_candidate_id'].','.pay_ctry_001_sql_hex($decision['decision']).','.pay_ctry_001_sql_hex($decision['trust_snapshot_sha256']).','.pay_ctry_001_sql_hex($decision['certification_snapshot_sha256']).','.pay_ctry_001_sql_hex($decision['compatibility_snapshot_sha256']).','.pay_ctry_001_sql_hex($decision['vulnerability_snapshot_sha256']).','.pay_ctry_001_sql_hex($decision['decision_reason_sha256']).','.$maker.','.$checker.','.pay_ctry_001_sql_hex($now).','.pay_ctry_001_sql_hex($decision['decision_sha256']).','.pay_ctry_001_sql_hex($now).')';
        if(!db_query($sql)){$error='catalog_decision_insert_failed';break;}$id=pay_ctry_001_insert_id();if($id<=0){$error='catalog_decision_identity_missing';break;}
        if(!pay_eco_001_onboarding_audit('pay_eco_catalog_decision',array('catalog_decision_id'=>$id),'pay.eco.catalog_decision.appended','catalog_review_decision_recorded',array('catalog_candidate_id'=>(int)$decision['catalog_candidate_id'],'artifact_admission_id'=>(int)$admission['artifact_admission_id'],'decision'=>(string)$decision['decision'],'maker_id'=>$maker,'checker_id'=>$checker,'decision_sha256'=>(string)$decision['decision_sha256'],'marketplace_publication'=>false,'pack_installation'=>false,'company_activation'=>false,'external_network_delivery'=>false,'payroll_execution'=>false,'historical_payroll_accounting_rows_rewritten'=>0),$checker,$error))break;
        $ok=true;$out=array('catalog_decision_id'=>$id,'decision_sha256'=>$decision['decision_sha256'],'replayed'=>false);
    }while(false);
    if($ok)commit_transaction();else cancel_transaction();
    return $ok?$out:false;
}
?>
