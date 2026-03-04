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
$page_security = 'SA_LEAVEREQUEST';
$path_to_root = "../..";
include($path_to_root . "/includes/session.inc");
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/hrm/includes/hrm_ui.inc');
include_once($path_to_root . '/hrm/includes/db/leave_request_db.inc');

/**
 * Calculate leave day count from date range.
 *
 * @param string $from_date
 * @param string $to_date
 * @param int $half_day
 * @return float
 */
function leave_request_days_between($from_date, $to_date, $half_day=0) {
    $from_sql = date2sql($from_date);
    $to_sql = date2sql($to_date);
    $days = (strtotime($to_sql) - strtotime($from_sql)) / 86400 + 1;
    if ($days < 0)
        $days = 0;
    if ((int)$half_day > 0)
        $days = min(1, $days) * 0.5;

    return (float)$days;
}

page(_("Leave Request"));

simple_page_mode(false);

$status_labels = array(0 => _('Pending'), 1 => _('Approved'), 2 => _('Rejected'), 3 => _('Cancelled'));

if ($Mode == 'ADD_ITEM' || $Mode == 'UPDATE_ITEM') {
    if ($_POST['employee_id'] == '' || $_POST['employee_id'] == ALL_TEXT) {
        display_error(_('Employee is required.'));
        set_focus('employee_id');
    } elseif ((int)$_POST['leave_id'] <= 0) {
        display_error(_('Leave type is required.'));
        set_focus('leave_id');
    } elseif (!is_date($_POST['from_date']) || !is_date($_POST['to_date'])) {
        display_error(_('From and To dates are required.'));
    } elseif (date1_greater_date2($_POST['from_date'], $_POST['to_date'])) {
        display_error(_('To date must be on or after From date.'));
    } else {
        $days = leave_request_days_between($_POST['from_date'], $_POST['to_date'], (int)$_POST['half_day']);
        if ($selected_id != '') {
            update_leave_request(
                $selected_id,
                (int)$_POST['leave_id'],
                $_POST['from_date'],
                $_POST['to_date'],
                $days,
                (int)$_POST['half_day'],
                $_POST['reason']
            );
            display_notification(_('Leave request has been updated.'));
        } else {
            add_leave_request(
                $_POST['employee_id'],
                (int)$_POST['leave_id'],
                $_POST['from_date'],
                $_POST['to_date'],
                $days,
                (int)$_POST['half_day'],
                $_POST['reason']
            );
            display_notification(_('Leave request has been created.'));
        }
        $Mode = 'RESET';
    }
}

if ($Mode == 'Delete') {
    $request = get_leave_request($selected_id);
    if ($request && (int)$request['status'] != 0) {
        display_error(_('Only pending requests can be deleted.'));
    } else {
        $sql = "DELETE FROM ".TB_PREF."leave_requests WHERE request_id = ".db_escape((int)$selected_id)." AND status = 0";
        db_query($sql, 'could not delete leave request');
        display_notification(_('Selected request has been deleted.'));
    }
    $Mode = 'RESET';
}

if ($Mode == 'RESET') {
    $selected_id = '';
    $_POST['selected_id'] = '';
    $_POST['employee_id'] = '';
    $_POST['leave_id'] = 0;
    $_POST['from_date'] = Today();
    $_POST['to_date'] = Today();
    $_POST['half_day'] = 0;
    $_POST['reason'] = '';
}

start_form();

start_table(TABLESTYLE2);
if ($selected_id != '' && $Mode == 'Edit') {
    $myrow = get_leave_request($selected_id);
    if ($myrow && (int)$myrow['status'] == 0) {
        $_POST['employee_id'] = $myrow['employee_id'];
        $_POST['leave_id'] = $myrow['leave_id'];
        $_POST['from_date'] = sql2date($myrow['from_date']);
        $_POST['to_date'] = sql2date($myrow['to_date']);
        $_POST['half_day'] = $myrow['half_day'];
        $_POST['reason'] = $myrow['reason'];
        hidden('selected_id', $selected_id);
    } else {
        display_error(_('Only pending requests can be edited.'));
        $Mode = 'RESET';
    }
}

employees_list_row(_('Employee:'), 'employee_id', null, false, false, false);
leave_types_list_row(_('Leave Type:'), 'leave_id');
date_row(_('From Date:'), 'from_date');
date_row(_('To Date:'), 'to_date');

$half_day_options = array(
    0 => _('Full Day(s)'),
    1 => _('First Half'),
    2 => _('Second Half')
);
array_selector_row(_('Half-day option:'), 'half_day', null, $half_day_options);
textarea_row(_('Reason:'), 'reason', null, 50, 3);

end_table(1);
submit_add_or_update_center($selected_id == '', '', 'both');

start_table(TABLESTYLE, "width='95%'");
$th = array(_('ID'), _('Employee'), _('Leave Type'), _('From'), _('To'), _('Days'), _('Status'), _('Requested On'), '', '');
table_header($th);

$result = get_leave_requests(null, '', '', '');
$k = 0;
while ($row = db_fetch($result)) {
    alt_table_row_color($k);
    label_cell($row['request_id']);
    label_cell($row['employee_id'] . ' ' . $row['employee_name']);
    label_cell($row['leave_name']);
    label_cell(sql2date($row['from_date']));
    label_cell(sql2date($row['to_date']));
    qty_cell($row['days']);
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

