<?php
/**********************************************************************
    HRM-FND-003 explicit normalized Job administration.
***********************************************************************/
$page_security = 'SA_JOB';
$path_to_root = '../..';
include_once($path_to_root.'/includes/session.inc');
include_once($path_to_root.'/includes/ui.inc');
include_once($path_to_root.'/hrm/includes/db/legal_entity_db.inc');
include_once($path_to_root.'/hrm/includes/db/job_db.inc');

page(_($help_context = 'Manage Jobs'));

if (!hrm_job_table_ready()) {
    display_error(_('The normalized Job writer is not installed. Run the normal software upgrade first.'));
    display_footer_exit();
}
$legal_entity_id = get_hrm_default_legal_entity_binding_id(false);
if ($legal_entity_id === false) {
    display_error(_('The default HRM Legal Entity is missing or inconsistent. Job maintenance is blocked.'));
    display_footer_exit();
}

simple_page_mode(false);

if ($Mode == 'ADD_ITEM' || $Mode == 'UPDATE_ITEM') {
    $identity = normalize_hrm_job_identity(get_post('job_code'), get_post('job_name'), get_post('job_status', 'active'));
    if ($identity === false) {
        display_error(_('Job code/name/status is invalid. Code must be 1-32 letters, digits, dot, underscore or hyphen and name must be 1-120 characters.'));
        set_focus('job_code');
    } elseif ($selected_id != '') {
        if (update_hrm_job((int)$selected_id, $identity['job_code'], $identity['job_name'], $identity['job_status']))
            display_notification(_('Selected Job has been updated.'));
        else
            display_error(_('The Job update was denied or could not be audited. No changes were committed.'));
        $Mode = 'RESET';
    } else {
        $created = add_hrm_job($identity['job_code'], $identity['job_name']);
        if ($created !== false)
            display_notification(_('New Job has been added.'));
        else
            display_error(_('The Job could not be created. Check authority, uniqueness and audit availability. No changes were committed.'));
        $Mode = 'RESET';
    }
}

if ($Mode == 'Delete') {
    display_error(_('Jobs cannot be physically deleted. Set the status to inactive instead.'));
    $Mode = 'RESET';
}

if ($Mode == 'RESET') {
    $selected_id = '';
    $_POST['selected_id'] = '';
    $_POST['job_code'] = '';
    $_POST['job_name'] = '';
    $_POST['job_status'] = 'active';
}

start_form();
start_table(TABLESTYLE);
table_header(array(_('Id'), _('Code'), _('Job'), _('Status'), ''));
$result = get_hrm_jobs_for_legal_entity((int)$legal_entity_id, false);
$k = 0;
if ($result) {
    while ($row = db_fetch_assoc($result)) {
        alt_table_row_color($k);
        label_cell((int)$row['job_id']);
        label_cell($row['job_code']);
        label_cell($row['job_name']);
        label_cell($row['job_status'] === 'active' ? _('Active') : _('Inactive'));
        edit_button_cell('Edit'.$row['job_id'], _('Edit'));
        end_row();
    }
}
end_table(1);

start_table(TABLESTYLE2);
if ($selected_id != '' && $Mode == 'Edit') {
    $row = get_hrm_job((int)$selected_id, false);
    if (!is_array($row) || (int)$row['legal_entity_id'] !== (int)$legal_entity_id
        || !hrm_job_row_owned_by_admin_writer($row, (int)$legal_entity_id)) {
        display_error(_('The selected Job is outside the maintained administrator-writer scope.'));
        $selected_id = '';
    } else {
        $_POST['job_code'] = $row['job_code'];
        $_POST['job_name'] = $row['job_name'];
        $_POST['job_status'] = $row['job_status'];
        hidden('selected_id', $selected_id);
    }
}
text_row_ex(_('Code:'), 'job_code', 32, 32);
text_row_ex(_('Job:'), 'job_name', 60, 120);
label_row(_('Status:'), array_selector('job_status', get_post('job_status', 'active'),
    array('active'=>_('Active'), 'inactive'=>_('Inactive'))));
end_table(1);
submit_add_or_update_center($selected_id == '', '', 'both');
end_form();
end_page();
