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
$page_security = 'SA_EMPHISTORY';
$path_to_root = "../..";
include($path_to_root . "/includes/session.inc");
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/hrm/includes/hrm_constants.inc');
include_once($path_to_root . '/hrm/includes/hrm_ui.inc');
include_once($path_to_root . '/hrm/includes/db/employee_history_db.inc');

page(_("Employee History"));

if (!isset($_POST['employee_id']))
    $_POST['employee_id'] = '';
if (!isset($_POST['change_type']))
    $_POST['change_type'] = '';

$change_types = array(
    '' => _('-- All --'),
    HRM_HIST_HIRE => _('Hired'),
    HRM_HIST_TRANSFER => _('Transfer'),
    HRM_HIST_PROMOTION => _('Promotion'),
    HRM_HIST_SALARY_CHANGE => _('Salary Change'),
    HRM_HIST_GRADE_CHANGE => _('Grade Change'),
    HRM_HIST_SEPARATION => _('Separation'),
    HRM_HIST_SUSPENSION => _('Suspension'),
    HRM_HIST_REINSTATE => _('Reinstatement')
);

start_form();
start_table(TABLESTYLE2);
employees_list_row(_('Employee:'), 'employee_id', null, true, true, false);
array_selector_row(_('Change Type:'), 'change_type', null, $change_types);
end_table(1);
submit_center('Refresh', _('Refresh'));

if ($_POST['employee_id'] != '' && $_POST['employee_id'] != ALL_TEXT) {
    start_table(TABLESTYLE, "width='95%'");
    $th = array(_('Date'), _('Type'), _('Old Dept'), _('New Dept'), _('Old Position'), _('New Position'), _('Old Grade'), _('New Grade'), _('Old Salary'), _('New Salary'), _('Reason'));
    table_header($th);

    $result = get_employee_history($_POST['employee_id'], $_POST['change_type']);
    $k = 0;
    while ($row = db_fetch($result)) {
        alt_table_row_color($k);
        label_cell(sql2date($row['effective_date']));
        label_cell(get_change_type_label($row['change_type']));
        label_cell($row['old_department_name']);
        label_cell($row['new_department_name']);
        label_cell($row['old_position_name']);
        label_cell($row['new_position_name']);
        label_cell($row['old_grade_name']);
        label_cell($row['new_grade_name']);
        label_cell($row['old_salary'] === null ? '' : price_format($row['old_salary']));
        label_cell($row['new_salary'] === null ? '' : price_format($row['new_salary']));
        label_cell($row['reason']);
        end_row();
    }
    end_table(1);
}

end_form();

end_page();

