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

page(_($help_context = 'HRM Migration Validation'));

$results = hrm_run_migration_validations();
$pass_count = 0;
$warn_count = 0;
$fail_count = 0;

foreach ($results as $result) {
	switch ($result['status']) {
		case 'pass':
			$pass_count++;
			break;
		case 'fail':
			$fail_count++;
			break;
		default:
			$warn_count++;
	}
}

if ($fail_count > 0)
	display_error(sprintf(_('Migration validation completed with %s failing check(s).'), $fail_count));
elseif ($warn_count > 0)
	display_warning(sprintf(_('Migration validation completed with %s warning(s).'), $warn_count));
else
	display_notification(_('Migration validation completed successfully.'));

start_form();
submit_center('refresh_validation', _('Re-run Validation'));

start_table(TABLESTYLE2, "width='95%'");
$summary = array(
	sprintf(_('Passed: %s'), $pass_count),
	sprintf(_('Warnings: %s'), $warn_count),
	sprintf(_('Failed: %s'), $fail_count)
);
	label_row(_('Summary:'), implode(' | ', $summary));
	label_row(_('Scope:'), _('Current company database with live HRM migration integrity checks.'));
end_table(1);

start_table(TABLESTYLE, "width='95%'");
table_header(array(_('Validation Check'), _('Status'), _('Details')));

$k = 0;
foreach ($results as $result) {
	alt_table_row_color($k);
	label_cell($result['title']);
	label_cell(ucfirst($result['status']));
	label_cell($result['details']);
	end_row();
}

end_table(1);
end_form();
end_page();