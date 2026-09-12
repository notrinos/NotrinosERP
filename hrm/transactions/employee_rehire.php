<?php
/** Same-code Rehire request page; every mutation belongs to central approval. */
$page_security = 'SA_EMPLOYEE';
$path_to_root = '../..';
include($path_to_root.'/includes/session.inc');
include_once($path_to_root.'/includes/ui.inc');
include_once($path_to_root.'/hrm/includes/hrm_ui.inc');
include_once($path_to_root.'/hrm/includes/db/lifecycle_rehire_approval_db.inc');
include_once($path_to_root.'/hrm/includes/db/lifecycle_service_state_browser_db.inc');

page(_('Employee Rehire'), false, false, '', user_use_date_picker() ? get_js_date_picker() : '');
if (!isset($_POST['employee_id'])) $_POST['employee_id'] = '';
if (!isset($_POST['effective_date'])) $_POST['effective_date'] = Today();
$binding = trim((string)$_POST['employee_id']) !== ''
    ? get_hrm_person_worker_binding((string)$_POST['employee_id']) : false;
$namespace = 'hrm_rehire_'.(int)user_company().'_'.hrm_lifecycle_separation_actor_id();
$status = $binding ? get_hrm_lifecycle_rehire_maker_status($binding['employee_number']) : false;
if (isset($_POST['NewRequest']) && $status && in_array($status['status'], array('rejected','cancelled'), true)) {
    hrm_lifecycle_service_state_browser_forget_key($namespace, 'employee_rehire:'.$status['effective_date'], $binding['employee_id']);
    display_notification(_('A new request can now be submitted. The prior decision is retained.'));
}
if (isset($_POST['Process'])) {
    if (!$binding || !empty($binding['retired_at']))
        display_error(_('Select an existing employee.'));
    elseif (!is_date($_POST['effective_date']))
        display_error(_('Enter a valid Rehire date.'));
    elseif (!hrm_lifecycle_rehire_workflow_is_maker_checker())
        display_error(_('Configure an independent Employee Rehire approval workflow before submitting.'));
    else {
        $date = date2sql($_POST['effective_date']);
        $key = hrm_lifecycle_service_state_browser_key($namespace, 'employee_rehire:'.$date, $binding['employee_id']);
        $result = $key ? submit_hrm_lifecycle_employee_rehire((int)$binding['employee_number'], $date, $key) : false;
        if (!$result)
            display_error(_('Rehire could not be submitted. Check the completed Separation, Rehire date, pending requests and payroll-period restrictions.'));
        else
            display_notification(sprintf(_('Rehire request #%d: %s.'), (int)$result['lifecycle_command_id'], _($result['status'])));
    }
    $status = $binding ? get_hrm_lifecycle_rehire_maker_status($binding['employee_number']) : false;
}
display_note(_('Rehire preserves the employee code, original hire date and existing terms. The employee becomes active only after independent approval.'));
start_form();
start_table(TABLESTYLE2);
employees_list_row(_('Employee:'), 'employee_id', null, _('Select employee'), true, true);
date_row(_('Rehire Date:'), 'effective_date');
if ($status) label_row(_('Your latest Rehire request:'), sprintf('#%d: %s (%s)',
    $status['lifecycle_command_id'], _($status['status']), sql2date($status['effective_date'])));
end_table(1);
submit_center('Process', _('Submit Rehire for Approval'));
if ($status && in_array($status['status'], array('rejected','cancelled'), true))
    submit_center('NewRequest', _('Start New Request'));
end_form();
end_page();
