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
$page_security = 'SA_EMPLOYEETRANSVIEW';
$path_to_root = "../..";
include_once($path_to_root . '/includes/db_pager.inc');
include($path_to_root . "/includes/session.inc");
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/hrm/includes/hrm_ui.inc');
include_once($path_to_root . '/hrm/includes/db/payslip_db.inc');
include_once($path_to_root . '/hrm/includes/db/loan_db.inc');

$js = '';
if (user_use_date_picker())
	$js .= get_js_date_picker();

page(_("Employee Transactions"), false, false, '', $js);

/**
 * Build payslip inquiry SQL for the selected employee and date range.
 *
 * @param string $employee_id
 * @param string $from_date
 * @param string $to_date
 * @return string|false
 */
function get_employee_payslip_inquiry_sql($employee_id, $from_date, $to_date) {
    $table_name = payslip_header_table();
    if (!$table_name)
        return false;

    $id_col = payslip_has_column($table_name, 'payslip_id') ? 'payslip_id' : 'payslip_no';
    $gross_col = payslip_has_column($table_name, 'gross_salary') ? 'gross_salary' : (payslip_has_column($table_name, 'salary_amount') ? 'salary_amount' : 'NULL');
    $deduction_col = payslip_has_column($table_name, 'total_deductions') ? 'total_deductions' : 'NULL';
    $net_col = payslip_has_column($table_name, 'payable_amount') ? 'payable_amount' : (payslip_has_column($table_name, 'net_salary') ? 'net_salary' : 'NULL');

    $where = array("p.employee_id = ".db_escape($employee_id));
    if ($from_date !== '' && payslip_has_column($table_name, 'to_date'))
        $where[] = "p.to_date >= ".db_escape(payslip_to_sql_date($from_date));
    if ($to_date !== '' && payslip_has_column($table_name, 'from_date'))
        $where[] = "p.from_date <= ".db_escape(payslip_to_sql_date($to_date));

    $where[] = payslip_non_voided_condition($table_name, 'p');

    return "SELECT p.".$id_col." AS payslip_no,
            p.from_date,
            p.to_date,
            ".$gross_col." AS gross_amount,
            ".$deduction_col." AS deduction_amount,
            ".$net_col." AS net_amount
        FROM ".TB_PREF.$table_name." p
        WHERE ".implode(' AND ', $where)."
        ORDER BY p.from_date DESC, p.to_date DESC, p.".$id_col." DESC";
}

/**
 * Build loan inquiry SQL for the selected employee and date range.
 *
 * @param string $employee_id
 * @param string $from_date
 * @param string $to_date
 * @return string
 */
function get_employee_loan_inquiry_sql($employee_id, $from_date, $to_date) {
    $where = array("l.employee_id = ".db_escape($employee_id));
    if ($from_date !== '')
        $where[] = "l.loan_date >= ".db_escape(loan_to_sql_date($from_date));
    if ($to_date !== '')
        $where[] = "l.loan_date <= ".db_escape(loan_to_sql_date($to_date));

    return "SELECT l.loan_id,
            lt.loan_type_name,
            l.loan_date,
            l.loan_amount,
            l.outstanding_amount,
            l.status
        FROM ".TB_PREF."employee_loans l
        LEFT JOIN ".TB_PREF."loan_types lt ON lt.loan_type_id = l.loan_type_id
        WHERE ".implode(' AND ', $where)."
        ORDER BY l.loan_date DESC, l.loan_id DESC";
}

/**
 * Build employee payment inquiry SQL for core bank payment transactions.
 *
 * @param string $employee_id
 * @param string $from_date
 * @param string $to_date
 * @return string
 */
function get_employee_payment_inquiry_sql($employee_id, $from_date, $to_date) {
    $where = array(
        "bt.type = ".db_escape(ST_BANKPAYMENT),
        "bt.person_type_id = ".db_escape(PT_EMPLOYEE),
        "bt.person_id = ".db_escape($employee_id)
    );

    if ($from_date !== '')
        $where[] = "bt.trans_date >= ".db_escape(date2sql($from_date));
    if ($to_date !== '')
        $where[] = "bt.trans_date <= ".db_escape(date2sql($to_date));

    return "SELECT bt.type,
            bt.trans_no,
            IFNULL(r.reference, '') AS reference,
            bt.trans_date,
            ba.bank_account_name,
            ABS(bt.amount) AS payment_amount,
            IFNULL(c.memo_, '') AS memo_
        FROM ".TB_PREF."bank_trans bt
        LEFT JOIN ".TB_PREF."bank_accounts ba ON ba.id = bt.bank_act
        LEFT JOIN ".TB_PREF."refs r ON r.type = bt.type AND r.id = bt.trans_no
        LEFT JOIN ".TB_PREF."comments c ON c.type = bt.type AND c.id = bt.trans_no
        WHERE ".implode(' AND ', $where)."
        ORDER BY bt.trans_date DESC, bt.trans_no DESC";
}

/**
 * Format a payslip number column for pager output.
 *
 * @param array $row
 * @param string $cell
 * @return string
 */
function employee_trans_payslip_number($row, $cell) {
    return $cell === null ? '' : $cell;
}

/**
 * Format loan status labels for pager output.
 *
 * @param array $row
 * @param string $cell
 * @return string
 */
function employee_trans_loan_status($row, $cell) {
    $status_labels = array(0 => _('Pending'), 1 => _('Active'), 2 => _('Completed'), 3 => _('Cancelled'));
    $status = (int)$cell;

    return isset($status_labels[$status]) ? $status_labels[$status] : $cell;
}

/**
 * Render a bank payment transaction link for pager output.
 *
 * @param array $row
 * @param string $cell
 * @return string
 */
function employee_trans_payment_view($row, $cell) {
    return get_trans_view_str($row['type'], $row['trans_no'], $cell !== '' ? $cell : $row['trans_no']);
}

/**
 * Render a GL view link for pager output.
 *
 * @param array $row
 * @param string $cell
 * @return string
 */
function employee_trans_payment_gl_view($row, $cell) {
    return get_gl_view_str($row['type'], $row['trans_no'], _('GL'));
}

/**
 * Render the payslip transactions pager.
 *
 * @param string $employee_id
 * @param string $from_date
 * @param string $to_date
 * @return void
 */
function display_employee_payslip_transactions($employee_id, $from_date, $to_date) {
    $sql = get_employee_payslip_inquiry_sql($employee_id, $from_date, $to_date);
    if ($sql === false) {
        display_warning(_('Payslip header table is not available.'));
        return;
    }

    $cols = array(
        _('Payslip') => array('name' => 'payslip_no', 'fun' => 'employee_trans_payslip_number', 'ord' => 'desc'),
        _('From') => array('name' => 'from_date', 'type' => 'date', 'ord' => ''),
        _('To') => array('name' => 'to_date', 'type' => 'date', 'ord' => ''),
        _('Gross') => array('name' => 'gross_amount', 'type' => 'amount'),
        _('Deductions') => array('name' => 'deduction_amount', 'type' => 'amount'),
        _('Net') => array('name' => 'net_amount', 'type' => 'amount')
    );

    $table =& new_db_pager('employee_payslip_tbl', $sql, $cols);
    $table->width = '95%';
    display_db_pager($table);
}

/**
 * Render the loan transactions pager.
 *
 * @param string $employee_id
 * @param string $from_date
 * @param string $to_date
 * @return void
 */
function display_employee_loan_transactions($employee_id, $from_date, $to_date) {
    $cols = array(
        _('Loan ID') => array('name' => 'loan_id', 'ord' => 'desc'),
        _('Loan Type') => array('name' => 'loan_type_name', 'ord' => ''),
        _('Loan Date') => array('name' => 'loan_date', 'type' => 'date', 'ord' => ''),
        _('Amount') => array('name' => 'loan_amount', 'type' => 'amount'),
        _('Outstanding') => array('name' => 'outstanding_amount', 'type' => 'amount'),
        _('Status') => array('name' => 'status', 'fun' => 'employee_trans_loan_status', 'ord' => '')
    );

    $table =& new_db_pager('employee_loan_tbl', get_employee_loan_inquiry_sql($employee_id, $from_date, $to_date), $cols);
    $table->width = '95%';
    display_db_pager($table);
}

/**
 * Render the payment transactions pager.
 *
 * @param string $employee_id
 * @param string $from_date
 * @param string $to_date
 * @return void
 */
function display_employee_payment_transactions($employee_id, $from_date, $to_date) {
    $cols = array(
        _('#') => array('name' => 'reference', 'fun' => 'employee_trans_payment_view', 'ord' => ''),
        _('Date') => array('name' => 'trans_date', 'type' => 'date', 'ord' => 'desc'),
        _('Reference') => array('name' => 'reference', 'ord' => ''),
        _('Bank Account') => array('name' => 'bank_account_name', 'ord' => ''),
        _('Amount') => array('name' => 'payment_amount', 'type' => 'amount'),
        _('Memo') => array('name' => 'memo_'),
        array('insert' => true, 'fun' => 'employee_trans_payment_gl_view', 'align' => 'center')
    );

    $table =& new_db_pager('employee_payment_tbl', get_employee_payment_inquiry_sql($employee_id, $from_date, $to_date), $cols);
    $table->width = '95%';
    display_db_pager($table);
}

if (!isset($_POST['from_date']))
    $_POST['from_date'] = begin_month(Today());
if (!isset($_POST['to_date']))
    $_POST['to_date'] = end_month(Today());

start_form(true);
start_table(TABLESTYLE_NOBORDER);
start_row();
employees_list_cells(_('Employee:'), 'employee_id', null, false, false, false);
date_cells(_('From Date:'), 'from_date');
date_cells(_('To Date:'), 'to_date');
submit_cells('Search', _('Apply Filter'));
end_row();
end_table(1);

$employee_id = get_post('employee_id', '');
if ($employee_id != '' && $employee_id != ALL_TEXT) {
    $tabs = array(
        'payslips' => array(_('Payslip Transactions'), true),
        'loans' => array(_('Loan Transactions'), true),
        'payments' => array(_('Payment Transactions'), true)
    );

    tabbed_content_start('employee_trans_tabs', $tabs);

    switch (get_post('_employee_trans_tabs_sel')) {
        case 'loans':
            display_employee_loan_transactions($employee_id, get_post('from_date'), get_post('to_date'));
            break;

        case 'payments':
            display_employee_payment_transactions($employee_id, get_post('from_date'), get_post('to_date'));
            break;

        case 'payslips':
        default:
            display_employee_payslip_transactions($employee_id, get_post('from_date'), get_post('to_date'));
            break;
    }

    tabbed_content_end();
}

end_form();

end_page();

