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
 * CRM Activity Plans Management
 *
 * @package NotrinosERP
 * @subpackage CRM
 */

$page_security = 'SA_CRM_SETTINGS';
$path_to_root  = '../..';

include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/crm/includes/ui/crm_ui.inc');

include_crm_files();

page(_($help_context = 'CRM Activity Plans'));

simple_page_mode(false);

if ($Mode == 'ADD_ITEM' || $Mode == 'UPDATE_ITEM') {
    if (empty(trim($_POST['plan_name']))) {
        display_error(_('Plan name cannot be empty.'));
        set_focus('plan_name');
    } else {
        if ($selected_id != '') {
            update_crm_activity_plan($selected_id, $_POST['plan_name'],
                $_POST['description'], check_value('active'));
            display_notification(_('Activity plan has been updated.'));
        } else {
            add_crm_activity_plan($_POST['plan_name'], $_POST['description']);
            display_notification(_('New activity plan has been added.'));
        }
        $Mode = 'RESET';
    }
}

if ($Mode == 'Delete') {
    if (key_in_foreign_table($selected_id, 'crm_activity_plan_lines', 'plan_id')) {
        display_error(_('Cannot delete this plan — it has plan line(s). Remove them first.'));
    } else {
        delete_crm_activity_plan($selected_id);
        display_notification(_('Activity plan has been deleted.'));
    }
    $Mode = 'RESET';
}

if ($Mode == 'RESET') {
    $selected_id = '';
    $_POST['plan_name'] = '';
    $_POST['description'] = '';
}

//--------------------------------------------------------------------------

start_form();

start_table(TABLESTYLE, "width='70%'");

$th = array(_('ID'), _('Plan Name'), _('Description'), _('Active'), '', '');
table_header($th);

$result = get_crm_activity_plans(true);
$k = 0;
while ($myrow = db_fetch($result)) {
    alt_table_row_color($k);
    label_cell($myrow['id']);
    label_cell($myrow['plan_name']);
    label_cell($myrow['description'] ?: '-');
    label_cell($myrow['active'] ? _('Yes') : _('No'));
    edit_button_cell('Edit' . $myrow['id'], _('Edit'));
    delete_button_cell('Delete' . $myrow['id'], _('Delete'));
    end_row();
}
end_table(1);

start_table(TABLESTYLE2);

if ($selected_id != '') {
    if ($Mode == 'Edit') {
        $myrow = get_crm_activity_plan($selected_id);
        $_POST['plan_name'] = $myrow['plan_name'];
        $_POST['description'] = $myrow['description'];
        $_POST['active'] = $myrow['active'];
    }
    hidden('selected_id', $selected_id);
}

text_row_ex(_('Plan Name:'), 'plan_name', 40, 100);
text_row_ex(_('Description:'), 'description', 60, 255);

if ($selected_id != '') {
    check_row(_('Active:'), 'active', null);
}

end_table(1);

submit_add_or_update_center($selected_id == '', '', 'both');

end_form();
end_page();

