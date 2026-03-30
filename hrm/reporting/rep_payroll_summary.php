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
// NOTE: This file is included by reporting/rep881.php
// $path_to_root and session are already initialized when called via report framework.
// Direct access uses the above declarations.
include_once($path_to_root.'/includes/date_functions.inc');
include_once($path_to_root.'/includes/data_checks.inc');
include_once($path_to_root.'/hrm/includes/hrm_constants.inc');
include_once($path_to_root.'/hrm/includes/hrm_db.inc');

/**
 * Print payroll summary report.
 *
 * @return void
 */
function print_payroll_summary_report() {
    global $path_to_root;

    $payroll_period_id = isset($_POST['PARAM_0']) ? (int)$_POST['PARAM_0'] : 0;
    $department_id = isset($_POST['PARAM_1']) ? (int)$_POST['PARAM_1'] : 0;
    $comments = isset($_POST['PARAM_2']) ? $_POST['PARAM_2'] : '';
    $orientation = !empty($_POST['PARAM_3']) ? 1 : 0;
    $destination = isset($_POST['PARAM_4']) ? (int)$_POST['PARAM_4'] : 0;

    if ($destination)
        include_once($path_to_root.'/reporting/includes/excel_report.inc');
    else
        include_once($path_to_root.'/reporting/includes/pdf_report.inc');

    $rep = new FrontReport(_('Payroll Summary Report'), 'PayrollSummary', user_pagesize(), 9, $orientation ? 'L' : 'P');
    $cols = array(0, 60, 210, 280, 350, 430, 510, 620);
    $headers = array(_('ID'), _('Period'), _('From'), _('To'), _('Gross'), _('Deductions'), _('Net'));
    $aligns = array('left', 'left', 'left', 'left', 'right', 'right', 'right');

    if ($orientation)
        recalculate_cols($cols);

    $rep->Info(array(0 => $comments), $cols, $headers, $aligns);
    $rep->NewPage();

    $res = get_payroll_periods();
    $dec = user_price_dec();
    $printed_rows = 0;

    if (!$res) {
        $rep->TextCol(0, 3, _('No payroll periods found for selected criteria.'));
        $rep->End();
        return;
    }

    while ($row = db_fetch($res)) {
        if ($payroll_period_id > 0 && (int)$row['period_id'] !== $payroll_period_id)
            continue;

        if ($department_id > 0) {
            $row_department_id = isset($row['department_id']) ? (int)$row['department_id'] : 0;
            if ($row_department_id !== $department_id)
                continue;
        }

        $rep->TextCol(0, 1, $row['period_id']);
        $rep->TextCol(1, 2, $row['period_name']);
        $rep->TextCol(2, 3, sql2date($row['from_date']));
        $rep->TextCol(3, 4, sql2date($row['to_date']));
        $rep->AmountCol(4, 5, $row['total_gross'], $dec);
        $rep->AmountCol(5, 6, $row['total_deductions'], $dec);
        $rep->AmountCol(6, 7, $row['total_net'], $dec);
        $rep->NewLine();
        $printed_rows++;
    }

    if ($printed_rows === 0)
        $rep->TextCol(0, 3, _('No payroll periods found for selected criteria.'));

    $rep->End();
}

print_payroll_summary_report();
