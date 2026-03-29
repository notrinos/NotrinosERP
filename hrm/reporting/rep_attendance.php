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
// NOTE: This file is included by reporting/rep883.php
// $path_to_root and session are already initialized when called via report framework.
// Direct access uses the above declarations.
include_once($path_to_root.'/includes/date_functions.inc');
include_once($path_to_root.'/includes/data_checks.inc');
include_once($path_to_root.'/hrm/includes/hrm_constants.inc');
include_once($path_to_root.'/hrm/includes/hrm_db.inc');

/**
 * Fetch attendance summary rows for report.
 *
 * @param int $year
 * @param int $month
 * @param int $department_id
 * @param string $employee_id
 * @return resource
 */
function get_report_attendance_rows($year, $month, $department_id=0, $employee_id='') {
    $where = array(
        "YEAR(a.date) = ".db_escape((int)$year),
        "MONTH(a.date) = ".db_escape((int)$month)
    );

    if ((int)$department_id > 0)
        $where[] = "e.department_id = ".db_escape((int)$department_id);

    if ($employee_id !== '' && $employee_id !== ALL_TEXT)
        $where[] = "a.employee_id = ".db_escape($employee_id);

    $sql = "SELECT
            a.employee_id,
            TRIM(CONCAT(COALESCE(e.first_name,''), ' ', COALESCE(e.last_name,''))) employee_name,
            COALESCE(d.department_name, '') department_name,
            SUM(CASE WHEN a.status = 0 THEN 1 ELSE 0 END) worked_days,
            SUM(CASE WHEN a.status = 1 THEN 1 ELSE 0 END) absent_days,
            SUM(IFNULL(a.overtime_hours, 0)) overtime_hours,
            SUM(IFNULL(a.regular_hours, 0)) regular_hours
        FROM ".TB_PREF."attendance a
        LEFT JOIN ".TB_PREF."employees e ON e.employee_id = a.employee_id
        LEFT JOIN ".TB_PREF."departments d ON d.department_id = e.department_id
        WHERE ".implode(' AND ', $where)."
        GROUP BY a.employee_id, employee_name, department_name
        ORDER BY a.employee_id";

    return db_query($sql, 'could not get attendance report rows');
}

/**
 * Render attendance report.
 *
 * @return void
 */
function print_attendance_report() {
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
    $rep = new FrontReport(_('Attendance Report'), 'AttendanceReport', user_pagesize(), 9, $orientation);

    $cols = array(0, 70, 210, 330, 400, 470, 540, 620);
    $headers = array(_('Emp ID'), _('Employee'), _('Department'), _('Worked Days'), _('Absent Days'), _('Regular Hrs'), _('Overtime Hrs'));
    $aligns = array('left', 'left', 'left', 'right', 'right', 'right', 'right');

    $params = array(
        0 => $comments,
        1 => array('text' => _('Year'), 'from' => $year, 'to' => ''),
        2 => array('text' => _('Month'), 'from' => $month, 'to' => '')
    );

    if ($orientation == 'L')
        recalculate_cols($cols);

    $rep->Info($params, $cols, $headers, $aligns);
    $rep->NewPage();

    $result = get_report_attendance_rows($year, $month, $department_id, $employee_id);
    if (!$result || db_num_rows($result) == 0) {
        $rep->TextCol(0, 3, _('No attendance data found for selected criteria.'));
        $rep->End();
        return;
    }

    $dec = user_qty_dec();
    while ($row = db_fetch($result)) {
        $rep->TextCol(0, 1, $row['employee_id']);
        $rep->TextCol(1, 2, $row['employee_name']);
        $rep->TextCol(2, 3, $row['department_name']);
        $rep->AmountCol(3, 4, (float)$row['worked_days'], $dec);
        $rep->AmountCol(4, 5, (float)$row['absent_days'], $dec);
        $rep->AmountCol(5, 6, (float)$row['regular_hours'], $dec);
        $rep->AmountCol(6, 7, (float)$row['overtime_hours'], $dec);
        $rep->NewLine();

        if ($rep->row < $rep->bottomMargin + $rep->lineHeight)
            $rep->NewPage();
    }

    $rep->End();
}

print_attendance_report();
