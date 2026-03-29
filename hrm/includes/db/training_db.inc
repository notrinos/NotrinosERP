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
 * Training courses and employee training DB helpers.
 */

/**
 * Add training course.
 *
 * @param string $course_code
 * @param string $course_name
 * @param string $provider
 * @param float $default_hours
 * @param float $default_cost
 * @return int
 */
function add_training_course($course_code, $course_name, $provider='', $default_hours=0, $default_cost=0) {
    $sql = "INSERT INTO ".TB_PREF."training_courses (course_code, course_name, provider, default_hours, default_cost)
        VALUES (
            ".db_escape($course_code).",
            ".db_escape($course_name).",
            ".db_escape($provider).",
            ".db_escape((float)$default_hours).",
            ".db_escape((float)$default_cost)."
        )";
    db_query($sql, 'could not add training course');

    return (int)db_insert_id();
}

/**
 * Update training course.
 *
 * @param int $course_id
 * @param string $course_code
 * @param string $course_name
 * @param string $provider
 * @param float $default_hours
 * @param float $default_cost
 * @return bool
 */
function update_training_course($course_id, $course_code, $course_name, $provider='', $default_hours=0, $default_cost=0) {
    $sql = "UPDATE ".TB_PREF."training_courses SET
        course_code = ".db_escape($course_code).",
        course_name = ".db_escape($course_name).",
        provider = ".db_escape($provider).",
        default_hours = ".db_escape((float)$default_hours).",
        default_cost = ".db_escape((float)$default_cost)."
        WHERE course_id = ".db_escape((int)$course_id);

    return (bool)db_query($sql, 'could not update training course');
}

/**
 * Get training course.
 *
 * @param int $course_id
 * @return array|false
 */
function get_training_course($course_id) {
    $sql = "SELECT * FROM ".TB_PREF."training_courses WHERE course_id = ".db_escape((int)$course_id);
    $result = db_query($sql, 'could not get training course');

    return db_fetch_assoc($result);
}

/**
 * Get training courses.
 *
 * @return resource
 */
function get_training_courses() {
    $sql = "SELECT * FROM ".TB_PREF."training_courses WHERE inactive = 0 ORDER BY course_name";
    return db_query($sql, 'could not get training courses');
}

/**
 * Add employee training record.
 *
 * @param array $data
 * @return int
 */
function add_employee_training($data) {
    $sql = "INSERT INTO ".TB_PREF."employee_training
        (employee_id, course_id, training_date, completion_date, status, score, cost_amount, remarks)
        VALUES (
            ".db_escape($data['employee_id']).",
            ".db_escape((int)$data['course_id']).",
            ".db_escape(date2sql($data['training_date'])).",
            ".(trim($data['completion_date']) === '' ? 'NULL' : db_escape(date2sql($data['completion_date']))).",
            ".db_escape((int)$data['status']).",
            ".(trim($data['score']) === '' ? 'NULL' : db_escape((float)$data['score'])).",
            ".db_escape((float)$data['cost_amount']).",
            ".db_escape($data['remarks'])."
        )";
    db_query($sql, 'could not add employee training');

    return (int)db_insert_id();
}

/**
 * Get employee training records.
 *
 * @return resource
 */
function get_employee_training_records() {
    $sql = "SELECT et.*, tc.course_name,
            CONCAT(COALESCE(e.first_name,''), ' ', COALESCE(e.last_name,'')) employee_name
        FROM ".TB_PREF."employee_training et
        LEFT JOIN ".TB_PREF."training_courses tc ON tc.course_id = et.course_id
        LEFT JOIN ".TB_PREF."employees e ON e.employee_id = et.employee_id
        ORDER BY et.training_date DESC, et.training_id DESC";

    return db_query($sql, 'could not get employee training records');
}
