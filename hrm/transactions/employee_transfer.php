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
$page_security = 'SA_EMPTRANSFER';
$path_to_root = "../..";
include($path_to_root . "/includes/session.inc");
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/hrm/includes/hrm_constants.inc');
include_once($path_to_root . '/hrm/includes/hrm_ui.inc');
include_once($path_to_root . '/hrm/includes/hrm_security.inc');
include_once($path_to_root . '/hrm/includes/db/employee_person_worker_db.inc');
include_once($path_to_root . '/hrm/includes/db/lifecycle_transfer_browser_db.inc');

/**
 * Resolve one Employee Transfer selector label at the page-level instant.
 *
 * SA_EMPTRANSFER is deliberately not a Person/Worker identity-read capability,
 * so canonical identity is shown only when the current principal independently
 * holds the accepted identity-read authority. Otherwise the shared legacy label
 * remains unchanged.
 *
 * @param array $row
 * @return string
 */
function employee_transfer_authoritative_employee_list($row) {
    global $employee_transfer_selector_as_of;

    $legacy_label = _format_employee_list($row);
    if (!is_array($row) || !isset($row[0]) || trim((string)$row[0]) === ''
        || $employee_transfer_selector_as_of === false)
        return $legacy_label;

    $identity = get_hrm_person_worker_report_name_as_of(
        $row[0], $employee_transfer_selector_as_of
    );
    if (!is_array($identity) || empty($identity['canonical_linked']))
        return $legacy_label;

    $first_name = isset($identity['first_name']) ? trim((string)$identity['first_name']) : '';
    $middle_name = isset($identity['middle_name']) ? trim((string)$identity['middle_name']) : '';
    $last_name = isset($identity['last_name']) ? trim((string)$identity['last_name']) : '';
    $canonical_name = trim($first_name.' '.($middle_name !== '' ? $middle_name.' ' : '').$last_name);
    if ($canonical_name === '')
        return $legacy_label;

    return (user_show_codes() ? ((string)$row[0].' - ') : '').$canonical_name;
}

/** @return array */
function employee_transfer_browser_status($employee_id)
{
    if (trim((string)$employee_id) === '' || (string)$employee_id === (string)ALL_TEXT)
        return array('status'=>'none','own'=>false,'lifecycle_command_id'=>0,'approval_draft_id'=>0);
    return get_hrm_lifecycle_employee_transfer_browser_status($employee_id);
}

/** @return void */
function employee_transfer_display_recovery_status($employee_id, $status)
{
    if (!is_array($status) || !isset($status['status']))
        return;
    switch ((string)$status['status']) {
        case 'pending':
            display_notification(sprintf(
                _('Your Employee Transfer request #%d is pending independent approval. No employee or assignment change is applied until approval completes.'),
                (int)$status['lifecycle_command_id']
            ));
            break;
        case 'blocked':
            display_error(_('Another Employee Transfer request is already pending for this employee. A parallel transfer request cannot be submitted.'));
            break;
        case 'completed':
            display_notification(sprintf(
                _('Your Employee Transfer request #%d has completed.'),
                (int)$status['lifecycle_command_id']
            ));
            hrm_lifecycle_transfer_browser_forget_idempotency_key($employee_id);
            break;
        case 'rejected':
            display_notification(sprintf(
                _('Your Employee Transfer request #%d was rejected. No rejected request payload or approval comment is copied into this page.'),
                (int)$status['lifecycle_command_id']
            ));
            hrm_lifecycle_transfer_browser_forget_idempotency_key($employee_id);
            break;
        case 'cancelled':
            display_notification(sprintf(
                _('Your Employee Transfer request #%d was cancelled.'),
                (int)$status['lifecycle_command_id']
            ));
            hrm_lifecycle_transfer_browser_forget_idempotency_key($employee_id);
            break;
        case 'inconsistent':
            display_error(_('Employee Transfer command/approval state is inconsistent. Submission is blocked until the lifecycle/approval state is recovered.'));
            break;
    }
}

$js = '';
if (user_use_date_picker())
    $js .= get_js_date_picker();

page(_($help_context = "Employee Transfer"), false, false, '', $js);

if (!isset($_POST['employee_id']))
    $_POST['employee_id'] = '';
if (!isset($_POST['new_department_id']))
    $_POST['new_department_id'] = 0;
if (!isset($_POST['new_position_id']))
    $_POST['new_position_id'] = 0;
if (!isset($_POST['new_grade_id']))
    $_POST['new_grade_id'] = 0;
if (!isset($_POST['new_job_id']))
    $_POST['new_job_id'] = 0;
if (!isset($_POST['new_work_location_id']))
    $_POST['new_work_location_id'] = 0;
if (!isset($_POST['new_manager_employee_id']))
    $_POST['new_manager_employee_id'] = '';
if (!isset($_POST['effective_date']))
    $_POST['effective_date'] = Today();

$employee_transfer_process_message = false;
if (isset($_POST['Process'])) {
    if ($_POST['employee_id'] == '' || $_POST['employee_id'] == ALL_TEXT) {
        display_error(_('Employee is required.'));
        set_focus('employee_id');
    } elseif (!is_date($_POST['effective_date'])) {
        display_error(_('Effective date is invalid.'));
        set_focus('effective_date');
    } elseif (strcmp(date2sql($_POST['effective_date']), date2sql(Today())) > 0) {
        display_error(_('Future-dated transfers are not supported until scheduled lifecycle execution is enabled.'));
        set_focus('effective_date');
    } elseif ((int)$_POST['new_department_id'] == 0) {
        display_error(_('New Department is required and must be selected.'));
        set_focus('new_department_id');
    } elseif (trim((string)$_POST['new_job_id']) !== '0'
        && (preg_match('/^[1-9][0-9]*$/D', trim((string)$_POST['new_job_id'])) !== 1)) {
        display_error(_('New Assignment Job selection is invalid.'));
        set_focus('new_job_id');
    } elseif (trim((string)$_POST['new_work_location_id']) !== '0'
        && (preg_match('/^[1-9][0-9]*$/D', trim((string)$_POST['new_work_location_id'])) !== 1)) {
        display_error(_('New Work Location selection is invalid.'));
        set_focus('new_work_location_id');
    } elseif (strlen(trim((string)$_POST['new_manager_employee_id'])) > 20) {
        display_error(_('New Manager selection is invalid.'));
        set_focus('new_manager_employee_id');
    } elseif (trim((string)$_POST['new_manager_employee_id']) !== ''
        && trim((string)$_POST['new_manager_employee_id']) === trim((string)$_POST['employee_id'])) {
        display_error(_('An employee cannot be their own manager.'));
        set_focus('new_manager_employee_id');
    } else {
        $before = employee_transfer_browser_status($_POST['employee_id']);
        if ($before['status'] === 'pending') {
            $employee_transfer_process_message = 'status_only';
        } elseif ($before['status'] === 'blocked') {
            display_error(_('Another Employee Transfer request is already pending for this employee. No parallel request was created.'));
            $employee_transfer_process_message = 'status_only';
        } elseif ($before['status'] === 'inconsistent') {
            display_error(_('Employee Transfer command/approval state is inconsistent. No transfer was submitted or applied.'));
            $employee_transfer_process_message = 'status_only';
        } elseif (!hrm_lifecycle_transfer_workflow_is_maker_checker()) {
            display_error(_('Employee Transfer approval workflow is not configured for independent maker/checker approval. No transfer was submitted or applied.'));
        } else {
            $idempotency_key = hrm_lifecycle_transfer_browser_idempotency_key($_POST['employee_id']);
            if ($idempotency_key === false) {
                display_error(_('A secure server-owned Employee Transfer retry key could not be created. No transfer was submitted or applied.'));
            } else {
                $result = submit_hrm_lifecycle_employee_transfer(
                    $_POST['employee_id'],
                    date2sql($_POST['effective_date']),
                    (int)$_POST['new_department_id'],
                    (int)$_POST['new_position_id'],
                    (int)$_POST['new_grade_id'],
                    (int)$_POST['new_job_id'],
                    (int)$_POST['new_work_location_id'],
                    trim((string)$_POST['new_manager_employee_id']),
                    $idempotency_key
                );
                if (!is_array($result) || !isset($result['status']) || $result['status'] !== 'pending') {
                    display_error(_('Could not submit the Employee Transfer lifecycle command. No direct transfer fallback exists; no employee or assignment change was applied.'));
                } elseif (!empty($result['exact_retry'])) {
                    display_notification(sprintf(
                        _('Employee Transfer request #%d was already submitted and remains pending approval.'),
                        (int)$result['lifecycle_command_id']
                    ));
                    $employee_transfer_process_message = 'submitted';
                } else {
                    display_notification(sprintf(
                        _('Employee Transfer request #%d was submitted for independent approval.'),
                        (int)$result['lifecycle_command_id']
                    ));
                    $employee_transfer_process_message = 'submitted';
                }
            }
        }
    }
}

$current_status = employee_transfer_browser_status($_POST['employee_id']);
if ($employee_transfer_process_message !== 'submitted')
    employee_transfer_display_recovery_status($_POST['employee_id'], $current_status);

$employee_transfer_selector_as_of = hrm_person_worker_utc_now();
hrm_log_restricted_employee_projection('employee_transfer_selector');

start_form();
start_table(TABLESTYLE2);
employees_list_row(_('Employee:'), 'employee_id', null, false, false, false, false,
    array('format' => 'employee_transfer_authoritative_employee_list'));
departments_list_row(_('New Department:'), 'new_department_id');
positions_list_row(_('New Position:'), 'new_position_id');
grades_list_row(_('New Grade:'), 'new_grade_id');
assignment_jobs_list_row(_('New Assignment Job:'), 'new_job_id');
assignment_work_locations_list_row(_('New Work Location:'), 'new_work_location_id');
assignment_managers_list_row(_('New Manager:'), 'new_manager_employee_id', '', get_post('employee_id', ''));
date_row(_('Effective Date:'), 'effective_date');
end_table(1);
submit_center('Process', _('Submit Transfer for Approval'));
end_form();

end_page();
