<?php
/**
 * PAY-CORE-005 governed bridge from an already sealed PAY-CORE-004 snapshot to
 * the deterministic calculator and back to PAY-CORE-004 immutable results.
 *
 * Deliberately not included by payroll_engine.inc: this is not production
 * payroll calculation authority and owns no GL, journal, bank or payment path.
 */
include_once(dirname(dirname(__FILE__)).'/payroll/pay_core_005_runtime.inc');
include_once(dirname(__FILE__).'/pay_core_004_governance_db.inc');

function hrm_pay_core_005_actor(){return isset($_SESSION['wa_current_user']->user)?(int)$_SESSION['wa_current_user']->user:0;}
function hrm_pay_core_005_access($permission){return function_exists('user_check_access')&&user_check_access($permission);}

function hrm_pay_core_005_snapshot_from_row(array $row,&$error=null)
{
    $error=null;
    $payload=json_decode((string)$row['snapshot_payload_json'],true);
    if(!is_array($payload)||!isset($payload['descriptor'],$payload['identity_sets'])){$error='pc5_snapshot_payload_invalid';return false;}
    $material=array('descriptor'=>$payload['descriptor'],'identity_sets'=>$payload['identity_sets'],'payload_json'=>(string)$row['snapshot_payload_json'],'snapshot_sha256'=>(string)$row['snapshot_sha256']);
    if(!hrm_pay_core_005_validate_snapshot_material($material,$error))return false;
    return $material;
}

function hrm_pay_core_005_scope_matches(array $descriptor,array $expected)
{
    foreach(array('payroll_relationship_id','calendar_period_id','legal_entity_id') as $k)
        if(!isset($expected[$k])||(int)$descriptor[$k] !== (int)$expected[$k])return false;
    $expected_assignment=array_key_exists('assignment_id',$expected)&&$expected['assignment_id']!==null?(int)$expected['assignment_id']:null;
    $actual_assignment=array_key_exists('assignment_id',$descriptor)&&$descriptor['assignment_id']!==null?(int)$descriptor['assignment_id']:null;
    if($actual_assignment!==$expected_assignment)return false;
    if(isset($expected['run_identity'])&&(string)$descriptor['run_identity']!==(string)$expected['run_identity'])return false;
    return true;
}

/**
 * Execute an explicit deterministic plan against one locked, sealed snapshot.
 * Same-actor exact replay is idempotent through PAY-CORE-004 append_result;
 * drift or a different executor fails closed there.
 */
function hrm_pay_core_005_execute_sealed_snapshot($snapshot_id,array $expected_scope,array $plan,&$error=null)
{
    $error=null;$snapshot_id=(int)$snapshot_id;$actor=hrm_pay_core_005_actor();
    if($snapshot_id<=0||$actor<=0||!hrm_pay_core_005_access('SA_HRM_EXECUTE_PAY_CORE_CALC')||!hrm_pay_core_005_access('SA_HRM_WRITE_PAY_CORE_RESULT')){$error='pc5_access_denied';return false;}
    if(!empty($GLOBALS['transaction_level'])){$error='pc5_top_level_transaction_required';return false;}
    begin_transaction();
    $q=db_query('SELECT snapshot_id,snapshot_payload_json,snapshot_sha256,sealed_by FROM '.TB_PREF.'hrm_pay_core_004_snapshots WHERE snapshot_id='.$snapshot_id.' LIMIT 2 FOR UPDATE');
    $row=$q&&db_num_rows($q)===1?db_fetch_assoc($q):false;
    if(!$row){cancel_transaction();$error='pc5_snapshot_missing_or_ambiguous';return false;}
    if((int)$row['sealed_by']===$actor){cancel_transaction();$error='pc5_independent_executor_required';return false;}
    $material=hrm_pay_core_005_snapshot_from_row($row,$error);if($material===false){cancel_transaction();return false;}
    if(!hrm_pay_core_005_scope_matches($material['descriptor'],$expected_scope)){cancel_transaction();$error='pc5_object_scope_denied';return false;}
    if(!isset($plan['snapshot_sha256'],$plan['snapshot_material'])||!hash_equals($material['snapshot_sha256'],(string)$plan['snapshot_sha256'])||hrm_pay_core_005_canonical_json($plan['snapshot_material'])!==hrm_pay_core_005_canonical_json($material)){cancel_transaction();$error='pc5_plan_snapshot_binding_mismatch';return false;}
    $bundle=hrm_pay_core_005_execute_plan($plan,$error);if($bundle===false){cancel_transaction();return false;}
    $totals=array('currency'=>$bundle['result']['currency'],'gross_amount'=>$bundle['result']['gross_amount'],'deduction_amount'=>$bundle['result']['deduction_amount'],'net_amount'=>$bundle['result']['net_amount']);
    $result_id=hrm_pay_core_004_append_result($snapshot_id,$totals,$bundle['lines'],$bundle['traces'],$error);
    if($result_id===false){cancel_transaction();return false;}
    commit_transaction();
    return array('result_id'=>(int)$result_id,'plan_sha256'=>$bundle['plan_sha256'],'bundle_sha256'=>$bundle['bundle_sha256'],'result_sha256'=>$bundle['result']['result_sha256']);
}
