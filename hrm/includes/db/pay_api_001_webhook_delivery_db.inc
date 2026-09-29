<?php
/** PAY-API-001 claim-bound initial webhook delivery and immutable evidence writers. */
include_once(dirname(__FILE__).'/pay_api_001_webhook_runtime_db.inc');
include_once($path_to_root.'/includes/pay_api_001_webhook_transport.inc');

/** Check one exact delivery-stage authority against the current company database version. */
function pay_api_001_webhook_delivery_runtime_enabled($kind)
{
    $v=pay_api_001_runtime_database_version();if($v===false)return false;
    if($kind==='attempt')return pay_api_001_webhook_delivery_attempt_writer_allowed_for_database_version($v);
    if($kind==='response')return pay_api_001_webhook_provider_response_writer_allowed_for_database_version($v);
    if($kind==='delivery')return pay_api_001_webhook_delivery_executor_allowed_for_database_version($v);
    return false;
}

/** Validate minimized transport evidence and derive immutable persistence hashes. */
function pay_api_001_webhook_delivery_result_evidence(array $result,&$error=null)
{
    $error=null;$keys=array('classification','retry_safe','http_status','response_headers_sha256','response_body_sha256','response_body_bytes','response_content_type_sha256','error_code');
    if(array_keys($result)!==$keys||!in_array($result['classification'],array('pre_send_failure','delivery_uncertain','delivered','provider_retryable','provider_rejected'),true)
        ||!is_bool($result['retry_safe'])||!is_int($result['http_status'])||$result['http_status']<0||$result['http_status']>599
        ||!pay_api_001_is_sha256($result['response_headers_sha256'])||!pay_api_001_is_sha256($result['response_body_sha256'])||!is_int($result['response_body_bytes'])||$result['response_body_bytes']<0||$result['response_body_bytes']>PAY_API_001_WEBHOOK_MAX_RESPONSE_BYTES
        ||!pay_api_001_is_sha256($result['response_content_type_sha256'])||!is_string($result['error_code'])||strlen($result['error_code'])>64||preg_match('/^[A-Za-z0-9_.:-]*$/D',$result['error_code'])!==1){$error='webhook_delivery_result_invalid';return false;}
    $definite=in_array($result['classification'],array('delivered','provider_retryable','provider_rejected'),true);
    if($definite&&($result['http_status']<100||$result['http_status']>599)){$error='webhook_provider_response_status_invalid';return false;}
    if(!$definite&&$result['classification']==='pre_send_failure'&&($result['http_status']!==0||$result['retry_safe']!==true)){$error='webhook_pre_send_result_invalid';return false;}
    if($result['classification']!=='pre_send_failure'&&$result['retry_safe']!==false){$error='webhook_post_send_retry_flag_invalid';return false;}
    return array('outcome_code'=>$result['classification'],'http_status_code'=>$result['http_status']>0?$result['http_status']:null,'error_code'=>$result['error_code']===''?null:$result['error_code'],'definite_provider_response'=>$definite,'response_headers_sha256'=>$result['response_headers_sha256'],'response_body_sha256'=>$result['response_body_sha256'],'response_body_bytes'=>$result['response_body_bytes'],'response_content_type_sha256'=>$result['response_content_type_sha256']);
}

/** Resolve only an endpoint path from an opaque approved endpoint reference. */
function pay_api_001_webhook_resolve_endpoint(array $target,$resolver,&$error=null)
{
    $error=null;if(!isset($target['company_id'],$target['endpoint_config_ref_sha256'])||pay_api_001_uint_value($target['company_id'],true,4294967295)===false||!pay_api_001_is_sha256($target['endpoint_config_ref_sha256'])||!is_callable($resolver)){$error='webhook_endpoint_resolver_invalid';return false;}
    $request=array('company_id'=>(int)$target['company_id'],'event_type'=>PAY_API_001_WEBHOOK_EVENT_TYPE,'endpoint_config_ref_sha256'=>(string)$target['endpoint_config_ref_sha256']);
    try{$endpoint=call_user_func($resolver,$request);}catch(Throwable $t){$error='webhook_endpoint_resolver_failed';return false;}
    if(!is_array($endpoint)||array_keys($endpoint)!==array('endpoint_config_ref_sha256','path')){$error='webhook_endpoint_resolver_invalid';return false;}
    return pay_api_001_webhook_endpoint_material($target,$endpoint,$error);
}

/** Load one exact active claim-bound immutable delivery context without exposing secrets. */
function pay_api_001_webhook_claimed_delivery_context($company_id,$webhook_outbox_id,$claim_token_sha256,&$error=null)
{
    $error=null;$actor=pay_api_001_runtime_actor();$company=pay_api_001_uint_value($company_id,true,4294967295);$outbox=pay_api_001_positive_id($webhook_outbox_id);$token=pay_api_001_webhook_claim_token($claim_token_sha256);
    if($actor<=0||!pay_api_001_runtime_access('SA_HRM_EXECUTE_PAY_API')||$company===false||$outbox===false||$token===false||!pay_api_001_webhook_delivery_runtime_enabled('delivery')){$error='webhook_delivery_access_or_version_denied';return false;}
    if(!pay_api_001_runtime_require_company($company,$error))return false;$now=pay_api_001_runtime_now();$manage=pay_api_001_runtime_manage_transaction();if($manage)begin_transaction();
    $o=pay_api_001_runtime_one('SELECT o.*,e.company_id,e.event_type,e.payload_json,e.payload_sha256 event_payload_sha256,t.endpoint_config_ref_sha256,t.signing_key_ref_sha256,t.webhook_target_id target_id FROM '.TB_PREF.'hrm_pay_api_001_webhook_outbox o INNER JOIN '.TB_PREF.'hrm_pay_api_001_webhook_events e ON e.webhook_event_id=o.webhook_event_id INNER JOIN '.TB_PREF.'hrm_pay_api_001_webhook_targets t ON t.webhook_target_id=o.webhook_target_id WHERE o.webhook_outbox_id='.$outbox.' LIMIT 2 FOR UPDATE','could not lock PAY-API-001 claimed delivery outbox');
    if(!$o||(int)$o['company_id']!==$company||(string)$o['event_type']!==PAY_API_001_WEBHOOK_EVENT_TYPE||!hash_equals((string)$o['payload_sha256'],(string)$o['event_payload_sha256'])){if($manage)cancel_transaction();$error='webhook_delivery_scope_or_payload_invalid';return false;}
    $claim=pay_api_001_runtime_one('SELECT * FROM '.TB_PREF.'hrm_pay_api_001_webhook_claim_leases WHERE webhook_outbox_id='.$outbox.' AND claim_token_sha256='.db_escape($token).' LIMIT 2 FOR UPDATE','could not lock PAY-API-001 delivery claim');
    if(!$claim||(int)$claim['claimed_by']!==$actor||strcmp((string)$claim['expires_at'],$now)<=0){if($manage)cancel_transaction();$error='webhook_delivery_claim_invalid_or_expired';return false;}
    $target=pay_api_001_webhook_active_target((int)$o['target_id'],$company,$now,$error);$cfg=$target?pay_api_001_webhook_runtime_configuration((int)$o['target_id'],$company,$now,$error):false;if(!$target||!$cfg){if($manage)cancel_transaction();return false;}
    if((int)$claim['webhook_delivery_policy_id']!==(int)$cfg['delivery_policy']['webhook_delivery_policy_id']||!hash_equals((string)$target['signing_key_ref_sha256'],(string)$cfg['signing_profile']['secret_ref_sha256'])){if($manage)cancel_transaction();$error='webhook_delivery_configuration_mismatch';return false;}
    $dead=db_query('SELECT webhook_dead_letter_id FROM '.TB_PREF.'hrm_pay_api_001_webhook_dead_letters WHERE webhook_outbox_id='.$outbox.' LIMIT 1 FOR UPDATE');$attempt=db_query('SELECT webhook_delivery_attempt_id FROM '.TB_PREF.'hrm_pay_api_001_webhook_delivery_attempts WHERE webhook_outbox_id='.$outbox.' LIMIT 1 FOR UPDATE');$defense=db_query('SELECT webhook_replay_defense_id FROM '.TB_PREF.'hrm_pay_api_001_webhook_replay_defenses WHERE webhook_outbox_id='.$outbox.' LIMIT 1 FOR UPDATE');
    if(!$dead||!$attempt||!$defense||db_num_rows($dead)>0||db_num_rows($attempt)>0||db_num_rows($defense)>0){if($manage)cancel_transaction();$error='webhook_delivery_initial_dispatch_not_available';return false;}
    if($manage)commit_transaction();return array('company_id'=>$company,'actor_id'=>$actor,'outbox'=>$o,'claim'=>$claim,'target'=>$target,'delivery_policy'=>$cfg['delivery_policy'],'tls_policy'=>$cfg['tls_policy'],'signing_profile'=>$cfg['signing_profile'],'claim_token_sha256'=>$token,'loaded_at'=>$now);
}

/** Append one replay-defense dispatch reservation before the irreversible network side effect. */
function pay_api_001_webhook_reserve_initial_dispatch(array $context,array $request,&$error=null)
{
    $error=null;$company=pay_api_001_uint_value($context['company_id']??null,true,4294967295);$actor=pay_api_001_positive_id($context['actor_id']??null);$outbox=pay_api_001_positive_id($context['outbox']['webhook_outbox_id']??null);$claim=pay_api_001_positive_id($context['claim']['webhook_claim_lease_id']??null);$token=pay_api_001_webhook_claim_token($context['claim_token_sha256']??null);$session_actor=pay_api_001_runtime_actor();
    if($company===false||$actor===false||$outbox===false||$claim===false||$token===false||$session_actor!==$actor||!pay_api_001_runtime_access('SA_HRM_EXECUTE_PAY_API')||!pay_api_001_webhook_delivery_runtime_enabled('delivery')||!pay_api_001_webhook_curl_request_valid($request)||$request['webhook_outbox_id']!==$outbox||!hash_equals((string)$context['outbox']['payload_sha256'],(string)$request['payload_sha256'])||!hash_equals((string)$context['outbox']['queue_key_sha256'],(string)$request['queue_key_sha256'])||strtolower((string)($context['tls_policy']['destination_host']??''))!==$request['host']||(string)($context['tls_policy']['minimum_tls_version']??'')!==$request['minimum_tls_version']){$error='webhook_dispatch_reservation_input_invalid';return false;}
    if(!pay_api_001_runtime_require_company($company,$error))return false;
    $now=pay_api_001_runtime_now();$manage=pay_api_001_runtime_manage_transaction();if($manage)begin_transaction();
    $c=pay_api_001_runtime_one('SELECT * FROM '.TB_PREF.'hrm_pay_api_001_webhook_claim_leases WHERE webhook_claim_lease_id='.$claim.' AND webhook_outbox_id='.$outbox.' AND claim_token_sha256='.db_escape($token).' LIMIT 2 FOR UPDATE','could not relock PAY-API-001 dispatch claim');
    if(!$c||(int)$c['claimed_by']!==$actor||strcmp((string)$c['expires_at'],$now)<=0){if($manage)cancel_transaction();$error='webhook_dispatch_claim_expired';return false;}
    $current_target=pay_api_001_webhook_active_target((int)($context['target']['webhook_target_id']??0),$company,$now,$error);$current_cfg=$current_target?pay_api_001_webhook_runtime_configuration((int)$current_target['webhook_target_id'],$company,$now,$error):false;
    if(!$current_target||!$current_cfg||!hash_equals((string)($context['target']['target_sha256']??''),(string)$current_target['target_sha256'])||(int)($context['delivery_policy']['webhook_delivery_policy_id']??0)!==(int)$current_cfg['delivery_policy']['webhook_delivery_policy_id']||(int)($context['tls_policy']['webhook_egress_tls_policy_id']??0)!==(int)$current_cfg['tls_policy']['webhook_egress_tls_policy_id']||(int)($context['signing_profile']['webhook_signing_profile_id']??0)!==(int)$current_cfg['signing_profile']['webhook_signing_profile_id']||!hash_equals((string)($context['signing_profile']['secret_ref_sha256']??''),(string)$current_cfg['signing_profile']['secret_ref_sha256'])){if($manage)cancel_transaction();$error=$error?:'webhook_dispatch_configuration_changed';return false;}
    if((int)$c['webhook_delivery_policy_id']!==(int)$current_cfg['delivery_policy']['webhook_delivery_policy_id']){if($manage)cancel_transaction();$error='webhook_dispatch_claim_policy_changed';return false;}
    $existing=db_query('SELECT webhook_replay_defense_id FROM '.TB_PREF.'hrm_pay_api_001_webhook_replay_defenses WHERE webhook_outbox_id='.$outbox.' LIMIT 1 FOR UPDATE');$attempt=db_query('SELECT webhook_delivery_attempt_id FROM '.TB_PREF.'hrm_pay_api_001_webhook_delivery_attempts WHERE webhook_outbox_id='.$outbox.' LIMIT 1 FOR UPDATE');
    if(!$existing||!$attempt||db_num_rows($existing)>0||db_num_rows($attempt)>0){if($manage)cancel_transaction();$error='webhook_dispatch_already_reserved_or_attempted';return false;}
    $replay_key=pay_api_001_provisioning_hash(array('webhook_outbox_id'=>$outbox,'webhook_claim_lease_id'=>$claim,'claim_token_sha256'=>$token,'request_sha256'=>$request['request_sha256'],'payload_sha256'=>$request['payload_sha256']));
    $defense_sha=pay_api_001_provisioning_hash(array('webhook_outbox_id'=>$outbox,'replay_key_sha256'=>$replay_key,'payload_sha256'=>$request['payload_sha256'],'valid_until'=>$c['expires_at'],'recorded_by'=>$actor,'recorded_at'=>$now));
    if($replay_key===false||$defense_sha===false){if($manage)cancel_transaction();$error='webhook_dispatch_reservation_hash_failed';return false;}
    $sql='INSERT INTO '.TB_PREF.'hrm_pay_api_001_webhook_replay_defenses (webhook_outbox_id,replay_key_sha256,payload_sha256,valid_until,defense_sha256,recorded_by,recorded_at,created_at) VALUES ('.$outbox.','.db_escape($replay_key).','.db_escape($request['payload_sha256']).','.db_escape($c['expires_at']).','.db_escape($defense_sha).','.$actor.','.db_escape($now).','.db_escape($now).')';
    if(!db_query($sql)){if($manage)cancel_transaction();$error='webhook_dispatch_reservation_insert_failed';return false;}$id=(int)db_insert_id();if($manage)commit_transaction();return array('webhook_replay_defense_id'=>$id,'replay_key_sha256'=>$replay_key,'valid_until'=>$c['expires_at']);
}

/** Persist the immutable first delivery attempt; optional provider response is atomic with it. */
function pay_api_001_record_initial_webhook_delivery($company_id,$webhook_outbox_id,$replay_key_sha256,array $result,&$error=null)
{
    $error=null;$actor=pay_api_001_runtime_actor();$company=pay_api_001_uint_value($company_id,true,4294967295);$outbox=pay_api_001_positive_id($webhook_outbox_id);$evidence=pay_api_001_webhook_delivery_result_evidence($result,$error);
    if($actor<=0||!pay_api_001_runtime_access('SA_HRM_EXECUTE_PAY_API')||$company===false||$outbox===false||!pay_api_001_is_sha256($replay_key_sha256)||$evidence===false||!pay_api_001_webhook_delivery_runtime_enabled('attempt')){$error=$error?:'webhook_delivery_attempt_access_or_version_denied';return false;}
    if($evidence['definite_provider_response']&&!pay_api_001_webhook_delivery_runtime_enabled('response')){$error='webhook_provider_response_writer_not_enabled';return false;}if(!pay_api_001_runtime_require_company($company,$error))return false;$now=pay_api_001_runtime_now();
    $manage=pay_api_001_runtime_manage_transaction();if($manage)begin_transaction();$o=pay_api_001_runtime_one('SELECT o.webhook_outbox_id,e.company_id,o.payload_sha256 FROM '.TB_PREF.'hrm_pay_api_001_webhook_outbox o INNER JOIN '.TB_PREF.'hrm_pay_api_001_webhook_events e ON e.webhook_event_id=o.webhook_event_id WHERE o.webhook_outbox_id='.$outbox.' LIMIT 2 FOR UPDATE','could not lock PAY-API-001 delivery evidence outbox');
    $d=pay_api_001_runtime_one('SELECT * FROM '.TB_PREF.'hrm_pay_api_001_webhook_replay_defenses WHERE webhook_outbox_id='.$outbox.' AND replay_key_sha256='.db_escape($replay_key_sha256).' LIMIT 2 FOR UPDATE','could not lock PAY-API-001 replay defense');$existing=db_query('SELECT webhook_delivery_attempt_id FROM '.TB_PREF.'hrm_pay_api_001_webhook_delivery_attempts WHERE webhook_outbox_id='.$outbox.' LIMIT 1 FOR UPDATE');
    if(!$o||(int)$o['company_id']!==$company||!$d||!hash_equals((string)$d['payload_sha256'],(string)$o['payload_sha256'])||!$existing||db_num_rows($existing)>0){if($manage)cancel_transaction();$error='webhook_delivery_evidence_lineage_invalid';return false;}
    $response_sha=null;if($evidence['definite_provider_response']){$response_sha=pay_api_001_provisioning_hash(array('webhook_outbox_id'=>$outbox,'attempt_no'=>1,'http_status_code'=>$evidence['http_status_code'],'provider_reference_sha256'=>null,'response_headers_sha256'=>$evidence['response_headers_sha256'],'response_body_sha256'=>$evidence['response_body_sha256'],'response_body_bytes'=>$evidence['response_body_bytes']));if($response_sha===false){if($manage)cancel_transaction();$error='webhook_provider_response_hash_failed';return false;}}
    $attempt_sha=pay_api_001_provisioning_hash(array('webhook_outbox_id'=>$outbox,'attempt_no'=>1,'outcome_code'=>$evidence['outcome_code'],'http_status_code'=>$evidence['http_status_code'],'response_sha256'=>$response_sha,'error_code'=>$evidence['error_code'],'replay_key_sha256'=>$replay_key_sha256,'attempted_by'=>$actor,'attempted_at'=>$now));if($attempt_sha===false){if($manage)cancel_transaction();$error='webhook_delivery_attempt_hash_failed';return false;}
    $sql='INSERT INTO '.TB_PREF.'hrm_pay_api_001_webhook_delivery_attempts (webhook_outbox_id,attempt_no,outcome_code,http_status_code,provider_reference_sha256,response_sha256,error_code,attempt_sha256,attempted_by,attempted_at,created_at) VALUES ('.$outbox.',1,'.db_escape($evidence['outcome_code']).','.($evidence['http_status_code']===null?'NULL':(int)$evidence['http_status_code']).',NULL,'.($response_sha===null?'NULL':db_escape($response_sha)).','.($evidence['error_code']===null?'NULL':db_escape($evidence['error_code'])).','.db_escape($attempt_sha).','.$actor.','.db_escape($now).','.db_escape($now).')';
    if(!db_query($sql)){if($manage)cancel_transaction();$error='webhook_delivery_attempt_insert_failed';return false;}$attempt_id=(int)db_insert_id();$response_id=null;
    if($evidence['definite_provider_response']){$sql='INSERT INTO '.TB_PREF.'hrm_pay_api_001_webhook_provider_responses (webhook_delivery_attempt_id,http_status_code,provider_reference_sha256,response_headers_sha256,response_body_sha256,response_body_bytes,response_sha256,recorded_at,created_at) VALUES ('.$attempt_id.','.(int)$evidence['http_status_code'].',NULL,'.db_escape($evidence['response_headers_sha256']).','.db_escape($evidence['response_body_sha256']).','.(int)$evidence['response_body_bytes'].','.db_escape($response_sha).','.db_escape($now).','.db_escape($now).')';if(!db_query($sql)){if($manage)cancel_transaction();$error='webhook_provider_response_insert_failed';return false;}$response_id=(int)db_insert_id();}
    if($manage)commit_transaction();return array('webhook_delivery_attempt_id'=>$attempt_id,'webhook_provider_response_id'=>$response_id,'classification'=>$evidence['outcome_code'],'http_status'=>$evidence['http_status_code'],'response_sha256'=>$response_sha);
}

/** Execute exactly one initial claim-bound delivery; never retries, reprocesses, or receives. */
function pay_api_001_deliver_claimed_webhook_outbox($company_id,$webhook_outbox_id,$claim_token_sha256,$endpoint_resolver,$secret_resolver,&$error=null)
{
    $error=null;if(!is_callable($endpoint_resolver)||!is_callable($secret_resolver)||!pay_api_001_webhook_delivery_runtime_enabled('delivery')){$error='webhook_delivery_executor_input_or_version_denied';return false;}
    if(!pay_api_001_runtime_manage_transaction()){$error='webhook_delivery_outer_transaction_denied';return false;}$t=time();
    $context=pay_api_001_webhook_claimed_delivery_context($company_id,$webhook_outbox_id,$claim_token_sha256,$error);if($context===false)return false;
    $endpoint=pay_api_001_webhook_resolve_endpoint($context['target'],$endpoint_resolver,$error);if($endpoint===false)return false;
    $secret_ref=array('company_id'=>$context['company_id'],'ref_kind'=>(string)$context['signing_profile']['ref_kind'],'provider_key'=>(string)$context['signing_profile']['provider_key'],'provider_reference_sha256'=>(string)$context['signing_profile']['provider_reference_sha256'],'secret_ref_sha256'=>(string)$context['signing_profile']['secret_ref_sha256']);
    $secret=pay_api_001_webhook_resolve_signing_secret($secret_ref,$secret_resolver,$error);if($secret===false)return false;
    $delivery=array('webhook_outbox_id'=>(int)$context['outbox']['webhook_outbox_id'],'event_type'=>(string)$context['outbox']['event_type'],'payload_json'=>(string)$context['outbox']['payload_json'],'payload_sha256'=>(string)$context['outbox']['payload_sha256'],'queue_key_sha256'=>(string)$context['outbox']['queue_key_sha256']);
    $request=pay_api_001_webhook_signed_request($delivery,$context['tls_policy'],$context['signing_profile'],$context['target'],$endpoint,$secret,$t,$error);$secret='';if($request===false)return false;
    $reservation=pay_api_001_webhook_reserve_initial_dispatch($context,$request,$error);if($reservation===false)return false;
    $result=pay_api_001_webhook_curl_transport($request);if(!is_array($result)){$error='webhook_delivery_transport_result_invalid';return false;}
    $persist=pay_api_001_record_initial_webhook_delivery($context['company_id'],$request['webhook_outbox_id'],$reservation['replay_key_sha256'],$result,$error);
    if($persist===false){$error='webhook_delivery_evidence_persistence_failed_after_dispatch_reservation'.($error?':'.$error:'');return false;}
    return array('webhook_replay_defense_id'=>$reservation['webhook_replay_defense_id'],'webhook_delivery_attempt_id'=>$persist['webhook_delivery_attempt_id'],'webhook_provider_response_id'=>$persist['webhook_provider_response_id'],'classification'=>$persist['classification'],'http_status'=>$persist['http_status'],'retry_executed'=>false,'reprocess_executed'=>false);
}
?>
