<?php
/**
 * Pure PAY-CORE-002 contract. This file owns no database or runtime payroll authority.
 */
if (!defined('HRM_PAY_CORE_002_CONTRACT_VERSION'))
    define('HRM_PAY_CORE_002_CONTRACT_VERSION', '1.0');
if (!defined('HRM_PAY_CORE_002_PRODUCTION_ACTIVATION_ALLOWED'))
    define('HRM_PAY_CORE_002_PRODUCTION_ACTIVATION_ALLOWED', false);

function hrm_pay_core_002_allowed_frequencies()
{
    return array('weekly','biweekly','semimonthly','monthly','four_weekly','custom');
}

function hrm_pay_core_002_allowed_run_types()
{
    return array('regular','offcycle','supplemental','correction','reversal');
}

function hrm_pay_core_002_validate_iso_date($value)
{
    if (!is_string($value) || !preg_match('/^\\d{4}-\\d{2}-\\d{2}$/D', $value))
        return false;
    $dt = DateTimeImmutable::createFromFormat('!Y-m-d', $value);
    return $dt !== false && $dt->format('Y-m-d') === $value;
}

function hrm_pay_core_002_validate_period_contract($period)
{
    if (!is_array($period))
        return false;
    foreach (array('period_start','period_end','cutoff_date','pay_date','post_date','file_date','run_type') as $key)
        if (!array_key_exists($key, $period)) return false;
    foreach (array('period_start','period_end','cutoff_date','pay_date','post_date','file_date') as $key)
        if (!hrm_pay_core_002_validate_iso_date($period[$key])) return false;
    if (!in_array($period['run_type'], hrm_pay_core_002_allowed_run_types(), true)) return false;
    return $period['period_start'] <= $period['period_end']
        && $period['cutoff_date'] <= $period['pay_date']
        && $period['post_date'] <= $period['pay_date']
        && $period['file_date'] <= $period['pay_date'];
}

function hrm_pay_core_002_validate_group_contract($group)
{
    if (!is_array($group) || empty($group['group_code']) || empty($group['frequency']))
        return false;
    return in_array($group['frequency'], hrm_pay_core_002_allowed_frequencies(), true);
}
