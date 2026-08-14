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
include_once($path_to_root . '/hrm/includes/hrm_security.inc');
include_once($path_to_root . '/hrm/includes/db/employee_person_worker_db.inc');

/**
 * Normalize the leave approval status filter value.
 *
 * @param mixed $filter_status
 * @return int|null
 */
function get_leave_approval_filter_status($filter_status)
{
    if ($filter_status === null || $filter_status === '' || $filter_status === 'all' || $filter_status === ALL_TEXT)
        return null;

    return (int)$filter_status;
}

/**
 * Resolve one Leave Approval Employee name at the page-level instant.
 *
 * Leave Approval remains authorized by SA_LEAVEAPPROVE, which is deliberately
 * not a Person/Worker identity-read capability. Canonical naming is additive
 * only when the same principal independently holds an existing approved
 * identity-read area. Approval lookup, decisions, remarks and workflow payloads
 * continue using the exact legacy Leave Request identity values.
 *
 * @param string $employee_ref
 * @param string $legacy_name
 * @return string
 */
function leave_approval_authoritative_history_name($employee_ref, $legacy_name)
{
    global $leave_approval_history_as_of;

    $legacy_name = (string)$legacy_name;
    if (trim((string)$employee_ref) === '' || $leave_approval_history_as_of === false)
        return $legacy_name;

    $identity = get_hrm_person_worker_report_name_as_of(
        $employee_ref, $leave_approval_history_as_of
    );
    if (!is_array($identity) || empty($identity['canonical_linked']))
        return $legacy_name;

    $first_name = isset($identity['first_name']) ? trim((string)$identity['first_name']) : '';
    $middle_name = isset($identity['middle_name']) ? trim((string)$identity['middle_name']) : '';
    $last_name = isset($identity['last_name']) ? trim((string)$identity['last_name']) : '';
    $canonical_name = trim($first_name.' '.($middle_name !== '' ? $middle_name.' ' : '').$last_name);

    return $canonical_name === '' ? $legacy_name : $canonical_name;
}

/**
 * Check whether a leave request can be approved safely.
 *
 * @param array $request
 * @return bool
 */
function leave_request_is_approvable($request)
{
    if (empty($request))
        return false;

    if ((int)$request['status'] !== 0) {
        display_error(_('Only pending requests can be processed.'));
        return false;
    }

    if ((int)$request['half_day'] > 0 && $request['from_date'] !== $request['to_date']) {
        display_error(_('Half-day leave requests must use the same From and To date before approval.'));
        return false;
    }

    if ((float)$request['days'] <= 0) {
        display_error(_('Leave request days must be greater than zero before approval.'));
        return false;
    }

    return true;
}

page(_("Leave Approval"));

simple_page_mode(false);

$status_labels = array(0 => _('Pending'), 1 => _('Approved'), 2 => _('Rejected'), 3 => _('Cancelled'));

$recover_request_id = find_submit('Recover');
if ($recover_request_id > 0) {
    $recovery = reconcile_pending_leave_request_approval($recover_request_id);
    if (!isset($recovery['status']) || $recovery['status'] === 'error')
        display_error(isset($recovery['message'])
            ? $recovery['message']
            : _('The leave approval could not be recovered.'));
    else
        display_notification($recovery['message']);
    if (isset($Ajax))
        $Ajax->activate('_page_body');
}

if (isset($_POST['approve']) || isset($_POST['reject'])) {
    $request_id = (int)get_post('request_id');
    $request = get_leave_request($request_id);

    if (!$request) {
        display_error(_('Leave request was not found.'));
    } elseif (!leave_request_is_approvable($request) && isset($_POST['approve'])) {
    } else {
        $remarks = get_post('approval_remarks', '');

        $approval_service = get_approval_workflow_service();
        $core_draft = find_approval_draft_for_hrm_request(ST_LEAVE_REQUEST, $request_id);

        if (!$core_draft || (int)$core_draft['status'] !== APPROVAL_STATUS_PENDING) {
            display_error(_('This leave request is not pending in the core approval workflow.'));
        } else {
            if (isset($_POST['approve'])) {
                $result = $approval_service->approve((int)$core_draft['draft_id'], $remarks);
            } else {
                $result = $approval_service->reject((int)$core_draft['draft_id'], $remarks);
            }

            if (isset($result['status']) && $result['status'] === 'error') {
                display_error($result['message']);
            } else {
                display_notification(isset($result['message']) ? $result['message'] : _('Leave request processed through core approval.'));
            }
        }
        if (isset($Ajax))
            $Ajax->activate('_page_body');
    }
}

if ($Mode == 'Edit') {
    $_POST['request_id'] = $selected_id;
    $Mode = 'RESET';
}

$filter_status = get_post('filter_status', '0');

$leave_approval_history_as_of = hrm_person_worker_utc_now();
hrm_log_restricted_employee_projection('employee_leave_approval_history');

start_form();

start_table(TABLESTYLE2);
$status_filter_opts = array(
    'all' => _('All Statuses'),
    '0' => _('Pending'),
    '1' => _('Approved'),
    '2' => _('Rejected'),
    '3' => _('Cancelled')
);
array_selector_row(null, 'filter_status', $filter_status, $status_filter_opts, array('select_submit' => true));
end_table(1);

start_table(TABLESTYLE, "width='98%'");
$th = array(_('ID'), _('Employee'), _('Leave Type'), _('From'), _('To'), _('Days'), _('Reason'), _('Status'), _('Approved By'), _('Approval Date'), '');
table_header($th);

$status_arg = get_leave_approval_filter_status($filter_status);
$result = get_leave_requests($status_arg, '', '', '');

$k = 0;
while ($row = db_fetch($result)) {
    alt_table_row_color($k);
    label_cell($row['request_id']);
    label_cell($row['employee_id'] . ' ' . leave_approval_authoritative_history_name(
        $row['employee_id'], $row['employee_name']
    ));
    label_cell($row['leave_name']);
    label_cell(sql2date($row['from_date']));
    label_cell(sql2date($row['to_date']));
    qty_cell($row['days']);
    label_cell($row['reason']);
    label_cell($status_labels[(int)$row['status']]);
    label_cell($row['approved_by']);
    label_cell(empty($row['approval_date']) ? '' : sql2date(substr($row['approval_date'], 0, 10)));
    $has_pending_core_approval = ((int)$row['status'] == 0) ? find_approval_draft_for_hrm_request(ST_LEAVE_REQUEST, (int)$row['request_id']) : false;
    if ((int)$row['status'] == 0 && $has_pending_core_approval)
        edit_button_cell('Edit' . $row['request_id'], _('Process'));
    elseif ((int)$row['status'] == 0)
        submit_cells(
            'Recover' . $row['request_id'],
            _('Recover'),
            _('Create the missing approval draft from this pending request.'),
            true
        );
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
    label_row(_('Employee:'), $selected_request['employee_id'] . ' ' . leave_approval_authoritative_history_name(
        $selected_request['employee_id'], $selected_request['employee_name']
    ));
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
