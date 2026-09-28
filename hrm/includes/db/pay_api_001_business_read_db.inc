<?php
/** PAY-API-001 governed payroll-period summary dispatcher and response-audit runtime. */
include_once($path_to_root.'/includes/pay_api_001_business_read.inc');
include_once(dirname(__FILE__).'/pay_api_001_runtime_db.inc');

function pay_api_001_record_read_response_audit(array $event,&$error=null)
{
    $error=null;
    $request=pay_api_001_positive_id($event['api_read_request_id']??null);$company=pay_api_001_uint_value($event['company_id']??null,true,4294967295);
    $outcome=isset($event['outcome_code'])?pay_api_001_provisioning_token((string)$event['outcome_code'],48):false;
    $rows=isset($event['row_count'])?pay_api_001_uint_value($event['row_count'],true,100):false;
    foreach(array('resource_key_sha256','object_scope_sha256') as $k)if(!isset($event[$k])||!pay_api_001_is_sha256($event[$k])){$error='read_response_audit_hash_invalid';return false;}
    $payload=array_key_exists('payload_sha256',$event)&&$event['payload_sha256']!==null?(pay_api_001_is_sha256($event['payload_sha256'])?(string)$event['payload_sha256']:false):null;
    if($request===false||$company===false||$outcome===false||$rows===false||$payload===false){$error='read_response_audit_input_invalid';return false;}
    if(($rows===0)!==($payload===null)){$error='read_response_audit_payload_cardinality_invalid';return false;}
    $now=isset($event['occurred_at'])&&is_string($event['occurred_at'])?pay_api_001_provisioning_datetime($event['occurred_at']):pay_api_001_runtime_now();if($now===false){$error='read_response_audit_time_invalid';return false;}
    $sha=pay_api_001_provisioning_hash(array('api_read_request_id'=>$request,'company_id'=>$company,'resource_key_sha256'=>$event['resource_key_sha256'],'object_scope_sha256'=>$event['object_scope_sha256'],'outcome_code'=>$outcome,'payload_sha256'=>$payload,'row_count'=>$rows));
    $manage=pay_api_001_runtime_manage_transaction();if($manage)begin_transaction();
    $q=db_query('SELECT api_read_response_id,response_sha256,outcome_code,payload_sha256,row_count FROM '.TB_PREF.'hrm_pay_api_001_read_responses WHERE api_read_request_id='.$request.' LIMIT 2 FOR UPDATE');
    if(!$q||db_num_rows($q)>1){if($manage)cancel_transaction();$error='read_response_audit_replay_state_invalid';return false;}
    if(db_num_rows($q)===1){$old=db_fetch_assoc($q);if(!hash_equals((string)$old['response_sha256'],$sha)){if($manage)cancel_transaction();$error='read_response_audit_replay_mismatch';return false;}if($manage)commit_transaction();return array('api_read_response_id'=>(int)$old['api_read_response_id'],'response_sha256'=>$sha,'replayed'=>true);}
    $sql='INSERT INTO '.TB_PREF.'hrm_pay_api_001_read_responses (api_read_request_id,company_id,resource_key_sha256,object_scope_sha256,outcome_code,payload_sha256,row_count,response_sha256,occurred_at,created_at) VALUES ('.$request.','.$company.','.db_escape($event['resource_key_sha256']).','.db_escape($event['object_scope_sha256']).','.db_escape($outcome).','.($payload===null?'NULL':db_escape($payload)).','.$rows.','.db_escape($sha).','.db_escape($now).','.db_escape($now).')';
    if(!db_query($sql)){if($manage)cancel_transaction();$error='read_response_audit_insert_failed';return false;}
    $id=(int)db_insert_id();if($manage)commit_transaction();return array('api_read_response_id'=>$id,'response_sha256'=>$sha,'replayed'=>false);
}

function pay_api_001_dispatch_authorized_read($request,$preroute,&$error=null)
{
    $error=null;
    if(!pay_api_001_business_data_dispatch_enabled()){$error='business_data_dispatch_disabled';return false;}
    if(!is_array($request)||!is_array($preroute)||empty($preroute['allowed'])){$error='business_data_dispatch_context_invalid';return false;}
    $company=pay_api_001_uint_value($request['company_id']??null,true,4294967295);$read_request=pay_api_001_positive_id($preroute['api_read_request_id']??null);
    if($company===false||$read_request===false||$company!==pay_api_001_uint_value($preroute['company_id']??null,true,4294967295)){$error='business_data_dispatch_context_mismatch';return false;}
    $spec=pay_api_001_payroll_period_summary_request($request['resource_key']??null,$request['contract_version']??null,$request['object_scope_type']??null,$request['object_scope_key']??null,$request['cursor_sha256']??null);
    if($spec===false)return array('ok'=>false,'status'=>404,'code'=>'business_data_not_found');
    $resource_sha=hash('sha256',$spec['resource_key'].':'.$spec['contract_version']);$scope_sha=hash('sha256',$spec['scope_type'].':'.$spec['scope_key']);
    $sql='SELECT period_id,from_date,to_date,pay_date,status,CAST(total_gross AS CHAR) total_gross,CAST(total_deductions AS CHAR) total_deductions,CAST(total_net AS CHAR) total_net,CAST(total_employer_cost AS CHAR) total_employer_cost FROM '.TB_PREF.'payroll_periods WHERE period_id='.(int)$spec['period_id'].' AND status IN (2,3,4,5,6) LIMIT 2';
    $q=db_query($sql,'could not read PAY-API-001 payroll-period summary');if(!$q||db_num_rows($q)>1){$error='payroll_period_summary_read_failed';return false;}
    if(db_num_rows($q)===0){$ae=null;$a=pay_api_001_record_read_response_audit(array('api_read_request_id'=>$read_request,'company_id'=>$company,'resource_key_sha256'=>$resource_sha,'object_scope_sha256'=>$scope_sha,'outcome_code'=>'not_found','payload_sha256'=>null,'row_count'=>0),$ae);if($a===false){$error=$ae;return false;}return array('ok'=>false,'status'=>404,'code'=>'business_data_not_found');}
    $data=pay_api_001_payroll_period_summary_project_row(db_fetch_assoc($q));if($data===false){$error='payroll_period_summary_projection_invalid';return false;}
    $body=array('ok'=>true,'resource'=>$spec['resource_key'],'contract'=>$spec['contract_version'],'data'=>$data);$json=json_encode($body,JSON_UNESCAPED_SLASHES|JSON_UNESCAPED_UNICODE);if(!is_string($json)){$error='payroll_period_summary_encoding_failed';return false;}
    $ae=null;$a=pay_api_001_record_read_response_audit(array('api_read_request_id'=>$read_request,'company_id'=>$company,'resource_key_sha256'=>$resource_sha,'object_scope_sha256'=>$scope_sha,'outcome_code'=>'returned','payload_sha256'=>hash('sha256',$json),'row_count'=>1),$ae);if($a===false){$error=$ae;return false;}
    return array('ok'=>true,'status'=>200,'code'=>'business_data_returned','body'=>$body);
}
?>
