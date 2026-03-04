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
$page_security = 'SA_PAYROLL';
$path_to_root = "../..";
include($path_to_root . "/includes/session.inc");

include_once($path_to_root.'/includes/ui.inc');
include_once($path_to_root.'/hrm/includes/hrm_constants.inc');
include_once($path_to_root.'/hrm/includes/db/employee_db.inc');
include_once($path_to_root.'/hrm/includes/db/payroll_db.inc');
include_once($path_to_root.'/hrm/includes/payroll_engine.inc');

page(_($help_context = 'Payroll Processing'));

/**
 * Validate payroll process request.
 *
 * @return bool
 */
function validate_payroll_request() {
    if (!is_date(get_post('from_date'))) {
        display_error(_('From Date is invalid.'));
        set_focus('from_date');
        return false;
    }
    if (!is_date(get_post('to_date'))) {
        display_error(_('To Date is invalid.'));
        set_focus('to_date');
        return false;
    }
    if (date_comp(get_post('from_date'), get_post('to_date')) > 0) {
        display_error(_('From Date cannot be later than To Date.'));
        set_focus('from_date');
        return false;
    }
    if (empty(trim(get_post('period_name')))) {
        display_error(_('Payroll Period Name is required.'));
        set_focus('period_name');
        return false;
    }
    return true;
}

if (isset($_POST['process_payroll']) && validate_payroll_request()) {
    $period_name = get_post('period_name');
    $from_date = get_post('from_date');
    $to_date = get_post('to_date');
    $department_id = (int)get_post('department_id', 0);
    $employee_id = trim((string)get_post('employee_id', ''));

    $period_id = add_payroll_period($period_name, $from_date, $to_date, $department_id ? $department_id : null);
    if (!$period_id) {
        display_error(_('Could not create payroll period.'));
    } else {
        $success_count = 0;
        $failed_count = 0;

        $employees = array();
        if ($employee_id !== '') {
            $employee = get_employee_by_code($employee_id);
            if ($employee)
                $employees[] = $employee;
        } else {
            $result = get_active_employees($department_id);
            while ($row = db_fetch_assoc($result))
                $employees[] = $row;
        }

        if (empty($employees)) {
            display_warning(_('No active employees found for selected filters.'));
            $failed_count++;
        }

        foreach ($employees as $employee) {
            $payslip_doc = calculate_employee_payslip($employee, $from_date, $to_date, $period_id);
            if (!$payslip_doc) {
                $failed_count++;
                continue;
            }

            $trans_no = post_payslip_to_gl($payslip_doc);
            if (!$trans_no)
                $failed_count++;
            else
                $success_count++;
        }

        update_payroll_period_totals($period_id);
        if ($success_count > 0)
            update_payroll_period_status($period_id, defined('PP_POSTED') ? PP_POSTED : 3);

        display_notification(_('Payroll processing completed.'));
        display_note(sprintf(_('Success: %d employee(s), Failed: %d employee(s), Period ID: %d'), $success_count, $failed_count, $period_id));
    }
}

start_form();

start_table(TABLESTYLE2);
text_row(_('Payroll Period Name:'), 'period_name', get_post('period_name', _('Payroll ').Today()), 42, 80);
date_row(_('From Date:'), 'from_date', get_post('from_date', begin_month(Today())));
date_row(_('To Date:'), 'to_date', get_post('to_date', end_month(Today())));
departments_list_row(_('Department (Optional):'), 'department_id', null, true, _('All departments'));

$employee_sql = "SELECT employee_id, CONCAT(employee_id, ' - ', first_name, ' ', last_name) as name
    FROM ".TB_PREF."employees WHERE !inactive ORDER BY first_name, last_name";
label_row(_('Employee (Optional):'), combo_input('employee_id', get_post('employee_id', ''), $employee_sql, 'employee_id', 'name', array(
    'spec_option' => _('-- All Employees --'),
    'spec_id' => ''
)));
end_table(1);

submit_center('process_payroll', _('Process Payroll'), true, '', 'default');

end_form();

end_page();

