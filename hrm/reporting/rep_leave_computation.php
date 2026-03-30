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
// NOTE: This file is included by reporting/rep885.php
// $path_to_root and session are already initialized when called via report framework.
// Direct access uses the above declarations.
include_once($path_to_root.'/includes/date_functions.inc');
include_once($path_to_root.'/includes/data_checks.inc');
include_once($path_to_root.'/hrm/includes/hrm_constants.inc');
include_once($path_to_root.'/hrm/includes/hrm_db.inc');

/**
 * Print leave computation report.
 *
 * @return void
 */
function print_leave_computation_report() {
    global $path_to_root;

    $from_date = isset($_POST['PARAM_0']) ? $_POST['PARAM_0'] : begin_month(Today());
    $to_date = isset($_POST['PARAM_1']) ? $_POST['PARAM_1'] : end_month(Today());
    $employee_id = isset($_POST['PARAM_2']) ? $_POST['PARAM_2'] : '';
    $leave_id = isset($_POST['PARAM_3']) ? (int)$_POST['PARAM_3'] : 0;
    $comments = isset($_POST['PARAM_4']) ? $_POST['PARAM_4'] : '';
    $orientation = !empty($_POST['PARAM_5']) ? 1 : 0;
    $destination = isset($_POST['PARAM_6']) ? (int)$_POST['PARAM_6'] : 0;

    $to_date_sql = is_date($to_date) ? date2sql($to_date) : date('Y-m-d');
    $fiscal_year = (int)substr($to_date_sql, 0, 4);

    if ($destination)
        include_once($path_to_root.'/reporting/includes/excel_report.inc');
    else
        include_once($path_to_root.'/reporting/includes/pdf_report.inc');

    $rep = new FrontReport(_('Leave Computation Report'), 'LeaveComputation', user_pagesize(), 9, $orientation ? 'L' : 'P');
    $cols = array(0, 80, 220, 310, 390, 470, 550, 630);
    $headers = array(_('Emp ID'), _('Employee'), _('Leave Type'), _('Entitled'), _('Taken'), _('Pending'), _('Available'));
    $aligns = array('left', 'left', 'left', 'right', 'right', 'right', 'right');

    if ($orientation)
        recalculate_cols($cols);

    $rep->Info(
        array(
            0 => $comments,
            1 => array('text' => _('From Date'), 'from' => $from_date, 'to' => ''),
            2 => array('text' => _('To Date'), 'from' => $to_date, 'to' => '')
        ),
        $cols,
        $headers,
        $aligns
    );
    $rep->NewPage();

    if (function_exists('ensure_leave_balance_entitlements_for_filters'))
        ensure_leave_balance_entitlements_for_filters($fiscal_year, $employee_id, $leave_id);

    $res = get_leave_balances($fiscal_year, $employee_id, $leave_id);
    $dec = user_qty_dec();

    if (!$res || db_num_rows($res) == 0) {
        $rep->TextCol(0, 3, _('No leave computation rows found for selected criteria.'));
        $rep->End();
        return;
    }

    while ($row = db_fetch($res)) {
        $available = (float)$row['entitled'] + (float)$row['carried_forward'] + (float)$row['adjusted'] - (float)$row['taken'] - (float)$row['pending'];
        $rep->TextCol(0, 1, $row['employee_id']);
        $rep->TextCol(1, 2, $row['employee_name']);
        $rep->TextCol(2, 3, $row['leave_name']);
        $rep->AmountCol(3, 4, $row['entitled'], $dec);
        $rep->AmountCol(4, 5, $row['taken'], $dec);
        $rep->AmountCol(5, 6, $row['pending'], $dec);
        $rep->AmountCol(6, 7, $available, $dec);
        $rep->NewLine();
    }

    $rep->End();
}

print_leave_computation_report();
