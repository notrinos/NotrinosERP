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
 * Check whether a table exists in current company database.
 *
 * @param string $table_name Table name without prefix.
 * @return bool
 */
function payslip_table_exists($table_name) {
	$sql = "SHOW TABLES LIKE '".TB_PREF.$table_name."'";
	$result = db_query($sql);
	return $result && db_num_rows($result) > 0;
}

/**
 * Get fallback header columns for the standard payslips schema.
 *
 * @return array
 */
function payslip_default_header_columns() {
	return array_fill_keys(array(
		'payslip_id', 'payroll_period_id', 'trans_no', 'reference', 'employee_id',
		'tran_date', 'from_date', 'to_date', 'department_id', 'position_id',
		'grade_id', 'basic_salary', 'total_earnings', 'gross_salary',
		'total_deductions', 'net_salary', 'employer_contributions', 'total_cost',
		'working_days', 'worked_days', 'overtime_hours', 'leave_days',
		'absent_days', 'leaves', 'deductable_leaves', 'loan_deduction',
		'payable_amount', 'salary_amount', 'status', 'payment_trans_no', 'notes',
		'custom_data'
	), true);
}

/**
 * Get fallback detail columns for the standard payslip_details schema.
 *
 * @return array
 */
function payslip_default_detail_columns() {
	return array_fill_keys(array(
		'detail_id', 'payslip_id', 'element_id', 'element_name', 'element_category',
		'is_deduction', 'amount_type', 'base_amount', 'rate', 'calculated_amount',
		'adjusted_amount', 'final_amount', 'account_code', 'formula_used',
		'is_taxable', 'display_order', 'memo'
	), true);
}

/**
 * Resolve payslip header table name for current installation.
 *
 * @return string|false
 */
function payslip_header_table() {
	static $table_name = null;

	if ($table_name !== null)
		return $table_name;

	if (payslip_table_exists('payslips')) {
		$table_name = 'payslips';
		return $table_name;
	}

	$table_name = false;
	return false;
}

/**
 * Get table columns as associative map: column_name => true.
 *
 * @param string $table_name Table name without prefix.
 * @return array
 */
function payslip_table_columns($table_name) {
	static $cache = array();

	if (isset($cache[$table_name]))
		return $cache[$table_name];

	$columns = array();
	if (!payslip_table_exists($table_name)) {
		$cache[$table_name] = $columns;
		return $columns;
	}

	$sql = "SHOW COLUMNS FROM ".TB_PREF.$table_name;
	$result = db_query($sql);
	while ($row = db_fetch_assoc($result))
		$columns[$row['Field']] = true;

	$cache[$table_name] = $columns;
	return $columns;
}

/**
 * Check whether a column exists in a table.
 *
 * @param string $table_name Table name without prefix.
 * @param string $column_name Column name.
 * @return bool
 */
function payslip_has_column($table_name, $column_name) {
	$columns = payslip_table_columns($table_name);
	return isset($columns[$column_name]);
}

/**
 * Convert user date/string to SQL date format when needed.
 *
 * @param string $date_value
 * @return string
 */
function payslip_to_sql_date($date_value) {
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
 * Get current user id for audit columns when available.
 *
 * @return int|null
 */
function payslip_current_user_id() {
	if (!isset($_SESSION['wa_current_user']))
		return null;

	$user = $_SESSION['wa_current_user'];
	if (isset($user->user) && is_numeric($user->user))
		return (int)$user->user;
	if (isset($user->id) && is_numeric($user->id))
		return (int)$user->id;

	return null;
}

/**
 * Build employee condition for current payslip header table.
 *
 * @param string $employee_id
 * @param string $table_alias Optional table alias.
 * @return string
 */
function payslip_employee_where($employee_id, $table_alias='') {
	$prefix = $table_alias !== '' ? $table_alias.'.' : '';

	return $prefix."employee_id = ".db_escape($employee_id);
}

/**
 * Save payslip header + details from payslip_doc.
 *
 * @param payslip_doc $payslip_doc
 * @return int|false
 */
function add_payslip(&$payslip_doc) {
	$table_name = payslip_header_table();
	if (!$table_name)
		$table_name = 'payslips';
	if (!is_object($payslip_doc))
		return false;

	$columns = payslip_table_columns($table_name);
	if (empty($columns) && $table_name == 'payslips')
		$columns = payslip_default_header_columns();
	if (empty($columns))
		return false;

	$from_date = payslip_to_sql_date(isset($payslip_doc->from_date) ? $payslip_doc->from_date : '');
	$to_date = payslip_to_sql_date(isset($payslip_doc->to_date) ? $payslip_doc->to_date : '');
	$tran_date = payslip_to_sql_date(isset($payslip_doc->tran_date) ? $payslip_doc->tran_date : Today());

	$fields = array();
	$values = array();

	if (isset($columns['employee_id'])) {
		$fields[] = 'employee_id';
		$values[] = db_escape($payslip_doc->employee_id);
	}
	if (isset($columns['payroll_period_id'])) {
		$fields[] = 'payroll_period_id';
		$values[] = db_escape((int)$payslip_doc->payroll_period_id);
	}
	if (isset($columns['from_date']) && $from_date !== '') {
		$fields[] = 'from_date';
		$values[] = db_escape($from_date);
	}
	if (isset($columns['to_date']) && $to_date !== '') {
		$fields[] = 'to_date';
		$values[] = db_escape($to_date);
	}
	if (isset($columns['tran_date'])) {
		$fields[] = 'tran_date';
		$values[] = db_escape($tran_date);
	}
	if (isset($columns['generated_date'])) {
		$fields[] = 'generated_date';
		$values[] = db_escape($tran_date);
	}
	if (isset($columns['reference'])) {
		$fields[] = 'reference';
		$values[] = db_escape($payslip_doc->reference);
	}
	if (isset($columns['department_id']) && isset($payslip_doc->employee['department_id'])) {
		$fields[] = 'department_id';
		$values[] = db_escape((int)$payslip_doc->employee['department_id']);
	}
	if (isset($columns['position_id']) && isset($payslip_doc->employee['position_id'])) {
		$fields[] = 'position_id';
		$values[] = db_escape((int)$payslip_doc->employee['position_id']);
	}
	if (isset($columns['grade_id']) && isset($payslip_doc->employee['grade_id'])) {
		$fields[] = 'grade_id';
		$values[] = db_escape((int)$payslip_doc->employee['grade_id']);
	}
	if (isset($columns['basic_salary'])) {
		$fields[] = 'basic_salary';
		$values[] = db_escape((float)$payslip_doc->basic_salary);
	}
	if (isset($columns['total_earnings'])) {
		$fields[] = 'total_earnings';
		$values[] = db_escape((float)$payslip_doc->total_earnings);
	}
	if (isset($columns['gross_salary'])) {
		$fields[] = 'gross_salary';
		$values[] = db_escape((float)$payslip_doc->gross_salary);
	}
	if (isset($columns['salary_amount'])) {
		$fields[] = 'salary_amount';
		$values[] = db_escape((float)$payslip_doc->gross_salary);
	}
	if (isset($columns['total_deductions'])) {
		$fields[] = 'total_deductions';
		$values[] = db_escape((float)$payslip_doc->total_deductions);
	}
	if (isset($columns['net_salary'])) {
		$fields[] = 'net_salary';
		$values[] = db_escape((float)$payslip_doc->net_salary);
	}
	if (isset($columns['currency_code'])) {
		$fields[] = 'currency_code';
		$values[] = db_escape(isset($payslip_doc->currency_code) ? $payslip_doc->currency_code : '');
	}
	if (isset($columns['exchange_rate'])) {
		$fields[] = 'exchange_rate';
		$values[] = db_escape(isset($payslip_doc->exchange_rate) ? (float)$payslip_doc->exchange_rate : 1);
	}
	if (isset($columns['net_salary_home'])) {
		$fields[] = 'net_salary_home';
		$values[] = db_escape(isset($payslip_doc->net_salary_home) ? (float)$payslip_doc->net_salary_home : (float)$payslip_doc->net_salary);
	}
	if (isset($columns['payable_amount'])) {
		$fields[] = 'payable_amount';
		$values[] = db_escape((float)$payslip_doc->net_salary);
	}
	if (isset($columns['employer_contributions'])) {
		$fields[] = 'employer_contributions';
		$values[] = db_escape((float)$payslip_doc->employer_contributions);
	}
	if (isset($columns['total_cost'])) {
		$fields[] = 'total_cost';
		$values[] = db_escape((float)$payslip_doc->total_cost);
	}
	if (isset($columns['working_days'])) {
		$fields[] = 'working_days';
		$values[] = db_escape(isset($payslip_doc->working_days) ? (float)$payslip_doc->working_days : 0);
	}
	if (isset($columns['worked_days'])) {
		$fields[] = 'worked_days';
		$values[] = db_escape(isset($payslip_doc->worked_days) ? (float)$payslip_doc->worked_days : 0);
	}
	if (isset($columns['overtime_hours'])) {
		$fields[] = 'overtime_hours';
		$values[] = db_escape(isset($payslip_doc->overtime_hours) ? (float)$payslip_doc->overtime_hours : 0);
	}
	if (isset($columns['leave_days'])) {
		$fields[] = 'leave_days';
		$values[] = db_escape(isset($payslip_doc->leave_days) ? (float)$payslip_doc->leave_days : 0);
	}
	if (isset($columns['absent_days'])) {
		$fields[] = 'absent_days';
		$values[] = db_escape(isset($payslip_doc->absent_days) ? (float)$payslip_doc->absent_days : 0);
	}
	if (isset($columns['loan_deduction'])) {
		$fields[] = 'loan_deduction';
		$values[] = db_escape(isset($payslip_doc->loan_deduction) ? (float)$payslip_doc->loan_deduction : 0);
	}
	if (isset($columns['status'])) {
		$fields[] = 'status';
		$values[] = db_escape(isset($payslip_doc->status) ? (int)$payslip_doc->status : 0);
	}
	if (isset($columns['gl_trans_no'])) {
		$fields[] = 'gl_trans_no';
		$values[] = db_escape((int)$payslip_doc->gl_trans_no);
	}
	if (isset($columns['trans_no'])) {
		$fields[] = 'trans_no';
		$values[] = db_escape((int)$payslip_doc->gl_trans_no);
	}
	if (isset($columns['created_by'])) {
		$user_id = payslip_current_user_id();
		if ($user_id !== null) {
			$fields[] = 'created_by';
			$values[] = db_escape($user_id);
		}
	}
	if (isset($columns['created_date'])) {
		$fields[] = 'created_date';
		$values[] = "NOW()";
	}
	if (isset($columns['notes'])) {
		$fields[] = 'notes';
		$values[] = db_escape(isset($payslip_doc->comments) ? $payslip_doc->comments : '');
	}

	if (empty($fields))
		return false;

	begin_transaction();

	$sql = "INSERT INTO ".TB_PREF.$table_name." (".implode(', ', $fields).") VALUES (".implode(', ', $values).")";
	$result = db_query($sql, 'could not add payslip header');
	if (!$result)
		return false;

	$payslip_id = db_insert_id();
	if (!$payslip_id)
		$payslip_id = isset($payslip_doc->payslip_id) ? (int)$payslip_doc->payslip_id : 0;
	if (!$payslip_id && isset($columns['trans_no'])) {
		$lookup_sql = "SELECT payslip_id FROM ".TB_PREF.$table_name." WHERE trans_no = ".db_escape((int)$payslip_doc->gl_trans_no)." ORDER BY payslip_id DESC LIMIT 1";
		$lookup_result = db_query($lookup_sql);
		if ($lookup_result) {
			$lookup_row = db_fetch($lookup_result);
			if ($lookup_row && isset($lookup_row['payslip_id']))
				$payslip_id = (int)$lookup_row['payslip_id'];
		}
	}

	if ($payslip_id && payslip_table_exists('payslip_details') && !empty($payslip_doc->lines)) {
		$detail_columns = payslip_table_columns('payslip_details');
		if (empty($detail_columns))
			$detail_columns = payslip_default_detail_columns();
		foreach ($payslip_doc->lines as $line) {
			$d_fields = array();
			$d_values = array();

			if (isset($detail_columns['payslip_id'])) {
				$d_fields[] = 'payslip_id';
				$d_values[] = db_escape($payslip_id);
			}
			if (isset($detail_columns['element_id'])) {
				$d_fields[] = 'element_id';
				$d_values[] = db_escape((int)$line->element_id);
			}
			if (isset($detail_columns['element_name'])) {
				$d_fields[] = 'element_name';
				$d_values[] = db_escape($line->element_name);
			}
			if (isset($detail_columns['element_category'])) {
				$d_fields[] = 'element_category';
				$d_values[] = db_escape((int)$line->element_category);
			}
			if (isset($detail_columns['is_deduction'])) {
				$d_fields[] = 'is_deduction';
				$d_values[] = db_escape((int)!empty($line->is_deduction));
			}
			if (isset($detail_columns['amount_type'])) {
				$d_fields[] = 'amount_type';
				$d_values[] = db_escape((int)$line->amount_type);
			}
			if (isset($detail_columns['base_amount'])) {
				$d_fields[] = 'base_amount';
				$d_values[] = db_escape((float)$line->base_amount);
			}
			if (isset($detail_columns['rate'])) {
				$d_fields[] = 'rate';
				$d_values[] = db_escape((float)$line->rate);
			}
			if (isset($detail_columns['calculated_amount'])) {
				$d_fields[] = 'calculated_amount';
				$d_values[] = db_escape((float)$line->calculated_amount);
			}
			if (isset($detail_columns['adjusted_amount'])) {
				$d_fields[] = 'adjusted_amount';
				$d_values[] = 'NULL';
			}
			if (isset($detail_columns['final_amount'])) {
				$d_fields[] = 'final_amount';
				$d_values[] = db_escape((float)$line->final_amount);
			}
			if (isset($detail_columns['account_code'])) {
				$d_fields[] = 'account_code';
				$d_values[] = db_escape($line->account_code);
			}
			if (isset($detail_columns['formula_used'])) {
				$d_fields[] = 'formula_used';
				$d_values[] = db_escape($line->formula_used);
			}
			if (isset($detail_columns['is_taxable'])) {
				$d_fields[] = 'is_taxable';
				$d_values[] = db_escape((int)!empty($line->is_taxable));
			}
			if (isset($detail_columns['display_order'])) {
				$d_fields[] = 'display_order';
				$d_values[] = db_escape((int)$line->display_order);
			}

			if (!empty($d_fields)) {
				$dsql = "INSERT INTO ".TB_PREF."payslip_details (".implode(', ', $d_fields).") VALUES (".implode(', ', $d_values).")";
				db_query($dsql, 'could not add payslip detail');
			}
		}
	}

	commit_transaction();
	return (int)$payslip_id;
}

/**
 * Update payslip header fields.
 *
 * @param int $payslip_id
 * @param array $data
 * @return bool
 */
function update_payslip($payslip_id, $data) {
	$table_name = payslip_header_table();
	if (!$table_name || !is_array($data) || empty($data))
		return false;

	$columns = payslip_table_columns($table_name);
	$sets = array();
	$date_fields = array('from_date', 'to_date', 'tran_date', 'generated_date', 'voided_date');

	foreach ($data as $field => $value) {
		if (!isset($columns[$field]) || $field == 'payslip_id' || $field == 'payslip_no')
			continue;

		if (in_array($field, $date_fields)) {
			$sql_date = payslip_to_sql_date($value);
			$sets[] = $field." = ".($sql_date === '' ? 'NULL' : db_escape($sql_date));
		} else {
			$sets[] = $field." = ".(is_null($value) ? 'NULL' : db_escape($value));
		}
	}

	if (empty($sets))
		return false;

	$sql = "UPDATE ".TB_PREF.$table_name." SET ".implode(', ', $sets)." WHERE payslip_id = ".db_escape((int)$payslip_id);
	return (bool)db_query($sql, 'could not update payslip');
}

/**
 * Get one payslip header record.
 *
 * @param int $payslip_id
 * @return array|false
 */
function get_payslip($payslip_id) {
	$table_name = payslip_header_table();
	if (!$table_name)
		return false;

	$sql = "SELECT * FROM ".TB_PREF.$table_name." WHERE payslip_id = ".db_escape((int)$payslip_id);
	$result = db_query($sql, 'could not get payslip');

	return db_fetch($result);
}

/**
 * Get line details for a payslip.
 *
 * @param int $payslip_id
 * @return resource|false
 */
function get_payslip_details($payslip_id) {
	if (!payslip_table_exists('payslip_details'))
		return false;

	$sql = "SELECT * FROM ".TB_PREF."payslip_details WHERE payslip_id = ".db_escape((int)$payslip_id)." ORDER BY display_order, detail_id";
	return db_query($sql, 'could not get payslip details');
}

/**
 * Get payslips for one payroll period.
 *
 * @param int $period_id
 * @return resource|false
 */
function get_payslips_for_period($period_id) {
	$table_name = payslip_header_table();
	if (!$table_name)
		return false;

	$where = "payroll_period_id = ".db_escape((int)$period_id);

	$sql = "SELECT * FROM ".TB_PREF.$table_name." WHERE ".$where." ORDER BY from_date, to_date";
	return db_query($sql, 'could not get payslips for period');
}

/**
 * Get payslips for one employee with optional date range.
 *
 * @param string $employee_id
 * @param string|null $from
 * @param string|null $to
 * @return resource|false
 */
function get_payslips_for_employee($employee_id, $from=null, $to=null) {
	$table_name = payslip_header_table();
	if (!$table_name)
		return false;

	$where = array(payslip_employee_where($employee_id));

	if ($from !== null && $from !== '' && payslip_has_column($table_name, 'to_date'))
		$where[] = "to_date >= ".db_escape(payslip_to_sql_date($from));
	if ($to !== null && $to !== '' && payslip_has_column($table_name, 'from_date'))
		$where[] = "from_date <= ".db_escape(payslip_to_sql_date($to));

	$sql = "SELECT * FROM ".TB_PREF.$table_name." WHERE ".implode(' AND ', $where)." ORDER BY from_date DESC, to_date DESC";
	return db_query($sql, 'could not get payslips for employee');
}

/**
 * Void a payslip record without deleting it.
 *
 * @param int $payslip_id
 * @return bool
 */
function void_payslip($payslip_id) {
	$table_name = payslip_header_table();
	if (!$table_name)
		return false;

	$sets = array();

	if (payslip_has_column($table_name, 'status'))
		$sets[] = "status = 6";
	if (payslip_has_column($table_name, 'voided'))
		$sets[] = "voided = 1";
	if (payslip_has_column($table_name, 'is_voided'))
		$sets[] = "is_voided = 1";
	if (payslip_has_column($table_name, 'inactive'))
		$sets[] = "inactive = 1";
	if (payslip_has_column($table_name, 'voided_date'))
		$sets[] = "voided_date = ".db_escape(date2sql(Today()));

	if (empty($sets))
		return false;

	$sql = "UPDATE ".TB_PREF.$table_name." SET ".implode(', ', $sets)." WHERE payslip_id = ".db_escape((int)$payslip_id);
	$ok = (bool)db_query($sql, 'could not void payslip');

	if ($ok && function_exists('hrm_has_hook') && hrm_has_hook('on_void_payslip'))
		hrm_fire_hook('on_void_payslip', $payslip_id);

	return $ok;
}

/**
 * Check whether employee already has a payslip overlapping the period.
 *
 * @param string $employee_id
 * @param string $from_date
 * @param string $to_date
 * @return bool
 */
function payslip_exists_for_period($employee_id, $from_date, $to_date) {
	$table_name = payslip_header_table();
	if (!$table_name)
		return false;

	$from_sql = payslip_to_sql_date($from_date);
	$to_sql = payslip_to_sql_date($to_date);
	if ($from_sql === '' || $to_sql === '')
		return false;

	$sql = "SELECT COUNT(*) FROM ".TB_PREF.$table_name." WHERE ".payslip_employee_where($employee_id)." AND "
		."(from_date <= ".db_escape($to_sql)." AND to_date >= ".db_escape($from_sql).")";

	return check_empty_result($sql);
}

/**
 * Validate if payslip already exists for specific date.
 *
 * @param string $date
 * @param string $employee
 * @return bool
 */
function payslip_generated_for_date($date, $employee) {
	$table_name = payslip_header_table();
	if (!$table_name)
		return false;

	$sql_date = payslip_to_sql_date($date);
	if ($sql_date === '')
		return false;

	$sql = "SELECT COUNT(*) FROM ".TB_PREF.$table_name." WHERE ".payslip_employee_where($employee)
		." AND (".db_escape($sql_date)." BETWEEN from_date AND to_date)";

	return check_empty_result($sql);
}

/**
 * Validate if any payslip is fully enclosed in requested period.
 *
 * @param string $from_date
 * @param string $to_date
 * @param string $employee
 * @return bool
 */
function payslip_generated_for_period($from_date, $to_date, $employee) {
	$table_name = payslip_header_table();
	if (!$table_name)
		return false;

	$from_sql = payslip_to_sql_date($from_date);
	$to_sql = payslip_to_sql_date($to_date);
	if ($from_sql === '' || $to_sql === '')
		return false;

	$sql = "SELECT COUNT(*) FROM ".TB_PREF.$table_name." WHERE ".payslip_employee_where($employee)
		." AND (".db_escape($from_sql)." <= from_date AND ".db_escape($to_sql)." >= to_date)";

	return check_empty_result($sql);
}

/**
 * Get employee data required for payslip generation.
 *
 * @param string $employee_id
 * @return array|false
 */
function get_employee_to_payslip($employee_id) {
	$sql = "SELECT employee_id, CONCAT(first_name, ' ', last_name) as name,
		position_id, grade_id, hire_date, released_date
		FROM ".TB_PREF."employees
		WHERE employee_id = ".db_escape($employee_id);
	$result = db_query($sql, 'could not retrieve employee record');

	return db_fetch($result);
}

/**
 * Check employee is hired on or before date and not released before date.
 *
 * @param string $employee_id
 * @param string $date
 * @return bool
 */
function check_employee_hired($employee_id, $date) {
	$sql_date = payslip_to_sql_date($date);
	if ($sql_date === '')
		return false;

	$sql = "SELECT COUNT(*) FROM ".TB_PREF."employees
		WHERE employee_id = ".db_escape($employee_id)."
		AND (hire_date IS NULL OR hire_date = '0000-00-00' OR hire_date <= ".db_escape($sql_date).")
		AND (
			released_date IS NULL
			OR released_date = ''
			OR released_date = '0000-00-00'
			OR released_date >= ".db_escape($sql_date)."
		)";

	return check_empty_result($sql);
}

/**
 * Check whether employee has a valid job position.
 *
 * @param string $employee_id
 * @return bool
 */
function employee_has_position($employee_id) {
	$sql = "SELECT COUNT(*) FROM ".TB_PREF."employees
		WHERE employee_id = ".db_escape($employee_id)."
		AND position_id IS NOT NULL
		AND position_id > 0";

	return check_empty_result($sql);
}

/**
 * Check whether employee position has at least one salary structure row.
 *
 * @param string $employee_id
 * @return bool
 */
function emp_position_has_structure($employee_id) {
	$sql = "SELECT COUNT(*)
		FROM ".TB_PREF."employees e
		INNER JOIN ".TB_PREF."salary_structure ss ON ss.position_id = e.position_id
		WHERE e.employee_id = ".db_escape($employee_id);

	return check_empty_result($sql);
}
