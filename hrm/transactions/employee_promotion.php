<?php
/**********************************************************************
    Copyright (C) NotrinosERP.
    Released under the terms of the GNU General Public License, GPL,
    as published by the Free Software Foundation, either version 3
    of the License, or (at your option) any later version.
***********************************************************************/
$page_security = 'SA_EMPTRANSFER';
$path_to_root = "../..";
include($path_to_root . "/includes/session.inc");
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/hrm/includes/hrm_constants.inc');
include_once($path_to_root . '/hrm/includes/hrm_ui.inc');
include_once($path_to_root . '/hrm/includes/hrm_security.inc');
include_once($path_to_root . '/hrm/includes/db/employee_person_worker_db.inc');
include_once($path_to_root . '/hrm/includes/db/lifecycle_promotion_browser_db.inc');

function employee_promotion_authoritative_employee_list($row) {
    global $employee_promotion_selector_as_of;
    $legacy_label = _format_employee_list($row);
    if (!is_array($row) || !isset($row[0]) || trim((string)$row[0]) === ''
        || $employee_promotion_selector_as_of === false)
        return $legacy_label;
    $identity = get_hrm_person_worker_report_name_as_of($row[0], $employee_promotion_selector_as_of);
    if (!is_array($identity) || empty($identity['canonical_linked']))
        return $legacy_label;
    $name = trim((isset($identity['first_name']) ? (string)$identity['first_name'] : '').' '
        .(isset($identity['middle_name']) && trim((string)$identity['middle_name']) !== '' ? trim((string)$identity['middle_name']).' ' : '')
        .(isset($identity['last_name']) ? (string)$identity['last_name'] : ''));
    return $name === '' ? $legacy_label : (user_show_codes() ? ((string)$row[0].' - ') : '').$name;
}

function employee_promotion_browser_status($employee_id)
{
    if (trim((string)$employee_id) === '' || (string)$employee_id === (string)ALL_TEXT)
        return array('status'=>'none','own'=>false,'lifecycle_command_id'=>0,'approval_draft_id'=>0);
    return get_hrm_lifecycle_employee_promotion_browser_status($employee_id);
}

function employee_promotion_display_recovery_status($employee_id, $status)
{
    if (!is_array($status) || !isset($status['status']))
        return;
    switch ((string)$status['status']) {
        case 'pending':
            display_notification(sprintf(_('Your Employee Promotion request #%d is pending independent approval. No employee or assignment change is applied until approval completes.'), (int)$status['lifecycle_command_id']));
            break;
        case 'blocked':
            display_error(_('Another fixed Employee lifecycle request is already pending for this employee. A parallel Promotion request cannot be submitted.'));
            break;
        case 'completed':
            display_notification(sprintf(_('Your Employee Promotion request #%d has completed.'), (int)$status['lifecycle_command_id']));
            hrm_lifecycle_promotion_browser_forget_idempotency_key($employee_id);
            break;
        case 'rejected':
            display_notification(sprintf(_('Your Employee Promotion request #%d was rejected. No rejected request payload or approval comment is copied into this page.'), (int)$status['lifecycle_command_id']));
            hrm_lifecycle_promotion_browser_forget_idempotency_key($employee_id);
            break;
        case 'cancelled':
            display_notification(sprintf(_('Your Employee Promotion request #%d was cancelled.'), (int)$status['lifecycle_command_id']));
            hrm_lifecycle_promotion_browser_forget_idempotency_key($employee_id);
            break;
        case 'inconsistent':
            display_error(_('Employee Promotion command/approval state is inconsistent. Submission is blocked until lifecycle/approval custody is recovered.'));
            break;
    }
}

$js = user_use_date_picker() ? get_js_date_picker() : '';
page(_($help_context = "Employee Promotion"), false, false, '', $js);

if (!isset($_POST['employee_id'])) $_POST['employee_id'] = '';
if (!isset($_POST['new_position_id'])) $_POST['new_position_id'] = 0;
if (!isset($_POST['new_grade_id'])) $_POST['new_grade_id'] = 0;
if (!isset($_POST['new_job_id'])) $_POST['new_job_id'] = 0;
if (!isset($_POST['new_work_location_id'])) $_POST['new_work_location_id'] = 0;
if (!isset($_POST['new_manager_employee_id'])) $_POST['new_manager_employee_id'] = '';
if (!isset($_POST['effective_date'])) $_POST['effective_date'] = Today();

$snapshot = get_hrm_lifecycle_employee_promotion_browser_snapshot($_POST['employee_id']);
if (!isset($_POST['Process']) && is_array($snapshot)) {
    if ((int)$_POST['new_position_id'] <= 0) $_POST['new_position_id'] = (int)$snapshot['position_id'];
    if ((int)$_POST['new_grade_id'] <= 0) $_POST['new_grade_id'] = (int)$snapshot['grade_id'];
}

$employee_promotion_process_message = false;
if (isset($_POST['Process'])) {
    if ($_POST['employee_id'] == '' || $_POST['employee_id'] == ALL_TEXT) {
        display_error(_('Employee is required.')); set_focus('employee_id');
    } elseif (!is_date($_POST['effective_date'])) {
        display_error(_('Effective date is invalid.')); set_focus('effective_date');
    } elseif (strcmp(date2sql($_POST['effective_date']), date2sql(Today())) > 0) {
        display_error(_('Future-dated promotions are not supported until scheduled lifecycle execution is enabled.')); set_focus('effective_date');
    } elseif (preg_match('/^[1-9][0-9]*$/D', trim((string)$_POST['new_position_id'])) !== 1) {
        display_error(_('Target Position is required.')); set_focus('new_position_id');
    } elseif (preg_match('/^[1-9][0-9]*$/D', trim((string)$_POST['new_grade_id'])) !== 1) {
        display_error(_('Target Grade is required.')); set_focus('new_grade_id');
    } elseif (trim((string)$_POST['new_job_id']) !== '0' && preg_match('/^[1-9][0-9]*$/D', trim((string)$_POST['new_job_id'])) !== 1) {
        display_error(_('Target Assignment Job selection is invalid.')); set_focus('new_job_id');
    } elseif (trim((string)$_POST['new_work_location_id']) !== '0' && preg_match('/^[1-9][0-9]*$/D', trim((string)$_POST['new_work_location_id'])) !== 1) {
        display_error(_('Target Work Location selection is invalid.')); set_focus('new_work_location_id');
    } elseif (strlen(trim((string)$_POST['new_manager_employee_id'])) > 20
        || (trim((string)$_POST['new_manager_employee_id']) !== '' && trim((string)$_POST['new_manager_employee_id']) === trim((string)$_POST['employee_id']))) {
        display_error(_('Target Manager selection is invalid.')); set_focus('new_manager_employee_id');
    } else {
        $snapshot = get_hrm_lifecycle_employee_promotion_browser_snapshot($_POST['employee_id']);
        if (!is_array($snapshot)) {
            display_error(_('Current Worker/Employment/Assignment custody is unavailable or inconsistent. No Promotion request was submitted.'));
        } elseif ((int)$snapshot['position_id'] === (int)$_POST['new_position_id']
            && (int)$snapshot['grade_id'] === (int)$_POST['new_grade_id']) {
            display_error(_('Promotion requires an actual Position and/or Grade change.'));
        } else {
            $before = employee_promotion_browser_status($_POST['employee_id']);
            if ($before['status'] === 'pending') {
                $employee_promotion_process_message = 'status_only';
            } elseif ($before['status'] === 'blocked') {
                display_error(_('Another fixed Employee lifecycle request is pending. No parallel Promotion request was created.'));
                $employee_promotion_process_message = 'status_only';
            } elseif ($before['status'] === 'inconsistent') {
                display_error(_('Employee Promotion command/approval state is inconsistent. No Promotion request was submitted or applied.'));
                $employee_promotion_process_message = 'status_only';
            } elseif (!hrm_lifecycle_promotion_workflow_is_maker_checker()) {
                display_error(_('Employee Promotion approval workflow is not configured for independent maker/checker approval. No Promotion request was submitted or applied.'));
            } else {
                $idempotency_key = hrm_lifecycle_promotion_browser_idempotency_key($_POST['employee_id']);
                if ($idempotency_key === false) {
                    display_error(_('A secure server-owned Employee Promotion retry key could not be created. No Promotion request was submitted or applied.'));
                } else {
                    $result = submit_hrm_lifecycle_employee_promotion(
                        $_POST['employee_id'], date2sql($_POST['effective_date']), (int)$snapshot['department_id'],
                        (int)$_POST['new_position_id'], (int)$_POST['new_grade_id'], (int)$_POST['new_job_id'],
                        (int)$_POST['new_work_location_id'], trim((string)$_POST['new_manager_employee_id']), $idempotency_key
                    );
                    if (!is_array($result) || !isset($result['status']) || $result['status'] !== 'pending') {
                        display_error(_('Could not submit the Employee Promotion lifecycle command. No direct Promotion fallback exists; no employee or assignment change was applied.'));
                    } elseif (!empty($result['exact_retry'])) {
                        display_notification(sprintf(_('Employee Promotion request #%d was already submitted and remains pending approval.'), (int)$result['lifecycle_command_id']));
                        $employee_promotion_process_message = 'submitted';
                    } else {
                        display_notification(sprintf(_('Employee Promotion request #%d was submitted for independent approval.'), (int)$result['lifecycle_command_id']));
                        $employee_promotion_process_message = 'submitted';
                    }
                }
            }
        }
    }
}

$current_status = employee_promotion_browser_status($_POST['employee_id']);
if ($employee_promotion_process_message !== 'submitted')
    employee_promotion_display_recovery_status($_POST['employee_id'], $current_status);

$employee_promotion_selector_as_of = hrm_person_worker_utc_now();
hrm_log_restricted_employee_projection('employee_promotion_selector');
start_form();
start_table(TABLESTYLE2);
employees_list_row(_('Employee:'), 'employee_id', null, false, true, false, false,
    array('format' => 'employee_promotion_authoritative_employee_list'));
if (is_array($snapshot))
    label_row(_('Current Assignment:'), sprintf(_('Department %d / Position %d / Grade %d'), (int)$snapshot['department_id'], (int)$snapshot['position_id'], (int)$snapshot['grade_id']));
positions_list_row(_('Target Position:'), 'new_position_id', get_post('new_position_id'));
grades_list_row(_('Target Grade:'), 'new_grade_id', get_post('new_grade_id'));
assignment_jobs_list_row(_('Target Assignment Job (optional):'), 'new_job_id');
assignment_work_locations_list_row(_('Target Work Location (optional):'), 'new_work_location_id');
assignment_managers_list_row(_('Target Manager (optional):'), 'new_manager_employee_id', '', get_post('employee_id', ''));
date_row(_('Effective Date:'), 'effective_date');
end_table(1);
submit_center('Process', _('Submit Promotion for Approval'));
end_form();
end_page();
