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
 * CRM Sales Stages (Pipeline Stages) Management
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
include_once($path_to_root . '/crm/includes/db/crm_leads_db.inc');

page(_($help_context = 'CRM Pipeline Stages'));

simple_page_mode(false);

if ($Mode == 'ADD_ITEM' || $Mode == 'UPDATE_ITEM') {
    if (empty(trim($_POST['name']))) {
        display_error(_('Stage name cannot be empty.'));
        set_focus('name');
    } elseif (!is_numeric($_POST['sequence'])) {
        display_error(_('Sequence must be a number.'));
        set_focus('sequence');
    } elseif (!is_numeric($_POST['probability']) || $_POST['probability'] < 0 || $_POST['probability'] > 100) {
        display_error(_('Probability must be between 0 and 100.'));
        set_focus('probability');
    } else {
        if ($selected_id != '') {
            update_crm_sales_stage($selected_id, $_POST['name'],
                $_POST['sequence'], $_POST['probability'],
                $_POST['description'], check_value('active'));
            display_notification(_('Pipeline stage has been updated.'));
        } else {
            add_crm_sales_stage($_POST['name'], $_POST['sequence'],
                $_POST['probability'], $_POST['description']);
            display_notification(_('New pipeline stage has been added.'));
        }
        $Mode = 'RESET';
    }
}

if ($Mode == 'Delete') {
    // Check if stage is used by any leads
    $sql = "SELECT COUNT(*) AS cnt FROM " . TB_PREF . "crm_leads WHERE stage_id = " . db_escape($selected_id);
    $res = db_query($sql, "");
    $row = db_fetch($res);
    if ($row['cnt'] > 0) {
        display_error(_('This stage is in use and cannot be deleted.'));
    } else {
        delete_crm_sales_stage($selected_id);
        display_notification(_('Pipeline stage has been deleted.'));
    }
    $Mode = 'RESET';
}

if ($Mode == 'RESET') {
    $selected_id = '';
    $_POST['name'] = '';
    $_POST['sequence'] = '';
    $_POST['probability'] = '';
    $_POST['description'] = '';
}

//--------------------------------------------------------------------------

start_form();

start_table(TABLESTYLE, "width='70%'");

$th = array(_('ID'), _('Stage Name'), _('Sequence'), _('Probability %'), _('Description'), _('Active'), '', '');
table_header($th);

$result = get_crm_sales_stages(true);
$k = 0;
while ($myrow = db_fetch($result)) {
    alt_table_row_color($k);
    label_cell($myrow['id']);
    label_cell($myrow['name']);
    label_cell($myrow['sequence'], "align='center'");
    label_cell($myrow['probability'] . '%', "align='center'");
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
        $myrow = get_crm_sales_stage($selected_id);
        $_POST['name'] = $myrow['name'];
        $_POST['sequence'] = $myrow['sequence'];
        $_POST['probability'] = $myrow['probability'];
        $_POST['description'] = $myrow['description'];
        $_POST['active'] = $myrow['active'];
    }
    hidden('selected_id', $selected_id);
}

text_row_ex(_('Stage Name:'), 'name', 40, 100);
text_row_ex(_('Sequence:'), 'sequence', 10, 10);
text_row_ex(_('Probability (%):'), 'probability', 10, 3);
text_row_ex(_('Description:'), 'description', 60, 255);

if ($selected_id != '') {
    check_row(_('Active:'), 'active', null);
}

end_table(1);

submit_add_or_update_center($selected_id == '', '', 'both');

end_form();
end_page();

