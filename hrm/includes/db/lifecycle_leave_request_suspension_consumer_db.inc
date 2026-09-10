<?php
/**
 * HRM-FND-005 Leave Request terminal-approval Suspension consumer.
 *
 * Validation/guard composition only. The caller MUST already own the
 * transaction. This helper starts/commits/rolls back no transaction and
 * performs no business mutation.
 */

/** Normalize one strict SQL date or return false. */
function hrm_fnd_005_leave_request_approval_sql_date($value)
{
    $value = trim((string)$value);
    if (preg_match('/^\d{4}-\d{2}-\d{2}$/D', $value) !== 1)
        return false;
    $parsed = DateTime::createFromFormat('!Y-m-d', $value);
    return ($parsed && $parsed->format('Y-m-d') === $value) ? $value : false;
}

/**
 * Guard every inclusive request date while the approval executor owns its
 * transaction. Returns only allow/deny and exposes no internal guard reason.
 */
function hrm_fnd_005_guard_leave_request_approval_range_for_update($employee_id, $from_date, $to_date)
{
    global $path_to_root, $transaction_level;

    $employee_id = trim((string)$employee_id);
    $from_date = hrm_fnd_005_leave_request_approval_sql_date($from_date);
    $to_date = hrm_fnd_005_leave_request_approval_sql_date($to_date);
    if ($employee_id === '' || $from_date === false || $to_date === false
        || strcmp($from_date, $to_date) > 0 || (int)$transaction_level <= 0)
        return false;

    if (!function_exists('get_hrm_lifecycle_attendance_mutation_guard_for_update'))
        include_once($path_to_root.'/hrm/includes/db/lifecycle_attendance_mutation_guard_db.inc');
    if (!function_exists('get_hrm_lifecycle_attendance_mutation_guard_for_update'))
        return false;

    $day = DateTime::createFromFormat('!Y-m-d', $from_date);
    $end = DateTime::createFromFormat('!Y-m-d', $to_date);
    if (!$day || !$end)
        return false;

    while ($day <= $end) {
        $decision = get_hrm_lifecycle_attendance_mutation_guard_for_update(
            $employee_id,
            $day->format('Y-m-d'),
            'attendance_leave_write'
        );
        if (!is_array($decision) || !isset($decision['can_mutate']) || $decision['can_mutate'] !== true)
            return false;
        $day->modify('+1 day');
    }
    return true;
}
