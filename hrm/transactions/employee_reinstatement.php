<?php
/**********************************************************************
    Copyright (C) NotrinosERP.
    Released under the terms of the GNU General Public License, GPL.
***********************************************************************/
$page_security = 'SA_EMPSEPARATION';
$path_to_root = "../..";
include($path_to_root . "/includes/session.inc");
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/hrm/includes/hrm_constants.inc');
include_once($path_to_root . '/hrm/includes/hrm_ui.inc');
include_once($path_to_root . '/hrm/includes/hrm_security.inc');
include_once($path_to_root . '/hrm/includes/db/employee_person_worker_db.inc');
include_once($path_to_root . '/hrm/includes/db/lifecycle_service_state_browser_db.inc');

function employee_reinstatement_authoritative_employee_list($row) {
    global $employee_reinstatement_selector_as_of;
    $legacy_label = _format_employee_list($row);
    if (!is_array($row) || !isset($row[0]) || trim((string)$row[0]) === '' || $employee_reinstatement_selector_as_of === false) return $legacy_label;
    $identity = get_hrm_person_worker_report_name_as_of($row[0], $employee_reinstatement_selector_as_of);
    if (!is_array($identity) || empty($identity['canonical_linked'])) return $legacy_label;
    $name = trim((isset($identity['first_name'])?(string)$identity['first_name']:'').' '.(isset($identity['middle_name'])&&trim((string)$identity['middle_name'])!==''?trim((string)$identity['middle_name']).' ':'').(isset($identity['last_name'])?(string)$identity['last_name']:''));
    return $name === '' ? $legacy_label : (user_show_codes() ? ((string)$row[0].' - ') : '').$name;
}
function employee_reinstatement_display_status($status) {
    if (!is_array($status) || !isset($status['status'])) return;
    switch((string)$status['status']) {
        case 'pending': display_notification(sprintf(_('Your Employee Reinstatement request #%d is pending independent approval.'),(int)$status['lifecycle_command_id'])); break;
        case 'blocked': display_error(_('Another fixed Employee lifecycle request is already pending for this employee. A parallel Reinstatement request cannot be submitted.')); break;
        case 'completed': display_notification(sprintf(_('Your Employee Reinstatement request #%d has completed.'),(int)$status['lifecycle_command_id'])); hrm_lifecycle_reinstatement_browser_forget_idempotency_key($_POST['employee_id']); break;
        case 'rejected': display_notification(sprintf(_('Your Employee Reinstatement request #%d was rejected. No approval comment is copied into this page.'),(int)$status['lifecycle_command_id'])); hrm_lifecycle_reinstatement_browser_forget_idempotency_key($_POST['employee_id']); break;
        case 'cancelled': display_notification(sprintf(_('Your Employee Reinstatement request #%d was cancelled.'),(int)$status['lifecycle_command_id'])); hrm_lifecycle_reinstatement_browser_forget_idempotency_key($_POST['employee_id']); break;
        case 'inconsistent': display_error(_('Employee Reinstatement command/approval custody is inconsistent. Submission is blocked until lifecycle/approval custody is recovered.')); break;
    }
}
$js = user_use_date_picker() ? get_js_date_picker() : '';
page(_($help_context = "Employee Reinstatement"), false, false, '', $js);
if (!isset($_POST['employee_id'])) $_POST['employee_id']='';
if (!isset($_POST['effective_date'])) $_POST['effective_date']=Today();
$snapshot=get_hrm_lifecycle_employee_reinstatement_browser_snapshot($_POST['employee_id']);
$process=false;
if(isset($_POST['Process'])){
    if($_POST['employee_id']==='' || $_POST['employee_id']==ALL_TEXT){display_error(_('Employee is required.'));set_focus('employee_id');}
    elseif(!is_date($_POST['effective_date'])){display_error(_('Effective date is invalid.'));set_focus('effective_date');}
    elseif(strcmp(date2sql($_POST['effective_date']),date2sql(Today()))>0){display_error(_('Future-dated reinstatement is not supported until scheduled lifecycle execution is enabled.'));set_focus('effective_date');}
    else{
        $snapshot=get_hrm_lifecycle_employee_reinstatement_browser_snapshot($_POST['employee_id']);
        if(!is_array($snapshot)) display_error(_('Exact current Worker/Employment/Assignment custody is unavailable or the employee is not currently suspended. No Reinstatement request was submitted.'));
        else{
            $before=get_hrm_lifecycle_employee_reinstatement_browser_status($_POST['employee_id']);
            if($before['status']==='pending')$process='status_only';
            elseif($before['status']==='blocked'){display_error(_('Another fixed Employee lifecycle request is pending. No parallel Reinstatement request was created.'));$process='status_only';}
            elseif($before['status']==='inconsistent'){display_error(_('Employee Reinstatement command/approval custody is inconsistent. No request was submitted or applied.'));$process='status_only';}
            elseif(!hrm_lifecycle_reinstatement_workflow_is_maker_checker())display_error(_('Employee Reinstatement approval workflow is not configured for independent maker/checker approval. No request was submitted or applied.'));
            else{
                $idempotency_key=hrm_lifecycle_reinstatement_browser_idempotency_key($_POST['employee_id']);
                if($idempotency_key===false)display_error(_('A secure server-owned Employee Reinstatement retry key could not be created. No request was submitted or applied.'));
                else{
                    $result=submit_hrm_lifecycle_employee_reinstatement($_POST['employee_id'],date2sql($_POST['effective_date']),$idempotency_key);
                    if(!is_array($result)||!isset($result['status'])||!in_array($result['status'],array('pending','completed','rejected','cancelled'),true))display_error(_('Could not submit the Employee Reinstatement lifecycle command. No direct service-state fallback exists; no employee or employment change was applied.'));
                    else{display_notification(sprintf(_('Employee Reinstatement request #%d %s.'),(int)$result['lifecycle_command_id'],!empty($result['exact_retry'])?_('was already submitted'):_('was submitted for independent approval')));$process='submitted';}
                }
            }
        }
    }
}
$current=get_hrm_lifecycle_employee_reinstatement_browser_status($_POST['employee_id']); if($process!=='submitted')employee_reinstatement_display_status($current);
$employee_reinstatement_selector_as_of=hrm_person_worker_utc_now();hrm_log_restricted_employee_projection('employee_reinstatement_selector');
start_form();start_table(TABLESTYLE2);
employees_list_row(_('Employee:'),'employee_id',null,false,true,false,false,array('format'=>'employee_reinstatement_authoritative_employee_list'));
if(is_array($snapshot))label_row(_('Current Service State:'),htmlspecialchars((string)$snapshot['employment_status'],ENT_QUOTES,'UTF-8'));
date_row(_('Effective Date:'),'effective_date');end_table(1);
submit_center('Process',_('Submit Reinstatement for Approval'));end_form();end_page();
