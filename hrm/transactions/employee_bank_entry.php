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

page(_($help_context = 'Payment Advice'));

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

    $sql = "SELECT p.".$id_col." as payslip_id,
            p.".$employee_col." as employee_id,
            CONCAT(e.first_name, ' ', e.last_name) as employee_name,
            p.from_date, p.to_date,
            p.".$amount_col." as payable_amount";

    if ($status_col)
        $sql .= ", p.".$status_col." as status";
    if ($payment_col)
        $sql .= ", p.".$payment_col." as payment_trans_no";

    $sql .= " FROM ".TB_PREF.$table." p
        LEFT JOIN ".TB_PREF."employees e ON e.employee_id = p.".$employee_col." WHERE 1=1";

    if ($employee_id !== '')
        $sql .= " AND p.".$employee_col." = ".db_escape($employee_id);

    if (!$include_paid) {
        if ($payment_col)
            $sql .= " AND (p.".$payment_col." IS NULL OR p.".$payment_col." = 0)";
        if ($status_col)
            $sql .= " AND p.".$status_col." < 3";
    }

    $sql .= " ORDER BY p.to_date DESC";

    return db_query($sql, 'could not get payment advice rows');
}

if (($payslip_id = find_submit('MarkPaid')) != -1) {
    $payslip_id = (int)$payslip_id;
    $payslip = get_payslip($payslip_id);

    if (!$payslip) {
        display_error(_('The selected payslip could not be found.'));
    } else {
        $data = array('status' => 3);
        if (update_payslip($payslip_id, $data))
            display_notification(_('Payslip marked as paid.'));
        else
            display_error(_('Could not update payslip payment status.'));
    }
}

$selected_employee = trim((string)get_post('employee_id', ''));
$selected_employee_row = false;
if ($selected_employee !== '') {
    $selected_employee_row = get_employee_by_code($selected_employee);
    if (!$selected_employee_row) {
        display_error(_('The selected employee does not exist.'));
        $selected_employee = '';
    }
}
$show_paid = check_value('show_paid');

start_form();

start_table(TABLESTYLE2);
$employee_sql = "SELECT employee_id, CONCAT(employee_id, ' - ', first_name, ' ', last_name) as name
    FROM ".TB_PREF."employees WHERE !inactive";
label_row(_('Employee:'), combo_input('employee_id', $selected_employee, $employee_sql, 'employee_id', 'name', array(
    'spec_option' => _('-- All Employees --'),
    'spec_id' => ''
)));
check_row(_('Show Already Paid:'), 'show_paid', $show_paid);
end_table(1);

submit_center('refresh_list', _('Refresh'));

$rows = get_payment_advice_rows($selected_employee, $show_paid);
if (!$rows) {
    display_warning(_('Payslip header table is not available.'));
} else {
    start_table(TABLESTYLE);
    $th = array(_('Payslip #'), _('Employee'), _('From'), _('To'), _('Payable Amount'), _('Status'), _('Action'));
    table_header($th);

    $k = 0;
    while ($row = db_fetch_assoc($rows)) {
        alt_table_row_color($k);

        label_cell($row['payslip_id']);
        label_cell($row['employee_id'].' - '.$row['employee_name']);
        label_cell(!empty($row['from_date']) ? sql2date($row['from_date']) : '-');
        label_cell(!empty($row['to_date']) ? sql2date($row['to_date']) : '-');
        amount_cell((float)$row['payable_amount']);

        $status = isset($row['status']) ? (int)$row['status'] : 0;
        $status_txt = $status >= 3 ? _('Paid') : _('Pending');
        if (isset($row['payment_trans_no']) && !empty($row['payment_trans_no']))
            $status_txt .= ' #'.$row['payment_trans_no'];
        label_cell($status_txt);

        if ($status >= 3)
            label_cell('-');
        else
            submit_cells('MarkPaid'.$row['payslip_id'], _('Mark Paid'), false, '', '', false);

        end_row();
    }

    end_table(1);
}

end_form();

end_page();

