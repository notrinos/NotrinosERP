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
$page_security = 'SA_OVERTIME';
$path_to_root = "../..";
include($path_to_root . "/includes/session.inc");
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/hrm/includes/db/overtime_type_db.inc');
page(_("Overtime Types"));

simple_page_mode(false);

if ($Mode == 'ADD_ITEM' || $Mode == 'UPDATE_ITEM') {
    if (trim($_POST['overtime_name']) == '') {
        display_error(_('Overtime type name is required.'));
        set_focus('overtime_name');
    } elseif (!check_num('pay_rate', 0.01)) {
        display_error(_('Pay rate must be greater than zero.'));
        set_focus('pay_rate');
    } else {
        if ($selected_id != '') {
            update_overtime_type($selected_id, $_POST['overtime_name'], input_num('pay_rate'));
            display_notification(_('Selected overtime type has been updated.'));
        } else {
            add_overtime_type($_POST['overtime_name'], input_num('pay_rate'));
            display_notification(_('New overtime type has been added.'));
        }
        $Mode = 'RESET';
    }
}

if ($Mode == 'Delete') {
    delete_overtime_type($selected_id);
    display_notification(_('Selected overtime type has been deleted.'));
    $Mode = 'RESET';
}

if ($Mode == 'RESET') {
    $selected_id = '';
    $_POST['selected_id'] = '';
    $_POST['overtime_name'] = '';
    $_POST['pay_rate'] = 1;
}

start_form();

start_table(TABLESTYLE, "width='60%'");
$th = array(_('ID'), _('Overtime Type'), _('Pay Rate Multiplier'), '', '');
table_header($th);

$result = get_overtime_types(true);
$k = 0;
while ($row = db_fetch($result)) {
    alt_table_row_color($k);
    label_cell($row['overtime_id']);
    label_cell($row['overtime_name']);
    amount_cell($row['pay_rate']);
    edit_button_cell('Edit'.$row['overtime_id'], _('Edit'));
    delete_button_cell('Delete'.$row['overtime_id'], _('Delete'));
    end_row();
}
end_table(1);

start_table(TABLESTYLE2);
if ($selected_id != '' && $Mode == 'Edit') {
    $myrow = get_overtime_type($selected_id);
    $_POST['overtime_name'] = $myrow['overtime_name'];
    $_POST['pay_rate'] = qty_format($myrow['pay_rate']);
    hidden('selected_id', $selected_id);
}

text_row_ex(_('Overtime Type Name:'), 'overtime_name', 40, 100);
amount_row(_('Pay Rate Multiplier:'), 'pay_rate');

end_table(1);
submit_add_or_update_center($selected_id == '', '', 'both');
end_form();

end_page();

