<?php
/** PAY-API-001 governed machine-authenticated read pre-controller runtime. Public HTTP exposure remains disabled. */
include_once(dirname(__FILE__).'/pay_api_001_runtime_db.inc');
include_once($path_to_root.'/includes/pay_api_001_http_readonly.inc');

function pay_api_001_record_read_request_audit(array $event,&$error=null)
{
    $error=null;
    $company=pay_api_001_uint_value($event['company_id']??null,true,4294967295);
    $authn=array_key_exists('api_authn_decision_id',$event)&&$event['api_authn_decision_id']!==null?pay_api_001_positive_id($event['api_authn_decision_id']):null;
    $principal=array_key_exists('api_principal_id',$event)&&$event['api_principal_id']!==null?pay_api_001_positive_id($event['api_principal_id']):null;
    $resource=array_key_exists('api_resource_id',$event)&&$event['api_resource_id']!==null?pay_api_001_positive_id($event['api_resource_id']):null;
    $rate=array_key_exists('api_rate_limit_decision_id',$event)&&$event['api_rate_limit_decision_id']!==null?pay_api_001_positive_id($event['api_rate_limit_decision_id']):null;
    $method=isset($event['method'])?strtoupper((string)$event['method']):'';
    $outcome=isset($event['outcome_code'])?pay_api_001_provisioning_token((string)$event['outcome_code'],48):false;
    foreach(array('resource_key_sha256','request_target_sha256','correlation_sha256') as $k)if(!isset($event[$k])||!pay_api_001_is_sha256($event[$k])){$error='read_request_audit_hash_invalid';return false;}
    if($company===false||($authn===false)||($principal===false)||($resource===false)||($rate===false)||preg_match('/^[A-Z]{3,8}$/D',$method)!==1||$outcome===false){$error='read_request_audit_input_invalid';return false;}
    $fingerprint=pay_api_001_provisioning_hash(array(
        'company_id'=>$company,'api_authn_decision_id'=>$authn,'api_principal_id'=>$principal,'api_resource_id'=>$resource,'api_rate_limit_decision_id'=>$rate,
        'method'=>$method,'resource_key_sha256'=>$event['resource_key_sha256'],'request_target_sha256'=>$event['request_target_sha256'],
        'outcome_code'=>$outcome,'correlation_sha256'=>$event['correlation_sha256']));
    $now=isset($event['occurred_at'])&&is_string($event['occurred_at'])?pay_api_001_provisioning_datetime($event['occurred_at']):pay_api_001_runtime_now();
    if($now===false){$error='read_request_audit_time_invalid';return false;}
    $manage=pay_api_001_runtime_manage_transaction();if($manage)begin_transaction();
    $q=db_query('SELECT api_read_request_id,request_sha256,outcome_code FROM '.TB_PREF.'hrm_pay_api_001_read_requests WHERE company_id='.$company.' AND correlation_sha256='.db_escape($event['correlation_sha256']).' LIMIT 2 FOR UPDATE');
    if(!$q||db_num_rows($q)>1){if($manage)cancel_transaction();$error='read_request_audit_replay_state_invalid';return false;}
    if(db_num_rows($q)===1){
        $old=db_fetch_assoc($q);
        if(!hash_equals((string)$old['request_sha256'],$fingerprint)){if($manage)cancel_transaction();$error='read_request_correlation_reuse_mismatch';return false;}
        if($manage)commit_transaction();
        return array('api_read_request_id'=>(int)$old['api_read_request_id'],'request_sha256'=>$old['request_sha256'],'outcome_code'=>$old['outcome_code'],'replayed'=>true);
    }
    $sql='INSERT INTO '.TB_PREF.'hrm_pay_api_001_read_requests (company_id,api_authn_decision_id,api_principal_id,api_resource_id,api_rate_limit_decision_id,method,resource_key_sha256,request_target_sha256,outcome_code,correlation_sha256,request_sha256,occurred_at,created_at) VALUES ('.
        $company.','.($authn===null?'NULL':$authn).','.($principal===null?'NULL':$principal).','.($resource===null?'NULL':$resource).','.($rate===null?'NULL':$rate).','.db_escape($method).','.db_escape($event['resource_key_sha256']).','.db_escape($event['request_target_sha256']).','.db_escape($outcome).','.db_escape($event['correlation_sha256']).','.db_escape($fingerprint).','.db_escape($now).','.db_escape($now).')';
    if(!db_query($sql)){
        $retry=db_query('SELECT api_read_request_id,request_sha256,outcome_code FROM '.TB_PREF.'hrm_pay_api_001_read_requests WHERE company_id='.$company.' AND correlation_sha256='.db_escape($event['correlation_sha256']).' LIMIT 2 FOR UPDATE');
        if($retry&&db_num_rows($retry)===1){
            $old=db_fetch_assoc($retry);
            if(hash_equals((string)$old['request_sha256'],$fingerprint)){if($manage)commit_transaction();return array('api_read_request_id'=>(int)$old['api_read_request_id'],'request_sha256'=>$old['request_sha256'],'outcome_code'=>$old['outcome_code'],'replayed'=>true);}
        }
        if($manage)cancel_transaction();$error='read_request_audit_insert_failed';return false;
    }
    $id=(int)db_insert_id();if($manage)commit_transaction();
    return array('api_read_request_id'=>$id,'request_sha256'=>$fingerprint,'outcome_code'=>$outcome,'replayed'=>false);
}

function pay_api_001_preroute_read_request($method,$transport_secure,$headers,$query,$path,$company_id,$resource_key,$contract_version,$object_scope_type,$object_scope_key,$limit,$cursor_sha256=null,$unix_time=null,&$error=null)
{
    $error=null;
    $env=pay_api_001_http_preroute_envelope($method,$transport_secure,$headers,$query,$path,$company_id,$resource_key,$contract_version,$object_scope_type,$object_scope_key,$limit,$cursor_sha256);
    if(empty($env['allowed'])){$error=$env['code'];return $env;}
    $request_time=$unix_time===null?null:pay_api_001_uint_value($unix_time,true,PHP_INT_MAX);
    if($unix_time!==null&&$request_time===false){$error='request_clock_invalid';return false;}
    $occurred=$request_time===null?pay_api_001_runtime_now():gmdate('Y-m-d H:i:s',$request_time);
    $auth_error=null;
    $auth=pay_api_001_authenticate_machine_credential($env['company_id'],$env['credential_key'],$env['presented_secret'],$env['correlation_sha256'],$unix_time,$auth_error);
    $env['presented_secret']=null;unset($env['presented_secret']);
    if($auth===false){$error=$auth_error?:'authentication_runtime_failed';return false;}
    if(empty($auth['authenticated'])){
        $audit_error=null;$audit=pay_api_001_record_read_request_audit(array('company_id'=>$env['company_id'],'api_authn_decision_id'=>$auth['api_authn_decision_id'],'api_principal_id'=>null,'api_resource_id'=>null,'api_rate_limit_decision_id'=>null,'method'=>'GET','resource_key_sha256'=>$env['resource_key_sha256'],'request_target_sha256'=>$env['request_target_sha256'],'outcome_code'=>'authentication_failed','correlation_sha256'=>$env['correlation_sha256'],'occurred_at'=>$occurred),$audit_error);
        if($audit===false){$error=$audit_error;return false;}
        return array('allowed'=>false,'code'=>'authentication_required','api_read_request_id'=>$audit['api_read_request_id']);
    }
    $authority_error=null;
    $authority=pay_api_001_resolve_machine_read_authority($env['company_id'],$auth['api_principal_id'],$env['resource_key'],$env['contract_version'],$env['object_scope_type'],$env['object_scope_key'],$authority_error);
    if($authority===false){
        $audit_error=null;$audit=pay_api_001_record_read_request_audit(array('company_id'=>$env['company_id'],'api_authn_decision_id'=>$auth['api_authn_decision_id'],'api_principal_id'=>$auth['api_principal_id'],'api_resource_id'=>null,'api_rate_limit_decision_id'=>null,'method'=>'GET','resource_key_sha256'=>$env['resource_key_sha256'],'request_target_sha256'=>$env['request_target_sha256'],'outcome_code'=>'authorization_failed','correlation_sha256'=>$env['correlation_sha256'],'occurred_at'=>$occurred),$audit_error);
        if($audit===false){$error=$audit_error;return false;}
        return array('allowed'=>false,'code'=>'authorization_required','api_read_request_id'=>$audit['api_read_request_id']);
    }
    $rate_error=null;$rate=pay_api_001_enforce_machine_read_rate_limit($auth,$authority,$env['correlation_sha256'],$unix_time,$rate_error);
    if($rate===false){$error=$rate_error?:'rate_limit_runtime_failed';return false;}
    $adapter=pay_api_001_readonly_adapter_gate('GET',true,$query,$auth,$authority['resource'],$authority['scope'],$env['company_id'],$env['object_scope_type'],$env['object_scope_key'],$rate,$env['page']['limit'],$env['page']['cursor_sha256']);
    $outcome=!empty($adapter['allowed'])?'readonly_preroute_allowed':((string)($rate['code']??'')==='rate_limit_exceeded'?'rate_limit_exceeded':'readonly_preroute_denied');
    $audit_error=null;$audit=pay_api_001_record_read_request_audit(array(
        'company_id'=>$env['company_id'],'api_authn_decision_id'=>$auth['api_authn_decision_id'],'api_principal_id'=>$auth['api_principal_id'],
        'api_resource_id'=>$authority['resource']['api_resource_id'],'api_rate_limit_decision_id'=>$rate['api_rate_limit_decision_id'],
        'method'=>'GET','resource_key_sha256'=>$env['resource_key_sha256'],'request_target_sha256'=>$env['request_target_sha256'],
        'outcome_code'=>$outcome,'correlation_sha256'=>$env['correlation_sha256'],'occurred_at'=>$occurred),$audit_error);
    if($audit===false){$error=$audit_error;return false;}
    if(empty($adapter['allowed']))return array('allowed'=>false,'code'=>$rate['code']==='rate_limit_exceeded'?'rate_limit_exceeded':$adapter['code'],'api_read_request_id'=>$audit['api_read_request_id']);
    return array('allowed'=>true,'code'=>'readonly_preroute_allowed','company_id'=>$env['company_id'],'api_principal_id'=>$auth['api_principal_id'],'api_resource_id'=>$authority['resource']['api_resource_id'],'api_principal_scope_id'=>$authority['scope']['api_principal_scope_id'],'api_rate_limit_policy_id'=>$authority['rate_policy']['api_rate_limit_policy_id'],'api_rate_limit_decision_id'=>$rate['api_rate_limit_decision_id'],'api_read_request_id'=>$audit['api_read_request_id'],'page'=>$adapter['page']);
}
?>
