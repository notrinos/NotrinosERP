<?php
/** Company compatibility controls use existing authenticated HR administration. */
$page_security = 'SA_HRSETTINGS';
$path_to_root = '../..';
include($path_to_root.'/includes/session.inc');
include_once($path_to_root.'/includes/ui.inc');
include_once($path_to_root.'/hrm/includes/db/employee_db.inc');
page(_('Employee Compatibility'));
if (!user_check_access('SA_EMPLOYEE')) {
    display_error(_('Employee maintenance access is also required.'));
    end_page(); exit;
}
if ($_SERVER['REQUEST_METHOD'] === 'POST') {
    $result = false;
    if (isset($_POST['rebuild'])) {
        $number = isset($_POST['employee_number']) ? (string)$_POST['employee_number'] : '';
        if (ctype_digit($number) && (int)$number > 0) $result = hrm_employee_projection_reconcile((int)$number,true);
    } elseif (isset($_POST['apply'])) {
        $result = hrm_employee_projection_control((string)get_post('projection_mode'),(int)get_post('policy_revision'));
    }
    if ($result === false) display_error(_('Compatibility operation refused. Check current revision, ownership, parity and audit availability.'));
    else display_notification(_('Compatibility operation completed.'));
}
$policy = hrm_employee_projection_policy();
if (!$policy) { display_error(_('Company compatibility policy is unavailable.')); end_page(); exit; }
start_form();
start_table(TABLESTYLE2);
label_row(_('Current mode'), htmlspecialchars($policy['mode']));
label_row(_('Revision'), (int)$policy['revision']);
hidden('policy_revision',(int)$policy['revision']);
label_row(_('Requested mode'),array_selector('projection_mode',$policy['mode'],array('shadow'=>_('Shadow'),'active'=>_('Active'),'fallback'=>_('Fallback'))));
end_table(1);
submit_center('apply',_('Compare and Apply Mode'));
display_note(_('Active mode requires full company parity. Fallback keeps compatibility reads; payroll still requires verified inputs.'));
start_table(TABLESTYLE2);
text_row(_('Employee number to rebuild'),'employee_number',null,12,12);
end_table(1);
submit_center('rebuild',_('Rebuild from Owners'));
end_form();
end_page();
