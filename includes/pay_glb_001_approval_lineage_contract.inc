<?php
/**
 * PAY-GLB-001 S32: bounded GL-to-approved-PAY-CORE-010 *diagnostic* only.
 * GL journal has no proven PAY-CORE-009 run FK. No "approved run" can be
 * inferred from a shared company/entity/period, equal hash, or this query.
 * This module contains no route, grants, writes, external calls or escalation.
 */
require_once __DIR__.'/pay_glb_001_native_lineage_contract.inc';

/** Build one fixed, integer-scoped approval/journal aggregate; never infer run lineage. */
function pay_glb_001_approval_lineage_query($prefix,$scope,$entity,$selection)
{
    if (!pay_glb_001_lineage_selection_valid($scope,$entity,$selection) ||
        !is_string($prefix) || preg_match('/^[0-9]{1,5}_$/D',$prefix)!==1) return false;
    $p=$prefix;
    $c=(string)$scope['company_id'];
    $e=(string)$entity['legal_entity_id'];
    $period=(string)$selection['payroll_period_id'];
    $j='`'.$p.'hrm_pay_gl_001_journal_intents`';
    $a='`'.$p.'hrm_pay_core_010_approvals`';
    $i='`'.$p.'hrm_pay_core_010_approval_intents`';
    // A missing or revoked/unsupported approval is a discrepancy. A matching
    // approval is NOT sufficient run/reconciliation evidence; the query has
    // deliberately no run_id predicate on journals (no such column exists).
    return "SELECT COUNT(*) AS journal_count,".
       "COALESCE(SUM(CASE WHEN a.approval_id IS NULL OR i.approval_intent_id IS NULL ".
       "THEN 1 ELSE 0 END),0) AS unbound_count,".
       "COUNT(DISTINCT j.result_set_sha256) AS distinct_result_sets,".
       "COUNT(DISTINCT j.approval_sha256) AS distinct_approvals ".
       "FROM $j j LEFT JOIN $a a ".
       "ON a.approval_sha256=j.approval_sha256 ".
       "AND a.result_set_sha256=j.result_set_sha256 ".
       "AND a.payroll_period_id=j.payroll_period_id ".
       "AND LOWER(a.decision_token)='approved' ".
       "LEFT JOIN $i i ON i.approval_intent_id=a.approval_intent_id ".
       "AND i.result_set_sha256=j.result_set_sha256 ".
       "AND i.payroll_period_id=j.payroll_period_id ".
       "AND i.payroll_group_id=j.payroll_group_id ".
       "AND i.legal_entity_id=j.legal_entity_id ".
       "WHERE j.company_id=$c AND j.legal_entity_id=$e ".
       "AND j.payroll_period_id=$period";
}

/** Reject altered, replayed or authority-promoting diagnostic projections. */
function pay_glb_001_approval_lineage_diagnostic_valid($scope,$entity,$selection,$d)
{
    if (!pay_glb_001_lineage_selection_valid($scope,$entity,$selection) ||
        !pay_glb_001_exact_keys($d,array('company_id','legal_entity_id','run_id',
            'payroll_period_id','entity_code','as_of_utc','status','journal_present',
            'unbound_approval_present','multiple_result_sets','multiple_approvals',
            'same_run_result_hash_verified','reconciled_verified',
            'production_route_allowed','close_ready','diagnostic_sha256')) ||
        $d['company_id']!==$scope['company_id'] ||
        $d['legal_entity_id']!==$entity['legal_entity_id'] ||
        $d['run_id']!==$selection['run_id'] ||
        $d['payroll_period_id']!==$selection['payroll_period_id'] ||
        $d['entity_code']!==$entity['entity_code'] ||
        $d['as_of_utc']!==$scope['as_of_utc'] ||
        $d['status']!=='UNVERIFIED_APPROVAL_RUN_LINK' ||
        !pay_glb_001_sha256($d['diagnostic_sha256'])) return false;
    foreach (array('journal_present','unbound_approval_present',
        'multiple_result_sets','multiple_approvals') as $key)
        if (!is_bool($d[$key])) return false;
    foreach (array('same_run_result_hash_verified','reconciled_verified',
        'production_route_allowed','close_ready') as $key)
        if ($d[$key]!==false) return false;
    $copy=$d;unset($copy['diagnostic_sha256']);
    $json=json_encode($copy,JSON_UNESCAPED_SLASHES);
    return $json!==false && hash_equals(hash('sha256',$json),$d['diagnostic_sha256']);
}
?>
