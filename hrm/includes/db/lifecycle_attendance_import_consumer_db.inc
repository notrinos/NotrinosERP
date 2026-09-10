<?php
/** CSV attendance batch transaction owner after legacy CSV parsing/validation. */
if (!function_exists('get_hrm_lifecycle_attendance_mutation_guard_for_update'))
    include_once($path_to_root.'/hrm/includes/db/lifecycle_attendance_mutation_guard_db.inc');

/** Preserve valid-row import semantics with lifecycle serialization before reads. */
function hrm_fnd_005_execute_attendance_import_rows($rows)
{
    $result = array('imported'=>0, 'failed'=>0, 'batch_failed'=>false);
    $employee_ids = array();
    foreach ($rows as $row) $employee_ids[] = $row['employee_id'];
    $employee_ids = array_unique($employee_ids);
    sort($employee_ids, SORT_STRING);
    begin_transaction();
    try {
        // Lock every lifecycle serialization row before creating a read snapshot.
        foreach ($employee_ids as $employee_id) {
            if (!db_query('SELECT employee_number FROM '.TB_PREF.'employees WHERE employee_id='
                .db_escape($employee_id).' ORDER BY employee_number FOR UPDATE',
                'could not lock attendance import Employee projection'))
                throw new Exception('Attendance import Employee lock unavailable.');
        }
        foreach ($rows as $row) {
            $guard = get_hrm_lifecycle_attendance_mutation_guard_for_update(
                $row['employee_id'], $row['date'], 'attendance_write');
            if (!is_array($guard) || !isset($guard['can_mutate']) || $guard['can_mutate'] !== true) {
                $result['failed']++;
                continue;
            }
            $date = sql2date($row['date']);
            if ($row['regular_hours'] >= 0)
                write_attendance($row['employee_id'], 0, $row['regular_hours'], $row['rate'], $date);
            if ($row['overtime_hours'] > 0)
                write_attendance($row['employee_id'], $row['overtime_type_id'], $row['overtime_hours'], $row['rate'], $date);
            $result['imported']++;
        }
        commit_transaction();
        return $result;
    } catch (Exception $error) {
        cancel_transaction();
    } catch (Throwable $error) {
        cancel_transaction();
    }
    return array('imported'=>0, 'failed'=>count($rows), 'batch_failed'=>true);
}
