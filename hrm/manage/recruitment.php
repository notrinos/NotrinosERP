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
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/hrm/includes/hrm_db.inc');

page(_("Recruitment"));

/**
 * Get recruitment status labels.
 *
 * @return array
 */
function recruitment_statuses() {
    return array(0 => _('Open'), 1 => _('On Hold'), 2 => _('Closed'), 3 => _('Cancelled'));
}

/**
 * Get applicant status labels.
 *
 * @return array
 */
function applicant_statuses() {
    return array(0 => _('New'), 1 => _('Screened'), 2 => _('Interviewed'), 3 => _('Offered'), 4 => _('Hired'), 5 => _('Rejected'));
}

if (!isset($_POST['opening_date']))
    $_POST['opening_date'] = Today();
if (!isset($_POST['applied_date']))
    $_POST['applied_date'] = Today();

if (isset($_POST['add_opening'])) {
    if (trim(get_post('job_title')) == '')
        display_error(_('Job title is required.'));
    elseif (!is_date(get_post('opening_date')))
        display_error(_('Opening date is invalid.'));
    else {
        add_recruitment_opening(array(
            'job_title' => get_post('job_title'),
            'department_id' => get_post('department_id', 0),
            'position_id' => get_post('position_id', 0),
            'headcount' => input_num('headcount', 1),
            'opening_date' => get_post('opening_date'),
            'closing_date' => get_post('closing_date', ''),
            'status' => get_post('opening_status', 0),
            'description' => get_post('opening_description', '')
        ));
        display_notification(_('Recruitment opening has been added.'));
    }
}

if (isset($_POST['add_applicant'])) {
    if (trim(get_post('full_name')) == '')
        display_error(_('Applicant name is required.'));
    elseif (!is_date(get_post('applied_date')))
        display_error(_('Applied date is invalid.'));
    else {
        add_recruitment_applicant(array(
            'opening_id' => get_post('opening_id', 0),
            'full_name' => get_post('full_name'),
            'email' => get_post('email', ''),
            'mobile' => get_post('mobile', ''),
            'source' => get_post('source', ''),
            'applied_date' => get_post('applied_date'),
            'status' => get_post('applicant_status', 0),
            'expected_salary' => get_post('expected_salary', ''),
            'remarks' => get_post('remarks', '')
        ));
        display_notification(_('Applicant has been added.'));
    }
}

start_form();

display_heading(_('Recruitment Openings'));
start_table(TABLESTYLE2, "width='85%'");
text_row_ex(_('Job Title:'), 'job_title', 50, 120);
departments_list_row(_('Department:'), 'department_id', null, true, _('None'));
$text_sql = "SELECT position_id, position_name FROM ".TB_PREF."positions WHERE !inactive";
label_row(_('Position:'), combo_input('position_id', get_post('position_id', 0), $text_sql, 'position_id', 'position_name', array('spec_option' => _('None'), 'spec_id' => 0)));
qty_row(_('Headcount:'), 'headcount', get_post('headcount', 1), null, null, 1);
date_row(_('Opening Date:'), 'opening_date');
date_row(_('Closing Date:'), 'closing_date');
label_row(_('Status:'), array_selector('opening_status', get_post('opening_status', 0), recruitment_statuses()));
textarea_row(_('Description:'), 'opening_description', get_post('opening_description', ''), 50, 2);
end_table(1);
submit_center('add_opening', _('Add Opening'));

start_table(TABLESTYLE, "width='95%'");
table_header(array(_('ID'), _('Job Title'), _('Department'), _('Position'), _('Headcount'), _('Opening Date'), _('Status')));
$statuses = recruitment_statuses();
$rows = get_recruitment_openings();
$k = 0;
while ($row = db_fetch($rows)) {
    alt_table_row_color($k);
    label_cell($row['opening_id']);
    label_cell($row['job_title']);
    label_cell($row['department_name']);
    label_cell($row['position_name']);
    qty_cell($row['headcount']);
    label_cell(sql2date($row['opening_date']));
    label_cell(isset($statuses[(int)$row['status']]) ? $statuses[(int)$row['status']] : $row['status']);
    end_row();
}
end_table(2);

$opening_sql = "SELECT opening_id, job_title FROM ".TB_PREF."recruitment_openings WHERE status IN (0,1)";

display_heading(_('Applicants'));
start_table(TABLESTYLE2, "width='85%'");
label_row(_('Opening:'), combo_input('opening_id', get_post('opening_id', 0), $opening_sql, 'opening_id', 'job_title', array('spec_option' => _('General Pool'), 'spec_id' => 0)));
text_row_ex(_('Full Name:'), 'full_name', 40, 140);
text_row_ex(_('Email:'), 'email', 40, 120);
text_row_ex(_('Mobile:'), 'mobile', 30, 40);
text_row_ex(_('Source:'), 'source', 30, 80);
date_row(_('Applied Date:'), 'applied_date');
amount_row(_('Expected Salary:'), 'expected_salary');
label_row(_('Status:'), array_selector('applicant_status', get_post('applicant_status', 0), applicant_statuses()));
textarea_row(_('Remarks:'), 'remarks', get_post('remarks', ''), 50, 2);
end_table(1);
submit_center('add_applicant', _('Add Applicant'));

start_table(TABLESTYLE, "width='95%'");
table_header(array(_('ID'), _('Opening'), _('Applicant'), _('Applied Date'), _('Status'), _('Expected Salary')));
$app_status = applicant_statuses();
$app_rows = get_recruitment_applicants();
$k = 0;
while ($row = db_fetch($app_rows)) {
    alt_table_row_color($k);
    label_cell($row['applicant_id']);
    label_cell($row['job_title']);
    label_cell($row['full_name']);
    label_cell(sql2date($row['applied_date']));
    label_cell(isset($app_status[(int)$row['status']]) ? $app_status[(int)$row['status']] : $row['status']);
    amount_cell($row['expected_salary']);
    end_row();
}
end_table(1);

end_form();
end_page();
