<?php
/** Governed PAY-CORE-008 append-only writers. No calculator/GL/payment authority. */
include_once(dirname(__FILE__).'/pay_core_008_schema_db.inc');
include_once(dirname(__FILE__).'/../payroll/pay_core_008_contract.inc');
include_once(dirname(__FILE__).'/../payroll/pay_core_008_adapters.inc');

function hrm_pay_core_008_actor(){return isset($_SESSION['wa_current_user']->user)?(int)$_SESSION['wa_current_user']->user:0;}
function hrm_pay_core_008_access($p){return function_exists('user_check_access')&&user_check_access($p);}
function hrm_pay_core_008_manage_transaction(){return empty($GLOBALS['transaction_level']);}
function hrm_pay_core_008_one($sql){$r=db_query($sql);if(!$r)return false;$x=db_fetch_assoc($r);if(!$x||db_fetch_assoc($r))return false;return $x;}
function hrm_pay_core_008_sql_id($v){return $v===null?'NULL':(string)(int)$v;}
function hrm_pay_core_008_sql_token($v){return $v===null?'NULL':db_escape($v);}

/** Object-level scope/BOLA: relationship -> legal entity/worker/person/assignment must agree. */
function hrm_pay_core_008_assert_object_scope(array $scope,&$error=null)
{
    $error=null;$s=hrm_pay_core_008_scope($scope,$error);if($s===false)return false;
    $rel=hrm_pay_core_008_one('SELECT worker_id,legal_entity_id FROM '.TB_PREF.'hrm_payroll_relationships WHERE payroll_relationship_id='.(int)$s['payroll_relationship_id'].' LIMIT 2');
    if(!$rel){$error='payroll_relationship_missing';return false;}
    if((int)$rel['legal_entity_id']!==$s['legal_entity_id']){$error='legal_entity_scope_mismatch';return false;}
    $worker=(int)$rel['worker_id'];
    if($s['worker_id']!==null&&$worker!==$s['worker_id']){$error='worker_scope_mismatch';return false;}
    $w=hrm_pay_core_008_one('SELECT person_id FROM '.TB_PREF.'hrm_workers WHERE worker_id='.$worker.' LIMIT 2');
    if(!$w){$error='worker_scope_missing';return false;}
    $person=(int)$w['person_id'];
    if($s['person_id']!==null&&$person!==$s['person_id']){$error='person_scope_mismatch';return false;}
    if($s['assignment_id']!==null){
        $m=hrm_pay_core_008_one('SELECT relationship_member_id FROM '.TB_PREF.'hrm_payroll_relationship_members WHERE payroll_relationship_id='.$s['payroll_relationship_id'].' AND assignment_id='.$s['assignment_id'].' LIMIT 2');
        if(!$m){$error='assignment_scope_mismatch';return false;}
    }
    $s['worker_id']=$worker;$s['person_id']=$person;
    return $s;
}

function hrm_pay_core_008_bind_result_scope(array $scope,array $result,&$error=null)
{
    $error=null;
    if(!isset($result['scope'])||!is_array($result['scope'])){$error='result_scope_missing';return false;}
    $r=$result['scope'];
    if((int)$r['legal_entity_id']!==(int)$scope['legal_entity_id']||(int)$r['payroll_relationship_id']!==(int)$scope['payroll_relationship_id']){$error='result_object_scope_mismatch';return false;}
    if($scope['assignment_id']!==null&&$r['assignment_id']!==$scope['assignment_id']){$error='result_assignment_scope_mismatch';return false;}
    if($scope['calendar_period_id']!==null&&$r['calendar_period_id']!==$scope['calendar_period_id']){$error='result_calendar_scope_mismatch';return false;}
    $scope['assignment_id']=$r['assignment_id'];$scope['calendar_period_id']=$r['calendar_period_id'];
    return hrm_pay_core_008_assert_object_scope($scope,$error);
}

function hrm_pay_core_008_scope_where(array $scope,$alias='r')
{
    $p=$alias===''?'':$alias.'.';
    $w=array($p.'legal_entity_id='.(int)$scope['legal_entity_id'],$p.'payroll_relationship_id='.(int)$scope['payroll_relationship_id']);
    foreach(array('worker_id','person_id','assignment_id','calendar_period_id') as $k)if($scope[$k]!==null)$w[]=$p.$k.'='.(int)$scope[$k];
    return implode(' AND ',$w);
}

function hrm_pay_core_008_threshold_policy_from_row(array $r,&$error=null)
{
    $p=hrm_pay_core_008_threshold_policy(array('policy_key'=>$r['policy_key'],'version_no'=>$r['version_no'],'measure'=>$r['measure'],'unit_code'=>$r['unit_code'],'currency_code'=>$r['currency_code'],'mode'=>$r['mode'],'tolerance'=>$r['tolerance'],'effective_from'=>$r['effective_from'],'effective_to'=>$r['effective_to']),$error);
    if($p===false||!hash_equals((string)$r['policy_sha256'],$p['policy_sha256'])){$error=$p===false?$error:'threshold_policy_hash_drift';return false;}
    return $p;
}

function hrm_pay_core_008_require_threshold_policy(array $input,&$error=null)
{
    $p=hrm_pay_core_008_threshold_policy($input,$error);if($p===false)return false;
    $r=hrm_pay_core_008_one('SELECT * FROM '.TB_PREF.'hrm_pay_validation_threshold_policies WHERE policy_sha256='.db_escape($p['policy_sha256']).' LIMIT 2');
    if(!$r){$error='threshold_policy_not_governed';return false;}
    $stored=hrm_pay_core_008_threshold_policy_from_row($r,$error);if($stored===false)return false;
    return $stored;
}

function hrm_pay_core_008_append_validator_definition(array $input,&$error=null)
{
    $error=null;$actor=hrm_pay_core_008_actor();if($actor<=0||!hrm_pay_core_008_access('SA_HRM_MANAGE_PAY_CORE_VALIDATION')){$error='access_denied';return false;}
    $v=hrm_pay_core_008_validator_definition($input,$error);if($v===false)return false;$app=hrm_pay_core_004_canonical_json($v['applicability']);
    $sql='INSERT INTO '.TB_PREF.'hrm_pay_validation_validator_defs (validator_key,validator_version,domain,severity,origin,capability,purpose,input_contract_sha256,output_contract_sha256,threshold_policy_sha256,applicability_json,definition_sha256,created_by) VALUES ('.db_escape($v['validator_key']).','.db_escape($v['validator_version']).','.db_escape($v['domain']).','.db_escape($v['severity']).','.db_escape($v['origin']).','.db_escape($v['capability']).','.db_escape($v['purpose']).','.db_escape($v['input_contract_sha256']).','.db_escape($v['output_contract_sha256']).','.hrm_pay_core_008_sql_token($v['threshold_policy_sha256']).','.db_escape($app).','.db_escape($v['definition_sha256']).','.$actor.')';
    if(!db_query($sql)){$same=hrm_pay_core_008_one('SELECT validator_definition_id,definition_sha256 FROM '.TB_PREF.'hrm_pay_validation_validator_defs WHERE validator_key='.db_escape($v['validator_key']).' AND validator_version='.db_escape($v['validator_version']).' LIMIT 2');if($same&&hash_equals($same['definition_sha256'],$v['definition_sha256']))return (int)$same['validator_definition_id'];$error='validator_definition_conflict';return false;}
    return (int)db_insert_id();
}

function hrm_pay_core_008_append_threshold_policy(array $input,&$error=null)
{
    $error=null;$actor=hrm_pay_core_008_actor();if($actor<=0||!hrm_pay_core_008_access('SA_HRM_MANAGE_PAY_CORE_VALIDATION')){$error='access_denied';return false;}
    $p=hrm_pay_core_008_threshold_policy($input,$error);if($p===false)return false;
    $sql='INSERT INTO '.TB_PREF.'hrm_pay_validation_threshold_policies (policy_key,version_no,measure,unit_code,currency_code,mode,tolerance,effective_from,effective_to,policy_sha256,created_by) VALUES ('.db_escape($p['policy_key']).','.$p['version_no'].','.db_escape($p['measure']).','.db_escape($p['unit_code']).','.hrm_pay_core_008_sql_token($p['currency_code']).','.db_escape($p['mode']).','.db_escape($p['tolerance']).','.db_escape($p['effective_from']).','.hrm_pay_core_008_sql_token($p['effective_to']).','.db_escape($p['policy_sha256']).','.$actor.')';
    if(!db_query($sql)){$same=hrm_pay_core_008_one('SELECT threshold_policy_id,policy_sha256 FROM '.TB_PREF.'hrm_pay_validation_threshold_policies WHERE policy_key='.db_escape($p['policy_key']).' AND version_no='.$p['version_no'].' LIMIT 2');if($same&&hash_equals($same['policy_sha256'],$p['policy_sha256']))return (int)$same['threshold_policy_id'];$error='threshold_policy_conflict';return false;}
    return (int)db_insert_id();
}

function hrm_pay_core_008_append_pack_binding($validator_definition_id,array $input,&$error=null)
{
    $error=null;$actor=hrm_pay_core_008_actor();if($actor<=0||!hrm_pay_core_008_access('SA_HRM_MANAGE_PAY_CORE_VALIDATION')){$error='access_denied';return false;}
    $r=hrm_pay_core_008_one('SELECT * FROM '.TB_PREF.'hrm_pay_validation_validator_defs WHERE validator_definition_id='.(int)$validator_definition_id.' LIMIT 2');if(!$r){$error='validator_definition_missing';return false;}
    $app=json_decode($r['applicability_json'],true);$v=hrm_pay_core_008_validator_definition(array('validator_key'=>$r['validator_key'],'validator_version'=>$r['validator_version'],'domain'=>$r['domain'],'severity'=>$r['severity'],'origin'=>$r['origin'],'capability'=>$r['capability'],'purpose'=>$r['purpose'],'input_contract_sha256'=>$r['input_contract_sha256'],'output_contract_sha256'=>$r['output_contract_sha256'],'threshold_policy_sha256'=>$r['threshold_policy_sha256'],'applicability'=>$app),$ve);
    if($v===false||!hash_equals($r['definition_sha256'],$v['definition_sha256'])){$error='validator_definition_hash_drift';return false;}
    $b=hrm_pay_core_008_pack_binding($input,$v,$error);if($b===false)return false;
    $sql='INSERT INTO '.TB_PREF.'hrm_pay_validation_pack_bindings (pack_key,pack_version,validator_definition_id,capability,artifact_sha256,binding_sha256,created_by) VALUES ('.db_escape($b['pack_key']).','.db_escape($b['pack_version']).','.(int)$validator_definition_id.','.db_escape($b['capability']).','.db_escape($b['artifact_sha256']).','.db_escape($b['binding_sha256']).','.$actor.')';
    if(!db_query($sql)){$same=hrm_pay_core_008_one('SELECT pack_binding_id,binding_sha256 FROM '.TB_PREF.'hrm_pay_validation_pack_bindings WHERE pack_key='.db_escape($b['pack_key']).' AND pack_version='.db_escape($b['pack_version']).' AND validator_definition_id='.(int)$validator_definition_id.' LIMIT 2');if($same&&hash_equals($same['binding_sha256'],$b['binding_sha256']))return (int)$same['pack_binding_id'];$error='pack_binding_conflict';return false;}
    return (int)db_insert_id();
}

function hrm_pay_core_008_execute_validator($validator_definition_id,array $scope,$result_id,$source_sha256,array $evidence_hashes,array $findings,$idempotency_key,&$error=null)
{
    $error=null;$actor=hrm_pay_core_008_actor();if($actor<=0||!hrm_pay_core_008_access('SA_HRM_EXECUTE_PAY_CORE_VALIDATION')){$error='access_denied';return false;}
    $s=hrm_pay_core_008_assert_object_scope($scope,$error);if($s===false)return false;
    $vr=hrm_pay_core_008_one('SELECT * FROM '.TB_PREF.'hrm_pay_validation_validator_defs WHERE validator_definition_id='.(int)$validator_definition_id.' LIMIT 2');if(!$vr){$error='validator_definition_missing';return false;}
    $app=json_decode($vr['applicability_json'],true);$v=hrm_pay_core_008_validator_definition(array('validator_key'=>$vr['validator_key'],'validator_version'=>$vr['validator_version'],'domain'=>$vr['domain'],'severity'=>$vr['severity'],'origin'=>$vr['origin'],'capability'=>$vr['capability'],'purpose'=>$vr['purpose'],'input_contract_sha256'=>$vr['input_contract_sha256'],'output_contract_sha256'=>$vr['output_contract_sha256'],'threshold_policy_sha256'=>$vr['threshold_policy_sha256'],'applicability'=>$app),$ve);
    if($v===false||!hash_equals($vr['definition_sha256'],$v['definition_sha256'])){$error='validator_definition_hash_drift';return false;}
    if($v['origin']==='pack'){
        $pb=hrm_pay_core_008_one('SELECT * FROM '.TB_PREF.'hrm_pay_validation_pack_bindings WHERE validator_definition_id='.(int)$validator_definition_id.' LIMIT 2');
        if(!$pb){$error='pack_binding_required';return false;}
        $bound=hrm_pay_core_008_pack_binding(array('pack_key'=>$pb['pack_key'],'pack_version'=>$pb['pack_version'],'capability'=>$pb['capability'],'artifact_sha256'=>$pb['artifact_sha256']),$v,$pe);
        if($bound===false||!hash_equals((string)$pb['binding_sha256'],$bound['binding_sha256'])){$error='pack_binding_hash_drift';return false;}
        $evidence_hashes[]=$bound['binding_sha256'];
    }
    $rr=hrm_pay_core_008_load_result($result_id,$error);if($rr===false)return false;
    $s=hrm_pay_core_008_bind_result_scope($s,$rr,$error);if($s===false)return false;
    $e=hrm_pay_core_008_execute($v,$s,$source_sha256,$rr['result_sha256'],$evidence_hashes,$findings,$idempotency_key,$actor,$error);if($e===false)return false;
    $manage=hrm_pay_core_008_manage_transaction();if($manage)begin_transaction();
    $same=hrm_pay_core_008_one('SELECT validation_run_id,execution_sha256 FROM '.TB_PREF.'hrm_pay_validation_runs WHERE validator_definition_id='.(int)$validator_definition_id.' AND payroll_relationship_id='.$s['payroll_relationship_id'].' AND idempotency_key='.db_escape($idempotency_key).' LIMIT 2 FOR UPDATE');
    if($same){if(hash_equals((string)$same['execution_sha256'],$e['execution_sha256'])){if($manage)commit_transaction();return (int)$same['validation_run_id'];}if($manage)cancel_transaction();$error='validation_command_conflict';return false;}
    $sql='INSERT INTO '.TB_PREF.'hrm_pay_validation_runs (validator_definition_id,legal_entity_id,payroll_relationship_id,worker_id,person_id,assignment_id,calendar_period_id,result_id,result_sha256,source_sha256,evidence_set_sha256,idempotency_key,outcome,finding_set_sha256,execution_sha256,executed_by) VALUES ('.(int)$validator_definition_id.','.$s['legal_entity_id'].','.$s['payroll_relationship_id'].','.hrm_pay_core_008_sql_id($s['worker_id']).','.hrm_pay_core_008_sql_id($s['person_id']).','.hrm_pay_core_008_sql_id($s['assignment_id']).','.hrm_pay_core_008_sql_id($s['calendar_period_id']).','.(int)$result_id.','.db_escape($e['result_sha256']).','.db_escape($e['source_sha256']).','.db_escape($e['evidence_set_sha256']).','.db_escape($e['idempotency_key']).','.db_escape($e['outcome']).','.db_escape($e['finding_set_sha256']).','.db_escape($e['execution_sha256']).','.$actor.')';
    if(!db_query($sql)){if($manage)cancel_transaction();$error='validation_insert_failed';return false;}
    $run=(int)db_insert_id();
    foreach($e['findings'] as $f)if(!db_query('INSERT INTO '.TB_PREF.'hrm_pay_validation_findings (validation_run_id,finding_key,code,severity,evidence_sha256,finding_sha256) VALUES ('.$run.','.db_escape($f['finding_key']).','.db_escape($f['code']).','.db_escape($f['severity']).','.db_escape($f['evidence_sha256']).','.db_escape($f['finding_sha256']).')')){if($manage)cancel_transaction();$error='finding_insert_failed';return false;}
    if($manage)commit_transaction();return $run;
}

function hrm_pay_core_008_record_resolution($finding_id,$reason,$support_sha,$state,$command,&$error=null)
{
    $error=null;$actor=hrm_pay_core_008_actor();if($actor<=0||!hrm_pay_core_008_access('SA_HRM_RESOLVE_PAY_CORE_VALIDATION')){$error='access_denied';return false;}
    $manage=hrm_pay_core_008_manage_transaction();if($manage)begin_transaction();
    $f=hrm_pay_core_008_one('SELECT f.*,r.executed_by,r.legal_entity_id,r.payroll_relationship_id,r.worker_id,r.person_id,r.assignment_id,r.calendar_period_id FROM '.TB_PREF.'hrm_pay_validation_findings f INNER JOIN '.TB_PREF.'hrm_pay_validation_runs r ON r.validation_run_id=f.validation_run_id WHERE f.finding_id='.(int)$finding_id.' LIMIT 2 FOR UPDATE');
    if(!$f){if($manage)cancel_transaction();$error='finding_missing';return false;}
    $s=hrm_pay_core_008_assert_object_scope(array('legal_entity_id'=>$f['legal_entity_id'],'payroll_relationship_id'=>$f['payroll_relationship_id'],'worker_id'=>$f['worker_id'],'person_id'=>$f['person_id'],'assignment_id'=>$f['assignment_id'],'calendar_period_id'=>$f['calendar_period_id']),$error);if($s===false){if($manage)cancel_transaction();return false;}
    if($actor===(int)$f['executed_by']){if($manage)cancel_transaction();$error='resolver_executor_separation_required';return false;}
    $x=hrm_pay_core_008_resolution($f,$reason,$support_sha,$state,$command,$actor,$error);if($x===false){if($manage)cancel_transaction();return false;}
    $same=hrm_pay_core_008_one('SELECT resolution_id,resolution_sha256 FROM '.TB_PREF.'hrm_pay_validation_resolutions WHERE finding_id='.(int)$finding_id.' AND command_key='.db_escape($command).' LIMIT 2 FOR UPDATE');
    if($same){if(hash_equals((string)$same['resolution_sha256'],$x['resolution_sha256'])){if($manage)commit_transaction();return (int)$same['resolution_id'];}if($manage)cancel_transaction();$error='resolution_command_conflict';return false;}
    $sql='INSERT INTO '.TB_PREF.'hrm_pay_validation_resolutions (finding_id,reason_code,support_sha256,resulting_state,command_key,resolution_sha256,resolver_id) VALUES ('.(int)$finding_id.','.db_escape($x['reason_code']).','.db_escape($x['support_sha256']).','.db_escape($x['resulting_state']).','.db_escape($x['command_key']).','.db_escape($x['resolution_sha256']).','.$actor.')';
    if(!db_query($sql)){if($manage)cancel_transaction();$error='resolution_insert_failed';return false;}
    $id=(int)db_insert_id();if($manage)commit_transaction();return $id;
}

/**
 * Signoff is derived from the current immutable validation/resolution/result rows.
 * Caller hashes are assertions, not authority: a stale or incomplete set fails closed.
 */
function hrm_pay_core_008_record_signoff(array $scope,$result_set_sha,$validation_set_sha,$resolution_set_sha,$state,$command,&$error=null)
{
    $error=null;$actor=hrm_pay_core_008_actor();if($actor<=0||!hrm_pay_core_008_access('SA_HRM_SIGNOFF_PAY_CORE_VALIDATION')){$error='access_denied';return false;}
    $s=hrm_pay_core_008_assert_object_scope($scope,$error);if($s===false)return false;
    if($s['calendar_period_id']===null){$error='signoff_calendar_scope_required';return false;}
    $where=hrm_pay_core_008_scope_where($s,'r');$manage=hrm_pay_core_008_manage_transaction();if($manage)begin_transaction();
    $q=db_query('SELECT r.validation_run_id,r.result_id,r.result_sha256,r.execution_sha256,r.executed_by FROM '.TB_PREF.'hrm_pay_validation_runs r WHERE '.$where.' ORDER BY r.validation_run_id FOR UPDATE');
    if(!$q){if($manage)cancel_transaction();$error='signoff_validation_read_failed';return false;}
    $results=array();$validations=array();$executors=array();$run_count=0;
    while($row=db_fetch_assoc($q)){
        $run_count++;$rr=hrm_pay_core_008_load_result((int)$row['result_id'],$re);
        if($rr===false||!hash_equals((string)$row['result_sha256'],(string)$rr['result_sha256'])){if($manage)cancel_transaction();$error='signoff_result_stale';return false;}
        if(hrm_pay_core_008_bind_result_scope($s,$rr,$re)===false){if($manage)cancel_transaction();$error='signoff_result_scope_mismatch';return false;}
        $results[$rr['result_sha256']]=true;$validations[]=$row['execution_sha256'];$executors[]=(int)$row['executed_by'];
    }
    if($run_count===0){if($manage)cancel_transaction();$error='signoff_validation_required';return false;}
    $unresolved=hrm_pay_core_008_one('SELECT f.finding_id FROM '.TB_PREF.'hrm_pay_validation_findings f INNER JOIN '.TB_PREF.'hrm_pay_validation_runs r ON r.validation_run_id=f.validation_run_id WHERE '.$where." AND f.severity='blocking' AND NOT EXISTS (SELECT 1 FROM ".TB_PREF."hrm_pay_validation_resolutions x WHERE x.finding_id=f.finding_id AND x.resulting_state='resolved') LIMIT 1");
    if($unresolved){if($manage)cancel_transaction();$error='unresolved_blocking_finding';return false;}
    $q=db_query('SELECT x.resolution_sha256,x.resolver_id FROM '.TB_PREF.'hrm_pay_validation_resolutions x INNER JOIN '.TB_PREF.'hrm_pay_validation_findings f ON f.finding_id=x.finding_id INNER JOIN '.TB_PREF.'hrm_pay_validation_runs r ON r.validation_run_id=f.validation_run_id WHERE '.$where.' ORDER BY x.resolution_id FOR UPDATE');
    if(!$q){if($manage)cancel_transaction();$error='signoff_resolution_read_failed';return false;}
    $resolutions=array();$resolvers=array();while($row=db_fetch_assoc($q)){$resolutions[]=$row['resolution_sha256'];$resolvers[]=(int)$row['resolver_id'];}
    $derived_result=hrm_pay_core_004_hash_set(array_keys($results));$derived_validation=hrm_pay_core_004_hash_set($validations);$derived_resolution=hrm_pay_core_004_hash_set($resolutions);
    if(!hrm_pay_core_008_is_sha256($result_set_sha)||!hrm_pay_core_008_is_sha256($validation_set_sha)||!hrm_pay_core_008_is_sha256($resolution_set_sha)||!hash_equals($derived_result,$result_set_sha)||!hash_equals($derived_validation,$validation_set_sha)||!hash_equals($derived_resolution,$resolution_set_sha)){if($manage)cancel_transaction();$error='signoff_evidence_set_stale';return false;}
    $x=hrm_pay_core_008_signoff($s,$derived_result,$derived_validation,$derived_resolution,$state,$command,$actor,$executors,$resolvers,$error);if($x===false){if($manage)cancel_transaction();return false;}
    $same=hrm_pay_core_008_one('SELECT signoff_id,signoff_sha256 FROM '.TB_PREF.'hrm_pay_validation_signoffs WHERE payroll_relationship_id='.$s['payroll_relationship_id'].' AND command_key='.db_escape($command).' LIMIT 2 FOR UPDATE');
    if($same){if(hash_equals((string)$same['signoff_sha256'],$x['signoff_sha256'])){if($manage)commit_transaction();return (int)$same['signoff_id'];}if($manage)cancel_transaction();$error='signoff_command_conflict';return false;}
    $sql='INSERT INTO '.TB_PREF.'hrm_pay_validation_signoffs (legal_entity_id,payroll_relationship_id,calendar_period_id,result_set_sha256,validation_set_sha256,resolution_set_sha256,signoff_state,command_key,signoff_sha256,signer_id) VALUES ('.$s['legal_entity_id'].','.$s['payroll_relationship_id'].','.$s['calendar_period_id'].','.db_escape($derived_result).','.db_escape($derived_validation).','.db_escape($derived_resolution).','.db_escape($state).','.db_escape($command).','.db_escape($x['signoff_sha256']).','.$actor.')';
    if(!db_query($sql)){if($manage)cancel_transaction();$error='signoff_insert_failed';return false;}
    $id=(int)db_insert_id();if($manage)commit_transaction();return $id;
}

/** Persist variance only from verified immutable PAY-CORE-004 results and independently recomputed line totals. */
function hrm_pay_core_008_record_variance($kind,array $scope,$current_result_id,$comparison_result_id,array $measures,$threshold_policy_sha=null,&$error=null)
{
    $error=null;$actor=hrm_pay_core_008_actor();if($actor<=0||!hrm_pay_core_008_access('SA_HRM_EXECUTE_PAY_CORE_VALIDATION')){$error='access_denied';return false;}
    $s=hrm_pay_core_008_assert_object_scope($scope,$error);if($s===false)return false;
    foreach($measures as $measure)if(!in_array($measure,array('gross','net'),true)){$error='variance_measure_requires_independent_evidence';return false;}
    $cr=hrm_pay_core_008_load_result($current_result_id,$error);if($cr===false)return false;$s=hrm_pay_core_008_bind_result_scope($s,$cr,$error);if($s===false)return false;
    $pr=hrm_pay_core_008_load_result($comparison_result_id,$error);if($pr===false)return false;
    if((int)$pr['scope']['legal_entity_id']!==$s['legal_entity_id']||(int)$pr['scope']['payroll_relationship_id']!==$s['payroll_relationship_id']){$error='variance_comparison_scope_mismatch';return false;}
    if($kind==='parallel_run'&&(int)$pr['scope']['calendar_period_id']!==(int)$cr['scope']['calendar_period_id']){$error='parallel_run_period_mismatch';return false;}
    if((int)$current_result_id===(int)$comparison_result_id){$error='variance_distinct_results_required';return false;}
    $ct=hrm_pay_core_008_result_totals($cr['descriptor'],$cr['lines'],$ce);$pt=hrm_pay_core_008_result_totals($pr['descriptor'],$pr['lines'],$pe);
    if($ct===false||$pt===false||!$ct['matches_result']||!$pt['matches_result']){$error='variance_result_reconciliation_failed';return false;}
    $current=array('result_id'=>(int)$current_result_id,'result_sha256'=>$cr['result_sha256'],'currency'=>$ct['currency'],'gross'=>$ct['gross'],'net'=>$ct['net']);
    $comparison=array('result_id'=>(int)$comparison_result_id,'result_sha256'=>$pr['result_sha256'],'currency'=>$pt['currency'],'gross'=>$pt['gross'],'net'=>$pt['net']);
    $v=hrm_pay_core_008_variance($kind,$current,$comparison,$measures,null,$error);if($v===false)return false;
    if($threshold_policy_sha!==null){
        if(!hrm_pay_core_008_is_sha256($threshold_policy_sha)){$error='threshold_policy_hash_invalid';return false;}
        $tr=hrm_pay_core_008_one('SELECT * FROM '.TB_PREF.'hrm_pay_validation_threshold_policies WHERE policy_sha256='.db_escape($threshold_policy_sha).' LIMIT 2');if(!$tr){$error='threshold_policy_not_governed';return false;}
        if(hrm_pay_core_008_threshold_policy_from_row($tr,$error)===false)return false;
    }
    $sha=hrm_pay_core_008_hash(array($v,$threshold_policy_sha));$manage=hrm_pay_core_008_manage_transaction();if($manage)begin_transaction();
    $same=hrm_pay_core_008_one('SELECT variance_id FROM '.TB_PREF.'hrm_pay_validation_variances WHERE variance_sha256='.db_escape($sha).' LIMIT 2 FOR UPDATE');if($same){if($manage)commit_transaction();return (int)$same['variance_id'];}
    $sql='INSERT INTO '.TB_PREF.'hrm_pay_validation_variances (variance_kind,legal_entity_id,payroll_relationship_id,current_result_id,comparison_result_id,current_result_sha256,comparison_result_sha256,currency,measure_set_json,threshold_policy_sha256,variance_sha256,executed_by) VALUES ('.db_escape($kind).','.$s['legal_entity_id'].','.$s['payroll_relationship_id'].','.(int)$current_result_id.','.(int)$comparison_result_id.','.db_escape($cr['result_sha256']).','.db_escape($pr['result_sha256']).','.db_escape($ct['currency']).','.db_escape(hrm_pay_core_004_canonical_json($v['measures'])).','.hrm_pay_core_008_sql_token($threshold_policy_sha).','.db_escape($sha).','.$actor.')';
    if(!db_query($sql)){if($manage)cancel_transaction();$error='variance_insert_failed';return false;}
    $id=(int)db_insert_id();if($manage)commit_transaction();return $id;
}

function hrm_pay_core_008_record_reconciliation($kind,array $scope,$result_set_sha,$source_evidence_sha,$expected,$actual,$unit,$currency,array $threshold_policy=null,$as_of=null,$evidence_sha=null,&$error=null)
{
    $error=null;$actor=hrm_pay_core_008_actor();if($actor<=0||!hrm_pay_core_008_access('SA_HRM_REVIEW_PAY_CORE_VALIDATION')){$error='access_denied';return false;}
    $s=hrm_pay_core_008_assert_object_scope($scope,$error);if($s===false)return false;
    if($threshold_policy!==null){$threshold_policy=hrm_pay_core_008_require_threshold_policy($threshold_policy,$error);if($threshold_policy===false)return false;}
    $r=hrm_pay_core_008_reconciliation($kind,$s,$result_set_sha,$source_evidence_sha,$expected,$actual,$unit,$currency,$threshold_policy,$as_of,$evidence_sha,$error);if($r===false)return false;
    $sql='INSERT INTO '.TB_PREF.'hrm_pay_validation_reconciliations (reconciliation_kind,legal_entity_id,payroll_relationship_id,calendar_period_id,result_set_sha256,source_evidence_sha256,expected_value,actual_value,difference_value,unit_code,currency_code,threshold_policy_sha256,reconciliation_state,evidence_sha256,reconciliation_sha256,reviewed_by) VALUES ('.db_escape($kind).','.$s['legal_entity_id'].','.$s['payroll_relationship_id'].','.hrm_pay_core_008_sql_id($s['calendar_period_id']).','.db_escape($result_set_sha).','.db_escape($source_evidence_sha).','.db_escape($r['expected_value']).','.db_escape($r['actual_value']).','.db_escape($r['difference_value']).','.db_escape($unit).','.hrm_pay_core_008_sql_token($currency).','.hrm_pay_core_008_sql_token($r['threshold_policy_sha256']).','.db_escape($r['reconciliation_state']).','.db_escape($r['evidence_sha256']).','.db_escape($r['reconciliation_sha256']).','.$actor.')';
    if(!db_query($sql)){$same=hrm_pay_core_008_one('SELECT reconciliation_id FROM '.TB_PREF.'hrm_pay_validation_reconciliations WHERE reconciliation_sha256='.db_escape($r['reconciliation_sha256']).' LIMIT 2');if($same)return (int)$same['reconciliation_id'];$error='reconciliation_insert_failed';return false;}
    return (int)db_insert_id();
}

/** Gross/net reconciliation derives the actual value from immutable result lines. */
function hrm_pay_core_008_record_result_reconciliation($kind,array $scope,$result_id,array $threshold_policy=null,$as_of=null,&$error=null)
{
    $error=null;if(!in_array($kind,array('gross','net'),true)){$error='result_reconciliation_kind_invalid';return false;}
    $s=hrm_pay_core_008_assert_object_scope($scope,$error);if($s===false)return false;$rr=hrm_pay_core_008_load_result($result_id,$error);if($rr===false)return false;$s=hrm_pay_core_008_bind_result_scope($s,$rr,$error);if($s===false)return false;
    $t=hrm_pay_core_008_result_totals($rr['descriptor'],$rr['lines'],$error);if($t===false)return false;
    $expected=$kind==='gross'?$rr['descriptor']['gross_amount']:$rr['descriptor']['net_amount'];$actual=$t[$kind];
    $result_set=hrm_pay_core_004_hash_set(array($rr['result_sha256']));
    return hrm_pay_core_008_record_reconciliation($kind,$s,$result_set,$t['evidence_sha256'],$expected,$actual,'money',$rr['descriptor']['currency'],$threshold_policy,$as_of,$t['evidence_sha256'],$error);
}

function hrm_pay_core_008_record_recovery($subject_type,$subject_sha,$action,$source_sha,$evidence_sha,&$error=null)
{
    $error=null;$actor=hrm_pay_core_008_actor();if($actor<=0||!hrm_pay_core_008_access('SA_HRM_EXECUTE_PAY_CORE_VALIDATION')){$error='access_denied';return false;}
    $r=hrm_pay_core_008_recovery($subject_type,$subject_sha,$action,$source_sha,$evidence_sha,$actor,$error);if($r===false)return false;
    $sql='INSERT INTO '.TB_PREF.'hrm_pay_validation_recovery_evidence (subject_type,subject_sha256,action_token,source_sha256,evidence_sha256,recovery_sha256,executed_by) VALUES ('.db_escape($r['subject_type']).','.db_escape($r['subject_sha256']).','.db_escape($r['action_token']).','.db_escape($r['source_sha256']).','.db_escape($r['evidence_sha256']).','.db_escape($r['recovery_sha256']).','.$actor.')';
    if(!db_query($sql)){$same=hrm_pay_core_008_one('SELECT recovery_evidence_id FROM '.TB_PREF.'hrm_pay_validation_recovery_evidence WHERE recovery_sha256='.db_escape($r['recovery_sha256']).' LIMIT 2');if($same)return (int)$same['recovery_evidence_id'];$error='recovery_insert_failed';return false;}
    return (int)db_insert_id();
}
?>
