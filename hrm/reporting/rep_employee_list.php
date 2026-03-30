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
// NOTE: This file is included by reporting/rep886.php
// $path_to_root and session are already initialized when called via report framework.
// Direct access uses the above declarations.
include_once($path_to_root.'/includes/date_functions.inc');
include_once($path_to_root.'/includes/data_checks.inc');
include_once($path_to_root.'/hrm/includes/hrm_constants.inc');
include_once($path_to_root.'/hrm/includes/hrm_db.inc');

/**
 * Print employee list report.
 *
 * @return void
 */
function print_employee_list_report() {
    global $path_to_root;

    $department_id = isset($_POST['PARAM_0']) ? (int)$_POST['PARAM_0'] : 0;
    $position_id = isset($_POST['PARAM_1']) ? (int)$_POST['PARAM_1'] : 0;
    $grade_id = isset($_POST['PARAM_2']) ? (int)$_POST['PARAM_2'] : 0;
    $status_filter = isset($_POST['PARAM_3']) ? trim((string)$_POST['PARAM_3']) : '';
    $show_inactive = !empty($_POST['PARAM_4']) ? 1 : 0;
    $comments = isset($_POST['PARAM_5']) ? $_POST['PARAM_5'] : '';
    $orientation = !empty($_POST['PARAM_6']) ? 1 : 0;
    $destination = isset($_POST['PARAM_7']) ? (int)$_POST['PARAM_7'] : 0;

    if ($destination)
        include_once($path_to_root.'/reporting/includes/excel_report.inc');
    else
        include_once($path_to_root.'/reporting/includes/pdf_report.inc');

    $rep = new FrontReport(_('Employee List'), 'EmployeeList', user_pagesize(), 9, $orientation ? 'L' : 'P');
    $cols = array(0, 70, 230, 340, 450, 540, 620);
    $headers = array(_('Employee ID'), _('Employee Name'), _('Department'), _('Position'), _('Grade'), _('Status'));
    $aligns = array('left', 'left', 'left', 'left', 'left', 'left');
    if ($orientation)
        recalculate_cols($cols);
    $rep->Info(array(0 => $comments), $cols, $headers, $aligns);
    $rep->NewPage();

    $where = array('1=1');

    if ($department_id > 0)
        $where[] = "e.department_id = ".db_escape($department_id);

    if ($position_id > 0)
        $where[] = "e.position_id = ".db_escape($position_id);

    if ($grade_id > 0)
        $where[] = "e.grade_id = ".db_escape($grade_id);

    if ($status_filter === 'active')
        $where[] = "e.inactive = 0";
    elseif ($status_filter === 'inactive')
        $where[] = "e.inactive = 1";
    elseif (!$show_inactive)
        $where[] = "e.inactive = 0";

    $sql = "SELECT e.employee_id,
        TRIM(CONCAT(COALESCE(e.first_name,''), ' ', COALESCE(e.last_name,''))) employee_name,
        COALESCE(d.department_name, '') department_name,
        COALESCE(p.position_name, '') position_name,
        COALESCE(g.grade_name, '') grade_name,
        e.inactive
        FROM ".TB_PREF."employees e
        LEFT JOIN ".TB_PREF."departments d ON d.department_id = e.department_id
        LEFT JOIN ".TB_PREF."positions p ON p.position_id = e.position_id
        LEFT JOIN ".TB_PREF."pay_grades g ON g.grade_id = e.grade_id
        WHERE ".implode(' AND ', $where)."
        ORDER BY e.employee_id";

    $res = db_query($sql, 'could not get employee list report rows');

    if (!$res || db_num_rows($res) == 0) {
        $rep->TextCol(0, 3, _('No employees found for selected criteria.'));
        $rep->End();
        return;
    }

    while ($row = db_fetch($res)) {
        $rep->TextCol(0, 1, $row['employee_id']);
        $rep->TextCol(1, 2, $row['employee_name']);
        $rep->TextCol(2, 3, $row['department_name']);
        $rep->TextCol(3, 4, $row['position_name']);
        $rep->TextCol(4, 5, $row['grade_name']);
        $rep->TextCol(5, 6, $row['inactive'] ? _('Inactive') : _('Active'));
        $rep->NewLine();
    }

    $rep->End();
}

print_employee_list_report();
