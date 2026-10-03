<?php
/** PAY-ECO-001 append-only partner onboarding writers. */
include_once($path_to_root.'/includes/pay_eco_001_contract.inc');
include_once(dirname(__FILE__).'/pay_ctry_001_db.inc');
if(!function_exists('append_domain_audit_event'))include_once($path_to_root.'/includes/db/domain_audit_db.inc');

/** Return the authenticated actor id, or zero when no actor is available. */
function pay_eco_001_onboarding_actor_id()
{
    return function_exists('pay_ctry_001_actor_id') ? (int)pay_ctry_001_actor_id() : 0;
}
/** Check an existing PAY-CTRY permission for the current actor. */
function pay_eco_001_onboarding_access($permission)
{
    return function_exists('pay_ctry_001_access') && pay_ctry_001_access($permission);
}
/** Read at most one locked row and fail closed on query or ambiguity errors. */
function pay_eco_001_onboarding_exact_row($sql,&$error,$allow_none)
{
    $q=db_query($sql);
    if(!$q)return pay_eco_001_fail($error,'onboarding_query_failed');
    $n=(int)db_num_rows($q);
    if($n>1)return pay_eco_001_fail($error,'onboarding_state_ambiguous');
    if($n===0)return $allow_none?null:pay_eco_001_fail($error,'onboarding_state_missing');
    return db_fetch_assoc($q);
}
/** Format a server timestamp as a UTC ISO-8601 value. */
function pay_eco_001_onboarding_now_iso($epoch)
{
    return gmdate('Y-m-d\TH:i:s\Z',(int)$epoch);
}
/** Require writer ownership of a new transaction before custody writes. */
function pay_eco_001_onboarding_owned_transaction(&$error)
{
    if(!empty($GLOBALS['transaction_level']))return pay_eco_001_fail($error,'onboarding_requires_owned_transaction');
    begin_transaction();return true;
}
/** Append restricted domain-audit evidence inside the writer transaction. */
function pay_eco_001_onboarding_audit($entity_type,$entity_key,$action,$reason,$after,$actor,&$error)
{
    if(!function_exists('append_domain_audit_event'))return pay_eco_001_fail($error,'onboarding_audit_unavailable');
    $event=append_domain_audit_event(array(
        'company_id'=>PAY_CTRY_001_SITE_COMPANY_ID,
        'legal_entity_id'=>0,
        'entity_type'=>$entity_type,
        'entity_key'=>$entity_key,
        'action'=>$action,
        'reason_code'=>$reason,
        'before'=>array(),
        'after'=>$after,
        'restricted_diff'=>array(
            'actor_id'=>(int)$actor,
            'append_only'=>true,
            'marketplace_publication'=>false,
            'pack_installation'=>false,
            'company_activation'=>false,
            'payroll_execution'=>false,
            'historical_payroll_accounting_rows_rewritten'=>0
        )
    ));
    return $event===false?pay_eco_001_fail($error,'onboarding_audit_failed'):true;
}

/**
 * Append one minimized publisher onboarding profile under existing site trust authority.
 * The writer stores evidence digests only; it does not verify identity or grant publisher trust.
 */
function pay_eco_001_write_publisher_profile($request,&$error,$now_epoch=null)
{
    $error=null;$actor=pay_eco_001_onboarding_actor_id();$now_epoch=$now_epoch===null?time():(int)$now_epoch;
    if($actor<=0||$now_epoch<=0||!pay_eco_001_onboarding_access(PAY_ECO_001_PUBLISHER_PROFILE_PERMISSION))return pay_eco_001_fail($error,'publisher_profile_authority_denied');
    $iso=pay_eco_001_onboarding_now_iso($now_epoch);$profile=pay_eco_001_build_publisher_profile($request,$actor,$iso,$error);if($profile===false)return false;
    if(!pay_eco_001_onboarding_owned_transaction($error))return false;$ok=false;$out=false;
    do{
        $publisher=pay_eco_001_onboarding_exact_row('SELECT publisher_id,publisher_code,valid_from,valid_to FROM '.TB_PREF.'pay_ctry_publishers WHERE publisher_id='.(int)$profile['publisher_id'].' LIMIT 2 FOR UPDATE',$error,false);if($publisher===false)break;
        $revocation=pay_eco_001_onboarding_exact_row('SELECT publisher_revocation_id FROM '.TB_PREF.'pay_ctry_publisher_revocations WHERE publisher_id='.(int)$publisher['publisher_id'].' LIMIT 2 FOR UPDATE',$error,true);if($revocation===false)break;if(is_array($revocation)){$error='publisher_profile_publisher_revoked';break;}
        $from=strtotime((string)$publisher['valid_from'].' UTC');$to=$publisher['valid_to']===null?false:strtotime((string)$publisher['valid_to'].' UTC');
        if($from===false||($publisher['valid_to']!==null&&$to===false)||$from>$now_epoch||($to!==false&&$to<$now_epoch)){$error='publisher_profile_publisher_inactive';break;}
        $existing=pay_eco_001_onboarding_exact_row('SELECT * FROM '.TB_PREF.'pay_eco_001_publisher_profiles WHERE profile_code='.pay_ctry_001_sql_hex($profile['profile_code']).' OR profile_sha256='.pay_ctry_001_sql_hex($profile['profile_sha256']).' LIMIT 2 FOR UPDATE',$error,true);if($existing===false)break;
        if(is_array($existing)){
            if((int)$existing['publisher_id']!==(int)$request['publisher_id']||(string)$existing['profile_code']!==(string)$request['profile_code']||(string)$existing['identity_evidence_sha256']!==(string)$request['identity_evidence_sha256']||(string)$existing['support_policy_sha256']!==(string)$request['support_policy_sha256']||(string)$existing['security_contact_sha256']!==(string)$request['security_contact_sha256']||(string)$existing['terms_sha256']!==(string)$request['terms_sha256']||(int)$existing['requested_by']!==$actor){$error='publisher_profile_replay_conflict';break;}
            $ok=true;$out=array('publisher_profile_id'=>(int)$existing['publisher_profile_id'],'profile_sha256'=>(string)$existing['profile_sha256'],'replayed'=>true);break;
        }
        $sql='INSERT INTO '.TB_PREF.'pay_eco_001_publisher_profiles (publisher_id,profile_code,identity_evidence_sha256,support_policy_sha256,security_contact_sha256,terms_sha256,requested_by,requested_at,profile_sha256,created_at) VALUES ('.(int)$profile['publisher_id'].','.pay_ctry_001_sql_hex($profile['profile_code']).','.pay_ctry_001_sql_hex($profile['identity_evidence_sha256']).','.pay_ctry_001_sql_hex($profile['support_policy_sha256']).','.pay_ctry_001_sql_hex($profile['security_contact_sha256']).','.pay_ctry_001_sql_hex($profile['terms_sha256']).','.$actor.','.pay_ctry_001_sql_hex(gmdate('Y-m-d H:i:s',$now_epoch)).','.pay_ctry_001_sql_hex($profile['profile_sha256']).','.pay_ctry_001_sql_hex(gmdate('Y-m-d H:i:s',$now_epoch)).')';
        if(!db_query($sql)){$error='publisher_profile_insert_failed';break;}$id=pay_ctry_001_insert_id();if($id<=0){$error='publisher_profile_identity_missing';break;}
        if(!pay_eco_001_onboarding_audit('pay_eco_publisher_profile',array('publisher_profile_id'=>$id),'pay.eco.publisher_profile.appended','partner_onboarding_profile_submitted',array('publisher_id'=>(int)$profile['publisher_id'],'profile_code'=>(string)$profile['profile_code'],'profile_sha256'=>(string)$profile['profile_sha256']),$actor,$error))break;
        $ok=true;$out=array('publisher_profile_id'=>$id,'profile_sha256'=>$profile['profile_sha256'],'replayed'=>false);
    }while(false);
    if($ok)commit_transaction();else cancel_transaction();
    return $ok?$out:false;
}

/**
 * Append one pack candidate under existing signed-pack installation authority.
 * Candidate admission requires an already accepted PAY-CTRY-001 verification report and exact publisher lineage.
 * No catalog publication, installation, company activation or payroll authority is granted here.
 */
function pay_eco_001_write_pack_candidate($request,&$error,$now_epoch=null)
{
    $error=null;$actor=pay_eco_001_onboarding_actor_id();$now_epoch=$now_epoch===null?time():(int)$now_epoch;
    if($actor<=0||$now_epoch<=0||!pay_eco_001_onboarding_access(PAY_ECO_001_PACK_CANDIDATE_PERMISSION))return pay_eco_001_fail($error,'pack_candidate_authority_denied');
    $iso=pay_eco_001_onboarding_now_iso($now_epoch);$candidate=pay_eco_001_build_pack_candidate($request,$actor,$iso,$error);if($candidate===false)return false;
    if(!pay_eco_001_onboarding_owned_transaction($error))return false;$ok=false;$out=false;
    do{
        $profile=pay_eco_001_onboarding_exact_row('SELECT ep.publisher_profile_id,ep.publisher_id,ep.profile_sha256,p.publisher_code,p.valid_from,p.valid_to FROM '.TB_PREF.'pay_eco_001_publisher_profiles ep INNER JOIN '.TB_PREF.'pay_ctry_publishers p ON p.publisher_id=ep.publisher_id WHERE ep.publisher_profile_id='.(int)$candidate['publisher_profile_id'].' LIMIT 2 FOR UPDATE',$error,false);if($profile===false)break;
        $revocation=pay_eco_001_onboarding_exact_row('SELECT publisher_revocation_id FROM '.TB_PREF.'pay_ctry_publisher_revocations WHERE publisher_id='.(int)$profile['publisher_id'].' LIMIT 2 FOR UPDATE',$error,true);if($revocation===false)break;if(is_array($revocation)){$error='pack_candidate_publisher_revoked';break;}
        $from=strtotime((string)$profile['valid_from'].' UTC');$to=$profile['valid_to']===null?false:strtotime((string)$profile['valid_to'].' UTC');
        if($from===false||($profile['valid_to']!==null&&$to===false)||$from>$now_epoch||($to!==false&&$to<$now_epoch)){$error='pack_candidate_publisher_inactive';break;}
        $pack=pay_eco_001_onboarding_exact_row('SELECT pack_version_id,pack_id,version,publisher_code,manifest_sha256,semantic_root,evidence_root FROM '.TB_PREF.'pay_ctry_pack_versions WHERE pack_version_id='.(int)$candidate['pack_version_id'].' LIMIT 2 FOR UPDATE',$error,false);if($pack===false)break;
        if((string)$pack['publisher_code']!==(string)$profile['publisher_code']){$error='pack_candidate_publisher_lineage_mismatch';break;}
        $verification=pay_eco_001_onboarding_exact_row('SELECT verification_report_id,report_sha256 FROM '.TB_PREF.'pay_ctry_verification_reports WHERE pack_version_id='.(int)$candidate['pack_version_id']." AND final_status='accepted' ORDER BY verification_report_id DESC LIMIT 1",$error,false);if($verification===false){if($error==='onboarding_state_missing')$error='pack_candidate_verified_pack_required';break;}
        $existing=pay_eco_001_onboarding_exact_row('SELECT * FROM '.TB_PREF.'pay_eco_001_pack_candidates WHERE candidate_code='.pay_ctry_001_sql_hex($candidate['candidate_code']).' OR candidate_sha256='.pay_ctry_001_sql_hex($candidate['candidate_sha256']).' OR (publisher_profile_id='.(int)$candidate['publisher_profile_id'].' AND pack_version_id='.(int)$candidate['pack_version_id'].') LIMIT 2 FOR UPDATE',$error,true);if($existing===false)break;
        if(is_array($existing)){
            if((int)$existing['publisher_profile_id']!==(int)$request['publisher_profile_id']||(int)$existing['pack_version_id']!==(int)$request['pack_version_id']||(string)$existing['candidate_code']!==(string)$request['candidate_code']||(string)$existing['compatibility_sha256']!==(string)$request['compatibility_sha256']||(string)$existing['certification_sha256']!==(string)$request['certification_sha256']||(string)$existing['support_sha256']!==(string)$request['support_sha256']||(string)$existing['vulnerability_sha256']!==(string)$request['vulnerability_sha256']||(string)$existing['revocation_state_sha256']!==(string)$request['revocation_state_sha256']||(string)$existing['sandbox_profile_sha256']!==(string)$request['sandbox_profile_sha256']||(int)$existing['submitted_by']!==$actor){$error='pack_candidate_replay_conflict';break;}
            $ok=true;$out=array('catalog_candidate_id'=>(int)$existing['catalog_candidate_id'],'candidate_sha256'=>(string)$existing['candidate_sha256'],'replayed'=>true);break;
        }
        $now=gmdate('Y-m-d H:i:s',$now_epoch);
        $sql='INSERT INTO '.TB_PREF.'pay_eco_001_pack_candidates (publisher_profile_id,pack_version_id,candidate_code,compatibility_sha256,certification_sha256,support_sha256,vulnerability_sha256,revocation_state_sha256,sandbox_profile_sha256,submitted_by,submitted_at,candidate_sha256,created_at) VALUES ('.(int)$candidate['publisher_profile_id'].','.(int)$candidate['pack_version_id'].','.pay_ctry_001_sql_hex($candidate['candidate_code']).','.pay_ctry_001_sql_hex($candidate['compatibility_sha256']).','.pay_ctry_001_sql_hex($candidate['certification_sha256']).','.pay_ctry_001_sql_hex($candidate['support_sha256']).','.pay_ctry_001_sql_hex($candidate['vulnerability_sha256']).','.pay_ctry_001_sql_hex($candidate['revocation_state_sha256']).','.pay_ctry_001_sql_hex($candidate['sandbox_profile_sha256']).','.$actor.','.pay_ctry_001_sql_hex($now).','.pay_ctry_001_sql_hex($candidate['candidate_sha256']).','.pay_ctry_001_sql_hex($now).')';
        if(!db_query($sql)){$error='pack_candidate_insert_failed';break;}$id=pay_ctry_001_insert_id();if($id<=0){$error='pack_candidate_identity_missing';break;}
        if(!pay_eco_001_onboarding_audit('pay_eco_pack_candidate',array('catalog_candidate_id'=>$id),'pay.eco.pack_candidate.appended','verified_partner_pack_submitted',array('publisher_profile_id'=>(int)$candidate['publisher_profile_id'],'pack_version_id'=>(int)$candidate['pack_version_id'],'verification_report_id'=>(int)$verification['verification_report_id'],'candidate_code'=>(string)$candidate['candidate_code'],'candidate_sha256'=>(string)$candidate['candidate_sha256']),$actor,$error))break;
        $ok=true;$out=array('catalog_candidate_id'=>$id,'candidate_sha256'=>$candidate['candidate_sha256'],'replayed'=>false);
    }while(false);
    if($ok)commit_transaction();else cancel_transaction();
    return $ok?$out:false;
}
?>
