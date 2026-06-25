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
 * CRM Lead Sources Management
 *
 * @package NotrinosERP
 * @subpackage CRM
 */

$page_security = 'SA_CRM_SETTINGS';
$path_to_root  = '../..';

include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/crm/includes/crm_constants.inc');
include_once($path_to_root . '/crm/includes/db/crm_settings_db.inc');
include_once($path_to_root . '/crm/includes/db/crm_lead_sources_entity.inc');

page(_($help_context = 'CRM Lead Sources'));

simple_page_mode(false);

if ($Mode == 'ADD_ITEM' || $Mode == 'UPDATE_ITEM') {
    if (empty(trim($_POST['name']))) {
        display_error(_('Source name cannot be empty.'));
        set_focus('name');
    } else {
        if ($selected_id != '') {
            crm_lead_sources_entity::modify($selected_id, array(
                'name' => $_POST['name'],
                'description' => $_POST['description'],
                'active' => check_value('active')
            ));
            display_notification(_('Lead source has been updated.'));
        } else {
            crm_lead_sources_entity::create(array(
                'name' => $_POST['name'],
                'description' => $_POST['description']
            ));
            display_notification(_('New lead source has been added.'));
        }
        $Mode = 'RESET';
    }
}

if ($Mode == 'Delete') {
    if (key_in_foreign_table($selected_id, 'crm_leads', 'lead_source_id')) {
        display_error(_('Cannot delete this lead source — it is referenced by existing lead(s).'));
    } else {
        crm_lead_sources_entity::remove($selected_id);
        display_notification(_('Lead source has been deleted.'));
    }
    $Mode = 'RESET';
}

if ($Mode == 'RESET') {
    $selected_id = '';
    $_POST['name'] = '';
    $_POST['description'] = '';
}

//--------------------------------------------------------------------------

start_form();

start_table(TABLESTYLE, "width='60%'");

$th = array(_('ID'), _('Name'), _('Description'), _('Active'), '', '');
table_header($th);

$result = crm_lead_sources_entity::all_db_resource('1=1 ORDER BY name');
$k = 0;
while ($myrow = db_fetch($result)) {
    alt_table_row_color($k);
    label_cell($myrow['id']);
    label_cell($myrow['name']);
    label_cell($myrow['description']);
    label_cell($myrow['active'] ? _('Yes') : _('No'));
    edit_button_cell('Edit' . $myrow['id'], _('Edit'));
    delete_button_cell('Delete' . $myrow['id'], _('Delete'));
    end_row();
}
end_table(1);

start_table(TABLESTYLE2);

if ($selected_id != '') {
    if ($Mode == 'Edit') {
        $myrow = crm_lead_sources_entity::find($selected_id);
        $_POST['name'] = $myrow['name'];
        $_POST['description'] = $myrow['description'];
        $_POST['active'] = $myrow['active'];
    }
    hidden('selected_id', $selected_id);
}

text_row_ex(_('Source Name:'), 'name', 40, 100);
text_row_ex(_('Description:'), 'description', 60, 255);

if ($selected_id != '') {
    check_row(_('Active:'), 'active', null);
}

end_table(1);

submit_add_or_update_center($selected_id == '', '', 'both');

end_form();
end_page();

