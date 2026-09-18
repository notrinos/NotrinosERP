<?php
/**
 * Pure PAY-CORE-002 legacy payroll-period mapping review.
 *
 * This contract is intentionally storage-free and non-authoritative. It never
 * infers legal entity, currency, payroll group, calendar frequency, cutoff,
 * post or payment-file dates from legacy payroll_periods data.
 */
include_once(dirname(__FILE__).'/pay_core_002_contract.inc');

if (!defined('HRM_PAY_CORE_002_LEGACY_MAPPING_REVIEW_VERSION'))
    define('HRM_PAY_CORE_002_LEGACY_MAPPING_REVIEW_VERSION', '1.0');
if (!defined('HRM_PAY_CORE_002_LEGACY_MAPPING_AUTOMATIC_CONVERSION_ALLOWED'))
    define('HRM_PAY_CORE_002_LEGACY_MAPPING_AUTOMATIC_CONVERSION_ALLOWED', false);

function hrm_pay_core_002_legacy_period_status_labels()
{
    return array(
        0 => 'draft',
        1 => 'calculated',
        2 => 'approved',
        3 => 'posted',
        4 => 'paid',
        5 => 'closed',
        6 => 'voided'
    );
}

function hrm_pay_core_002_legacy_period_required_fields()
{
    return array(
        'period_id','period_name','from_date','to_date','pay_date','department_id',
        'status','total_gross','total_deductions','total_net','total_employer_cost',
        'approved_by','approval_date','gl_trans_no','created_by','created_date','notes'
    );
}

function hrm_pay_core_002_review_legacy_period_mapping($row)
{
    if (!is_array($row))
        return array('disposition'=>'invalid_source','ambiguity_codes'=>array('source_not_array'));

    $missing = array();
    foreach (hrm_pay_core_002_legacy_period_required_fields() as $field)
        if (!array_key_exists($field, $row)) $missing[] = $field;
    if ($missing)
        return array('disposition'=>'invalid_source','ambiguity_codes'=>array('missing_fields'),'missing_fields'=>$missing);

    $status = filter_var($row['status'], FILTER_VALIDATE_INT);
    $period_id = filter_var($row['period_id'], FILTER_VALIDATE_INT);
    $valid_from = hrm_pay_core_002_validate_iso_date((string)$row['from_date']);
    $valid_to = hrm_pay_core_002_validate_iso_date((string)$row['to_date']);
    $labels = hrm_pay_core_002_legacy_period_status_labels();

    $invalid = array();
    if ($period_id === false || $period_id <= 0) $invalid[] = 'invalid_period_id';
    if (trim((string)$row['period_name']) === '') $invalid[] = 'blank_period_name';
    if (!$valid_from || !$valid_to || ($valid_from && $valid_to && (string)$row['from_date'] > (string)$row['to_date'])) $invalid[] = 'invalid_period_dates';
    if ($status === false || !array_key_exists((int)$status, $labels)) $invalid[] = 'invalid_status';
    if ($invalid)
        return array('disposition'=>'invalid_source','legacy_period_id'=>(int)$row['period_id'],'ambiguity_codes'=>$invalid);

    $ambiguity = array(
        'legal_entity_unavailable',
        'currency_unavailable',
        'calendar_frequency_unverified',
        'cutoff_date_unavailable',
        'post_date_unavailable',
        'file_date_unavailable'
    );
    if ($row['department_id'] !== null && $row['department_id'] !== '' && (int)$row['department_id'] > 0)
        $ambiguity[] = 'department_scope_requires_explicit_group_review';
    else
        $ambiguity[] = 'all_departments_scope_requires_explicit_group_review';
    if ((int)$status >= 2 && trim((string)$row['approved_by']) === '')
        $ambiguity[] = 'approval_evidence_incomplete';
    if ((int)$status >= 3 && ($row['gl_trans_no'] === null || (int)$row['gl_trans_no'] <= 0))
        $ambiguity[] = 'gl_link_evidence_incomplete';

    return array(
        'disposition'=>'manual_review',
        'legacy_period_id'=>(int)$period_id,
        'legacy_status'=>(int)$status,
        'legacy_status_label'=>$labels[(int)$status],
        'department_scoped'=>($row['department_id'] !== null && $row['department_id'] !== '' && (int)$row['department_id'] > 0),
        'automatic_conversion_allowed'=>false,
        'ambiguity_codes'=>$ambiguity
    );
}
