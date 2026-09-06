<?php
/**********************************************************************
    Copyright (C) NotrinosERP.
    Released under the terms of the GNU General Public License, GPL,
    as published by the Free Software Foundation, either version 3
    of the License, or (at your option) any later version.
***********************************************************************/
$page_security = 'SA_EMPSEPARATION';
$path_to_root = "../..";
include($path_to_root . "/includes/session.inc");
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/hrm/includes/hrm_constants.inc');
include_once($path_to_root . '/hrm/includes/hrm_ui.inc');
include_once($path_to_root . '/hrm/includes/db/employee_db.inc');
include_once($path_to_root . '/hrm/includes/db/employee_salary_db.inc');
include_once($path_to_root . '/hrm/includes/db/eos_db.inc');
include_once($path_to_root . '/hrm/includes/hrm_security.inc');
include_once($path_to_root . '/hrm/includes/db/employee_person_worker_db.inc');
include_once($path_to_root . '/hrm/includes/db/lifecycle_separation_browser_db.inc');

/**
 * Calculate years of service for read-only EOS preview only.
 * The fixed lifecycle checker recalculates authoritative EOS at approval.
 */
function employee_service_years($from_date, $to_date) {
    $from_sql = date2sql($from_date);
    $to_sql = date2sql($to_date);
    $days = (strtotime($to_sql) - strtotime($from_sql)) / 86400;
    if ($days < 0) $days = 0;
    return round2($days / 365, 4);
}
function employee_separation_authoritative_employee_list($row) {
    global $employee_separation_selector_as_of;
    $legacy_label = _format_employee_list($row);
    if (!is_array($row) || !isset($row[0]) || trim((string)$row[0]) === ''
        || $employee_separation_selector_as_of === false) return $legacy_label;
    $identity = get_hrm_person_worker_report_name_as_of($row[0], $employee_separation_selector_as_of);
    if (!is_array($identity) || empty($identity['canonical_linked'])) return $legacy_label;
    $first_name = isset($identity['first_name']) ? trim((string)$identity['first_name']) : '';
    $middle_name = isset($identity['middle_name']) ? trim((string)$identity['middle_name']) : '';
    $last_name = isset($identity['last_name']) ? trim((string)$identity['last_name']) : '';
    $canonical_name = trim($first_name.' '.($middle_name !== '' ? $middle_name.' ' : '').$last_name);
    if ($canonical_name === '') return $legacy_label;
    return (user_show_codes() ? ((string)$row[0].' - ') : '').$canonical_name;
}
function employee_separation_browser_status($employee_id) {
    if (trim((string)$employee_id) === '' || (string)$employee_id === (string)ALL_TEXT)
        return array('status'=>'none','own'=>false,'lifecycle_command_id'=>0,'approval_draft_id'=>0);
    return get_hrm_lifecycle_employee_separation_browser_status($employee_id);
}
function employee_separation_display_recovery_status($employee_id, $status) {
    if (!is_array($status) || !isset($status['status'])) return;
    switch ((string)$status['status']) {
        case 'pending':
            display_notification(sprintf(_('Your Employee Separation request #%d is pending independent approval. No employee, Employment or Assignment termination is applied until approval completes.'),(int)$status['lifecycle_command_id'])); break;
        case 'blocked': display_error(_('Another lifecycle request is already pending for this employee. A parallel Employee Separation request cannot be submitted.')); break;
        case 'completed': display_notification(sprintf(_('Your Employee Separation request #%d has completed.'),(int)$status['lifecycle_command_id'])); hrm_lifecycle_separation_browser_forget_idempotency_key($employee_id); break;
        case 'rejected': display_notification(sprintf(_('Your Employee Separation request #%d was rejected. No rejected request payload or approval comment is copied into this page.'),(int)$status['lifecycle_command_id'])); hrm_lifecycle_separation_browser_forget_idempotency_key($employee_id); break;
        case 'cancelled': display_notification(sprintf(_('Your Employee Separation request #%d was cancelled.'),(int)$status['lifecycle_command_id'])); hrm_lifecycle_separation_browser_forget_idempotency_key($employee_id); break;
        case 'inconsistent': display_error(_('Employee Separation command/approval state is inconsistent. Submission is blocked until the lifecycle/approval state is recovered.')); break;
    }
}
$js = ''; if (user_use_date_picker()) $js .= get_js_date_picker();
page(_($help_context = 'Employee Separation / EOS'), false, false, '', $js);
if (!isset($_POST['employee_id'])) $_POST['employee_id'] = '';
if (!isset($_POST['separation_date'])) $_POST['separation_date'] = Today();
if (!isset($_POST['is_resignation'])) $_POST['is_resignation'] = 0;
$eos_amount = 0; $eos_preview_available = false;
if (isset($_POST['Calculate'])) {
    if ($_POST['employee_id'] == '' || $_POST['employee_id'] == ALL_TEXT) { display_error(_('Employee is required.')); set_focus('employee_id'); }
    elseif (!is_date($_POST['separation_date'])) { display_error(_('Separation date is invalid.')); set_focus('separation_date'); }
    elseif (date_comp($_POST['separation_date'], Today()) > 0) { display_error(_('Future-dated separations are not supported until scheduled lifecycle execution is enabled.')); set_focus('separation_date'); }
    else {
        $employee = get_employee_separation_context_projection($_POST['employee_id']);
        if ($employee) hrm_log_restricted_employee_projection('employee_separation_context');
        if (!$employee) display_error(_('Selected employee was not found.'));
        elseif (empty($employee['hire_date'])) display_error(_('Employee hire date is required before an EOS preview can be calculated.'));
        elseif (date_comp($_POST['separation_date'], sql2date($employee['hire_date'])) < 0) { display_error(_('Separation date cannot be before hire date.')); set_focus('separation_date'); }
        else {
            $monthly_salary = get_employee_total_salary($_POST['employee_id'], $_POST['separation_date']);
            $years = employee_service_years(sql2date($employee['hire_date']), $_POST['separation_date']);
            $eos_amount = calculate_eos_amount($monthly_salary, $years, check_value('is_resignation') ? 1 : 0);
            $eos_preview_available = true;
            display_notification(_('EOS preview calculated for display only. The independent checker recalculates authoritative EOS at final approval; this preview is not stored in lifecycle or approval custody.'));
        }
    }
}
$employee_separation_process_message = false;
if (isset($_POST['Process'])) {
    if ($_POST['employee_id'] == '' || $_POST['employee_id'] == ALL_TEXT) { display_error(_('Employee is required.')); set_focus('employee_id'); }
    elseif (!is_date($_POST['separation_date'])) { display_error(_('Separation date is invalid.')); set_focus('separation_date'); }
    elseif (date_comp($_POST['separation_date'], Today()) > 0) { display_error(_('Future-dated separations are not supported until scheduled lifecycle execution is enabled.')); set_focus('separation_date'); }
    else {
        $before = employee_separation_browser_status($_POST['employee_id']);
        if ($before['status'] === 'pending') $employee_separation_process_message = 'status_only';
        elseif ($before['status'] === 'blocked') { display_error(_('Another lifecycle request is already pending for this employee. No parallel Employee Separation request was created.')); $employee_separation_process_message = 'status_only'; }
        elseif ($before['status'] === 'inconsistent') { display_error(_('Employee Separation command/approval state is inconsistent. No separation was submitted or applied.')); $employee_separation_process_message = 'status_only'; }
        elseif (!hrm_lifecycle_separation_workflow_is_maker_checker()) display_error(_('Employee Separation approval workflow is not configured for independent maker/checker approval. No separation was submitted or applied.'));
        else {
            $idempotency_key = hrm_lifecycle_separation_browser_idempotency_key($_POST['employee_id']);
            if ($idempotency_key === false) display_error(_('A secure server-owned Employee Separation retry key could not be created. No separation was submitted or applied.'));
            else {
                $result = submit_hrm_lifecycle_employee_separation($_POST['employee_id'],date2sql($_POST['separation_date']),check_value('is_resignation') ? 1 : 0,$idempotency_key);
                if (!is_array($result) || !isset($result['status']) || $result['status'] !== 'pending') display_error(_('Could not submit the Employee Separation lifecycle command. No direct separation fallback exists; no employee, Employment or Assignment termination was applied.'));
                elseif (!empty($result['exact_retry'])) { display_notification(sprintf(_('Employee Separation request #%d was already submitted and remains pending approval.'),(int)$result['lifecycle_command_id'])); $employee_separation_process_message = 'submitted'; }
                else { display_notification(sprintf(_('Employee Separation request #%d was submitted for independent approval. Authoritative EOS is recalculated only by the final checker.'),(int)$result['lifecycle_command_id'])); $employee_separation_process_message = 'submitted'; }
            }
        }
    }
}
$current_status = employee_separation_browser_status($_POST['employee_id']);
if ($employee_separation_process_message !== 'submitted') employee_separation_display_recovery_status($_POST['employee_id'], $current_status);
$employee_separation_selector_as_of = hrm_person_worker_utc_now();
hrm_log_restricted_employee_projection('employee_separation_selector');
start_form(); start_table(TABLESTYLE2); label_cell(_('Employee:'));
employees_list_cells(null, 'employee_id', null, true, true, false, false, array('layout_class'=>'combo-layout-equal','format'=>'employee_separation_authoritative_employee_list')); end_row();
date_row(_('Separation Date:'), 'separation_date'); check_row(_('Is Resignation:'), 'is_resignation'); label_row(_('Calculated EOS Preview:'), $eos_preview_available ? price_format($eos_amount) : _('Not calculated')); end_table(1);
submit_center('Calculate', _('Calculate EOS Preview')); submit_center('Process', _('Submit Separation for Approval')); end_form(); end_page();
