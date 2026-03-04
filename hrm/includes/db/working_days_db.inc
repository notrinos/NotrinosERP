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
 * Get one working day rule by weekday index.
 *
 * @param int $day_of_week
 * @return array|false
 */
function get_working_day($day_of_week) {
    $sql = "SELECT * FROM ".TB_PREF."working_days WHERE day_of_week = ".db_escape((int)$day_of_week);
    $result = db_query($sql, 'could not get working day');

    return db_fetch($result);
}

/**
 * Get all working day rules.
 *
 * @return resource
 */
function get_working_days() {
    $sql = "SELECT * FROM ".TB_PREF."working_days ORDER BY day_of_week";
    return db_query($sql, 'could not get working days');
}

/**
 * Save one weekday rule.
 *
 * @param int $day_of_week
 * @param int $is_working
 * @param float $work_hours
 * @return void
 */
function save_working_day($day_of_week, $is_working, $work_hours) {
    $current = get_working_day($day_of_week);
    if ($current) {
        $sql = "UPDATE ".TB_PREF."working_days SET
                is_working = ".db_escape((int)$is_working).",
                work_hours = ".db_escape((float)$work_hours)."
            WHERE day_of_week = ".db_escape((int)$day_of_week);
    } else {
        $sql = "INSERT INTO ".TB_PREF."working_days
            (day_of_week, is_working, work_hours)
            VALUES (
                ".db_escape((int)$day_of_week).",
                ".db_escape((int)$is_working).",
                ".db_escape((float)$work_hours)."
            )";
    }

    db_query($sql, 'could not save working day');
}

/**
 * Save all weekday rules in one call.
 *
 * @param array $rules Indexed by day_of_week.
 * @return void
 */
function save_working_days($rules) {
    foreach ($rules as $day => $data) {
        save_working_day(
            (int)$day,
            empty($data['is_working']) ? 0 : 1,
            isset($data['work_hours']) ? (float)$data['work_hours'] : 0
        );
    }
}

