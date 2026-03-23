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
$page_security = 'SA_LEAVEAPPROVE';
$path_to_root = "../..";
include($path_to_root . "/includes/session.inc");
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/hrm/includes/db/leave_request_db.inc');
include_once($path_to_root . '/hrm/includes/db/leave_balance_db.inc');
include_once($path_to_root . '/includes/approval/db/approval_db.inc');
page(_("Leave Approval"));

simple_page_mode(false);

$status_labels = array(0 => _('Pending'), 1 => _('Approved'), 2 => _('Rejected'), 3 => _('Cancelled'));

if (isset($_POST['approve']) || isset($_POST['reject'])) {
    $request_id = (int)get_post('request_id');
    $request = get_leave_request($request_id);

    if (!$request) {
        display_error(_('Leave request was not found.'));
    } elseif ((int)$request['status'] != 0) {
        display_error(_('Only pending requests can be processed.'));
    } else {
        $user = $_SESSION['wa_current_user']->loginname;
        $remarks = get_post('approval_remarks');

        // Check for core approval draft linked to this request
        $approval_service = get_approval_workflow_service();
        $core_draft = find_approval_draft_for_hrm_request(ST_LEAVE_REQUEST, $request_id);

        if ($core_draft && (int)$core_draft['status'] === APPROVAL_STATUS_PENDING) {
            // Use core approval workflow
            if (isset($_POST['approve'])) {
                $result = $approval_service->approve($core_draft['draft_id'], $remarks);
                if ($result['status'] === 'error') {
                    display_error($result['message']);
                } else {
                    display_notification($result['message']);
                }
            } else {
                $result = $approval_service->reject($core_draft['draft_id'], $remarks);
                if ($result['status'] === 'error') {
                    display_error($result['message']);
                } else {
                    display_notification($result['message']);
                }
            }
        } else {
            // Fallback: use legacy direct approval
            if (isset($_POST['approve'])) {
                approve_leave_request($request_id, $user, $remarks);
                $fiscal_year = (int)date('Y', strtotime($request['from_date']));
                apply_leave_balance_movement($request['employee_id'], (int)$request['leave_id'], $fiscal_year, (float)$request['days'], 0, 0);
                display_notification(_('Leave request has been approved.'));
            } else {
                reject_leave_request($request_id, $user, $remarks);
                display_notification(_('Leave request has been rejected.'));
            }
        }
    }
}

if ($Mode == 'Edit') {
    $_POST['request_id'] = $selected_id;
    $Mode = 'RESET';
}

$filter_status = get_post('filter_status', 0);

start_form();

start_table(TABLESTYLE2);
$status_filter_opts = array(
    '' => _('All'),
    0 => _('Pending'),
    1 => _('Approved'),
    2 => _('Rejected'),
    3 => _('Cancelled')
);
array_selector_row(_('Status Filter:'), 'filter_status', $filter_status, $status_filter_opts, array('select_submit' => true));
end_table(1);

start_table(TABLESTYLE, "width='98%'");
$th = array(_('ID'), _('Employee'), _('Leave Type'), _('From'), _('To'), _('Days'), _('Reason'), _('Status'), _('Approved By'), _('Approval Date'), '');
table_header($th);

$status_arg = ($filter_status === '' || $filter_status === ALL_TEXT) ? null : (int)$filter_status;
$result = get_leave_requests($status_arg, '', '', '');

$k = 0;
while ($row = db_fetch($result)) {
    alt_table_row_color($k);
    label_cell($row['request_id']);
    label_cell($row['employee_id'] . ' ' . $row['employee_name']);
    label_cell($row['leave_name']);
    label_cell(sql2date($row['from_date']));
    label_cell(sql2date($row['to_date']));
    qty_cell($row['days']);
    label_cell($row['reason']);
    label_cell($status_labels[(int)$row['status']]);
    label_cell($row['approved_by']);
    label_cell(empty($row['approval_date']) ? '' : sql2date(substr($row['approval_date'], 0, 10)));
    if ((int)$row['status'] == 0)
        edit_button_cell('Edit' . $row['request_id'], _('Process'));
    else
        label_cell('');
    end_row();
}
end_table(1);

$selected_request = null;
if (!empty($_POST['request_id']))
    $selected_request = get_leave_request((int)$_POST['request_id']);

if ($selected_request && (int)$selected_request['status'] == 0) {
    start_table(TABLESTYLE2);
    label_row(_('Request ID:'), $selected_request['request_id']);
    label_row(_('Employee:'), $selected_request['employee_id'] . ' ' . $selected_request['employee_name']);
    label_row(_('Leave Type:'), $selected_request['leave_name']);
    label_row(_('Period:'), sql2date($selected_request['from_date']) . ' - ' . sql2date($selected_request['to_date']));
    label_row(_('Days:'), qty_format($selected_request['days']));
    textarea_row(_('Approval Remarks:'), 'approval_remarks', null, 50, 3);
    hidden('request_id', $selected_request['request_id']);
    end_table(1);
    submit_center_first('approve', _('Approve'));
    submit_center_last('reject', _('Reject'));
}

end_form();

end_page();

