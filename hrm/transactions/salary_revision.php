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
include_once($path_to_root . '/hrm/includes/hrm_security.inc');
include_once($path_to_root . '/hrm/includes/db/employee_person_worker_db.inc');
include_once($path_to_root . '/hrm/includes/db/lifecycle_salary_revision_command_db.inc');
include_once($path_to_root . '/hrm/includes/db/lifecycle_salary_revision_browser_db.inc');

/**
 * Resolve one Salary Revision selector label at the page-level instant.
 *
 * The shared employees_list() query, submitted employee_id and compensation
 * mutation/history path remain authoritative. SA_SALARYREVISION is deliberately
 * not a Person/Worker identity-read capability, so this route-local formatter
 * adopts canonical identity only for principals that independently hold an
 * approved read area and otherwise returns the exact shared legacy label.
 *
 * @param array $row
 * @return string
 */
function salary_revision_authoritative_employee_list($row) {
    global $salary_revision_selector_as_of;

    $legacy_label = _format_employee_list($row);
    if (!is_array($row) || !isset($row[0]) || trim((string)$row[0]) === ''
        || $salary_revision_selector_as_of === false)
        return $legacy_label;

    $identity = get_hrm_person_worker_report_name_as_of(
        $row[0], $salary_revision_selector_as_of
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

page(_($help_context = "Salary Revision"), false, false, '', $js);

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
    $employee_id = trim((string)$_POST['employee_id']);
    if ($employee_id === '' || $employee_id == ALL_TEXT || preg_match('/^[A-Za-z0-9_-]+$/D', $employee_id) !== 1) {
        display_error(_('Employee is required.'));
        set_focus('employee_id');
        $has_error = true;
    }

    $salary = hrm_lifecycle_salary_revision_amount($_POST['new_salary']);
    if ($salary === false) {
        display_error(_('New salary must be a positive number with at most 6 decimal places.'));
        set_focus('new_salary');
        $has_error = true;
    }

    $effective_date_sql = false;
    if (!is_date($_POST['effective_date'])) {
        display_error(_('Effective date is invalid.'));
        set_focus('effective_date');
        $has_error = true;
    } else {
        $effective_date_sql = date2sql($_POST['effective_date']);
        if (hrm_lifecycle_salary_revision_date($effective_date_sql) === false) {
            display_error(_('Effective date is invalid.'));
            set_focus('effective_date');
            $has_error = true;
        }
    }

    // The command owns canonical sanitization and durable reason custody.
    $reason = trim(strip_tags((string)$_POST['reason']));
    if (strlen($reason) < 3 || strlen($reason) > 240) {
        display_error(_('Reason is required and must be between 3 and 240 characters.'));
        set_focus('reason');
        $has_error = true;
    }
    $_POST['reason'] = $reason;

    if (!$has_error) {
        $status = get_hrm_lifecycle_employee_salary_revision_browser_status($employee_id);
        if (!is_array($status) || !isset($status['status']) || $status['status'] === 'inconsistent') {
            display_error(_('Salary revision approval custody is inconsistent. No change was made.'));
            $has_error = true;
        } elseif ($status['status'] === 'blocked') {
            display_error(_('Another employee lifecycle request is pending for this employee.'));
            $has_error = true;
        } else {
            if (in_array($status['status'], array('completed','rejected','cancelled'), true))
                hrm_lifecycle_salary_revision_browser_forget_idempotency_key($employee_id);
            $idempotency_key = hrm_lifecycle_salary_revision_browser_idempotency_key($employee_id);
            if ($idempotency_key === false) {
                display_error(_('Could not create secure retry custody for the salary revision request.'));
                $has_error = true;
            } else {
                $result = submit_hrm_lifecycle_employee_salary_revision(
                    $employee_id, $salary, $effective_date_sql, $reason, $idempotency_key
                );
                if (!is_array($result) || !isset($result['status']) || $result['status'] !== 'pending') {
                    display_error(_('Salary revision could not be submitted for approval. No direct salary change was made.'));
                    $has_error = true;
                } else {
                    display_notification(!empty($result['exact_retry'])
                        ? _('Salary revision approval request is already pending.')
                        : _('Salary revision has been submitted for approval.'));
                }
            }
        }
    }
    // No direct salary mutation fallback exists; final mutation is checker-owned.
    if (isset($Ajax))
        $Ajax->activate('_page_body');
}

$salary_revision_selector_as_of = hrm_person_worker_utc_now();
hrm_log_restricted_employee_projection('salary_revision_selector');

$salary_revision_browser_status = false;
if (isset($_POST['employee_id']) && trim((string)$_POST['employee_id']) !== ''
    && $_POST['employee_id'] != ALL_TEXT && preg_match('/^[A-Za-z0-9_-]+$/D', (string)$_POST['employee_id']) === 1) {
    $salary_revision_browser_status = get_hrm_lifecycle_employee_salary_revision_browser_status($_POST['employee_id']);
    if (is_array($salary_revision_browser_status) && isset($salary_revision_browser_status['status'])) {
        $coarse = (string)$salary_revision_browser_status['status'];
        if ($coarse === 'pending')
            display_notification(_('A salary revision approval request is pending.'));
        elseif ($coarse === 'blocked')
            display_warning(_('Another employee lifecycle request is pending for this employee.'));
        elseif ($coarse === 'completed') {
            display_notification(_('The latest salary revision approval request is completed.'));
            hrm_lifecycle_salary_revision_browser_forget_idempotency_key($_POST['employee_id']);
        } elseif ($coarse === 'rejected') {
            display_warning(_('The latest salary revision approval request was rejected.'));
            hrm_lifecycle_salary_revision_browser_forget_idempotency_key($_POST['employee_id']);
        } elseif ($coarse === 'cancelled') {
            display_warning(_('The latest salary revision approval request was cancelled.'));
            hrm_lifecycle_salary_revision_browser_forget_idempotency_key($_POST['employee_id']);
        } elseif ($coarse === 'inconsistent')
            display_error(_('Salary revision approval custody is inconsistent.'));
    }
}

start_form();
start_table(TABLESTYLE2);
employees_list_row(_('Employee:'), 'employee_id', null, false, false, false, false,
    array('format' => 'salary_revision_authoritative_employee_list'));
amount_row(_('New Salary:'), 'new_salary');
date_row(_('Effective Date:'), 'effective_date');
textarea_row(_('Reason:'), 'reason', null, 50, 3);
end_table(1);
submit_center('Process', _('Submit Revision for Approval'));

end_form();

// Regression/lifecycle shortcut: open employee history using POST filter values.
echo '<form method="post" action="../inquiry/employee_history.php" target="_blank" style="margin-top:8px;">';
echo '<input type="hidden" name="_token" value="'.htmlspecialchars(ensure_csrf_token(), ENT_QUOTES, 'UTF-8').'">';
echo '<input type="hidden" name="employee_id" value="'.htmlspecialchars($_POST['employee_id'], ENT_QUOTES, 'UTF-8').'">';
echo '<input type="hidden" name="change_type" value="'.htmlspecialchars(HRM_HIST_SALARY_CHANGE, ENT_QUOTES, 'UTF-8').'">';
echo '<button type="submit" class="button">'._('View Salary Change History').'</button>';
echo '</form>';

end_page();
