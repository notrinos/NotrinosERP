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
 * Build one migration validation result row.
 *
 * @param string $title
 * @param string $status pass|warn|fail
 * @param string $details
 * @return array
 */
function hrm_migration_result($title, $status, $details) {
	return array(
		'title' => $title,
		'status' => $status,
		'details' => $details
	);
}

/**
 * Check whether an HRM table exists.
 *
 * @param string $table_name
 * @return bool
 */
function hrm_migration_table_exists($table_name) {
	if (function_exists('employee_table_exists'))
		return employee_table_exists($table_name);
	if (function_exists('payroll_table_exists'))
		return payroll_table_exists($table_name);

	$sql = "SHOW TABLES LIKE '".TB_PREF.$table_name."'";
	$result = db_query($sql, 'could not inspect table');
	return $result && db_num_rows($result) > 0;
}

/**
 * Count rows from a table.
 *
 * @param string $table_name
 * @return int
 */
function hrm_migration_count_rows($table_name) {
	if (!hrm_migration_table_exists($table_name))
		return 0;

	$result = db_query("SELECT COUNT(*) FROM ".TB_PREF.$table_name, 'could not count table rows');
	$row = db_fetch_row($result);
	return $row ? (int)$row[0] : 0;
}

/**
 * Validate orphaned child rows against a parent table.
 *
 * @param string $child_table
 * @param string $child_column
 * @param string $parent_table
 * @param string $parent_column
 * @param string $title
 * @return array
 */
function hrm_validate_orphan_links($child_table, $child_column, $parent_table, $parent_column, $title) {
	if (!hrm_migration_table_exists($child_table) || !hrm_migration_table_exists($parent_table)) {
		return hrm_migration_result($title, 'warn', _('Skipped because one or more tables are not available.'));
	}

	$sql = "SELECT COUNT(*)
		FROM ".TB_PREF.$child_table." child
		LEFT JOIN ".TB_PREF.$parent_table." parent ON parent.".$parent_column." = child.".$child_column."
		WHERE child.".$child_column." IS NOT NULL
		AND child.".$child_column." <> ''
		AND parent.".$parent_column." IS NULL";
	$result = db_query($sql, 'could not validate migration links');
	$row = db_fetch_row($result);
	$orphan_count = $row ? (int)$row[0] : 0;

	if ($orphan_count > 0)
		return hrm_migration_result($title, 'fail', sprintf(_('%s orphaned row(s) found.'), $orphan_count));

	return hrm_migration_result($title, 'pass', _('No orphaned rows found.'));
}

/**
 * Validate that core HRM tables required by migration are present.
 *
 * @return array
 */
function hrm_validate_required_tables() {
	$required = array('employees', 'pay_elements', 'payroll_periods');
	$missing = array();

	foreach ($required as $table_name) {
		if (!hrm_migration_table_exists($table_name))
			$missing[] = TB_PREF.$table_name;
	}

	if (!empty($missing))
		return hrm_migration_result(_('Required HRM tables'), 'fail', sprintf(_('Missing tables: %s'), implode(', ', $missing)));

	return hrm_migration_result(_('Required HRM tables'), 'pass', _('All core HRM tables required for migration validation are available.'));
}

/**
 * Validate salary structure references.
 *
 * @return array
 */
function hrm_validate_salary_structure_references() {
	if (!hrm_migration_table_exists('salary_structure'))
		return hrm_migration_result(_('Salary structure references'), 'warn', _('Salary structure table is not available.'));

	$sql = "SELECT COUNT(*)
		FROM ".TB_PREF."salary_structure ss
		LEFT JOIN ".TB_PREF."positions p ON p.position_id = ss.position_id
		LEFT JOIN ".TB_PREF."pay_grades g ON g.grade_id = ss.grade_id
		LEFT JOIN ".TB_PREF."pay_elements pe ON pe.element_id = ss.element_id
		WHERE p.position_id IS NULL OR g.grade_id IS NULL OR pe.element_id IS NULL";
	$result = db_query($sql, 'could not validate salary structure references');
	$row = db_fetch_row($result);
	$invalid_count = $row ? (int)$row[0] : 0;

	if ($invalid_count > 0)
		return hrm_migration_result(_('Salary structure references'), 'fail', sprintf(_('%s salary structure row(s) point to missing master data.'), $invalid_count));

	return hrm_migration_result(_('Salary structure references'), 'pass', _('All salary structure rows point to valid positions, grades, and pay elements.'));
}

/**
 * Validate employee document attachment integrity.
 *
 * Checks for:
 *  - non-null attachment_id pointing to a missing attachment
 *  - linked attachment type not equal to ST_EMPLOYEE
 *  - linked attachment owner not equal to the employee's employee_number
 *  - orphan ST_EMPLOYEE attachments with no employee_document link
 *
 * @return array
 */
function hrm_validate_employee_attachment_integrity() {
	$title = _('Employee document attachment integrity');
	$issues = array();

	if (!hrm_migration_table_exists('employee_documents') || !hrm_migration_table_exists('attachments')) {
		return hrm_migration_result($title, 'warn', _('Skipped because required tables are not available.'));
	}

	// Check: non-null attachment_id pointing to a missing attachment.
	$sql = "SELECT COUNT(*) AS cnt FROM ".TB_PREF."employee_documents d
		LEFT JOIN ".TB_PREF."attachments a ON a.id = d.attachment_id
		WHERE d.attachment_id IS NOT NULL AND a.id IS NULL";
	$result = db_query($sql, 'could not validate attachment links');
	$row = db_fetch($result);
	if ($row && (int)$row['cnt'] > 0) {
		$issues[] = sprintf(_('%d employee document(s) have broken attachment links.'), (int)$row['cnt']);
	}

	// Check: linked attachment type not equal to ST_EMPLOYEE.
	$sql = "SELECT COUNT(*) AS cnt FROM ".TB_PREF."employee_documents d
		JOIN ".TB_PREF."attachments a ON a.id = d.attachment_id
		WHERE a.type_no != " . (int)ST_EMPLOYEE;
	$result = db_query($sql, 'could not validate attachment types');
	$row = db_fetch($result);
	if ($row && (int)$row['cnt'] > 0) {
		$issues[] = sprintf(_('%d employee document(s) linked to non-employee attachment type.'), (int)$row['cnt']);
	}

	// Check: linked attachment owner (trans_no) does not match employee_number.
	$sql = "SELECT COUNT(*) AS cnt FROM ".TB_PREF."employee_documents d
		JOIN ".TB_PREF."attachments a ON a.id = d.attachment_id
		JOIN ".TB_PREF."employees e ON e.employee_id = d.employee_id
		WHERE a.trans_no != e.employee_number";
	$result = db_query($sql, 'could not validate attachment ownership');
	$row = db_fetch($result);
	if ($row && (int)$row['cnt'] > 0) {
		$issues[] = sprintf(_('%d employee document(s) linked to wrong employee attachment.'), (int)$row['cnt']);
	}

	// Check: orphan ST_EMPLOYEE attachments (no employee_document references them).
	$sql = "SELECT COUNT(*) AS cnt FROM ".TB_PREF."attachments a
		WHERE a.type_no = " . (int)ST_EMPLOYEE . "
		AND NOT EXISTS (
			SELECT 1 FROM ".TB_PREF."employee_documents d
			WHERE d.attachment_id = a.id
		)";
	$result = db_query($sql, 'could not validate orphan attachments');
	$row = db_fetch($result);
	if ($row && (int)$row['cnt'] > 0) {
		$issues[] = sprintf(_('%d orphan employee attachment(s) found.'), (int)$row['cnt']);
	}

	if (!empty($issues)) {
		return hrm_migration_result($title, 'fail', implode(' ', $issues));
	}

	return hrm_migration_result($title, 'pass', _('All employee document attachments are valid.'));
}

/**
 * Validate HRM migration data against the current company database.
 *
 * @return array
 */
function hrm_run_migration_validations() {
	$results = array();
	$results[] = hrm_validate_required_tables();
	$results[] = hrm_migration_result(
		_('Employee master count'),
		hrm_migration_count_rows('employees') > 0 ? 'pass' : 'warn',
		sprintf(_('%s employee row(s) found.'), hrm_migration_count_rows('employees'))
	);
	$results[] = hrm_validate_orphan_links('employee_documents', 'employee_id', 'employees', 'employee_id', _('Employee document ownership'));
	$results[] = hrm_validate_employee_attachment_integrity();
	$results[] = hrm_validate_orphan_links('employee_history', 'employee_id', 'employees', 'employee_id', _('Employee history ownership'));
	$results[] = hrm_validate_orphan_links('employee_salary', 'employee_id', 'employees', 'employee_id', _('Employee salary ownership'));
	$results[] = hrm_validate_orphan_links('payslip_details', 'payslip_id', payslip_header_table() ? payslip_header_table() : 'payslips', payslip_header_table() && payslip_has_column(payslip_header_table(), 'payslip_id') ? 'payslip_id' : 'payslip_no', _('Payslip detail linkage'));
	$results[] = hrm_validate_orphan_links(payslip_header_table() ? payslip_header_table() : 'payslips', 'employee_id', 'employees', 'employee_id', _('Payslip employee linkage'));
	$results[] = hrm_validate_salary_structure_references();

	return $results;
}

if (!defined('HRM_MIGRATION_PROFILE_CONTRACT_ID'))
	define('HRM_MIGRATION_PROFILE_CONTRACT_ID', 'notrinoserp.hrm.migration.profile');
if (!defined('HRM_MIGRATION_PROFILE_CONTRACT_VERSION'))
	define('HRM_MIGRATION_PROFILE_CONTRACT_VERSION', '1.0.0');

/**
 * Return the logical tables included in the Phase 0 HRM source profile.
 *
 * The table list is fixed so no request value can become a SQL identifier.
 *
 * @return array
 */
function hrm_migration_profile_table_catalog() {
	return array(
		'employees', 'departments', 'positions', 'pay_grades', 'work_shifts',
		'employee_history', 'employee_salary', 'salary_structure', 'pay_elements',
		'attendance', 'overtime', 'overtime_requests', 'leave_requests',
		'leave_details', 'leave_balances', 'employee_loans', 'loan_types',
		'loan_repayments', 'payroll_periods', 'payslips', 'payslip_details',
		'employee_documents', 'document_types', 'attachments', 'tax_brackets',
		'statutory_deductions', 'hr_settings', 'gl_trans', 'bank_trans',
		'chart_master', 'dimensions', 'users'
	);
}

/**
 * Determine whether a SQL statement is permitted in the read-only profiler.
 *
 * @param string $sql
 * @return bool
 */
function hrm_migration_profile_is_read_only_sql($sql) {
	$normalized = strtoupper(trim(preg_replace('/\s+/', ' ', (string)$sql)));
	if ($normalized === '')
		return false;

	if (preg_match('/^(SET TRANSACTION READ ONLY|START TRANSACTION WITH CONSISTENT SNAPSHOT|ROLLBACK)$/', $normalized))
		return true;
	if (!preg_match('/^(SELECT|SHOW)\b/', $normalized))
		return false;
	if (preg_match('/\b(INTO\s+(OUTFILE|DUMPFILE)|FOR\s+UPDATE|LOCK\s+IN\s+SHARE\s+MODE)\b/', $normalized))
		return false;

	return true;
}

/**
 * Execute a profiler statement after enforcing the read-only allowlist.
 *
 * @param string $sql
 * @param string $error_message
 * @return mixed
 * @throws InvalidArgumentException
 */
function hrm_migration_profile_query($sql, $error_message='could not run HRM migration profile query') {
	if (!hrm_migration_profile_is_read_only_sql($sql))
		throw new InvalidArgumentException('HRM migration profiler rejected a non-read-only SQL statement.');

	$result = db_query($sql);
	if ($result === false)
		throw new RuntimeException($error_message);

	return $result;
}

/**
 * Test whether the current authenticated principal can run the profiler.
 *
 * @return bool
 */
function hrm_migration_profile_user_authorized() {
	if (!isset($_SESSION['wa_current_user']) || !is_object($_SESSION['wa_current_user']))
		return false;

	$user = $_SESSION['wa_current_user'];
	if (!method_exists($user, 'logged_in') || !$user->logged_in())
		return false;

	return method_exists($user, 'can_access') && $user->can_access('SA_HRSETTINGS');
}

/**
 * Return the current numeric user id without exposing a login name.
 *
 * @return int
 */
function hrm_migration_profile_actor_id() {
	if (!isset($_SESSION['wa_current_user']) || !is_object($_SESSION['wa_current_user']))
		return 0;

	$user = $_SESSION['wa_current_user'];
	return isset($user->user) && is_numeric($user->user) ? (int)$user->user : 0;
}

/**
 * Return the concrete prefix for the current company connection.
 *
 * TB_PREF remains the replacement token in application SQL, so it cannot be
 * used to parse table names returned by INFORMATION_SCHEMA.
 *
 * @return string
 */
function hrm_migration_profile_runtime_prefix() {
	global $db_connections;

	$company = 0;
	if (isset($_SESSION['wa_current_user']) && is_object($_SESSION['wa_current_user'])) {
		$user = $_SESSION['wa_current_user'];
		if (isset($user->cur_con) && is_numeric($user->cur_con))
			$company = (int)$user->cur_con;
		elseif (isset($user->company) && is_numeric($user->company))
			$company = (int)$user->company;
	}

	return isset($db_connections[$company]['tbpref']) ? (string)$db_connections[$company]['tbpref'] : '';
}

/**
 * Recursively normalize associative array key order for deterministic hashes.
 *
 * Numeric list order is retained because it is part of the report contract.
 *
 * @param mixed $value
 * @return mixed
 */
function hrm_migration_profile_normalize($value) {
	if (!is_array($value))
		return $value;

	$is_list = empty($value) || array_keys($value) === range(0, count($value) - 1);
	if (!$is_list)
		ksort($value, SORT_STRING);

	foreach ($value as $key => $item)
		$value[$key] = hrm_migration_profile_normalize($item);

	return $value;
}

/**
 * Encode a report value as deterministic JSON.
 *
 * @param mixed $value
 * @param bool $pretty
 * @return string
 * @throws RuntimeException
 */
function hrm_migration_profile_json($value, $pretty=false) {
	$options = JSON_UNESCAPED_SLASHES;
	if ($pretty)
		$options |= JSON_PRETTY_PRINT;

	$json = json_encode(hrm_migration_profile_normalize($value), $options);
	if ($json === false)
		throw new RuntimeException('Could not encode the HRM migration profile as JSON.');

	return $json;
}

/**
 * Read a database row value regardless of driver column-name casing.
 *
 * The PDO connection normalizes metadata names to lowercase while mysqli
 * preserves the INFORMATION_SCHEMA spelling.
 *
 * @param array $row
 * @param string $key
 * @return mixed|null
 */
function hrm_migration_profile_row_value($row, $key) {
	if (array_key_exists($key, $row))
		return $row[$key];

	$lower_key = strtolower($key);
	if (array_key_exists($lower_key, $row))
		return $row[$lower_key];

	return null;
}

/**
 * Return table schemas and row counts without selecting business values.
 *
 * @return array
 */
function hrm_migration_profile_schema_inventory() {
	$catalog = hrm_migration_profile_table_catalog();
	$runtime_prefix = hrm_migration_profile_runtime_prefix();
	$qualified_names = array();
	foreach ($catalog as $table_name)
		$qualified_names[] = db_escape($runtime_prefix.$table_name);

	$sql = "SELECT TABLE_NAME, COLUMN_NAME, COLUMN_TYPE, IS_NULLABLE, COLUMN_KEY, EXTRA"
		." FROM INFORMATION_SCHEMA.COLUMNS"
		." WHERE TABLE_SCHEMA = DATABASE()"
		." AND TABLE_NAME IN (".implode(', ', $qualified_names).")"
		." ORDER BY TABLE_NAME, ORDINAL_POSITION";
	$result = hrm_migration_profile_query($sql, 'could not inventory HRM migration schemas');

	$columns_by_table = array();
	while ($row = db_fetch_assoc($result)) {
		$table_name = hrm_migration_profile_row_value($row, 'TABLE_NAME');
		$column_name = hrm_migration_profile_row_value($row, 'COLUMN_NAME');
		$logical_name = substr($table_name, strlen($runtime_prefix));
		if (!in_array($logical_name, $catalog, true))
			continue;
		if (!isset($columns_by_table[$logical_name]))
			$columns_by_table[$logical_name] = array();
		$columns_by_table[$logical_name][$column_name] = array(
			'type' => strtolower(hrm_migration_profile_row_value($row, 'COLUMN_TYPE')),
			'nullable' => hrm_migration_profile_row_value($row, 'IS_NULLABLE'),
			'key' => hrm_migration_profile_row_value($row, 'COLUMN_KEY'),
			'extra' => hrm_migration_profile_row_value($row, 'EXTRA')
		);
	}

	$tables = array();
	foreach ($catalog as $table_name) {
		$present = isset($columns_by_table[$table_name]);
		$row_count = 0;
		if ($present) {
			$count_result = hrm_migration_profile_query(
				"SELECT COUNT(*) AS row_count FROM ".TB_PREF.$table_name,
				'could not count HRM migration source table'
			);
			$count_row = db_fetch_assoc($count_result);
			$row_count = $count_row ? (int)$count_row['row_count'] : 0;
		}

		$schema_columns = $present ? $columns_by_table[$table_name] : array();
		$tables[$table_name] = array(
			'present' => $present,
			'row_count' => $row_count,
			'column_count' => count($schema_columns),
			'schema_hash' => hash('sha256', hrm_migration_profile_json($schema_columns))
		);
	}

	return array('tables' => $tables, 'columns' => $columns_by_table);
}

/**
 * Return the deterministic anomaly checks for the current-company profile.
 *
 * Each query returns one aggregate count and never projects a source value.
 *
 * @return array
 */
function hrm_migration_profile_check_catalog() {
	$p = TB_PREF;
	return array(
		array(
			'key' => 'HRM.IDENTITY.EMPLOYEE_ID_BLANK', 'family' => 'identity',
			'severity' => 'blocker', 'owner' => 'hr_data_owner', 'gate' => 'blocks_affected_activation',
			'description' => 'Employee rows with a blank employee identifier.',
			'recommendation' => 'Assign and review a stable company-scoped employee identifier.',
			'requires' => array('employees' => array('employee_id')),
			'sql' => "SELECT COUNT(*) AS anomaly_count FROM {$p}employees WHERE TRIM(COALESCE(employee_id, '')) = ''"
		),
		array(
			'key' => 'HRM.IDENTITY.EMPLOYEE_ID_DUPLICATE', 'family' => 'identity',
			'severity' => 'blocker', 'owner' => 'hr_data_owner', 'gate' => 'blocks_affected_activation',
			'description' => 'Case-insensitive employee identifier duplicate groups.',
			'recommendation' => 'Resolve each candidate manually without merging payroll history.',
			'requires' => array('employees' => array('employee_id')),
			'sql' => "SELECT COUNT(*) AS anomaly_count FROM (SELECT 1 FROM {$p}employees WHERE TRIM(COALESCE(employee_id, '')) <> '' GROUP BY LOWER(TRIM(employee_id)) HAVING COUNT(*) > 1) profile_groups"
		),
		array(
			'key' => 'HRM.IDENTITY.SENSITIVE_IDENTIFIER_DUPLICATE', 'family' => 'identity',
			'severity' => 'high', 'owner' => 'security_privacy_owner', 'gate' => 'blocks_affected_activation',
			'description' => 'Duplicate groups across national, tax, social-security, passport, or bank identifiers.',
			'recommendation' => 'Review masked source records under the approved sensitive-data process.',
			'requires' => array('employees' => array('national_id', 'tax_number', 'social_security_no', 'passport', 'bank_account')),
			'sql' => "SELECT COUNT(*) AS anomaly_count FROM ("
				."SELECT 1 FROM {$p}employees WHERE TRIM(COALESCE(national_id, '')) <> '' GROUP BY LOWER(TRIM(national_id)) HAVING COUNT(*) > 1 UNION ALL "
				."SELECT 1 FROM {$p}employees WHERE TRIM(COALESCE(tax_number, '')) <> '' GROUP BY LOWER(TRIM(tax_number)) HAVING COUNT(*) > 1 UNION ALL "
				."SELECT 1 FROM {$p}employees WHERE TRIM(COALESCE(social_security_no, '')) <> '' GROUP BY LOWER(TRIM(social_security_no)) HAVING COUNT(*) > 1 UNION ALL "
				."SELECT 1 FROM {$p}employees WHERE TRIM(COALESCE(passport, '')) <> '' GROUP BY LOWER(TRIM(passport)) HAVING COUNT(*) > 1 UNION ALL "
				."SELECT 1 FROM {$p}employees WHERE TRIM(COALESCE(bank_account, '')) <> '' GROUP BY LOWER(TRIM(bank_account)) HAVING COUNT(*) > 1"
				.") profile_groups"
		),
		array(
			'key' => 'HRM.IDENTITY.LOGIN_DUPLICATE', 'family' => 'identity',
			'severity' => 'high', 'owner' => 'security_privacy_owner', 'gate' => 'blocks_affected_activation',
			'description' => 'Nonblank login identifiers linked to multiple employee rows.',
			'recommendation' => 'Review identity links and deny ambiguous self-service scope.',
			'requires' => array('employees' => array('login_id')),
			'sql' => "SELECT COUNT(*) AS anomaly_count FROM (SELECT 1 FROM {$p}employees WHERE TRIM(COALESCE(login_id, '')) <> '' GROUP BY LOWER(TRIM(login_id)) HAVING COUNT(*) > 1) profile_groups"
		),
		array(
			'key' => 'HRM.DATES.EMPLOYMENT_INVALID', 'family' => 'dates',
			'severity' => 'blocker', 'owner' => 'hr_data_owner', 'gate' => 'blocks_affected_activation',
			'description' => 'Employee rows with missing hire dates or impossible birth, confirmation, probation, or release ordering.',
			'recommendation' => 'Correct the source or record an explicit owner-approved exception; never substitute the current date.',
			'requires' => array('employees' => array('birth_date', 'hire_date', 'confirmation_date', 'probation_end_date', 'released_date')),
			'sql' => "SELECT COUNT(*) AS anomaly_count FROM {$p}employees WHERE hire_date IS NULL OR hire_date = '0000-00-00'"
				." OR (birth_date IS NOT NULL AND birth_date <> '0000-00-00' AND birth_date > hire_date)"
				." OR (released_date IS NOT NULL AND released_date <> '0000-00-00' AND released_date < hire_date)"
				." OR (confirmation_date IS NOT NULL AND confirmation_date <> '0000-00-00' AND (confirmation_date < hire_date OR (released_date IS NOT NULL AND released_date <> '0000-00-00' AND confirmation_date > released_date)))"
				." OR (probation_end_date IS NOT NULL AND probation_end_date <> '0000-00-00' AND probation_end_date < hire_date)"
		),
		array(
			'key' => 'HRM.DATES.EFFECTIVE_INTERVAL_INVALID', 'family' => 'dates',
			'severity' => 'blocker', 'owner' => 'hr_data_owner', 'gate' => 'blocks_affected_activation',
			'description' => 'Employee salary or salary-structure rows with an end date before the start date.',
			'recommendation' => 'Correct interval boundaries while preserving original source evidence.',
			'requires' => array('employee_salary' => array('effective_from', 'effective_to'), 'salary_structure' => array('effective_from', 'effective_to')),
			'sql' => "SELECT (SELECT COUNT(*) FROM {$p}employee_salary WHERE effective_to IS NOT NULL AND effective_to <> '0000-00-00' AND effective_to < effective_from)"
				." + (SELECT COUNT(*) FROM {$p}salary_structure WHERE effective_to IS NOT NULL AND effective_to <> '0000-00-00' AND effective_to < effective_from) AS anomaly_count"
		),
		array(
			'key' => 'HRM.DATES.PAYROLL_PERIOD_INVALID', 'family' => 'dates',
			'severity' => 'blocker', 'owner' => 'payroll_owner', 'gate' => 'blocks_affected_activation',
			'description' => 'Payroll periods with invalid boundaries or a pay date before the period start.',
			'recommendation' => 'Review the payroll calendar and retain the original period as historical evidence.',
			'requires' => array('payroll_periods' => array('from_date', 'to_date', 'pay_date')),
			'sql' => "SELECT COUNT(*) AS anomaly_count FROM {$p}payroll_periods WHERE from_date > to_date OR (pay_date IS NOT NULL AND pay_date <> '0000-00-00' AND pay_date < from_date)"
		),
		array(
			'key' => 'HRM.REFERENCES.EMPLOYEE_ORGANIZATION_ORPHAN', 'family' => 'references',
			'severity' => 'blocker', 'owner' => 'hr_data_owner', 'gate' => 'blocks_affected_activation',
			'description' => 'Employee department, position, grade, shift, or manager references that do not resolve.',
			'recommendation' => 'Map only verified master data; retain unknown references as migration exceptions.',
			'requires' => array(
				'employees' => array('department_id', 'position_id', 'grade_id', 'shift_id', 'reporting_to'),
				'departments' => array('department_id'), 'positions' => array('position_id'),
				'pay_grades' => array('grade_id'), 'work_shifts' => array('shift_id')
			),
			'sql' => "SELECT COUNT(*) AS anomaly_count FROM {$p}employees e"
				." LEFT JOIN {$p}departments d ON d.department_id = e.department_id"
				." LEFT JOIN {$p}positions p ON p.position_id = e.position_id"
				." LEFT JOIN {$p}pay_grades g ON g.grade_id = e.grade_id"
				." LEFT JOIN {$p}work_shifts s ON s.shift_id = e.shift_id"
				." LEFT JOIN {$p}employees m ON m.employee_id = e.reporting_to"
				." WHERE (e.department_id <> 0 AND d.department_id IS NULL)"
				." OR (e.position_id <> 0 AND p.position_id IS NULL)"
				." OR (e.grade_id <> 0 AND g.grade_id IS NULL)"
				." OR (e.shift_id IS NOT NULL AND e.shift_id <> 0 AND s.shift_id IS NULL)"
				." OR (TRIM(COALESCE(e.reporting_to, '')) <> '' AND m.employee_id IS NULL)"
		),
		array(
			'key' => 'HRM.REFERENCES.WORKFORCE_FACT_EMPLOYEE_ORPHAN', 'family' => 'references',
			'severity' => 'blocker', 'owner' => 'hr_data_owner', 'gate' => 'blocks_affected_activation',
			'description' => 'Attendance, leave, loan, salary, history, or document rows whose employee does not resolve.',
			'recommendation' => 'Quarantine affected facts; do not fabricate employees or rewrite payroll history.',
			'requires' => array(
				'employees' => array('employee_id'), 'attendance' => array('employee_id'),
				'leave_requests' => array('employee_id'), 'leave_balances' => array('employee_id'),
				'employee_loans' => array('employee_id'), 'employee_salary' => array('employee_id'),
				'employee_history' => array('employee_id'), 'employee_documents' => array('employee_id')
			),
			'sql' => "SELECT (SELECT COUNT(*) FROM {$p}attendance c LEFT JOIN {$p}employees e ON e.employee_id = c.employee_id WHERE e.employee_id IS NULL)"
				." + (SELECT COUNT(*) FROM {$p}leave_requests c LEFT JOIN {$p}employees e ON e.employee_id = c.employee_id WHERE e.employee_id IS NULL)"
				." + (SELECT COUNT(*) FROM {$p}leave_balances c LEFT JOIN {$p}employees e ON e.employee_id = c.employee_id WHERE e.employee_id IS NULL)"
				." + (SELECT COUNT(*) FROM {$p}employee_loans c LEFT JOIN {$p}employees e ON e.employee_id = c.employee_id WHERE e.employee_id IS NULL)"
				." + (SELECT COUNT(*) FROM {$p}employee_salary c LEFT JOIN {$p}employees e ON e.employee_id = c.employee_id WHERE e.employee_id IS NULL)"
				." + (SELECT COUNT(*) FROM {$p}employee_history c LEFT JOIN {$p}employees e ON e.employee_id = c.employee_id WHERE e.employee_id IS NULL)"
				." + (SELECT COUNT(*) FROM {$p}employee_documents c LEFT JOIN {$p}employees e ON e.employee_id = c.employee_id WHERE e.employee_id IS NULL) AS anomaly_count"
		),
		array(
			'key' => 'HRM.REFERENCES.PAY_CONFIGURATION_ORPHAN', 'family' => 'references',
			'severity' => 'blocker', 'owner' => 'payroll_owner', 'gate' => 'blocks_affected_activation',
			'description' => 'Salary or salary-structure rows whose pay element or organization master does not resolve.',
			'recommendation' => 'Resolve element, position, and grade mappings before parallel payroll.',
			'requires' => array(
				'employee_salary' => array('element_id'), 'salary_structure' => array('position_id', 'grade_id', 'element_id'),
				'pay_elements' => array('element_id'), 'positions' => array('position_id'), 'pay_grades' => array('grade_id')
			),
			'sql' => "SELECT (SELECT COUNT(*) FROM {$p}employee_salary c LEFT JOIN {$p}pay_elements pe ON pe.element_id = c.element_id WHERE pe.element_id IS NULL)"
				." + (SELECT COUNT(*) FROM {$p}salary_structure c LEFT JOIN {$p}positions p ON p.position_id = c.position_id LEFT JOIN {$p}pay_grades g ON g.grade_id = c.grade_id LEFT JOIN {$p}pay_elements pe ON pe.element_id = c.element_id WHERE p.position_id IS NULL OR g.grade_id IS NULL OR pe.element_id IS NULL) AS anomaly_count"
		),
		array(
			'key' => 'HRM.REFERENCES.LOAN_REPAYMENT_ORPHAN', 'family' => 'references',
			'severity' => 'high', 'owner' => 'payroll_owner', 'gate' => 'blocks_affected_activation',
			'description' => 'Loan repayment rows whose loan does not resolve.',
			'recommendation' => 'Reconcile the schedule to the original agreement without inventing a loan.',
			'requires' => array('loan_repayments' => array('loan_id'), 'employee_loans' => array('loan_id')),
			'sql' => "SELECT COUNT(*) AS anomaly_count FROM {$p}loan_repayments r LEFT JOIN {$p}employee_loans l ON l.loan_id = r.loan_id WHERE l.loan_id IS NULL"
		),
		array(
			'key' => 'HRM.REFERENCES.PAYSLIP_ORPHAN', 'family' => 'references',
			'severity' => 'blocker', 'owner' => 'payroll_owner', 'gate' => 'blocks_affected_activation',
			'description' => 'Payslip headers or lines whose employee, period, header, or element does not resolve.',
			'recommendation' => 'Keep affected history read-only and uncertified until the relationship is reconciled.',
			'requires' => array(
				'payslips' => array('payslip_id', 'payroll_period_id', 'employee_id'),
				'payslip_details' => array('payslip_id', 'element_id'), 'employees' => array('employee_id'),
				'payroll_periods' => array('period_id'), 'pay_elements' => array('element_id')
			),
			'sql' => "SELECT (SELECT COUNT(*) FROM {$p}payslips p LEFT JOIN {$p}employees e ON e.employee_id = p.employee_id LEFT JOIN {$p}payroll_periods pp ON pp.period_id = p.payroll_period_id WHERE e.employee_id IS NULL OR (p.payroll_period_id IS NOT NULL AND pp.period_id IS NULL))"
				." + (SELECT COUNT(*) FROM {$p}payslip_details d LEFT JOIN {$p}payslips p ON p.payslip_id = d.payslip_id LEFT JOIN {$p}pay_elements pe ON pe.element_id = d.element_id WHERE p.payslip_id IS NULL OR pe.element_id IS NULL) AS anomaly_count"
		),
		array(
			'key' => 'HRM.OVERLAP.EMPLOYEE_SALARY', 'family' => 'overlaps',
			'severity' => 'blocker', 'owner' => 'payroll_owner', 'gate' => 'blocks_affected_activation',
			'description' => 'Overlapping inclusive salary-entry intervals for one employee and element.',
			'recommendation' => 'Resolve precedence explicitly; do not select a row by incidental database order.',
			'requires' => array('employee_salary' => array('salary_id', 'employee_id', 'element_id', 'effective_from', 'effective_to', 'is_active')),
			'sql' => "SELECT COUNT(*) AS anomaly_count FROM {$p}employee_salary a JOIN {$p}employee_salary b"
				." ON a.salary_id < b.salary_id AND a.employee_id = b.employee_id AND a.element_id = b.element_id"
				." AND a.is_active = 1 AND b.is_active = 1"
				." AND a.effective_from <= COALESCE(NULLIF(b.effective_to, '0000-00-00'), '9999-12-31')"
				." AND b.effective_from <= COALESCE(NULLIF(a.effective_to, '0000-00-00'), '9999-12-31')"
		),
		array(
			'key' => 'HRM.OVERLAP.SALARY_STRUCTURE', 'family' => 'overlaps',
			'severity' => 'blocker', 'owner' => 'payroll_owner', 'gate' => 'blocks_affected_activation',
			'description' => 'Overlapping inclusive salary-structure intervals for one position, grade, and element.',
			'recommendation' => 'Resolve active configuration precedence before target calculation.',
			'requires' => array('salary_structure' => array('structure_id', 'position_id', 'grade_id', 'element_id', 'effective_from', 'effective_to', 'is_active')),
			'sql' => "SELECT COUNT(*) AS anomaly_count FROM {$p}salary_structure a JOIN {$p}salary_structure b"
				." ON a.structure_id < b.structure_id AND a.position_id = b.position_id AND a.grade_id = b.grade_id AND a.element_id = b.element_id"
				." AND a.is_active = 1 AND b.is_active = 1"
				." AND a.effective_from <= COALESCE(NULLIF(b.effective_to, '0000-00-00'), '9999-12-31')"
				." AND b.effective_from <= COALESCE(NULLIF(a.effective_to, '0000-00-00'), '9999-12-31')"
		),
		array(
			'key' => 'HRM.OVERLAP.PAYROLL_PERIOD', 'family' => 'overlaps',
			'severity' => 'high', 'owner' => 'payroll_owner', 'gate' => 'review_required',
			'description' => 'Overlapping payroll periods in the same legacy department scope.',
			'recommendation' => 'Classify intended parallel or special periods; unexplained overlap blocks calendar mapping.',
			'requires' => array('payroll_periods' => array('period_id', 'department_id', 'from_date', 'to_date')),
			'sql' => "SELECT COUNT(*) AS anomaly_count FROM {$p}payroll_periods a JOIN {$p}payroll_periods b"
				." ON a.period_id < b.period_id AND ((a.department_id = b.department_id) OR (a.department_id IS NULL AND b.department_id IS NULL))"
				." AND a.from_date <= b.to_date AND b.from_date <= a.to_date"
		),
		array(
			'key' => 'HRM.DUPLICATE.PAYSLIP_EMPLOYEE_PERIOD', 'family' => 'duplicates',
			'severity' => 'blocker', 'owner' => 'payroll_owner', 'gate' => 'blocks_affected_activation',
			'description' => 'Duplicate nonvoided payslip groups for one employee and payroll period.',
			'recommendation' => 'Preserve every row and reconcile which result is authoritative; do not delete history.',
			'requires' => array('payslips' => array('payroll_period_id', 'employee_id', 'status')),
			'sql' => "SELECT COUNT(*) AS anomaly_count FROM (SELECT 1 FROM {$p}payslips WHERE COALESCE(status, 0) <> 6 GROUP BY payroll_period_id, employee_id HAVING COUNT(*) > 1) profile_groups"
		),
		array(
			'key' => 'HRM.DUPLICATE.ATTENDANCE_DAY', 'family' => 'duplicates',
			'severity' => 'high', 'owner' => 'hr_data_owner', 'gate' => 'review_required',
			'description' => 'Duplicate attendance groups for one employee and date.',
			'recommendation' => 'Determine the authoritative fact and preserve source provenance.',
			'requires' => array('attendance' => array('employee_id', 'date')),
			'sql' => "SELECT COUNT(*) AS anomaly_count FROM (SELECT 1 FROM {$p}attendance GROUP BY employee_id, date HAVING COUNT(*) > 1) profile_groups"
		),
		array(
			'key' => 'HRM.OVERLAP.APPROVED_LEAVE', 'family' => 'overlaps',
			'severity' => 'high', 'owner' => 'hr_data_owner', 'gate' => 'review_required',
			'description' => 'Overlapping legacy approved leave requests for one employee.',
			'recommendation' => 'Classify duplicates and valid concurrent leave without changing approved source rows.',
			'requires' => array('leave_requests' => array('request_id', 'employee_id', 'from_date', 'to_date', 'status')),
			'sql' => "SELECT COUNT(*) AS anomaly_count FROM {$p}leave_requests a JOIN {$p}leave_requests b"
				." ON a.request_id < b.request_id AND a.employee_id = b.employee_id AND a.status = 1 AND b.status = 1"
				." AND a.from_date <= b.to_date AND b.from_date <= a.to_date"
		),
		array(
			'key' => 'HRM.STATUS.PAYROLL_INVALID', 'family' => 'status',
			'severity' => 'blocker', 'owner' => 'payroll_owner', 'gate' => 'blocks_affected_activation',
			'description' => 'Payroll period or payslip rows with status values outside the verified legacy numeric domain.',
			'recommendation' => 'Map every status explicitly and retain unknown states as blocking exceptions.',
			'requires' => array('payroll_periods' => array('status'), 'payslips' => array('status')),
			'sql' => "SELECT (SELECT COUNT(*) FROM {$p}payroll_periods WHERE status NOT IN (0,1,2,3,4,5,6))"
				." + (SELECT COUNT(*) FROM {$p}payslips WHERE status NOT IN (0,1,2,3,4,5,6)) AS anomaly_count"
		),
		array(
			'key' => 'HRM.STATUS.PERIOD_PAYSLIP_MISMATCH', 'family' => 'status',
			'severity' => 'blocker', 'owner' => 'payroll_owner', 'gate' => 'blocks_affected_activation',
			'description' => 'Linked period and payslip states that disagree on void or paid truth.',
			'recommendation' => 'Reconcile status against GL and payment evidence; never infer approval from a later flag.',
			'requires' => array('payroll_periods' => array('period_id', 'status'), 'payslips' => array('payroll_period_id', 'status', 'payable_amount')),
			'sql' => "SELECT COUNT(*) AS anomaly_count FROM {$p}payslips p JOIN {$p}payroll_periods pp ON pp.period_id = p.payroll_period_id"
				." WHERE (pp.status = 6 AND p.status <> 6) OR (pp.status <> 6 AND p.status = 6)"
				." OR (pp.status = 4 AND p.payable_amount > 0 AND (p.status < 3 OR p.status >= 6))"
		),
		array(
			'key' => 'HRM.RESULT.PAYSLIP_ARITHMETIC_MISMATCH', 'family' => 'results',
			'severity' => 'blocker', 'owner' => 'payroll_owner', 'gate' => 'blocks_affected_activation',
			'description' => 'Nonvoided payslip headers whose gross, deduction, net, or payable arithmetic differs beyond 0.01.',
			'recommendation' => 'Preserve the historical row, classify the difference, and block certified target history use.',
			'requires' => array('payslips' => array('gross_salary', 'total_deductions', 'net_salary', 'payable_amount', 'status')),
			'sql' => "SELECT COUNT(*) AS anomaly_count FROM {$p}payslips WHERE COALESCE(status, 0) <> 6"
				." AND (ABS(COALESCE(net_salary, 0) - (COALESCE(gross_salary, 0) - COALESCE(total_deductions, 0))) > 0.01"
				." OR ABS(COALESCE(payable_amount, 0) - COALESCE(net_salary, 0)) > 0.01)"
		),
		array(
			'key' => 'HRM.RESULT.PERIOD_TOTAL_MISMATCH', 'family' => 'results',
			'severity' => 'blocker', 'owner' => 'payroll_owner', 'gate' => 'blocks_affected_activation',
			'description' => 'Payroll-period totals that do not reconcile to linked nonvoided payslips within 0.01.',
			'recommendation' => 'Retain both values, classify the variance, and do not rewrite historical totals automatically.',
			'requires' => array(
				'payroll_periods' => array('period_id', 'total_gross', 'total_deductions', 'total_net', 'total_employer_cost'),
				'payslips' => array('payroll_period_id', 'gross_salary', 'total_deductions', 'net_salary', 'total_cost', 'status')
			),
			'sql' => "SELECT COUNT(*) AS anomaly_count FROM {$p}payroll_periods pp LEFT JOIN ("
				."SELECT payroll_period_id, SUM(COALESCE(gross_salary,0)) gross_total, SUM(COALESCE(total_deductions,0)) deduction_total, SUM(COALESCE(net_salary,0)) net_total, SUM(COALESCE(total_cost,0)) cost_total"
				." FROM {$p}payslips WHERE COALESCE(status,0) <> 6 GROUP BY payroll_period_id"
				.") p ON p.payroll_period_id = pp.period_id"
				." WHERE ABS(COALESCE(pp.total_gross,0) - COALESCE(p.gross_total,0)) > 0.01"
				." OR ABS(COALESCE(pp.total_deductions,0) - COALESCE(p.deduction_total,0)) > 0.01"
				." OR ABS(COALESCE(pp.total_net,0) - COALESCE(p.net_total,0)) > 0.01"
				." OR ABS(COALESCE(pp.total_employer_cost,0) - COALESCE(p.cost_total,0)) > 0.01"
		),
		array(
			'key' => 'HRM.DOWNSTREAM.PAYROLL_GL_MISSING', 'family' => 'downstream',
			'severity' => 'blocker', 'owner' => 'finance_owner', 'gate' => 'blocks_affected_activation',
			'description' => 'Posted or paid payslips without a matching legacy payroll GL transaction.',
			'recommendation' => 'Investigate the existing reference; never create or repost a historical journal during profiling.',
			'requires' => array('payslips' => array('trans_no', 'status'), 'gl_trans' => array('type', 'type_no')),
			'sql' => "SELECT COUNT(*) AS anomaly_count FROM {$p}payslips p WHERE p.status IN (2,3,4,5)"
				." AND NOT EXISTS (SELECT 1 FROM {$p}gl_trans g WHERE g.type = 80 AND g.type_no = p.trans_no)"
		),
		array(
			'key' => 'HRM.DOWNSTREAM.PAYROLL_GL_UNBALANCED', 'family' => 'downstream',
			'severity' => 'blocker', 'owner' => 'finance_owner', 'gate' => 'blocks_affected_activation',
			'description' => 'Legacy payroll GL transaction groups that do not balance within 0.01.',
			'recommendation' => 'Reconcile existing lines and linked source; do not insert balancing history.',
			'requires' => array('gl_trans' => array('type', 'type_no', 'amount')),
			'sql' => "SELECT COUNT(*) AS anomaly_count FROM (SELECT type_no FROM {$p}gl_trans WHERE type = 80 GROUP BY type_no HAVING ABS(SUM(COALESCE(amount,0))) > 0.01) profile_groups"
		),
		array(
			'key' => 'HRM.DOWNSTREAM.PAYMENT_REFERENCE_MISSING', 'family' => 'downstream',
			'severity' => 'blocker', 'owner' => 'treasury_owner', 'gate' => 'blocks_affected_activation',
			'description' => 'Positive paid payslips with no payment reference or no matching bank transaction.',
			'recommendation' => 'Reconcile amount, account, date, and reference; never fabricate a payment.',
			'requires' => array('payslips' => array('status', 'payable_amount', 'payment_trans_no'), 'bank_trans' => array('type', 'trans_no')),
			'sql' => "SELECT COUNT(*) AS anomaly_count FROM {$p}payslips p WHERE p.status IN (3,4,5) AND p.payable_amount > 0"
				." AND (p.payment_trans_no IS NULL OR p.payment_trans_no = 0 OR NOT EXISTS (SELECT 1 FROM {$p}bank_trans b WHERE b.type = 1 AND b.trans_no = p.payment_trans_no))"
		),
		array(
			'key' => 'HRM.DOWNSTREAM.PAYMENT_GL_UNBALANCED', 'family' => 'downstream',
			'severity' => 'blocker', 'owner' => 'finance_owner', 'gate' => 'blocks_affected_activation',
			'description' => 'Referenced payroll payment GL groups that do not balance within 0.01.',
			'recommendation' => 'Use controlled reconciliation and reversal handling; never edit posted GL lines.',
			'requires' => array('payslips' => array('payment_trans_no'), 'gl_trans' => array('type', 'type_no', 'amount')),
			'sql' => "SELECT COUNT(*) AS anomaly_count FROM (SELECT g.type_no FROM {$p}gl_trans g JOIN {$p}payslips p ON p.payment_trans_no = g.type_no WHERE g.type = 1 GROUP BY g.type_no HAVING ABS(SUM(COALESCE(g.amount,0))) > 0.01) profile_groups"
		),
		array(
			'key' => 'HRM.ATTACHMENT.BROKEN_LINK', 'family' => 'attachments',
			'severity' => 'high', 'owner' => 'security_privacy_owner', 'gate' => 'review_required',
			'description' => 'Employee documents whose nonnull core attachment link does not resolve.',
			'recommendation' => 'Quarantine the mapping and retain legacy metadata until content is verified.',
			'requires' => array('employee_documents' => array('attachment_id'), 'attachments' => array('id')),
			'sql' => "SELECT COUNT(*) AS anomaly_count FROM {$p}employee_documents d LEFT JOIN {$p}attachments a ON a.id = d.attachment_id WHERE d.attachment_id IS NOT NULL AND a.id IS NULL"
		),
		array(
			'key' => 'HRM.ATTACHMENT.TYPE_OR_OWNER_MISMATCH', 'family' => 'attachments',
			'severity' => 'blocker', 'owner' => 'security_privacy_owner', 'gate' => 'blocks_affected_activation',
			'description' => 'Employee documents linked to the wrong attachment type or employee owner.',
			'recommendation' => 'Deny the content link until both object type and employee ownership reconcile.',
			'requires' => array(
				'employee_documents' => array('attachment_id', 'employee_id'),
				'attachments' => array('id', 'type_no', 'trans_no'), 'employees' => array('employee_id', 'employee_number')
			),
			'sql' => "SELECT COUNT(*) AS anomaly_count FROM {$p}employee_documents d JOIN {$p}attachments a ON a.id = d.attachment_id LEFT JOIN {$p}employees e ON e.employee_id = d.employee_id WHERE a.type_no <> 46 OR e.employee_number IS NULL OR a.trans_no <> e.employee_number"
		),
		array(
			'key' => 'HRM.ATTACHMENT.ORPHAN_CORE_RECORD', 'family' => 'attachments',
			'severity' => 'high', 'owner' => 'security_privacy_owner', 'gate' => 'review_required',
			'description' => 'Core employee attachment records with no employee-document link.',
			'recommendation' => 'Inventory content and retention state before any cleanup decision.',
			'requires' => array('employee_documents' => array('attachment_id'), 'attachments' => array('id', 'type_no')),
			'sql' => "SELECT COUNT(*) AS anomaly_count FROM {$p}attachments a WHERE a.type_no = 46 AND NOT EXISTS (SELECT 1 FROM {$p}employee_documents d WHERE d.attachment_id = a.id)"
		),
		array(
			'key' => 'HRM.ATTACHMENT.LEGACY_PATH_ONLY', 'family' => 'attachments',
			'severity' => 'high', 'owner' => 'security_privacy_owner', 'gate' => 'review_required',
			'description' => 'Employee documents that retain only a legacy raw file path.',
			'recommendation' => 'Keep authorized fallback read-only and route the later attachment item through inventory, hash, quarantine, and restore testing.',
			'requires' => array('employee_documents' => array('file_path', 'attachment_id')),
			'sql' => "SELECT COUNT(*) AS anomaly_count FROM {$p}employee_documents WHERE TRIM(COALESCE(file_path, '')) <> '' AND attachment_id IS NULL"
		),
		array(
			'key' => 'HRM.ATTACHMENT.DATE_INVALID', 'family' => 'dates',
			'severity' => 'medium', 'owner' => 'hr_data_owner', 'gate' => 'review_required',
			'description' => 'Employee document rows whose expiry date precedes the issue date.',
			'recommendation' => 'Correct or explicitly accept the metadata exception without altering document bytes.',
			'requires' => array('employee_documents' => array('issue_date', 'expiry_date')),
			'sql' => "SELECT COUNT(*) AS anomaly_count FROM {$p}employee_documents WHERE issue_date IS NOT NULL AND expiry_date IS NOT NULL AND issue_date <> '0000-00-00' AND expiry_date <> '0000-00-00' AND expiry_date < issue_date"
		)
	);
}

/**
 * Return whether all tables and columns required by one check are available.
 *
 * @param array $requires
 * @param array $columns_by_table
 * @return array
 */
function hrm_migration_profile_requirements($requires, $columns_by_table) {
	$missing = array();
	foreach ($requires as $table_name => $columns) {
		if (!isset($columns_by_table[$table_name])) {
			$missing[] = $table_name;
			continue;
		}
		foreach ($columns as $column_name) {
			if (!isset($columns_by_table[$table_name][$column_name]))
				$missing[] = $table_name.'.'.$column_name;
		}
	}

	return array('available' => empty($missing), 'missing' => $missing);
}

/**
 * Execute all deterministic aggregate checks.
 *
 * @param array $columns_by_table
 * @return array
 */
function hrm_migration_profile_run_checks($columns_by_table) {
	$findings = array();
	foreach (hrm_migration_profile_check_catalog() as $check) {
		$requirements = hrm_migration_profile_requirements($check['requires'], $columns_by_table);
		$finding = array(
			'key' => $check['key'], 'family' => $check['family'],
			'severity' => $check['severity'], 'owner' => $check['owner'],
			'gate' => $check['gate'], 'description' => $check['description'],
			'recommendation' => $check['recommendation'], 'count' => 0,
			'status' => 'pass', 'query_hash' => hash('sha256', preg_replace('/\s+/', ' ', trim($check['sql'])))
		);

		if (!$requirements['available']) {
			$finding['status'] = 'skipped';
			$finding['missing_sources'] = $requirements['missing'];
			$findings[] = $finding;
			continue;
		}

		$result = hrm_migration_profile_query($check['sql'], 'could not execute HRM migration anomaly check');
		$row = $result ? db_fetch_assoc($result) : false;
		if (!$row || !array_key_exists('anomaly_count', $row)) {
			$finding['status'] = 'error';
		} else {
			$finding['count'] = (int)$row['anomaly_count'];
			$finding['status'] = $finding['count'] > 0 ? 'finding' : 'pass';
		}
		$findings[] = $finding;
	}

	return $findings;
}

/**
 * Inventory formula sources as counts and hashes without returning formula text.
 *
 * @param array $columns_by_table
 * @return array
 */
function hrm_migration_profile_formula_inventory($columns_by_table) {
	$sources = array(
		'pay_elements.formula' => array('table' => 'pay_elements', 'column' => 'formula'),
		'employee_salary.formula' => array('table' => 'employee_salary', 'column' => 'formula'),
		'salary_structure.formula' => array('table' => 'salary_structure', 'column' => 'formula')
	);
	$inventory = array();
	foreach ($sources as $source_key => $source) {
		$table_name = $source['table'];
		$column_name = $source['column'];
		if (!isset($columns_by_table[$table_name][$column_name])) {
			$inventory[$source_key] = array(
				'available' => false, 'formula_count' => 0,
				'distinct_formula_count' => 0, 'formula_set_hash' => hash('sha256', '')
			);
			continue;
		}

		$count_sql = "SELECT COUNT(*) AS formula_count FROM ".TB_PREF.$table_name
			." WHERE TRIM(COALESCE(".$column_name.", '')) <> ''";
		$count_result = hrm_migration_profile_query($count_sql, 'could not count legacy HRM formulas');
		$count_row = db_fetch_assoc($count_result);

		$hash_sql = "SELECT DISTINCT SHA2(TRIM(".$column_name."), 256) AS formula_hash FROM ".TB_PREF.$table_name
			." WHERE TRIM(COALESCE(".$column_name.", '')) <> '' ORDER BY formula_hash";
		$hash_result = hrm_migration_profile_query($hash_sql, 'could not fingerprint legacy HRM formulas');
		$hashes = array();
		while ($hash_row = db_fetch_assoc($hash_result))
			$hashes[] = $hash_row['formula_hash'];

		$inventory[$source_key] = array(
			'available' => true,
			'formula_count' => $count_row ? (int)$count_row['formula_count'] : 0,
			'distinct_formula_count' => count($hashes),
			'formula_set_hash' => hash('sha256', implode('|', $hashes))
		);
	}

	return $inventory;
}

/**
 * Inventory custom database objects by count without exposing object names.
 *
 * @return array
 */
function hrm_migration_profile_database_object_inventory() {
	$runtime_prefix = hrm_migration_profile_runtime_prefix();
	$prefix_length = strlen($runtime_prefix);
	$prefix = db_escape($runtime_prefix);
	$queries = array(
		'company_table_views' => "SELECT COUNT(*) AS object_count FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA = DATABASE() AND TABLE_TYPE = 'VIEW' AND LEFT(TABLE_NAME, ".(int)$prefix_length.") = ".$prefix,
		'company_table_triggers' => "SELECT COUNT(*) AS object_count FROM INFORMATION_SCHEMA.TRIGGERS WHERE TRIGGER_SCHEMA = DATABASE() AND LEFT(EVENT_OBJECT_TABLE, ".(int)$prefix_length.") = ".$prefix,
		'database_routines' => "SELECT COUNT(*) AS object_count FROM INFORMATION_SCHEMA.ROUTINES WHERE ROUTINE_SCHEMA = DATABASE()",
		'database_events' => "SELECT COUNT(*) AS object_count FROM INFORMATION_SCHEMA.EVENTS WHERE EVENT_SCHEMA = DATABASE()"
	);

	$inventory = array();
	foreach ($queries as $key => $sql) {
		$result = hrm_migration_profile_query($sql, 'could not inventory database customization objects');
		$row = $result ? db_fetch_assoc($result) : false;
		$inventory[$key] = $row ? (int)$row['object_count'] : 0;
	}

	return $inventory;
}

/**
 * Summarize profile findings by status and severity.
 *
 * @param array $findings
 * @return array
 */
function hrm_migration_profile_summary($findings) {
	$summary = array(
		'check_count' => count($findings),
		'status_counts' => array('pass' => 0, 'finding' => 0, 'skipped' => 0, 'error' => 0),
		'finding_severity_counts' => array('blocker' => 0, 'high' => 0, 'medium' => 0, 'low' => 0),
		'blocking_finding_count' => 0
	);

	foreach ($findings as $finding) {
		$status = $finding['status'];
		if (isset($summary['status_counts'][$status]))
			$summary['status_counts'][$status]++;
		if ($status !== 'finding')
			continue;

		$severity = $finding['severity'];
		if (isset($summary['finding_severity_counts'][$severity]))
			$summary['finding_severity_counts'][$severity]++;
		if ($finding['gate'] === 'blocks_affected_activation')
			$summary['blocking_finding_count']++;
	}

	return $summary;
}

/**
 * Build the authorized, read-only current-company migration profile.
 *
 * Business rows are read in a consistent read-only transaction and the
 * transaction is always rolled back. The returned report contains aggregate
 * counts and hashes only; it does not include source identifiers, salary,
 * bank, formula text, or attachment paths.
 *
 * @param bool $require_authorization
 * @return array
 * @throws RuntimeException
 */
function hrm_run_migration_profile($require_authorization=true) {
	global $SysPrefs;

	if ($require_authorization && !hrm_migration_profile_user_authorized())
		throw new RuntimeException('Not authorized to run the HRM migration profile.');

	$original_sql_trail = isset($SysPrefs->sql_trail) ? $SysPrefs->sql_trail : false;
	if (isset($SysPrefs))
		$SysPrefs->sql_trail = false;

	$transaction_started = false;
	try {
		hrm_migration_profile_query('SET TRANSACTION READ ONLY');
		hrm_migration_profile_query('START TRANSACTION WITH CONSISTENT SNAPSHOT');
		$transaction_started = true;

		$schema = hrm_migration_profile_schema_inventory();
		$formulas = hrm_migration_profile_formula_inventory($schema['columns']);
		$database_objects = hrm_migration_profile_database_object_inventory();
		$findings = hrm_migration_profile_run_checks($schema['columns']);
		$summary = hrm_migration_profile_summary($findings);

		hrm_migration_profile_query('ROLLBACK');
		$transaction_started = false;
	} catch (Exception $exception) {
		if ($transaction_started) {
			try {
				hrm_migration_profile_query('ROLLBACK', 'could not roll back failed HRM migration profile');
			} catch (Exception $rollback_exception) {
				// Preserve the original failure; the connection closes any open transaction.
			}
		}
		if (isset($SysPrefs))
			$SysPrefs->sql_trail = $original_sql_trail;
		throw $exception;
	}
	if (isset($SysPrefs))
		$SysPrefs->sql_trail = $original_sql_trail;

	$scope = array(
		'company_index' => isset($_SESSION['wa_current_user']->company) ? (int)$_SESSION['wa_current_user']->company : 0,
		'table_prefix_hash' => hash('sha256', hrm_migration_profile_runtime_prefix())
	);
	$source_evidence = array(
		'contract_version' => HRM_MIGRATION_PROFILE_CONTRACT_VERSION,
		'scope' => $scope,
		'tables' => $schema['tables'],
		'formula_inventory' => $formulas,
		'database_object_inventory' => $database_objects
	);
	$source_fingerprint = hash('sha256', hrm_migration_profile_json($source_evidence));
	$profile_evidence = array(
		'source_fingerprint' => $source_fingerprint,
		'findings' => $findings,
		'summary' => $summary
	);

	return array(
		'contract' => array(
			'id' => HRM_MIGRATION_PROFILE_CONTRACT_ID,
			'version' => HRM_MIGRATION_PROFILE_CONTRACT_VERSION,
			'mode' => 'read-only-current-company'
		),
		'run' => array(
			'generated_at_utc' => gmdate('Y-m-d\TH:i:s\Z'),
			'actor_user_id' => hrm_migration_profile_actor_id(),
			'rollback_completed' => true
		),
		'data_policy' => array(
			'contains_raw_source_values' => false,
			'contains_direct_identifiers' => false,
			'contains_salary_or_bank_values' => false,
			'contains_formula_text' => false,
			'contains_attachment_paths' => false
		),
		'scope' => $scope,
		'inventory' => array(
			'tables' => $schema['tables'],
			'formula_sources' => $formulas,
			'database_objects' => $database_objects
		),
		'findings' => $findings,
		'summary' => $summary,
		'source_fingerprint' => $source_fingerprint,
		'profile_hash' => hash('sha256', hrm_migration_profile_json($profile_evidence))
	);
}
