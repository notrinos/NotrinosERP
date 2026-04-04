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
 * CRM Activity Types Management
 *
 * @package NotrinosERP
 * @subpackage CRM
 */

$page_security = 'SA_CRM_SETTINGS';
$path_to_root  = '../..';

include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/crm/includes/crm_constants.inc');
include_once($path_to_root . '/crm/includes/ui/crm_ui.inc');
include_once($path_to_root . '/crm/includes/db/crm_settings_db.inc');
include_once($path_to_root . '/crm/includes/db/crm_activities_db.inc');

page(_($help_context = 'CRM Activity Types'));

simple_page_mode(false);

if ($Mode == 'ADD_ITEM' || $Mode == 'UPDATE_ITEM') {
    if (empty(trim($_POST['name']))) {
        display_error(_('Type name cannot be empty.'));
        set_focus('name');
    } else {
        $chained_id = !empty($_POST['chained_activity_type_id']) ? $_POST['chained_activity_type_id'] : null;
        if ($selected_id != '') {
            update_crm_activity_type($selected_id, $_POST['name'], $_POST['category'],
                $_POST['icon'], $_POST['chaining_type'], $chained_id,
                (int)$_POST['schedule_days'], $_POST['schedule_type'],
                check_value('active'));
            display_notification(_('Activity type has been updated.'));
        } else {
            add_crm_activity_type($_POST['name'], $_POST['category'],
                $_POST['icon'], $_POST['chaining_type'], $chained_id,
                (int)$_POST['schedule_days'], $_POST['schedule_type']);
            display_notification(_('New activity type has been added.'));
        }
        $Mode = 'RESET';
    }
}

if ($Mode == 'Delete') {
    if (key_in_foreign_table($selected_id, 'crm_activities', 'activity_type_id')) {
        display_error(_('Cannot delete this activity type — it is referenced by existing activity(ies).'));
    } else {
        delete_crm_activity_type($selected_id);
        display_notification(_('Activity type has been deleted.'));
    }
    $Mode = 'RESET';
}

if ($Mode == 'RESET') {
    $selected_id = '';
    $_POST['name'] = '';
    $_POST['category'] = 'todo';
    $_POST['icon'] = '';
    $_POST['chaining_type'] = 'none';
    $_POST['chained_activity_type_id'] = '';
    $_POST['schedule_days'] = '0';
    $_POST['schedule_type'] = 'completion';
}

//--------------------------------------------------------------------------

start_form();

start_table(TABLESTYLE, "width='80%'");

$th = array(_('ID'), _('Name'), _('Category'), _('Icon'), _('Chaining'), _('Active'), '', '');
table_header($th);

$result = get_crm_activity_types(true);
$k = 0;
while ($myrow = db_fetch($result)) {
    alt_table_row_color($k);
    label_cell($myrow['id']);
    label_cell($myrow['name']);
    label_cell(ucfirst($myrow['category']));
    label_cell($myrow['icon'] ? "<i class='fa " . htmlspecialchars($myrow['icon']) . "'></i> " . $myrow['icon'] : '-');
    $chain_text = $myrow['chaining_type'];
    if ($myrow['chaining_type'] !== 'none' && $myrow['chained_activity_type_id']) {
        $chain_text .= ' (+' . $myrow['schedule_days'] . ' days)';
    }
    label_cell($chain_text);
    label_cell($myrow['active'] ? _('Yes') : _('No'));
    edit_button_cell('Edit' . $myrow['id'], _('Edit'));
    delete_button_cell('Delete' . $myrow['id'], _('Delete'));
    end_row();
}
end_table(1);

start_table(TABLESTYLE2);

if ($selected_id != '') {
    if ($Mode == 'Edit') {
        $myrow = get_crm_activity_type($selected_id);
        $_POST['name'] = $myrow['name'];
        $_POST['category'] = $myrow['category'];
        $_POST['icon'] = $myrow['icon'];
        $_POST['chaining_type'] = $myrow['chaining_type'];
        $_POST['chained_activity_type_id'] = $myrow['chained_activity_type_id'];
        $_POST['schedule_days'] = $myrow['schedule_days'];
        $_POST['schedule_type'] = $myrow['schedule_type'];
        $_POST['active'] = $myrow['active'];
    }
    hidden('selected_id', $selected_id);
}

text_row_ex(_('Type Name:'), 'name', 40, 100);

// Category dropdown
$categories = crm_activity_categories();
$cat_options = '';
foreach ($categories as $key => $label) {
    $sel = (get_post('category') === $key) ? ' selected' : '';
    $cat_options .= "<option value='" . htmlspecialchars($key) . "'$sel>" . htmlspecialchars($label) . "</option>";
}
label_row(_('Category:'), "<select name='category'>$cat_options</select>");

text_row_ex(_('Icon (FA class):'), 'icon', 30, 50);

// Chaining type
$chain_options = '';
$chain_types = array('none' => _('None'), 'suggest' => _('Suggest'), 'trigger' => _('Auto-trigger'));
foreach ($chain_types as $key => $label) {
    $sel = (get_post('chaining_type') === $key) ? ' selected' : '';
    $chain_options .= "<option value='$key'$sel>" . htmlspecialchars($label) . "</option>";
}
label_row(_('Chaining:'), "<select name='chaining_type'>$chain_options</select>");

// Chained activity type
crm_activity_type_list_row(_('Chain to Activity Type:'), 'chained_activity_type_id',
    get_post('chained_activity_type_id'));

text_row_ex(_('Schedule Days:'), 'schedule_days', 10, 10);

$sched_options = '';
$sched_types = array('completion' => _('After completion'), 'deadline' => _('Before deadline'));
foreach ($sched_types as $key => $label) {
    $sel = (get_post('schedule_type') === $key) ? ' selected' : '';
    $sched_options .= "<option value='$key'$sel>" . htmlspecialchars($label) . "</option>";
}
label_row(_('Schedule Type:'), "<select name='schedule_type'>$sched_options</select>");

if ($selected_id != '') {
    check_row(_('Active:'), 'active', null);
}

end_table(1);

submit_add_or_update_center($selected_id == '', '', 'both');

end_form();
end_page();

