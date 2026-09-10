<?php
/** Attendance Sheet transaction owner; route validation remains authoritative. */
if (!function_exists('get_hrm_lifecycle_attendance_mutation_guard_for_update'))
    include_once($path_to_root.'/hrm/includes/db/lifecycle_attendance_mutation_guard_db.inc');

/** Apply one validated Sheet cell, deletion or working-day fill atomically. */
function hrm_fnd_005_execute_attendance_sheet_day($employee_id, $sql_date, $operation,
    $regular='', $ot_hours='', $ot_type=0, $leave_id=0, $status=0,
    $shift_id=0, $clock_in='', $clock_out='', $notes='')
{
    $denied = array('saved'=>false, 'skipped'=>false);
    if (!in_array($operation, array('save', 'delete', 'bulk_delete', 'fill'), true)
        || preg_match('/^\d{4}-\d{2}-\d{2}$/D', (string)$sql_date) !== 1)
        return $denied;

    $kinds = array();
    if ($operation === 'delete' || $operation === 'bulk_delete') {
        $kinds[] = 'attendance_delete';
    } else {
        if ($operation === 'fill') $kinds[] = 'attendance_bulk_fill';
        if ((int)$leave_id > 0) $kinds[] = 'attendance_leave_write';
        elseif ($regular !== '' || ($ot_hours !== '' && (int)$ot_type > 0))
            $kinds[] = 'attendance_write';
        $kinds[] = 'attendance_meta_write';
    }

    begin_transaction();
    try {
        foreach ($kinds as $kind) {
            $guard = get_hrm_lifecycle_attendance_mutation_guard_for_update($employee_id, $sql_date, $kind);
            if (!is_array($guard) || !isset($guard['can_mutate']) || $guard['can_mutate'] !== true) {
                cancel_transaction();
                return $denied;
            }
        }
        $date = sql2date($sql_date);
        $leave = hrm_get_leave_coverage_flags($employee_id, $sql_date);
        if (($operation === 'save' || $operation === 'delete') && $leave['has_multi_day']) {
            cancel_transaction();
            return $denied;
        }
        if ($operation === 'fill' && ($leave['has_leave'] || get_attendance_entry($employee_id, $date))) {
            cancel_transaction();
            return array('saved'=>false, 'skipped'=>true);
        }
        if ($operation === 'delete' || $operation === 'bulk_delete') {
            delete_attendance_record($employee_id, $sql_date);
        } else {
            if ((int)$leave_id > 0) {
                write_attendance($employee_id, 0, 0, 1, $date, (int)$leave_id);
                $status = 3;
            } else {
                if ($leave['has_single_day']) {
                    $guard = get_hrm_lifecycle_attendance_mutation_guard_for_update($employee_id, $sql_date, 'attendance_delete');
                    if (!is_array($guard) || !isset($guard['can_mutate']) || $guard['can_mutate'] !== true) {
                        cancel_transaction();
                        return $denied;
                    }
                    hrm_delete_single_day_leave_records($employee_id, $sql_date);
                }
                if ($regular !== '') write_attendance($employee_id, 0, time_to_float($regular), 1, $date);
                if ($ot_hours !== '' && (int)$ot_type > 0) {
                    $overtime = get_overtime((int)$ot_type);
                    write_attendance($employee_id, (int)$ot_type, time_to_float($ot_hours), $overtime ? $overtime['pay_rate'] : 1, $date);
                }
            }
            hrm_upsert_attendance_meta($employee_id, $date, (int)$status, (int)$shift_id, $clock_in, $clock_out, $notes);
        }
        commit_transaction();
        return array('saved'=>true, 'skipped'=>false);
    } catch (Exception $error) {
        cancel_transaction();
        return $denied;
    } catch (Throwable $error) {
        cancel_transaction();
        return $denied;
    }
}
