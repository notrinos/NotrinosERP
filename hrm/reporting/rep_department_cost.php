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
$path_to_root  = '../..';
// NOTE: This file is included by reporting/rep890.php
// $path_to_root and session are already initialized when called via report framework.
// Direct access uses the above declarations.
include_once($path_to_root.'/includes/date_functions.inc');
include_once($path_to_root.'/includes/data_checks.inc');
include_once($path_to_root.'/hrm/includes/hrm_constants.inc');
include_once($path_to_root.'/hrm/includes/hrm_db.inc');

/**
 * Print department cost report.
 *
 * @return void
 */
function print_department_cost_report() {
    global $path_to_root;

    $from_date = isset($_POST['PARAM_0']) ? $_POST['PARAM_0'] : begin_month(Today());
    $to_date = isset($_POST['PARAM_1']) ? $_POST['PARAM_1'] : end_month(Today());
    $comments = isset($_POST['PARAM_2']) ? $_POST['PARAM_2'] : '';
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
    $from_col = payslip_has_column($table, 'from_date') ? 'from_date' : 'tran_date';

    $rep = new FrontReport(_('Department Cost Report'), 'DepartmentCost', user_pagesize(), 9, 'P');
    $cols = array(0, 220, 340, 440, 540);
    $headers = array(_('Department'), _('Gross'), _('Deductions'), _('Net'));
    $aligns = array('left', 'right', 'right', 'right');
    $rep->Info(array(0 => $comments), $cols, $headers, $aligns);
    $rep->NewPage();

    $sql = "SELECT COALESCE(d.department_name, 'Unassigned') department_name,
        SUM(IFNULL(p.$gross_col,0)) gross_total,
        SUM(IFNULL(".($ded_col ? 'p.'.$ded_col : '0').",0)) ded_total,
        SUM(IFNULL(p.$net_col,0)) net_total
        FROM ".TB_PREF.$table." p
        JOIN ".TB_PREF."employees e ON e.employee_id = p.$emp_col
        LEFT JOIN ".TB_PREF."departments d ON d.department_id = e.department_id
        WHERE p.$from_col >= ".db_escape(date2sql($from_date))."
            AND p.$from_col <= ".db_escape(date2sql($to_date))."
        GROUP BY d.department_name
        ORDER BY d.department_name";

    $res = db_query($sql, 'could not get department cost report rows');
    $dec = user_price_dec();
    while ($row = db_fetch($res)) {
        $rep->TextCol(0, 1, $row['department_name']);
        $rep->AmountCol(1, 2, $row['gross_total'], $dec);
        $rep->AmountCol(2, 3, $row['ded_total'], $dec);
        $rep->AmountCol(3, 4, $row['net_total'], $dec);
        $rep->NewLine();
    }

    $rep->End();
}

print_department_cost_report();
