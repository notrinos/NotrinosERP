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
 * Get existing department table columns map.
 *
 * @return array
 */
function department_columns() {
	static $columns = null;

	if ($columns !== null)
		return $columns;

	$columns = array();
	$sql = "SHOW COLUMNS FROM ".TB_PREF."departments";
	$result = db_query($sql, 'could not get department columns');
	while ($row = db_fetch_assoc($result))
		$columns[$row['Field']] = true;

	return $columns;
}

/**
 * Check whether departments table has a specific column.
 *
 * @param string $column_name
 * @return bool
 */
function department_has_column($column_name) {
	$columns = department_columns();
	return isset($columns[$column_name]);
}

/**
 * Build safe parent department value.
 *
 * @param mixed $parent_department_id
 * @param int $department_id
 * @return int|null
 */
function normalize_parent_department($parent_department_id, $department_id=0) {
	$parent_department_id = (int)$parent_department_id;
	$department_id = (int)$department_id;

	if ($parent_department_id <= 0 || $parent_department_id == $department_id)
		return null;

	return $parent_department_id;
}

/**
 * Update department record.
 *
 * Backward compatible with old signature:
 * update_department($department_id, $department_name, $account)
 *
 * @param int $department_id
 * @param string $department_name
 * @param string $account
 * @param array $data Optional hierarchy/enhanced fields.
 * @return bool
 */
function update_department($department_id, $department_name, $account, $data=array()) {
	$department_id = (int)$department_id;
	if ($department_id <= 0)
		return false;

	if (!is_array($data))
		$data = array();

	$set = array(
		"department_name = ".db_escape($department_name),
		"payroll_expense_account = ".db_escape($account)
	);

	if (department_has_column('department_code') && array_key_exists('department_code', $data))
		$set[] = "department_code = ".db_escape(trim((string)$data['department_code']));

	if (department_has_column('parent_department_id') && array_key_exists('parent_department_id', $data)) {
		$parent_id = normalize_parent_department($data['parent_department_id'], $department_id);
		$set[] = "parent_department_id = ".($parent_id === null ? 'NULL' : db_escape($parent_id));
	}

	if (department_has_column('manager_employee_id') && array_key_exists('manager_employee_id', $data)) {
		$manager = trim((string)$data['manager_employee_id']);
		$set[] = "manager_employee_id = ".($manager === '' ? 'NULL' : db_escape($manager));
	}

	if (department_has_column('cost_center_id') && array_key_exists('cost_center_id', $data))
		$set[] = "cost_center_id = ".db_escape((int)$data['cost_center_id']);

	if (department_has_column('payroll_liability_account') && array_key_exists('payroll_liability_account', $data)) {
		$liability = trim((string)$data['payroll_liability_account']);
		$set[] = "payroll_liability_account = ".($liability === '' ? 'NULL' : db_escape($liability));
	}

	if (department_has_column('description') && array_key_exists('description', $data))
		$set[] = "description = ".db_escape($data['description']);

	$sql = "UPDATE ".TB_PREF."departments SET ".implode(', ', $set)." WHERE department_id = ".db_escape($department_id);
	return (bool)db_query($sql, 'could not update department record');
}

/**
 * Add department record.
 *
 * Backward compatible with old signature:
 * add_department($department_name, $account)
 *
 * @param string $department_name
 * @param string $account
 * @param array $data Optional hierarchy/enhanced fields.
 * @return int|false
 */
function add_department($department_name, $account, $data=array()) {
	if (!is_array($data))
		$data = array();

	$fields = array('department_name', 'payroll_expense_account');
	$values = array(db_escape($department_name), db_escape($account));

	if (department_has_column('department_code') && array_key_exists('department_code', $data)) {
		$fields[] = 'department_code';
		$values[] = db_escape(trim((string)$data['department_code']));
	}

	if (department_has_column('parent_department_id') && array_key_exists('parent_department_id', $data)) {
		$parent_id = normalize_parent_department($data['parent_department_id']);
		$fields[] = 'parent_department_id';
		$values[] = ($parent_id === null ? 'NULL' : db_escape($parent_id));
	}

	if (department_has_column('manager_employee_id') && array_key_exists('manager_employee_id', $data)) {
		$manager = trim((string)$data['manager_employee_id']);
		$fields[] = 'manager_employee_id';
		$values[] = ($manager === '' ? 'NULL' : db_escape($manager));
	}

	if (department_has_column('cost_center_id') && array_key_exists('cost_center_id', $data)) {
		$fields[] = 'cost_center_id';
		$values[] = db_escape((int)$data['cost_center_id']);
	}

	if (department_has_column('payroll_liability_account') && array_key_exists('payroll_liability_account', $data)) {
		$liability = trim((string)$data['payroll_liability_account']);
		$fields[] = 'payroll_liability_account';
		$values[] = ($liability === '' ? 'NULL' : db_escape($liability));
	}

	if (department_has_column('description') && array_key_exists('description', $data)) {
		$fields[] = 'description';
		$values[] = db_escape($data['description']);
	}

	$sql = "INSERT INTO ".TB_PREF."departments (".implode(', ', $fields).") VALUES (".implode(', ', $values).")";
	$db_ok = db_query($sql, 'could not add department data');
	if (!$db_ok)
		return false;

	return db_insert_id();
}

/**
 * Get one department by id.
 *
 * @param int $department_id
 * @return array|false
 */
function get_department($department_id) {
	$sql = "SELECT d.*, p.department_name as parent_department_name
		FROM ".TB_PREF."departments d
		LEFT JOIN ".TB_PREF."departments p ON p.department_id = d.parent_department_id
		WHERE d.department_id = ".db_escape((int)$department_id);
	$result = db_query($sql, 'could not get department record');

	return db_fetch($result);
}

/**
 * Get all departments.
 *
 * @param bool $all
 * @return resource
 */
function get_departments($all=false) {
	$sql = "SELECT d.*, p.department_name as parent_department_name
		FROM ".TB_PREF."departments d
		LEFT JOIN ".TB_PREF."departments p ON p.department_id = d.parent_department_id";

	if (!$all)
		$sql .= " WHERE !d.inactive";

	$sql .= " ORDER BY COALESCE(d.parent_department_id, 0), d.department_name";

	return db_query($sql, 'could not get departments data');
}

/**
 * Get department direct children.
 *
 * @param int $parent_department_id
 * @param bool $all
 * @return resource
 */
function get_department_children($parent_department_id, $all=false) {
	$sql = "SELECT * FROM ".TB_PREF."departments WHERE parent_department_id = ".db_escape((int)$parent_department_id);
	if (!$all)
		$sql .= " AND !inactive";
	$sql .= " ORDER BY department_name";

	return db_query($sql, 'could not get department children');
}

/**
 * Check if one department has children.
 *
 * @param int $department_id
 * @return bool
 */
function department_has_children($department_id) {
	return check_empty_result("SELECT COUNT(*) FROM ".TB_PREF."departments WHERE parent_department_id = ".db_escape((int)$department_id));
}

/**
 * Delete department if it has no child departments.
 *
 * @param int $department_id
 * @return bool
 */
function delete_department($department_id) {
	if (department_has_children($department_id))
		return false;

	$sql = "DELETE FROM ".TB_PREF."departments WHERE department_id = ".db_escape((int)$department_id);
	return (bool)db_query($sql, 'Could not delete department record');
}

/**
 * Check whether any departments exist.
 *
 * @return bool
 */
function db_has_departments() {
	return check_empty_result("SELECT COUNT(*) FROM ".TB_PREF."departments");
}
