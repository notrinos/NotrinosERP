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
$page_security = 'SA_WORKSHIFT';
$path_to_root = "../..";
include($path_to_root . "/includes/session.inc");
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/hrm/includes/db/work_shift_db.inc');
page(_("Work Shifts"));

simple_page_mode(false);

if ($Mode == 'ADD_ITEM' || $Mode == 'UPDATE_ITEM') {
    if (trim($_POST['shift_name']) == '') {
        display_error(_('Shift name is required.'));
        set_focus('shift_name');
    } elseif (trim($_POST['start_time']) == '' || trim($_POST['end_time']) == '') {
        display_error(_('Start and end times are required (HH:MM:SS).'));
    } elseif (!check_num('break_duration', 0) || !check_num('work_hours', 0)) {
        display_error(_('Break duration and work hours must be positive values.'));
    } else {
        if ($selected_id != '') {
            update_work_shift($selected_id, $_POST['shift_name'], $_POST['start_time'], $_POST['end_time'], input_num('break_duration'), input_num('work_hours'), check_value('is_night_shift') ? 1 : 0, 0);
            display_notification(_('Work shift has been updated.'));
        } else {
            add_work_shift($_POST['shift_name'], $_POST['start_time'], $_POST['end_time'], input_num('break_duration'), input_num('work_hours'), check_value('is_night_shift') ? 1 : 0);
            display_notification(_('Work shift has been added.'));
        }
        $Mode = 'RESET';
    }
}

if ($Mode == 'Delete') {
    delete_work_shift($selected_id);
    display_notification(_('Selected work shift has been deleted.'));
    $Mode = 'RESET';
}

if ($Mode == 'RESET') {
    $selected_id = '';
    $_POST['selected_id'] = '';
    $_POST['shift_name'] = '';
    $_POST['start_time'] = '08:00:00';
    $_POST['end_time'] = '17:00:00';
    $_POST['break_duration'] = 60;
    $_POST['work_hours'] = 8;
    $_POST['is_night_shift'] = 0;
}

start_form();

start_table(TABLESTYLE, "width='90%'");
$th = array(_('ID'), _('Shift Name'), _('Start'), _('End'), _('Break (min)'), _('Work Hours'), _('Night Shift'), '', '');
table_header($th);
$result = get_work_shifts(check_value('show_inactive'));
$k = 0;
while ($row = db_fetch($result)) {
    alt_table_row_color($k);
    label_cell($row['shift_id']);
    label_cell($row['shift_name']);
    label_cell($row['start_time']);
    label_cell($row['end_time']);
    label_cell($row['break_duration']);
    qty_cell($row['work_hours']);
    label_cell($row['is_night_shift'] ? _('Yes') : _('No'));
    edit_button_cell('Edit' . $row['shift_id'], _('Edit'));
    delete_button_cell('Delete' . $row['shift_id'], _('Delete'));
    end_row();
}
end_table(1);

start_table(TABLESTYLE2);
if ($selected_id != '' && $Mode == 'Edit') {
    $myrow = get_work_shift($selected_id);
    $_POST['shift_name'] = $myrow['shift_name'];
    $_POST['start_time'] = $myrow['start_time'];
    $_POST['end_time'] = $myrow['end_time'];
    $_POST['break_duration'] = qty_format($myrow['break_duration']);
    $_POST['work_hours'] = qty_format($myrow['work_hours']);
    $_POST['is_night_shift'] = (int)$myrow['is_night_shift'];
    hidden('selected_id', $selected_id);
}

text_row_ex(_('Shift Name:'), 'shift_name', 40, 60);
text_row(_('Start Time (HH:MM:SS):'), 'start_time', null, 10, 8);
text_row(_('End Time (HH:MM:SS):'), 'end_time', null, 10, 8);
small_amount_row(_('Break Duration (minutes):'), 'break_duration');
amount_row(_('Work Hours:'), 'work_hours');
check_row(_('Night shift:'), 'is_night_shift');
end_table(1);
submit_add_or_update_center($selected_id == '', '', 'both');

end_form();

end_page();

