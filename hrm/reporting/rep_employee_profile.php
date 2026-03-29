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
// NOTE: This file is included by reporting/rep887.php
// $path_to_root and session are already initialized when called via report framework.
// Direct access uses the above declarations.
include_once($path_to_root.'/includes/date_functions.inc');
include_once($path_to_root.'/includes/data_checks.inc');
include_once($path_to_root.'/hrm/includes/hrm_constants.inc');
include_once($path_to_root.'/hrm/includes/hrm_db.inc');

/**
 * Print employee profile report.
 *
 * @return void
 */
function print_employee_profile_report() {
    global $path_to_root;

    $employee_id = isset($_POST['PARAM_0']) ? $_POST['PARAM_0'] : '';
    $comments = isset($_POST['PARAM_1']) ? $_POST['PARAM_1'] : '';
    $destination = isset($_POST['PARAM_6']) ? (int)$_POST['PARAM_6'] : 0;

    if ($destination)
        include_once($path_to_root.'/reporting/includes/excel_report.inc');
    else
        include_once($path_to_root.'/reporting/includes/pdf_report.inc');

    $rep = new FrontReport(_('Employee Profile'), 'EmployeeProfile', user_pagesize(), 9, 'P');
    $cols = array(0, 180, 520);
    $headers = array(_('Field'), _('Value'));
    $aligns = array('left', 'left');
    $rep->Info(array(0 => $comments), $cols, $headers, $aligns);
    $rep->NewPage();

    if ($employee_id == '' || $employee_id == ALL_TEXT) {
        $rep->TextCol(0, 2, _('Please select an employee for this report.'));
        $rep->End();
        return;
    }

    $row = get_employee_by_code($employee_id);
    if (!$row) {
        $rep->TextCol(0, 2, _('Employee not found.'));
        $rep->End();
        return;
    }

    $fields = array(
        _('Employee ID') => $row['employee_id'],
        _('Name') => trim($row['first_name'].' '.$row['last_name']),
        _('Email') => $row['email'],
        _('Mobile') => $row['mobile'],
        _('Hire Date') => empty($row['hire_date']) ? '' : sql2date($row['hire_date']),
        _('Department') => isset($row['department_name']) ? $row['department_name'] : '',
        _('Position') => isset($row['position_name']) ? $row['position_name'] : '',
        _('Grade') => isset($row['grade_name']) ? $row['grade_name'] : '',
        _('Status') => !empty($row['inactive']) ? _('Inactive') : _('Active')
    );

    foreach ($fields as $field => $value) {
        $rep->TextCol(0, 1, $field);
        $rep->TextCol(1, 2, $value);
        $rep->NewLine();
    }

    $rep->End();
}

print_employee_profile_report();
