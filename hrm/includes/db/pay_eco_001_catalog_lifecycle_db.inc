<?php
/** PAY-ECO-001 fail-closed catalog suspension/revocation propagation. */
include_once($path_to_root.'/includes/pay_eco_001_contract.inc');
include_once($path_to_root.'/includes/pay_ctry_004_contract.inc');
include_once(dirname(__FILE__).'/pay_eco_001_onboarding_db.inc');

function pay_eco_001_catalog_lifecycle_iso($sql_value,&$error)
{
    $epoch=strtotime((string)$sql_value.' UTC');if($epoch===false)return pay_eco_001_fail($error,'catalog_lifecycle_time_state_invalid');return gmdate('Y-m-d\TH:i:s\Z',$epoch);
}
function pay_eco_001_catalog_lifecycle_exact_row($sql,&$error,$allow_none=false){return pay_eco_001_onboarding_exact_row($sql,$error,$allow_none);}
function pay_eco_001_catalog_lifecycle_lineage($catalog_publication_id,&$error,$locked=false)
{
    $suffix=$locked?' FOR UPDATE':'';
    return pay_eco_001_catalog_lifecycle_exact_row('SELECT cp.catalog_publication_id,cp.catalog_candidate_id,cp.certification_publication_id,cp.catalog_release_id,cp.publication_sha256,pb.certification_event_id,pb.certification_claim_id,cr.publisher_id,cr.trust_key_id FROM '.TB_PREF.'pay_eco_001_catalog_publications cp INNER JOIN '.TB_PREF.'pay_ctry_publication_records pb ON pb.publication_id=cp.certification_publication_id INNER JOIN '.TB_PREF.'pay_ctry_catalog_releases cr ON cr.catalog_release_id=cp.catalog_release_id WHERE cp.catalog_publication_id='.(int)$catalog_publication_id.' LIMIT 2'.$suffix,$error,false);
}
function pay_eco_001_catalog_lifecycle_event_source_id(array $event)
{
    if($event['source_type']==='certification_emergency_action')return (int)$event['certification_emergency_action_id'];
    if($event['source_type']==='publisher_revocation')return (int)$event['publisher_revocation_id'];
    return (int)$event['trust_key_revocation_id'];
}
function pay_eco_001_catalog_lifecycle_source_column($source_type)
{
    if($source_type==='certification_emergency_action')return 'certification_emergency_action_id';
    if($source_type==='publisher_revocation')return 'publisher_revocation_id';
    if($source_type==='trust_key_revocation')return 'trust_key_revocation_id';
    return false;
}
/** Load and integrity-check every authoritative denial source effective at the requested server time. */
function pay_eco_001_catalog_lifecycle_authoritative_events(array $lineage,$now_epoch,&$error,$locked=false)
{
    $error=null;$events=array();$nowSql=gmdate('Y-m-d H:i:s',(int)$now_epoch);$suffix=$locked?' FOR UPDATE':'';
    $sql='SELECT * FROM '.TB_PREF.'pay_ctry_certification_emergency_actions WHERE certification_event_id='.(int)$lineage['certification_event_id'].' AND (certification_claim_id IS NULL OR certification_claim_id='.(int)$lineage['certification_claim_id'].') AND effective_at<='.pay_ctry_001_sql_hex($nowSql).' ORDER BY effective_at,emergency_action_id'.$suffix;
    $q=db_query($sql);if(!$q)return pay_eco_001_fail($error,'catalog_lifecycle_certification_state_unavailable');
    while($r=db_fetch_assoc($q)){
        $effective=pay_eco_001_catalog_lifecycle_iso($r['effective_at'],$error);if($effective===false)return false;
        $payload=array('certification_event_id'=>(int)$r['certification_event_id'],'certification_claim_id'=>$r['certification_claim_id']===null?0:(int)$r['certification_claim_id'],'action_type'=>(string)$r['action_type'],'effective_at'=>$effective,'reason_code'=>(string)$r['reason_code'],'reason_sha256'=>(string)$r['reason_sha256'],'affected_scope_sha256'=>(string)$r['affected_scope_sha256'],'follow_up_required'=>(int)$r['follow_up_required'],'assurance_event_id'=>$r['assurance_event_id']===null?0:(int)$r['assurance_event_id'],'acted_by'=>(int)$r['acted_by'],'actor_role'=>(string)$r['actor_role']);
        if(!in_array($payload['action_type'],array('suspend','revoke'),true)||$payload['actor_role']!=='emergency_authority'||$payload['follow_up_required']!==1||!hash_equals(pay_ctry_004_emergency_action_sha256($payload),(string)$r['action_sha256']))return pay_eco_001_fail($error,'catalog_lifecycle_certification_source_integrity_invalid');
        $events[]=array('source_type'=>'certification_emergency_action','certification_emergency_action_id'=>(int)$r['emergency_action_id'],'publisher_revocation_id'=>null,'trust_key_revocation_id'=>null,'action_type'=>$payload['action_type'],'effective_at'=>$effective,'reason_code'=>$payload['reason_code'],'reason_sha256'=>$payload['reason_sha256'],'source_sha256'=>(string)$r['action_sha256']);
    }
    $pub=pay_eco_001_catalog_lifecycle_exact_row('SELECT r.*,p.publisher_code FROM '.TB_PREF.'pay_ctry_publisher_revocations r INNER JOIN '.TB_PREF.'pay_ctry_publishers p ON p.publisher_id=r.publisher_id WHERE r.publisher_id='.(int)$lineage['publisher_id'].' AND r.effective_at<='.pay_ctry_001_sql_hex($nowSql).' LIMIT 2'.$suffix,$error,true);if($pub===false)return false;
    if(is_array($pub)){
        $effective=pay_eco_001_catalog_lifecycle_iso($pub['effective_at'],$error);if($effective===false)return false;$payload=array('publisher_id'=>(int)$pub['publisher_id'],'publisher_code'=>(string)$pub['publisher_code'],'effective_at'=>$effective,'reason_code'=>(string)$pub['reason_code'],'reason_sha256'=>(string)$pub['reason_sha256']);$sha=hash('sha256',pay_ctry_001_canonical_json($payload));if(!hash_equals($sha,(string)$pub['revocation_sha256']))return pay_eco_001_fail($error,'catalog_lifecycle_publisher_source_integrity_invalid');
        $events[]=array('source_type'=>'publisher_revocation','certification_emergency_action_id'=>null,'publisher_revocation_id'=>(int)$pub['publisher_revocation_id'],'trust_key_revocation_id'=>null,'action_type'=>'revoke','effective_at'=>$effective,'reason_code'=>(string)$pub['reason_code'],'reason_sha256'=>(string)$pub['reason_sha256'],'source_sha256'=>(string)$pub['revocation_sha256']);
    }
    $key=pay_eco_001_catalog_lifecycle_exact_row('SELECT r.*,k.key_id FROM '.TB_PREF.'pay_ctry_trust_key_revocations r INNER JOIN '.TB_PREF.'pay_ctry_trust_keys k ON k.trust_key_id=r.trust_key_id WHERE r.trust_key_id='.(int)$lineage['trust_key_id'].' AND r.effective_at<='.pay_ctry_001_sql_hex($nowSql).' LIMIT 2'.$suffix,$error,true);if($key===false)return false;
    if(is_array($key)){
        $effective=pay_eco_001_catalog_lifecycle_iso($key['effective_at'],$error);if($effective===false)return false;$payload=array('trust_key_id'=>(int)$key['trust_key_id'],'key_id'=>(string)$key['key_id'],'effective_at'=>$effective,'reason_code'=>(string)$key['reason_code'],'reason_sha256'=>(string)$key['reason_sha256'],'compromised'=>!empty($key['compromised']));$sha=hash('sha256',pay_ctry_001_canonical_json($payload));if(!hash_equals($sha,(string)$key['revocation_sha256']))return pay_eco_001_fail($error,'catalog_lifecycle_key_source_integrity_invalid');
        $events[]=array('source_type'=>'trust_key_revocation','certification_emergency_action_id'=>null,'publisher_revocation_id'=>null,'trust_key_revocation_id'=>(int)$key['key_revocation_id'],'action_type'=>'revoke','effective_at'=>$effective,'reason_code'=>(string)$key['reason_code'],'reason_sha256'=>(string)$key['reason_sha256'],'source_sha256'=>(string)$key['revocation_sha256']);
    }
    usort($events,function($a,$b){if($a['action_type']!==$b['action_type'])return $a['action_type']==='revoke'?-1:1;$ta=pay_eco_001_time($a['effective_at']);$tb=pay_eco_001_time($b['effective_at']);if($ta===$tb)return strcmp($a['source_type'],$b['source_type']);return $ta<$tb?-1:1;});return $events;
}
/** Runtime denial is derived directly from authoritative sources; propagation custody is not a prerequisite. */
function pay_eco_001_catalog_lifecycle_guard($catalog_publication_id,&$error,$now_epoch=null,$locked=false)
{
    $error=null;$now_epoch=$now_epoch===null?time():(int)$now_epoch;if(!pay_eco_001_positive_id($catalog_publication_id)||$now_epoch<=0)return pay_eco_001_fail($error,'catalog_lifecycle_guard_request_invalid');
    $profile=pay_eco_001_catalog_lifecycle_profile();if(empty($profile['runtime_denies_before_propagation_record']))return pay_eco_001_fail($error,'catalog_lifecycle_guard_profile_invalid');
    $lineage=pay_eco_001_catalog_lifecycle_lineage($catalog_publication_id,$error,$locked);if($lineage===false)return false;$events=pay_eco_001_catalog_lifecycle_authoritative_events($lineage,$now_epoch,$error,$locked);if($events===false)return false;
    if(!$events)return true;foreach($events as $event)if($event['action_type']==='revoke')return pay_eco_001_fail($error,'catalog_runtime_revoked');return pay_eco_001_fail($error,'catalog_runtime_suspended');
}
function pay_eco_001_catalog_lifecycle_audit(array $event,$catalog_lifecycle_event_id,$actor,&$error)
{
    if(!function_exists('append_domain_audit_event'))return pay_eco_001_fail($error,'catalog_lifecycle_audit_unavailable');$audit=append_domain_audit_event(array('company_id'=>PAY_CTRY_001_SITE_COMPANY_ID,'legal_entity_id'=>0,'entity_type'=>'pay_eco_catalog_lifecycle','entity_key'=>array('catalog_lifecycle_event_id'=>(int)$catalog_lifecycle_event_id),'action'=>'pay.eco.catalog_lifecycle.propagated','reason_code'=>(string)$event['reason_code'],'before'=>array(),'after'=>array('catalog_publication_id'=>(int)$event['catalog_publication_id'],'source_type'=>(string)$event['source_type'],'source_id'=>(int)$event['source_id'],'action_type'=>(string)$event['action_type'],'source_sha256'=>(string)$event['source_sha256'],'runtime_denies_before_propagation_record'=>true,'pack_installation'=>false,'company_activation'=>false,'historical_payroll_accounting_rows_rewritten'=>0),'restricted_diff'=>array('actor_id'=>(int)$actor,'append_only'=>true,'pack_installation'=>false,'company_activation'=>false,'historical_payroll_accounting_rows_rewritten'=>0)));return $audit===false?pay_eco_001_fail($error,'catalog_lifecycle_audit_failed'):true;
}
/** Append immutable local propagation evidence for all currently effective authoritative denial sources. */
function pay_eco_001_propagate_catalog_lifecycle($request,&$error,$now_epoch=null)
{
    $error=null;if(!pay_eco_001_validate_catalog_lifecycle_request($request,$error))return false;$actor=pay_eco_001_onboarding_actor_id();$now_epoch=$now_epoch===null?time():(int)$now_epoch;if($actor<=0||$now_epoch<=0||!pay_eco_001_onboarding_access(PAY_ECO_001_CATALOG_LIFECYCLE_PERMISSION))return pay_eco_001_fail($error,'catalog_lifecycle_authority_denied');if(!pay_eco_001_onboarding_owned_transaction($error))return false;$ok=false;$out=false;
    do{
        $lineage=pay_eco_001_catalog_lifecycle_lineage((int)$request['catalog_publication_id'],$error,true);if($lineage===false)break;$events=pay_eco_001_catalog_lifecycle_authoritative_events($lineage,$now_epoch,$error,true);if($events===false)break;if(!$events){$error='catalog_lifecycle_no_authoritative_event';break;}$ids=array();$appended=0;$replayed=0;$propagatedAt=pay_eco_001_onboarding_now_iso($now_epoch);$nowSql=gmdate('Y-m-d H:i:s',$now_epoch);
        foreach($events as $source){$column=pay_eco_001_catalog_lifecycle_source_column($source['source_type']);$sourceId=pay_eco_001_catalog_lifecycle_event_source_id($source);if($column===false||$sourceId<=0){$error='catalog_lifecycle_source_mapping_invalid';break 2;}$existing=pay_eco_001_catalog_lifecycle_exact_row('SELECT * FROM '.TB_PREF.'pay_eco_001_catalog_lifecycle_events WHERE catalog_publication_id='.(int)$lineage['catalog_publication_id'].' AND '.$column.'='.$sourceId.' LIMIT 2 FOR UPDATE',$error,true);if($existing===false)break 2;
            if(is_array($existing)){if((string)$existing['source_type']!==$source['source_type']||(string)$existing['action_type']!==$source['action_type']||!hash_equals((string)$existing['source_sha256'],$source['source_sha256'])||!hash_equals((string)$existing['reason_sha256'],$source['reason_sha256'])){$error='catalog_lifecycle_replay_conflict';break 2;}$ids[]=(int)$existing['catalog_lifecycle_event_id'];$replayed++;continue;}
            $event=array('catalog_publication_id'=>(int)$lineage['catalog_publication_id'],'source_type'=>(string)$source['source_type'],'source_id'=>$sourceId,'action_type'=>(string)$source['action_type'],'effective_at'=>(string)$source['effective_at'],'reason_code'=>(string)$source['reason_code'],'reason_sha256'=>(string)$source['reason_sha256'],'source_sha256'=>(string)$source['source_sha256'],'propagated_by'=>$actor,'propagated_at'=>$propagatedAt,'lifecycle_sha256'=>'');$event['lifecycle_sha256']=pay_eco_001_catalog_lifecycle_sha256($event);if(!pay_eco_001_validate_catalog_lifecycle_event($event,$error))break 2;
            $cert=$source['certification_emergency_action_id']===null?'NULL':(int)$source['certification_emergency_action_id'];$pub=$source['publisher_revocation_id']===null?'NULL':(int)$source['publisher_revocation_id'];$key=$source['trust_key_revocation_id']===null?'NULL':(int)$source['trust_key_revocation_id'];$sql='INSERT INTO '.TB_PREF.'pay_eco_001_catalog_lifecycle_events (catalog_publication_id,source_type,certification_emergency_action_id,publisher_revocation_id,trust_key_revocation_id,action_type,effective_at,reason_code,reason_sha256,source_sha256,propagated_by,propagated_at,lifecycle_sha256,created_at) VALUES ('.(int)$event['catalog_publication_id'].','.pay_ctry_001_sql_hex($event['source_type']).','.$cert.','.$pub.','.$key.','.pay_ctry_001_sql_hex($event['action_type']).','.pay_ctry_001_sql_hex(gmdate('Y-m-d H:i:s',pay_eco_001_time($event['effective_at']))).','.pay_ctry_001_sql_hex($event['reason_code']).','.pay_ctry_001_sql_hex($event['reason_sha256']).','.pay_ctry_001_sql_hex($event['source_sha256']).','.$actor.','.pay_ctry_001_sql_hex($nowSql).','.pay_ctry_001_sql_hex($event['lifecycle_sha256']).','.pay_ctry_001_sql_hex($nowSql).')';if(!db_query($sql)){$error='catalog_lifecycle_insert_failed';break 2;}$id=pay_ctry_001_insert_id();if($id<=0){$error='catalog_lifecycle_identity_missing';break 2;}if(!pay_eco_001_catalog_lifecycle_audit($event,$id,$actor,$error))break 2;$ids[]=$id;$appended++;
        }
        $action='suspend';foreach($events as $event)if($event['action_type']==='revoke'){$action='revoke';break;}$ok=true;$out=array('catalog_publication_id'=>(int)$lineage['catalog_publication_id'],'action_type'=>$action,'appended'=>$appended,'replayed'=>$replayed,'catalog_lifecycle_event_ids'=>$ids,'pack_installation'=>false,'company_activation'=>false,'historical_payroll_accounting_rows_rewritten'=>0);
    }while(false);if($ok)commit_transaction();else cancel_transaction();return $ok?$out:false;
}
?>
