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
*******************************************************************************/
$page_security = 'SA_HRMREPORTS';
if (!isset($path_to_root) || $path_to_root == '')
    $path_to_root  = '../..';
// NOTE: This file is included by reporting/rep882.php
// $path_to_root and session are already initialized when called via report framework.
// Direct access uses the above declarations.
include_once($path_to_root.'/includes/date_functions.inc');
include_once($path_to_root.'/includes/data_checks.inc');
include_once($path_to_root.'/hrm/includes/hrm_constants.inc');
include_once($path_to_root.'/hrm/includes/hrm_db.inc');

/**
 * Print salary sheet report.
 *
 * @return void
 */
function print_salary_sheet_report() {
    global $path_to_root;

    $year = isset($_POST['PARAM_0']) ? (int)$_POST['PARAM_0'] : (int)date('Y');
    $month = isset($_POST['PARAM_1']) ? (int)$_POST['PARAM_1'] : (int)date('n');
    $department_id = isset($_POST['PARAM_2']) ? (int)$_POST['PARAM_2'] : 0;
    $employee_id = isset($_POST['PARAM_3']) ? $_POST['PARAM_3'] : '';
    $comments = isset($_POST['PARAM_4']) ? $_POST['PARAM_4'] : '';
    $orientation = !empty($_POST['PARAM_5']) ? 1 : 0;
    $destination = isset($_POST['PARAM_6']) ? (int)$_POST['PARAM_6'] : 0;

    if ($destination)
        include_once($path_to_root.'/reporting/includes/excel_report.inc');
    else
        include_once($path_to_root.'/reporting/includes/pdf_report.inc');

    $table = payslip_header_table();
    if (!$table)
        return;

    $emp_col = payslip_has_column($table, 'employee_id') ? 'employee_id' : 'emp_id';
    $gross_col = payslip_has_column($table, 'gross_salary') ? 'gross_salary' : 'salary_amount';
    $ded_col = payslip_has_column($table, 'total_deductions') ? 'total_deductions' : null;
    $net_col = payslip_has_column($table, 'net_salary') ? 'net_salary' : 'payable_amount';
    $date_col = payslip_has_column($table, 'from_date') ? 'from_date' : (payslip_has_column($table, 'tran_date') ? 'tran_date' : 'to_date');

    $rep = new FrontReport(_('Salary Sheet'), 'SalarySheet', user_pagesize(), 9, $orientation ? 'L' : 'P');
    $cols = array(0, 80, 230, 340, 430, 520, 620);
    $headers = array(_('Emp ID'), _('Employee'), _('Gross'), _('Deductions'), _('Net'), _('Reference'));
    $aligns = array('left', 'left', 'right', 'right', 'right', 'left');

    if ($orientation)
        recalculate_cols($cols);

    $rep->Info(array(0 => $comments), $cols, $headers, $aligns);
    $rep->NewPage();

    $where = array(
        "YEAR(p.$date_col) = ".db_escape($year),
        "MONTH(p.$date_col) = ".db_escape($month)
    );

    if ($employee_id !== '' && $employee_id !== ALL_TEXT)
        $where[] = "p.$emp_col = ".db_escape($employee_id);

    if ($department_id > 0)
        $where[] = "e.department_id = ".db_escape($department_id);

    $sql = "SELECT p.$emp_col employee_id,
        TRIM(CONCAT(COALESCE(e.first_name,''), ' ', COALESCE(e.last_name,''))) employee_name,
        p.$gross_col gross_salary,
        ".($ded_col ? 'p.'.$ded_col : '0')." total_deductions,
        p.$net_col net_salary,
        p.reference
        FROM ".TB_PREF.$table." p
        LEFT JOIN ".TB_PREF."employees e ON e.employee_id = p.$emp_col
        WHERE ".implode(' AND ', $where)."
        ORDER BY p.$emp_col";

    $dec = user_price_dec();
    $res = db_query($sql, 'could not get salary sheet report rows');

    if (!$res || db_num_rows($res) == 0) {
        $rep->TextCol(0, 3, _('No salary sheet rows found for selected criteria.'));
        $rep->End();
        return;
    }

    while ($row = db_fetch($res)) {
        $rep->TextCol(0, 1, $row['employee_id']);
        $rep->TextCol(1, 2, $row['employee_name']);
        $rep->AmountCol(2, 3, $row['gross_salary'], $dec);
        $rep->AmountCol(3, 4, $row['total_deductions'], $dec);
        $rep->AmountCol(4, 5, $row['net_salary'], $dec);
        $rep->TextCol(5, 6, $row['reference']);
        $rep->NewLine();
    }

    $rep->End();
}

print_salary_sheet_report();
