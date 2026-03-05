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
 * Add employee appraisal.
 *
 * @param array $data
 * @return int
 */
function add_employee_appraisal($data) {
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
