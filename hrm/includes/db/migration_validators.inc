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
	$results[] = hrm_validate_orphan_links('employee_history', 'employee_id', 'employees', 'employee_id', _('Employee history ownership'));
	$results[] = hrm_validate_orphan_links('employee_salary', 'employee_id', 'employees', 'employee_id', _('Employee salary ownership'));
	$results[] = hrm_validate_orphan_links('payslip_details', 'payslip_id', payslip_header_table() ? payslip_header_table() : 'payslips', payslip_header_table() && payslip_has_column(payslip_header_table(), 'payslip_id') ? 'payslip_id' : 'payslip_no', _('Payslip detail linkage'));
	$results[] = hrm_validate_orphan_links(payslip_header_table() ? payslip_header_table() : 'payslips', 'employee_id', 'employees', 'employee_id', _('Payslip employee linkage'));
	$results[] = hrm_validate_salary_structure_references();

	return $results;
}
