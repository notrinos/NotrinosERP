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
$page_security = 'SA_OVERTIMEAPPROVE';
$path_to_root = "../..";
include($path_to_root . "/includes/session.inc");
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/hrm/includes/db/overtime_request_db.inc');
include_once($path_to_root . '/includes/approval/db/approval_db.inc');
page(_("Overtime Approval"));

simple_page_mode(false);

$status_labels = array(0 => _('Pending'), 1 => _('Approved'), 2 => _('Rejected'));

if (isset($_POST['approve']) || isset($_POST['reject'])) {
    $request_id = (int)get_post('request_id');
    $request = get_overtime_request($request_id);

    if (!$request) {
        display_error(_('Overtime request was not found.'));
    } elseif ((int)$request['status'] != 0) {
        display_error(_('Only pending requests can be processed.'));
    } else {
        $user = $_SESSION['wa_current_user']->loginname;
        $remarks = get_post('approval_remarks', '');

        // Check for core approval draft linked to this request
        $approval_service = get_approval_workflow_service();
        $core_draft = find_approval_draft_for_hrm_request(ST_OVERTIME_REQUEST, $request_id);

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
                approve_overtime_request($request_id, $user);
                display_notification(_('Overtime request has been approved.'));
            } else {
                reject_overtime_request($request_id, $user);
                display_notification(_('Overtime request has been rejected.'));
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
$status_filter_opts = array('' => _('All'), 0 => _('Pending'), 1 => _('Approved'), 2 => _('Rejected'));
array_selector_row(_('Status Filter:'), 'filter_status', $filter_status, $status_filter_opts, array('select_submit' => true));
end_table(1);

start_table(TABLESTYLE, "width='95%'");
$th = array(_('ID'), _('Employee'), _('Type'), _('Date'), _('Hours'), _('Reason'), _('Status'), _('Approved By'), _('Approval Date'), '');
table_header($th);

$status_arg = ($filter_status === '' || $filter_status === ALL_TEXT) ? null : (int)$filter_status;
$result = get_overtime_requests($status_arg, '', '', '');
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
    $selected_request = get_overtime_request((int)$_POST['request_id']);

if ($selected_request && (int)$selected_request['status'] == 0) {
    start_table(TABLESTYLE2);
    label_row(_('Request ID:'), $selected_request['request_id']);
    label_row(_('Employee:'), $selected_request['employee_id'] . ' ' . $selected_request['employee_name']);
    label_row(_('Overtime Type:'), $selected_request['overtime_name']);
    label_row(_('Date:'), sql2date($selected_request['date']));
    label_row(_('Hours:'), qty_format($selected_request['hours']));
    textarea_row(_('Approval Remarks:'), 'approval_remarks', null, 50, 3);
    hidden('request_id', $selected_request['request_id']);
    end_table(1);
    submit_center_first('approve', _('Approve'));
    submit_center_last('reject', _('Reject'));
}

end_form();

end_page();

