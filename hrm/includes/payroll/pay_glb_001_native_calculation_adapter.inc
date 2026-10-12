<?php
/**
 * S39 versioned, pure CORE-005 to CORE-004 storage adaptation.
 * Executes a validated plan, preserves its original identity and binds the
 * lossless decimal spelling change in a new hash-only trace. No DB, session,
 * business approval, producer completion, route or financial authority.
 */
require_once dirname(__FILE__).'/pay_core_005_runtime.inc';
require_once dirname(__FILE__).'/pay_glb_001_native_execution_contract.inc';

/** Return the explicit forward-only storage contract, separate from CORE-005. */
function pay_glb_001_native_calculation_storage_version()
{
    return 'PAY_GLB_001_CORE004_STORAGE_V1';
}

/**
 * Pad canonical CORE-005 scale-six text to CORE-004 DECIMAL(24,8).
 * Reject noncanonical input and overflow before SQL; never round or use floats.
 */
function pay_glb_001_native_calculation_decimal_8($value, &$error=null)
{
    $error=null;
    if (!is_string($value) || preg_match('/^-?(?:0|[1-9][0-9]{0,15})\\.[0-9]{6}$/D',$value)!==1
        || $value==='-0.000000') {
        $error='native_calculation_decimal_storage_invalid';return false;
    }
    return $value.'00';
}

/**
 * Execute the existing validated pure plan and derive a fresh storage bundle.
 * Only this plan path is accepted: callers cannot submit a prebuilt result.
 * A new provenance trace binds the original plan/bundle/result, original trace
 * set, transformed totals/line set and adapter version before final hashing.
 */
function pay_glb_001_native_calculation_execute_v1(array $plan, &$error=null)
{
    $error=null;
    if (hrm_pay_core_005_validate_plan($plan,$error)===false) return false;
    $scope=$plan['snapshot_material']['descriptor'];
    $ids=array($scope['payroll_relationship_id'],$scope['calendar_period_id'],$scope['legal_entity_id']);
    if (isset($scope['assignment_id'])) $ids[]=$scope['assignment_id'];
    foreach ($plan['nodes'] as $node) if ($node['emit_line']) $ids[]=$node['element_version_id'];
    foreach ($ids as $id) {
        if ((!is_int($id) && !is_string($id)) || pay_glb_001_native_helper_id((string)$id,$error)===false) {
            $error='native_calculation_helper_id_invalid';return false;
        }
    }
    if ($plan['currency']!==$scope['currency']) {
        $error='native_calculation_snapshot_currency_mismatch';return false;
    }
    $original=hrm_pay_core_005_execute_plan($plan,$error);
    if ($original===false) return false;
    $result=$original['result'];$lines=$original['lines'];$traces=$original['traces'];
    foreach (array('gross_amount','deduction_amount','net_amount') as $field) {
        $result[$field]=pay_glb_001_native_calculation_decimal_8($result[$field],$error);
        if ($result[$field]===false) return false;
    }
    if (!pay_glb_001_native_totals_match($result['gross_amount'],$result['deduction_amount'],$result['net_amount'])) {
        $error='native_calculation_totals_mismatch';return false;
    }
    $line_hashes=array();$gross='0.00000000';$deduction='0.00000000';
    foreach ($lines as &$line) {
        foreach (array('amount','quantity','rate') as $field) {
            $line[$field]=pay_glb_001_native_calculation_decimal_8($line[$field],$error);
            if ($line[$field]===false) { unset($line);return false; }
        }
        $hash=hrm_pay_core_004_result_line_sha256($line);
        if ($hash===false) { unset($line);$error='native_calculation_line_invalid';return false; }
        $line_hashes[]=$hash;
        if ($line['line_kind']==='earning') $gross=pay_glb_001_native_decimal_add($gross,$line['amount'],$error);
        elseif ($line['line_kind']==='deduction') $deduction=pay_glb_001_native_decimal_add($deduction,$line['amount'],$error);
        if ($gross===false || $deduction===false) { unset($line);return false; }
    }
    unset($line);
    if ($gross!==$result['gross_amount'] || $deduction!==$result['deduction_amount']) {
        $error='native_calculation_line_totals_mismatch';return false;
    }
    $result['line_set_sha256']=hrm_pay_core_004_hash_set($line_hashes);
    $binding=array(
        'storage_contract_version'=>pay_glb_001_native_calculation_storage_version(),
        'core005_contract_version'=>HRM_PAY_CORE_005_CONTRACT_VERSION,
        'plan_sha256'=>$original['plan_sha256'],
        'core005_bundle_sha256'=>$original['bundle_sha256'],
        'core005_result_sha256'=>$original['result']['result_sha256'],
        'snapshot_sha256'=>$result['snapshot_sha256'],
        'currency'=>$result['currency'],
        'gross_8'=>$result['gross_amount'],
        'deduction_8'=>$result['deduction_amount'],
        'net_8'=>$result['net_amount'],
        'stored_line_set_sha256'=>$result['line_set_sha256'],
        'original_trace_set_sha256'=>$original['result']['trace_set_sha256']
    );
    $binding_json=hrm_pay_core_004_canonical_json($binding);
    if ($binding_json===false) { $error='native_calculation_binding_invalid';return false; }
    $binding_sha=hash('sha256',"PAY-GLB-001|calculation-storage|v1\n".$binding_json);
    $last=0;
    foreach ($traces as $trace) $last=max($last,(int)$trace['trace_seq']);
    // Reserve one new hash-only custody row without exceeding CORE-005 limits.
    $limits=hrm_pay_core_005_resource_limits();
    if (count($traces)>=$limits['max_trace_rows'] || $last>=4294967295) {
        $error='native_calculation_storage_trace_limit';return false;
    }
    $traces[]=array(
        'trace_seq'=>$last+1,'trace_kind'=>'pay_glb_001.storage_v1',
        'artifact_sha256'=>hash('sha256',pay_glb_001_native_calculation_storage_version()),
        'input_sha256'=>$original['bundle_sha256'],'output_sha256'=>$binding_sha,
        'value_mode'=>'hash_only','value_digest'=>$binding_sha
    );
    $trace_hashes=array();
    foreach ($traces as $trace) {
        $hash=hrm_pay_core_004_trace_sha256($trace);
        if ($hash===false) { $error='native_calculation_trace_invalid';return false; }
        $trace_hashes[]=$hash;
    }
    $result['trace_set_sha256']=hrm_pay_core_004_hash_set($trace_hashes);
    unset($result['result_sha256']);
    $result_sha=hrm_pay_core_004_result_sha256($result);
    if ($result_sha===false) { $error='native_calculation_result_invalid';return false; }
    $result['result_sha256']=$result_sha;
    $bundle=array(
        'storage_contract_version'=>pay_glb_001_native_calculation_storage_version(),
        'plan_sha256'=>$original['plan_sha256'],
        'core005_bundle_sha256'=>$original['bundle_sha256'],
        'storage_binding'=>$binding,'storage_binding_sha256'=>$binding_sha,
        'result'=>$result,'lines'=>$lines,'traces'=>$traces
    );
    $json=hrm_pay_core_004_canonical_json($bundle);
    if ($json===false) { $error='native_calculation_bundle_invalid';return false; }
    $bundle['bundle_sha256']=hash('sha256',"PAY-GLB-001|calculation-bundle|v1\n".$json);
    return $bundle;
}
