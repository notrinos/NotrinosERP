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
$page_security = 'SA_ATTDEDUCTRULE';
$path_to_root = "../..";
include($path_to_root . "/includes/session.inc");
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/hrm/includes/db/attendance_deduction_db.inc');
page(_("Attendance Deduction Rules"));

simple_page_mode(false);

$rule_types = array(
    0 => _('Absence count based'),
    1 => _('Late minutes based')
);

$day_options = array(
    '' => _('All Days'),
    0 => _('Sunday'),
    1 => _('Monday'),
    2 => _('Tuesday'),
    3 => _('Wednesday'),
    4 => _('Thursday'),
    5 => _('Friday'),
    6 => _('Saturday')
);

if ($Mode == 'ADD_ITEM' || $Mode == 'UPDATE_ITEM') {
    if (!check_num('from_value', 0)) {
        display_error(_('From value must be a valid number.'));
        set_focus('from_value');
    } elseif (!check_num('to_value', 0)) {
        display_error(_('To value must be a valid number.'));
        set_focus('to_value');
    } elseif (!check_num('deduction_rate', 0)) {
        display_error(_('Deduction rate must be a valid number.'));
        set_focus('deduction_rate');
    } else {
        $day_of_week = get_post('day_of_week', '');
        if ($day_of_week === '')
            $day_of_week = null;

        if ($selected_id != '') {
            update_attendance_deduction_rule($selected_id, (int)$_POST['rule_type'], input_num('from_value'), input_num('to_value'), input_num('deduction_rate'), $day_of_week, input_num('work_hours'), 0);
            display_notification(_('Attendance deduction rule has been updated.'));
        } else {
            add_attendance_deduction_rule((int)$_POST['rule_type'], input_num('from_value'), input_num('to_value'), input_num('deduction_rate'), $day_of_week, input_num('work_hours'));
            display_notification(_('Attendance deduction rule has been added.'));
        }
        $Mode = 'RESET';
    }
}

if ($Mode == 'Delete') {
    delete_attendance_deduction_rule($selected_id);
    display_notification(_('Selected rule has been deleted.'));
    $Mode = 'RESET';
}

if ($Mode == 'RESET') {
    $selected_id = '';
    $_POST['selected_id'] = '';
    $_POST['rule_type'] = 0;
    $_POST['from_value'] = 0;
    $_POST['to_value'] = 0;
    $_POST['deduction_rate'] = 0;
    $_POST['day_of_week'] = '';
    $_POST['work_hours'] = 8;
}

start_form();

start_table(TABLESTYLE, "width='95%'");
$th = array(_('ID'), _('Type'), _('From'), _('To'), _('Deduction Rate (days)'), _('Day'), _('Work Hours'), '', '');
table_header($th);
$result = get_attendance_deduction_rules(check_value('show_inactive'));
$k = 0;
while ($row = db_fetch($result)) {
    alt_table_row_color($k);
    label_cell($row['rule_id']);
    label_cell($rule_types[(int)$row['rule_type']]);
    qty_cell($row['from_value']);
    qty_cell($row['to_value']);
    qty_cell($row['deduction_rate']);
    label_cell(isset($day_options[(string)$row['day_of_week']]) ? $day_options[(string)$row['day_of_week']] : $day_options['']);
    qty_cell($row['work_hours']);
    edit_button_cell('Edit' . $row['rule_id'], _('Edit'));
    delete_button_cell('Delete' . $row['rule_id'], _('Delete'));
    end_row();
}
end_table(1);

start_table(TABLESTYLE2);
if ($selected_id != '' && $Mode == 'Edit') {
    $myrow = get_attendance_deduction_rule($selected_id);
    $_POST['rule_type'] = (int)$myrow['rule_type'];
    $_POST['from_value'] = qty_format($myrow['from_value']);
    $_POST['to_value'] = qty_format($myrow['to_value']);
    $_POST['deduction_rate'] = qty_format($myrow['deduction_rate']);
    $_POST['day_of_week'] = is_null($myrow['day_of_week']) ? '' : (string)$myrow['day_of_week'];
    $_POST['work_hours'] = qty_format($myrow['work_hours']);
    hidden('selected_id', $selected_id);
}

array_selector_row(_('Rule Type:'), 'rule_type', null, $rule_types);
amount_row(_('From Value:'), 'from_value');
amount_row(_('To Value:'), 'to_value');
amount_row(_('Deduction Rate (days):'), 'deduction_rate');
array_selector_row(_('Day of Week:'), 'day_of_week', null, $day_options);
amount_row(_('Work Hours:'), 'work_hours');
end_table(1);
submit_add_or_update_center($selected_id == '', '', 'both');

end_form();

end_page();

