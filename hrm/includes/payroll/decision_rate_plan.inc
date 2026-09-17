<?php
/** PAY-RULE-003 deterministic decision/rate compile-plan foundation. */
include_once(__DIR__.'/decision_table_contract.inc');
include_once(__DIR__.'/rate_table_contract.inc');
if (!defined('HRM_PAY_RULE_003_PLAN_VERSION'))
    define('HRM_PAY_RULE_003_PLAN_VERSION', '1.0.475');

function hrm_payroll_rule_plan_fail(&$error, $code)
{
    $error = $code;
    return false;
}

function hrm_payroll_rule_plan_hash($plan)
{
    $copy = $plan;
    unset($copy['plan_hash']);
    return hash('sha256', json_encode(hrm_payroll_decision_table_canonicalize($copy), JSON_UNESCAPED_SLASHES));
}

function hrm_payroll_decision_table_compile($artifact, &$error)
{
    if (!hrm_payroll_decision_table_validate($artifact, $error)) return false;
    $rows = $artifact['rows'];
    usort($rows, function($a,$b) use ($artifact) {
        if ($artifact['hit_policy'] === 'first_by_priority') {
            if ($a['priority'] !== $b['priority']) return $a['priority'] < $b['priority'] ? -1 : 1;
        }
        return strcmp($a['row_id'], $b['row_id']);
    });
    $plan = array(
        'plan_schema'=>1,'plan_type'=>'decision_table','compiler_version'=>HRM_PAY_RULE_003_PLAN_VERSION,
        'table_id'=>$artifact['table_id'],'artifact_version'=>$artifact['version'],'artifact_hash'=>hrm_payroll_decision_table_hash($artifact,$error),
        'hit_policy'=>$artifact['hit_policy'],'overlap_policy'=>$artifact['overlap_policy'],'gap_policy'=>$artifact['gap_policy'],
        'inputs'=>$artifact['inputs'],'outputs'=>$artifact['outputs'],'rows'=>$rows
    );
    if ($plan['artifact_hash'] === false) return false;
    $plan['plan_hash'] = hrm_payroll_rule_plan_hash($plan);
    return $plan;
}

function hrm_payroll_rate_table_compile($artifact, &$error)
{
    if (!hrm_payroll_rate_table_validate($artifact, $error)) return false;
    $brackets = $artifact['brackets'];
    usort($brackets, 'hrm_payroll_rate_table_sort_brackets');
    $vectors = $artifact['test_vectors'];
    usort($vectors, function($a,$b){ return strcmp($a['vector_id'],$b['vector_id']); });
    $plan = array(
        'plan_schema'=>1,'plan_type'=>'rate_table','compiler_version'=>HRM_PAY_RULE_003_PLAN_VERSION,
        'table_id'=>$artifact['table_id'],'artifact_version'=>$artifact['version'],'artifact_hash'=>hrm_payroll_rate_table_hash($artifact,$error),
        'legal_interval'=>$artifact['legal_interval'],'publication_interval'=>$artifact['publication_interval'],
        'jurisdiction'=>$artifact['jurisdiction'],'currency'=>$artifact['currency'],'unit'=>$artifact['unit'],'period_basis'=>$artifact['period_basis'],
        'rounding'=>$artifact['rounding'],'gap_policy'=>$artifact['gap_policy'],'brackets'=>$brackets,'test_vectors'=>$vectors
    );
    if ($plan['artifact_hash'] === false) return false;
    $plan['plan_hash'] = hrm_payroll_rule_plan_hash($plan);
    return $plan;
}
