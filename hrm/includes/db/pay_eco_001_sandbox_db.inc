<?php
/** PAY-ECO-001 declarative-only sandbox/conformance evaluation writer. */
include_once($path_to_root.'/includes/pay_eco_001_contract.inc');
include_once(dirname(__FILE__).'/pay_eco_001_onboarding_db.inc');

/** Read and validate the immutable artifact inventory for one accepted pack version. */
function pay_eco_001_sandbox_artifact_inventory($pack_version_id,&$error)
{
    $error=null;
    $q=db_query('SELECT ordinal_no,normalized_path,artifact_type,media_type,byte_length,sha256,is_evidence FROM '.TB_PREF.'pay_ctry_artifacts WHERE pack_version_id='.(int)$pack_version_id.' ORDER BY ordinal_no,artifact_id LIMIT '.(int)PAY_CTRY_001_MAX_MEMBERS);
    if(!$q)return pay_eco_001_fail($error,'sandbox_artifact_query_failed');
    $rows=array();$total=0;$types=pay_ctry_001_allowed_artifact_types();$suffixes=pay_ctry_001_allowed_suffixes();$ord=0;$paths=array();
    while($r=db_fetch_assoc($q)){
        $ord++;if($ord>PAY_CTRY_001_MAX_MEMBERS-1)return pay_eco_001_fail($error,'sandbox_artifact_count_exceeded');
        if((int)$r['ordinal_no']!==$ord)return pay_eco_001_fail($error,'sandbox_artifact_ordinal_invalid');
        $path=(string)$r['normalized_path'];$pe=null;$normalized=pay_ctry_001_normalize_member_path($path,$pe);
        if($normalized===false||$normalized!==$path||substr($path,-1)==='/'||(strpos($path,'artifacts/')!==0&&strpos($path,'evidence/')!==0))return pay_eco_001_fail($error,'sandbox_artifact_path_invalid');
        $path_key=strtolower($path);if(isset($paths[$path_key]))return pay_eco_001_fail($error,'sandbox_artifact_path_duplicate');$paths[$path_key]=true;
        $type=(string)$r['artifact_type'];$media=(string)$r['media_type'];$suffix=strtolower(pathinfo($path,PATHINFO_EXTENSION));
        if(!isset($types[$type])||!in_array($media,$types[$type],true)||!in_array($suffix,$suffixes,true))return pay_eco_001_fail($error,'sandbox_artifact_type_media_denied');
        $bytes=(int)$r['byte_length'];if($bytes<0||$bytes>PAY_CTRY_001_MAX_MEMBER_BYTES||!pay_eco_001_sha((string)$r['sha256']))return pay_eco_001_fail($error,'sandbox_artifact_digest_or_size_invalid');
        if($total>PAY_CTRY_001_MAX_TOTAL_MEMBER_BYTES-$bytes)return pay_eco_001_fail($error,'sandbox_artifact_total_size_exceeded');$total+=$bytes;
        $is_evidence=(int)$r['is_evidence'];$expected_evidence=pay_ctry_001_is_evidence_type($type)||strpos($path,'evidence/')===0?1:0;
        if(($is_evidence!==0&&$is_evidence!==1)||$is_evidence!==$expected_evidence)return pay_eco_001_fail($error,'sandbox_artifact_evidence_flag_invalid');
        $rows[]=array('ordinal_no'=>(int)$r['ordinal_no'],'path'=>$path,'type'=>$type,'media_type'=>$media,'bytes'=>$bytes,'sha256'=>(string)$r['sha256'],'is_evidence'=>$is_evidence);
    }
    if($ord<1)return pay_eco_001_fail($error,'sandbox_artifact_inventory_missing');
    return array('count'=>$ord,'bytes'=>$total,'sha256'=>pay_eco_001_hash($rows));
}

/** Build deterministic minimized findings for the core-owned declarative sandbox profile. */
function pay_eco_001_sandbox_findings($candidate,$inventory)
{
    $profile=pay_eco_001_sandbox_profile();$expected=pay_eco_001_sandbox_profile_sha256();
    $profileMatch=is_array($candidate)&&isset($candidate['sandbox_profile_sha256'])&&hash_equals($expected,(string)$candidate['sandbox_profile_sha256']);
    return array(
        'result'=>$profileMatch?'passed':'failed',
        'finding_code'=>$profileMatch?'none':'sandbox_profile_mismatch',
        'profile_sha256'=>$expected,
        'suite_sha256'=>pay_eco_001_sandbox_suite_sha256(),
        'artifact_inventory_sha256'=>(string)$inventory['sha256'],
        'artifact_count'=>(int)$inventory['count'],
        'artifact_bytes'=>(int)$inventory['bytes'],
        'declarative_only'=>$profile['declarative_only']===true,
        'partner_executable_code'=>$profile['partner_executable_code']===true,
        'network_access'=>$profile['network_access']===true,
        'filesystem_write'=>$profile['filesystem_write']===true,
        'database_write'=>$profile['database_write']===true,
        'process_spawn'=>$profile['process_spawn']===true,
        'production_data_access'=>$profile['production_data_access']===true
    );
}

/**
 * Append one deterministic sandbox/conformance evaluation for an immutable candidate.
 * This inspects registered declarative artifact metadata only; it never loads or executes partner code,
 * opens network connections, writes pack files, mutates payroll, or grants catalog/install authority.
 */
function pay_eco_001_write_sandbox_evaluation($request,&$error,$now_epoch=null)
{
    $error=null;if(!pay_eco_001_validate_sandbox_evaluation_request($request,$error))return false;
    $actor=pay_eco_001_onboarding_actor_id();$now_epoch=$now_epoch===null?time():(int)$now_epoch;
    if($actor<=0||$now_epoch<=0||!pay_eco_001_onboarding_access(PAY_ECO_001_SANDBOX_EVALUATION_PERMISSION))return pay_eco_001_fail($error,'sandbox_evaluation_authority_denied');
    if(!pay_eco_001_onboarding_owned_transaction($error))return false;$ok=false;$out=false;
    do{
        $candidate=pay_eco_001_onboarding_exact_row('SELECT c.catalog_candidate_id,c.publisher_profile_id,c.pack_version_id,c.candidate_code,c.compatibility_sha256,c.certification_sha256,c.support_sha256,c.vulnerability_sha256,c.revocation_state_sha256,c.sandbox_profile_sha256,c.submitted_by,c.submitted_at,c.candidate_sha256,v.manifest_sha256,v.semantic_root,v.evidence_root FROM '.TB_PREF.'pay_eco_001_pack_candidates c INNER JOIN '.TB_PREF.'pay_ctry_pack_versions v ON v.pack_version_id=c.pack_version_id WHERE c.catalog_candidate_id='.(int)$request['catalog_candidate_id'].' LIMIT 2 FOR UPDATE',$error,false);if($candidate===false)break;
        $candidate_payload=array('publisher_profile_id'=>(int)$candidate['publisher_profile_id'],'pack_version_id'=>(int)$candidate['pack_version_id'],'candidate_code'=>(string)$candidate['candidate_code'],'compatibility_sha256'=>(string)$candidate['compatibility_sha256'],'certification_sha256'=>(string)$candidate['certification_sha256'],'support_sha256'=>(string)$candidate['support_sha256'],'vulnerability_sha256'=>(string)$candidate['vulnerability_sha256'],'revocation_state_sha256'=>(string)$candidate['revocation_state_sha256'],'sandbox_profile_sha256'=>(string)$candidate['sandbox_profile_sha256'],'submitted_by'=>(int)$candidate['submitted_by'],'submitted_at'=>gmdate('Y-m-d\\TH:i:s\\Z',strtotime((string)$candidate['submitted_at'].' UTC')),'candidate_sha256'=>(string)$candidate['candidate_sha256']);
        if(pay_eco_001_time($candidate_payload['submitted_at'])===false||!pay_eco_001_validate_pack_candidate($candidate_payload,$error)){$error='sandbox_candidate_integrity_invalid';break;}
        $verification=pay_eco_001_onboarding_exact_row('SELECT verification_report_id,report_sha256 FROM '.TB_PREF.'pay_ctry_verification_reports WHERE pack_version_id='.(int)$candidate['pack_version_id']." AND final_status='accepted' ORDER BY verification_report_id DESC LIMIT 1",$error,false);if($verification===false){if($error==='onboarding_state_missing')$error='sandbox_accepted_verification_required';break;}if(!pay_eco_001_sha((string)$verification['report_sha256'])){$error='sandbox_verification_digest_invalid';break;}
        $inventory=pay_eco_001_sandbox_artifact_inventory((int)$candidate['pack_version_id'],$error);if($inventory===false)break;
        $findings=pay_eco_001_sandbox_findings($candidate,$inventory);$iso=pay_eco_001_onboarding_now_iso($now_epoch);
        $evaluation=array(
            'catalog_candidate_id'=>(int)$candidate['catalog_candidate_id'],
            'sandbox_profile_sha256'=>pay_eco_001_sandbox_profile_sha256(),
            'conformance_suite_sha256'=>pay_eco_001_sandbox_suite_sha256(),
            'manifest_sha256'=>(string)$candidate['manifest_sha256'],
            'semantic_root'=>(string)$candidate['semantic_root'],
            'evidence_root'=>(string)$candidate['evidence_root'],
            'artifact_inventory_sha256'=>(string)$inventory['sha256'],
            'result'=>(string)$findings['result'],
            'findings_sha256'=>pay_eco_001_hash($findings),
            'evaluated_by'=>$actor,
            'evaluated_at'=>$iso,
            'evaluation_sha256'=>''
        );
        $evaluation['evaluation_sha256']=pay_eco_001_sandbox_evaluation_sha256($evaluation);if(!pay_eco_001_validate_sandbox_evaluation($evaluation,$error))break;
        $existing=pay_eco_001_onboarding_exact_row('SELECT * FROM '.TB_PREF.'pay_eco_001_sandbox_evaluations WHERE catalog_candidate_id='.(int)$evaluation['catalog_candidate_id'].' AND sandbox_profile_sha256='.pay_ctry_001_sql_hex($evaluation['sandbox_profile_sha256']).' LIMIT 2 FOR UPDATE',$error,true);if($existing===false)break;
        if(is_array($existing)){
            foreach(array('conformance_suite_sha256','manifest_sha256','semantic_root','evidence_root','artifact_inventory_sha256','result','findings_sha256') as $field)if((string)$existing[$field]!== (string)$evaluation[$field]){$error='sandbox_evaluation_replay_conflict';break 2;}
            if((int)$existing['evaluated_by']!==$actor){$error='sandbox_evaluation_replay_conflict';break;}
            $ok=true;$out=array('sandbox_evaluation_id'=>(int)$existing['sandbox_evaluation_id'],'result'=>(string)$existing['result'],'evaluation_sha256'=>(string)$existing['evaluation_sha256'],'replayed'=>true);break;
        }
        $now=gmdate('Y-m-d H:i:s',$now_epoch);
        $sql='INSERT INTO '.TB_PREF.'pay_eco_001_sandbox_evaluations (catalog_candidate_id,sandbox_profile_sha256,conformance_suite_sha256,manifest_sha256,semantic_root,evidence_root,artifact_inventory_sha256,result,findings_sha256,evaluated_by,evaluated_at,evaluation_sha256,created_at) VALUES ('.(int)$evaluation['catalog_candidate_id'].','.pay_ctry_001_sql_hex($evaluation['sandbox_profile_sha256']).','.pay_ctry_001_sql_hex($evaluation['conformance_suite_sha256']).','.pay_ctry_001_sql_hex($evaluation['manifest_sha256']).','.pay_ctry_001_sql_hex($evaluation['semantic_root']).','.pay_ctry_001_sql_hex($evaluation['evidence_root']).','.pay_ctry_001_sql_hex($evaluation['artifact_inventory_sha256']).','.pay_ctry_001_sql_hex($evaluation['result']).','.pay_ctry_001_sql_hex($evaluation['findings_sha256']).','.$actor.','.pay_ctry_001_sql_hex($now).','.pay_ctry_001_sql_hex($evaluation['evaluation_sha256']).','.pay_ctry_001_sql_hex($now).')';
        if(!db_query($sql)){$error='sandbox_evaluation_insert_failed';break;}$id=pay_ctry_001_insert_id();if($id<=0){$error='sandbox_evaluation_identity_missing';break;}
        if(!pay_eco_001_onboarding_audit('pay_eco_sandbox_evaluation',array('sandbox_evaluation_id'=>$id),'pay.eco.sandbox_evaluation.appended','declarative_conformance_evaluated',array('catalog_candidate_id'=>(int)$evaluation['catalog_candidate_id'],'verification_report_id'=>(int)$verification['verification_report_id'],'result'=>(string)$evaluation['result'],'sandbox_profile_sha256'=>(string)$evaluation['sandbox_profile_sha256'],'conformance_suite_sha256'=>(string)$evaluation['conformance_suite_sha256'],'evaluation_sha256'=>(string)$evaluation['evaluation_sha256'],'sandbox_execution'=>false,'dependency_vulnerability_scanning'=>false,'marketplace_publication'=>false,'pack_installation'=>false,'company_activation'=>false,'payroll_execution'=>false,'historical_payroll_accounting_rows_rewritten'=>0),$actor,$error))break;
        $ok=true;$out=array('sandbox_evaluation_id'=>$id,'result'=>$evaluation['result'],'evaluation_sha256'=>$evaluation['evaluation_sha256'],'replayed'=>false);
    }while(false);
    if($ok)commit_transaction();else cancel_transaction();
    return $ok?$out:false;
}
?>
