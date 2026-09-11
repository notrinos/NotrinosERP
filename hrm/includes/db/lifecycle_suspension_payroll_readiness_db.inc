<?php
/**
 * HRM-FND-005 Suspension payroll calculation readiness guard foundation.
 *
 * Read-only capability only. It does not define paid/unpaid Suspension,
 * proration, absence, leave, earnings, deductions, tax or accounting behavior.
 * No production payroll consumer is wired to this helper in the foundation.
 */

/** Canonical SQL date or false. */
function hrm_fnd_005_suspension_payroll_readiness_date($date)
{
    $date = trim((string)$date);
    if (preg_match('/^\d{4}-\d{2}-\d{2}$/D', $date) !== 1)
        return false;
    $parsed = DateTime::createFromFormat('!Y-m-d', $date);
    return $parsed && $parsed->format('Y-m-d') === $date ? $date : false;
}

/** Fail-closed result; deliberately not a payroll result or eligibility skip. */
function hrm_fnd_005_suspension_payroll_readiness_denied($employee_id, $from_date, $to_date, $decision)
{
    return array(
        'can_use_legacy_calculation' => false,
        'decision' => (string)$decision,
        'employee_id' => trim((string)$employee_id),
        'from_date' => trim((string)$from_date),
        'to_date' => trim((string)$to_date),
        'period_days' => 0,
        'suspended_days' => 0,
        'pay_treatment' => 'unconfigured',
        'scope' => 'temporary_suspension_payroll_readiness_only',
        'business_mutation' => false
    );
}

/** Build a bounded inclusive canonical date list. */
function hrm_fnd_005_suspension_payroll_readiness_dates($from_date, $to_date)
{
    $from_date = hrm_fnd_005_suspension_payroll_readiness_date($from_date);
    $to_date = hrm_fnd_005_suspension_payroll_readiness_date($to_date);
    if ($from_date === false || $to_date === false || strcmp($from_date, $to_date) > 0)
        return false;

    $start = DateTime::createFromFormat('!Y-m-d', $from_date);
    $end = DateTime::createFromFormat('!Y-m-d', $to_date);
    $days = (int)$start->diff($end)->format('%a') + 1;
    if ($days <= 0 || $days > 370)
        return false;

    $dates = array();
    $cursor = clone $start;
    for ($i = 0; $i < $days; $i++) {
        $dates[] = $cursor->format('Y-m-d');
        $cursor->modify('+1 day');
    }
    return $dates;
}

/** Decide readiness over already-resolved per-date service-state evidence. */
function hrm_fnd_005_suspension_payroll_readiness_decide($employee_id, $from_date, $to_date, $states)
{
    $denied = hrm_fnd_005_suspension_payroll_readiness_denied(
        $employee_id, $from_date, $to_date, 'deny_invalid_or_ambiguous_service_state'
    );
    $employee_id = trim((string)$employee_id);
    $dates = hrm_fnd_005_suspension_payroll_readiness_dates($from_date, $to_date);
    if ($employee_id === '' || strlen($employee_id) > 20 || $dates === false || !is_array($states)
        || count($states) !== count($dates))
        return $denied;

    $suspended = 0;
    foreach ($dates as $i => $date) {
        $state = isset($states[$i]) && is_array($states[$i]) ? $states[$i] : false;
        if (!$state || !isset($state['employee_id'], $state['as_of_date'], $state['service_state'])
            || trim((string)$state['employee_id']) !== $employee_id
            || (string)$state['as_of_date'] !== $date
            || !in_array((string)$state['service_state'], array('active', 'suspended'), true))
            return $denied;
        if ((string)$state['service_state'] === 'suspended')
            $suspended++;
    }

    if ($suspended > 0) {
        $denied['decision'] = 'deny_suspension_pay_treatment_unconfigured';
        $denied['period_days'] = count($dates);
        $denied['suspended_days'] = $suspended;
        return $denied;
    }

    return array(
        'can_use_legacy_calculation' => true,
        'decision' => 'allow_legacy_calculation_no_suspension_overlap',
        'employee_id' => $employee_id,
        'from_date' => $dates[0],
        'to_date' => $dates[count($dates)-1],
        'period_days' => count($dates),
        'suspended_days' => 0,
        'pay_treatment' => 'not_applicable_no_suspension_overlap',
        'scope' => 'temporary_suspension_payroll_readiness_only',
        'business_mutation' => false
    );
}

/**
 * Resolve one Employee/period using the accepted temporary service-state model.
 * Canonical SQL dates are required. This function performs reads only.
 */
function get_hrm_fnd_005_suspension_payroll_calculation_readiness($employee_id, $from_date, $to_date)
{
    global $path_to_root;
    $employee_id = trim((string)$employee_id);
    $dates = hrm_fnd_005_suspension_payroll_readiness_dates($from_date, $to_date);
    if ($employee_id === '' || strlen($employee_id) > 20 || $dates === false)
        return hrm_fnd_005_suspension_payroll_readiness_denied(
            $employee_id, $from_date, $to_date, 'deny_invalid_readiness_request'
        );

    if (!function_exists('get_hrm_lifecycle_employee_service_state_as_of'))
        include_once($path_to_root.'/hrm/includes/db/lifecycle_service_state_as_of_db.inc');
    if (!function_exists('get_hrm_lifecycle_employee_service_state_as_of'))
        return hrm_fnd_005_suspension_payroll_readiness_denied(
            $employee_id, $from_date, $to_date, 'deny_service_state_dependency_unavailable'
        );

    $states = array();
    foreach ($dates as $date) {
        $state = get_hrm_lifecycle_employee_service_state_as_of($employee_id, $date, false);
        if (!is_array($state))
            return hrm_fnd_005_suspension_payroll_readiness_denied(
                $employee_id, $from_date, $to_date, 'deny_invalid_or_ambiguous_service_state'
            );
        $states[] = $state;
    }
    return hrm_fnd_005_suspension_payroll_readiness_decide($employee_id, $from_date, $to_date, $states);
}
