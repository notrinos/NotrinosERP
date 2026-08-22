<?php
/**********************************************************************
    HRM-FND-003 explicit Work Location administration.
***********************************************************************/
$page_security = 'SA_WORKLOCATION';
$path_to_root = '../..';
include_once($path_to_root.'/includes/session.inc');
include_once($path_to_root.'/includes/ui.inc');
include_once($path_to_root.'/hrm/includes/db/legal_entity_db.inc');
include_once($path_to_root.'/hrm/includes/db/work_location_db.inc');

page(_($help_context = 'Manage Work Locations'));

if (!hrm_work_location_table_ready()) {
    display_error(_('The normalized Work Location foundation is not installed. Run the normal software upgrade first.'));
    display_footer_exit();
}
$legal_entity_id = get_hrm_default_legal_entity_binding_id(false);
if ($legal_entity_id === false) {
    display_error(_('The default HRM Legal Entity is missing or inconsistent. Work Location maintenance is blocked.'));
    display_footer_exit();
}

simple_page_mode(false);

if ($Mode == 'ADD_ITEM' || $Mode == 'UPDATE_ITEM') {
    $identity = normalize_hrm_work_location_identity(
        get_post('work_location_code'), get_post('work_location_name'), get_post('work_location_status', 'active')
    );
    if ($identity === false) {
        display_error(_('Work Location code/name/status is invalid. Code must be 1-32 letters, digits, dot, underscore or hyphen and name must be 1-120 characters.'));
        set_focus('work_location_code');
    } elseif ($selected_id != '') {
        if (update_hrm_work_location((int)$selected_id, $identity['work_location_code'],
            $identity['work_location_name'], $identity['work_location_status']))
            display_notification(_('Selected Work Location has been updated.'));
        else
            display_error(_('The Work Location update was denied or could not be audited. No changes were committed.'));
        $Mode = 'RESET';
    } else {
        $created = add_hrm_work_location($identity['work_location_code'], $identity['work_location_name']);
        if ($created !== false)
            display_notification(_('New Work Location has been added.'));
        else
            display_error(_('The Work Location could not be created. Check authority, uniqueness and audit availability. No changes were committed.'));
        $Mode = 'RESET';
    }
}

if ($Mode == 'Delete') {
    display_error(_('Work Locations cannot be physically deleted. Set the status to inactive instead.'));
    $Mode = 'RESET';
}

if ($Mode == 'RESET') {
    $selected_id = '';
    $_POST['selected_id'] = '';
    $_POST['work_location_code'] = '';
    $_POST['work_location_name'] = '';
    $_POST['work_location_status'] = 'active';
}

start_form();
start_table(TABLESTYLE);
$th = array(_('Id'), _('Code'), _('Work Location'), _('Status'), '');
table_header($th);
$result = get_hrm_work_locations_for_legal_entity((int)$legal_entity_id, false);
$k = 0;
if ($result) {
    while ($row = db_fetch_assoc($result)) {
        alt_table_row_color($k);
        label_cell((int)$row['work_location_id']);
        label_cell($row['work_location_code']);
        label_cell($row['work_location_name']);
        label_cell($row['work_location_status'] === 'active' ? _('Active') : _('Inactive'));
        edit_button_cell('Edit'.$row['work_location_id'], _('Edit'));
        end_row();
    }
}
end_table(1);

start_table(TABLESTYLE2);
if ($selected_id != '' && $Mode == 'Edit') {
    $row = get_hrm_work_location((int)$selected_id, false);
    if (!is_array($row) || (int)$row['legal_entity_id'] !== (int)$legal_entity_id
        || !hrm_work_location_row_owned_by_admin_writer($row, (int)$legal_entity_id)) {
        display_error(_('The selected Work Location is outside the maintained administrator-writer scope.'));
        $selected_id = '';
    } else {
        $_POST['work_location_code'] = $row['work_location_code'];
        $_POST['work_location_name'] = $row['work_location_name'];
        $_POST['work_location_status'] = $row['work_location_status'];
        hidden('selected_id', $selected_id);
    }
}
text_row_ex(_('Code:'), 'work_location_code', 32, 32);
text_row_ex(_('Work Location:'), 'work_location_name', 60, 120);
label_row(_('Status:'), array_selector('work_location_status', get_post('work_location_status', 'active'),
    array('active'=>_('Active'), 'inactive'=>_('Inactive'))));
end_table(1);
submit_add_or_update_center($selected_id == '', '', 'both');
end_form();
end_page();
