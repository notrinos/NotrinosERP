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
 * Statutory deduction CRUD
 */

/**
 * Convert user date to SQL date.
 *
 * @param string $date_value
 * @return string
 */
function statutory_to_sql_date($date_value) {
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
 * Add statutory deduction rule.
 *
 * @param string $name
 * @param string $code
 * @param float $employee_rate
 * @param float $employer_rate
 * @param float $employee_fixed
 * @param float $employer_fixed
 * @param float|null $ceiling_amount
 * @param float|null $floor_amount
 * @param int $calculation_base
 * @param string $employee_account
 * @param string $employer_account
 * @param string $effective_from
 * @param string $effective_to
 * @param int $inactive
 * @return int|false
 */
function add_statutory_deduction($name, $code, $employee_rate, $employer_rate, $employee_fixed, $employer_fixed, $ceiling_amount, $floor_amount, $calculation_base, $employee_account, $employer_account, $effective_from, $effective_to='', $inactive=0) {
    $from_sql = statutory_to_sql_date($effective_from);
    $to_sql = statutory_to_sql_date($effective_to);
    if ($from_sql === '')
        return false;

    $sql = "INSERT INTO ".TB_PREF."statutory_deductions
        (statutory_name, statutory_code, employee_rate, employer_rate, employee_fixed, employer_fixed,
         ceiling_amount, floor_amount, calculation_base, employee_account, employer_account, effective_from, effective_to, inactive)
        VALUES (
            ".db_escape($name).",
            ".db_escape($code).",
            ".db_escape((float)$employee_rate).",
            ".db_escape((float)$employer_rate).",
            ".db_escape((float)$employee_fixed).",
            ".db_escape((float)$employer_fixed).",
            ".($ceiling_amount === null || $ceiling_amount === '' ? 'NULL' : db_escape((float)$ceiling_amount)).",
            ".($floor_amount === null || $floor_amount === '' ? 'NULL' : db_escape((float)$floor_amount)).",
            ".db_escape((int)$calculation_base).",
            ".(trim((string)$employee_account) === '' ? 'NULL' : db_escape($employee_account)).",
            ".(trim((string)$employer_account) === '' ? 'NULL' : db_escape($employer_account)).",
            ".db_escape($from_sql).",
            ".($to_sql === '' ? 'NULL' : db_escape($to_sql)).",
            ".db_escape((int)$inactive)."
        )";

    db_query($sql, 'could not add statutory deduction');
    return db_insert_id();
}

/**
 * Update statutory deduction rule.
 *
 * @param int $statutory_id
 * @param string $name
 * @param string $code
 * @param float $employee_rate
 * @param float $employer_rate
 * @param float $employee_fixed
 * @param float $employer_fixed
 * @param float|null $ceiling_amount
 * @param float|null $floor_amount
 * @param int $calculation_base
 * @param string $employee_account
 * @param string $employer_account
 * @param string $effective_from
 * @param string $effective_to
 * @param int $inactive
 * @return bool
 */
function update_statutory_deduction($statutory_id, $name, $code, $employee_rate, $employer_rate, $employee_fixed, $employer_fixed, $ceiling_amount, $floor_amount, $calculation_base, $employee_account, $employer_account, $effective_from, $effective_to='', $inactive=0) {
    $from_sql = statutory_to_sql_date($effective_from);
    $to_sql = statutory_to_sql_date($effective_to);
    if ($from_sql === '')
        return false;

    $sql = "UPDATE ".TB_PREF."statutory_deductions SET
        statutory_name = ".db_escape($name).",
        statutory_code = ".db_escape($code).",
        employee_rate = ".db_escape((float)$employee_rate).",
        employer_rate = ".db_escape((float)$employer_rate).",
        employee_fixed = ".db_escape((float)$employee_fixed).",
        employer_fixed = ".db_escape((float)$employer_fixed).",
        ceiling_amount = ".($ceiling_amount === null || $ceiling_amount === '' ? 'NULL' : db_escape((float)$ceiling_amount)).",
        floor_amount = ".($floor_amount === null || $floor_amount === '' ? 'NULL' : db_escape((float)$floor_amount)).",
        calculation_base = ".db_escape((int)$calculation_base).",
        employee_account = ".(trim((string)$employee_account) === '' ? 'NULL' : db_escape($employee_account)).",
        employer_account = ".(trim((string)$employer_account) === '' ? 'NULL' : db_escape($employer_account)).",
        effective_from = ".db_escape($from_sql).",
        effective_to = ".($to_sql === '' ? 'NULL' : db_escape($to_sql)).",
        inactive = ".db_escape((int)$inactive)."
        WHERE statutory_id = ".db_escape((int)$statutory_id);

    return (bool)db_query($sql, 'could not update statutory deduction');
}

/**
 * Get one statutory deduction rule.
 *
 * @param int $statutory_id
 * @return array|false
 */
function get_statutory_deduction($statutory_id) {
    $sql = "SELECT * FROM ".TB_PREF."statutory_deductions WHERE statutory_id = ".db_escape((int)$statutory_id);
    $result = db_query($sql, 'could not get statutory deduction');
    return db_fetch_assoc($result);
}

/**
 * Get statutory deduction rules.
 *
 * @param bool $all
 * @param string $as_of_date
 * @return resource
 */
function get_statutory_deductions($all=false, $as_of_date='') {
    $sql = "SELECT * FROM ".TB_PREF."statutory_deductions WHERE 1=1";

    if (!$all)
        $sql .= " AND inactive = 0";

    if (trim((string)$as_of_date) !== '') {
        $as_of_sql = statutory_to_sql_date($as_of_date);
        if ($as_of_sql !== '') {
            $sql .= " AND effective_from <= ".db_escape($as_of_sql)
                ." AND (effective_to IS NULL OR effective_to >= ".db_escape($as_of_sql).")";
        }
    }

    $sql .= " ORDER BY statutory_name, statutory_id";
    return db_query($sql, 'could not get statutory deductions');
}

/**
 * Delete one statutory deduction rule.
 *
 * @param int $statutory_id
 * @return bool
 */
function delete_statutory_deduction($statutory_id) {
    $sql = "DELETE FROM ".TB_PREF."statutory_deductions WHERE statutory_id = ".db_escape((int)$statutory_id);
    return (bool)db_query($sql, 'could not delete statutory deduction');
}

