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
$page_security = 'SA_DEDUCTIONCODE';
$path_to_root = "../..";
include($path_to_root . "/includes/session.inc");
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/hrm/includes/db/deduction_code_db.inc');
page(_("Deduction Codes"));

simple_page_mode(false);

if ($Mode == 'ADD_ITEM' || $Mode == 'UPDATE_ITEM') {
    if (trim($_POST['deduction_name']) == '') {
        display_error(_('Deduction name is required.'));
        set_focus('deduction_name');
    } else {
        if ($selected_id != '') {
            update_deduction_code($selected_id, $_POST['deduction_name'], $_POST['account_code']);
            display_notification(_('Selected deduction code has been updated.'));
        } else {
            add_deduction_code($_POST['deduction_name'], $_POST['account_code']);
            display_notification(_('New deduction code has been added.'));
        }
        $Mode = 'RESET';
    }
}

if ($Mode == 'Delete') {
    delete_deduction_code($selected_id);
    display_notification(_('Selected deduction code has been deleted.'));
    $Mode = 'RESET';
}

if ($Mode == 'RESET') {
    $selected_id = '';
    $_POST['selected_id'] = '';
    $_POST['deduction_name'] = '';
    $_POST['account_code'] = '';
}

start_form();

start_table(TABLESTYLE, "width='70%'");
$th = array(_('ID'), _('Deduction Name'), _('Account Code'), '', '');
table_header($th);

$result = get_deduction_codes(true);
$k = 0;
while ($row = db_fetch($result)) {
    alt_table_row_color($k);
    label_cell($row['deduction_id']);
    label_cell($row['deduction_name']);
    label_cell($row['account_code']);
    edit_button_cell('Edit'.$row['deduction_id'], _('Edit'));
    delete_button_cell('Delete'.$row['deduction_id'], _('Delete'));
    end_row();
}
end_table(1);

start_table(TABLESTYLE2);

if ($selected_id != '' && $Mode == 'Edit') {
    $myrow = get_deduction_code($selected_id);
    $_POST['deduction_name'] = $myrow['deduction_name'];
    $_POST['account_code'] = $myrow['account_code'];
    hidden('selected_id', $selected_id);
}

text_row_ex(_('Deduction Name:'), 'deduction_name', 40, 100);
text_row_ex(_('Account Code:'), 'account_code', 20, 30);

end_table(1);
submit_add_or_update_center($selected_id == '', '', 'both');
end_form();

end_page();

