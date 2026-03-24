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
// NOTE: This file is included by reporting/rep884.php
// $path_to_root and session are already initialized when called via report framework.
include_once($path_to_root.'/includes/date_functions.inc');
include_once($path_to_root.'/includes/data_checks.inc');
include_once($path_to_root.'/hrm/includes/hrm_constants.inc');
include_once($path_to_root.'/hrm/includes/hrm_db.inc');

/**
 * Fetch leave balance rows by report parameters.
 *
 * @param int $year
 * @param string $employee_id
 * @param int $leave_id
 * @return resource
 */
function get_report_leave_balance_rows($year, $employee_id='', $leave_id=0) {
    $where = array('lb.fiscal_year = '.db_escape((int)$year));

    if ($employee_id !== '' && $employee_id !== ALL_TEXT)
        $where[] = 'lb.employee_id = '.db_escape($employee_id);

    if ((int)$leave_id > 0)
        $where[] = 'lb.leave_id = '.db_escape((int)$leave_id);

    $sql = "SELECT lb.*, lt.leave_name,
        TRIM(CONCAT(COALESCE(e.first_name,''), ' ', COALESCE(e.last_name,''))) employee_name
        FROM ".TB_PREF."leave_balances lb
        LEFT JOIN ".TB_PREF."leave_types lt ON lt.leave_id = lb.leave_id
        LEFT JOIN ".TB_PREF."employees e ON e.employee_id = lb.employee_id
        WHERE ".implode(' AND ', $where)."
        ORDER BY lb.employee_id, lt.leave_name";

    return db_query($sql, 'could not get leave balance report rows');
}

/**
 * Render leave balance report.
 *
 * @return void
 */
function print_leave_balance_report() {
    global $path_to_root;

    $year = (int)$_POST['PARAM_0'];
    $employee_id = $_POST['PARAM_1'];
    $leave_id = (int)$_POST['PARAM_2'];
    $comments = $_POST['PARAM_3'];
    $orientation = !empty($_POST['PARAM_4']) ? 1 : 0;
    $destination = isset($_POST['PARAM_5']) ? (int)$_POST['PARAM_5'] : 0;

    if ($destination)
        include_once($path_to_root.'/reporting/includes/excel_report.inc');
    else
        include_once($path_to_root.'/reporting/includes/pdf_report.inc');

    $orientation = ($orientation ? 'L' : 'P');
    $rep = new FrontReport(_('Leave Balance Report'), 'LeaveBalanceReport', user_pagesize(), 9, $orientation);

    $cols = array(0, 80, 250, 330, 395, 460, 525, 590, 655);
    $headers = array(_('Emp ID'), _('Employee'), _('Leave Type'), _('Entitled'), _('Carry Fwd'), _('Adjusted'), _('Taken'), _('Available'));
    $aligns = array('left', 'left', 'left', 'right', 'right', 'right', 'right', 'right');

    $params = array(
        0 => $comments,
        1 => array('text' => _('Year'), 'from' => $year, 'to' => '')
    );

    if ($orientation == 'L')
        recalculate_cols($cols);

    $rep->Info($params, $cols, $headers, $aligns);
    $rep->NewPage();

    $result = get_report_leave_balance_rows($year, $employee_id, $leave_id);
    if (!$result || db_num_rows($result) == 0) {
        $rep->TextCol(0, 3, _('No leave balances found for selected criteria.'));
        $rep->End();
        return;
    }

    $dec = user_qty_dec();
    while ($row = db_fetch($result)) {
        $available = (float)$row['entitled'] + (float)$row['carried_forward'] + (float)$row['adjusted'] - (float)$row['taken'] - (float)$row['pending'];

        $rep->TextCol(0, 1, $row['employee_id']);
        $rep->TextCol(1, 2, $row['employee_name']);
        $rep->TextCol(2, 3, $row['leave_name']);
        $rep->AmountCol(3, 4, (float)$row['entitled'], $dec);
        $rep->AmountCol(4, 5, (float)$row['carried_forward'], $dec);
        $rep->AmountCol(5, 6, (float)$row['adjusted'], $dec);
        $rep->AmountCol(6, 7, (float)$row['taken'], $dec);
        $rep->AmountCol(7, 8, $available, $dec);
        $rep->NewLine();

        if ($rep->row < $rep->bottomMargin + $rep->lineHeight)
            $rep->NewPage();
    }

    $rep->End();
}

print_leave_balance_report();
