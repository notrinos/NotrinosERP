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
 * Get attendance deduction rule by id.
 *
 * @param int $rule_id
 * @return array|false
 */
function get_attendance_deduction_rule($rule_id) {
    $sql = "SELECT * FROM ".TB_PREF."attendance_deduction_rules WHERE rule_id = ".db_escape((int)$rule_id);
    $result = db_query($sql, 'could not get attendance deduction rule');

    return db_fetch($result);
}

/**
 * Get all attendance deduction rules.
 *
 * @param bool $show_inactive
 * @param int|null $rule_type
 * @return resource
 */
function get_attendance_deduction_rules($show_inactive=false, $rule_type=null) {
    $where = array('1=1');

    if (!$show_inactive)
        $where[] = 'inactive = 0';

    if ($rule_type !== null && $rule_type !== '')
        $where[] = 'rule_type = '.db_escape((int)$rule_type);

    $sql = "SELECT * FROM ".TB_PREF."attendance_deduction_rules
        WHERE ".implode(' AND ', $where)."
        ORDER BY rule_type, from_value";

    return db_query($sql, 'could not get attendance deduction rules');
}

/**
 * Add attendance deduction rule.
 *
 * @param int $rule_type
 * @param float $from_value
 * @param float $to_value
 * @param float $deduction_rate
 * @param int|null $day_of_week
 * @param float $work_hours
 * @return int
 */
function add_attendance_deduction_rule($rule_type, $from_value, $to_value, $deduction_rate, $day_of_week=null, $work_hours=8) {
    $day_sql = ($day_of_week === null || $day_of_week === '' || (int)$day_of_week < 0) ? 'NULL' : db_escape((int)$day_of_week);

    $sql = "INSERT INTO ".TB_PREF."attendance_deduction_rules
        (rule_type, from_value, to_value, deduction_rate, day_of_week, work_hours, inactive)
        VALUES (
            ".db_escape((int)$rule_type).",
            ".db_escape((float)$from_value).",
            ".db_escape((float)$to_value).",
            ".db_escape((float)$deduction_rate).",
            $day_sql,
            ".db_escape((float)$work_hours).",
            0
        )";

    db_query($sql, 'could not add attendance deduction rule');
    return (int)db_insert_id();
}

/**
 * Update attendance deduction rule.
 *
 * @param int $rule_id
 * @param int $rule_type
 * @param float $from_value
 * @param float $to_value
 * @param float $deduction_rate
 * @param int|null $day_of_week
 * @param float $work_hours
 * @param int $inactive
 * @return void
 */
function update_attendance_deduction_rule($rule_id, $rule_type, $from_value, $to_value, $deduction_rate, $day_of_week=null, $work_hours=8, $inactive=0) {
    $day_sql = ($day_of_week === null || $day_of_week === '' || (int)$day_of_week < 0) ? 'NULL' : db_escape((int)$day_of_week);

    $sql = "UPDATE ".TB_PREF."attendance_deduction_rules SET
            rule_type = ".db_escape((int)$rule_type).",
            from_value = ".db_escape((float)$from_value).",
            to_value = ".db_escape((float)$to_value).",
            deduction_rate = ".db_escape((float)$deduction_rate).",
            day_of_week = $day_sql,
            work_hours = ".db_escape((float)$work_hours).",
            inactive = ".db_escape((int)$inactive)."
        WHERE rule_id = ".db_escape((int)$rule_id);

    db_query($sql, 'could not update attendance deduction rule');
}

/**
 * Delete attendance deduction rule.
 *
 * @param int $rule_id
 * @return void
 */
function delete_attendance_deduction_rule($rule_id) {
    $sql = "DELETE FROM ".TB_PREF."attendance_deduction_rules WHERE rule_id = ".db_escape((int)$rule_id);
    db_query($sql, 'could not delete attendance deduction rule');
}

