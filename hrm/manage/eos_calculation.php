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
$page_security = 'SA_EOSCALC';
$path_to_root = "../..";
include($path_to_root . "/includes/session.inc");
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/hrm/includes/db/eos_db.inc');

page(_("End of Service Calculation"));

simple_page_mode(false);

if ($Mode == 'ADD_ITEM' || $Mode == 'UPDATE_ITEM') {
    if (!is_numeric($_POST['from_years'])) {
        display_error(_('From years must be numeric.'));
        set_focus('from_years');
    } elseif (trim($_POST['to_years']) !== '' && !is_numeric($_POST['to_years'])) {
        display_error(_('To years must be numeric.'));
        set_focus('to_years');
    } elseif (!is_numeric($_POST['termination_rate']) || !is_numeric($_POST['resignation_rate'])) {
        display_error(_('Termination and resignation rates must be numeric.'));
    } else {
        $to_years = trim($_POST['to_years']) === '' ? null : input_num('to_years');

        if ($selected_id != '') {
            update_eos_tier($selected_id, input_num('from_years'), $to_years, input_num('termination_rate'), input_num('resignation_rate'), $_POST['description']);
            display_notification(_('EOS tier has been updated.'));
        } else {
            add_eos_tier(input_num('from_years'), $to_years, input_num('termination_rate'), input_num('resignation_rate'), $_POST['description']);
            display_notification(_('EOS tier has been added.'));
        }

        $Mode = 'RESET';
    }
}

if ($Mode == 'Delete') {
    delete_eos_tier($selected_id);
    display_notification(_('Selected EOS tier has been deleted.'));
    $Mode = 'RESET';
}

if ($Mode == 'RESET') {
    $selected_id = '';
    $_POST['selected_id'] = '';
    $_POST['from_years'] = 0;
    $_POST['to_years'] = '';
    $_POST['termination_rate'] = 0;
    $_POST['resignation_rate'] = 0;
    $_POST['description'] = '';
}

start_form();

start_table(TABLESTYLE, "width='95%'");
$th = array(_('ID'), _('From Years'), _('To Years'), _('Termination Rate %'), _('Resignation Rate %'), _('Description'), '', '');
table_header($th);

$result = get_eos_tiers();
$k = 0;
while ($row = db_fetch($result)) {
    alt_table_row_color($k);
    label_cell($row['eos_id']);
    qty_cell($row['from_years']);
    label_cell($row['to_years'] === null ? _('And Above') : qty_format($row['to_years']));
    qty_cell($row['termination_rate']);
    qty_cell($row['resignation_rate']);
    label_cell($row['description']);
    edit_button_cell('Edit'.$row['eos_id'], _('Edit'));
    delete_button_cell('Delete'.$row['eos_id'], _('Delete'));
    end_row();
}
end_table(1);

start_table(TABLESTYLE2);
if ($selected_id != '' && $Mode == 'Edit') {
    $myrow = get_eos_tier($selected_id);
    $_POST['from_years'] = qty_format($myrow['from_years']);
    $_POST['to_years'] = is_null($myrow['to_years']) ? '' : qty_format($myrow['to_years']);
    $_POST['termination_rate'] = qty_format($myrow['termination_rate']);
    $_POST['resignation_rate'] = qty_format($myrow['resignation_rate']);
    $_POST['description'] = $myrow['description'];
    hidden('selected_id', $selected_id);
}

amount_row(_('From Years:'), 'from_years');
amount_row(_('To Years (blank=and above):'), 'to_years');
amount_row(_('Termination Rate (% per year):'), 'termination_rate');
amount_row(_('Resignation Rate (% per year):'), 'resignation_rate');
textarea_row(_('Description:'), 'description', null, 50, 3);

end_table(1);
submit_add_or_update_center($selected_id == '', '', 'both');
end_form();

end_page();

