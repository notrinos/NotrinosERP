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
$page_security = 'SA_PAYGRADE';
$path_to_root = "../..";
include($path_to_root . "/includes/session.inc");

include_once($path_to_root.'/includes/ui.inc');
include_once($path_to_root.'/hrm/includes/db/grade_db.inc');

page(_($help_context = 'Manage Pay Grades'));

simple_page_mode(false);

if ($Mode=='ADD_ITEM' || $Mode=='UPDATE_ITEM') {
    if (empty(trim(get_post('grade_name')))) {
        display_error(_('Pay grade name cannot be empty.'));
        set_focus('grade_name');
    }
    elseif (pay_grade_has_column('min_salary') && pay_grade_has_column('max_salary') && input_num('min_salary') > 0 && input_num('max_salary') > 0 && input_num('min_salary') > input_num('max_salary')) {
        display_error(_('Minimum salary cannot be greater than maximum salary.'));
        set_focus('min_salary');
    }
    elseif (pay_grade_has_column('mid_salary') && input_num('mid_salary') > 0) {
        $min = input_num('min_salary');
        $mid = input_num('mid_salary');
        $max = input_num('max_salary');
        if (($min > 0 && $mid < $min) || ($max > 0 && $mid > $max)) {
            display_error(_('Mid salary must be between minimum and maximum salary.'));
            set_focus('mid_salary');
        }
        else {
            $extra = array(
                'grade_code' => get_post('grade_code', ''),
                'grade_level' => get_post('grade_level', 0),
                'min_salary' => input_num('min_salary'),
                'mid_salary' => input_num('mid_salary'),
                'max_salary' => input_num('max_salary'),
                'description' => get_post('description', '')
            );

            if ($selected_id != '') {
                update_pay_grade($selected_id, get_post('grade_name'), get_post('position_id', 0), input_num('pay_amount'), $extra);
                display_notification(_('Selected pay grade has been updated.'));
            }
            else {
                add_pay_grade(get_post('grade_name'), get_post('position_id', 0), input_num('pay_amount'), $extra);
                display_notification(_('New pay grade has been added.'));
            }

            $Mode = 'RESET';
        }
    }
    else {
        $extra = array(
            'grade_code' => get_post('grade_code', ''),
            'grade_level' => get_post('grade_level', 0),
            'min_salary' => input_num('min_salary'),
            'mid_salary' => input_num('mid_salary'),
            'max_salary' => input_num('max_salary'),
            'description' => get_post('description', '')
        );

        if ($selected_id != '') {
            update_pay_grade($selected_id, get_post('grade_name'), get_post('position_id', 0), input_num('pay_amount'), $extra);
            display_notification(_('Selected pay grade has been updated.'));
        }
        else {
            add_pay_grade(get_post('grade_name'), get_post('position_id', 0), input_num('pay_amount'), $extra);
            display_notification(_('New pay grade has been added.'));
        }

        $Mode = 'RESET';
    }
}

if ($Mode == 'Delete') {
    if (key_in_foreign_table($selected_id, 'employees', 'grade_id')) {
        display_error(_('The selected pay grade cannot be deleted because it is in use by employees.'));
    }
    else {
        delete_pay_grade($selected_id);
        display_notification(_('Selected pay grade has been deleted.'));
    }
    $Mode = 'RESET';
}

if ($Mode == 'RESET') {
    $selected_id = '';
    $_POST['selected_id'] = '';
    $_POST['grade_code'] = '';
    $_POST['grade_name'] = '';
    $_POST['grade_level'] = 0;
    $_POST['min_salary'] = '';
    $_POST['mid_salary'] = '';
    $_POST['max_salary'] = '';
    $_POST['position_id'] = 0;
    $_POST['pay_amount'] = '';
    $_POST['description'] = '';
}

start_form();

start_table(TABLESTYLE, "width='75%'");
$th = array(_('ID'));
if (pay_grade_has_column('grade_code'))
    $th[] = _('Code');
$th[] = _('Grade Name');
if (pay_grade_has_column('grade_level'))
    $th[] = _('Level');
if (pay_grade_has_column('min_salary'))
    $th[] = _('Min Salary');
if (pay_grade_has_column('mid_salary'))
    $th[] = _('Mid Salary');
if (pay_grade_has_column('max_salary'))
    $th[] = _('Max Salary');
if (pay_grade_has_column('position_id'))
    $th[] = _('Position');
if (pay_grade_has_column('pay_amount'))
    $th[] = _('Base Amount');
$th[] = '';
$th[] = '';

inactive_control_column($th);
table_header($th);

$result = get_pay_grades(check_value('show_inactive'));
$k = 0;
while ($myrow = db_fetch($result)) {
    alt_table_row_color($k);

    label_cell($myrow['grade_id']);
    if (pay_grade_has_column('grade_code'))
        label_cell($myrow['grade_code']);
    label_cell($myrow['grade_name']);
    if (pay_grade_has_column('grade_level'))
        label_cell($myrow['grade_level']);
    if (pay_grade_has_column('min_salary'))
        amount_cell($myrow['min_salary']);
    if (pay_grade_has_column('mid_salary'))
        amount_cell($myrow['mid_salary']);
    if (pay_grade_has_column('max_salary'))
        amount_cell($myrow['max_salary']);
    if (pay_grade_has_column('position_id'))
        label_cell($myrow['position_id']);
    if (pay_grade_has_column('pay_amount'))
        amount_cell($myrow['pay_amount']);

    inactive_control_cell($myrow['grade_id'], $myrow['inactive'], 'pay_grades', 'grade_id');
    edit_button_cell('Edit'.$myrow['grade_id'], _('Edit'));
    delete_button_cell('Delete'.$myrow['grade_id'], _('Delete'));
    end_row();
}
inactive_control_row($th);
end_table(1);

start_table(TABLESTYLE2);

if ($selected_id != '') {
    if ($Mode == 'Edit') {
        $myrow = get_pay_grade($selected_id);
        $_POST['grade_code'] = @$myrow['grade_code'];
        $_POST['grade_name'] = $myrow['grade_name'];
        $_POST['grade_level'] = @$myrow['grade_level'];
        $_POST['min_salary'] = @$myrow['min_salary'];
        $_POST['mid_salary'] = @$myrow['mid_salary'];
        $_POST['max_salary'] = @$myrow['max_salary'];
        $_POST['position_id'] = @$myrow['position_id'];
        $_POST['pay_amount'] = @$myrow['pay_amount'];
        $_POST['description'] = @$myrow['description'];
    }
    hidden('selected_id', $selected_id);
}

if (pay_grade_has_column('grade_code'))
    text_row_ex(_('Grade Code:'), 'grade_code', 20, 20);
text_row_ex(_('Grade Name:'), 'grade_name', 40, 60);
if (pay_grade_has_column('grade_level'))
    qty_row(_('Grade Level:'), 'grade_level', get_post('grade_level', 0), null, null, 0);
if (pay_grade_has_column('min_salary'))
    amount_row(_('Minimum Salary:'), 'min_salary');
if (pay_grade_has_column('mid_salary'))
    amount_row(_('Mid Salary:'), 'mid_salary');
if (pay_grade_has_column('max_salary'))
    amount_row(_('Maximum Salary:'), 'max_salary');
if (pay_grade_has_column('position_id'))
    qty_row(_('Position ID (legacy):'), 'position_id', get_post('position_id', 0), null, null, 0);
if (pay_grade_has_column('pay_amount'))
    amount_row(_('Base Amount (legacy):'), 'pay_amount');
if (pay_grade_has_column('description'))
    textarea_row(_('Description:'), 'description', null, 50, 3);

end_table(1);

submit_add_or_update_center($selected_id == '', '', 'both');

end_form();

end_page();

