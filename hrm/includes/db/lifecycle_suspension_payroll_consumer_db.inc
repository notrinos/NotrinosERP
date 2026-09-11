<?php
/**
 * HRM-FND-005 Suspension payroll calculation consumer guard.
 *
 * This helper adds only transaction/locking custody around the accepted
 * read-only readiness policy. It never calculates payroll amounts and never
 * writes payroll, lifecycle, leave, attendance or accounting business rows.
 */

/** Build a fail-closed result without exposing payroll values. */
function hrm_fnd_005_suspension_payroll_consumer_denied($employee_id, $from_date, $to_date, $decision)
{
    if (function_exists('hrm_fnd_005_suspension_payroll_readiness_denied'))
        return hrm_fnd_005_suspension_payroll_readiness_denied($employee_id, $from_date, $to_date, $decision);

    return array(
        'can_use_legacy_calculation' => false,
        'decision' => (string)$decision,
        'employee_id' => trim((string)$employee_id),
        'from_date' => trim((string)$from_date),
        'to_date' => trim((string)$to_date),
        'period_days' => 0,
        'suspended_days' => 0,
        'pay_treatment' => 'unconfigured',
        'scope' => 'temporary_suspension_payroll_consumer_guard',
        'business_mutation' => false
    );
}

/**
 * Lock the Employee serialization row and resolve the accepted lifecycle state
 * for every date while the caller's transaction remains open.
 */
function get_hrm_fnd_005_suspension_payroll_calculation_readiness_for_update($employee_id, $from_date, $to_date)
{
    global $path_to_root, $transaction_level;

    $employee_id = trim((string)$employee_id);
    if (empty($transaction_level))
        return hrm_fnd_005_suspension_payroll_consumer_denied(
            $employee_id, $from_date, $to_date, 'deny_atomic_guard_transaction_required'
        );

    if (!function_exists('hrm_fnd_005_suspension_payroll_readiness_dates'))
        include_once($path_to_root.'/hrm/includes/db/lifecycle_suspension_payroll_readiness_db.inc');
    if (!function_exists('get_hrm_lifecycle_employee_service_state_as_of'))
        include_once($path_to_root.'/hrm/includes/db/lifecycle_service_state_as_of_db.inc');
    if (!function_exists('hrm_fnd_005_suspension_payroll_readiness_dates')
        || !function_exists('hrm_fnd_005_suspension_payroll_readiness_decide')
        || !function_exists('get_hrm_lifecycle_employee_service_state_as_of'))
        return hrm_fnd_005_suspension_payroll_consumer_denied(
            $employee_id, $from_date, $to_date, 'deny_service_state_dependency_unavailable'
        );

    $dates = hrm_fnd_005_suspension_payroll_readiness_dates($from_date, $to_date);
    if ($employee_id === '' || strlen($employee_id) > 20 || $dates === false)
        return hrm_fnd_005_suspension_payroll_consumer_denied(
            $employee_id, $from_date, $to_date, 'deny_invalid_readiness_request'
        );

    $employee_sql = "SELECT employee_number,employee_id FROM ".TB_PREF."employees "
        ."WHERE employee_id=".db_escape($employee_id)." LIMIT 2 FOR UPDATE";
    $employee_result = db_query($employee_sql, 'could not lock Suspension payroll Employee projection');
    if (!$employee_result || (int)db_num_rows($employee_result) !== 1)
        return hrm_fnd_005_suspension_payroll_consumer_denied(
            $employee_id, $from_date, $to_date, 'deny_employee_projection_unavailable'
        );
    $locked_employee = db_fetch_assoc($employee_result);
    if (!$locked_employee || trim((string)$locked_employee['employee_id']) !== $employee_id
        || (int)$locked_employee['employee_number'] <= 0)
        return hrm_fnd_005_suspension_payroll_consumer_denied(
            $employee_id, $from_date, $to_date, 'deny_employee_projection_unavailable'
        );

    $states = array();
    foreach ($dates as $date) {
        $state = get_hrm_lifecycle_employee_service_state_as_of($employee_id, $date, true);
        if (!is_array($state))
            return hrm_fnd_005_suspension_payroll_consumer_denied(
                $employee_id, $from_date, $to_date, 'deny_invalid_or_ambiguous_service_state'
            );
        $states[] = $state;
    }

    return hrm_fnd_005_suspension_payroll_readiness_decide(
        $employee_id, $from_date, $to_date, $states
    );
}
