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
// NOTE: This file is included by reporting/rep889.php
// $path_to_root and session are already initialized when called via report framework.
// Direct access uses the above declarations.
include_once($path_to_root.'/includes/date_functions.inc');
include_once($path_to_root.'/includes/data_checks.inc');
include_once($path_to_root.'/hrm/includes/hrm_constants.inc');
include_once($path_to_root.'/hrm/includes/hrm_db.inc');

/**
 * Print overtime report.
 *
 * @return void
 */
function print_overtime_report() {
    global $path_to_root;

    $from_date = isset($_POST['PARAM_0']) ? $_POST['PARAM_0'] : begin_month(Today());
    $to_date = isset($_POST['PARAM_1']) ? $_POST['PARAM_1'] : end_month(Today());
    $comments = isset($_POST['PARAM_2']) ? $_POST['PARAM_2'] : '';
    $destination = isset($_POST['PARAM_6']) ? (int)$_POST['PARAM_6'] : 0;

    if ($destination)
        include_once($path_to_root.'/reporting/includes/excel_report.inc');
    else
        include_once($path_to_root.'/reporting/includes/pdf_report.inc');

    $rep = new FrontReport(_('Overtime Report'), 'OvertimeReport', user_pagesize(), 9, 'L');
    $cols = array(0, 80, 230, 330, 430, 530, 620);
    $headers = array(_('Emp ID'), _('Employee'), _('Date'), _('Regular Hrs'), _('OT Hrs'), _('OT Type'));
    $aligns = array('left', 'left', 'left', 'right', 'right', 'left');
    recalculate_cols($cols);
    $rep->Info(array(0 => $comments), $cols, $headers, $aligns);
    $rep->NewPage();

    $sql = "SELECT a.employee_id,
        TRIM(CONCAT(COALESCE(e.first_name,''), ' ', COALESCE(e.last_name,''))) employee_name,
        a.date, a.regular_hours, a.overtime_hours, COALESCE(ot.overtime_name, '') overtime_name
        FROM ".TB_PREF."attendance a
        LEFT JOIN ".TB_PREF."employees e ON e.employee_id = a.employee_id
        LEFT JOIN ".TB_PREF."overtime_types ot ON ot.overtime_id = a.overtime_type_id
        WHERE a.date >= ".db_escape(date2sql($from_date))."
            AND a.date <= ".db_escape(date2sql($to_date))."
            AND IFNULL(a.overtime_hours,0) > 0
        ORDER BY a.employee_id, a.date";

    $dec = user_qty_dec();
    $res = db_query($sql, 'could not get overtime report rows');
    while ($row = db_fetch($res)) {
        $rep->TextCol(0, 1, $row['employee_id']);
        $rep->TextCol(1, 2, $row['employee_name']);
        $rep->TextCol(2, 3, sql2date($row['date']));
        $rep->AmountCol(3, 4, $row['regular_hours'], $dec);
        $rep->AmountCol(4, 5, $row['overtime_hours'], $dec);
        $rep->TextCol(5, 6, $row['overtime_name']);
        $rep->NewLine();
    }

    $rep->End();
}

print_overtime_report();
