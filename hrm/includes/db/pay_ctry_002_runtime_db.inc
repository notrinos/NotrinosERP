<?php
/**
 * PAY-CTRY-002 runtime resolution, lifecycle control, outbox and run guard.
 *
 * This file resolves immutable binding metadata only. It never evaluates a
 * country-pack callback and never grants the pack ERP, accounting, employee,
 * bank, tax, security, administration, or unrestricted SQL authority.
 */
$pay_ctry_002_runtime_root = isset($path_to_root) ? $path_to_root : dirname(dirname(dirname(__DIR__)));
if (!function_exists('pay_ctry_002_validate_runtime_snapshot')) include_once($pay_ctry_002_runtime_root.'/includes/pay_ctry_002_contract.inc');
if (!function_exists('pay_ctry_002_schema_groups')) include_once($pay_ctry_002_runtime_root.'/hrm/includes/db/pay_ctry_002_schema_db.inc');
if (!function_exists('pay_ctry_002_activation_actor_id')) include_once($pay_ctry_002_runtime_root.'/hrm/includes/db/pay_ctry_002_activation_db.inc');
unset($pay_ctry_002_runtime_root);

function pay_ctry_002_runtime_tables_ready()
{
    if (!function_exists('hrm_migration_table_exists')) return false;
    foreach (array('pay_ctry_activation_bindings','pay_ctry_activation_suspensions','pay_ctry_activation_outbox','pay_ctry_activation_delivery_attempts','pay_ctry_activation_run_bindings','pay_ctry_activation_revocations') as $table)
        if (!hrm_migration_table_exists($table)) return false;
    return true;
}

function pay_ctry_002_runtime_nullable_scope_sql($value)
{
    return $value === null || $value === '' || $value === 0 || $value === '0' ? ' IS NULL' : '='.(int)$value;
}
function pay_ctry_002_runtime_nullable_text_sql($value)
{
    return $value === null || $value === '' ? ' IS NULL' : '='.db_escape((string)$value);
}
function pay_ctry_002_runtime_scope_where($company,$scope_type,$legal_entity_id,$payroll_scope_key)
{
    return 'company_id='.(int)$company.' AND scope_type='.db_escape((string)$scope_type)
        .' AND legal_entity_id'.pay_ctry_002_runtime_nullable_scope_sql($legal_entity_id)
        .' AND payroll_scope_key'.pay_ctry_002_runtime_nullable_text_sql($payroll_scope_key);
}

/** Resolve one exact scope leaf, with the row locked when requested. */
function pay_ctry_002_runtime_binding_for_update($company,$legal_entity_id=null,$payroll_scope_key=null,$effective_date=null,$for_update=true)
{
    $date=$effective_date===null?gmdate('Y-m-d'):(string)$effective_date;
    $queries=array();
    if ($legal_entity_id!==null && (int)$legal_entity_id>0)
        $queries[]=pay_ctry_002_runtime_scope_where($company,'legal_entity',$legal_entity_id,null);
    if ($payroll_scope_key!==null && $payroll_scope_key!=='')
        $queries[]=pay_ctry_002_runtime_scope_where($company,'payroll_scope', $legal_entity_id===null?0:$legal_entity_id,$payroll_scope_key);
    $queries[]=pay_ctry_002_runtime_scope_where($company,'company',null,null);
    foreach ($queries as $where) {
        $sql='SELECT b.binding_id,b.activation_id,b.site_installation_id,b.company_id,b.scope_type,b.legal_entity_id,b.payroll_scope_key,b.action_type,b.effective_from,b.effective_to,b.activation_sha256,b.binding_sha256,a.action_type AS activation_action_type,s.pack_version_id FROM '.TB_PREF.'pay_ctry_activation_bindings b INNER JOIN '.TB_PREF.'pay_ctry_activations a ON a.activation_id=b.activation_id INNER JOIN '.TB_PREF.'pay_ctry_site_installations s ON s.site_installation_id=b.site_installation_id WHERE '.$where.' AND b.effective_from<='.db_escape($date).' AND (b.effective_to IS NULL OR b.effective_to>='.db_escape($date).') ORDER BY b.effective_from DESC,b.binding_id DESC LIMIT 1'.($for_update?' FOR UPDATE':'');
        $q=db_query($sql);if(!$q) return false;if(db_num_rows($q)===1)return db_fetch_assoc($q);
    }
    return null;
}

function pay_ctry_002_runtime_latest_event($table,$binding_id,$effective_at,$for_update=true)
{
    $sql='SELECT * FROM '.TB_PREF.$table.' WHERE binding_id='.(int)$binding_id.' AND effective_at<='.db_escape($effective_at).' ORDER BY effective_at DESC,'.(strpos($table,'revocations')!==false?'revocation_id':'suspension_id').' DESC LIMIT 1'.($for_update?' FOR UPDATE':'');
    $q=db_query($sql);if(!$q)return false;return db_num_rows($q)===1?db_fetch_assoc($q):null;
}

/** Return true only when no hard-revoke/suspend event currently denies execution. */
function pay_ctry_002_activation_lifecycle_clear_for_update($binding_id,$now_epoch,&$error=null)
{
    $error=null;if((int)$binding_id<=0){$error='binding_missing';return false;}
    if (!function_exists('hrm_migration_table_exists') || !hrm_migration_table_exists('pay_ctry_activation_suspensions') || !hrm_migration_table_exists('pay_ctry_activation_revocations')) {$error='lifecycle_schema_unavailable';return false;}
    $at=gmdate('Y-m-d H:i:s',(int)$now_epoch);$rev=pay_ctry_002_runtime_latest_event('pay_ctry_activation_revocations',$binding_id,$at,true);
    if($rev===false){$error='revocation_state_unavailable';return false;}if(is_array($rev)){$error='hard_revoked_for_new_execution';return false;}
    $susp=pay_ctry_002_runtime_latest_event('pay_ctry_activation_suspensions',$binding_id,$at,true);
    if($susp===false){$error='suspension_state_unavailable';return false;}if(is_array($susp)&&$susp['action_type']==='suspend'){$error='activation_suspended_for_new_execution';return false;}
    return true;
}

/**
 * Resolve current runtime state. A missing binding is explicit legacy state;
 * a governed binding is fail-closed until a separately governed pack adapter
 * exists, so the legacy engine cannot silently ignore an activation.
 */
function pay_ctry_002_runtime_resolution_for_update($company,$legal_entity_id=null,$payroll_scope_key=null,$effective_date=null,$now_epoch=null,$for_update=true,&$error=null)
{
    $error=null;$company=(int)$company;$now_epoch=$now_epoch===null?time():(int)$now_epoch;$date=$effective_date===null?gmdate('Y-m-d',$now_epoch):(string)$effective_date;
    if($company<0||!pay_ctry_002_iso_date($date)){$error='runtime_scope_invalid';return false;}
    if(!pay_ctry_002_runtime_tables_ready()){$error='runtime_schema_not_ready';return false;}
    $binding=pay_ctry_002_runtime_binding_for_update($company,$legal_entity_id,$payroll_scope_key,$date,$for_update);
    if($binding===false){$error='runtime_binding_lookup_failed';return false;}
    if($binding===null)return array('status'=>'legacy','company_id'=>$company,'scope_type'=>'company','legal_entity_id'=>null,'payroll_scope_key'=>null,'binding_id'=>null,'activation_id'=>null,'adapter_available'=>true);
    if((string)$binding['action_type']==='deactivate')return array('status'=>'legacy','company_id'=>$company,'scope_type'=>(string)$binding['scope_type'],'legal_entity_id'=>$binding['legal_entity_id']===null?null:(int)$binding['legal_entity_id'],'payroll_scope_key'=>$binding['payroll_scope_key']===null?null:(string)$binding['payroll_scope_key'],'binding_id'=>(int)$binding['binding_id'],'activation_id'=>(int)$binding['activation_id'],'adapter_available'=>true,'deactivated_binding_sha256'=>(string)$binding['binding_sha256']);
    $trust_error=null;if(!pay_ctry_002_activation_pack_trust_for_update($binding,$now_epoch,$trust_error)){$error=$trust_error?:'runtime_pack_trust_denied';return false;}
    $rev=pay_ctry_002_runtime_latest_event('pay_ctry_activation_revocations',(int)$binding['binding_id'],gmdate('Y-m-d H:i:s',$now_epoch),$for_update);if($rev===false){$error='runtime_revocation_lookup_failed';return false;}if(is_array($rev)){$error='hard_revoked';return false;}
    $susp=pay_ctry_002_runtime_latest_event('pay_ctry_activation_suspensions',(int)$binding['binding_id'],gmdate('Y-m-d H:i:s',$now_epoch),$for_update);if($susp===false){$error='runtime_suspension_lookup_failed';return false;}
    if(is_array($susp)&&$susp['action_type']==='suspend'){$error='suspended';return false;}
    return array('status'=>'governed','company_id'=>(int)$binding['company_id'],'scope_type'=>(string)$binding['scope_type'],'legal_entity_id'=>$binding['legal_entity_id']===null?null:(int)$binding['legal_entity_id'],'payroll_scope_key'=>$binding['payroll_scope_key']===null?null:(string)$binding['payroll_scope_key'],'binding_id'=>(int)$binding['binding_id'],'activation_id'=>(int)$binding['activation_id'],'binding_sha256'=>(string)$binding['binding_sha256'],'site_installation_id'=>(int)$binding['site_installation_id'],'pack_version_id'=>(int)$binding['pack_version_id'],'adapter_available'=>false);
}

function pay_ctry_002_runtime_action_hash(array $binding,$action,$reason)
{
    return pay_ctry_002_hash(array('action_class'=>PAY_CTRY_002_LIFECYCLE_ACTION_CLASS,'binding_id'=>(int)$binding['binding_id'],'activation_id'=>(int)$binding['activation_id'],'company_id'=>(int)$binding['company_id'],'scope_type'=>(string)$binding['scope_type'],'legal_entity_id'=>empty($binding['legal_entity_id'])?null:(int)$binding['legal_entity_id'],'payroll_scope_key'=>empty($binding['payroll_scope_key'])?null:(string)$binding['payroll_scope_key'],'action_type'=>(string)$action,'reason_code'=>(string)$reason,'binding_sha256'=>(string)$binding['binding_sha256']));
}

function pay_ctry_002_runtime_insert_id(){return function_exists('db_insert_id')?(int)db_insert_id():0;}

/** Append one durable local runtime-delivery intent; duplicate replay is exact. */
function pay_ctry_002_append_outbox_intent(array $event,&$error=null)
{
    $error=null;foreach(array('outbox_key','event_type','source_type','source_id','company_id','scope_type','effective_at','payload_sha256','created_by') as $k)if(!array_key_exists($k,$event)){$error='outbox_missing_'.$k;return false;}
    if(!pay_ctry_002_token($event['outbox_key'],96)||!pay_ctry_002_token($event['event_type'],32)||!pay_ctry_002_token($event['source_type'],32)||pay_ctry_002_uint($event['source_id'],false)===false||pay_ctry_002_sha256($event['payload_sha256'])===false){$error='outbox_descriptor_invalid';return false;}
    $hash=pay_ctry_002_hash($event);$existing=pay_ctry_002_activation_fetch_one('SELECT outbox_id,outbox_sha256 FROM '.TB_PREF.'pay_ctry_activation_outbox WHERE outbox_key='.db_escape($event['outbox_key']).' LIMIT 2 FOR UPDATE');
    if($existing!==false){if(!pay_ctry_002_activation_hash_equals((string)$existing['outbox_sha256'],$hash)){$error='outbox_replay_mismatch';return false;}return (int)$existing['outbox_id'];}
    $sql='INSERT INTO '.TB_PREF.'pay_ctry_activation_outbox (outbox_key,event_type,source_type,source_id,activation_id,suspension_id,company_id,scope_type,legal_entity_id,payroll_scope_key,effective_at,payload_sha256,outbox_sha256,created_by,created_at) VALUES ('.db_escape($event['outbox_key']).','.db_escape($event['event_type']).','.db_escape($event['source_type']).','.(int)$event['source_id'].','.(isset($event['activation_id'])&&$event['activation_id']!==null?(int)$event['activation_id']:'NULL').','.(isset($event['suspension_id'])&&$event['suspension_id']!==null?(int)$event['suspension_id']:'NULL').','.(int)$event['company_id'].','.db_escape($event['scope_type']).','.(isset($event['legal_entity_id'])&&$event['legal_entity_id']!==null?(int)$event['legal_entity_id']:'NULL').','.(isset($event['payroll_scope_key'])&&$event['payroll_scope_key']!==null&&$event['payroll_scope_key']!==''?db_escape($event['payroll_scope_key']):'NULL').','.db_escape($event['effective_at']).','.db_escape($event['payload_sha256']).','.db_escape($hash).','.(int)$event['created_by'].','.db_escape(isset($event['created_at'])?$event['created_at']:gmdate('Y-m-d H:i:s')).')';
    if(!db_query($sql,'could not append PAY-CTRY-002 runtime outbox intent')){$error='outbox_insert_failed';return false;}return pay_ctry_002_runtime_insert_id();
}

/** Append an immutable local delivery attempt; no outbox row is ever updated. */
function pay_ctry_002_deliver_outbox_event($outbox_id,$target_code='runtime_guard',$now_epoch=null,&$error=null)
{
    $error=null;$actor=pay_ctry_002_activation_actor_id();$now_epoch=$now_epoch===null?time():(int)$now_epoch;
    if($actor<=0||(!function_exists('user_check_access')||(!user_check_access(PAY_CTRY_002_RECOVERY_PERMISSION)&&!user_check_access(PAY_CTRY_002_EXECUTOR_PERMISSION)))){$error='outbox_delivery_unauthorized';return false;}
    if(!pay_ctry_002_runtime_tables_ready()){$error='outbox_schema_not_ready';return false;}$out=pay_ctry_002_activation_fetch_one('SELECT * FROM '.TB_PREF.'pay_ctry_activation_outbox WHERE outbox_id='.(int)$outbox_id.' LIMIT 2 FOR UPDATE');if(!is_array($out)){$error='outbox_missing';return false;}
    $q=db_query('SELECT attempt_no FROM '.TB_PREF.'pay_ctry_activation_delivery_attempts WHERE outbox_id='.(int)$outbox_id.' ORDER BY attempt_no DESC LIMIT 1 FOR UPDATE');if(!$q){$error='outbox_attempt_lookup_failed';return false;}$attempt_no=db_num_rows($q)===1?(int)db_fetch_assoc($q)['attempt_no']+1:1;$target=pay_ctry_002_token($target_code,64);if(!$target){$error='outbox_target_invalid';return false;}
    $result=pay_ctry_002_hash(array('outbox_sha256'=>(string)$out['outbox_sha256'],'target_code'=>$target,'attempt_no'=>$attempt_no,'propagation'=>'local_runtime_guard'));$evidence=pay_ctry_002_hash(array('outbox_id'=>(int)$outbox_id,'payload_sha256'=>(string)$out['payload_sha256'],'result_sha256'=>$result,'attempt_no'=>$attempt_no));
    if(!db_query('INSERT INTO '.TB_PREF.'pay_ctry_activation_delivery_attempts (outbox_id,attempt_no,target_code,delivery_status,result_sha256,error_code,evidence_sha256,attempted_by,created_at) VALUES ('.(int)$outbox_id.','.$attempt_no.','.db_escape($target).',\'delivered\','.db_escape($result).',NULL,'.db_escape($evidence).','.$actor.','.db_escape(gmdate('Y-m-d H:i:s',$now_epoch)).')','could not append PAY-CTRY-002 outbox delivery attempt')){$error='outbox_delivery_insert_failed';return false;}
    return array('outbox_id'=>(int)$outbox_id,'attempt_no'=>$attempt_no,'delivery_status'=>'delivered','evidence_sha256'=>$evidence);
}

/** Append suspend/resume/hard-revoke custody plus its outbox intent atomically. */
function pay_ctry_002_append_lifecycle_event($binding_id,$action,$reason,$action_hash,$now_epoch=null,&$error=null)
{
    $error=null;$actor=pay_ctry_002_activation_actor_id();$company=pay_ctry_002_activation_company_id();$now_epoch=$now_epoch===null?time():(int)$now_epoch;$action=(string)$action;$reason=(string)$reason;
    if($actor<=0||$company<0||!function_exists('user_check_access')||!user_check_access(PAY_CTRY_002_RECOVERY_PERMISSION)||!in_array($action,pay_ctry_002_lifecycle_actions(),true)||pay_ctry_002_token($reason,96)===false||pay_ctry_002_sha256($action_hash)===false){$error='lifecycle_unauthorized_or_invalid';return false;}
    if(!pay_ctry_002_runtime_tables_ready()){$error='lifecycle_schema_not_ready';return false;}if(!empty($GLOBALS['transaction_level'])){$error='lifecycle_requires_owned_transaction';return false;}
    begin_transaction();$ok=false;$out=false;
    do{
        $binding=pay_ctry_002_activation_fetch_one('SELECT b.*,a.action_type AS activation_action_type,s.pack_version_id FROM '.TB_PREF.'pay_ctry_activation_bindings b INNER JOIN '.TB_PREF.'pay_ctry_activations a ON a.activation_id=b.activation_id INNER JOIN '.TB_PREF.'pay_ctry_site_installations s ON s.site_installation_id=b.site_installation_id WHERE b.binding_id='.(int)$binding_id.' LIMIT 2 FOR UPDATE');
        if(!is_array($binding)){$error='binding_missing';break;}if((int)$binding['company_id']!==$company){$error='company_scope_mismatch';break;}
        $now=gmdate('Y-m-d H:i:s',$now_epoch);$rev=pay_ctry_002_runtime_latest_event('pay_ctry_activation_revocations',(int)$binding_id,$now,true);if($rev===false){$error='revocation_state_unavailable';break;}if($action!=='hard_revoke'&&is_array($rev)){$error='hard_revoked';break;}
        $susp=pay_ctry_002_runtime_latest_event('pay_ctry_activation_suspensions',(int)$binding_id,$now,true);if($susp===false){$error='suspension_state_unavailable';break;}
        if($action==='suspend'&&is_array($susp)&&$susp['action_type']==='suspend'){$error='already_suspended';break;}
        if($action==='resume'&&(!is_array($susp)||$susp['action_type']!=='suspend')){$error='not_suspended';break;}
        if($action==='hard_revoke'&&is_array($rev)){$error='already_revoked';break;}
        $descriptor=array('lifecycle_key'=>'ctry2.lifecycle.'.substr(pay_ctry_002_hash(array('binding_id'=>(int)$binding_id,'action'=>$action,'reason'=>$reason,'action_hash'=>$action_hash)),0,48),'activation_id'=>(int)$binding['activation_id'],'binding_id'=>(int)$binding['binding_id'],'company_id'=>$company,'scope_type'=>(string)$binding['scope_type'],'legal_entity_id'=>empty($binding['legal_entity_id'])?null:(int)$binding['legal_entity_id'],'payroll_scope_key'=>empty($binding['payroll_scope_key'])?null:(string)$binding['payroll_scope_key'],'action_type'=>$action,'effective_at'=>$now,'reason_code'=>$reason,'reason_sha256'=>hash('sha256',$reason),'assurance_event_id'=>0,'action_sha256'=>$action_hash,'acted_by'=>$actor);
        $assurance=pay_ctry_002_consume_lifecycle_assurance($company,$action_hash,$now_epoch);if($assurance===false){$error='fresh_step_up_required';break;}$descriptor['assurance_event_id']=$assurance;$descriptor['lifecycle_sha256']=pay_ctry_002_hash($descriptor);if(!pay_ctry_002_validate_lifecycle_descriptor($descriptor,$error))break;
        $table=$action==='hard_revoke'?'pay_ctry_activation_revocations':'pay_ctry_activation_suspensions';$key_col=$action==='hard_revoke'?'revocation_key':'suspension_key';$hash_col=$action==='hard_revoke'?'revocation_sha256':'suspension_sha256';$sql='INSERT INTO '.TB_PREF.$table.' ('.$key_col.',activation_id,binding_id,company_id,scope_type,legal_entity_id,payroll_scope_key,action_type,effective_at,reason_code,reason_sha256,assurance_event_id,action_sha256,'.$hash_col.','.($action==='hard_revoke'?'revoked_by':'acted_by').',created_at) VALUES ('.db_escape($descriptor['lifecycle_key']).','.(int)$descriptor['activation_id'].','.(int)$descriptor['binding_id'].','.$company.','.db_escape($descriptor['scope_type']).','.(empty($descriptor['legal_entity_id'])?'NULL':(int)$descriptor['legal_entity_id']).','.(empty($descriptor['payroll_scope_key'])?'NULL':db_escape($descriptor['payroll_scope_key'])).','.db_escape($action).','.db_escape($now).','.db_escape($reason).','.db_escape($descriptor['reason_sha256']).','.(int)$assurance.','.db_escape($action_hash).','.db_escape($descriptor['lifecycle_sha256']).','.$actor.','.db_escape($now).')';
        if(!db_query($sql,'could not append PAY-CTRY-002 lifecycle event')){$error='lifecycle_insert_failed';break;}$source_id=pay_ctry_002_runtime_insert_id();if($source_id<=0){$error='lifecycle_identity_missing';break;}
        $event=array('outbox_key'=>'ctry2.outbox.'.substr($descriptor['lifecycle_key'],16),'event_type'=>$action,'source_type'=>$action==='hard_revoke'?'revocation':'suspension','source_id'=>$source_id,'activation_id'=>(int)$binding['activation_id'],'suspension_id'=>$action==='hard_revoke'?null:$source_id,'company_id'=>$company,'scope_type'=>(string)$binding['scope_type'],'legal_entity_id'=>empty($binding['legal_entity_id'])?null:(int)$binding['legal_entity_id'],'payroll_scope_key'=>empty($binding['payroll_scope_key'])?null:(string)$binding['payroll_scope_key'],'effective_at'=>$now,'payload_sha256'=>$descriptor['lifecycle_sha256'],'created_by'=>$actor,'created_at'=>$now);$outbox=pay_ctry_002_append_outbox_intent($event,$oe);if($outbox===false){$error=$oe;break;}
        if(function_exists('append_domain_audit_event')&&!append_domain_audit_event(array('company_id'=>$company,'legal_entity_id'=>empty($binding['legal_entity_id'])?0:(int)$binding['legal_entity_id'],'entity_type'=>'pay_ctry_activation_lifecycle','entity_key'=>array('binding_id'=>(int)$binding_id,'source_id'=>$source_id),'action'=>'pay.ctry.activation.'.$action,'reason_code'=>$reason,'before'=>array('runtime_state'=>$action==='resume'?'suspended':'governed'),'after'=>array('runtime_state'=>$action==='suspend'?'suspended':($action==='hard_revoke'?'revoked':'governed')),'restricted_diff'=>array('fresh_step_up_consumed'=>true,'outbox_id'=>(int)$outbox,'country_pack_authority_granted'=>false,'historical_payroll_accounting_rows_rewritten'=>0)))){$error='lifecycle_audit_failed';break;}
        $ok=true;$out=array('binding_id'=>(int)$binding_id,'source_id'=>$source_id,'outbox_id'=>(int)$outbox,'action_type'=>$action,'lifecycle_sha256'=>$descriptor['lifecycle_sha256']);
    }while(false);
    if($ok)commit_transaction();else cancel_transaction();return $ok?$out:false;
}
function pay_ctry_002_suspend_activation($binding_id,$reason,$action_hash,$now_epoch=null,&$error=null){return pay_ctry_002_append_lifecycle_event($binding_id,'suspend',$reason,$action_hash,$now_epoch,$error);}
function pay_ctry_002_resume_activation($binding_id,$reason,$action_hash,$now_epoch=null,&$error=null){return pay_ctry_002_append_lifecycle_event($binding_id,'resume',$reason,$action_hash,$now_epoch,$error);}
function pay_ctry_002_hard_revoke_activation($binding_id,$reason,$action_hash,$now_epoch=null,&$error=null){return pay_ctry_002_append_lifecycle_event($binding_id,'hard_revoke',$reason,$action_hash,$now_epoch,$error);}

/** Capture a run's exact runtime decision before any run item is created. */
function pay_ctry_002_capture_run_binding($run_id,$company,$legal_entity_id,$from_date,$to_date,$actor,&$error=null)
{
    $error=null;if(!pay_ctry_002_runtime_tables_ready()){$error='run_guard_schema_not_ready';return false;}$resolved=pay_ctry_002_runtime_resolution_for_update($company,$legal_entity_id,null,$from_date,time(),true,$error);if($resolved===false)return false;
    $state=(string)$resolved['status'];if($state==='governed'&&!isset($resolved['adapter_available'])){$error='country_pack_runtime_adapter_unavailable';return false;}if($state==='governed'&&empty($resolved['adapter_available'])){$error='country_pack_runtime_adapter_unavailable';return false;}$binding_id=isset($resolved['binding_id'])?$resolved['binding_id']:null;$activation_id=isset($resolved['activation_id'])?$resolved['activation_id']:null;$binding_sha=isset($resolved['binding_sha256'])?$resolved['binding_sha256']:null;$scope_type=isset($resolved['scope_type'])?$resolved['scope_type']:'company';$snapshot=pay_ctry_002_hash(array('run_id'=>(int)$run_id,'company_id'=>(int)$company,'scope_type'=>$scope_type,'legal_entity_id'=>$legal_entity_id===null?null:(int)$legal_entity_id,'from_date'=>$from_date,'to_date'=>$to_date,'resolution_state'=>$state,'binding_id'=>$binding_id,'activation_id'=>$activation_id,'binding_sha256'=>$binding_sha));$descriptor=array('run_id'=>(int)$run_id,'company_id'=>(int)$company,'scope_type'=>$scope_type,'legal_entity_id'=>$legal_entity_id,'from_date'=>$from_date,'to_date'=>$to_date,'resolution_state'=>$state,'binding_id'=>$binding_id,'binding_sha256'=>$binding_sha,'snapshot_sha256'=>$snapshot,'captured_by'=>(int)$actor);if(!pay_ctry_002_validate_runtime_snapshot($descriptor,$error))return false;
    $existing=pay_ctry_002_activation_fetch_one('SELECT * FROM '.TB_PREF.'pay_ctry_activation_run_bindings WHERE run_id='.(int)$run_id.' LIMIT 2 FOR UPDATE');if($existing!==false){return pay_ctry_002_activation_hash_equals((string)$existing['snapshot_sha256'],$snapshot)?$existing:false;}
    $sql='INSERT INTO '.TB_PREF.'pay_ctry_activation_run_bindings (run_id,binding_id,activation_id,company_id,scope_type,legal_entity_id,payroll_scope_key,from_date,to_date,resolution_state,binding_sha256,snapshot_sha256,captured_by,created_at) VALUES ('.(int)$run_id.','.($binding_id===null?'NULL':(int)$binding_id).','.($activation_id===null?'NULL':(int)$activation_id).','.(int)$company.','.db_escape($scope_type).','.(empty($legal_entity_id)?'NULL':(int)$legal_entity_id).',NULL,'.db_escape($from_date).','.db_escape($to_date).','.db_escape($state).','.($binding_sha===null?'NULL':db_escape($binding_sha)).','.db_escape($snapshot).','.(int)$actor.','.db_escape(gmdate('Y-m-d H:i:s')).')';if(!db_query($sql,'could not capture PAY-CTRY-002 run binding snapshot')){$error='run_binding_insert_failed';return false;}return array('run_binding_id'=>pay_ctry_002_runtime_insert_id(),'resolution_state'=>$state,'snapshot_sha256'=>$snapshot,'binding_id'=>$binding_id);
}

/** Re-check a run snapshot at claim/checkpoint/finalize boundaries. */
function pay_ctry_002_assert_run_binding_current($run_id,$for_update=true,&$error=null)
{
    $error=null;if(!pay_ctry_002_runtime_tables_ready()){$error='run_guard_schema_not_ready';return false;}$snap=pay_ctry_002_activation_fetch_one('SELECT * FROM '.TB_PREF.'pay_ctry_activation_run_bindings WHERE run_id='.(int)$run_id.' LIMIT 2'.($for_update?' FOR UPDATE':''));if(!is_array($snap)){$error='run_binding_snapshot_missing';return false;}$current=pay_ctry_002_runtime_resolution_for_update((int)$snap['company_id'],$snap['legal_entity_id']===null?null:(int)$snap['legal_entity_id'],null,(string)$snap['from_date'],time(),$for_update,$error);if($current===false)return false;if((string)$snap['resolution_state']!==(string)$current['status']){$error='open_run_binding_changed';return false;}if((string)$snap['resolution_state']==='governed'&&((int)$snap['binding_id']!==(int)$current['binding_id']||!pay_ctry_002_activation_hash_equals((string)$snap['binding_sha256'],(string)$current['binding_sha256']))){$error='open_run_binding_changed';return false;}return $snap;
}
?>
