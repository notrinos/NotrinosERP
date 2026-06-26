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
 * CRM Tags Management
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
include_once($path_to_root . '/admin/db/tags_db.inc');

page(_($help_context = 'CRM Tags'));

simple_page_mode(false);

if ($Mode == 'ADD_ITEM' || $Mode == 'UPDATE_ITEM') {
    if (empty(trim($_POST['name']))) {
        display_error(_('Tag name cannot be empty.'));
        set_focus('name');
    } else {
        $color = !empty($_POST['color']) ? $_POST['color'] : '#2196F3';
        if ($selected_id != '') {
            update_tag($selected_id, $_POST['name'], $_POST['name'], null, $color);
            display_notification(_('Tag has been updated.'));
        } else {
            add_tag(TAG_CRM, $_POST['name'], $_POST['name'], $color);
            display_notification(_('New tag has been added.'));
        }
        $Mode = 'RESET';
    }
}

if ($Mode == 'Delete') {
    if (key_in_foreign_table($selected_id, 'tag_associations', 'tag_id')) {
        display_error(_('Cannot delete this tag — it is associated with existing record(s). Remove associations first.'));
    } else {
        $sql_del = "DELETE FROM " . TB_PREF . "tag_associations WHERE tag_id = " . db_escape($selected_id);
        db_query($sql_del, "Could not delete tag associations");
        delete_tag($selected_id);
        display_notification(_('Tag has been deleted.'));
    }
    $Mode = 'RESET';
}

if ($Mode == 'RESET') {
    $selected_id = '';
    $_POST['name'] = '';
    $_POST['color'] = '#2196F3';
}

//--------------------------------------------------------------------------

start_form();

start_table(TABLESTYLE, "width='50%'");

$th = array(_('ID'), _('Name'), _('Color'), _('Preview'), '', '');
table_header($th);

$result = get_tags(TAG_CRM);
$k = 0;
while ($myrow = db_fetch($result)) {
    alt_table_row_color($k);
    label_cell($myrow['id']);
    label_cell($myrow['name']);
    label_cell($myrow['color']);
    label_cell("<span style='display:inline-block;padding:2px 12px;background:"
        . htmlspecialchars($myrow['color']) . ";color:#fff;border-radius:3px;'>"
        . htmlspecialchars($myrow['name']) . "</span>");
    edit_button_cell('Edit' . $myrow['id'], _('Edit'));
    delete_button_cell('Delete' . $myrow['id'], _('Delete'));
    end_row();
}
end_table(1);

start_table(TABLESTYLE2);

if ($selected_id != '') {
    if ($Mode == 'Edit') {
        $myrow = get_tag($selected_id);
        $_POST['name'] = $myrow['name'];
        $_POST['color'] = $myrow['color'];
    }
    hidden('selected_id', $selected_id);
}

text_row_ex(_('Tag Name:'), 'name', 30, 60);
label_row(_('Color:'), "<input type='color' name='color' value='"
    . htmlspecialchars(get_post('color', '#2196F3')) . "'>");

end_table(1);

submit_add_or_update_center($selected_id == '', '', 'both');

end_form();
end_page();

