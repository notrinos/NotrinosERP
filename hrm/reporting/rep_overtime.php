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

    $year = isset($_POST['PARAM_0']) ? (int)$_POST['PARAM_0'] : (int)date('Y');
    $from_month = isset($_POST['PARAM_1']) ? (int)$_POST['PARAM_1'] : (int)date('n');
    $to_month = isset($_POST['PARAM_2']) ? (int)$_POST['PARAM_2'] : (int)date('n');
    $employee_id = isset($_POST['PARAM_3']) ? $_POST['PARAM_3'] : '';
    $department_id = isset($_POST['PARAM_4']) ? (int)$_POST['PARAM_4'] : 0;
    $comments = isset($_POST['PARAM_5']) ? $_POST['PARAM_5'] : '';
    $orientation = !empty($_POST['PARAM_6']) ? 1 : 0;
    $destination = isset($_POST['PARAM_7']) ? (int)$_POST['PARAM_7'] : 0;

    $from_month = max(1, min(12, $from_month));
    $to_month = max(1, min(12, $to_month));
    if ($from_month > $to_month) {
        $tmp = $from_month;
        $from_month = $to_month;
        $to_month = $tmp;
    }

    if ($destination)
        include_once($path_to_root.'/reporting/includes/excel_report.inc');
    else
        include_once($path_to_root.'/reporting/includes/pdf_report.inc');

    $rep = new FrontReport(_('Overtime Report'), 'OvertimeReport', user_pagesize(), 9, $orientation ? 'L' : 'P');
    $cols = array(0, 80, 230, 330, 430, 530, 620);
    $headers = array(_('Emp ID'), _('Employee'), _('Date'), _('Regular Hrs'), _('OT Hrs'), _('OT Type'));
    $aligns = array('left', 'left', 'left', 'right', 'right', 'left');

    if ($orientation)
        recalculate_cols($cols);

    $rep->Info(
        array(
            0 => $comments,
            1 => array('text' => _('Year'), 'from' => $year, 'to' => ''),
            2 => array('text' => _('Months'), 'from' => $from_month, 'to' => $to_month)
        ),
        $cols,
        $headers,
        $aligns
    );
    $rep->NewPage();

    $overtime_join = "LEFT JOIN ".TB_PREF."overtime ot ON ot.overtime_id = a.overtime_type_id";
    if (function_exists('employee_table_exists') && employee_table_exists('overtime_types'))
        $overtime_join = "LEFT JOIN ".TB_PREF."overtime_types ot ON ot.overtime_id = a.overtime_type_id";

    $where = array(
        'YEAR(a.date) = '.db_escape($year),
        'MONTH(a.date) >= '.db_escape($from_month),
        'MONTH(a.date) <= '.db_escape($to_month),
        'IFNULL(a.overtime_hours,0) > 0'
    );

    if ($employee_id !== '' && $employee_id !== ALL_TEXT)
        $where[] = 'a.employee_id = '.db_escape($employee_id);

    if ($department_id > 0)
        $where[] = 'e.department_id = '.db_escape($department_id);

    $sql = "SELECT a.employee_id,
        TRIM(CONCAT(COALESCE(e.first_name,''), ' ', COALESCE(e.last_name,''))) employee_name,
        a.date, a.regular_hours, a.overtime_hours, COALESCE(ot.overtime_name, '') overtime_name
        FROM ".TB_PREF."attendance a
        LEFT JOIN ".TB_PREF."employees e ON e.employee_id = a.employee_id
        $overtime_join
        WHERE ".implode(' AND ', $where)."
        ORDER BY a.employee_id, a.date";

    $dec = user_qty_dec();
    $res = db_query($sql, 'could not get overtime report rows');

    if (!$res || db_num_rows($res) == 0) {
        $rep->TextCol(0, 3, _('No overtime rows found for selected criteria.'));
        $rep->End();
        return;
    }

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
