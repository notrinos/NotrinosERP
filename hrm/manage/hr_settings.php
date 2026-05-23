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
include_once($path_to_root.'/gl/includes/db/gl_db_accounts.inc');
include_once($path_to_root.'/hrm/includes/db/hr_settings_db.inc');
include_once($path_to_root.'/hrm/includes/db/working_days_db.inc');

page(_("HR Settings"));

$day_labels = array(
    0 => _('Sunday'),
    1 => _('Monday'),
    2 => _('Tuesday'),
    3 => _('Wednesday'),
    4 => _('Thursday'),
    5 => _('Friday'),
    6 => _('Saturday')
);

//----------------------------------------------------------------------
// Validation helpers
//----------------------------------------------------------------------

/**
 * Validate submitted HR settings form data.
 *
 * @return bool
 */
function validate_hr_settings_form() {
    $input_error = 0;

    if (!check_num('payroll_month_work_days', 1, 31)) {
        display_error(_('Payroll month work days must be between 1 and 31.'));
        set_focus('payroll_month_work_days');
        $input_error = 1;
    }

    $payable_account = trim((string)$_POST['payroll_payable_act']);
    if ($payable_account !== '' && !get_gl_account($payable_account)) {
        display_error(_('Payroll payable account is invalid.'));
        set_focus('payroll_payable_act');
        $input_error = 1;
    }

    return $input_error == 0;
}

/**
 * Validate submitted working days form data.
 *
 * @param array $day_labels
 * @return array|false
 */
function validate_working_days_form($day_labels) {
    $rules = array();

    for ($day = 0; $day <= 6; $day++) {
        if (!check_num('work_hours_' . $day, 0)) {
            display_error(sprintf(_('Work hours for %s must be a valid non-negative number.'), $day_labels[$day]));
            set_focus('work_hours_' . $day);
            return false;
        }

        $is_working = check_value('is_working_' . $day) ? 1 : 0;
        $work_hours = $is_working ? input_num('work_hours_' . $day) : 0;

        if ($is_working && $work_hours <= 0) {
            display_error(sprintf(_('Work hours for %s must be greater than zero when the day is marked as working.'), $day_labels[$day]));
            set_focus('work_hours_' . $day);
            return false;
        }

        $rules[$day] = array(
            'is_working' => $is_working,
            'work_hours' => $work_hours
        );
    }

    return $rules;
}

//----------------------------------------------------------------------
// Save handler
//----------------------------------------------------------------------

if (isset($_POST['update']) && validate_hr_settings_form()) {
    $settings = array(
        'attendance_deduction_type' => (int)$_POST['attendance_deduction_type'],
        'hrm_absence_deduct_from' => (int)$_POST['hrm_absence_deduct_from'],
        'calculate_extra_absent_days' => check_value('calculate_extra_absent_days') ? 1 : 0,
        'payroll_attendance_calculate' => check_value('payroll_attendance_calculate') ? 1 : 0,
        'payroll_month_work_days' => (int)$_POST['payroll_month_work_days'],
        'payroll_payable_act' => trim((string)$_POST['payroll_payable_act']),
    );

    if (save_hr_settings_values($settings)) {
        display_notification_centered(_('HR settings have been updated.'));
        $Ajax->activate('_page_body');
    } else {
        display_error(_('HR settings could not be updated.'));
    }
}

if (isset($_POST['save_working_days'])) {
    $rules = validate_working_days_form($day_labels);

    if ($rules !== false) {
        save_working_days($rules);
        display_notification_centered(_('Working days configuration has been saved.'));
        $Ajax->activate('_page_body');
    }
}

//----------------------------------------------------------------------
// Load current values
//----------------------------------------------------------------------

$current = get_hr_settings_values();
$working_days = array();
$working_days_result = get_working_days();
while ($working_days_result && ($working_day = db_fetch($working_days_result))) {
    $working_days[(int)$working_day['day_of_week']] = $working_day;
}

$_POST['payroll_attendance_calculate'] = !empty($current['payroll_attendance_calculate']) ? 1 : 0;
$_POST['payroll_month_work_days'] = isset($current['payroll_month_work_days']) ? (int)$current['payroll_month_work_days'] : 30;
$_POST['attendance_deduction_type'] = isset($current['attendance_deduction_type']) ? (int)$current['attendance_deduction_type'] : 0;
$_POST['hrm_absence_deduct_from'] = isset($current['hrm_absence_deduct_from']) ? (int)$current['hrm_absence_deduct_from'] : 0;
$_POST['payroll_payable_act'] = isset($current['payroll_payable_act']) ? $current['payroll_payable_act'] : '';
$_POST['calculate_extra_absent_days'] = !empty($current['calculate_extra_absent_days']) ? 1 : 0;

for ($day = 0; $day <= 6; $day++) {
    $working_day = isset($working_days[$day]) ? $working_days[$day] : array('is_working' => 0, 'work_hours' => 0);
    $_POST['is_working_' . $day] = !empty($working_day['is_working']) ? 1 : 0;
    $_POST['work_hours_' . $day] = qty_format($working_day['work_hours']);
}

$absence_deduct_from = array(
    0 => _('Basic Salary'),
    1 => _('Gross Salary')
);

$deduction_by_options = array(
    0 => _('By Day'),
    1 => _('By Time')
);

//----------------------------------------------------------------------
// 2-column layout
//----------------------------------------------------------------------

start_form();

$tabs = array(
    'defaults' => array(_('Defaults'), 1),
    'working_days' => array(_('Working Days'), 1)
);

$requested_tab = isset($_GET['tab']) ? $_GET['tab'] : 'defaults';
$selected_tab = get_post('_tabs_sel', $requested_tab);
if (!isset($tabs[$selected_tab])) {
    $selected_tab = 'defaults';
}

tabbed_content_start('tabs', $tabs, $selected_tab);

if (tab_visible('tabs', 'defaults')) {
    start_outer_table(TABLESTYLE2);

    table_section(1);
    table_section_title(_('Attendance Defaults'));
    array_selector_row(_('Deduction By:'), 'attendance_deduction_type', null, $deduction_by_options);
    array_selector_row(_('Absence Deduct From:'), 'hrm_absence_deduct_from', null, $absence_deduct_from);
    check_row(_('Calculate Extra Absent Days:'), 'calculate_extra_absent_days', null);

    table_section(2);
    table_section_title(_('Payroll Defaults'));
    check_row(_('Use Fixed Month Working Days:'), 'payroll_attendance_calculate', null);
    text_row_ex(_('Fixed Month Working Days:'), 'payroll_month_work_days', 5, 5);
    gl_all_accounts_list_row(_('Payroll Payable Account:'), 'payroll_payable_act', null, true);

    end_outer_table(1);
    submit_center('update', _('Update'), true, '', 'default');
}

if (tab_visible('tabs', 'working_days')) {
    start_table(TABLESTYLE, "width='60%'");
    table_header(array(_('Day'), _('Is Working'), _('Work Hours')));

    $k = 0;
    for ($day = 0; $day <= 6; $day++) {
        alt_table_row_color($k);
        label_cell($day_labels[$day]);
        check_cells('', 'is_working_' . $day, get_post('is_working_' . $day));
        text_cells('', 'work_hours_' . $day, get_post('work_hours_' . $day), 8, 6);
        end_row();
    }

    end_table(1);
    submit_center('save_working_days', _('Save Working Days'), true, '', 'default');
}

tabbed_content_end();
end_form();

end_page();

