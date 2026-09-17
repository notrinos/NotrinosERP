<?php
/** PAY-RULE-003 pure typed rate-table contract foundation. */
include_once(__DIR__.'/decision_table_contract.inc');
if (!defined('HRM_PAY_RULE_003_RATE_CONTRACT_VERSION'))
    define('HRM_PAY_RULE_003_RATE_CONTRACT_VERSION', '1.0.474');

function hrm_payroll_rate_table_fail(&$error, $code)
{
    $error = $code;
    return false;
}

function hrm_payroll_rate_table_decimal($value)
{
    return is_string($value) && preg_match('/^-?(0|[1-9][0-9]*)(?:\.[0-9]+)?$/D', $value) === 1;
}

function hrm_payroll_rate_table_nullable_decimal($value)
{
    return $value === null || hrm_payroll_rate_table_decimal($value);
}

function hrm_payroll_rate_table_valid_currency($value)
{
    return $value === null || (is_string($value) && preg_match('/^[A-Z]{3}$/D', $value) === 1);
}

function hrm_payroll_rate_table_validate_interval($value, $prefix, &$error)
{
    if (!hrm_payroll_decision_table_exact_keys($value, array('from','to'), array(), $error, $prefix)) return false;
    if (!hrm_payroll_decision_table_valid_date($value['from'])) return hrm_payroll_rate_table_fail($error, $prefix.'_from_invalid');
    if ($value['to'] !== null && !hrm_payroll_decision_table_valid_date($value['to'])) return hrm_payroll_rate_table_fail($error, $prefix.'_to_invalid');
    if ($value['to'] !== null && strcmp($value['from'], $value['to']) > 0) return hrm_payroll_rate_table_fail($error, $prefix.'_order_invalid');
    return true;
}

function hrm_payroll_rate_table_validate_rounding($value, &$error)
{
    if (!hrm_payroll_decision_table_exact_keys($value, array('mode','scale','boundary'), array(), $error, 'rate_rounding')) return false;
    if (!in_array($value['mode'], array('half_up','half_even','down','up','truncate'), true)) return hrm_payroll_rate_table_fail($error, 'rate_rounding_mode_invalid');
    if (!is_int($value['scale']) || $value['scale'] < 0 || $value['scale'] > 12) return hrm_payroll_rate_table_fail($error, 'rate_rounding_scale_invalid');
    if (!in_array($value['boundary'], array('per_bracket','per_line','final_result'), true)) return hrm_payroll_rate_table_fail($error, 'rate_rounding_boundary_invalid');
    return true;
}

function hrm_payroll_rate_table_sort_brackets($left, $right)
{
    if ($left['lower_bound'] === null && $right['lower_bound'] !== null) return -1;
    if ($left['lower_bound'] !== null && $right['lower_bound'] === null) return 1;
    if ($left['lower_bound'] !== null && $right['lower_bound'] !== null) {
        $cmp = hrm_payroll_decision_table_decimal_compare($left['lower_bound'], $right['lower_bound']);
        if ($cmp !== 0) return $cmp;
    }
    return strcmp($left['bracket_id'], $right['bracket_id']);
}

function hrm_payroll_rate_table_validate($artifact, &$error)
{
    $error = null;
    $required = array('schema_version','artifact_type','table_id','version','value_type','jurisdiction','currency','unit','period_basis','legal_interval','publication_interval','rate_set_revision','source_ids','rounding','overlap_policy','gap_policy','brackets','test_vectors');
    if (!hrm_payroll_decision_table_exact_keys($artifact, $required, array('description'), $error, 'rate_artifact')) return false;
    if ($artifact['schema_version'] !== 1 || $artifact['artifact_type'] !== 'rate_table') return hrm_payroll_rate_table_fail($error, 'rate_artifact_identity_invalid');
    if (!hrm_payroll_decision_table_valid_id($artifact['table_id']) || !hrm_payroll_decision_table_valid_semver($artifact['version'])) return hrm_payroll_rate_table_fail($error, 'rate_artifact_version_invalid');
    if (!in_array($artifact['value_type'], array('rate','amount','quantity','index','threshold','bracket'), true)) return hrm_payroll_rate_table_fail($error, 'rate_value_type_invalid');
    if (!hrm_payroll_decision_table_valid_id($artifact['jurisdiction'])) return hrm_payroll_rate_table_fail($error, 'rate_jurisdiction_invalid');
    if (!hrm_payroll_rate_table_valid_currency($artifact['currency'])) return hrm_payroll_rate_table_fail($error, 'rate_currency_invalid');
    if (!in_array($artifact['unit'], array('currency','quantity','rate','index','threshold','scalar'), true)) return hrm_payroll_rate_table_fail($error, 'rate_unit_invalid');
    if ($artifact['unit'] === 'currency' && $artifact['currency'] === null) return hrm_payroll_rate_table_fail($error, 'rate_currency_required');
    if (!in_array($artifact['period_basis'], array('event','day','week','month','payroll_period','quarter','year'), true)) return hrm_payroll_rate_table_fail($error, 'rate_period_basis_invalid');
    if (!hrm_payroll_rate_table_validate_interval($artifact['legal_interval'], 'rate_legal_interval', $error)) return false;
    if (!hrm_payroll_rate_table_validate_interval($artifact['publication_interval'], 'rate_publication_interval', $error)) return false;
    if (!hrm_payroll_decision_table_valid_id($artifact['rate_set_revision'])) return hrm_payroll_rate_table_fail($error, 'rate_revision_invalid');
    if (!hrm_payroll_decision_table_is_list($artifact['source_ids']) || count($artifact['source_ids']) < 1 || count($artifact['source_ids']) > 32) return hrm_payroll_rate_table_fail($error, 'rate_source_ids_invalid');
    $source_ids = array();
    foreach ($artifact['source_ids'] as $source) {
        if (!hrm_payroll_decision_table_valid_id($source)) return hrm_payroll_rate_table_fail($error, 'rate_source_id_invalid');
        if (isset($source_ids[$source])) return hrm_payroll_rate_table_fail($error, 'rate_source_id_duplicate');
        $source_ids[$source] = true;
    }
    if (!hrm_payroll_rate_table_validate_rounding($artifact['rounding'], $error)) return false;
    if ($artifact['overlap_policy'] !== 'reject') return hrm_payroll_rate_table_fail($error, 'rate_overlap_policy_invalid');
    if (!in_array($artifact['gap_policy'], array('reject','allow'), true)) return hrm_payroll_rate_table_fail($error, 'rate_gap_policy_invalid');
    if (!hrm_payroll_decision_table_is_list($artifact['brackets']) || count($artifact['brackets']) < 1 || count($artifact['brackets']) > 1000) return hrm_payroll_rate_table_fail($error, 'rate_brackets_invalid');

    $ids = array(); $brackets = array(); $boundaries = array();
    foreach ($artifact['brackets'] as $bracket) {
        $required_bracket = array('bracket_id','lower_bound','upper_bound','lower_inclusive','upper_inclusive','base_amount','fixed_amount','marginal_rate','floor','cap','employee_rate','employer_rate','source_ids','explanation_label');
        if (!hrm_payroll_decision_table_exact_keys($bracket, $required_bracket, array(), $error, 'rate_bracket')) return false;
        if (!hrm_payroll_decision_table_valid_id($bracket['bracket_id'])) return hrm_payroll_rate_table_fail($error, 'rate_bracket_id_invalid');
        if (isset($ids[$bracket['bracket_id']])) return hrm_payroll_rate_table_fail($error, 'rate_bracket_id_duplicate');
        $ids[$bracket['bracket_id']] = true;
        if (!hrm_payroll_rate_table_nullable_decimal($bracket['lower_bound']) || !hrm_payroll_rate_table_nullable_decimal($bracket['upper_bound'])) return hrm_payroll_rate_table_fail($error, 'rate_bracket_bound_invalid');
        if (!is_bool($bracket['lower_inclusive']) || !is_bool($bracket['upper_inclusive'])) return hrm_payroll_rate_table_fail($error, 'rate_bracket_inclusivity_invalid');
        if ($bracket['lower_bound'] === null && $bracket['lower_inclusive']) return hrm_payroll_rate_table_fail($error, 'rate_bracket_open_lower_inclusive');
        if ($bracket['upper_bound'] === null && $bracket['upper_inclusive']) return hrm_payroll_rate_table_fail($error, 'rate_bracket_open_upper_inclusive');
        if ($bracket['lower_bound'] !== null && $bracket['upper_bound'] !== null && hrm_payroll_decision_table_decimal_compare($bracket['lower_bound'], $bracket['upper_bound']) >= 0) return hrm_payroll_rate_table_fail($error, 'rate_bracket_interval_invalid');
        foreach (array('base_amount','fixed_amount','marginal_rate','floor','cap','employee_rate','employer_rate') as $key)
            if (!hrm_payroll_rate_table_nullable_decimal($bracket[$key])) return hrm_payroll_rate_table_fail($error, 'rate_bracket_'.$key.'_invalid');
        if ($bracket['floor'] !== null && $bracket['cap'] !== null && hrm_payroll_decision_table_decimal_compare($bracket['floor'], $bracket['cap']) > 0) return hrm_payroll_rate_table_fail($error, 'rate_bracket_floor_cap_invalid');
        if (!hrm_payroll_decision_table_is_list($bracket['source_ids']) || count($bracket['source_ids']) < 1 || count($bracket['source_ids']) > 16) return hrm_payroll_rate_table_fail($error, 'rate_bracket_source_ids_invalid');
        foreach ($bracket['source_ids'] as $source) if (!isset($source_ids[$source])) return hrm_payroll_rate_table_fail($error, 'rate_bracket_source_unknown');
        if (!is_string($bracket['explanation_label']) || trim($bracket['explanation_label']) === '' || strlen($bracket['explanation_label']) > 200) return hrm_payroll_rate_table_fail($error, 'rate_bracket_explanation_invalid');
        if ($bracket['lower_bound'] !== null) $boundaries[$bracket['lower_bound']] = true;
        if ($bracket['upper_bound'] !== null) $boundaries[$bracket['upper_bound']] = true;
        $brackets[] = $bracket;
    }
    usort($brackets, 'hrm_payroll_rate_table_sort_brackets');
    for ($i=0; $i<count($brackets); $i++) {
        if ($i > 0 && $brackets[$i]['lower_bound'] === null) return hrm_payroll_rate_table_fail($error, 'rate_bracket_multiple_open_lower');
        if ($i < count($brackets)-1 && $brackets[$i]['upper_bound'] === null) return hrm_payroll_rate_table_fail($error, 'rate_bracket_open_upper_not_last');
        if ($i === 0) continue;
        $prev = $brackets[$i-1]; $cur = $brackets[$i];
        if ($prev['upper_bound'] === null) return hrm_payroll_rate_table_fail($error, 'rate_bracket_overlap');
        $cmp = hrm_payroll_decision_table_decimal_compare($prev['upper_bound'], $cur['lower_bound']);
        if ($cmp > 0) return hrm_payroll_rate_table_fail($error, 'rate_bracket_overlap');
        if ($cmp < 0 && $artifact['gap_policy'] === 'reject') return hrm_payroll_rate_table_fail($error, 'rate_bracket_gap');
        if ($cmp === 0) {
            if ($prev['upper_inclusive'] && $cur['lower_inclusive']) return hrm_payroll_rate_table_fail($error, 'rate_bracket_boundary_overlap');
            if (!$prev['upper_inclusive'] && !$cur['lower_inclusive'] && $artifact['gap_policy'] === 'reject') return hrm_payroll_rate_table_fail($error, 'rate_bracket_boundary_gap');
        }
    }

    if (!hrm_payroll_decision_table_is_list($artifact['test_vectors']) || count($artifact['test_vectors']) < 1 || count($artifact['test_vectors']) > 5000) return hrm_payroll_rate_table_fail($error, 'rate_test_vectors_invalid');
    $vector_ids = array(); $coverage = array();
    foreach ($artifact['test_vectors'] as $vector) {
        if (!hrm_payroll_decision_table_exact_keys($vector, array('vector_id','input','boundary','relation','expected_bracket_id'), array(), $error, 'rate_test_vector')) return false;
        if (!hrm_payroll_decision_table_valid_id($vector['vector_id']) || isset($vector_ids[$vector['vector_id']])) return hrm_payroll_rate_table_fail($error, 'rate_test_vector_id_invalid');
        $vector_ids[$vector['vector_id']] = true;
        if (!hrm_payroll_rate_table_decimal($vector['input']) || !hrm_payroll_rate_table_decimal($vector['boundary'])) return hrm_payroll_rate_table_fail($error, 'rate_test_vector_decimal_invalid');
        if (!isset($boundaries[$vector['boundary']])) return hrm_payroll_rate_table_fail($error, 'rate_test_vector_boundary_unknown');
        if (!in_array($vector['relation'], array('below','exact','above'), true)) return hrm_payroll_rate_table_fail($error, 'rate_test_vector_relation_invalid');
        $cmp = hrm_payroll_decision_table_decimal_compare($vector['input'], $vector['boundary']);
        if (($vector['relation']==='below' && $cmp>=0) || ($vector['relation']==='exact' && $cmp!==0) || ($vector['relation']==='above' && $cmp<=0)) return hrm_payroll_rate_table_fail($error, 'rate_test_vector_relation_mismatch');
        if ($vector['expected_bracket_id'] !== null && !isset($ids[$vector['expected_bracket_id']])) return hrm_payroll_rate_table_fail($error, 'rate_test_vector_expected_unknown');
        $coverage[$vector['boundary'].'|'.$vector['relation']] = true;
    }
    foreach (array_keys($boundaries) as $boundary)
        foreach (array('below','exact','above') as $relation)
            if (!isset($coverage[$boundary.'|'.$relation])) return hrm_payroll_rate_table_fail($error, 'rate_test_vector_boundary_coverage_missing');
    return true;
}

function hrm_payroll_rate_table_hash($artifact, &$error)
{
    if (!hrm_payroll_rate_table_validate($artifact, $error)) return false;
    return hash('sha256', json_encode(hrm_payroll_decision_table_canonicalize($artifact), JSON_UNESCAPED_SLASHES));
}
