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
 * Employee personal salary override CRUD
 * Used when employee.personal_salary = 1 (individual salary structure).
 */

/**
 * Convert date into SQL format.
 *
 * @param string $date_value
 * @return string
 */
function employee_salary_to_sql_date($date_value) {
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
 * Add one employee salary override row.
 *
 * @param string $employee_id
 * @param int $element_id
 * @param float $amount
 * @param string $effective_from
 * @param string $effective_to
 * @param string $formula
 * @param string $reference
 * @return int|false
 */
function add_employee_salary($employee_id, $element_id, $amount, $effective_from, $effective_to='', $formula='', $reference='') {
	$effective_from_sql = employee_salary_to_sql_date($effective_from);
	$effective_to_sql = employee_salary_to_sql_date($effective_to);
	if ($effective_from_sql === '')
		return false;

	$sql = "INSERT INTO ".TB_PREF."employee_salary (employee_id, element_id, amount, effective_from";

	if (!empty($effective_to_sql)) $sql .= ", effective_to";
	if (!empty($formula))      $sql .= ", formula";
	if (!empty($reference))    $sql .= ", reference";

	$sql .= ") VALUES ("
		.db_escape($employee_id).","
		.db_escape($element_id).","
		.db_escape($amount).",'"
		.$effective_from_sql."'";

	if (!empty($effective_to_sql)) $sql .= ", '".$effective_to_sql."'";
	if (!empty($formula))      $sql .= ", ".db_escape($formula);
	if (!empty($reference))    $sql .= ", ".db_escape($reference);

	$sql .= ")";

	db_query($sql, 'could not add employee salary record');
	return db_insert_id();
}

/**
 * Update one employee salary override row.
 *
 * @param int $salary_id
 * @param float $amount
 * @param string $effective_from
 * @param string $effective_to
 * @param string $formula
 * @param string $reference
 * @param int $is_active
 * @return bool
 */
function update_employee_salary($salary_id, $amount, $effective_from, $effective_to='', $formula='', $reference='', $is_active=1) {
	$effective_from_sql = employee_salary_to_sql_date($effective_from);
	$effective_to_sql = employee_salary_to_sql_date($effective_to);
	if ($effective_from_sql === '')
		return false;

	$sql = "UPDATE ".TB_PREF."employee_salary SET "
		."amount = ".db_escape($amount).", "
		."effective_from = '".$effective_from_sql."', "
		."formula = ".db_escape($formula).", "
		."reference = ".db_escape($reference).", "
		."is_active = ".db_escape($is_active);

	if (!empty($effective_to_sql))
		$sql .= ", effective_to = '".$effective_to_sql."'";
	else
		$sql .= ", effective_to = NULL";

	$sql .= " WHERE salary_id = ".db_escape($salary_id);

	return (bool)db_query($sql, 'could not update employee salary');
}

/**
 * Get one employee salary override row.
 *
 * @param int $salary_id
 * @return array|false
 */
function get_employee_salary($salary_id) {
	$sql = "SELECT es.*, pe.element_name, pe.element_code, pe.is_deduction, pe.element_category, pe.amount_type, pe.account_code
			FROM ".TB_PREF."employee_salary es
			JOIN ".TB_PREF."pay_elements pe ON es.element_id = pe.element_id
			WHERE es.salary_id = ".db_escape($salary_id);
	$result = db_query($sql, 'could not get employee salary');
	return db_fetch_assoc($result);
}

/**
 * Get employee salary overrides with optional status/date filtering.
 *
 * @param string $employee_id
 * @param bool $active_only
 * @param string|null $as_of_date
 * @return resource
 */
function get_employee_salaries($employee_id, $active_only=true, $as_of_date=null) {
	$sql = "SELECT es.*, pe.element_name, pe.element_code, pe.is_deduction, pe.element_category, pe.amount_type, pe.account_code, pe.display_order
			FROM ".TB_PREF."employee_salary es
			JOIN ".TB_PREF."pay_elements pe ON es.element_id = pe.element_id
			WHERE es.employee_id = ".db_escape($employee_id);

	if ($active_only)
		$sql .= " AND es.is_active = 1";

	if ($as_of_date !== null && $as_of_date !== '') {
		$as_of_sql = employee_salary_to_sql_date($as_of_date);
		if ($as_of_sql !== '') {
			$sql .= " AND es.effective_from <= ".db_escape($as_of_sql)
				." AND (es.effective_to IS NULL OR es.effective_to = '0000-00-00' OR es.effective_to >= ".db_escape($as_of_sql).")";
		}
	}

	$sql .= " ORDER BY pe.display_order, pe.element_name";

	return db_query($sql, 'could not get employee salaries');
}

/**
 * Delete one employee salary override row.
 *
 * @param int $salary_id
 * @return bool
 */
function delete_employee_salary($salary_id) {
	$sql = "DELETE FROM ".TB_PREF."employee_salary WHERE salary_id = ".db_escape($salary_id);
	return (bool)db_query($sql, 'could not delete employee salary');
}

/**
 * Soft-disable one employee salary override row.
 *
 * @param int $salary_id
 * @return bool
 */
function deactivate_employee_salary($salary_id) {
	$sql = "UPDATE ".TB_PREF."employee_salary SET is_active = 0 WHERE salary_id = ".db_escape($salary_id);
	return (bool)db_query($sql, 'could not deactivate employee salary');
}

/**
 * Get the total personal salary for an employee as of a given date.
 *
 * @param string $employee_id
 * @param string|null $as_of_date
 * @return float
 */
function get_employee_total_salary($employee_id, $as_of_date=null) {
	if (!$as_of_date)
		$as_of_date = Today();

	$as_of_sql = employee_salary_to_sql_date($as_of_date);
	if ($as_of_sql === '')
		$as_of_sql = date2sql(Today());

	$sql = "SELECT SUM(CASE WHEN pe.is_deduction = 0 THEN es.amount ELSE -es.amount END) as total
			FROM ".TB_PREF."employee_salary es
			JOIN ".TB_PREF."pay_elements pe ON es.element_id = pe.element_id
			WHERE es.employee_id = ".db_escape($employee_id)."
			AND es.is_active = 1
			AND es.effective_from <= ".db_escape($as_of_sql)."
			AND (es.effective_to IS NULL OR es.effective_to = '0000-00-00' OR es.effective_to >= ".db_escape($as_of_sql).")";

	$result = db_query($sql, 'could not calculate total salary');
	$row = db_fetch($result);
	return $row ? (float)$row['total'] : 0;
}

