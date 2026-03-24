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
$page_security = 'SA_LOAN';
$path_to_root = "../..";
include($path_to_root . "/includes/session.inc");
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/hrm/includes/hrm_ui.inc');
include_once($path_to_root . '/hrm/includes/db/loan_type_db.inc');
include_once($path_to_root . '/hrm/includes/db/loan_db.inc');

/**
 * Resolve current username/login for audit fields.
 *
 * @return string
 */
function current_hrm_user_login() {
    if (!isset($_SESSION['wa_current_user']))
        return '';

    $user = $_SESSION['wa_current_user'];
    if (!empty($user->loginname))
        return $user->loginname;
    if (!empty($user->username))
        return $user->username;

    return '';
}

page(_("Loan Request"));

simple_page_mode(false);

if ($Mode == 'ADD_ITEM' || $Mode == 'UPDATE_ITEM') {
    if ($_POST['employee_id'] == '' || $_POST['employee_id'] == ALL_TEXT) {
        display_error(_('Employee is required.'));
        set_focus('employee_id');
    } elseif ((int)$_POST['loan_type_id'] <= 0) {
        display_error(_('Loan type is required.'));
        set_focus('loan_type_id');
    } elseif (!is_date($_POST['loan_date']) || !is_date($_POST['first_repayment'])) {
        display_error(_('Loan date and first repayment date are required.'));
    } elseif ((int)$_POST['installments'] <= 0) {
        display_error(_('Installments must be greater than zero.'));
        set_focus('installments');
    } elseif (input_num('loan_amount') <= 0) {
        display_error(_('Loan amount must be greater than zero.'));
        set_focus('loan_amount');
    } else {
        $loan_type = get_loan_type((int)$_POST['loan_type_id']);
        $interest_rate = input_num('interest_rate');
        if ($interest_rate == 0 && $loan_type)
            $interest_rate = (float)$loan_type['interest_rate'];

        $installments = max((int)$_POST['installments'], 1);
        $total = input_num('loan_amount') + (input_num('loan_amount') * $interest_rate / 100);
        $installment_amount = round2($total / $installments, user_price_dec());

        if ($selected_id != '') {
            update_employee_loan(
                $selected_id,
                (int)$_POST['loan_type_id'],
                input_num('loan_amount'),
                $interest_rate,
                $installments,
                $installment_amount,
                $_POST['loan_date'],
                $_POST['first_repayment'],
                $_POST['notes']
            );
            display_notification(_('Loan request has been updated.'));
        } else {
            add_employee_loan(
                $_POST['employee_id'],
                (int)$_POST['loan_type_id'],
                input_num('loan_amount'),
                $interest_rate,
                $installments,
                $installment_amount,
                $_POST['loan_date'],
                $_POST['first_repayment'],
                $_POST['notes']
            );
            display_notification(_('Loan request has been created.'));
        }

        $Mode = 'RESET';
    }
}

if ($Mode == 'Delete') {
    if (!delete_employee_loan($selected_id))
        display_error(_('Only pending loans can be deleted.'));
    else
        display_notification(_('Selected loan has been deleted.'));

    $Mode = 'RESET';
}

foreach ($_POST as $name => $value) {
    if (strpos($name, 'Approve') === 0) {
        $loan_id = (int)substr($name, 7);
        if ($loan_id > 0) {
            approve_employee_loan($loan_id, current_hrm_user_login());
            display_notification(_('Loan request has been approved.'));
        }
    }
    if (strpos($name, 'Cancel') === 0) {
        $loan_id = (int)substr($name, 6);
        if ($loan_id > 0) {
            cancel_employee_loan($loan_id);
            display_notification(_('Loan request has been cancelled.'));
        }
    }
}

if ($Mode == 'RESET') {
    $selected_id = '';
    $_POST['selected_id'] = '';
    $_POST['employee_id'] = '';
    $_POST['loan_type_id'] = 0;
    $_POST['loan_amount'] = 0;
    $_POST['interest_rate'] = 0;
    $_POST['installments'] = 1;
    $_POST['loan_date'] = Today();
    $_POST['first_repayment'] = Today();
    $_POST['notes'] = '';
}

$status_labels = array(0 => _('Pending'), 1 => _('Active'), 2 => _('Completed'), 3 => _('Cancelled'));

start_form();

start_table(TABLESTYLE2);
if ($selected_id != '' && $Mode == 'Edit') {
    $myrow = get_employee_loan($selected_id);
    if ($myrow && (int)$myrow['status'] == 0) {
        $_POST['employee_id'] = $myrow['employee_id'];
        $_POST['loan_type_id'] = $myrow['loan_type_id'];
        $_POST['loan_amount'] = qty_format($myrow['loan_amount']);
        $_POST['interest_rate'] = qty_format($myrow['interest_rate']);
        $_POST['installments'] = $myrow['installments'];
        $_POST['loan_date'] = sql2date($myrow['loan_date']);
        $_POST['first_repayment'] = sql2date($myrow['first_repayment']);
        $_POST['notes'] = $myrow['notes'];
        hidden('selected_id', $selected_id);
    }
}

employees_list_row(_('Employee:'), 'employee_id', null, false, false, false);
loan_types_list_row(_('Loan Type:'), 'loan_type_id');
amount_row(_('Loan Amount:'), 'loan_amount');
amount_row(_('Interest Rate (%):'), 'interest_rate');
small_amount_row(_('Installments:'), 'installments', get_post('installments', 1), 1, 360);
date_row(_('Loan Date:'), 'loan_date');
date_row(_('First Repayment Date:'), 'first_repayment');
textarea_row(_('Notes:'), 'notes', null, 50, 3);

end_table(1);
submit_add_or_update_center($selected_id == '', '', 'both');

start_table(TABLESTYLE, "width='95%'");
$th = array(_('ID'), _('Employee'), _('Loan Type'), _('Amount'), _('Outstanding'), _('Installments'), _('Loan Date'), _('Status'), '', '', '', '');
table_header($th);

$result = get_employee_loans();
$k = 0;
while ($row = db_fetch($result)) {
    alt_table_row_color($k);
    label_cell($row['loan_id']);
    label_cell($row['employee_id'].' '.$row['employee_name']);
    label_cell($row['loan_type_name']);
    amount_cell($row['loan_amount']);
    amount_cell($row['outstanding_amount']);
    label_cell($row['installments']);
    label_cell(sql2date($row['loan_date']));
    label_cell($status_labels[(int)$row['status']]);

    if ((int)$row['status'] == 0) {
        edit_button_cell('Edit'.$row['loan_id'], _('Edit'));
        delete_button_cell('Delete'.$row['loan_id'], _('Delete'));
        submit_cells('Approve'.$row['loan_id'], _('Approve'));
        submit_cells('Cancel'.$row['loan_id'], _('Cancel'));
    } else {
        label_cell('');
        label_cell('');
        label_cell('');
        label_cell('');
    }

    end_row();
}
end_table(1);

end_form();
end_page();

