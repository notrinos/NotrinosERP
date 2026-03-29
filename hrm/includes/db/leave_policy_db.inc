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

/**
 * Normalize leave policy date input to SQL format.
 *
 * @param string $date_value
 * @return string
 */
function leave_policy_to_sql_date($date_value) {
    $date_value = trim((string)$date_value);
    if ($date_value === '')
        return '';

    if (preg_match('/^\d{4}-\d{2}-\d{2}$/', $date_value))
        return $date_value;

    if (is_date($date_value))
        return date2sql($date_value);

    return $date_value;
}

/**
 * Find the best matching leave policy for an employee and leave type.
 *
 * @param array $employee
 * @param int $leave_id
 * @param string $as_of_date
 * @return array|false
 */
function get_leave_policy_for_employee($employee, $leave_id, $as_of_date) {
    if (!is_array($employee) || empty($employee['employee_id']) || (int)$leave_id <= 0)
        return false;

    $as_of_sql = leave_policy_to_sql_date($as_of_date);
    if ($as_of_sql === '')
        $as_of_sql = date2sql(Today());

    $grade_id = isset($employee['grade_id']) ? (int)$employee['grade_id'] : 0;
    $employment_type = isset($employee['employment_type']) ? (int)$employee['employment_type'] : -1;

    $sql = "SELECT p.*, lt.leave_name, g.grade_name
        FROM ".TB_PREF."leave_policies p
        LEFT JOIN ".TB_PREF."leave_types lt ON lt.leave_id = p.leave_id
        LEFT JOIN ".TB_PREF."pay_grades g ON g.grade_id = p.grade_id
        WHERE p.inactive = 0
            AND p.leave_id = ".db_escape((int)$leave_id)."
            AND p.effective_from <= ".db_escape($as_of_sql)."
            AND (p.effective_to IS NULL OR p.effective_to = '' OR p.effective_to >= ".db_escape($as_of_sql).")
            AND (p.grade_id IS NULL OR p.grade_id = 0 OR p.grade_id = ".db_escape($grade_id).")
            AND (p.employment_type IS NULL OR p.employment_type = ".db_escape($employment_type).")
        ORDER BY
            CASE WHEN p.grade_id = ".db_escape($grade_id)." THEN 0 ELSE 1 END,
            CASE WHEN p.employment_type = ".db_escape($employment_type)." THEN 0 ELSE 1 END,
            p.effective_from DESC,
            p.policy_id DESC";

    $result = db_query($sql, 'could not get leave policy for employee');
    return db_fetch_assoc($result);
}

/**
 * Calculate leave entitlement from policy settings.
 *
 * @param array $policy
 * @param array $employee
 * @param string $as_of_date
 * @return float
 */
function calculate_leave_policy_entitlement($policy, $employee, $as_of_date) {
    if (!is_array($policy) || !is_array($employee))
        return 0;

    $annual_entitlement = (float)$policy['annual_entitlement'];
    if ($annual_entitlement <= 0)
        return 0;

    $as_of_sql = leave_policy_to_sql_date($as_of_date);
    if ($as_of_sql === '')
        $as_of_sql = date2sql(Today());

    $hire_date_sql = !empty($employee['hire_date']) ? leave_policy_to_sql_date($employee['hire_date']) : '';
    $probation_end_sql = !empty($employee['probation_end_date']) ? leave_policy_to_sql_date($employee['probation_end_date']) : '';

    if (empty($policy['probation_applicable']) && $probation_end_sql !== '' && $probation_end_sql > $as_of_sql)
        return 0;

    $service_months = 0;
    if ($hire_date_sql !== '') {
        list($hire_year, $hire_month, $hire_day) = array_map('intval', explode('-', $hire_date_sql));
        list($asof_year, $asof_month, $asof_day) = array_map('intval', explode('-', $as_of_sql));
        $service_months = (($asof_year - $hire_year) * 12) + ($asof_month - $hire_month);
        if ($asof_day < $hire_day)
            $service_months--;
        if ($service_months < 0)
            $service_months = 0;
    }

    if ((int)$policy['min_service_months'] > $service_months)
        return 0;

    $accrual_method = isset($policy['accrual_method']) ? (int)$policy['accrual_method'] : 0;
    if ($accrual_method === 0)
        return round2($annual_entitlement, user_price_dec());

    $period_start_sql = substr($as_of_sql, 0, 4).'-01-01';
    if ($hire_date_sql !== '' && $hire_date_sql > $period_start_sql)
        $period_start_sql = $hire_date_sql;

    list($start_year, $start_month, $start_day) = array_map('intval', explode('-', $period_start_sql));
    list($asof_year, $asof_month, $asof_day) = array_map('intval', explode('-', $as_of_sql));
    $elapsed_months = (($asof_year - $start_year) * 12) + ($asof_month - $start_month) + 1;
    if ($asof_day < $start_day)
        $elapsed_months--;

    if ($elapsed_months < 0)
        $elapsed_months = 0;
    if ($elapsed_months > 12)
        $elapsed_months = 12;

    if ($accrual_method === 1)
        return round2(($annual_entitlement / 12) * $elapsed_months, user_price_dec());

    $elapsed_quarters = (int)ceil($elapsed_months / 3);
    if ($elapsed_quarters > 4)
        $elapsed_quarters = 4;

    return round2(($annual_entitlement / 4) * $elapsed_quarters, user_price_dec());
}

