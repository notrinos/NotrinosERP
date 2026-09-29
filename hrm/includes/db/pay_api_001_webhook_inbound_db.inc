<?php
/** PAY-API-001 fail-closed inbound signature verification and replay-protected receipt custody. */
include_once($path_to_root.'/includes/pay_api_001_webhook_inbound.inc');
include_once($path_to_root.'/includes/pay_api_001_contract.inc');

function pay_api_001_webhook_inbound_database_version(){if(!function_exists('get_company_pref'))return false;$v=get_company_pref('version_id',true);return is_string($v)?$v:false;}
function pay_api_001_webhook_inbound_runtime_enabled(){ $v=pay_api_001_webhook_inbound_database_version(); return $v!==false&&pay_api_001_webhook_inbound_verifier_allowed_for_database_version($v); }
function pay_api_001_webhook_inbound_route_enabled(){ $v=pay_api_001_webhook_inbound_database_version(); return $v!==false&&pay_api_001_webhook_inbound_route_allowed_for_database_version($v); }
function pay_api_001_webhook_inbound_manage_transaction(){return empty($GLOBALS['transaction_level']);}

function pay_api_001_webhook_inbound_resolve_source($company_id,$source_key,$unix_time=null,&$error=null)
{
    $error=null;$company=pay_api_001_uint_value($company_id,true,4294967295);$key=pay_api_001_webhook_inbound_source_key($source_key);$t=$unix_time===null?time():pay_api_001_uint_value($unix_time,true,PHP_INT_MAX);
    if($company===false||$key===false||$t===false){$error='inbound_source_input_invalid';return false;}
    $now=gmdate('Y-m-d H:i:s',$t);
    $sql='SELECT s.*,r.ref_kind,r.provider_key,r.provider_reference_sha256,r.secret_ref_sha256 FROM '.TB_PREF.'hrm_pay_api_001_webhook_inbound_sources s INNER JOIN '.TB_PREF.'hrm_pay_api_001_webhook_secret_refs r ON r.webhook_secret_ref_id=s.webhook_secret_ref_id WHERE s.company_id='.$company.' AND s.source_key='.db_escape($key).' AND s.valid_from<='.db_escape($now).' AND s.expires_at>'.db_escape($now).' ORDER BY s.valid_from DESC,s.webhook_inbound_source_id DESC LIMIT 2';
    $q=db_query($sql,'could not resolve PAY-API-001 inbound webhook source');
    if(!$q||db_num_rows($q)!==1){$error='inbound_source_unavailable_or_ambiguous';return false;}$row=db_fetch_assoc($q);
    if((int)$row['company_id']!==$company||(string)$row['ref_kind']!=='webhook_signing_key'){$error='inbound_source_scope_invalid';return false;}
    return $row;
}

function pay_api_001_receive_inbound_webhook($company_id,$source_key,array $headers,$body,$secret_resolver,$unix_time=null,&$error=null)
{
    $error=null;
    if(!pay_api_001_webhook_inbound_runtime_enabled()){$error='inbound_verifier_disabled';return false;}
    $source=pay_api_001_webhook_inbound_resolve_source($company_id,$source_key,$unix_time,$error);if($source===false)return false;
    $secret_ref=array('company_id'=>(int)$source['company_id'],'ref_kind'=>$source['ref_kind'],'provider_key'=>$source['provider_key'],'provider_reference_sha256'=>$source['provider_reference_sha256'],'secret_ref_sha256'=>$source['secret_ref_sha256']);
    $secret=pay_api_001_webhook_resolve_signing_secret($secret_ref,$secret_resolver,$error);if($secret===false)return false;
    $verified=pay_api_001_webhook_inbound_verify($source,$headers,$body,$secret,$unix_time,$error);unset($secret);if($verified===false)return false;
    $manage=pay_api_001_webhook_inbound_manage_transaction();if($manage)begin_transaction();
    $sid=(int)$verified['webhook_inbound_source_id'];$company=(int)$verified['company_id'];
    $replayq=db_query('SELECT webhook_inbound_replay_defense_id FROM '.TB_PREF.'hrm_pay_api_001_webhook_inbound_replay_defenses WHERE webhook_inbound_source_id='.$sid.' AND replay_key_sha256='.db_escape($verified['replay_key_sha256']).' LIMIT 2 FOR UPDATE');
    if(!$replayq||db_num_rows($replayq)>1){if($manage)cancel_transaction();$error='inbound_replay_state_invalid';return false;}
    if(db_num_rows($replayq)===1){if($manage)cancel_transaction();$error='inbound_replay_denied';return false;}
    $corq=db_query('SELECT webhook_inbound_receipt_id FROM '.TB_PREF.'hrm_pay_api_001_webhook_inbound_receipts WHERE company_id='.$company.' AND correlation_sha256='.db_escape($verified['correlation_sha256']).' LIMIT 2 FOR UPDATE');
    if(!$corq||db_num_rows($corq)>1){if($manage)cancel_transaction();$error='inbound_receipt_state_invalid';return false;}
    if(db_num_rows($corq)===1){if($manage)cancel_transaction();$error='inbound_replay_denied';return false;}
    $received=gmdate('Y-m-d H:i:s',$unix_time===null?time():(int)$unix_time);
    $receipt_sha=pay_api_001_provisioning_hash(array('webhook_inbound_source_id'=>$sid,'company_id'=>$company,'delivery_key_sha256'=>$verified['delivery_key_sha256'],'request_target_sha256'=>$verified['request_target_sha256'],'event_type_sha256'=>$verified['event_type_sha256'],'payload_sha256'=>$verified['payload_sha256'],'signature_sha256'=>$verified['signature_sha256'],'timestamp_value'=>$verified['timestamp_value'],'verification_outcome_code'=>'verified','replay_key_sha256'=>$verified['replay_key_sha256'],'correlation_sha256'=>$verified['correlation_sha256'],'received_at'=>$received));
    $sql='INSERT INTO '.TB_PREF.'hrm_pay_api_001_webhook_inbound_receipts (webhook_inbound_source_id,company_id,delivery_key_sha256,request_target_sha256,event_type_sha256,payload_sha256,signature_sha256,timestamp_value,verification_outcome_code,replay_key_sha256,correlation_sha256,receipt_sha256,received_at,created_at) VALUES ('.$sid.','.$company.','.db_escape($verified['delivery_key_sha256']).','.db_escape($verified['request_target_sha256']).','.db_escape($verified['event_type_sha256']).','.db_escape($verified['payload_sha256']).','.db_escape($verified['signature_sha256']).','.db_escape($verified['timestamp_value']).',\'verified\','.db_escape($verified['replay_key_sha256']).','.db_escape($verified['correlation_sha256']).','.db_escape($receipt_sha).','.db_escape($received).','.db_escape($received).')';
    if(!db_query($sql)){if($manage)cancel_transaction();$error='inbound_receipt_insert_failed_or_replayed';return false;}$receipt_id=(int)db_insert_id();
    $defense_sha=pay_api_001_provisioning_hash(array('webhook_inbound_source_id'=>$sid,'webhook_inbound_receipt_id'=>$receipt_id,'replay_key_sha256'=>$verified['replay_key_sha256'],'payload_sha256'=>$verified['payload_sha256'],'valid_until'=>$verified['valid_until'],'recorded_at'=>$received));
    $sql='INSERT INTO '.TB_PREF.'hrm_pay_api_001_webhook_inbound_replay_defenses (webhook_inbound_source_id,webhook_inbound_receipt_id,replay_key_sha256,payload_sha256,valid_until,defense_sha256,recorded_at,created_at) VALUES ('.$sid.','.$receipt_id.','.db_escape($verified['replay_key_sha256']).','.db_escape($verified['payload_sha256']).','.db_escape($verified['valid_until']).','.db_escape($defense_sha).','.db_escape($received).','.db_escape($received).')';
    if(!db_query($sql)){if($manage)cancel_transaction();$error='inbound_replay_defense_insert_failed_or_replayed';return false;}
    if($manage)commit_transaction();
    return array('accepted'=>true,'webhook_inbound_receipt_id'=>$receipt_id,'receipt_sha256'=>$receipt_sha,'replay_defense_sha256'=>$defense_sha,'payload_sha256'=>$verified['payload_sha256']);
}
?>
