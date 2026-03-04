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
$page_security = 'SA_OVERTIMEREQUEST';
$path_to_root = "../..";
include($path_to_root . "/includes/session.inc");
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/hrm/includes/db/overtime_request_db.inc');
page(_("Overtime Entry/Request"));

simple_page_mode(false);

$status_labels = array(0 => _('Pending'), 1 => _('Approved'), 2 => _('Rejected'));

if ($Mode == 'ADD_ITEM' || $Mode == 'UPDATE_ITEM') {
    if ($_POST['employee_id'] == '' || $_POST['employee_id'] == ALL_TEXT) {
        display_error(_('Employee is required.'));
        set_focus('employee_id');
    } elseif ((int)$_POST['overtime_id'] <= 0) {
        display_error(_('Overtime type is required.'));
        set_focus('overtime_id');
    } elseif (!is_date($_POST['date'])) {
        display_error(_('Date is required.'));
        set_focus('date');
    } elseif (!check_num('hours', 0)) {
        display_error(_('Hours must be greater than zero.'));
        set_focus('hours');
    } else {
        if ($selected_id != '') {
            update_overtime_request($selected_id, (int)$_POST['overtime_id'], $_POST['date'], input_num('hours'), $_POST['reason']);
            display_notification(_('Overtime request has been updated.'));
        } else {
            add_overtime_request($_POST['employee_id'], (int)$_POST['overtime_id'], $_POST['date'], input_num('hours'), $_POST['reason']);
            display_notification(_('Overtime request has been created.'));
        }
        $Mode = 'RESET';
    }
}

if ($Mode == 'Delete') {
    $request = get_overtime_request($selected_id);
    if ($request && (int)$request['status'] != 0)
        display_error(_('Only pending requests can be deleted.'));
    else {
        delete_overtime_request($selected_id);
        display_notification(_('Selected request has been deleted.'));
    }
    $Mode = 'RESET';
}

if ($Mode == 'RESET') {
    $selected_id = '';
    $_POST['selected_id'] = '';
    $_POST['employee_id'] = '';
    $_POST['overtime_id'] = 0;
    $_POST['date'] = Today();
    $_POST['hours'] = 0;
    $_POST['reason'] = '';
}

start_form();

start_table(TABLESTYLE2);
if ($selected_id != '' && $Mode == 'Edit') {
    $myrow = get_overtime_request($selected_id);
    if ($myrow && (int)$myrow['status'] == 0) {
        $_POST['employee_id'] = $myrow['employee_id'];
        $_POST['overtime_id'] = $myrow['overtime_id'];
        $_POST['date'] = sql2date($myrow['date']);
        $_POST['hours'] = qty_format($myrow['hours']);
        $_POST['reason'] = $myrow['reason'];
        hidden('selected_id', $selected_id);
    }
}

employees_list_row(_('Employee:'), 'employee_id', null, false, false, false);

$sql = "SELECT overtime_id, overtime_name FROM ".TB_PREF."overtime WHERE !inactive";
label_row(_('Overtime Type:'), combo_input('overtime_id', get_post('overtime_id', 0), $sql, 'overtime_id', 'overtime_name', array('spec_option' => _('Select overtime'), 'spec_id' => 0)));

date_row(_('Date:'), 'date');
amount_row(_('Hours:'), 'hours');
textarea_row(_('Reason:'), 'reason', null, 50, 3);
end_table(1);
submit_add_or_update_center($selected_id == '', '', 'both');

start_table(TABLESTYLE, "width='95%'");
$th = array(_('ID'), _('Employee'), _('Type'), _('Date'), _('Hours'), _('Reason'), _('Status'), _('Requested On'), '', '');
table_header($th);

$result = get_overtime_requests(null, '', '', '');
$k = 0;
while ($row = db_fetch($result)) {
    alt_table_row_color($k);
    label_cell($row['request_id']);
    label_cell($row['employee_id'] . ' ' . $row['employee_name']);
    label_cell($row['overtime_name']);
    label_cell(sql2date($row['date']));
    qty_cell($row['hours']);
    label_cell($row['reason']);
    label_cell($status_labels[(int)$row['status']]);
    label_cell(sql2date(substr($row['request_date'], 0, 10)));
    if ((int)$row['status'] == 0) {
        edit_button_cell('Edit' . $row['request_id'], _('Edit'));
        delete_button_cell('Delete' . $row['request_id'], _('Delete'));
    } else {
        label_cell('');
        label_cell('');
    }
    end_row();
}
end_table(1);

end_form();

end_page();

