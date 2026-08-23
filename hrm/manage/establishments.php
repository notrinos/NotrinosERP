<?php
/**********************************************************************
    HRM-FND-003 explicit Establishment administration.
***********************************************************************/
$page_security = 'SA_ESTABLISHMENT';
$path_to_root = '../..';
include_once($path_to_root.'/includes/session.inc');
include_once($path_to_root.'/includes/ui.inc');
include_once($path_to_root.'/hrm/includes/db/legal_entity_db.inc');
include_once($path_to_root.'/hrm/includes/db/establishment_db.inc');

page(_($help_context = 'Manage Establishments'));

if (!hrm_establishment_table_ready()) {
    display_error(_('The normalized Establishment foundation is not installed. Run the normal software upgrade first.'));
    display_footer_exit();
}
$legal_entity_id = get_hrm_default_legal_entity_binding_id(false);
if ($legal_entity_id === false) {
    display_error(_('The default HRM Legal Entity is missing or inconsistent. Establishment maintenance is blocked.'));
    display_footer_exit();
}

simple_page_mode(false);

if ($Mode == 'ADD_ITEM' || $Mode == 'UPDATE_ITEM') {
    $identity = normalize_hrm_establishment_identity(
        get_post('establishment_code'), get_post('establishment_name'), get_post('establishment_status', 'active')
    );
    if ($identity === false) {
        display_error(_('Establishment code/name/status is invalid. Code must be 1-32 letters, digits, dot, underscore or hyphen and name must be 1-120 characters.'));
        set_focus('establishment_code');
    } elseif ($selected_id != '') {
        if (update_hrm_establishment((int)$selected_id, $identity['establishment_code'],
            $identity['establishment_name'], $identity['establishment_status']))
            display_notification(_('Selected Establishment has been updated.'));
        else
            display_error(_('The Establishment update was denied or could not be audited. No changes were committed.'));
        $Mode = 'RESET';
    } else {
        $created = add_hrm_establishment($identity['establishment_code'], $identity['establishment_name']);
        if ($created !== false)
            display_notification(_('New Establishment has been added.'));
        else
            display_error(_('The Establishment could not be created. Check authority, uniqueness and audit availability. No changes were committed.'));
        $Mode = 'RESET';
    }
}

if ($Mode == 'Delete') {
    display_error(_('Establishments cannot be physically deleted. Set the status to inactive instead.'));
    $Mode = 'RESET';
}

if ($Mode == 'RESET') {
    $selected_id = '';
    $_POST['selected_id'] = '';
    $_POST['establishment_code'] = '';
    $_POST['establishment_name'] = '';
    $_POST['establishment_status'] = 'active';
}

start_form();
start_table(TABLESTYLE);
$th = array(_('Id'), _('Code'), _('Establishment'), _('Status'), '');
table_header($th);
$result = get_hrm_establishments_for_legal_entity((int)$legal_entity_id, false);
$k = 0;
if ($result) {
    while ($row = db_fetch_assoc($result)) {
        alt_table_row_color($k);
        label_cell((int)$row['establishment_id']);
        label_cell($row['establishment_code']);
        label_cell($row['establishment_name']);
        label_cell($row['establishment_status'] === 'active' ? _('Active') : _('Inactive'));
        edit_button_cell('Edit'.$row['establishment_id'], _('Edit'));
        end_row();
    }
}
end_table(1);

start_table(TABLESTYLE2);
if ($selected_id != '' && $Mode == 'Edit') {
    $row = get_hrm_establishment((int)$selected_id, false);
    if (!is_array($row) || (int)$row['legal_entity_id'] !== (int)$legal_entity_id
        || !hrm_establishment_row_owned_by_admin_writer($row, (int)$legal_entity_id)) {
        display_error(_('The selected Establishment is outside the maintained administrator-writer scope.'));
        $selected_id = '';
    } else {
        $_POST['establishment_code'] = $row['establishment_code'];
        $_POST['establishment_name'] = $row['establishment_name'];
        $_POST['establishment_status'] = $row['establishment_status'];
        hidden('selected_id', $selected_id);
    }
}
text_row_ex(_('Code:'), 'establishment_code', 32, 32);
text_row_ex(_('Establishment:'), 'establishment_name', 60, 120);
label_row(_('Status:'), array_selector('establishment_status', get_post('establishment_status', 'active'),
    array('active'=>_('Active'), 'inactive'=>_('Inactive'))));
end_table(1);
submit_add_or_update_center($selected_id == '', '', 'both');
end_form();
end_page();
