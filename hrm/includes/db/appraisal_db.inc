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
 * Employee appraisal DB helpers.
 */

/**
 * Ensure employee appraisal transaction type has numbering metadata.
 *
 * @return void
 */
function ensure_employee_appraisal_transaction_registration() {
    static $is_registered = false;

    if ($is_registered || !function_exists('register_systype_db_info'))
        return;

    register_systype_db_info(ST_EMPLOYEE_APPRAISAL, array(
        TB_PREF.'employee_appraisals',
        null,
        'appraisal_id',
        null,
        'appraisal_date'
    ));

    $is_registered = true;
}

/**
 * Add employee appraisal.
 *
 * @param array $data
 * @return int
 */
function add_employee_appraisal($data) {
    ensure_employee_appraisal_transaction_registration();

    $sql = "INSERT INTO ".TB_PREF."employee_appraisals
        (employee_id, reviewer_id, period_from, period_to, appraisal_date, overall_score, rating_scale, status, strengths, improvements, recommendation)
        VALUES (
            ".db_escape($data['employee_id']).",
            ".db_escape($data['reviewer_id']).",
            ".db_escape(date2sql($data['period_from'])).",
            ".db_escape(date2sql($data['period_to'])).",
            ".db_escape(date2sql($data['appraisal_date'])).",
            ".db_escape((float)$data['overall_score']).",
            ".db_escape((int)$data['rating_scale']).",
            ".db_escape((int)$data['status']).",
            ".db_escape($data['strengths']).",
            ".db_escape($data['improvements']).",
            ".db_escape($data['recommendation'])."
        )";
    db_query($sql, 'could not add appraisal');

    return (int)db_insert_id();
}

/**
 * Get one employee appraisal by id.
 *
 * @param int $appraisal_id
 * @return array|false
 */
function get_employee_appraisal($appraisal_id) {
    $sql = "SELECT ea.*,
            CONCAT(COALESCE(e.first_name,''), ' ', COALESCE(e.last_name,'')) employee_name,
            CONCAT(COALESCE(r.first_name,''), ' ', COALESCE(r.last_name,'')) reviewer_name
        FROM ".TB_PREF."employee_appraisals ea
        LEFT JOIN ".TB_PREF."employees e ON e.employee_id = ea.employee_id
        LEFT JOIN ".TB_PREF."employees r ON r.employee_id = ea.reviewer_id
        WHERE ea.appraisal_id = ".db_escape((int)$appraisal_id);

    $res = db_query($sql, 'could not get appraisal');
    return db_fetch($res);
}

/**
 * Update appraisal status.
 *
 * @param int $appraisal_id
 * @param int $status
 * @return bool
 */
function update_employee_appraisal_status($appraisal_id, $status) {
    $sql = "UPDATE ".TB_PREF."employee_appraisals
        SET status = ".db_escape((int)$status)."
        WHERE appraisal_id = ".db_escape((int)$appraisal_id);
    db_query($sql, 'could not update appraisal status');

    return db_num_affected_rows() > 0;
}

/**
 * Find a pending core approval draft for an appraisal.
 *
 * @param int $appraisal_id
 * @return array|false
 */
function find_approval_draft_for_employee_appraisal($appraisal_id) {
    $sql = "SELECT d.*, d.id AS draft_id
        FROM ".TB_PREF."approval_drafts d
        WHERE d.trans_type = ".db_escape((int)ST_EMPLOYEE_APPRAISAL)."
            AND d.status = 0
            AND d.draft_data LIKE ".db_escape('%\"appraisal_id\":'.(int)$appraisal_id.'%')."
        ORDER BY d.id DESC
        LIMIT 1";

    $res = db_query($sql, 'could not find appraisal approval draft');
    $row = db_fetch($res);

    return $row ? $row : false;
}

/**
 * Check if appraisal has a pending core approval draft.
 *
 * @param int $appraisal_id
 * @return bool
 */
function has_pending_employee_appraisal_approval($appraisal_id) {
    return find_approval_draft_for_employee_appraisal($appraisal_id) !== false;
}

/**
 * Get employee appraisals.
 *
 * @return resource
 */
function get_employee_appraisals() {
    $sql = "SELECT ea.*,
            CONCAT(COALESCE(e.first_name,''), ' ', COALESCE(e.last_name,'')) employee_name,
            CONCAT(COALESCE(r.first_name,''), ' ', COALESCE(r.last_name,'')) reviewer_name
        FROM ".TB_PREF."employee_appraisals ea
        LEFT JOIN ".TB_PREF."employees e ON e.employee_id = ea.employee_id
        LEFT JOIN ".TB_PREF."employees r ON r.employee_id = ea.reviewer_id
        ORDER BY ea.appraisal_date DESC, ea.appraisal_id DESC";

    return db_query($sql, 'could not get appraisals');
}
