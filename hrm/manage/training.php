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
include_once($path_to_root . '/hrm/includes/hrm_ui.inc');

page(_("Training Management"));

/**
 * Get employee training status labels.
 *
 * @return array
 */
function training_statuses() {
    return array(0 => _('Planned'), 1 => _('In Progress'), 2 => _('Completed'), 3 => _('Cancelled'));
}

if (!isset($_POST['training_date']))
    $_POST['training_date'] = Today();

if (isset($_POST['add_course'])) {
    if (trim(get_post('course_name')) == '')
        display_error(_('Course name is required.'));
    else {
        add_training_course(get_post('course_code', ''), get_post('course_name'), get_post('provider', ''), input_num('default_hours', 0), input_num('default_cost', 0));
        display_notification(_('Training course has been added.'));
    }
}

if (isset($_POST['assign_training'])) {
    if (trim(get_post('employee_id')) == '' || get_post('employee_id') == ALL_TEXT)
        display_error(_('Please select an employee.'));
    elseif ((int)get_post('course_id', 0) <= 0)
        display_error(_('Please select a training course.'));
    elseif (!is_date(get_post('training_date')))
        display_error(_('Training date is invalid.'));
    else {
        add_employee_training(array(
            'employee_id' => get_post('employee_id'),
            'course_id' => get_post('course_id', 0),
            'training_date' => get_post('training_date'),
            'completion_date' => get_post('completion_date', ''),
            'status' => get_post('training_status', 0),
            'score' => get_post('score', ''),
            'cost_amount' => input_num('cost_amount', 0),
            'remarks' => get_post('training_remarks', '')
        ));
        display_notification(_('Employee training record has been added.'));
    }
}

start_form();

display_heading(_('Courses'));
start_table(TABLESTYLE2, "width='80%'");
text_row_ex(_('Course Code:'), 'course_code', 20, 30);
text_row_ex(_('Course Name:'), 'course_name', 50, 140);
text_row_ex(_('Provider:'), 'provider', 40, 140);
qty_row(_('Default Hours:'), 'default_hours', get_post('default_hours', 0));
amount_row(_('Default Cost:'), 'default_cost', get_post('default_cost', 0));
end_table(1);
submit_center('add_course', _('Add Course'));

start_table(TABLESTYLE, "width='95%'");
table_header(array(_('ID'), _('Code'), _('Course Name'), _('Provider'), _('Hours'), _('Cost')));
$course_rows = get_training_courses();
$k = 0;
while ($row = db_fetch($course_rows)) {
    alt_table_row_color($k);
    label_cell($row['course_id']);
    label_cell($row['course_code']);
    label_cell($row['course_name']);
    label_cell($row['provider']);
    qty_cell($row['default_hours']);
    amount_cell($row['default_cost']);
    end_row();
}
end_table(2);

display_heading(_('Employee Training'));
start_table(TABLESTYLE2, "width='80%'");
employees_list_row(_('Employee:'), 'employee_id', null, false, false, false);
$course_sql = "SELECT course_id, course_name FROM ".TB_PREF."training_courses WHERE inactive = 0";
label_row(_('Course:'), combo_input('course_id', get_post('course_id', 0), $course_sql, 'course_id', 'course_name', array('spec_option' => _('Select course'), 'spec_id' => 0)));
date_row(_('Training Date:'), 'training_date');
date_row(_('Completion Date:'), 'completion_date');
label_row(_('Status:'), array_selector('training_status', get_post('training_status', 0), training_statuses()));
amount_row(_('Cost Amount:'), 'cost_amount', get_post('cost_amount', 0));
qty_row(_('Score:'), 'score', get_post('score', ''));
textarea_row(_('Remarks:'), 'training_remarks', get_post('training_remarks', ''), 50, 2);
end_table(1);
submit_center('assign_training', _('Add Employee Training'));

start_table(TABLESTYLE, "width='95%'");
table_header(array(_('ID'), _('Employee'), _('Course'), _('Date'), _('Status'), _('Score'), _('Cost')));
$labels = training_statuses();
$records = get_employee_training_records();
$k = 0;
while ($row = db_fetch($records)) {
    alt_table_row_color($k);
    label_cell($row['training_id']);
    label_cell($row['employee_name']);
    label_cell($row['course_name']);
    label_cell(sql2date($row['training_date']));
    label_cell(isset($labels[(int)$row['status']]) ? $labels[(int)$row['status']] : $row['status']);
    qty_cell($row['score']);
    amount_cell($row['cost_amount']);
    end_row();
}
end_table(1);

end_form();
end_page();
