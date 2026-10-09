<?php
/**
 * PAY-GLB-001 S35: non-authoritative PAY-CORE-009 item/PAY-CORE-004 result
 * diagnostic. Equal digests do not prove approved PAY-CORE-010 result-set
 * membership, source freshness, reviewer approval or correction finality.
 * No application route, mutation or credential handling is provided.
 */
require_once __DIR__.'/pay_glb_001_native_lineage_contract.inc';

/** Query a selected run's aggregate result-hash candidates, never employee data. */
function pay_glb_001_run_result_candidate_query($prefix,$scope,$entity,$selection)
{
    if (!pay_glb_001_lineage_selection_valid($scope,$entity,$selection) ||
        !is_string($prefix) || preg_match('/^[0-9]{1,5}_$/D',$prefix)!==1) return false;
    $p=$prefix;
    $run=(string)$selection['run_id'];
    $period=(string)$selection['payroll_period_id'];
    $entityId=(string)$entity['legal_entity_id'];
    $runs='`'.$p.'hrm_pay_core_009_runs`';
    $items='`'.$p.'hrm_pay_core_009_items`';
    $results='`'.$p.'hrm_pay_core_004_results`';
    $snapshots='`'.$p.'hrm_pay_core_004_snapshots`';
    $corrections='`'.$p.'hrm_pay_core_004_correction_lineage`';
    // A result_hash may be semantically different across CORE-009 and CORE-004.
    // This is a candidate comparison, NOT an approved-run or active-result join.
    return "SELECT COUNT(*) AS item_count,".
        "COALESCE(SUM(CASE WHEN i.item_state<>'completed' OR i.result_sha256 IS NULL " .
        "THEN 1 ELSE 0 END),0) AS incomplete_count,".
        "COALESCE(SUM(CASE WHEN r.result_id IS NULL OR s.snapshot_id IS NULL " .
        "THEN 1 ELSE 0 END),0) AS unresolved_count,".
        "COALESCE(SUM(CASE WHEN cl.lineage_id IS NOT NULL THEN 1 ELSE 0 END),0) AS superseded_count " .
        "FROM $runs run INNER JOIN $items i " .
        "ON i.run_id=run.run_id AND i.legal_entity_id=run.legal_entity_id " .
        "LEFT JOIN $results r ON r.result_sha256=i.result_sha256 " .
        "LEFT JOIN $snapshots s ON s.snapshot_id=r.snapshot_id " .
        "AND s.legal_entity_id=i.legal_entity_id " .
        "AND s.payroll_relationship_id=i.payroll_relationship_id " .
        "AND s.calendar_period_id=i.calendar_period_id " .
        "LEFT JOIN $corrections cl ON cl.predecessor_result_id=r.result_id " .
        "WHERE run.run_id=$run AND run.legal_entity_id=$entityId " .
        "AND run.payroll_period_id=$period";
}

/** Validate minimized candidate diagnostic without permitting authority promotion. */
function pay_glb_001_run_result_candidate_valid($scope,$entity,$selection,$diagnostic)
{
    if (!pay_glb_001_lineage_selection_valid($scope,$entity,$selection) ||
        !pay_glb_001_exact_keys($diagnostic,array('company_id','legal_entity_id',
            'entity_code','run_id','payroll_period_id','as_of_utc','status',
            'items_present','incomplete_items_present','unresolved_results_present',
            'superseded_results_present','same_run_result_hash_verified',
            'current_result_set_verified','approval_binding_verified',
            'production_route_allowed','close_ready','candidate_sha256')) ||
        $diagnostic['company_id']!==$scope['company_id'] ||
        $diagnostic['legal_entity_id']!==$entity['legal_entity_id'] ||
        $diagnostic['entity_code']!==$entity['entity_code'] ||
        $diagnostic['run_id']!==$selection['run_id'] ||
        $diagnostic['payroll_period_id']!==$selection['payroll_period_id'] ||
        $diagnostic['as_of_utc']!==$scope['as_of_utc'] ||
        $diagnostic['status']!=='UNVERIFIED_RESULT_HASH_CANDIDATE' ||
        !pay_glb_001_sha256($diagnostic['candidate_sha256'])) return false;
    foreach(array('items_present','incomplete_items_present',
        'unresolved_results_present','superseded_results_present') as $flag)
        if (!is_bool($diagnostic[$flag])) return false;
    foreach(array('same_run_result_hash_verified','current_result_set_verified',
        'approval_binding_verified','production_route_allowed','close_ready') as $flag)
        if ($diagnostic[$flag]!==false) return false;
    $copy=$diagnostic;unset($copy['candidate_sha256']);
    $json=json_encode($copy,JSON_UNESCAPED_SLASHES);
    return $json!==false && hash_equals(hash('sha256',$json),$diagnostic['candidate_sha256']);
}
?>
