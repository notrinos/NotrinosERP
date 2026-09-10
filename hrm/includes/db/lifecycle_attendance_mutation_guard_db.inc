<?php
/**
 * HRM-FND-005 Suspension attendance mutation DB-aware guard foundation.
 *
 * This helper is SELECT/lock-only. The caller MUST own a database transaction
 * before invoking get_hrm_lifecycle_attendance_mutation_guard_for_update().
 * It never starts/commits/rolls back a transaction and never mutates business
 * state. No production caller is wired to this foundation yet.
 */

/** Fail-closed guard result usable even when dependencies/DB reads fail. */
function hrm_lifecycle_attendance_mutation_guard_denied($employee_id, $mutation_date, $mutation_kind, $reason)
{
    return array(
        'can_mutate' => false,
        'decision' => (string)$reason,
        'employee_id' => trim((string)$employee_id),
        'mutation_date' => trim((string)$mutation_date),
        'mutation_kind' => trim((string)$mutation_kind),
        'existing_evidence' => false,
        'existing_evidence_disposition' => 'none',
        'correction_policy' => 'separate_review_required',
        'scope' => 'caller_transaction_lock_and_policy_only',
        'employee_lock_acquired' => false,
        'caller_transaction_required' => true
    );
}

/** Compose already-locked evidence through the accepted pure evaluator. */
function hrm_lifecycle_attendance_mutation_guard_decide_locked_evidence($employee_id, $mutation_date,
    $mutation_kind, $employee_lock_acquired, $service_state, $has_existing_evidence, $payroll_locked)
{
    $denied = hrm_lifecycle_attendance_mutation_guard_denied(
        $employee_id, $mutation_date, $mutation_kind, 'deny_invalid_or_ambiguous_guard_evidence'
    );
    if ($employee_lock_acquired !== true || !function_exists('hrm_lifecycle_attendance_mutation_policy_decide'))
        return $denied;

    $decision = hrm_lifecycle_attendance_mutation_policy_decide(
        $service_state, $mutation_date, $mutation_kind, (bool)$has_existing_evidence, (bool)$payroll_locked
    );
    if (!is_array($decision) || !array_key_exists('can_mutate', $decision) || !isset($decision['decision']))
        return $denied;

    $decision['employee_id'] = trim((string)$employee_id);
    $decision['scope'] = 'caller_transaction_lock_and_policy_only';
    $decision['employee_lock_acquired'] = true;
    $decision['caller_transaction_required'] = true;
    return $decision;
}

/**
 * Lock Employee/lifecycle/evidence custody and return one fail-closed policy decision.
 * Caller transaction ownership is mandatory by contract and is intentionally not
 * inferred from framework internals here.
 */
function get_hrm_lifecycle_attendance_mutation_guard_for_update($employee_id, $mutation_date, $mutation_kind)
{
    global $path_to_root;
    $employee_id = trim((string)$employee_id);
    $mutation_date = trim((string)$mutation_date);
    $mutation_kind = trim((string)$mutation_kind);
    $supported = array(
        'attendance_write', 'attendance_meta_write', 'attendance_delete',
        'attendance_leave_write', 'attendance_bulk_fill'
    );

    if ($employee_id === '' || strlen($employee_id) > 20
        || preg_match('/^\d{4}-\d{2}-\d{2}$/D', $mutation_date) !== 1
        || !in_array($mutation_kind, $supported, true))
        return hrm_lifecycle_attendance_mutation_guard_denied(
            $employee_id, $mutation_date, $mutation_kind, 'deny_invalid_guard_request'
        );
    $parsed = DateTime::createFromFormat('!Y-m-d', $mutation_date);
    if (!$parsed || $parsed->format('Y-m-d') !== $mutation_date)
        return hrm_lifecycle_attendance_mutation_guard_denied(
            $employee_id, $mutation_date, $mutation_kind, 'deny_invalid_guard_request'
        );

    if (!function_exists('hrm_lifecycle_attendance_mutation_policy_decide'))
        include_once($path_to_root.'/hrm/includes/db/lifecycle_attendance_mutation_policy_db.inc');
    if (!function_exists('get_hrm_lifecycle_employee_service_state_as_of'))
        include_once($path_to_root.'/hrm/includes/db/lifecycle_service_state_as_of_db.inc');
    if (!function_exists('hrm_lifecycle_attendance_mutation_policy_decide')
        || !function_exists('get_hrm_lifecycle_employee_service_state_as_of'))
        return hrm_lifecycle_attendance_mutation_guard_denied(
            $employee_id, $mutation_date, $mutation_kind, 'deny_guard_dependency_unavailable'
        );

    $employee_sql = "SELECT employee_number,employee_id,inactive,released_date FROM ".TB_PREF."employees "
        ."WHERE employee_id=".db_escape($employee_id)." LIMIT 2 FOR UPDATE";
    $employee_result = db_query($employee_sql, 'could not lock Suspension attendance Employee projection');
    if (!$employee_result || (int)db_num_rows($employee_result) !== 1)
        return hrm_lifecycle_attendance_mutation_guard_denied(
            $employee_id, $mutation_date, $mutation_kind, 'deny_employee_projection_unavailable'
        );
    $employee = db_fetch_assoc($employee_result);
    if (!$employee || (int)$employee['employee_number'] <= 0 || (int)$employee['inactive'] !== 0
        || ($employee['released_date'] !== null && trim((string)$employee['released_date']) !== ''))
        return hrm_lifecycle_attendance_mutation_guard_denied(
            $employee_id, $mutation_date, $mutation_kind, 'deny_employee_projection_inactive_or_released'
        );

    $service_state = get_hrm_lifecycle_employee_service_state_as_of($employee_id, $mutation_date);
    if (!is_array($service_state))
        return hrm_lifecycle_attendance_mutation_guard_denied(
            $employee_id, $mutation_date, $mutation_kind, 'deny_invalid_or_ambiguous_service_state'
        );

    $attendance_sql = "SELECT attendance_id FROM ".TB_PREF."attendance WHERE employee_id="
        .db_escape($employee_id)." AND date=".db_escape($mutation_date)." ORDER BY attendance_id FOR UPDATE";
    $attendance_result = db_query($attendance_sql, 'could not lock Suspension attendance evidence');
    if (!$attendance_result)
        return hrm_lifecycle_attendance_mutation_guard_denied(
            $employee_id, $mutation_date, $mutation_kind, 'deny_attendance_evidence_read_failure'
        );
    $has_attendance = (int)db_num_rows($attendance_result) > 0;

    $leave_sql = "SELECT id FROM ".TB_PREF."leave_details WHERE employee_id=".db_escape($employee_id)
        ." AND ".db_escape($mutation_date)." BETWEEN from_date AND to_date"
        ." AND approval_status<>'rejected' ORDER BY id FOR UPDATE";
    $leave_result = db_query($leave_sql, 'could not lock Suspension attendance leave evidence');
    if (!$leave_result)
        return hrm_lifecycle_attendance_mutation_guard_denied(
            $employee_id, $mutation_date, $mutation_kind, 'deny_leave_evidence_read_failure'
        );
    $has_leave = (int)db_num_rows($leave_result) > 0;

    $payslip_sql = "SELECT payslip_id FROM ".TB_PREF."payslips WHERE employee_id=".db_escape($employee_id)
        ." AND ".db_escape($mutation_date)." BETWEEN from_date AND to_date ORDER BY payslip_id FOR UPDATE";
    $payslip_result = db_query($payslip_sql, 'could not lock Suspension attendance payroll evidence');
    if (!$payslip_result)
        return hrm_lifecycle_attendance_mutation_guard_denied(
            $employee_id, $mutation_date, $mutation_kind, 'deny_payroll_evidence_read_failure'
        );
    $payroll_locked = (int)db_num_rows($payslip_result) > 0;

    return hrm_lifecycle_attendance_mutation_guard_decide_locked_evidence(
        $employee_id, $mutation_date, $mutation_kind, true, $service_state,
        ($has_attendance || $has_leave), $payroll_locked
    );
}
