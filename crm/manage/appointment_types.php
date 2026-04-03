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
 * CRM Appointment Types Management
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
include_once($path_to_root . '/crm/includes/db/crm_appointments_db.inc');

page(_($help_context = 'CRM Appointment Types'));

simple_page_mode(false);

if ($Mode == 'ADD_ITEM' || $Mode == 'UPDATE_ITEM') {
    if (empty(trim($_POST['name']))) {
        display_error(_('Type name cannot be empty.'));
        set_focus('name');
    } elseif (!is_numeric($_POST['default_duration']) || $_POST['default_duration'] < 1) {
        display_error(_('Duration must be a positive number.'));
        set_focus('default_duration');
    } else {
        if ($selected_id != '') {
            update_crm_appointment_type($selected_id, $_POST['name'],
                (int)$_POST['default_duration'], $_POST['location'],
                $_POST['video_link'], check_value('active'));
            display_notification(_('Appointment type has been updated.'));
        } else {
            add_crm_appointment_type($_POST['name'], (int)$_POST['default_duration'],
                $_POST['location'], $_POST['video_link']);
            display_notification(_('New appointment type has been added.'));
        }
        $Mode = 'RESET';
    }
}

if ($Mode == 'Delete') {
    delete_crm_appointment_type($selected_id);
    display_notification(_('Appointment type has been deleted.'));
    $Mode = 'RESET';
}

if ($Mode == 'RESET') {
    $selected_id = '';
    $_POST['name'] = '';
    $_POST['default_duration'] = '60';
    $_POST['location'] = '';
    $_POST['video_link'] = '';
}

//--------------------------------------------------------------------------

start_form();

start_table(TABLESTYLE, "width='70%'");

$th = array(_('ID'), _('Name'), _('Duration (min)'), _('Location'), _('Active'), '', '');
table_header($th);

$result = get_crm_appointment_types(true);
$k = 0;
while ($myrow = db_fetch($result)) {
    alt_table_row_color($k);
    label_cell($myrow['id']);
    label_cell($myrow['name']);
    label_cell($myrow['default_duration'], "align='center'");
    label_cell($myrow['location'] ?: '-');
    label_cell($myrow['active'] ? _('Yes') : _('No'));
    edit_button_cell('Edit' . $myrow['id'], _('Edit'));
    delete_button_cell('Delete' . $myrow['id'], _('Delete'));
    end_row();
}
end_table(1);

start_table(TABLESTYLE2);

if ($selected_id != '') {
    if ($Mode == 'Edit') {
        $myrow = get_crm_appointment_type($selected_id);
        $_POST['name'] = $myrow['name'];
        $_POST['default_duration'] = $myrow['default_duration'];
        $_POST['location'] = $myrow['location'];
        $_POST['video_link'] = $myrow['video_link'];
        $_POST['active'] = $myrow['active'];
    }
    hidden('selected_id', $selected_id);
}

text_row_ex(_('Type Name:'), 'name', 40, 100);
text_row_ex(_('Default Duration (minutes):'), 'default_duration', 10, 10);
text_row_ex(_('Default Location:'), 'location', 60, 255);
text_row_ex(_('Default Video Link:'), 'video_link', 60, 255);

if ($selected_id != '') {
    check_row(_('Active:'), 'active', null);
}

end_table(1);

submit_add_or_update_center($selected_id == '', '', 'both');

end_form();
end_page();

