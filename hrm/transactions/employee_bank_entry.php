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
$page_security = 'SA_EMPLOYEEPAYMENT';
$path_to_root = "../..";
include($path_to_root . "/includes/session.inc");

include_once($path_to_root.'/includes/ui.inc');
include_once($path_to_root.'/hrm/includes/db/employee_db.inc');
include_once($path_to_root.'/hrm/includes/db/payslip_db.inc');
include_once($path_to_root.'/hrm/includes/db/payroll_db.inc');
include_once($path_to_root.'/hrm/includes/db/payment_posting.inc');

$js = '';

if (user_use_date_picker())
	$js .= get_js_date_picker();

page(_($help_context = 'Payment Advice'), false, false, '', $js);

$default_bank_account = get_default_bank_account();
if (!isset($_POST['bank_account']) || (int)$_POST['bank_account'] <= 0)
    $_POST['bank_account'] = $default_bank_account ? $default_bank_account['id'] : '';
if (!isset($_POST['payment_date']) || trim((string)$_POST['payment_date']) === '')
    $_POST['payment_date'] = Today();
if (!isset($_POST['payment_ref']) || trim((string)$_POST['payment_ref']) === '')
    $_POST['payment_ref'] = get_payroll_payment_reference('', get_post('payment_date', Today()));
if (!isset($_POST['payment_memo']))
    $_POST['payment_memo'] = '';

/**
 * Get payslip rows for payment advice list.
 *
 * @param string $employee_id
 * @param bool $include_paid
 * @return resource|false
 */
function get_payment_advice_rows($employee_id='', $include_paid=false) {
    $table = payslip_header_table();
    if (!$table)
        return false;

    $id_col = payslip_has_column($table, 'payslip_id') ? 'payslip_id' : 'payslip_no';
    $employee_col = payslip_has_column($table, 'employee_id') ? 'employee_id' : 'emp_id';
    $amount_col = payslip_has_column($table, 'net_salary') ? 'net_salary' : 'payable_amount';
    $status_col = payslip_has_column($table, 'status') ? 'status' : null;
    $payment_col = payslip_has_column($table, 'payment_trans_no') ? 'payment_trans_no' : null;
    $period_col = payslip_has_column($table, 'payroll_period_id') ? 'payroll_period_id' : null;
    $gl_col = payslip_has_column($table, 'gl_trans_no')
        ? 'gl_trans_no'
        : (payslip_has_column($table, 'trans_no') ? 'trans_no' : null);
    $has_period_table = function_exists('payroll_table_exists')
        && payroll_table_exists('payroll_periods');

    $sql = "SELECT p.".$id_col." as payslip_id,
            p.".$employee_col." as employee_id,
            CONCAT(e.first_name, ' ', e.last_name) as employee_name,
            p.from_date, p.to_date,
            p.".$amount_col." as payable_amount";

    if ($status_col)
        $sql .= ", p.".$status_col." as status";
    if ($payment_col)
        $sql .= ", p.".$payment_col." as payment_trans_no";
    if ($period_col)
        $sql .= ", p.".$period_col." as payroll_period_id";
    else
        $sql .= ", NULL as payroll_period_id";
    if ($gl_col)
        $sql .= ", p.".$gl_col." as payroll_gl_trans_no";
    else
        $sql .= ", NULL as payroll_gl_trans_no";
    if ($period_col && $has_period_table)
        $sql .= ", pp.status as payroll_period_status";
    else
        $sql .= ", NULL as payroll_period_status";

    $sql .= " FROM ".TB_PREF.$table." p
        LEFT JOIN ".TB_PREF."employees e ON e.employee_id = p.".$employee_col;
    if ($period_col && $has_period_table)
        $sql .= " LEFT JOIN ".TB_PREF."payroll_periods pp"
            ." ON pp.period_id = p.".$period_col;
    $sql .= " WHERE 1=1";

    if ($status_col)
        $sql .= " AND COALESCE(p.".$status_col.", 0) <> ".db_escape(payslip_voided_status_value());

    if ($employee_id !== '')
        $sql .= " AND p.".$employee_col." = ".db_escape($employee_id);

    if (!$include_paid) {
        if ($payment_col)
            $sql .= " AND (p.".$payment_col." IS NULL OR p.".$payment_col." = 0)";
        if ($status_col)
            $sql .= " AND p.".$status_col." < 3";
    }

    $sql .= " ORDER BY p.to_date DESC";

    $result = db_query($sql, 'could not get payment advice rows');
    
    // Validate query result
    if (!$result)
        return false;
        
    return $result;
}

/**
 * Check whether a payment-advice row has the minimum approved/posting markers.
 *
 * This is only a UI suppression helper. The server-side payment command repeats
 * the authoritative period and balanced-GL checks under row locks.
 *
 * @param array $row
 * @return bool
 */
function payment_advice_row_has_approved_posted_set($row) {
    if (!is_array($row))
        return false;

    $period_id = isset($row['payroll_period_id']) ? (int)$row['payroll_period_id'] : 0;
    $period_status = isset($row['payroll_period_status'])
        ? (int)$row['payroll_period_status'] : -1;
    $gl_trans_no = isset($row['payroll_gl_trans_no'])
        ? (int)$row['payroll_gl_trans_no'] : 0;

    return $period_id > 0
        && in_array($period_status, array(2, 3), true)
        && $gl_trans_no > 0;
}

if (($payslip_id = find_submit('MarkPaid')) != -1) {
    $payslip_id = (int)$payslip_id;
    
    // Validate payslip_id is positive
    if ($payslip_id <= 0) {
        display_error(_('Invalid payslip ID provided.'));
    } else {
        $payslip = get_payslip($payslip_id);

        // Comprehensive validation
        if (!$payslip) {
            display_error(_('The selected payslip could not be found.'));
        } elseif (empty($payslip['payslip_id'])) {
            display_error(_('The selected payslip does not have a valid ID.'));
        } elseif (payslip_is_voided($payslip)) {
            display_error(_('Voided payslips cannot be marked as paid.'));
        } elseif (payslip_is_paid($payslip)) {
            display_warning(_('The selected payslip is already marked as paid.'));
        } else {
            $validation = validate_payslip_payment($payslip, get_post('bank_account'));
            if (!$validation['valid']) {
                display_error($validation['message']);
            } else {
                $payment_trans_no = create_payment_transaction_for_payslip(
                    $payslip_id,
                    get_post('bank_account'),
                    get_post('payment_date'),
                    get_post('payment_ref'),
                    get_post('payment_memo')
                );
                if ($payment_trans_no) {
                    display_notification(sprintf(_('Payment transaction %s created and payslip marked as paid.'), $payment_trans_no));
                    display_note(get_trans_view_str(ST_BANKPAYMENT, $payment_trans_no, _('&View this Payment')));
                    display_note(get_gl_view_str(ST_BANKPAYMENT, $payment_trans_no, _('&View the GL Postings for this Payment')));
                    $_POST['payment_ref'] = get_payroll_payment_reference('', get_post('payment_date', Today()));
                    $_POST['payment_memo'] = '';
                } else {
                    display_error(_('Could not create payroll payment transaction.'));
                }
            }
        }
    }
}

$selected_employee = trim((string)get_post('employee_id', ''));
$selected_employee_row = false;
if ($selected_employee !== '') {
    // Validate that employee ID is not excessively long (prevent buffer overflow)
    if (strlen($selected_employee) > 100) {
        display_error(_('Invalid employee ID: exceeds maximum length.'));
        $selected_employee = '';
    } else {
        $selected_employee_row = get_employee_by_code($selected_employee);
        if (!$selected_employee_row) {
            display_error(_('The selected employee does not exist.'));
            $selected_employee = '';
        }
    }
}
$show_paid = check_value('show_paid');

start_form();

start_table(TABLESTYLE_NOBORDER);

start_row();
employees_list_cells(_('Employee:'), 'employee_id', null, true, false, false);
check_cells(_('Show Already Paid'), 'show_paid', $show_paid);
submit_cells('refresh_list', _('Refresh'));
end_row();
end_table(1);

start_outer_table(TABLESTYLE, "data-order-header='1'");
table_section(1);
bank_accounts_list_row(_('Pay From:'), 'bank_account', null, true);
date_row(_('Payment Date:'), 'payment_date');

table_section(2);
ref_row(_('Reference:'), 'payment_ref', '', get_payroll_payment_reference('', get_post('payment_date', Today())), false, ST_BANKPAYMENT, array('date' => get_post('payment_date', Today())));
textarea_row(_('Memo:'), 'payment_memo', null, 35, 2);
end_outer_table(1);

$rows = get_payment_advice_rows($selected_employee, $show_paid);
if (!$rows) {
    display_warning(_('Payslip header table is not available.'));
} else {
    start_table(TABLESTYLE_DATA);
    $th = array(_('Payslip #'), _('Employee'), _('From'), _('To'), _('Payable Amount'), _('Status'), _('Action'));
    table_header($th);

    $k = 0;
    while ($row = db_fetch_assoc($rows)) {
        alt_table_row_color($k);

        // Safely display payslip ID
        $payslip_id_safe = isset($row['payslip_id']) ? (int)$row['payslip_id'] : '';
        label_cell(!empty($payslip_id_safe) ? $payslip_id_safe : '-');
        
        // Safely display employee
        $employee_safe = isset($row['employee_id']) ? $row['employee_id'] : '';
        $employee_name_safe = isset($row['employee_name']) ? $row['employee_name'] : '';
        label_cell(!empty($employee_safe) ? $employee_safe.' - '.$employee_name_safe : '-');
        
        // Safely display dates with null checking
        label_cell(isset($row['from_date']) && !empty($row['from_date']) ? sql2date($row['from_date']) : '-');
        label_cell(isset($row['to_date']) && !empty($row['to_date']) ? sql2date($row['to_date']) : '-');
        
        // Safely display payable amount
        $payable_amount = isset($row['payable_amount']) ? (float)$row['payable_amount'] : 0;
        amount_cell($payable_amount);

        $status_txt = payslip_payment_status_label($row);
        label_cell($status_txt);

        if (payslip_is_paid($row)
            || payslip_is_voided($row)
            || !payslip_requires_payment($row)
            || !payment_advice_row_has_approved_posted_set($row)
        )
            label_cell('-');
        else
            submit_cells('MarkPaid'.$payslip_id_safe, _('Process Payment'), false, '', '', false);

        end_row();
    }

    end_table(1);
}

end_form();

end_page();

