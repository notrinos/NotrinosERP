<?php
/**
 * PAY-GLB-001 S29: source-verified run/period linkage contract (INTERNAL ONLY).
 * None of these queries proves the PAY-GL result-set hash equals a PAY-CORE
 * result set. A company/entity/period coincidence cannot grant authority.
 */
require_once __DIR__.'/pay_glb_001_native_source_contract.inc';

/** Validate one server-selected native run. No request-derived SQL identifiers. */
function pay_glb_001_lineage_selection_valid($scope, $entity, $selection)
{
    return pay_glb_001_native_scope_valid($scope,array($entity)) &&
        pay_glb_001_exact_keys($selection,array('run_id','payroll_period_id')) &&
        is_int($selection['run_id']) && $selection['run_id']>0 &&
        is_int($selection['payroll_period_id']) && $selection['payroll_period_id']>0;
}

/** Build only fixed selected-run and bounded aggregate queries on accepted DDL. */
function pay_glb_001_lineage_queries($prefix, $scope, $entity, $selection)
{
    if (!pay_glb_001_lineage_selection_valid($scope,$entity,$selection) ||
        !is_string($prefix) || preg_match('/^[0-9]{1,5}_$/D',$prefix)!==1) return false;
    $p=$prefix;$c=(string)$scope['company_id'];$e=(string)$entity['legal_entity_id'];
    $r=(string)$selection['run_id'];$per=(string)$selection['payroll_period_id'];
    $runs='`'.$p.'hrm_pay_core_009_runs`';
    $items='`'.$p.'hrm_pay_core_009_items`';
    $journal='`'.$p.'hrm_pay_gl_001_journal_intents`';
    $post='`'.$p.'hrm_pay_gl_001_gl_postings`';
    $glrec='`'.$p.'hrm_pay_gl_001_reconciliations`';
    $profile='`'.$p.'hrm_pay_rec_001_profiles`';
    $checks='`'.$p.'hrm_pay_rec_001_checklists`';
    $runWhere="run_id=$r AND legal_entity_id=$e AND payroll_period_id=$per";
    $jWhere="j.company_id=$c AND j.legal_entity_id=$e AND j.payroll_period_id=$per";
    $prWhere="p.company_id=$c AND p.legal_entity_id=$e AND p.payroll_period_id=$per AND p.pay_core_run_id=$r";
    return array(
        'run'=>"SELECT state,state_version,state_sha256 FROM $runs WHERE $runWhere LIMIT 2",
        'items'=>"SELECT COUNT(*) AS total_count, COALESCE(SUM(CASE WHEN i.item_state='completed' AND i.result_sha256 REGEXP '^[a-f0-9]{64}$' THEN 0 ELSE 1 END),0) AS incomplete_count FROM $items i INNER JOIN $runs r ON r.run_id=i.run_id AND r.legal_entity_id=i.legal_entity_id WHERE i.run_id=$r AND r.legal_entity_id=$e AND r.payroll_period_id=$per",
        'journal'=>"SELECT COUNT(*) AS c FROM $journal j WHERE $jWhere",
        'postings'=>"SELECT COUNT(*) AS c FROM $post g INNER JOIN $journal j ON j.journal_intent_id=g.journal_intent_id WHERE $jWhere",
        'gl_reconciliations'=>"SELECT COUNT(*) AS c FROM $glrec gr INNER JOIN $journal j ON j.journal_intent_id=gr.journal_intent_id WHERE $jWhere",
        'profiles'=>"SELECT COUNT(*) AS c FROM $profile p INNER JOIN $runs r ON r.run_id=p.pay_core_run_id AND r.legal_entity_id=p.legal_entity_id AND r.payroll_period_id=p.payroll_period_id WHERE $prWhere",
        'checklists'=>"SELECT COUNT(*) AS c FROM $checks cl INNER JOIN $profile p ON p.reconciliation_profile_id=cl.reconciliation_profile_id INNER JOIN $runs r ON r.run_id=p.pay_core_run_id AND r.legal_entity_id=p.legal_entity_id AND r.payroll_period_id=p.payroll_period_id WHERE $prWhere",
        'currency_mismatches'=>"SELECT COUNT(*) AS c FROM $journal j INNER JOIN $profile p ON p.company_id=j.company_id AND p.legal_entity_id=j.legal_entity_id AND p.payroll_period_id=j.payroll_period_id WHERE $jWhere AND p.pay_core_run_id=$r AND j.currency_code<>p.currency_code"
    );
}

/** Verify an entire sanitized, non-authoritative projection and exact digest. */
function pay_glb_001_lineage_diagnostic_valid($scope,$entity,$selection,$diagnostic)
{
    if (!pay_glb_001_lineage_selection_valid($scope,$entity,$selection) ||
        !pay_glb_001_exact_keys($diagnostic,array('company_id','legal_entity_id',
            'run_id','payroll_period_id','entity_code','as_of_utc',
            'status','run_completed','all_items_completed','journal_present',
            'posting_present','gl_reconciliation_present','rec_profile_present',
            'checklist_present','currency_mismatch_present','lineage_sha256',
            'same_run_result_hash_verified','source_freshness_verified',
            'production_route_allowed','reconciled_verified')) ||
        $diagnostic['company_id']!==$scope['company_id'] ||
        $diagnostic['legal_entity_id']!==$entity['legal_entity_id'] ||
        $diagnostic['run_id']!==$selection['run_id'] ||
        $diagnostic['payroll_period_id']!==$selection['payroll_period_id'] ||
        $diagnostic['entity_code']!==$entity['entity_code'] ||
        $diagnostic['as_of_utc']!==$scope['as_of_utc'] ||
        $diagnostic['status']!=='UNVERIFIED_SAME_RUN_LINEAGE' ||
        !pay_glb_001_sha256($diagnostic['lineage_sha256'])) return false;
    foreach (array('run_completed','all_items_completed','journal_present','posting_present',
        'gl_reconciliation_present','rec_profile_present','checklist_present',
        'currency_mismatch_present') as $key) if (!is_bool($diagnostic[$key])) return false;
    foreach (array('same_run_result_hash_verified','source_freshness_verified',
        'production_route_allowed','reconciled_verified') as $key)
        if ($diagnostic[$key]!==false) return false;
    $copy=$diagnostic;unset($copy['lineage_sha256']);
    $json=json_encode($copy,JSON_UNESCAPED_SLASHES);
    return $json!==false && hash_equals(hash('sha256',$json),$diagnostic['lineage_sha256']);
}
?>
