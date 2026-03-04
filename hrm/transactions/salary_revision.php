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
$page_security = 'SA_SALARYREVISION';
$path_to_root = "../..";
include($path_to_root . "/includes/session.inc");
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/hrm/includes/hrm_constants.inc');
include_once($path_to_root . '/hrm/includes/hrm_ui.inc');
include_once($path_to_root . '/hrm/includes/db/employee_db.inc');
include_once($path_to_root . '/hrm/includes/db/employee_salary_db.inc');
include_once($path_to_root . '/hrm/includes/db/employee_history_db.inc');

/**
 * Get basic pay element id for salary revisions.
 *
 * @return int
 */
function get_basic_pay_element_id() {
    $sql = "SELECT element_id FROM ".TB_PREF."pay_elements
        WHERE element_category = ".db_escape(HRM_ELEM_BASIC)."
        AND inactive = 0
        ORDER BY display_order, element_id
        LIMIT 1";
    $result = db_query($sql, 'could not get basic pay element');
    $row = db_fetch_assoc($result);

    return $row ? (int)$row['element_id'] : 0;
}

page(_("Salary Revision"));

if (!isset($_POST['employee_id']))
    $_POST['employee_id'] = '';
if (!isset($_POST['new_salary']))
    $_POST['new_salary'] = 0;
if (!isset($_POST['effective_date']))
    $_POST['effective_date'] = Today();
if (!isset($_POST['reason']))
    $_POST['reason'] = '';

if (isset($_POST['Process'])) {
    if ($_POST['employee_id'] == '' || $_POST['employee_id'] == ALL_TEXT) {
        display_error(_('Employee is required.'));
        set_focus('employee_id');
    } elseif (input_num('new_salary') <= 0) {
        display_error(_('New salary must be greater than zero.'));
        set_focus('new_salary');
    } elseif (!is_date($_POST['effective_date'])) {
        display_error(_('Effective date is invalid.'));
        set_focus('effective_date');
    } else {
        $employee = get_employee_by_code($_POST['employee_id']);
        if (!$employee) {
            display_error(_('Selected employee was not found.'));
        } else {
            $old_salary = get_employee_total_salary($_POST['employee_id'], $_POST['effective_date']);
            $basic_element_id = get_basic_pay_element_id();

            if ($basic_element_id <= 0) {
                display_error(_('No active Basic pay element found. Please configure pay elements first.'));
            } else {
                update_employee($_POST['employee_id'], array('personal_salary' => 1));
                add_employee_salary(
                    $_POST['employee_id'],
                    $basic_element_id,
                    input_num('new_salary'),
                    $_POST['effective_date'],
                    '',
                    '',
                    _('Salary revision')
                );

                add_employee_history(
                    $_POST['employee_id'],
                    HRM_HIST_SALARY_CHANGE,
                    $_POST['effective_date'],
                    (int)$employee['department_id'],
                    (int)$employee['department_id'],
                    (int)$employee['position_id'],
                    (int)$employee['position_id'],
                    (int)$employee['grade_id'],
                    (int)$employee['grade_id'],
                    $old_salary,
                    input_num('new_salary'),
                    $_POST['reason'],
                    isset($_SESSION['wa_current_user']->loginname) ? $_SESSION['wa_current_user']->loginname : ''
                );

                display_notification(_('Salary revision has been processed.'));
            }
        }
    }
}

start_form();
start_table(TABLESTYLE2);
employees_list_row(_('Employee:'), 'employee_id', null, false, false, false);
amount_row(_('New Salary:'), 'new_salary');
date_row(_('Effective Date:'), 'effective_date');
textarea_row(_('Reason:'), 'reason', null, 50, 3);
end_table(1);
submit_center('Process', _('Process Revision'));
end_form();

end_page();

