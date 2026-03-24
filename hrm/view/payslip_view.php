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
$page_security = 'SA_PAYSLIPINQUIRY';
$path_to_root = "../..";
include($path_to_root . "/includes/session.inc");
include_once($path_to_root.'/includes/ui.inc');
include_once($path_to_root.'/hrm/includes/hrm_db.inc');

page(_("Payslip View"));

/**
 * Get payslip id column for active payslip header table.
 *
 * @return string
 */
function payslip_view_id_column() {
    $table_name = payslip_header_table();
    if ($table_name && payslip_has_column($table_name, 'payslip_id'))
        return 'payslip_id';

    return 'payslip_no';
}

/**
 * Build a dropdown list of recent payslips.
 *
 * @param int $limit
 * @return array
 */
function get_payslip_view_list($limit=200) {
    $table_name = payslip_header_table();
    if (!$table_name)
        return array();

    $id_col = payslip_view_id_column();
    $emp_col = payslip_has_column($table_name, 'employee_id') ? 'employee_id' : 'emp_id';
    $date_col = payslip_has_column($table_name, 'tran_date') ? 'tran_date' : (payslip_has_column($table_name, 'from_date') ? 'from_date' : $id_col);

    $sql = "SELECT p.$id_col id, p.$emp_col employee_code, p.from_date, p.to_date
        FROM ".TB_PREF.$table_name." p
        ORDER BY p.$date_col DESC, p.$id_col DESC
        LIMIT ".(int)$limit;
    $result = db_query($sql, 'could not get payslip list');

    $list = array('' => _('Select a payslip'));
    while ($row = db_fetch($result)) {
        $label = '#'.$row['id'].' - '.$row['employee_code'];
        if (!empty($row['from_date']) || !empty($row['to_date']))
            $label .= ' ('.sql2date($row['from_date']).' - '.sql2date($row['to_date']).')';
        $list[$row['id']] = $label;
    }

    return $list;
}

/**
 * Get display employee name by employee_id code.
 *
 * @param string $employee_code
 * @return string
 */
function get_payslip_view_employee_name($employee_code) {
    if ($employee_code === '' || !employee_table_exists('employees'))
        return '';

    $sql = "SELECT CONCAT(COALESCE(first_name,''), ' ', COALESCE(last_name,'')) AS employee_name
        FROM ".TB_PREF."employees
        WHERE employee_id = ".db_escape($employee_code);
    $result = db_query($sql, 'could not get employee name');
    $row = db_fetch($result);

    return $row && !empty($row['employee_name']) ? trim($row['employee_name']) : '';
}

if (isset($_GET['payslip_id']))
    $_POST['payslip_id'] = (int)$_GET['payslip_id'];
elseif (isset($_GET['trans_no']))
    $_POST['payslip_id'] = (int)$_GET['trans_no'];

if (!isset($_POST['payslip_id']))
    $_POST['payslip_id'] = '';

start_form();
start_table(TABLESTYLE_NOBORDER);
start_row();
label_cell(_('Payslip:'));
$payslip_list = get_payslip_view_list();
array_selector_cell('payslip_id', null, $payslip_list, array('select_submit' => true));
submit_cells('show', _('Show'), '', _('Show selected payslip'), 'default');
end_row();
end_table(1);

$selected_id = (int)$_POST['payslip_id'];
if ($selected_id > 0) {
    $header = get_payslip($selected_id);
    if (!$header) {
        display_error(_('Selected payslip was not found.'));
    } else {
        $id_col = payslip_view_id_column();
        $emp_col = isset($header['employee_id']) ? 'employee_id' : 'emp_id';
        $employee_code = isset($header[$emp_col]) ? $header[$emp_col] : '';
        $employee_name = get_payslip_view_employee_name((string)$employee_code);

        start_table(TABLESTYLE2);
        label_row(_('Payslip #:'), $header[$id_col]);
        label_row(_('Employee ID:'), $employee_code);
        if ($employee_name !== '')
            label_row(_('Employee Name:'), $employee_name);
        if (isset($header['from_date']))
            label_row(_('From Date:'), sql2date($header['from_date']));
        if (isset($header['to_date']))
            label_row(_('To Date:'), sql2date($header['to_date']));
        if (isset($header['tran_date']))
            label_row(_('Transaction Date:'), sql2date($header['tran_date']));
        if (isset($header['gross_salary']))
            amount_row(_('Gross Salary:'), $header['gross_salary']);
        if (isset($header['total_deductions']))
            amount_row(_('Total Deductions:'), $header['total_deductions']);
        if (isset($header['net_salary']))
            amount_row(_('Net Salary:'), $header['net_salary']);
        end_table(1);

        $details = get_payslip_details($selected_id);
        if ($details && db_num_rows($details) > 0) {
            start_table(TABLESTYLE);
            table_header(array(_('Element'), _('Category'), _('Amount Type'), _('Earnings'), _('Deductions')));

            while ($line = db_fetch($details)) {
                $line_name = isset($line['element_name']) && $line['element_name'] !== ''
                    ? $line['element_name']
                    : (isset($line['element_id']) ? '#'.$line['element_id'] : '');
                $category = isset($line['element_category']) ? (int)$line['element_category'] : 0;
                $amount_type = isset($line['amount_type']) ? (int)$line['amount_type'] : 0;
                $final_amount = isset($line['final_amount']) ? (float)$line['final_amount'] : 0;
                $is_deduction = !empty($line['is_deduction']);

                label_cell($line_name);
                label_cell((string)$category);
                label_cell((string)$amount_type);
                if ($is_deduction) {
                    label_cell('');
                    amount_cell($final_amount);
                } else {
                    amount_cell($final_amount);
                    label_cell('');
                }
                end_row();
            }

            end_table(1);
        } else {
            display_note(_('No payslip line details found for this payslip.'), 0, 1);
        }
    }
}

end_form();

end_page();

