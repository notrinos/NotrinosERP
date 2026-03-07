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
 * Enhanced Employee CRUD
 * Supports comprehensive employee profile with all fields from the enhanced 0_employees table.
 */

/**
 * Check whether a table exists in current company database.
 *
 * @param string $table_name
 * @return bool
 */
function employee_table_exists($table_name) {
	$sql = "SHOW TABLES LIKE ".db_escape(TB_PREF.$table_name);
	$result = db_query($sql);
	return $result && db_num_rows($result) > 0;
}

/**
 * Normalize user or SQL date to SQL date format.
 *
 * @param string $date_value
 * @return string
 */
function employee_to_sql_date($date_value) {
	$date_value = trim((string)$date_value);
	if ($date_value === '')
		return '';

	if (preg_match('/^\d{4}-\d{2}-\d{2}$/', $date_value))
		return $date_value;

	return date2sql($date_value);
}

/**
 * Resolve employee WHERE condition by code or numeric id.
 *
 * @param mixed $employee_ref
 * @return string
 */
function employee_where_condition($employee_ref) {
	if (is_numeric($employee_ref))
		return "employee_number = ".db_escape($employee_ref);

	return "employee_id = ".db_escape($employee_ref);
}

function db_has_employees() {
	return check_empty_result("SELECT COUNT(*) FROM ".TB_PREF."employees");
}

/**
 * Add a new employee using associative array.
 * @param array $data Associative array of employee fields
 * @return int|false The new employee_number or false on error
 */
function add_employee($data) {
	if (!is_array($data))
		return false;

	// Required fields
	$employee_id  = isset($data['employee_id']) ? trim($data['employee_id']) : '';
	$first_name   = isset($data['first_name']) ? trim($data['first_name']) : '';
	$last_name    = isset($data['last_name']) ? trim($data['last_name']) : '';

	if ($employee_id === '' || $first_name === '' || $last_name === '')
		return false;

	$fields = array(
		'employee_id'       => db_escape($employee_id),
		'first_name'        => db_escape($first_name),
		'last_name'         => db_escape($last_name),
	);

	// Optional text/numeric fields
	$optional_text = array(
		'middle_name', 'nationality', 'address', 'city', 'state', 'country',
		'phone', 'mobile', 'email', 'personal_email',
		'national_id', 'passport', 'tax_number', 'social_security_no',
		'bank_name', 'bank_branch', 'bank_account', 'bank_routing',
		'emergency_name', 'emergency_relation', 'emergency_phone',
		'login_id', 'notes', 'reporting_to'
	);
	foreach ($optional_text as $f) {
		if (isset($data[$f]) && $data[$f] !== '')
			$fields[$f] = db_escape($data[$f]);
	}

	// Optional integer fields
	$optional_int = array(
		'gender', 'marital_status', 'dependents_no', 'payment_method',
		'employment_type', 'separation_reason',
		'department_id', 'position_id', 'grade_id', 'shift_id',
		'personal_salary', 'cost_center_id', 'dimension2_id', 'inactive'
	);
	foreach ($optional_int as $f) {
		if (isset($data[$f]))
			$fields[$f] = db_escape($data[$f]);
	}

	// Date fields (convert from user format to SQL)
	$date_fields = array(
		'birth_date', 'hire_date', 'confirmation_date', 'probation_end_date',
		'released_date', 'passport_expiry'
	);
	foreach ($date_fields as $f) {
		if (!empty($data[$f]))
			$fields[$f] = "'".employee_to_sql_date($data[$f])."'";
	}

	$cols = implode(', ', array_keys($fields));
	$vals = implode(', ', array_values($fields));

	$sql = "INSERT INTO ".TB_PREF."employees ($cols) VALUES ($vals)";
	db_query($sql, 'could not add employee record');

	return db_insert_id();
}

/**
 * Update an existing employee by employee_id (string code).
 * @param string $employee_id The employee code (e.g. 'EMP001')
 * @param array $data Associative array of fields to update
 */
function update_employee($employee_id, $data) {
	if (!is_array($data))
		return;

	$sets = array();

	// Text fields
	$text_fields = array(
		'first_name', 'last_name', 'middle_name', 'nationality',
		'address', 'city', 'state', 'country',
		'phone', 'mobile', 'email', 'personal_email',
		'national_id', 'passport', 'tax_number', 'social_security_no',
		'bank_name', 'bank_branch', 'bank_account', 'bank_routing',
		'emergency_name', 'emergency_relation', 'emergency_phone',
		'login_id', 'notes', 'reporting_to'
	);
	foreach ($text_fields as $f) {
		if (array_key_exists($f, $data))
			$sets[] = "$f = ".db_escape($data[$f]);
	}

	// Integer fields
	$int_fields = array(
		'gender', 'marital_status', 'dependents_no', 'payment_method',
		'employment_type', 'separation_reason',
		'department_id', 'position_id', 'grade_id', 'shift_id',
		'personal_salary', 'cost_center_id', 'dimension2_id', 'inactive'
	);
	foreach ($int_fields as $f) {
		if (array_key_exists($f, $data))
			$sets[] = "$f = ".db_escape($data[$f]);
	}

	// Date fields
	$date_fields = array(
		'birth_date', 'hire_date', 'confirmation_date', 'probation_end_date',
		'released_date', 'passport_expiry'
	);
	foreach ($date_fields as $f) {
		if (array_key_exists($f, $data)) {
			if (!empty($data[$f]))
				$sets[] = "$f = '".employee_to_sql_date($data[$f])."'";
			else
				$sets[] = "$f = NULL";
		}
	}

	if (empty($sets))
		return;

	$sql = "UPDATE ".TB_PREF."employees SET ".implode(', ', $sets)
		." WHERE ".employee_where_condition($employee_id);

	db_query($sql, 'could not update employee');
}

/**
 * Get employee by auto-increment primary key.
 */
function get_employee($employee_number) {
	$sql = "SELECT e.*, d.department_name, p.position_name, g.grade_name,
				CONCAT(e.first_name, ' ', e.last_name) as employee_name
			FROM ".TB_PREF."employees e
			LEFT JOIN ".TB_PREF."departments d ON e.department_id = d.department_id
			LEFT JOIN ".TB_PREF."positions p ON e.position_id = p.position_id
			LEFT JOIN ".TB_PREF."pay_grades g ON e.grade_id = g.grade_id
			WHERE e.".employee_where_condition($employee_number);
	$result = db_query($sql, 'failed getting an employee record');

	return db_fetch_assoc($result);
}

/**
 * Get employee by employee_id code (e.g. 'EMP001').
 */
function get_employee_by_code($employee_id) {
	$sql = "SELECT e.*, d.department_name, p.position_name, g.grade_name,
				CONCAT(e.first_name, ' ', e.last_name) as employee_name
			FROM ".TB_PREF."employees e
			LEFT JOIN ".TB_PREF."departments d ON e.department_id = d.department_id
			LEFT JOIN ".TB_PREF."positions p ON e.position_id = p.position_id
			LEFT JOIN ".TB_PREF."pay_grades g ON e.grade_id = g.grade_id
			WHERE e.employee_id = ".db_escape($employee_id);
	$result = db_query($sql, 'could not get employee data');

	return db_fetch_assoc($result);
}

/**
 * Get employee name by code.
 */
function get_employee_name($employee_id) {
	$sql = "SELECT CONCAT(first_name, ' ', last_name) as name FROM ".TB_PREF."employees WHERE employee_id = ".db_escape($employee_id);
	$result = db_query($sql, 'could not get employee name');
	$row = db_fetch($result);
	return $row ? $row['name'] : '';
}

/**
 * Get list of employees with optional filters.
 * @param int    $department_id Filter by department (0 = all)
 * @param bool   $all           Include inactive employees
 * @param int    $position_id   Filter by position (0 = all)
 * @param int    $grade_id      Filter by grade (0 = all)
 * @param string $search        Search first_name, last_name, employee_id
 * @return resource DB result
 */
function get_employees($department_id=0, $all=false, $position_id=0, $grade_id=0, $search='') {
	$sql = "SELECT e.*, d.department_name, p.position_name, g.grade_name,
				CONCAT(e.first_name, ' ', e.last_name) as employee_name
			FROM ".TB_PREF."employees e
			LEFT JOIN ".TB_PREF."departments d ON e.department_id = d.department_id
			LEFT JOIN ".TB_PREF."positions p ON e.position_id = p.position_id
			LEFT JOIN ".TB_PREF."pay_grades g ON e.grade_id = g.grade_id
			WHERE 1=1";

	if (!$all)
		$sql .= " AND !e.inactive";

	if (!empty($department_id))
		$sql .= " AND e.department_id = ".db_escape($department_id);

	if (!empty($position_id))
		$sql .= " AND e.position_id = ".db_escape($position_id);

	if (!empty($grade_id))
		$sql .= " AND e.grade_id = ".db_escape($grade_id);

	if (!empty($search)) {
		$s = db_escape('%'.$search.'%');
		$sql .= " AND (e.employee_id LIKE $s OR e.first_name LIKE $s OR e.last_name LIKE $s OR e.mobile LIKE $s OR e.email LIKE $s)";
	}

	$sql .= " ORDER BY e.first_name, e.last_name";

	return db_query($sql, 'could not get employees data');
}

/**
 * Get active employees for payroll (not separated, has position).
 */
function get_active_employees($department_id=0) {
	$sql = "SELECT e.*, CONCAT(e.first_name, ' ', e.last_name) as employee_name
			FROM ".TB_PREF."employees e
			WHERE e.inactive = 0
			AND (e.released_date IS NULL OR e.released_date = '0000-00-00' OR e.released_date = '')
			AND e.position_id > 0";

	if (!empty($department_id))
		$sql .= " AND e.department_id = ".db_escape($department_id);

	$sql .= " ORDER BY e.first_name, e.last_name";

	return db_query($sql, 'could not get active employees');
}

/**
 * Delete employee and related records. Returns false if employee has transactions.
 */
function delete_employee($employee_id) {
	if (!employee_table_exists('employees'))
		return false;

	$employee_where = employee_where_condition($employee_id);

	// Check if employee has payslips
	if (employee_table_exists('payslips') && check_empty_result("SELECT COUNT(*) FROM ".TB_PREF."payslips WHERE employee_id = ".db_escape($employee_id)))
		return false;
	if (employee_table_exists('payslip') && check_empty_result("SELECT COUNT(*) FROM ".TB_PREF."payslip WHERE employee_id = ".db_escape($employee_id)))
		return false;

	begin_transaction();

	// Delete related records
	if (employee_table_exists('employee_documents')) {
		$docs = db_query(
			"SELECT file_path FROM ".TB_PREF."employee_documents WHERE employee_id = ".db_escape($employee_id),
			'could not get employee documents for delete'
		);
		while ($doc = db_fetch($docs)) {
			if (!empty($doc['file_path']) && file_exists($doc['file_path'])) {
				@unlink($doc['file_path']);
			}
		}

		$sql = "DELETE FROM ".TB_PREF."employee_documents WHERE employee_id = ".db_escape($employee_id);
		db_query($sql, 'could not delete employee documents');
	}

	if (employee_table_exists('employee_dependents')) {
		$sql = "DELETE FROM ".TB_PREF."employee_dependents WHERE employee_id = ".db_escape($employee_id);
		db_query($sql, 'could not delete employee dependents');
	}

	if (employee_table_exists('employee_history')) {
		$sql = "DELETE FROM ".TB_PREF."employee_history WHERE employee_id = ".db_escape($employee_id);
		db_query($sql, 'could not delete employee history');
	}

	if (employee_table_exists('employee_salary')) {
		$sql = "DELETE FROM ".TB_PREF."employee_salary WHERE employee_id = ".db_escape($employee_id);
		db_query($sql, 'could not delete employee salary');
	}

	if (employee_table_exists('leave_balances')) {
		$sql = "DELETE FROM ".TB_PREF."leave_balances WHERE employee_id = ".db_escape($employee_id);
		db_query($sql, 'could not delete employee leave balances');
	}

	$sql = "DELETE FROM ".TB_PREF."employees WHERE ".$employee_where;
	db_query($sql, 'could not delete employee');

	commit_transaction();

	return true;
}

/**
 * Check if employee can be deleted (no transactions).
 */
function employee_can_delete($employee_id) {
	if (employee_table_exists('payslips') && check_empty_result("SELECT COUNT(*) FROM ".TB_PREF."payslips WHERE employee_id = ".db_escape($employee_id)))
		return false;
	if (employee_table_exists('payslip') && check_empty_result("SELECT COUNT(*) FROM ".TB_PREF."payslip WHERE employee_id = ".db_escape($employee_id)))
		return false;
	if (employee_table_exists('attendance') && check_empty_result("SELECT COUNT(*) FROM ".TB_PREF."attendance WHERE employee_id = ".db_escape($employee_id)))
		return false;
	return true;
}

/**
 * Get number of employees in a department.
 */
function get_department_employee_count($department_id) {
	$sql = "SELECT COUNT(*) as cnt FROM ".TB_PREF."employees WHERE department_id = ".db_escape($department_id)." AND !inactive";
	$result = db_query($sql, 'could not count employees');
	$row = db_fetch($result);
	return $row['cnt'];
}

/**
 * Get employee salary components (from salary_structure or personal salary).
 * @param string $employee_id
 * @param string $as_of_date SQL date (Y-m-d)
 * @return array Array of pay element rows with amounts
 */
function get_employee_salary_components($employee_id, $as_of_date=null) {
	static $cache = array();

	if (!$as_of_date)
		$as_of_date = date('Y-m-d');
	else
		$as_of_date = employee_to_sql_date($as_of_date);

	$cache_key = $employee_id.'|'.$as_of_date;
	if (array_key_exists($cache_key, $cache))
		return $cache[$cache_key];

	$emp = get_employee_by_code($employee_id);
	if (!$emp) {
		$cache[$cache_key] = array();
		return $cache[$cache_key];
	}

	if ($emp['personal_salary']) {
		// Use personal salary table
		$sql = "SELECT es.salary_id, es.element_id, es.amount, es.formula,
					es.effective_from, es.effective_to, es.reference,
					pe.element_name, pe.element_code, pe.element_category,
					pe.is_deduction, pe.amount_type, pe.account_code,
					pe.is_taxable, pe.affects_gross, pe.display_order
				FROM ".TB_PREF."employee_salary es
				JOIN ".TB_PREF."pay_elements pe ON es.element_id = pe.element_id
				WHERE es.employee_id = ".db_escape($employee_id)."
				AND es.is_active = 1
				AND es.effective_from <= ".db_escape($as_of_date)."
				AND (es.effective_to IS NULL OR es.effective_to >= ".db_escape($as_of_date).")
				ORDER BY pe.display_order, pe.element_name";
	} else {
		// Use position × grade salary structure
		$sql = "SELECT ss.structure_id, ss.element_id, ss.pay_amount as amount, ss.formula,
					ss.effective_from, ss.effective_to,
					pe.element_name, pe.element_code, pe.element_category,
					pe.is_deduction, pe.amount_type, pe.account_code,
					pe.is_taxable, pe.affects_gross, pe.display_order
				FROM ".TB_PREF."salary_structure ss
				JOIN ".TB_PREF."pay_elements pe ON ss.element_id = pe.element_id
				WHERE ss.position_id = ".db_escape($emp['position_id'])."
				AND ss.grade_id = ".db_escape($emp['grade_id'])."
				AND ss.is_active = 1
				AND ss.effective_from <= ".db_escape($as_of_date)."
				AND (ss.effective_to IS NULL OR ss.effective_to >= ".db_escape($as_of_date).")
				ORDER BY pe.display_order, pe.element_name";
	}

	$result = db_query($sql);

	if (!$result) {
		if (!empty($emp['personal_salary'])) {
			$sql = "SELECT es.salary_id, es.element_id, es.amount,
						NULL as formula,
						es.effective_from, es.effective_to, es.reference,
						pe.element_name,
						'' as element_code,
						IF(pe.is_deduction = 1, 2, 1) as element_category,
						pe.is_deduction, pe.amount_type, pe.account_code,
						1 as is_taxable,
						1 as affects_gross,
						0 as display_order
					FROM ".TB_PREF."employee_salary es
					JOIN ".TB_PREF."pay_elements pe ON es.element_id = pe.element_id
					WHERE es.employee_id = ".db_escape($employee_id)."
					AND es.effective_from <= ".db_escape($as_of_date)."
					AND (es.effective_to IS NULL OR es.effective_to >= ".db_escape($as_of_date).")
					ORDER BY pe.element_name";
		} else {
			$sql = "SELECT 0 as structure_id, ss.element_id, ss.pay_amount as amount,
						NULL as formula,
						NULL as effective_from,
						NULL as effective_to,
						pe.element_name,
						'' as element_code,
						IF(pe.is_deduction = 1, 2, 1) as element_category,
						pe.is_deduction, pe.amount_type, pe.account_code,
						1 as is_taxable,
						1 as affects_gross,
						0 as display_order
					FROM ".TB_PREF."salary_structure ss
					JOIN ".TB_PREF."pay_elements pe ON ss.element_id = pe.element_id
					WHERE ss.position_id = ".db_escape($emp['position_id'])."
					AND ss.grade_id = ".db_escape($emp['grade_id'])."
					ORDER BY pe.element_name";
		}

		$result = db_query($sql, 'could not get salary components');
	}

	$components = array();
	while ($row = db_fetch_assoc($result)) {
		$components[] = $row;
	}

	$cache[$cache_key] = $components;
	return $cache[$cache_key];
}

/**
 * Preload salary components for a batch of employees.
 *
 * Used by payroll processing to avoid repeating the same salary structure
 * queries for each employee in large payroll runs.
 *
 * @param array $employees Array of employee rows or employee ids.
 * @param string|null $as_of_date SQL or user date.
 * @return array employee_id => array of salary components
 */
function get_salary_components_for_employees($employees, $as_of_date=null) {
	$as_of_sql = $as_of_date ? employee_to_sql_date($as_of_date) : date('Y-m-d');
	$components_by_employee = array();
	$employees_by_id = array();
	$personal_ids = array();
	$structure_groups = array();

	foreach ((array)$employees as $employee) {
		if (is_array($employee)) {
			$employee_id = isset($employee['employee_id']) ? trim((string)$employee['employee_id']) : '';
			$employee_row = $employee;
		} else {
			$employee_id = trim((string)$employee);
			$employee_row = $employee_id === '' ? false : get_employee_by_code($employee_id);
		}

		if ($employee_id === '' || !is_array($employee_row))
			continue;

		$employees_by_id[$employee_id] = $employee_row;
		$components_by_employee[$employee_id] = array();

		if (!empty($employee_row['personal_salary'])) {
			$personal_ids[] = $employee_id;
			continue;
		}

		$position_id = isset($employee_row['position_id']) ? (int)$employee_row['position_id'] : 0;
		$grade_id = isset($employee_row['grade_id']) ? (int)$employee_row['grade_id'] : 0;
		if ($position_id <= 0 || $grade_id <= 0)
			continue;

		$group_key = $position_id.':'.$grade_id;
		if (!isset($structure_groups[$group_key])) {
			$structure_groups[$group_key] = array(
				'position_id' => $position_id,
				'grade_id' => $grade_id,
				'employees' => array()
			);
		}
		$structure_groups[$group_key]['employees'][] = $employee_id;
	}

	if (!empty($personal_ids)) {
		$id_list = array();
		foreach (array_unique($personal_ids) as $employee_id)
			$id_list[] = db_escape($employee_id);

		$sql = "SELECT es.employee_id, es.salary_id, es.element_id, es.amount, es.formula,
					es.effective_from, es.effective_to, es.reference,
					pe.element_name, pe.element_code, pe.element_category,
					pe.is_deduction, pe.amount_type, pe.account_code,
					pe.is_taxable, pe.affects_gross, pe.display_order
				FROM ".TB_PREF."employee_salary es
				JOIN ".TB_PREF."pay_elements pe ON es.element_id = pe.element_id
				WHERE es.employee_id IN (".implode(', ', $id_list).")
				AND es.is_active = 1
				AND es.effective_from <= ".db_escape($as_of_sql)."
				AND (es.effective_to IS NULL OR es.effective_to >= ".db_escape($as_of_sql).")
				ORDER BY es.employee_id, pe.display_order, pe.element_name";
		$result = db_query($sql, 'could not preload employee salary components');

		while ($row = db_fetch_assoc($result))
			$components_by_employee[$row['employee_id']][] = $row;
	}

	if (!empty($structure_groups)) {
		$where = array();
		foreach ($structure_groups as $group)
			$where[] = "(ss.position_id = ".db_escape((int)$group['position_id'])." AND ss.grade_id = ".db_escape((int)$group['grade_id']).")";

		$sql = "SELECT ss.position_id, ss.grade_id, ss.structure_id, ss.element_id, ss.pay_amount as amount, ss.formula,
					ss.effective_from, ss.effective_to,
					pe.element_name, pe.element_code, pe.element_category,
					pe.is_deduction, pe.amount_type, pe.account_code,
					pe.is_taxable, pe.affects_gross, pe.display_order
				FROM ".TB_PREF."salary_structure ss
				JOIN ".TB_PREF."pay_elements pe ON ss.element_id = pe.element_id
				WHERE (".implode(' OR ', $where).")
				AND ss.is_active = 1
				AND ss.effective_from <= ".db_escape($as_of_sql)."
				AND (ss.effective_to IS NULL OR ss.effective_to >= ".db_escape($as_of_sql).")
				ORDER BY ss.position_id, ss.grade_id, pe.display_order, pe.element_name";
		$result = db_query($sql, 'could not preload salary structure components');
		$components_by_group = array();

		while ($row = db_fetch_assoc($result)) {
			$group_key = (int)$row['position_id'].':'.(int)$row['grade_id'];
			if (!isset($components_by_group[$group_key]))
				$components_by_group[$group_key] = array();
			$components_by_group[$group_key][] = $row;
		}

		foreach ($structure_groups as $group_key => $group) {
			$group_components = isset($components_by_group[$group_key]) ? $components_by_group[$group_key] : array();
			foreach ($group['employees'] as $employee_id)
				$components_by_employee[$employee_id] = $group_components;
		}
	}

	foreach ($components_by_employee as $employee_id => $rows) {
		if (empty($rows))
			$components_by_employee[$employee_id] = get_employee_salary_components($employee_id, $as_of_sql);
	}

	return $components_by_employee;
}

/**
 * Record a change in employee history (for audit trail).
 */
function record_employee_history($employee_id, $change_type, $effective_date, $old_data=array(), $new_data=array(), $reason='') {
	$created_by = isset($_SESSION['wa_current_user']->user) ? $_SESSION['wa_current_user']->user :
		(isset($_SESSION['wa_current_user']->loginname) ? $_SESSION['wa_current_user']->loginname : '');

	$sql = "INSERT INTO ".TB_PREF."employee_history (employee_id, change_type, effective_date,
				old_department_id, new_department_id, old_position_id, new_position_id,
				old_grade_id, new_grade_id, old_salary, new_salary, reason, created_by)
			VALUES (
				".db_escape($employee_id).",
				".db_escape($change_type).",
				'".employee_to_sql_date($effective_date)."',
				".db_escape(@$old_data['department_id']).",
				".db_escape(@$new_data['department_id']).",
				".db_escape(@$old_data['position_id']).",
				".db_escape(@$new_data['position_id']).",
				".db_escape(@$old_data['grade_id']).",
				".db_escape(@$new_data['grade_id']).",
				".db_escape(@$old_data['salary']).",
				".db_escape(@$new_data['salary']).",
				".db_escape($reason).",
				".db_escape($created_by).")";

	db_query($sql, 'could not record employee history');
}
