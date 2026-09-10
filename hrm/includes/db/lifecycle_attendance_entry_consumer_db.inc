<?php
/**
 * HRM-FND-005 Attendance Entry Suspension mutation consumer.
 *
 * This helper owns exactly one Employee/date transaction. It intentionally
 * reuses the existing attendance/leave/meta writers after the accepted
 * lifecycle/payroll guard allows mutation. Other attendance consumers do not
 * call this helper.
 */

function hrm_fnd_005_attendance_entry_mutation_kind($regular, $ot_hours, $ot_type, $leave_id)
{
    if ((int)$leave_id > 0)
        return 'attendance_leave_write';
    if (trim((string)$regular) !== ''
        || (trim((string)$ot_hours) !== '' && (int)$ot_type > 0))
        return 'attendance_write';
    return 'attendance_meta_write';
}

/**
 * Apply one already-validated Attendance Entry Employee/date unit.
 *
 * @return array saved(bool), decision(string), mutation_kind(string)
 */
function hrm_fnd_005_execute_attendance_entry_day($employee_id, $entry_date,
    $regular, $ot_hours, $ot_type, $leave_id, $status, $shift_id,
    $clock_in, $clock_out, $notes)
{
    $kind = hrm_fnd_005_attendance_entry_mutation_kind(
        $regular, $ot_hours, $ot_type, $leave_id
    );

    if (!function_exists('get_hrm_lifecycle_attendance_mutation_guard_for_update')
        || !function_exists('write_attendance')
        || !function_exists('hrm_upsert_attendance_meta')
        || !function_exists('begin_transaction')
        || !function_exists('commit_transaction')
        || !function_exists('cancel_transaction')) {
        return array(
            'saved' => false,
            'decision' => 'deny_consumer_dependency_unavailable',
            'mutation_kind' => $kind
        );
    }

    $sql_date = date2sql($entry_date);
    if (preg_match('/^\d{4}-\d{2}-\d{2}$/D', (string)$sql_date) !== 1) {
        return array(
            'saved' => false,
            'decision' => 'deny_invalid_consumer_date',
            'mutation_kind' => $kind
        );
    }

    begin_transaction();
    $guard = get_hrm_lifecycle_attendance_mutation_guard_for_update(
        $employee_id, $sql_date, $kind
    );
    if (!is_array($guard) || empty($guard['can_mutate'])) {
        cancel_transaction();
        return array(
            'saved' => false,
            'decision' => (is_array($guard) && isset($guard['decision']))
                ? (string)$guard['decision'] : 'deny_invalid_guard_result',
            'mutation_kind' => $kind
        );
    }

    if ((int)$leave_id > 0) {
        write_attendance($employee_id, 0, 0, 1, $entry_date, (int)$leave_id);
        hrm_upsert_attendance_meta(
            $employee_id, $entry_date, 3, (int)$shift_id,
            $clock_in, $clock_out, $notes
        );
    } else {
        if (trim((string)$regular) !== '')
            write_attendance($employee_id, 0, time_to_float($regular), 1, $entry_date);

        if (trim((string)$ot_hours) !== '' && (int)$ot_type > 0) {
            $overtime = function_exists('get_overtime') ? get_overtime((int)$ot_type) : false;
            $ot_rate = $overtime ? $overtime['pay_rate'] : 1;
            write_attendance(
                $employee_id, (int)$ot_type, time_to_float($ot_hours),
                $ot_rate, $entry_date
            );
        }

        hrm_upsert_attendance_meta(
            $employee_id, $entry_date, (int)$status, (int)$shift_id,
            $clock_in, $clock_out, $notes
        );
    }

    commit_transaction();
    return array(
        'saved' => true,
        'decision' => 'saved_after_guard',
        'mutation_kind' => $kind
    );
}
