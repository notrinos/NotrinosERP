<?php
/** PAY-API-001 machine authentication, read authority and atomic read-rate runtime service. */
require_once(__DIR__.'/../../../includes/value_helpers.inc');
include_once($path_to_root.'/includes/pay_api_001_provisioning.inc');
include_once($path_to_root.'/includes/pay_api_001_rate_limit.inc');
include_once($path_to_root.'/includes/pay_api_001_contract.inc');

function pay_api_001_runtime_company(){return isset($_SESSION['wa_current_user']->company)?pay_api_001_uint_value($_SESSION['wa_current_user']->company,true,4294967295):false;}
function pay_api_001_runtime_require_company($company,&$error){$current=pay_api_001_runtime_company();if($current===false||$current!==pay_api_001_uint_value($company,true,4294967295)){$error='company_scope_denied';return false;}return true;}
function pay_api_001_runtime_access($token){return function_exists('user_check_access')&&user_check_access($token);}
function pay_api_001_runtime_manage_transaction(){return empty($GLOBALS['transaction_level']);}
function pay_api_001_runtime_one($sql,$message){$q=db_query($sql,$message);return $q&&db_num_rows($q)===1?db_fetch_assoc($q):false;}
function pay_api_001_runtime_now(){return gmdate('Y-m-d H:i:s');}
/** Return the configured company database version for runtime authority checks. */
function pay_api_001_runtime_database_version(){if(!function_exists('get_company_pref'))return false;$v=get_company_pref('version_id',true);return is_string($v)?$v:false;}

function pay_api_001_rate_limit_runtime_core($company_id,$api_principal_id,$api_resource_id,$api_rate_limit_policy_id,$correlation_sha256,$unix_time,$require_browser_company,$api_principal_scope_id,&$error)
{
    $error=null;$company=pay_api_001_uint_value($company_id,true,4294967295);
    if($company===false){$error='rate_limit_input_invalid';return false;}
    if($require_browser_company&&!pay_api_001_runtime_require_company($company,$error))return false;
    $principal=pay_api_001_positive_id($api_principal_id);$resource=pay_api_001_positive_id($api_resource_id);$policy_id=pay_api_001_positive_id($api_rate_limit_policy_id);
    $scope_id=$api_principal_scope_id===null?null:pay_api_001_positive_id($api_principal_scope_id);
    if($principal===false||$resource===false||$policy_id===false||($api_principal_scope_id!==null&&$scope_id===false)||!is_sha256_hex($correlation_sha256)){$error='rate_limit_input_invalid';return false;}
    $t=$unix_time===null?time():pay_api_001_uint_value($unix_time,true,PHP_INT_MAX);if($t===false){$error='rate_limit_clock_invalid';return false;}
    $manage=pay_api_001_runtime_manage_transaction();if($manage)begin_transaction();
    $pr=pay_api_001_runtime_one('SELECT company_id,principal_status FROM '.TB_PREF.'hrm_pay_api_001_machine_principals WHERE api_principal_id='.$principal.' LIMIT 2 FOR UPDATE','could not lock PAY-API-001 principal for rate limit');
    $rs=pay_api_001_runtime_one('SELECT api_resource_id,read_capable,command_capable FROM '.TB_PREF.'hrm_pay_api_001_resources WHERE api_resource_id='.$resource.' LIMIT 2 FOR UPDATE','could not lock PAY-API-001 resource for rate limit');
    $pol=pay_api_001_runtime_one('SELECT * FROM '.TB_PREF.'hrm_pay_api_001_rate_limit_policies WHERE api_rate_limit_policy_id='.$policy_id.' AND api_resource_id='.$resource.' AND action_code=\'read\' LIMIT 2 FOR UPDATE','could not lock PAY-API-001 rate policy');
    if(!$pr||!$rs||!$pol||(int)$pr['company_id']!==$company||(string)$pr['principal_status']!=='active'||empty($rs['read_capable'])||!empty($rs['command_capable'])||!pay_api_001_rate_limit_policy_valid($pol)){if($manage)cancel_transaction();$error='rate_limit_scope_or_policy_invalid';return false;}
    $scope_sql='SELECT api_principal_scope_id FROM '.TB_PREF.'hrm_pay_api_001_principal_scopes WHERE api_principal_id='.$principal.' AND api_resource_id='.$resource.' AND action_code=\'read\'';
    if($scope_id!==null)$scope_sql.=' AND api_principal_scope_id='.$scope_id;
    $scopeq=db_query($scope_sql.' LIMIT 2 FOR UPDATE');
    $scope_rows=$scopeq?db_num_rows($scopeq):0;
    if(!$scopeq||($scope_id===null?$scope_rows<1:$scope_rows!==1)){if($manage)cancel_transaction();$error='read_scope_required';return false;}
    $replayq=db_query('SELECT * FROM '.TB_PREF.'hrm_pay_api_001_rate_limit_decisions WHERE company_id='.$company.' AND api_principal_id='.$principal.' AND api_resource_id='.$resource.' AND api_rate_limit_policy_id='.$policy_id.' AND correlation_sha256='.db_escape($correlation_sha256).' ORDER BY api_rate_limit_decision_id DESC LIMIT 2 FOR UPDATE');
    if(!$replayq||db_num_rows($replayq)>1){if($manage)cancel_transaction();$error='rate_limit_replay_state_invalid';return false;}
    if(db_num_rows($replayq)===1){
      $old=db_fetch_assoc($replayq);
      if($manage)commit_transaction();
      return array('allowed'=>(string)$old['outcome_code']==='rate_limit_allowed','code'=>$old['outcome_code'],'company_id'=>$company,
        'api_principal_id'=>$principal,'api_resource_id'=>$resource,'api_rate_limit_policy_id'=>$policy_id,'api_rate_limit_decision_id'=>(int)$old['api_rate_limit_decision_id'],
        'decision_sha256'=>$old['decision_sha256'],'replayed'=>true);
    }
    $window=pay_api_001_rate_limit_window_start($t,$pol['window_seconds']);if($window===false){if($manage)cancel_transaction();$error='rate_limit_clock_invalid';return false;}$window_dt=gmdate('Y-m-d H:i:s',$window);
    $q=db_query('SELECT COUNT(*) total,SUM(CASE WHEN outcome_code=\'rate_limit_allowed\' THEN 1 ELSE 0 END) accepted FROM '.TB_PREF.'hrm_pay_api_001_rate_limit_decisions WHERE api_rate_limit_policy_id='.$policy_id.' AND api_principal_id='.$principal.' AND window_start='.db_escape($window_dt));
    $counts=$q?db_fetch_assoc($q):false;if(!$counts){if($manage)cancel_transaction();$error='rate_limit_state_read_failed';return false;}
    $total=(int)$counts['total'];$accepted=(int)$counts['accepted'];if($total>=4294967294){if($manage)cancel_transaction();$error='rate_limit_ordinal_exhausted';return false;}
    $gate=pay_api_001_rate_limit_gate($pol,$accepted,$t);$ordinal=$total+1;$occurred=gmdate('Y-m-d H:i:s',$t);
    $decision_sha=pay_api_001_provisioning_hash(array('company_id'=>$company,'api_principal_id'=>$principal,'api_resource_id'=>$resource,'api_rate_limit_policy_id'=>$policy_id,'window_start'=>$window_dt,'request_ordinal'=>$ordinal,'outcome_code'=>$gate['code'],'correlation_sha256'=>$correlation_sha256,'occurred_at'=>$occurred));
    $sql='INSERT INTO '.TB_PREF.'hrm_pay_api_001_rate_limit_decisions (company_id,api_principal_id,api_resource_id,api_rate_limit_policy_id,window_start,request_ordinal,outcome_code,correlation_sha256,decision_sha256,occurred_at,created_at) VALUES ('
      .$company.','.$principal.','.$resource.','.$policy_id.','.db_escape($window_dt).','.$ordinal.','.db_escape($gate['code'])
      .','.db_escape($correlation_sha256).','.db_escape($decision_sha).','.db_escape($occurred).','.db_escape($occurred).')';
    if(!db_query($sql)){if($manage)cancel_transaction();$error='rate_limit_decision_insert_failed';return false;}
    $id=(int)db_insert_id();if($manage)commit_transaction();
    return array('allowed'=>!empty($gate['allowed']),'code'=>$gate['code'],'company_id'=>$company,'api_principal_id'=>$principal,'api_resource_id'=>$resource,'api_rate_limit_policy_id'=>$policy_id,'api_rate_limit_decision_id'=>$id,'decision_sha256'=>$decision_sha,'remaining'=>isset($gate['remaining'])?$gate['remaining']:0,'replayed'=>false);
}

function pay_api_001_enforce_read_rate_limit($company_id,$api_principal_id,$api_resource_id,$api_rate_limit_policy_id,$correlation_sha256,$unix_time=null,&$error=null)
{
    return pay_api_001_rate_limit_runtime_core($company_id,$api_principal_id,$api_resource_id,$api_rate_limit_policy_id,$correlation_sha256,$unix_time,true,null,$error);
}

function pay_api_001_authn_replay($company,$credential_key,$credential_key_sha256,$presented_secret,$correlation_sha256,$unix_time,&$error)
{
    $error=null;
    $q=db_query('SELECT * FROM '.TB_PREF.'hrm_pay_api_001_authn_decisions WHERE company_id='.(int)$company.' AND correlation_sha256='.db_escape($correlation_sha256).' LIMIT 2 FOR UPDATE');
    if(!$q||db_num_rows($q)!==1){$error='authentication_replay_state_invalid';return false;}
    $r=db_fetch_assoc($q);
    if(!hash_equals((string)$r['credential_key_sha256'],$credential_key_sha256)){$error='correlation_reuse_mismatch';return false;}
    $ok=(string)$r['outcome_code']==='authenticated';
    $out=array('authenticated'=>$ok,'code'=>$ok?'authenticated':'authentication_failed','company_id'=>(int)$company,'api_authn_decision_id'=>(int)$r['api_authn_decision_id'],'decision_sha256'=>$r['decision_sha256'],'replayed'=>true);
    if($ok){
        $principal=pay_api_001_positive_id($r['api_principal_id']);$credential=pay_api_001_positive_id($r['api_credential_id']);
        if($principal===false||$credential===false){$error='authentication_replay_state_invalid';return false;}
        $sql='SELECT c.*,p.company_id,p.principal_status FROM '.TB_PREF.'hrm_pay_api_001_credentials c INNER JOIN '.TB_PREF.'hrm_pay_api_001_machine_principals p ON p.api_principal_id=c.api_principal_id WHERE c.api_credential_id='.$credential.' AND c.api_principal_id='.$principal.' AND c.credential_key='.db_escape($credential_key).' LIMIT 2 FOR UPDATE';
        $cq=db_query($sql,'could not revalidate PAY-API-001 credential replay');
        if(!$cq||db_num_rows($cq)!==1){$error='authentication_replay_state_invalid';return false;}
        $c=db_fetch_assoc($cq);$occurred=gmdate('Y-m-d H:i:s',$unix_time);
        $valid=(int)$c['company_id']===(int)$company&&(string)$c['principal_status']==='active'&&(string)$c['verifier_scheme']==='password_hash'&&strcmp($occurred,(string)$c['valid_from'])>=0&&strcmp($occurred,(string)$c['expires_at'])<0&&pay_api_001_verify_client_secret($presented_secret,(string)$c['secret_verifier_hash']);
        if($valid){
            $rq=db_query('SELECT credential_revocation_id FROM '.TB_PREF.'hrm_pay_api_001_credential_revocations WHERE api_credential_id='.$credential.' LIMIT 2 FOR UPDATE');
            if(!$rq||db_num_rows($rq)>1){$error='authentication_revocation_state_invalid';return false;}
            if(db_num_rows($rq)!==0)$valid=false;
        }
        if(!$valid){$out['authenticated']=false;$out['code']='authentication_failed';$out['replay_revalidated']=false;return $out;}
        $out['api_principal_id']=$principal;$out['api_credential_id']=$credential;$out['replay_revalidated']=true;
    }
    return $out;
}

function pay_api_001_authenticate_machine_credential($company_id,$credential_key,$presented_secret,$correlation_sha256,$unix_time=null,&$error=null)
{
    $error=null;$company=pay_api_001_uint_value($company_id,true,4294967295);
    $key=pay_api_001_provisioning_token($credential_key,128,'/^[A-Za-z0-9][A-Za-z0-9._:-]{0,127}$/D');
    $t=$unix_time===null?time():pay_api_001_uint_value($unix_time,true,PHP_INT_MAX);
    if($company===false||$key===false||!is_string($presented_secret)||$presented_secret===''||strlen($presented_secret)>4096||!is_sha256_hex($correlation_sha256)||$t===false){$error='authentication_input_invalid';return false;}
    $key_sha=hash('sha256',$key);$manage=pay_api_001_runtime_manage_transaction();if($manage)begin_transaction();
    $rq=db_query('SELECT * FROM '.TB_PREF.'hrm_pay_api_001_authn_decisions WHERE company_id='.$company.' AND correlation_sha256='.db_escape($correlation_sha256).' LIMIT 2 FOR UPDATE');
    if(!$rq||db_num_rows($rq)>1){if($manage)cancel_transaction();$error='authentication_replay_state_invalid';return false;}
    if(db_num_rows($rq)===1){
        $out=pay_api_001_authn_replay($company,$key,$key_sha,$presented_secret,$correlation_sha256,$t,$error);
        if($out===false){if($manage)cancel_transaction();return false;}
        if($manage)commit_transaction();return $out;
    }
    $sql='SELECT c.*,p.company_id,p.principal_status FROM '.TB_PREF.'hrm_pay_api_001_credentials c INNER JOIN '.TB_PREF.'hrm_pay_api_001_machine_principals p ON p.api_principal_id=c.api_principal_id WHERE c.credential_key='.db_escape($key).' LIMIT 2 FOR UPDATE';
    $q=db_query($sql,'could not lock PAY-API-001 credential for authentication');
    if(!$q||db_num_rows($q)>1){if($manage)cancel_transaction();$error='authentication_credential_state_invalid';return false;}
    $row=db_num_rows($q)===1?db_fetch_assoc($q):false;
    $principal_id=$row?pay_api_001_positive_id($row['api_principal_id']):false;$credential_id=$row?pay_api_001_positive_id($row['api_credential_id']):false;
    if($row&&($principal_id===false||$credential_id===false)){if($manage)cancel_transaction();$error='authentication_credential_state_invalid';return false;}
    $occurred=gmdate('Y-m-d H:i:s',$t);$authenticated=false;
    if($row&&(int)$row['company_id']===$company&&(string)$row['principal_status']==='active'&&(string)$row['verifier_scheme']==='password_hash'&&strcmp($occurred,(string)$row['valid_from'])>=0&&strcmp($occurred,(string)$row['expires_at'])<0){
        $revocation_q=db_query('SELECT credential_revocation_id FROM '.TB_PREF.'hrm_pay_api_001_credential_revocations WHERE api_credential_id='.$credential_id.' LIMIT 2 FOR UPDATE');
        if(!$revocation_q||db_num_rows($revocation_q)>1){if($manage)cancel_transaction();$error='authentication_revocation_state_invalid';return false;}
        if(db_num_rows($revocation_q)===0&&pay_api_001_verify_client_secret($presented_secret,(string)$row['secret_verifier_hash']))$authenticated=true;
    }
    $outcome=$authenticated?'authenticated':'authentication_failed';
    $decision_sha=pay_api_001_provisioning_hash(array('company_id'=>$company,'api_principal_id'=>$row?$principal_id:null,'api_credential_id'=>$row?$credential_id:null,'credential_key_sha256'=>$key_sha,'outcome_code'=>$outcome,'correlation_sha256'=>$correlation_sha256,'occurred_at'=>$occurred));
    $sql='INSERT INTO '.TB_PREF.'hrm_pay_api_001_authn_decisions (company_id,api_principal_id,api_credential_id,credential_key_sha256,outcome_code,correlation_sha256,decision_sha256,occurred_at,created_at) VALUES ('
      .$company.','.($row?(int)$principal_id:'NULL').','.($row?(int)$credential_id:'NULL').','.db_escape($key_sha).','.db_escape($outcome)
      .','.db_escape($correlation_sha256).','.db_escape($decision_sha).','.db_escape($occurred).','.db_escape($occurred).')';
    if(!db_query($sql)){
        $replay_error=null;$replay=pay_api_001_authn_replay($company,$key,$key_sha,$presented_secret,$correlation_sha256,$t,$replay_error);
        if(is_array($replay)){if($manage)commit_transaction();return $replay;}
        if($manage)cancel_transaction();$error=$replay_error?:'authentication_decision_insert_failed';return false;
    }
    $id=(int)db_insert_id();if($manage)commit_transaction();
    $out=array('authenticated'=>$authenticated,'code'=>$outcome,'company_id'=>$company,'api_authn_decision_id'=>$id,'decision_sha256'=>$decision_sha,'replayed'=>false);
    if($authenticated){$out['api_principal_id']=$principal_id;$out['api_credential_id']=$credential_id;}
    return $out;
}

function pay_api_001_resolve_machine_read_authority($company_id,$api_principal_id,$resource_key,$contract_version,$object_scope_type=null,$object_scope_key=null,&$error=null)
{
    $error=null;$company=pay_api_001_uint_value($company_id,true,4294967295);$principal=pay_api_001_positive_id($api_principal_id);
    $rk=pay_api_001_provisioning_token($resource_key,96,'/^[a-z0-9][a-z0-9._\/-]{0,95}$/D');$cv=pay_api_001_provisioning_token($contract_version,32,'/^[A-Za-z0-9][A-Za-z0-9._-]{0,31}$/D');
    if($company===false||$principal===false||$rk===false||$cv===false||(($object_scope_type===null)!==($object_scope_key===null))){$error='read_authority_input_invalid';return false;}
    $pr=pay_api_001_runtime_one('SELECT api_principal_id,company_id,principal_status FROM '.TB_PREF.'hrm_pay_api_001_machine_principals WHERE api_principal_id='.$principal.' LIMIT 2','could not resolve PAY-API-001 machine principal');
    if(!$pr||(int)$pr['company_id']!==$company||(string)$pr['principal_status']!=='active'){$error='read_authority_denied';return false;}
    $rs=pay_api_001_runtime_one('SELECT * FROM '.TB_PREF.'hrm_pay_api_001_resources WHERE resource_key='.db_escape($rk).' AND contract_version='.db_escape($cv).' LIMIT 2','could not resolve PAY-API-001 resource');
    if(!$rs||empty($rs['company_scope_required'])||empty($rs['read_capable'])||!empty($rs['command_capable'])){$error='read_authority_denied';return false;}
    $resource_id=pay_api_001_positive_id($rs['api_resource_id']);if($resource_id===false){$error='read_authority_denied';return false;}
    if(!empty($rs['object_scope_required'])){
        $ost=pay_api_001_provisioning_token($object_scope_type,32);$osk=pay_api_001_provisioning_token($object_scope_key,128,'/^[A-Za-z0-9][A-Za-z0-9._:\/-]{0,127}$/D');
        if($ost===false||$osk===false){$error='object_scope_required';return false;}
        $scope_sql='SELECT * FROM '.TB_PREF.'hrm_pay_api_001_principal_scopes WHERE api_principal_id='.$principal.' AND api_resource_id='.$resource_id.' AND action_code=\'read\' AND object_scope_type='.db_escape($ost).' AND object_scope_key='.db_escape($osk).' LIMIT 2';
    }else{
        if($object_scope_type!==null||$object_scope_key!==null){$error='object_scope_not_applicable';return false;}
        $scope_sql='SELECT * FROM '.TB_PREF.'hrm_pay_api_001_principal_scopes WHERE api_principal_id='.$principal.' AND api_resource_id='.$resource_id.' AND action_code=\'read\' AND object_scope_type IS NULL AND object_scope_key IS NULL LIMIT 2';
    }
    $scope=pay_api_001_runtime_one($scope_sql,'could not resolve PAY-API-001 read scope');
    if(!$scope){$error='read_scope_required';return false;}
    $policy=pay_api_001_runtime_one('SELECT * FROM '.TB_PREF.'hrm_pay_api_001_rate_limit_policies WHERE api_resource_id='.$resource_id.' AND action_code=\'read\' LIMIT 2','could not resolve PAY-API-001 rate policy');
    if(!$policy||!pay_api_001_rate_limit_policy_valid($policy)){$error='rate_policy_required';return false;}
    return array('company_id'=>$company,'api_principal_id'=>$principal,'resource'=>$rs,'scope'=>$scope,'rate_policy'=>$policy);
}

function pay_api_001_enforce_machine_read_rate_limit($authn,$authority,$correlation_sha256,$unix_time=null,&$error=null)
{
    $error=null;
    if(!is_array($authn)||empty($authn['authenticated'])||!is_array($authority)||!isset($authority['resource'],$authority['scope'],$authority['rate_policy'])){$error='machine_rate_context_invalid';return false;}
    $company=pay_api_001_uint_value($authn['company_id']??null,true,4294967295);$principal=pay_api_001_positive_id($authn['api_principal_id']??null);
    if($company===false||$principal===false||$company!==pay_api_001_uint_value($authority['company_id']??null,true,4294967295)||$principal!==pay_api_001_positive_id($authority['api_principal_id']??null)){$error='machine_rate_context_mismatch';return false;}
    $resource=pay_api_001_positive_id($authority['resource']['api_resource_id']??null);$policy=pay_api_001_positive_id($authority['rate_policy']['api_rate_limit_policy_id']??null);$scope=pay_api_001_positive_id($authority['scope']['api_principal_scope_id']??null);
    if($resource===false||$policy===false||$scope===false){$error='machine_rate_context_invalid';return false;}
    return pay_api_001_rate_limit_runtime_core($company,$principal,$resource,$policy,$correlation_sha256,$unix_time,false,$scope,$error);
}
?>
