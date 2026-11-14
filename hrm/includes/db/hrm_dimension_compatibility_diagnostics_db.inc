<?php
/**
 * DIM-HRM-001: read-only Dimension compatibility diagnostics for HRM.
 *
 * HRM stores finance coordinates under legacy, compatibility names: `departments.cost_center_id`,
 * `hrm_organization_units.legacy_cost_center_id`, `employees.cost_center_id`, `hrm_assignments.legacy_cost_center_id` hold the native
 * Dimension category 1 coordinate (and `dimension2_id` / `legacy_dimension2_id` category 2). The column name is a historical
 * compatibility name, not a business declaration: a company that publishes a different category label (for example "Project") for
 * category 1 has not changed what these HRM columns are, and HRM must never let the two readings be confused silently.
 *
 * This adapter only reads. It
 *  - documents where each HRM coordinate lives and who owns it (`hrm_dimension_compat_storage_map()`);
 *  - states, wherever the legacy "Cost Center" name is shown, which company category that coordinate really belongs to
 *    (`hrm_dimension_compat_display_note()`), so a Project (or any other) label can never silently reinterpret Cost Center;
 *  - reports, per Department, whether the Department, its Organization Unit projection and the referenced Dimension agree, and, for a
 *    caller that also holds the Employee area, aggregate counts (never identities) of Employee / current-Assignment drift;
 *  - reports that attendance and overtime coordinate columns are retained schema evidence only: the maintained basic writers do not
 *    set them, so they are not proof of an approved time-cost allocation, and no project-hours driver exists here.
 *
 * It contains no INSERT, UPDATE, DELETE or DDL, no ledger call and no payroll reference, exposes no employee name, identifier, salary or bank
 * detail, and creates no Finance mapping: `SA_DEPARTMENT` and `SA_EMPLOYEE` are not PAY-GL mapping authority, and an effective-dated
 * Finance mapping command would need its own governed migration under the existing owner contracts (reported as not provided).
 *
 * PHP 5.6 compatible.
 */
require_once __DIR__.'/../../../dimensions/includes/dimension_category_policy.inc';

if (!defined('HRM_DIM_COMPAT_DEPARTMENT_AREA'))
	define('HRM_DIM_COMPAT_DEPARTMENT_AREA', 'SA_DEPARTMENT');
if (!defined('HRM_DIM_COMPAT_EMPLOYEE_AREA'))
	define('HRM_DIM_COMPAT_EMPLOYEE_AREA', 'SA_EMPLOYEE');
if (!defined('HRM_DIM_COMPAT_DEPARTMENT_LIMIT'))
	define('HRM_DIM_COMPAT_DEPARTMENT_LIMIT', 500);
if (!defined('HRM_DIM_COMPAT_LEGACY_NAME'))
	define('HRM_DIM_COMPAT_LEGACY_NAME', 'Cost Center');

/**
 * Where every HRM Dimension coordinate is stored, and who owns writing it.
 *
 * @return array[] Rows with table, column, axis, role and owner
 */
function hrm_dimension_compat_storage_map()
{
	return array(
		array('table' => 'departments', 'column' => 'cost_center_id', 'axis' => 1, 'role' => 'legacy_department_coordinate',
			'owner' => 'Department maintenance under the organization owner contract'),
		array('table' => 'hrm_organization_units', 'column' => 'legacy_cost_center_id', 'axis' => 1, 'role' => 'organization_projection',
			'owner' => 'Organization Unit projection of the Department'),
		array('table' => 'employees', 'column' => 'cost_center_id', 'axis' => 1, 'role' => 'employee_compatibility_projection',
			'owner' => 'Employee compatibility projection of the current Assignment; accepted only when an Employee is created'),
		array('table' => 'hrm_assignments', 'column' => 'legacy_cost_center_id', 'axis' => 1, 'role' => 'assignment_costing_compatibility',
			'owner' => 'Assignment owner contract'),
		array('table' => 'employees', 'column' => 'dimension2_id', 'axis' => 2, 'role' => 'employee_compatibility_projection',
			'owner' => 'Employee compatibility projection of the current Assignment'),
		array('table' => 'hrm_assignments', 'column' => 'legacy_dimension2_id', 'axis' => 2, 'role' => 'assignment_costing_compatibility',
			'owner' => 'Assignment owner contract'),
		array('table' => 'attendance', 'column' => 'dimension_id', 'axis' => 1, 'role' => 'retained_time_record_evidence',
			'owner' => 'none: the basic attendance writers omit it'),
		array('table' => 'attendance', 'column' => 'dimension2_id', 'axis' => 2, 'role' => 'retained_time_record_evidence',
			'owner' => 'none: the basic attendance writers omit it'),
		array('table' => 'overtime', 'column' => 'dimension_id', 'axis' => 1, 'role' => 'retained_time_record_evidence',
			'owner' => 'none: the basic overtime writers omit it'),
		array('table' => 'overtime', 'column' => 'dimension2_id', 'axis' => 2, 'role' => 'retained_time_record_evidence',
			'owner' => 'none: the basic overtime writers omit it'),
	);
}

/**
 * Label the active company published for one Dimension category, without the neutral fallback.
 *
 * @param int $axis Native category 1 or 2
 * @return string Published label, or '' when the company declared none
 */
function hrm_dimension_compat_company_label($axis)
{
	$axis = (int)$axis;
	if ($axis !== 1 && $axis !== 2)
		return '';
	$current = dimension_category_policy_current();
	return isset($current['axes'][$axis]['label']) ? trim((string)$current['axes'][$axis]['label']) : '';
}

/**
 * Plain-text note that keeps the legacy HRM "Cost Center" name from being read as another category.
 *
 * Empty when the company declared no label or declared the same name, so an unconfigured company sees no change. The caller escapes it.
 *
 * @param int $axis Native category (1 = the legacy "Cost Center" column)
 * @return string
 */
function hrm_dimension_compat_display_note($axis)
{
	$label = hrm_dimension_compat_company_label($axis);
	if ((int)$axis !== 1 || $label === '' || strcasecmp($label, HRM_DIM_COMPAT_LEGACY_NAME) === 0)
		return '';
	return sprintf(_('This legacy HRM column is named "Cost Center" but stores the company\'s "%s" category (category 1). The name does not change the category and no project or cost center meaning is inferred.'), $label);
}

/**
 * State of one stored coordinate against the Dimension master.
 *
 * @param mixed $coordinate Stored Dimension id (null / 0 = unassigned)
 * @param array|null $dimension Dimension master row (`type_`, `closed`) or null when absent
 * @param int $axis Expected native category
 * @return string unassigned|malformed|missing|wrong_axis|closed|open
 */
function hrm_dimension_compat_coordinate_state($coordinate, $dimension, $axis)
{
	if ($coordinate === null || $coordinate === '' || (string)$coordinate === '0')
		return 'unassigned';
	if (!is_numeric($coordinate) || (string)(int)$coordinate !== trim((string)$coordinate) || (int)$coordinate < 0)
		return 'malformed';
	if (!is_array($dimension))
		return 'missing';
	if ((int)$dimension['type_'] !== (int)$axis)
		return 'wrong_axis';
	return (int)$dimension['closed'] === 1 ? 'closed' : 'open';
}

/**
 * Alignment of a Department coordinate with its Organization Unit projection.
 *
 * @param mixed $department_value `departments.cost_center_id`
 * @param mixed $unit_value `hrm_organization_units.legacy_cost_center_id` (ignored when the unit is absent)
 * @param bool $unit_exists Whether the projection row exists
 * @return string org_unit_missing|aligned|mismatch
 */
function hrm_dimension_compat_alignment_state($department_value, $unit_value, $unit_exists)
{
	if (!$unit_exists)
		return 'org_unit_missing';
	$left = ($department_value === null || $department_value === '') ? '0' : (string)(int)$department_value;
	$right = ($unit_value === null || $unit_value === '') ? '0' : (string)(int)$unit_value;
	return $left === $right ? 'aligned' : 'mismatch';
}

/**
 * Which diagnostics the actor may read.
 *
 * The Department area unlocks the Department view; the Employee area additionally unlocks the aggregate Employee counts. Neither is Finance
 * mapping authority and neither is a payroll area.
 *
 * @return array departments => bool, employees => bool
 */
function hrm_dimension_compat_authority()
{
	$allowed = function_exists('user_check_access') && user_check_access(HRM_DIM_COMPAT_DEPARTMENT_AREA) ? true : false;
	return array('departments' => $allowed,
		'employees' => $allowed && user_check_access(HRM_DIM_COMPAT_EMPLOYEE_AREA) ? true : false);
}

/**
 * Is one physical column of the active company prefix present?
 *
 * @param string $table Unprefixed table name
 * @param string $column Column name
 * @return bool
 */
function hrm_dimension_compat_column_ready($table, $column)
{
	if (!preg_match('/^[a-z][a-z0-9_]*$/D', $table) || !preg_match('/^[a-z][a-z0-9_]*$/D', $column))
		return false;
	// Both names passed the plain-identifier check above, so they are embedded directly: db_escape() HTML-encodes the company-prefix token.
	$result = db_query("SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME='".TB_PREF.$table."' AND COLUMN_NAME='".$column."'",
		'could not check an HRM Dimension compatibility column');
	if (!$result)
		return false;
	$row = db_fetch_row($result);
	return $row && (int)$row[0] === 1;
}

/**
 * Department compatibility rows, bounded and without any Employee data.
 *
 * @return array[]|false Rows, or false when the Department table cannot be read
 */
function hrm_dimension_compat_department_rows()
{
	if (!hrm_dimension_compat_column_ready('departments', 'cost_center_id'))
		return false;
	$unit_ready = hrm_dimension_compat_column_ready('hrm_organization_units', 'legacy_cost_center_id');
	$sql = 'SELECT d.department_id, d.department_code, d.department_name, d.cost_center_id, d.inactive, '
		.($unit_ready ? 'o.organization_unit_id, o.legacy_cost_center_id AS unit_cost_center_id' : 'NULL AS organization_unit_id, NULL AS unit_cost_center_id')
		.', m.id AS dimension_found, m.type_ AS dimension_type, m.closed AS dimension_closed'
		.' FROM '.TB_PREF.'departments d '
		.($unit_ready ? 'LEFT JOIN '.TB_PREF.'hrm_organization_units o ON o.legacy_department_id=d.department_id ' : '')
		.'LEFT JOIN '.TB_PREF.'dimensions m ON m.id=d.cost_center_id AND d.cost_center_id>0 '
		.'ORDER BY d.department_id LIMIT '.(int)HRM_DIM_COMPAT_DEPARTMENT_LIMIT;
	$result = db_query($sql, 'could not read HRM Department Dimension compatibility');
	if (!$result)
		return false;
	$rows = array();
	while ($row = db_fetch_assoc($result)) {
		$dimension = $row['dimension_found'] === null ? null : array('type_' => $row['dimension_type'], 'closed' => $row['dimension_closed']);
		$rows[] = array(
			'department_id' => (int)$row['department_id'],
			'department_code' => (string)$row['department_code'],
			'department_name' => (string)$row['department_name'],
			'inactive' => (int)$row['inactive'],
			'coordinate' => (int)$row['cost_center_id'],
			'coordinate_state' => hrm_dimension_compat_coordinate_state($row['cost_center_id'], $dimension, 1),
			'organization_unit' => $unit_ready && $row['organization_unit_id'] !== null ? (int)$row['organization_unit_id'] : null,
			'organization_unit_coordinate' => $row['unit_cost_center_id'] === null ? null : (int)$row['unit_cost_center_id'],
			'alignment' => $unit_ready ? hrm_dimension_compat_alignment_state($row['cost_center_id'], $row['unit_cost_center_id'], $row['organization_unit_id'] !== null)
				: 'org_unit_unavailable',
		);
	}
	return $rows;
}

/**
 * Aggregate Employee against current Assignment drift. Counts only: no Employee identity, name or pay data leaves this function.
 *
 * @return array|false Counts, or false when the Employee table cannot be read
 */
function hrm_dimension_compat_employee_summary()
{
	if (!hrm_dimension_compat_column_ready('employees', 'cost_center_id') || !hrm_dimension_compat_column_ready('employees', 'dimension2_id'))
		return false;
	$assignment_ready = hrm_dimension_compat_column_ready('hrm_assignments', 'legacy_cost_center_id')
		&& hrm_dimension_compat_column_ready('hrm_assignments', 'legacy_dimension2_id');
	$sql = 'SELECT COUNT(*) AS total, '
		.'COALESCE(SUM(CASE WHEN COALESCE(e.cost_center_id,0)>0 THEN 1 ELSE 0 END),0) AS with_coordinate, '
		.'COALESCE(SUM(CASE WHEN COALESCE(e.cost_center_id,0)>0 AND m.id IS NULL THEN 1 ELSE 0 END),0) AS coordinate_missing, '
		.'COALESCE(SUM(CASE WHEN m.id IS NOT NULL AND m.type_<>1 THEN 1 ELSE 0 END),0) AS coordinate_wrong_axis, '
		.'COALESCE(SUM(CASE WHEN m.id IS NOT NULL AND m.type_=1 AND m.closed=1 THEN 1 ELSE 0 END),0) AS coordinate_closed, '
		.($assignment_ready
			? 'COALESCE(SUM(CASE WHEN a.assignment_id IS NULL THEN 1 ELSE 0 END),0) AS assignment_missing, '
				.'COALESCE(SUM(CASE WHEN a.assignment_id IS NOT NULL AND COALESCE(e.cost_center_id,0)<>COALESCE(a.legacy_cost_center_id,0) THEN 1 ELSE 0 END),0) AS axis1_drift, '
				.'COALESCE(SUM(CASE WHEN a.assignment_id IS NOT NULL AND COALESCE(e.dimension2_id,0)<>COALESCE(a.legacy_dimension2_id,0) THEN 1 ELSE 0 END),0) AS axis2_drift '
			: '0 AS assignment_missing, 0 AS axis1_drift, 0 AS axis2_drift ')
		.'FROM '.TB_PREF.'employees e '
		.($assignment_ready ? 'LEFT JOIN '.TB_PREF.'hrm_assignments a ON a.legacy_employee_number=e.employee_number AND a.legacy_current=1 ' : '')
		.'LEFT JOIN '.TB_PREF.'dimensions m ON m.id=e.cost_center_id AND e.cost_center_id>0';
	$result = db_query($sql, 'could not summarize HRM Employee Dimension compatibility');
	$row = $result ? db_fetch_assoc($result) : false;
	if (!$row)
		return false;
	$summary = array('assignment_table_ready' => $assignment_ready);
	foreach ($row as $key => $value)
		$summary[$key] = (int)$value;
	return $summary;
}

/**
 * Retained coordinate evidence on attendance and overtime, with the explicit statement that it allocates no time cost.
 *
 * @return array[] One entry per table: columns_present, rows, rows_with_coordinate
 */
function hrm_dimension_compat_time_record_summary()
{
	$summary = array();
	foreach (array('attendance', 'overtime') as $table) {
		$present = hrm_dimension_compat_column_ready($table, 'dimension_id') && hrm_dimension_compat_column_ready($table, 'dimension2_id');
		$entry = array('columns_present' => $present, 'rows' => null, 'rows_with_coordinate' => null,
			'allocation_authority' => 'none', 'project_hours_driver' => 'not_provided');
		if ($present) {
			$result = db_query('SELECT COUNT(*), COALESCE(SUM(CASE WHEN dimension_id>0 OR dimension2_id>0 THEN 1 ELSE 0 END),0) FROM '.TB_PREF.$table,
				'could not count retained HRM time-record coordinates');
			$row = $result ? db_fetch_row($result) : false;
			if ($row) {
				$entry['rows'] = (int)$row[0];
				$entry['rows_with_coordinate'] = (int)$row[1];
			}
		}
		$summary[$table] = $entry;
	}
	return $summary;
}

/**
 * Complete read-only diagnostic report.
 *
 * An actor without the Department area reads nothing: the answer is `authorized: false` before any query. The Employee aggregate is
 * present only for an actor who also holds the Employee area.
 *
 * @return array
 */
function hrm_dimension_compat_report()
{
	$authority = hrm_dimension_compat_authority();
	if (!$authority['departments'])
		return array('authorized' => false);
	$current = dimension_category_policy_current();
	$axes = array();
	foreach (array(1, 2) as $axis)
		$axes[$axis] = array('company_label' => hrm_dimension_compat_company_label($axis), 'display_label' => dimension_category_policy_label($axis),
			'legacy_name' => $axis === 1 ? HRM_DIM_COMPAT_LEGACY_NAME : 'Dimension 2', 'note' => hrm_dimension_compat_display_note($axis));
	$departments = hrm_dimension_compat_department_rows();
	$counts = array('departments' => 0, 'aligned' => 0, 'mismatch' => 0, 'org_unit_missing' => 0, 'coordinate_problem' => 0);
	if (is_array($departments))
		foreach ($departments as $row) {
			$counts['departments']++;
			if (isset($counts[$row['alignment']]))
				$counts[$row['alignment']]++;
			if (!in_array($row['coordinate_state'], array('unassigned', 'open'), true))
				$counts['coordinate_problem']++;
		}
	return array(
		'authorized' => true,
		'employee_aggregate_authorized' => $authority['employees'],
		'policy' => array('state' => $current['state'], 'version' => (int)$current['version']),
		'axes' => $axes,
		'storage' => hrm_dimension_compat_storage_map(),
		'departments' => $departments,
		'department_counts' => $counts,
		'employees' => $authority['employees'] ? hrm_dimension_compat_employee_summary() : null,
		'time_records' => hrm_dimension_compat_time_record_summary(),
		'finance_mapping_writer' => 'not_provided',
		'read_only' => true,
	);
}
