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
 * Get pay_elements table columns map.
 *
 * @return array
 */
function pay_element_columns() {
	static $columns = null;

	if ($columns !== null)
		return $columns;

	$columns = array();
	$sql = "SHOW COLUMNS FROM ".TB_PREF."pay_elements";
	$result = db_query($sql, 'could not get pay element columns');
	while ($row = db_fetch_assoc($result))
		$columns[$row['Field']] = true;

	return $columns;
}

/**
 * Check whether pay_elements table has a column.
 *
 * @param string $column_name
 * @return bool
 */
function pay_element_has_column($column_name) {
	$columns = pay_element_columns();
	return isset($columns[$column_name]);
}

/**
 * Add pay element.
 *
 * Backward compatible signature:
 * add_pay_element($element_name, $account_code, $is_deduction=0, $amount_type=0)
 *
 * @param string $element_name
 * @param string $account_code
 * @param int $is_deduction
 * @param int $amount_type
 * @param array $data Optional enhanced fields.
 * @return int|false
 */
function add_pay_element($element_name, $account_code, $is_deduction=0, $amount_type=0, $data=array()) {
	if (!is_array($data))
		$data = array();

	$fields = array('element_name', 'account_code', 'is_deduction', 'amount_type');
	$values = array(
		db_escape($element_name),
		db_escape($account_code),
		db_escape((int)$is_deduction),
		db_escape((int)$amount_type)
	);

	if (pay_element_has_column('element_code')) {
		$element_code = array_key_exists('element_code', $data) ? trim((string)$data['element_code']) : '';
		if ($element_code === '')
			$element_code = strtoupper(substr(preg_replace('/[^A-Za-z0-9_]/', '', (string)$element_name), 0, 20));
		if ($element_code === '')
			$element_code = 'ELEM'.time();
		$fields[] = 'element_code';
		$values[] = db_escape($element_code);
	}

	if (pay_element_has_column('element_category')) {
		$fields[] = 'element_category';
		$values[] = db_escape(array_key_exists('element_category', $data) ? (int)$data['element_category'] : ((int)$is_deduction ? 2 : 1));
	}

	if (pay_element_has_column('default_amount') && array_key_exists('default_amount', $data)) {
		$fields[] = 'default_amount';
		$values[] = db_escape((float)$data['default_amount']);
	}
	if (pay_element_has_column('formula') && array_key_exists('formula', $data)) {
		$fields[] = 'formula';
		$values[] = db_escape($data['formula']);
	}
	if (pay_element_has_column('employer_account') && array_key_exists('employer_account', $data)) {
		$fields[] = 'employer_account';
		$values[] = db_escape($data['employer_account']);
	}
	if (pay_element_has_column('is_taxable') && array_key_exists('is_taxable', $data)) {
		$fields[] = 'is_taxable';
		$values[] = db_escape((int)$data['is_taxable']);
	}
	if (pay_element_has_column('affects_gross') && array_key_exists('affects_gross', $data)) {
		$fields[] = 'affects_gross';
		$values[] = db_escape((int)$data['affects_gross']);
	}
	if (pay_element_has_column('is_statutory') && array_key_exists('is_statutory', $data)) {
		$fields[] = 'is_statutory';
		$values[] = db_escape((int)$data['is_statutory']);
	}
	if (pay_element_has_column('max_amount') && array_key_exists('max_amount', $data)) {
		$fields[] = 'max_amount';
		$values[] = ($data['max_amount'] === '' || $data['max_amount'] === null) ? 'NULL' : db_escape((float)$data['max_amount']);
	}
	if (pay_element_has_column('min_amount') && array_key_exists('min_amount', $data)) {
		$fields[] = 'min_amount';
		$values[] = ($data['min_amount'] === '' || $data['min_amount'] === null) ? 'NULL' : db_escape((float)$data['min_amount']);
	}
	if (pay_element_has_column('display_order') && array_key_exists('display_order', $data)) {
		$fields[] = 'display_order';
		$values[] = db_escape((int)$data['display_order']);
	}
	if (pay_element_has_column('description') && array_key_exists('description', $data)) {
		$fields[] = 'description';
		$values[] = db_escape($data['description']);
	}

	$sql = "INSERT INTO ".TB_PREF."pay_elements (".implode(', ', $fields).") VALUES (".implode(', ', $values).")";
	$db_ok = db_query($sql, 'could not insert pay element');
	if (!$db_ok)
		return false;

	return db_insert_id();
}

/**
 * Get one pay element by id or account code.
 *
 * @param int|false $id
 * @param string|false $account_code
 * @return array|false
 */
function get_pay_element($id=false, $account_code=false) {
	$select = "e.*, a.account_name";
	$sql = "SELECT ".$select." FROM ".TB_PREF."pay_elements e
		LEFT JOIN ".TB_PREF."chart_master a ON a.account_code = e.account_code
		WHERE 1=1";

	if ($id)
		$sql .= " AND e.element_id = ".db_escape((int)$id);
	if ($account_code)
		$sql .= " AND e.account_code = ".db_escape($account_code);

	$sql .= " LIMIT 1";
	$result = db_query($sql, 'Could not get pay element');

	return db_fetch($result);
}

/**
 * Get pay element list.
 *
 * @param bool $all
 * @return resource
 */
function get_pay_elements($all=true) {
	$sql = "SELECT e.*, a.account_name FROM ".TB_PREF."pay_elements e
		LEFT JOIN ".TB_PREF."chart_master a ON a.account_code = e.account_code";

	if (!$all && pay_element_has_column('inactive'))
		$sql .= " WHERE !e.inactive";

	if (pay_element_has_column('display_order'))
		$sql .= " ORDER BY e.display_order, e.element_name";
	else
		$sql .= " ORDER BY e.element_name";

	return db_query($sql, 'Could not get pay elements data');
}

/**
 * Update pay element.
 *
 * Backward compatible signature:
 * update_pay_element($element_id, $element_name, $account_code, $is_deduction, $amount_type)
 *
 * @param int $element_id
 * @param string $element_name
 * @param string $account_code
 * @param int $is_deduction
 * @param int $amount_type
 * @param array $data Optional enhanced fields.
 * @return bool
 */
function update_pay_element($element_id, $element_name, $account_code, $is_deduction, $amount_type, $data=array()) {
	if (!is_array($data))
		$data = array();

	$set = array(
		"element_name = ".db_escape($element_name),
		"account_code = ".db_escape($account_code),
		"is_deduction = ".db_escape((int)$is_deduction),
		"amount_type = ".db_escape((int)$amount_type)
	);

	if (pay_element_has_column('element_code') && array_key_exists('element_code', $data))
		$set[] = "element_code = ".db_escape(trim((string)$data['element_code']));
	if (pay_element_has_column('element_category') && array_key_exists('element_category', $data))
		$set[] = "element_category = ".db_escape((int)$data['element_category']);
	if (pay_element_has_column('default_amount') && array_key_exists('default_amount', $data))
		$set[] = "default_amount = ".db_escape((float)$data['default_amount']);
	if (pay_element_has_column('formula') && array_key_exists('formula', $data))
		$set[] = "formula = ".db_escape($data['formula']);
	if (pay_element_has_column('employer_account') && array_key_exists('employer_account', $data))
		$set[] = "employer_account = ".db_escape($data['employer_account']);
	if (pay_element_has_column('is_taxable') && array_key_exists('is_taxable', $data))
		$set[] = "is_taxable = ".db_escape((int)$data['is_taxable']);
	if (pay_element_has_column('affects_gross') && array_key_exists('affects_gross', $data))
		$set[] = "affects_gross = ".db_escape((int)$data['affects_gross']);
	if (pay_element_has_column('is_statutory') && array_key_exists('is_statutory', $data))
		$set[] = "is_statutory = ".db_escape((int)$data['is_statutory']);
	if (pay_element_has_column('max_amount') && array_key_exists('max_amount', $data))
		$set[] = "max_amount = ".(($data['max_amount'] === '' || $data['max_amount'] === null) ? 'NULL' : db_escape((float)$data['max_amount']));
	if (pay_element_has_column('min_amount') && array_key_exists('min_amount', $data))
		$set[] = "min_amount = ".(($data['min_amount'] === '' || $data['min_amount'] === null) ? 'NULL' : db_escape((float)$data['min_amount']));
	if (pay_element_has_column('display_order') && array_key_exists('display_order', $data))
		$set[] = "display_order = ".db_escape((int)$data['display_order']);
	if (pay_element_has_column('description') && array_key_exists('description', $data))
		$set[] = "description = ".db_escape($data['description']);

	$sql = "UPDATE ".TB_PREF."pay_elements SET ".implode(', ', $set)." WHERE element_id = ".db_escape((int)$element_id);
	return (bool)db_query($sql, 'could not update pay element');
}

/**
 * Delete pay element.
 *
 * @param int $element_id
 * @return bool
 */
function delete_pay_element($element_id) {
	$sql = "DELETE FROM ".TB_PREF."pay_elements WHERE element_id = ".db_escape((int)$element_id);
	return (bool)db_query($sql, 'Could not delete pay element');
}

/**
 * Check if another element already uses the same account.
 *
 * @param int $element_id
 * @param string $account_code
 * @return bool
 */
function check_pay_element_duplicated($element_id, $account_code) {
	return check_empty_result("SELECT COUNT(*) FROM ".TB_PREF."pay_elements WHERE account_code = ".db_escape($account_code)." AND element_id != ".db_escape((int)$element_id));
}

/**
 * Check whether pay element is referenced in payroll data.
 *
 * @param int $element_id
 * @return bool
 */
function pay_element_used($element_id) {
	$element_id = (int)$element_id;

	if (check_empty_result("SELECT COUNT(*) FROM ".TB_PREF."salary_structure WHERE element_id = ".db_escape($element_id)))
		return true;
	if (check_empty_result("SELECT COUNT(*) FROM ".TB_PREF."employee_salary WHERE element_id = ".db_escape($element_id)))
		return true;
	if (check_empty_result("SELECT COUNT(*) FROM ".TB_PREF."payslip_details WHERE element_id = ".db_escape($element_id)))
		return true;

	return false;
}
