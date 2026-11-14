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
 * DIM-HRM-001: HRM Dimension compatibility diagnostics.
 *
 * Read-only. Explains where HRM stores its legacy "Cost Center" coordinate, which company category that coordinate really is, whether
 * Departments agree with their Organization Unit projection, and that attendance/overtime coordinates are retained evidence rather than an
 * approved time-cost allocation. It writes nothing, shows no Employee identity or pay data, and is not a Finance mapping tool.
 *
 * @package NotrinosERP
 * @subpackage HRM
 */
$page_security = 'SA_DEPARTMENT';
$path_to_root = '../..';
include_once($path_to_root.'/includes/session.inc');
include_once($path_to_root.'/includes/ui.inc');
include_once($path_to_root.'/hrm/includes/db/hrm_dimension_compatibility_diagnostics_db.inc');

page(_($help_context = 'HRM Dimension Compatibility'));

$report = hrm_dimension_compat_report();
if (empty($report['authorized'])) {
	display_error(_('You are not authorized to read HRM Dimension compatibility diagnostics.'));
	end_page();
	exit;
}

/** Escape one value for output. */
function hrm_dimension_compat_page_text($value)
{
	return htmlspecialchars((string)$value, ENT_QUOTES, 'UTF-8');
}

display_note(_('This page is read-only. It changes no Department, Employee, Assignment or ledger row and creates no Finance mapping.'), 0, 1);

start_table(TABLESTYLE, "width='80%'");
table_header(array(_('HRM column name'), _('Company category label'), _('Reading')));
foreach ($report['axes'] as $axis => $info) {
	start_row();
	label_cell(hrm_dimension_compat_page_text($info['legacy_name']));
	label_cell(hrm_dimension_compat_page_text($info['company_label'] === '' ? _('(no label published)') : $info['company_label']));
	label_cell($info['note'] === '' ? _('The name matches the category, or no label has been published.') : hrm_dimension_compat_page_text($info['note']));
	end_row();
}
end_table(1);

start_table(TABLESTYLE, "width='80%'");
table_header(array(_('Table'), _('Column'), _('Category'), _('Role'), _('Written by')));
foreach ($report['storage'] as $entry) {
	start_row();
	label_cell(hrm_dimension_compat_page_text($entry['table']));
	label_cell(hrm_dimension_compat_page_text($entry['column']));
	label_cell((int)$entry['axis']);
	label_cell(hrm_dimension_compat_page_text($entry['role']));
	label_cell(hrm_dimension_compat_page_text($entry['owner']));
	end_row();
}
end_table(1);

display_heading(_('Departments'));
if (!is_array($report['departments'])) {
	display_note(_('The Department table could not be read for this company.'));
} else {
	$counts = $report['department_counts'];
	display_note(sprintf(_('%d departments: %d aligned with the Organization Unit projection, %d differ, %d without a projection, %d with a coordinate that is not an open category-1 Dimension.'),
		$counts['departments'], $counts['aligned'], $counts['mismatch'], $counts['org_unit_missing'], $counts['coordinate_problem']));
	start_table(TABLESTYLE, "width='80%'");
	table_header(array(_('Code'), _('Department'), _('Coordinate'), _('Dimension state'), _('Organization Unit'), _('Alignment')));
	$k = 0;
	foreach ($report['departments'] as $row) {
		alt_table_row_color($k);
		label_cell(hrm_dimension_compat_page_text($row['department_code']));
		label_cell(hrm_dimension_compat_page_text($row['department_name']));
		label_cell($row['coordinate'] > 0 ? '#'.(int)$row['coordinate'] : '-');
		label_cell(hrm_dimension_compat_page_text($row['coordinate_state']));
		label_cell($row['organization_unit_coordinate'] === null ? '-' : '#'.(int)$row['organization_unit_coordinate']);
		label_cell(hrm_dimension_compat_page_text($row['alignment']));
		end_row();
	}
	end_table(1);
}

if (!empty($report['employee_aggregate_authorized'])) {
	display_heading(_('Employees against current Assignment (counts only)'));
	$summary = $report['employees'];
	if (!is_array($summary)) {
		display_note(_('The Employee table could not be read for this company.'));
	} else {
		start_table(TABLESTYLE2);
		foreach (array('total' => _('Employees'), 'with_coordinate' => _('With a stored coordinate'), 'coordinate_missing' => _('Coordinate refers to no Dimension'),
			'coordinate_wrong_axis' => _('Coordinate is not a category-1 Dimension'), 'coordinate_closed' => _('Coordinate is a closed Dimension'),
			'assignment_missing' => _('Without a current Assignment'), 'axis1_drift' => _('Category 1 differs from the Assignment'),
			'axis2_drift' => _('Category 2 differs from the Assignment')) as $key => $label)
			label_row($label.':', (int)$summary[$key]);
		end_table(1);
	}
}

display_heading(_('Attendance and overtime coordinates'));
display_note(_('These columns are retained schema evidence. The maintained attendance and overtime writers do not set them, so they are not an approved time-cost allocation, and no project-hours driver exists in HRM.'));
start_table(TABLESTYLE2);
foreach ($report['time_records'] as $table => $entry)
	label_row(hrm_dimension_compat_page_text($table).':', $entry['columns_present']
		? sprintf(_('%d rows, %d with a retained coordinate'), (int)$entry['rows'], (int)$entry['rows_with_coordinate'])
		: _('columns not present'));
end_table(1);

display_note(_('An effective-dated Finance mapping command is not provided here. It would need its own governed migration under the existing Department, Assignment and payroll owner contracts; the Department and Employee areas are not payroll mapping authority.'));

end_page();
