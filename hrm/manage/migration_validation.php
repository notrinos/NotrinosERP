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
$page_security = 'SA_HRSETTINGS';
$path_to_root = "../..";
include($path_to_root . "/includes/session.inc");

include_once($path_to_root.'/includes/ui.inc');
include_once($path_to_root.'/hrm/includes/db/employee_db.inc');
include_once($path_to_root.'/hrm/includes/db/payroll_db.inc');
include_once($path_to_root.'/hrm/includes/db/payslip_db.inc');
include_once($path_to_root.'/hrm/includes/db/migration_validators.inc');

header('Cache-Control: no-store, private');
header('X-Content-Type-Options: nosniff');

$profile = hrm_run_migration_profile();

if (get_post('download_profile')) {
	$filename = 'hrm-migration-profile-company-'.(int)$profile['scope']['company_index'].'-'.gmdate('Ymd-His').'.json';
	while (ob_get_level())
		ob_end_clean();
	header('Content-Type: application/json; charset=UTF-8');
	header('Content-Disposition: attachment; filename="'.$filename.'"');
	echo hrm_migration_profile_json($profile, true);
	exit;
}

page(_($help_context = 'HRM Migration Validation'));

$status_counts = $profile['summary']['status_counts'];
$finding_count = $status_counts['finding'];
$skipped_count = $status_counts['skipped'];
$error_count = $status_counts['error'];
$blocking_count = $profile['summary']['blocking_finding_count'];

if ($error_count > 0 || $skipped_count > 0)
	display_error(sprintf(_('Migration profile is incomplete: %s error(s), %s skipped check(s).'), $error_count, $skipped_count));
elseif ($blocking_count > 0)
	display_error(sprintf(_('Migration profile found %s blocking check(s). No migration or cutover is authorized.'), $blocking_count));
elseif ($finding_count > 0)
	display_warning(sprintf(_('Migration profile found %s review item(s).'), $finding_count));
else
	display_notification(_('Migration profile completed with no aggregate anomalies.'));

start_form();
submit_center('refresh_validation', _('Re-run Validation'));
submit_center('download_profile', _('Download Masked JSON Report'), true, _('Download aggregate counts and hashes only'), false);

start_table(TABLESTYLE2, "width='95%'");
$summary = array(
	sprintf(_('Passed: %s'), $status_counts['pass']),
	sprintf(_('Findings: %s'), $finding_count),
	sprintf(_('Skipped: %s'), $skipped_count),
	sprintf(_('Errors: %s'), $error_count)
);
label_row(_('Summary:'), implode(' | ', $summary));
label_row(_('Scope:'), _('Current company database; consistent read-only snapshot; aggregate evidence only.'));
label_row(_('Profile contract:'), $profile['contract']['id'].' '.$profile['contract']['version']);
label_row(_('Source fingerprint:'), $profile['source_fingerprint']);
label_row(_('Profile hash:'), $profile['profile_hash']);
label_row(_('Generated at (UTC):'), $profile['run']['generated_at_utc']);
label_row(_('Recovery:'), _('The read-only transaction was rolled back; no source or historical payroll/accounting row was written.'));
end_table(1);

start_table(TABLESTYLE, "width='95%'");
table_header(array(_('Source Table'), _('Present'), _('Rows'), _('Columns'), _('Schema Hash')));

$k = 0;
foreach ($profile['inventory']['tables'] as $table_name => $table) {
	alt_table_row_color($k);
	label_cell($table_name);
	label_cell($table['present'] ? _('Yes') : _('No'));
	label_cell((string)$table['row_count']);
	label_cell((string)$table['column_count']);
	label_cell($table['schema_hash']);
	end_row();
}
end_table(1);

start_table(TABLESTYLE, "width='95%'");
table_header(array(_('Formula Source'), _('Available'), _('Rows With Formula'), _('Distinct Formulas'), _('Formula Set Hash')));

$k = 0;
foreach ($profile['inventory']['formula_sources'] as $source_name => $formula_source) {
	alt_table_row_color($k);
	label_cell($source_name);
	label_cell($formula_source['available'] ? _('Yes') : _('No'));
	label_cell((string)$formula_source['formula_count']);
	label_cell((string)$formula_source['distinct_formula_count']);
	label_cell($formula_source['formula_set_hash']);
	end_row();
}
end_table(1);

start_table(TABLESTYLE, "width='95%'");
table_header(array(_('Validation Check'), _('Family'), _('Status'), _('Count'), _('Severity'), _('Owner'), _('Details')));

$k = 0;
foreach ($profile['findings'] as $finding) {
	alt_table_row_color($k);
	label_cell($finding['key']);
	label_cell(ucfirst($finding['family']));
	label_cell(ucfirst($finding['status']));
	label_cell((string)$finding['count']);
	label_cell(ucfirst($finding['severity']));
	label_cell($finding['owner']);
	$details = _($finding['description']).' '._($finding['recommendation']);
	if (isset($finding['missing_sources']))
		$details .= ' '.sprintf(_('Missing source: %s'), implode(', ', $finding['missing_sources']));
	label_cell($details);
	end_row();
}

end_table(1);
end_form();
end_page();
