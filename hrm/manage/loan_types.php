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
$page_security = 'SA_LOANTYPE';
$path_to_root = "../..";
include($path_to_root . "/includes/session.inc");
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/hrm/includes/db/loan_type_db.inc');

page(_("Loan Types"));

simple_page_mode(false);

if ($Mode == 'ADD_ITEM' || $Mode == 'UPDATE_ITEM') {
    if (trim($_POST['loan_type_name']) == '') {
        display_error(_('Loan type name is required.'));
        set_focus('loan_type_name');
    } elseif (trim($_POST['loan_type_code']) == '') {
        display_error(_('Loan type code is required.'));
        set_focus('loan_type_code');
    } else {
        $max_amount = trim($_POST['max_amount']) === '' ? null : input_num('max_amount');
        $max_installments = trim($_POST['max_installments']) === '' ? null : (int)$_POST['max_installments'];

        if ($selected_id != '') {
            update_loan_type(
                $selected_id,
                $_POST['loan_type_name'],
                $_POST['loan_type_code'],
                input_num('interest_rate'),
                $max_amount,
                $max_installments,
                (int)$_POST['max_active_loans'],
                $_POST['account_code'],
                $_POST['interest_account'],
                check_value('inactive') ? 1 : 0
            );
            display_notification(_('Loan type has been updated.'));
        } else {
            add_loan_type(
                $_POST['loan_type_name'],
                $_POST['loan_type_code'],
                input_num('interest_rate'),
                $max_amount,
                $max_installments,
                (int)$_POST['max_active_loans'],
                $_POST['account_code'],
                $_POST['interest_account'],
                check_value('inactive') ? 1 : 0
            );
            display_notification(_('Loan type has been added.'));
        }

        $Mode = 'RESET';
    }
}

if ($Mode == 'Delete') {
    if (!delete_loan_type($selected_id))
        display_error(_('Loan type cannot be deleted because loans exist for it.'));
    else
        display_notification(_('Selected loan type has been deleted.'));

    $Mode = 'RESET';
}

if ($Mode == 'RESET') {
    $selected_id = '';
    $_POST['selected_id'] = '';
    $_POST['loan_type_name'] = '';
    $_POST['loan_type_code'] = '';
    $_POST['interest_rate'] = 0;
    $_POST['max_amount'] = '';
    $_POST['max_installments'] = '';
    $_POST['max_active_loans'] = 1;
    $_POST['account_code'] = '';
    $_POST['interest_account'] = '';
    $_POST['inactive'] = 0;
}

start_form();

start_table(TABLESTYLE, "width='95%'");
$th = array(_('ID'), _('Name'), _('Code'), _('Interest %'), _('Max Amount'), _('Max Installments'), _('Inactive'), '', '');
table_header($th);

$result = get_loan_types(true);
$k = 0;
while ($row = db_fetch($result)) {
    alt_table_row_color($k);
    label_cell($row['loan_type_id']);
    label_cell($row['loan_type_name']);
    label_cell($row['loan_type_code']);
    qty_cell($row['interest_rate']);
    label_cell($row['max_amount'] === null ? '' : price_format($row['max_amount']));
    label_cell($row['max_installments'] === null ? '' : $row['max_installments']);
    label_cell($row['inactive'] ? _('Yes') : _('No'));
    edit_button_cell('Edit'.$row['loan_type_id'], _('Edit'));
    delete_button_cell('Delete'.$row['loan_type_id'], _('Delete'));
    end_row();
}
end_table(1);

start_table(TABLESTYLE2);
if ($selected_id != '' && $Mode == 'Edit') {
    $myrow = get_loan_type($selected_id);
    $_POST['loan_type_name'] = $myrow['loan_type_name'];
    $_POST['loan_type_code'] = $myrow['loan_type_code'];
    $_POST['interest_rate'] = qty_format($myrow['interest_rate']);
    $_POST['max_amount'] = is_null($myrow['max_amount']) ? '' : qty_format($myrow['max_amount']);
    $_POST['max_installments'] = is_null($myrow['max_installments']) ? '' : $myrow['max_installments'];
    $_POST['max_active_loans'] = $myrow['max_active_loans'];
    $_POST['account_code'] = $myrow['account_code'];
    $_POST['interest_account'] = $myrow['interest_account'];
    $_POST['inactive'] = $myrow['inactive'];
    hidden('selected_id', $selected_id);
}

text_row_ex(_('Loan Type Name:'), 'loan_type_name', 40, 100);
text_row_ex(_('Loan Type Code:'), 'loan_type_code', 20, 20);
amount_row(_('Interest Rate (%):'), 'interest_rate');
amount_row(_('Maximum Amount:'), 'max_amount');
small_amount_row(_('Maximum Installments:'), 'max_installments', get_post('max_installments', ''), 1, 360);
small_amount_row(_('Maximum Active Loans:'), 'max_active_loans', get_post('max_active_loans', 1), 1, 99);
gl_all_accounts_list_row(_('Loan Receivable Account:'), 'account_code', null, true);
gl_all_accounts_list_row(_('Interest Account:'), 'interest_account', null, true, true, _('Optional'));
check_row(_('Inactive:'), 'inactive');

end_table(1);
submit_add_or_update_center($selected_id == '', '', 'both');
end_form();

end_page();

