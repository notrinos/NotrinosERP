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
 * Recruitment openings and applicants DB helpers.
 */

/**
 * Add recruitment opening.
 *
 * @param array $data
 * @return int
 */
function add_recruitment_opening($data) {
    $sql = "INSERT INTO ".TB_PREF."recruitment_openings
        (job_title, department_id, position_id, headcount, opening_date, closing_date, status, description, created_by)
        VALUES (
            ".db_escape($data['job_title']).",
            ".db_escape((int)$data['department_id']).",
            ".db_escape((int)$data['position_id']).",
            ".db_escape((int)$data['headcount']).",
            ".db_escape(date2sql($data['opening_date'])).",
            ".(trim($data['closing_date']) === '' ? 'NULL' : db_escape(date2sql($data['closing_date']))).",
            ".db_escape((int)$data['status']).",
            ".db_escape($data['description']).",
            ".db_escape(isset($_SESSION['wa_current_user']) ? (int)$_SESSION['wa_current_user']->user : 0)."
        )";
    db_query($sql, 'could not add recruitment opening');

    return (int)db_insert_id();
}

/**
 * Update recruitment opening.
 *
 * @param int $opening_id
 * @param array $data
 * @return bool
 */
function update_recruitment_opening($opening_id, $data) {
    $sql = "UPDATE ".TB_PREF."recruitment_openings SET
        job_title = ".db_escape($data['job_title']).",
        department_id = ".db_escape((int)$data['department_id']).",
        position_id = ".db_escape((int)$data['position_id']).",
        headcount = ".db_escape((int)$data['headcount']).",
        opening_date = ".db_escape(date2sql($data['opening_date'])).",
        closing_date = ".(trim($data['closing_date']) === '' ? 'NULL' : db_escape(date2sql($data['closing_date']))).",
        status = ".db_escape((int)$data['status']).",
        description = ".db_escape($data['description'])."
        WHERE opening_id = ".db_escape((int)$opening_id);

    return (bool)db_query($sql, 'could not update recruitment opening');
}

/**
 * Get recruitment opening.
 *
 * @param int $opening_id
 * @return array|false
 */
function get_recruitment_opening($opening_id) {
    $sql = "SELECT * FROM ".TB_PREF."recruitment_openings WHERE opening_id = ".db_escape((int)$opening_id);
    $result = db_query($sql, 'could not get recruitment opening');

    return db_fetch_assoc($result);
}

/**
 * Get recruitment openings list.
 *
 * @return resource
 */
function get_recruitment_openings() {
    $sql = "SELECT ro.*, d.department_name, p.position_name
        FROM ".TB_PREF."recruitment_openings ro
        LEFT JOIN ".TB_PREF."departments d ON d.department_id = ro.department_id
        LEFT JOIN ".TB_PREF."positions p ON p.position_id = ro.position_id
        ORDER BY ro.opening_date DESC, ro.opening_id DESC";

    return db_query($sql, 'could not get recruitment openings');
}

/**
 * Delete recruitment opening.
 *
 * @param int $opening_id
 * @return bool
 */
function delete_recruitment_opening($opening_id) {
    $sql = "DELETE FROM ".TB_PREF."recruitment_openings WHERE opening_id = ".db_escape((int)$opening_id);
    return (bool)db_query($sql, 'could not delete recruitment opening');
}

/**
 * Add recruitment applicant.
 *
 * @param array $data
 * @return int
 */
function add_recruitment_applicant($data) {
    $sql = "INSERT INTO ".TB_PREF."recruitment_applicants
        (opening_id, full_name, email, mobile, source, applied_date, status, expected_salary, remarks)
        VALUES (
            ".db_escape((int)$data['opening_id']).",
            ".db_escape($data['full_name']).",
            ".db_escape($data['email']).",
            ".db_escape($data['mobile']).",
            ".db_escape($data['source']).",
            ".db_escape(date2sql($data['applied_date'])).",
            ".db_escape((int)$data['status']).",
            ".(trim($data['expected_salary']) === '' ? 'NULL' : db_escape((float)$data['expected_salary'])).",
            ".db_escape($data['remarks'])."
        )";
    db_query($sql, 'could not add recruitment applicant');

    return (int)db_insert_id();
}

/**
 * Get recruitment applicants list.
 *
 * @return resource
 */
function get_recruitment_applicants() {
    $sql = "SELECT ra.*, ro.job_title
        FROM ".TB_PREF."recruitment_applicants ra
        LEFT JOIN ".TB_PREF."recruitment_openings ro ON ro.opening_id = ra.opening_id
        ORDER BY ra.applied_date DESC, ra.applicant_id DESC";

    return db_query($sql, 'could not get recruitment applicants');
}
