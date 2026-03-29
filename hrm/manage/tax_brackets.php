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
$page_security = 'SA_TAXBRACKET';
$path_to_root = "../..";
include($path_to_root . "/includes/session.inc");
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/hrm/includes/db/tax_bracket_db.inc');

page(_("Tax Brackets"));

simple_page_mode(false);

if ($Mode == 'ADD_ITEM' || $Mode == 'UPDATE_ITEM') {
    if (trim($_POST['bracket_name']) == '') {
        display_error(_('Bracket name is required.'));
        set_focus('bracket_name');
    } elseif (!is_numeric($_POST['from_amount'])) {
        display_error(_('From amount must be numeric.'));
        set_focus('from_amount');
    } elseif (trim($_POST['to_amount']) !== '' && !is_numeric($_POST['to_amount'])) {
        display_error(_('To amount must be numeric.'));
        set_focus('to_amount');
    } elseif (!is_numeric($_POST['rate'])) {
        display_error(_('Tax rate must be numeric.'));
        set_focus('rate');
    } elseif (!is_date($_POST['effective_from'])) {
        display_error(_('Effective from date is required.'));
        set_focus('effective_from');
    } elseif (trim($_POST['effective_to']) !== '' && !is_date($_POST['effective_to'])) {
        display_error(_('Effective to date is invalid.'));
        set_focus('effective_to');
    } else {
        $to_amount = trim($_POST['to_amount']) === '' ? null : input_num('to_amount');
        $effective_to = trim($_POST['effective_to']) === '' ? '' : $_POST['effective_to'];

        if ($selected_id != '') {
            update_tax_bracket($selected_id, $_POST['bracket_name'], input_num('from_amount'), $to_amount, input_num('rate'), input_num('fixed_amount'), $_POST['effective_from'], $effective_to);
            display_notification(_('Tax bracket has been updated.'));
        } else {
            add_tax_bracket($_POST['bracket_name'], input_num('from_amount'), $to_amount, input_num('rate'), input_num('fixed_amount'), $_POST['effective_from'], $effective_to);
            display_notification(_('Tax bracket has been added.'));
        }

        $Mode = 'RESET';
    }
}

if ($Mode == 'Delete') {
    delete_tax_bracket($selected_id);
    display_notification(_('Selected tax bracket has been deleted.'));
    $Mode = 'RESET';
}

if ($Mode == 'RESET') {
    $selected_id = '';
    $_POST['selected_id'] = '';
    $_POST['bracket_name'] = '';
    $_POST['from_amount'] = 0;
    $_POST['to_amount'] = '';
    $_POST['rate'] = 0;
    $_POST['fixed_amount'] = 0;
    $_POST['effective_from'] = Today();
    $_POST['effective_to'] = '';
}

start_form();

start_table(TABLESTYLE, "width='95%'");
$th = array(_('ID'), _('Bracket Name'), _('From'), _('To'), _('Rate %'), _('Fixed'), _('Effective From'), _('Effective To'), '', '');
table_header($th);

$result = get_tax_brackets();
$k = 0;
while ($row = db_fetch($result)) {
    alt_table_row_color($k);
    label_cell($row['bracket_id']);
    label_cell($row['bracket_name']);
    amount_cell($row['from_amount']);
    label_cell($row['to_amount'] === null ? _('No Limit') : price_format($row['to_amount']));
    qty_cell($row['rate']);
    amount_cell($row['fixed_amount']);
    label_cell(sql2date($row['effective_from']));
    label_cell(empty($row['effective_to']) ? '' : sql2date($row['effective_to']));
    edit_button_cell('Edit'.$row['bracket_id'], _('Edit'));
    delete_button_cell('Delete'.$row['bracket_id'], _('Delete'));
    end_row();
}
end_table(1);

start_table(TABLESTYLE2);
if ($selected_id != '' && $Mode == 'Edit') {
    $myrow = get_tax_bracket($selected_id);
    $_POST['bracket_name'] = $myrow['bracket_name'];
    $_POST['from_amount'] = qty_format($myrow['from_amount']);
    $_POST['to_amount'] = is_null($myrow['to_amount']) ? '' : qty_format($myrow['to_amount']);
    $_POST['rate'] = qty_format($myrow['rate']);
    $_POST['fixed_amount'] = qty_format($myrow['fixed_amount']);
    $_POST['effective_from'] = sql2date($myrow['effective_from']);
    $_POST['effective_to'] = empty($myrow['effective_to']) ? '' : sql2date($myrow['effective_to']);
    hidden('selected_id', $selected_id);
}

text_row_ex(_('Bracket Name:'), 'bracket_name', 40, 60);
amount_row(_('From Amount:'), 'from_amount');
amount_row(_('To Amount (blank=no limit):'), 'to_amount');
amount_row(_('Rate (%):'), 'rate');
amount_row(_('Fixed Amount:'), 'fixed_amount');
date_row(_('Effective From:'), 'effective_from');
date_row(_('Effective To:'), 'effective_to');

end_table(1);
submit_add_or_update_center($selected_id == '', '', 'both');
end_form();

end_page();

