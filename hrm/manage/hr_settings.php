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
$page_security = 'SA_HRSETTINGS';
$path_to_root = "../..";
include($path_to_root . "/includes/session.inc");
include_once($path_to_root.'/includes/ui.inc');
include_once($path_to_root.'/admin/db/company_db.inc');
include_once($path_to_root.'/gl/includes/gl_db.inc');
include_once($path_to_root.'/hrm/includes/db/hr_settings_db.inc');

page(_("HR Settings"));

/**
 * Validate submitted HR settings form data.
 *
 * @return bool
 */
function validate_hr_settings_form() {
    $input_error = 0;

    if (!check_num('default_work_hours', 0, 24)) {
        display_error(_('Default work hours must be between 0 and 24.'));
        set_focus('default_work_hours');
        $input_error = 1;
    }

    if (!check_num('payroll_month_work_days', 1, 31)) {
        display_error(_('Payroll month work days must be between 1 and 31.'));
        set_focus('payroll_month_work_days');
        $input_error = 1;
    }

    if (!check_num('weekend_day', 0, 6)) {
        display_error(_('Weekend day must be between 0 and 6.'));
        set_focus('weekend_day');
        $input_error = 1;
    }

    $payable_account = trim((string)$_POST['payroll_payable_act']);
    if ($payable_account !== '' && !is_account($payable_account)) {
        display_error(_('Payroll payable account is invalid.'));
        set_focus('payroll_payable_act');
        $input_error = 1;
    }

    return $input_error == 0;
}

if (isset($_POST['update']) && validate_hr_settings_form()) {
    $settings = array(
        'weekend_day' => (int)$_POST['weekend_day'],
        'default_work_hours' => input_num('default_work_hours'),
        'payroll_attendance_calculate' => check_value('payroll_attendance_calculate') ? 1 : 0,
        'payroll_month_work_days' => (int)$_POST['payroll_month_work_days'],
        'attendance_deduction_type' => check_value('attendance_deduction_type') ? 1 : 0,
        'hrm_absence_deduct_from' => (int)$_POST['hrm_absence_deduct_from'],
        'payroll_payable_act' => trim((string)$_POST['payroll_payable_act'])
    );

    if (save_hr_settings_values($settings)) {
        display_notification_centered(_('HR settings have been updated.'));
        $Ajax->activate('_page_body');
    } else {
        display_error(_('HR settings could not be updated.'));
    }
}

$current = get_hr_settings_values();

$_POST['weekend_day'] = isset($current['weekend_day']) ? (int)$current['weekend_day'] : 5;
$_POST['default_work_hours'] = isset($current['default_work_hours']) ? (float)$current['default_work_hours'] : 8;
$_POST['payroll_attendance_calculate'] = !empty($current['payroll_attendance_calculate']) ? 1 : 0;
$_POST['payroll_month_work_days'] = isset($current['payroll_month_work_days']) ? (int)$current['payroll_month_work_days'] : 30;
$_POST['attendance_deduction_type'] = !empty($current['attendance_deduction_type']) ? 1 : 0;
$_POST['hrm_absence_deduct_from'] = isset($current['hrm_absence_deduct_from']) ? (int)$current['hrm_absence_deduct_from'] : 0;
$_POST['payroll_payable_act'] = isset($current['payroll_payable_act']) ? $current['payroll_payable_act'] : '';

$week_days = array(
    0 => _('Sunday'),
    1 => _('Monday'),
    2 => _('Tuesday'),
    3 => _('Wednesday'),
    4 => _('Thursday'),
    5 => _('Friday'),
    6 => _('Saturday')
);

$absence_deduct_from = array(
    0 => _('Basic Salary'),
    1 => _('Gross Salary')
);

start_form();
start_outer_table(TABLESTYLE2);

table_section(1);
table_section_title(_('Attendance Defaults'));
array_selector_row(_('Weekend Day:'), 'weekend_day', null, $week_days);
amount_row(_('Default Work Hours:'), 'default_work_hours', null, 2);
check_row(_('Deduction by Time (unchecked = by Day):'), 'attendance_deduction_type', null);
array_selector_row(_('Absence Deduct From:'), 'hrm_absence_deduct_from', null, $absence_deduct_from);

table_section(2);
table_section_title(_('Payroll Defaults'));
check_row(_('Use Fixed Month Working Days:'), 'payroll_attendance_calculate', null);
text_row_ex(_('Fixed Month Working Days:'), 'payroll_month_work_days', 5, 5);
gl_all_accounts_list_row(_('Payroll Payable Account:'), 'payroll_payable_act', null, true);

end_outer_table(1);
submit_center('update', _('Update'), true, '', 'default');
end_form();

end_page();

