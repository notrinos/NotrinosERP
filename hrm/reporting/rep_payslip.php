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
// NOTE: This file is included by reporting/rep880.php
// $path_to_root and session are already initialized when called via report framework.
// Direct access uses the above declarations.
include_once($path_to_root.'/includes/date_functions.inc');
include_once($path_to_root.'/includes/data_checks.inc');
include_once($path_to_root.'/hrm/includes/hrm_constants.inc');
include_once($path_to_root.'/hrm/includes/hrm_db.inc');

/**
 * Build payslip report row source by selected filters.
 *
 * @param int $year
 * @param int $month
 * @param int $department_id
 * @param string $employee_id
 * @return resource|false
 */
function get_report_payslips($year, $month, $department_id=0, $employee_id='') {
    $table_name = payslip_header_table();
    if (!$table_name)
        return false;

    $id_col = payslip_has_column($table_name, 'payslip_id') ? 'payslip_id' : 'payslip_no';
    $emp_col = payslip_has_column($table_name, 'employee_id') ? 'employee_id' : 'emp_id';
    $date_col = payslip_has_column($table_name, 'from_date') ? 'from_date' : (payslip_has_column($table_name, 'tran_date') ? 'tran_date' : 'to_date');

    $employee_join = employee_table_exists('employees')
        ? " LEFT JOIN ".TB_PREF."employees e ON e.employee_id = p.$emp_col "
        : '';
    $department_join = employee_table_exists('employees')
        ? " LEFT JOIN ".TB_PREF."departments d ON d.department_id = e.department_id "
        : '';

    $select_name = employee_table_exists('employees')
        ? "TRIM(CONCAT(COALESCE(e.first_name,''), ' ', COALESCE(e.last_name,''))) employee_name"
        : "'' employee_name";

    $where = array(
        "YEAR(p.$date_col) = ".db_escape((int)$year),
        "MONTH(p.$date_col) = ".db_escape((int)$month)
    );

    if ($employee_id !== '' && $employee_id !== ALL_TEXT)
        $where[] = "p.$emp_col = ".db_escape($employee_id);

    if ((int)$department_id > 0 && employee_table_exists('employees'))
        $where[] = "e.department_id = ".db_escape((int)$department_id);

    $sql = "SELECT p.*, p.$id_col payslip_row_id, p.$emp_col employee_code,
        $select_name,
        COALESCE(d.department_name, '') department_name
        FROM ".TB_PREF.$table_name." p
        $employee_join
        $department_join
        WHERE ".implode(' AND ', $where)."
        ORDER BY p.$emp_col, p.$id_col";

    return db_query($sql, 'could not retrieve payslip report data');
}

/**
 * Get payslip line details for report output.
 *
 * @param int $payslip_id
 * @return resource|false
 */
function get_report_payslip_lines($payslip_id) {
    if (!payslip_table_exists('payslip_details'))
        return false;

    $sql = "SELECT * FROM ".TB_PREF."payslip_details
        WHERE payslip_id = ".db_escape((int)$payslip_id)."
        ORDER BY display_order, detail_id";

    return db_query($sql, 'could not retrieve payslip report lines');
}

/**
 * Render payslip print report.
 *
 * @param bool $email_mode
 * @return void
 */
function print_payslip_report($email_mode=false) {
    global $path_to_root;

    $year = (int)$_POST['PARAM_0'];
    $month = (int)$_POST['PARAM_1'];
    $department_id = (int)$_POST['PARAM_2'];
    $employee_id = $_POST['PARAM_3'];
    $comments = $_POST['PARAM_4'];
    $orientation = !empty($_POST['PARAM_5']) ? 1 : 0;
    $destination = isset($_POST['PARAM_6']) ? (int)$_POST['PARAM_6'] : 0;

    if ($destination)
        include_once($path_to_root.'/reporting/includes/excel_report.inc');
    else
        include_once($path_to_root.'/reporting/includes/pdf_report.inc');

    $orientation = ($orientation ? 'L' : 'P');
    $report_title = $email_mode ? _('Email Payslip') : _('Payslip Print');
    $report_name = $email_mode ? 'EmailPayslip' : 'PayslipPrint';

    $rep = new FrontReport($report_title, $report_name, user_pagesize(), 9, $orientation);
    $dec = user_price_dec();

    $cols = array(0, 90, 220, 310, 420, 520);
    $headers = array(_('Element'), _('Category'), _('Type'), _('Earnings'), _('Deductions'));
    $aligns = array('left', 'left', 'left', 'right', 'right');

    $params = array(
        0 => $comments,
        1 => array('text' => _('Year'), 'from' => $year, 'to' => ''),
        2 => array('text' => _('Month'), 'from' => $month, 'to' => '')
    );

    if ($orientation == 'L')
        recalculate_cols($cols);

    $rows = get_report_payslips($year, $month, $department_id, $employee_id);
    if (!$rows || db_num_rows($rows) == 0) {
        $rep->Font();
        $rep->Info($params, $cols, $headers, $aligns);
        $rep->NewPage();
        $rep->TextCol(0, 3, _('No payslips found for selected criteria.'));
        $rep->End();
        return;
    }

    while ($row = db_fetch($rows)) {
        $rep->Font();
        $rep->Info($params, $cols, $headers, $aligns);
        $rep->NewPage();

        $rep->TextCol(0, 2, _('Payslip #').': '.$row['payslip_row_id']);
        $rep->TextCol(2, 5, _('Employee').': '.$row['employee_code'].' '.$row['employee_name']);
        $rep->NewLine();
        if (!empty($row['department_name'])) {
            $rep->TextCol(0, 2, _('Department').': '.$row['department_name']);
            $rep->NewLine();
        }
        if (!empty($row['from_date']) || !empty($row['to_date'])) {
            $rep->TextCol(0, 3, _('Period').': '.sql2date($row['from_date']).' - '.sql2date($row['to_date']));
            $rep->NewLine();
        }
        $rep->Line($rep->row - 2);
        $rep->NewLine();

        $lines = get_report_payslip_lines((int)$row['payslip_row_id']);
        if ($lines && db_num_rows($lines) > 0) {
            while ($line = db_fetch($lines)) {
                $line_name = isset($line['element_name']) && $line['element_name'] !== ''
                    ? $line['element_name']
                    : (isset($line['element_id']) ? '#'.$line['element_id'] : '');

                $rep->TextCol(0, 1, $line_name);
                $rep->TextCol(1, 2, isset($line['element_category']) ? $line['element_category'] : '');
                $rep->TextCol(2, 3, isset($line['amount_type']) ? $line['amount_type'] : '');

                $amount = isset($line['final_amount']) ? (float)$line['final_amount'] : 0;
                if (!empty($line['is_deduction'])) {
                    $rep->TextCol(3, 4, '');
                    $rep->AmountCol(4, 5, $amount, $dec);
                } else {
                    $rep->AmountCol(3, 4, $amount, $dec);
                    $rep->TextCol(4, 5, '');
                }

                $rep->NewLine();
                if ($rep->row < $rep->bottomMargin + (4 * $rep->lineHeight)) {
                    $rep->NewPage();
                }
            }
        } else {
            $rep->TextCol(0, 3, _('No payslip detail lines found.'));
            $rep->NewLine(2);
        }

        $rep->Line($rep->row - 2);
        $rep->Font('bold');
        if (isset($row['gross_salary'])) {
            $rep->TextCol(2, 4, _('Gross Salary'));
            $rep->AmountCol(4, 5, $row['gross_salary'], $dec);
            $rep->NewLine();
        }
        if (isset($row['total_deductions'])) {
            $rep->TextCol(2, 4, _('Total Deductions'));
            $rep->AmountCol(4, 5, $row['total_deductions'], $dec);
            $rep->NewLine();
        }
        if (isset($row['net_salary'])) {
            $rep->TextCol(2, 4, _('Net Salary'));
            $rep->AmountCol(4, 5, $row['net_salary'], $dec);
            $rep->NewLine();
        }
        $rep->Font();
    }

    $rep->End();
}

print_payslip_report(defined('HRM_PAYSLIP_EMAIL_REPORT'));