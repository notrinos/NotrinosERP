<?php
/**
 * Pure PAY-CORE-003 contract for versioned elements, input definitions,
 * eligibility, and approved payroll-input provenance.
 *
 * This file deliberately owns no database or production payroll authority.
 */
if (!defined('HRM_PAY_CORE_003_CONTRACT_VERSION'))
    define('HRM_PAY_CORE_003_CONTRACT_VERSION', '1.0');
if (!defined('HRM_PAY_CORE_003_PRODUCTION_ACTIVATION_ALLOWED'))
    define('HRM_PAY_CORE_003_PRODUCTION_ACTIVATION_ALLOWED', false);
if (!defined('HRM_PAY_CORE_003_AUTOMATIC_LEGACY_MIGRATION_ALLOWED'))
    define('HRM_PAY_CORE_003_AUTOMATIC_LEGACY_MIGRATION_ALLOWED', false);

function hrm_pay_core_003_element_types()
{
    return array('earning','deduction','employer_contribution','information');
}

function hrm_pay_core_003_calculation_modes()
{
    return array('fixed','input','rule','derived');
}

function hrm_pay_core_003_input_value_types()
{
    return array('decimal','date','code','boolean');
}

function hrm_pay_core_003_is_sha256($value)
{
    return is_string($value) && preg_match('/^[a-f0-9]{64}$/D', $value) === 1;
}

function hrm_pay_core_003_is_iso_date($value)
{
    if (!is_string($value) || preg_match('/^\\d{4}-\\d{2}-\\d{2}$/D', $value) !== 1)
        return false;
    $dt = DateTimeImmutable::createFromFormat('!Y-m-d', $value);
    return $dt !== false && $dt->format('Y-m-d') === $value;
}

function hrm_pay_core_003_valid_effective_range($from, $to=null)
{
    if (!hrm_pay_core_003_is_iso_date($from))
        return false;
    if ($to === null || $to === '')
        return true;
    return hrm_pay_core_003_is_iso_date($to) && $from <= $to;
}

function hrm_pay_core_003_validate_element_version($row)
{
    if (!is_array($row))
        return false;
    foreach (array('element_code','version_no','effective_from','element_type','calculation_mode','version_sha256') as $key)
        if (!array_key_exists($key, $row)) return false;
    if (!preg_match('/^[A-Z0-9][A-Z0-9_.-]{1,63}$/D', (string)$row['element_code']))
        return false;
    if ((int)$row['version_no'] < 1)
        return false;
    if (!in_array($row['element_type'], hrm_pay_core_003_element_types(), true)
        || !in_array($row['calculation_mode'], hrm_pay_core_003_calculation_modes(), true))
        return false;
    if (!hrm_pay_core_003_valid_effective_range($row['effective_from'], isset($row['effective_to']) ? $row['effective_to'] : null)
        || !hrm_pay_core_003_is_sha256($row['version_sha256']))
        return false;
    if ($row['calculation_mode'] === 'rule')
        return isset($row['rule_artifact_sha256']) && hrm_pay_core_003_is_sha256($row['rule_artifact_sha256']);
    return !isset($row['rule_artifact_sha256']) || $row['rule_artifact_sha256'] === null || $row['rule_artifact_sha256'] === '' || hrm_pay_core_003_is_sha256($row['rule_artifact_sha256']);
}

function hrm_pay_core_003_validate_input_definition($row)
{
    if (!is_array($row))
        return false;
    foreach (array('input_code','value_type','unit_token','required_flag','definition_sha256') as $key)
        if (!array_key_exists($key, $row)) return false;
    if (!preg_match('/^[A-Z0-9][A-Z0-9_.-]{1,63}$/D', (string)$row['input_code']))
        return false;
    if (!in_array($row['value_type'], hrm_pay_core_003_input_value_types(), true))
        return false;
    if (!is_string($row['unit_token']) || trim($row['unit_token']) === '')
        return false;
    if (!in_array((int)$row['required_flag'], array(0,1), true))
        return false;
    return hrm_pay_core_003_is_sha256($row['definition_sha256']);
}

/**
 * Select the single explicit highest-priority candidate.
 * Missing ranks and tied top ranks fail closed; row order is never precedence.
 */
function hrm_pay_core_003_resolve_explicit_precedence($candidates)
{
    if (!is_array($candidates) || count($candidates) === 0)
        return false;
    $winner = false;
    $best = null;
    $ties = 0;
    foreach ($candidates as $candidate) {
        if (!is_array($candidate) || !array_key_exists('precedence_rank', $candidate)
            || !is_int($candidate['precedence_rank']) || $candidate['precedence_rank'] < 0)
            return false;
        if ($best === null || $candidate['precedence_rank'] < $best) {
            $best = $candidate['precedence_rank'];
            $winner = $candidate;
            $ties = 1;
        } elseif ($candidate['precedence_rank'] === $best) {
            $ties++;
        }
    }
    return $ties === 1 ? $winner : false;
}
