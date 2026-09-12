<?php
/** Restricted advisory offboarding reviews; no external action or settlement. */
$page_security = 'SA_EMPSEPARATION';
$path_to_root = '../..';
include($path_to_root.'/includes/session.inc');
include_once($path_to_root.'/includes/ui.inc');
include_once($path_to_root.'/hrm/includes/db/lifecycle_offboarding_db.inc');
page(_('Employee Offboarding'));
if (!isset($_POST['employee_id'])) $_POST['employee_id'] = '';
$labels = hrm_lifecycle_offboarding_task_labels();
if (isset($_POST['Acknowledge'])) {
    $task = isset($_POST['task_code']) ? $_POST['task_code'] : '';
    if (acknowledge_hrm_lifecycle_offboarding_task($_POST['employee_id'], $task) === false)
        display_error(_('Review could not be recorded. The latest completed Separation must have been submitted by you and independently approved.'));
    else
        display_notification(_('Advisory review recorded. An exact retry keeps the same acknowledgement.'));
}
display_note(_('These acknowledgements record your review. They do not return assets, remove access, settle payroll or change document retention. Complete those actions in their owning systems.'));
start_form(); start_table(TABLESTYLE2);
employees_list_row(_('Employee:'), 'employee_id', null, _('Select employee'), true, true);
echo '<tr><td>'._('Review:').'</td><td><select name="task_code">';
foreach ($labels as $code=>$label)
    echo '<option value="'.htmlspecialchars($code, ENT_QUOTES, 'UTF-8').'">'.htmlspecialchars($label, ENT_QUOTES, 'UTF-8').'</option>';
echo '</select></td></tr>';
end_table(1);
submit_center('Acknowledge', _('Record Advisory Review'));
end_form(); end_page();
