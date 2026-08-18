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
$page_security = 'SA_EMPTRANSFER';
$path_to_root = "../..";
include($path_to_root . "/includes/session.inc");
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/hrm/includes/hrm_constants.inc');
include_once($path_to_root . '/hrm/includes/hrm_ui.inc');
include_once($path_to_root . '/hrm/includes/db/employee_db.inc');
include_once($path_to_root . '/hrm/includes/db/employee_history_db.inc');
include_once($path_to_root . '/hrm/includes/hrm_security.inc');
include_once($path_to_root . '/hrm/includes/db/employee_person_worker_db.inc');

/**
 * Resolve one Employee Transfer selector label at the page-level instant.
 *
 * The shared employees_list() query, submitted employee_id and assignment
 * mutation/history path remain authoritative. SA_EMPTRANSFER is deliberately
 * not a Person/Worker identity-read capability, so this route-local formatter
 * adopts canonical identity only for principals that independently hold an
 * approved read area and otherwise returns the exact shared legacy label.
 *
 * @param array $row
 * @return string
 */
function employee_transfer_authoritative_employee_list($row) {
    global $employee_transfer_selector_as_of;

    $legacy_label = _format_employee_list($row);
    if (!is_array($row) || !isset($row[0]) || trim((string)$row[0]) === ''
        || $employee_transfer_selector_as_of === false)
        return $legacy_label;

    $identity = get_hrm_person_worker_report_name_as_of(
        $row[0], $employee_transfer_selector_as_of
    );
    if (!is_array($identity) || empty($identity['canonical_linked']))
        return $legacy_label;

    $first_name = isset($identity['first_name']) ? trim((string)$identity['first_name']) : '';
    $middle_name = isset($identity['middle_name']) ? trim((string)$identity['middle_name']) : '';
    $last_name = isset($identity['last_name']) ? trim((string)$identity['last_name']) : '';
    $canonical_name = trim($first_name.' '.($middle_name !== '' ? $middle_name.' ' : '').$last_name);
    if ($canonical_name === '')
        return $legacy_label;

    return (user_show_codes() ? ((string)$row[0].' - ') : '').$canonical_name;
}

$js = '';

if (user_use_date_picker())
	$js .= get_js_date_picker();

page(_($help_context = "Employee Transfer"), false, false, '', $js);

if (!isset($_POST['employee_id']))
    $_POST['employee_id'] = '';
if (!isset($_POST['new_department_id']))
    $_POST['new_department_id'] = 0;
if (!isset($_POST['new_position_id']))
    $_POST['new_position_id'] = 0;
if (!isset($_POST['new_grade_id']))
    $_POST['new_grade_id'] = 0;
if (!isset($_POST['effective_date']))
    $_POST['effective_date'] = Today();
if (!isset($_POST['reason']))
    $_POST['reason'] = '';

if (isset($_POST['Process'])) {
    if ($_POST['employee_id'] == '' || $_POST['employee_id'] == ALL_TEXT) {
        display_error(_('Employee is required.'));
        set_focus('employee_id');
    } elseif (!is_date($_POST['effective_date'])) {
        display_error(_('Effective date is invalid.'));
        set_focus('effective_date');
    } elseif ((int)$_POST['new_department_id'] == 0) {
        display_error(_('New Department is required and must be selected.'));
        set_focus('new_department_id');
    } else {
        $employee = get_employee_assignment_context_projection($_POST['employee_id']);
        if (!$employee) {
            display_error(_('Selected employee was not found.'));
        } else {
            hrm_log_restricted_employee_projection('employee_transfer_context');
            $old_department = (int)$employee['department_id'];
            $old_position = (int)$employee['position_id'];
            $old_grade = (int)$employee['grade_id'];

            $new_department = (int)$_POST['new_department_id'];
            $new_position = (int)$_POST['new_position_id'];
            $new_grade = (int)$_POST['new_grade_id'];

            $employee_updated = update_employee($_POST['employee_id'], array(
                'department_id' => $new_department,
                'position_id' => $new_position,
                'grade_id' => $new_grade
            ));

            if (!$employee_updated) {
                display_error(_('Could not update the employee or append required audit evidence.'));
            } else {
                add_employee_history(
                    $_POST['employee_id'],
                    HRM_HIST_TRANSFER,
                    $_POST['effective_date'],
                    $old_department,
                    $new_department,
                    $old_position,
                    $new_position,
                    $old_grade,
                    $new_grade,
                    null,
                    null,
                    $_POST['reason'],
                    isset($_SESSION['wa_current_user']->loginname) ? $_SESSION['wa_current_user']->loginname : ''
                );

                display_notification(_('Employee transfer has been processed.'));
            }
        }
    }
}

$employee_transfer_selector_as_of = hrm_person_worker_utc_now();
hrm_log_restricted_employee_projection('employee_transfer_selector');

start_form();
start_table(TABLESTYLE2);
employees_list_row(_('Employee:'), 'employee_id', null, false, false, false, false,
    array('format' => 'employee_transfer_authoritative_employee_list'));
departments_list_row(_('New Department:'), 'new_department_id');
positions_list_row(_('New Position:'), 'new_position_id');
grades_list_row(_('New Grade:'), 'new_grade_id');
date_row(_('Effective Date:'), 'effective_date');
textarea_row(_('Reason:'), 'reason', null, 50, 3);
end_table(1);
submit_center('Process', _('Process Transfer'));
end_form();

end_page();
