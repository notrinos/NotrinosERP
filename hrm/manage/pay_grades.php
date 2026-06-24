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
include_once($path_to_root.'/hrm/includes/db/grades_entity.inc');

page(_($help_context = 'Manage Pay Grades'));

simple_page_mode(false);

if ($Mode=='ADD_ITEM' || $Mode=='UPDATE_ITEM') {
    if (empty(trim(get_post('grade_name')))) {
        display_error(_('Pay grade name cannot be empty.'));
        set_focus('grade_name');
    }
    elseif (input_num('min_salary') > 0 && input_num('max_salary') > 0 && input_num('min_salary') > input_num('max_salary')) {
        display_error(_('Minimum salary cannot be greater than maximum salary.'));
        set_focus('min_salary');
    }
    elseif (input_num('mid_salary') > 0) {
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

            $data = array(
                'grade_name' => get_post('grade_name'),
                'grade_code' => get_post('grade_code', ''),
                'grade_level' => get_post('grade_level', 0),
                'min_salary' => input_num('min_salary'),
                'mid_salary' => input_num('mid_salary'),
                'max_salary' => input_num('max_salary'),
                'description' => get_post('description', '')
            );

            if ($selected_id != '') {
                grades_entity::modify($selected_id, $data);
                display_notification(_('Selected pay grade has been updated.'));
            }
            else {
                grades_entity::create($data);
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

        $data = array_merge(array(
            'grade_name' => get_post('grade_name'),
            'grade_code' => get_post('grade_code', ''),
            'grade_level' => get_post('grade_level', 0),
            'min_salary' => input_num('min_salary'),
            'mid_salary' => input_num('mid_salary'),
            'max_salary' => input_num('max_salary'),
            'description' => get_post('description', '')
        ), $extra);

        if ($selected_id != '') {
            grades_entity::modify($selected_id, $data);
            display_notification(_('Selected pay grade has been updated.'));
        }
        else {
            grades_entity::create($data);
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
        grades_entity::remove($selected_id);
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
    $_POST['description'] = '';
}

start_form();

start_table(TABLESTYLE, "width='75%'");
$th = array(_('ID'), _('Code'), _('Grade Name'), _('Level'), _('Min Salary'), _('Mid Salary'), _('Max Salary'), '', '');

inactive_control_column($th);
table_header($th);

$result = grades_entity::all_db_resource(check_value('show_inactive') ? '1=1' : '!inactive', array('grade_level', 'grade_name'));
$k = 0;
while ($myrow = db_fetch($result)) {
    alt_table_row_color($k);

    label_cell($myrow['grade_id']);
    label_cell($myrow['grade_code']);
    label_cell($myrow['grade_name']);
    label_cell($myrow['grade_level']);
    amount_cell($myrow['min_salary']);
    amount_cell($myrow['mid_salary']);
    amount_cell($myrow['max_salary']);

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
        $myrow = grades_entity::find($selected_id);
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

text_row_ex(_('Grade Code:'), 'grade_code', 20, 20);
text_row_ex(_('Grade Name:'), 'grade_name', 40, 60);
qty_row(_('Grade Level:'), 'grade_level', get_post('grade_level', 0), null, null, 0);
amount_row(_('Minimum Salary:'), 'min_salary');
amount_row(_('Mid Salary:'), 'mid_salary');
amount_row(_('Maximum Salary:'), 'max_salary');
textarea_row(_('Description:'), 'description', null, 50, 3);

end_table(1);

submit_add_or_update_center($selected_id == '', '', 'both');

end_form();

end_page();

