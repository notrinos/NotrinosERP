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
 * Get one leave policy.
 *
 * @param int $policy_id
 * @return array|false
 */
function get_leave_policy($policy_id) {
    $sql = "SELECT p.*, lt.leave_name, g.grade_name
        FROM ".TB_PREF."leave_policies p
        LEFT JOIN ".TB_PREF."leave_types lt ON lt.leave_id = p.leave_id
        LEFT JOIN ".TB_PREF."pay_grades g ON g.grade_id = p.grade_id
        WHERE p.policy_id = ".db_escape((int)$policy_id);
    $result = db_query($sql, 'could not get leave policy');

    return db_fetch($result);
}

/**
 * Get leave policies by optional filters.
 *
 * @param bool $show_inactive
 * @param int $leave_id
 * @param int|null $grade_id
 * @return resource
 */
function get_leave_policies($show_inactive=false, $leave_id=0, $grade_id=null) {
    $where = array('1=1');

    if (!$show_inactive)
        $where[] = 'p.inactive = 0';

    if ((int)$leave_id > 0)
        $where[] = 'p.leave_id = '.db_escape((int)$leave_id);

    if ($grade_id !== null && $grade_id !== '' && (int)$grade_id >= 0)
        $where[] = 'IFNULL(p.grade_id,0) = '.db_escape((int)$grade_id);

    $sql = "SELECT p.*, lt.leave_name, g.grade_name
        FROM ".TB_PREF."leave_policies p
        LEFT JOIN ".TB_PREF."leave_types lt ON lt.leave_id = p.leave_id
        LEFT JOIN ".TB_PREF."pay_grades g ON g.grade_id = p.grade_id
        WHERE ".implode(' AND ', $where)."
        ORDER BY p.policy_name, p.effective_from DESC";

    return db_query($sql, 'could not get leave policies');
}

/**
 * Create leave policy.
 *
 * @param string $policy_name
 * @param int $leave_id
 * @param int|null $grade_id
 * @param int|null $employment_type
 * @param float $annual_entitlement
 * @param int $accrual_method
 * @param int $probation_applicable
 * @param int $min_service_months
 * @param string $effective_from
 * @param string|null $effective_to
 * @return int
 */
function add_leave_policy($policy_name, $leave_id, $grade_id, $employment_type, $annual_entitlement, $accrual_method, $probation_applicable, $min_service_months, $effective_from, $effective_to=null) {
    $effective_to_sql = ($effective_to === null || trim((string)$effective_to) === '')
        ? 'NULL'
        : db_escape(date2sql($effective_to));

    $grade_sql = ($grade_id === null || $grade_id === '' || (int)$grade_id <= 0) ? 'NULL' : db_escape((int)$grade_id);
    $employment_sql = ($employment_type === null || $employment_type === '' || (int)$employment_type < 0) ? 'NULL' : db_escape((int)$employment_type);

    $sql = "INSERT INTO ".TB_PREF."leave_policies (
            policy_name, leave_id, grade_id, employment_type, annual_entitlement,
            accrual_method, probation_applicable, min_service_months, effective_from, effective_to
        ) VALUES (
            ".db_escape($policy_name).",
            ".db_escape((int)$leave_id).",
            $grade_sql,
            $employment_sql,
            ".db_escape((float)$annual_entitlement).",
            ".db_escape((int)$accrual_method).",
            ".db_escape((int)$probation_applicable).",
            ".db_escape((int)$min_service_months).",
            ".db_escape(date2sql($effective_from)).",
            $effective_to_sql
        )";

    db_query($sql, 'could not add leave policy');
    return (int)db_insert_id();
}

/**
 * Update leave policy.
 *
 * @param int $policy_id
 * @param string $policy_name
 * @param int $leave_id
 * @param int|null $grade_id
 * @param int|null $employment_type
 * @param float $annual_entitlement
 * @param int $accrual_method
 * @param int $probation_applicable
 * @param int $min_service_months
 * @param string $effective_from
 * @param string|null $effective_to
 * @param int $inactive
 * @return void
 */
function update_leave_policy($policy_id, $policy_name, $leave_id, $grade_id, $employment_type, $annual_entitlement, $accrual_method, $probation_applicable, $min_service_months, $effective_from, $effective_to=null, $inactive=0) {
    $effective_to_sql = ($effective_to === null || trim((string)$effective_to) === '')
        ? 'NULL'
        : db_escape(date2sql($effective_to));

    $grade_sql = ($grade_id === null || $grade_id === '' || (int)$grade_id <= 0) ? 'NULL' : db_escape((int)$grade_id);
    $employment_sql = ($employment_type === null || $employment_type === '' || (int)$employment_type < 0) ? 'NULL' : db_escape((int)$employment_type);

    $sql = "UPDATE ".TB_PREF."leave_policies SET
            policy_name = ".db_escape($policy_name).",
            leave_id = ".db_escape((int)$leave_id).",
            grade_id = $grade_sql,
            employment_type = $employment_sql,
            annual_entitlement = ".db_escape((float)$annual_entitlement).",
            accrual_method = ".db_escape((int)$accrual_method).",
            probation_applicable = ".db_escape((int)$probation_applicable).",
            min_service_months = ".db_escape((int)$min_service_months).",
            effective_from = ".db_escape(date2sql($effective_from)).",
            effective_to = $effective_to_sql,
            inactive = ".db_escape((int)$inactive)."
        WHERE policy_id = ".db_escape((int)$policy_id);

    db_query($sql, 'could not update leave policy');
}

/**
 * Delete leave policy.
 *
 * @param int $policy_id
 * @return void
 */
function delete_leave_policy($policy_id) {
    $sql = "DELETE FROM ".TB_PREF."leave_policies WHERE policy_id = ".db_escape((int)$policy_id);
    db_query($sql, 'could not delete leave policy');
}

