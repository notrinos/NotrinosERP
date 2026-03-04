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
***********************************************************************/
$page_security = 'SA_EMPSEPARATION';
$path_to_root = "../..";
include($path_to_root . "/includes/session.inc");
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/hrm/includes/hrm_constants.inc');
include_once($path_to_root . '/hrm/includes/hrm_ui.inc');
include_once($path_to_root . '/hrm/includes/db/employee_db.inc');
include_once($path_to_root . '/hrm/includes/db/employee_salary_db.inc');
include_once($path_to_root . '/hrm/includes/db/employee_history_db.inc');
include_once($path_to_root . '/hrm/includes/db/eos_db.inc');

/**
 * Calculate years of service from two dates.
 *
 * @param string $from_date
 * @param string $to_date
 * @return float
 */
function employee_service_years($from_date, $to_date) {
    $from_sql = date2sql($from_date);
    $to_sql = date2sql($to_date);
    $days = (strtotime($to_sql) - strtotime($from_sql)) / 86400;
    if ($days < 0)
        $days = 0;

    return round2($days / 365, 4);
}

page(_("Employee Separation / EOS"));

if (!isset($_POST['employee_id']))
    $_POST['employee_id'] = '';
if (!isset($_POST['separation_date']))
    $_POST['separation_date'] = Today();
if (!isset($_POST['is_resignation']))
    $_POST['is_resignation'] = 0;
if (!isset($_POST['reason']))
    $_POST['reason'] = '';

$eos_amount = 0;

if (isset($_POST['Calculate']) || isset($_POST['Process'])) {
    if ($_POST['employee_id'] != '' && $_POST['employee_id'] != ALL_TEXT && is_date($_POST['separation_date'])) {
        $employee = get_employee_by_code($_POST['employee_id']);
        if ($employee && !empty($employee['hire_date'])) {
            $monthly_salary = get_employee_total_salary($_POST['employee_id'], $_POST['separation_date']);
            $years = employee_service_years(sql2date($employee['hire_date']), $_POST['separation_date']);
            $eos_amount = calculate_eos_amount($monthly_salary, $years, check_value('is_resignation') ? 1 : 0);
        }
    }
}

if (isset($_POST['Process'])) {
    if ($_POST['employee_id'] == '' || $_POST['employee_id'] == ALL_TEXT) {
        display_error(_('Employee is required.'));
        set_focus('employee_id');
    } elseif (!is_date($_POST['separation_date'])) {
        display_error(_('Separation date is invalid.'));
        set_focus('separation_date');
    } else {
        $employee = get_employee_by_code($_POST['employee_id']);
        if (!$employee) {
            display_error(_('Selected employee was not found.'));
        } else {
            update_employee($_POST['employee_id'], array(
                'inactive' => 1,
                'released_date' => $_POST['separation_date']
            ));

            add_employee_history(
                $_POST['employee_id'],
                HRM_HIST_SEPARATION,
                $_POST['separation_date'],
                (int)$employee['department_id'],
                null,
                (int)$employee['position_id'],
                null,
                (int)$employee['grade_id'],
                null,
                get_employee_total_salary($_POST['employee_id'], $_POST['separation_date']),
                null,
                $_POST['reason'].'; EOS='.(string)$eos_amount,
                isset($_SESSION['wa_current_user']->loginname) ? $_SESSION['wa_current_user']->loginname : ''
            );

            display_notification(_('Employee separation has been processed. Calculated EOS: ').price_format($eos_amount));
        }
    }
}

start_form();
start_table(TABLESTYLE2);
employees_list_row(_('Employee:'), 'employee_id', null, false, false, false);
date_row(_('Separation Date:'), 'separation_date');
check_row(_('Is Resignation:'), 'is_resignation');
textarea_row(_('Reason:'), 'reason', null, 50, 3);
label_row(_('Calculated EOS Amount:'), price_format($eos_amount));
end_table(1);
submit_center('Calculate', _('Calculate EOS'));
submit_center('Process', _('Process Separation'));
end_form();

end_page();

