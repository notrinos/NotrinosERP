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
/**
 * CRM Schedule Activity - Create/Edit a CRM activity
 *
 * Can be linked to a lead, opportunity, customer, or standalone.
 *
 * @package NotrinosERP
 * @subpackage CRM
 */

$page_security = 'SA_CRM_ACTIVITY';
$path_to_root  = '../..';

include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/crm/includes/crm_constants.inc');
include_once($path_to_root . '/crm/includes/db/crm_settings_db.inc');
include_once($path_to_root . '/crm/includes/db/crm_activities_db.inc');
include_once($path_to_root . '/crm/includes/ui/crm_ui.inc');

$js = '';
if (user_use_date_picker())
    $js .= get_js_date_picker();

page(_($help_context = 'Schedule CRM Activity'), false, false, '', $js);

simple_page_mode(false);

//--------------------------------------------------------------------------
// Pre-fill entity from GET params
//--------------------------------------------------------------------------

$entity_type = isset($_GET['entity_type']) ? $_GET['entity_type'] : '';
$entity_id   = isset($_GET['entity_id'])   ? (int)$_GET['entity_id'] : 0;

//--------------------------------------------------------------------------
// Handle Save
//--------------------------------------------------------------------------

if ($Mode == 'ADD_ITEM' || $Mode == 'UPDATE_ITEM') {
    $input_error = 0;

    if (strlen(trim($_POST['subject'])) < 1) {
        display_error(_('Subject is required.'));
        set_focus('subject');
        $input_error = 1;
    }
    if ((int)$_POST['activity_type_id'] <= 0) {
        display_error(_('Activity type is required.'));
        set_focus('activity_type_id');
        $input_error = 1;
    }
    if (!is_date($_POST['due_date'])) {
        display_error(_('Due date is required.'));
        $input_error = 1;
    }

    if ($input_error == 0) {
        // Combine date + time for date_scheduled column
        $scheduled = date2sql($_POST['due_date']);
        if (!empty($_POST['due_time'])) {
            $scheduled .= ' ' . $_POST['due_time'] . ':00';
        }

        $data = array(
            'activity_type_id' => (int)$_POST['activity_type_id'],
            'entity_type'      => $_POST['entity_type'],
            'entity_id'        => (int)$_POST['entity_id'],
            'summary'          => $_POST['subject'],
            'description'      => $_POST['description'],
            'date_scheduled'   => $scheduled,
            'assigned_to'      => (int)$_POST['assigned_to'],
        );

        if ($selected_id != '') {
            update_crm_activity($selected_id, $data);
            display_notification(_('Activity has been updated.'));
        } else {
            $data['created_by'] = $_SESSION['wa_current_user']->user;
            $data['status'] = CRM_ACTIVITY_PLANNED;
            add_crm_activity($data);
            display_notification(_('Activity has been scheduled.'));
        }
        $Mode = 'RESET';
    }
}

//--------------------------------------------------------------------------
// Handle Complete / Cancel
//--------------------------------------------------------------------------

if (isset($_POST['Complete']) && $selected_id != '') {
    begin_transaction();
    complete_crm_activity($selected_id);
    commit_transaction();
    display_notification(_('Activity marked as done.'));
    $Mode = 'RESET';
}

if (isset($_POST['CancelActivity']) && $selected_id != '') {
    begin_transaction();
    cancel_crm_activity($selected_id);
    commit_transaction();
    display_notification(_('Activity cancelled.'));
    $Mode = 'RESET';
}

//--------------------------------------------------------------------------
// Handle Delete
//--------------------------------------------------------------------------

if ($Mode == 'Delete') {
    $sql = "DELETE FROM " . TB_PREF . "crm_activities WHERE id = " . db_escape((int)$selected_id);
    db_query($sql, 'could not delete activity');
    display_notification(_('Activity has been deleted.'));
    $Mode = 'RESET';
}

//--------------------------------------------------------------------------
// Reset
//--------------------------------------------------------------------------

if ($Mode == 'RESET') {
    $selected_id = '';
    $_POST['selected_id'] = '';
    $_POST['activity_type_id'] = 0;
    $_POST['entity_type'] = $entity_type;
    $_POST['entity_id'] = $entity_id;
    $_POST['subject'] = '';
    $_POST['description'] = '';
    $_POST['due_date'] = Today();
    $_POST['due_time'] = '09:00';
    $_POST['assigned_to'] = $_SESSION['wa_current_user']->user;
    $_POST['priority'] = CRM_PRIORITY_MEDIUM;
}

//--------------------------------------------------------------------------
// Edit mode: load data
//--------------------------------------------------------------------------

if ($selected_id != '' && $Mode == 'Edit') {
    $activity = get_crm_activity($selected_id);
    if ($activity) {
        $_POST['activity_type_id'] = $activity['activity_type_id'];
        $_POST['entity_type']      = $activity['entity_type'];
        $_POST['entity_id']        = $activity['entity_id'];
        $_POST['subject']          = $activity['summary'];
        $_POST['description']      = $activity['description'];
        $_POST['due_date']         = sql2date(substr($activity['date_scheduled'], 0, 10));
        $_POST['due_time']         = substr($activity['date_scheduled'], 11, 5);
        $_POST['assigned_to']      = $activity['assigned_to'];
        hidden('selected_id', $selected_id);
    }
}

//--------------------------------------------------------------------------
// Display Form
//--------------------------------------------------------------------------

start_form();

display_heading(_('Activity Details'));
start_table(TABLESTYLE2);

crm_activity_type_list_row(_('Activity Type:'), 'activity_type_id', null);

text_row(_('Subject:'), 'subject', null, 60, 200);
textarea_row(_('Description:'), 'description', null, 60, 3);
date_row(_('Due Date:'), 'due_date');

$due_time = get_post('due_time', '09:00');
label_row(_('Due Time:'), "<input type='time' name='due_time' value='" . htmlspecialchars($due_time) . "'>");

crm_priority_list_row(_('Priority:'), 'priority', null);

// Assigned To
$users_sql = "SELECT id, real_name FROM " . TB_PREF . "users ORDER BY real_name";
$users_result = db_query($users_sql);
$user_options = array(0 => _('-- Unassigned --'));
while ($u = db_fetch($users_result)) {
    $user_options[$u['id']] = $u['real_name'];
}
array_selector_row(_('Assigned To:'), 'assigned_to', null, $user_options);

end_table(1);

// -- Linked Entity -------------------------------------------------------
display_heading(_('Linked Entity'));
start_table(TABLESTYLE2);

$entity_types = array(
    ''              => _('-- None --'),
    CRM_ENTITY_LEAD     => _('Lead'),
    CRM_ENTITY_CUSTOMER => _('Customer'),
);
array_selector_row(_('Entity Type:'), 'entity_type', null, $entity_types);
text_row(_('Entity ID:'), 'entity_id', null, 10, 10);

end_table(1);

// -- Buttons -------------------------------------------------------------
submit_add_or_update_center($selected_id == '', '', 'both');

if ($selected_id != '') {
    echo "<center>";
    submit('Complete', _('Mark Done'), true, '', 'default');
    echo " &nbsp; ";
    submit('CancelActivity', _('Cancel Activity'), true, '', 'cancel');
    echo "</center><br>";
}

//--------------------------------------------------------------------------
// Activity List (filtered by entity if coming from lead/customer page)
//--------------------------------------------------------------------------

display_heading(_('Recent Activities'));

$filters = array();
if ($entity_type && $entity_id) {
    $filters['entity_type'] = $entity_type;
    $filters['entity_id']   = $entity_id;
}

start_table(TABLESTYLE, "width='95%'");
$th = array(_('ID'), _('Type'), _('Subject'), _('Due Date'), _('Priority'), _('Status'), _('Assigned To'), '', '');
table_header($th);

$result = get_crm_activities($filters, 50, 0);
$k = 0;
while ($row = db_fetch($result)) {
    alt_table_row_color($k);
    label_cell($row['id']);
    label_cell($row['type_name']);
    label_cell($row['summary']);
    label_cell(sql2date(substr($row['date_scheduled'], 0, 10)));
    label_cell(crm_priority_badge($row['priority']));
    label_cell(crm_activity_status_badge($row['status']));
    label_cell(@$row['assigned_name'] ?: '-');

    if ((int)$row['status'] == CRM_ACTIVITY_PLANNED) {
        edit_button_cell('Edit' . $row['id'], _('Edit'));
        delete_button_cell('Delete' . $row['id'], _('Delete'));
    } else {
        label_cell('');
        label_cell('');
    }
    end_row();
}
end_table(1);

end_form();
end_page();

