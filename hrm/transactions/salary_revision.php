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
    $has_error = false;
    // Employee validation
    if ($_POST['employee_id'] == '' || $_POST['employee_id'] == ALL_TEXT || !preg_match('/^[A-Za-z0-9_-]+$/', $_POST['employee_id'])) {
        display_error(_('Employee is required.'));
        set_focus('employee_id');
        $has_error = true;
    }
    // Salary validation: must be numeric and > 0
    $salary = input_num('new_salary');
    if (!is_numeric($_POST['new_salary']) || $salary <= 0) {
        display_error(_('New salary must be a positive number.'));
        set_focus('new_salary');
        $has_error = true;
    }
    // Date validation
    if (!is_date($_POST['effective_date'])) {
        display_error(_('Effective date is invalid.'));
        set_focus('effective_date');
        $has_error = true;
    }
    // Reason validation: require non-empty, sanitize, prevent XSS
    $reason = trim($_POST['reason']);
    if ($reason === '' || strlen($reason) < 3) {
        display_error(_('Reason is required and must be at least 3 characters.'));
        set_focus('reason');
        $has_error = true;
    }
    // Sanitize reason (strip tags, encode special chars)
    $reason = htmlspecialchars(strip_tags($reason), ENT_QUOTES, 'UTF-8');
    $_POST['reason'] = $reason;

    if (!$has_error) {
        $employee = get_employee_by_code($_POST['employee_id']);
        if (!$employee) {
            display_error(_('Selected employee was not found.'));
            $has_error = true;
        } else {
            $old_salary = get_employee_total_salary($_POST['employee_id'], $_POST['effective_date']);
            $basic_element_id = get_basic_pay_element_id();

            if ($basic_element_id <= 0) {
                display_error(_('No active Basic pay element found. Please configure pay elements first.'));
                $has_error = true;
            } else {
                update_employee($_POST['employee_id'], array('personal_salary' => 1));
                $existing_salary = get_employee_salary_by_key(
                    $_POST['employee_id'],
                    $basic_element_id,
                    $_POST['effective_date']
                );

                if ($existing_salary) {
                    $salary_saved = update_employee_salary(
                        (int)$existing_salary['salary_id'],
                        $salary,
                        $_POST['effective_date'],
                        empty($existing_salary['effective_to']) ? '' : sql2date($existing_salary['effective_to']),
                        empty($existing_salary['formula']) ? '' : $existing_salary['formula'],
                        _('Salary revision')
                    );
                } else {
                    $salary_saved = (bool)add_employee_salary(
                        $_POST['employee_id'],
                        $basic_element_id,
                        $salary,
                        $_POST['effective_date'],
                        '',
                        '',
                        _('Salary revision')
                    );
                }

                if (!$salary_saved) {
                    display_error(_('Could not save salary revision.'));
                    $has_error = true;
                } else {
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
                        $salary,
                        $reason,
                        isset($_SESSION['wa_current_user']->loginname) ? $_SESSION['wa_current_user']->loginname : ''
                    );
                    display_notification(_('Salary revision has been processed.'));
                }
            }
        }
    }
    // Always trigger AJAX reload for regression/consistency
    if (isset($Ajax))
        $Ajax->activate('_page_body');
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

// Regression/lifecycle shortcut: open employee history using POST filter values.
echo '<form method="post" action="../inquiry/employee_history.php" target="_blank" style="margin-top:8px;">';
echo '<input type="hidden" name="_token" value="'.htmlspecialchars(ensure_csrf_token(), ENT_QUOTES, 'UTF-8').'">';
echo '<input type="hidden" name="employee_id" value="'.htmlspecialchars($_POST['employee_id'], ENT_QUOTES, 'UTF-8').'">';
echo '<input type="hidden" name="change_type" value="'.htmlspecialchars(HRM_HIST_SALARY_CHANGE, ENT_QUOTES, 'UTF-8').'">';
echo '<button type="submit" class="button">'._('View Salary Change History').'</button>';
echo '</form>';

end_page();

