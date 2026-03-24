<?php
/**********************************************************************
    Copyright (C) NotrinosERP.
    Released under the terms of the GNU General Public License, GPL,
    as published by the Free Software Foundation, either version 3
    of the License, or (at your option) any later version.
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
    See the License here <http://www.gnu.org/licenses/gpl-3.0.html>.
***********************************************************************/

/**
 * Get one work shift.
 *
 * @param int $shift_id
 * @return array|false
 */
function get_work_shift($shift_id) {
    $sql = "SELECT * FROM ".TB_PREF."work_shifts WHERE shift_id = ".db_escape((int)$shift_id);
    $result = db_query($sql, 'could not get work shift');

    return db_fetch($result);
}

/**
 * Get work shifts list.
 *
 * @param bool $show_inactive
 * @return resource
 */
function get_work_shifts($show_inactive=false) {
    $sql = "SELECT * FROM ".TB_PREF."work_shifts";
    if (!$show_inactive)
        $sql .= " WHERE inactive = 0";
    $sql .= " ORDER BY shift_name";

    return db_query($sql, 'could not get work shifts');
}

/**
 * Add work shift.
 *
 * @param string $shift_name
 * @param string $start_time
 * @param string $end_time
 * @param int $break_duration
 * @param float $work_hours
 * @param int $is_night_shift
 * @return int
 */
function add_work_shift($shift_name, $start_time, $end_time, $break_duration=60, $work_hours=8, $is_night_shift=0) {
    $sql = "INSERT INTO ".TB_PREF."work_shifts
        (shift_name, start_time, end_time, break_duration, work_hours, is_night_shift, inactive)
        VALUES (
            ".db_escape($shift_name).",
            ".db_escape($start_time).",
            ".db_escape($end_time).",
            ".db_escape((int)$break_duration).",
            ".db_escape((float)$work_hours).",
            ".db_escape((int)$is_night_shift).",
            0
        )";

    db_query($sql, 'could not add work shift');
    return (int)db_insert_id();
}

/**
 * Update work shift.
 *
 * @param int $shift_id
 * @param string $shift_name
 * @param string $start_time
 * @param string $end_time
 * @param int $break_duration
 * @param float $work_hours
 * @param int $is_night_shift
 * @param int $inactive
 * @return void
 */
function update_work_shift($shift_id, $shift_name, $start_time, $end_time, $break_duration=60, $work_hours=8, $is_night_shift=0, $inactive=0) {
    $sql = "UPDATE ".TB_PREF."work_shifts SET
            shift_name = ".db_escape($shift_name).",
            start_time = ".db_escape($start_time).",
            end_time = ".db_escape($end_time).",
            break_duration = ".db_escape((int)$break_duration).",
            work_hours = ".db_escape((float)$work_hours).",
            is_night_shift = ".db_escape((int)$is_night_shift).",
            inactive = ".db_escape((int)$inactive)."
        WHERE shift_id = ".db_escape((int)$shift_id);

    db_query($sql, 'could not update work shift');
}

/**
 * Delete work shift.
 *
 * @param int $shift_id
 * @return void
 */
function delete_work_shift($shift_id) {
    $sql = "DELETE FROM ".TB_PREF."work_shifts WHERE shift_id = ".db_escape((int)$shift_id);
    db_query($sql, 'could not delete work shift');
}

