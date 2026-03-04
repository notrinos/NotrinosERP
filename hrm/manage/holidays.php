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
$page_security = 'SA_HOLIDAY';
$path_to_root = "../..";
include($path_to_root . "/includes/session.inc");
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/hrm/includes/db/holiday_db.inc');
page(_("Holiday Calendar"));

simple_page_mode(false);

if ($Mode == 'ADD_ITEM' || $Mode == 'UPDATE_ITEM') {
    if (trim($_POST['holiday_name']) == '') {
        display_error(_('Holiday name is required.'));
        set_focus('holiday_name');
    } elseif (!is_date($_POST['holiday_date'])) {
        display_error(_('Holiday date is invalid.'));
        set_focus('holiday_date');
    } elseif (trim($_POST['to_date']) != '' && !is_date($_POST['to_date'])) {
        display_error(_('To date is invalid.'));
        set_focus('to_date');
    } else {
        $to_date = trim($_POST['to_date']) == '' ? null : $_POST['to_date'];
        if ($selected_id != '') {
            update_holiday($selected_id, $_POST['holiday_name'], $_POST['holiday_date'], $to_date, check_value('recurring') ? 1 : 0, check_value('is_paid') ? 1 : 0, $_POST['description']);
            display_notification(_('Holiday has been updated.'));
        } else {
            add_holiday($_POST['holiday_name'], $_POST['holiday_date'], $to_date, check_value('recurring') ? 1 : 0, check_value('is_paid') ? 1 : 0, $_POST['description']);
            display_notification(_('Holiday has been added.'));
        }
        $Mode = 'RESET';
    }
}

if ($Mode == 'Delete') {
    delete_holiday($selected_id);
    display_notification(_('Selected holiday has been deleted.'));
    $Mode = 'RESET';
}

if ($Mode == 'RESET') {
    $selected_id = '';
    $_POST['selected_id'] = '';
    $_POST['holiday_name'] = '';
    $_POST['holiday_date'] = Today();
    $_POST['to_date'] = '';
    $_POST['recurring'] = 0;
    $_POST['is_paid'] = 1;
    $_POST['description'] = '';
}

start_form();

start_table(TABLESTYLE, "width='95%'");
$th = array(_('ID'), _('Holiday Name'), _('Date'), _('To Date'), _('Recurring'), _('Paid'), _('Description'), '', '');
table_header($th);
$result = get_holidays('', '');
$k = 0;
while ($row = db_fetch($result)) {
    alt_table_row_color($k);
    label_cell($row['holiday_id']);
    label_cell($row['holiday_name']);
    label_cell(sql2date($row['holiday_date']));
    label_cell(empty($row['to_date']) ? '' : sql2date($row['to_date']));
    label_cell($row['recurring'] ? _('Yes') : _('No'));
    label_cell($row['is_paid'] ? _('Yes') : _('No'));
    label_cell($row['description']);
    edit_button_cell('Edit' . $row['holiday_id'], _('Edit'));
    delete_button_cell('Delete' . $row['holiday_id'], _('Delete'));
    end_row();
}
end_table(1);

start_table(TABLESTYLE2);
if ($selected_id != '' && $Mode == 'Edit') {
    $myrow = get_holiday($selected_id);
    $_POST['holiday_name'] = $myrow['holiday_name'];
    $_POST['holiday_date'] = sql2date($myrow['holiday_date']);
    $_POST['to_date'] = empty($myrow['to_date']) ? '' : sql2date($myrow['to_date']);
    $_POST['recurring'] = (int)$myrow['recurring'];
    $_POST['is_paid'] = (int)$myrow['is_paid'];
    $_POST['description'] = $myrow['description'];
    hidden('selected_id', $selected_id);
}

text_row_ex(_('Holiday Name:'), 'holiday_name', 50, 100);
date_row(_('Holiday Date:'), 'holiday_date');
date_row(_('To Date (optional):'), 'to_date');
check_row(_('Recurring yearly:'), 'recurring');
check_row(_('Paid holiday:'), 'is_paid');
textarea_row(_('Description:'), 'description', null, 50, 3);
end_table(1);
submit_add_or_update_center($selected_id == '', '', 'both');

end_form();

end_page();

