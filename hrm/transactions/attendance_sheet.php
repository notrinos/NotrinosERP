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
$page_security = 'SA_ATTENDANCE';
$path_to_root = "../..";
include($path_to_root . "/includes/session.inc");

$js = '';
if ($SysPrefs->use_popup_windows)
    $js .= get_js_open_window(900, 500);
if (user_use_date_picker())
    $js .= get_js_date_picker();

include_once($path_to_root.'/includes/ui.inc');
include_once($path_to_root.'/hrm/includes/hrm_db.inc');
include_once($path_to_root.'/hrm/includes/hrm_ui.inc');
include_once($path_to_root.'/hrm/includes/hrm_security.inc');
include_once($path_to_root.'/hrm/includes/db/employee_person_worker_db.inc');
include_once($path_to_root.'/hrm/includes/db/lifecycle_attendance_sheet_consumer_db.inc');
include_once($path_to_root.'/hrm/includes/ui/attendance_sheet_paging.inc');
include_once($path_to_root.'/includes/ui/db_pager_view.inc');

/**
 * Resolve one Attendance Sheet Employee selector label at the page-level instant.
 *
 * Attendance Sheet remains authorized by SA_ATTENDANCE, which does not grant
 * Person/Worker identity-read access. Canonical naming is additive only when
 * the same principal independently holds an approved identity-read capability.
 * The submitted employee_id, department filter, roster employee_name and all
 * attendance write values remain exact legacy values; this helper changes
 * selector presentation only.
 *
 * @param array $row
 * @return string
 */
function attendance_sheet_authoritative_employee_list($row) {
    global $attendance_sheet_selector_as_of;
    global $attendance_sheet_name_batch;

    $legacy_label = _format_employee_list($row);
    if (!is_array($row) || !isset($row[0]) || trim((string)$row[0]) === ''
        || $attendance_sheet_selector_as_of === false)
        return $legacy_label;

    $identity = get_hrm_person_worker_report_name_as_of(
        $row[0], $attendance_sheet_selector_as_of, $attendance_sheet_name_batch
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


/**
 * Resolve one Attendance Sheet roster Employee presentation name.
 *
 * The exact employee_name returned by get_attendance_sheet_employees() remains
 * the fail-closed fallback and the legacy roster SQL/cohort/order plus every
 * submitted employee/date attendance key remain authoritative. Canonical naming
 * is presentation-only and additive only when the SA_ATTENDANCE principal
 * independently holds approved Person/Worker read access. The page-level
 * selector as-of instant is reused so the selector, roster cell and modal name
 * cannot observe different identity times within one rendered request.
 *
 * @param array $row
 * @return string
 */
function attendance_sheet_authoritative_roster_employee_name($row) {
    global $attendance_sheet_selector_as_of;
    global $attendance_sheet_name_batch;

    $employee_id = is_array($row) && isset($row['employee_id'])
        ? trim((string)$row['employee_id']) : '';
    $legacy_name = is_array($row) && isset($row['employee_name'])
        ? (string)$row['employee_name'] : '';

    if ($employee_id === '' || $attendance_sheet_selector_as_of === false)
        return $legacy_name;

    $identity = get_hrm_person_worker_report_name_as_of(
        $employee_id, $attendance_sheet_selector_as_of, $attendance_sheet_name_batch
    );
    if (!is_array($identity) || empty($identity['canonical_linked']))
        return $legacy_name;

    $first_name = isset($identity['first_name']) ? trim((string)$identity['first_name']) : '';
    $middle_name = isset($identity['middle_name']) ? trim((string)$identity['middle_name']) : '';
    $last_name = isset($identity['last_name']) ? trim((string)$identity['last_name']) : '';
    $canonical_name = trim($first_name.' '.($middle_name !== '' ? $middle_name.' ' : '').$last_name);

    return $canonical_name !== '' ? $canonical_name : $legacy_name;
}

add_js_ufile($path_to_root.'/hrm/js/attendance_sheet.js');

page(_($help_context = 'Attendance Sheet'), false, false, '', $js);

//----------------------------------------------------------------------
// Status display map
//----------------------------------------------------------------------

$status_map = array(
    0 => array('label' => 'P',  'class' => 'att-present',  'title' => _('Present')),
    1 => array('label' => 'A',  'class' => 'att-absent',   'title' => _('Absent')),
    2 => array('label' => 'H',  'class' => 'att-halfday',  'title' => _('Half Day')),
    3 => array('label' => 'L',  'class' => 'att-leave',    'title' => _('On Leave')),
    4 => array('label' => 'HD', 'class' => 'att-holiday',  'title' => _('Holiday')),
    5 => array('label' => 'W',  'class' => 'att-weekend',  'title' => _('Weekend')),
);

$status_options = array(
    0 => _('Present'),
    1 => _('Absent'),
    2 => _('Half Day'),
    3 => _('On Leave'),
);

/**
 * Return whether the sheet modal has enough input to save.
 *
 * @param string $regular_hours
 * @param string $overtime_hours
 * @param int $leave_id
 * @param string $clock_in
 * @param string $clock_out
 * @param string $notes
 * @return bool
 */
function hrm_attendance_sheet_has_input($regular_hours, $overtime_hours, $leave_id, $clock_in, $clock_out, $notes) {
    return !($regular_hours === '' && $overtime_hours === '' && (int)$leave_id <= 0
        && trim((string)$clock_in) === '' && trim((string)$clock_out) === ''
        && trim((string)$notes) === '');
}

/**
 * Determine CSS class, label, and tooltip for a calendar cell.
 *
 * @param int $day Day of month.
 * @param array $att_data Attendance records keyed by day.
 * @param array $leave_data Leave records keyed by day.
 * @param array $holidays Holidays keyed by day.
 * @param int $dow Day of week (0=Sun..6=Sat).
 * @param array $wd_map Working day map keyed by DOW.
 * @param array $status_map Status display definitions.
 * @return array [class, label, title, has_data]
 */
function get_cell_info($day, $att_data, $leave_data, $holidays, $dow, $wd_map, $status_map) {
    if (isset($att_data[$day])) {
        $row = $att_data[$day];
        $status = (int)$row['status'];
        if (isset($status_map[$status])) {
            $sm = $status_map[$status];
            $hrs = ($row['regular_hours'] > 0) ? number_format((float)$row['regular_hours'], 1) : '';
            $title = $sm['title'];
            if ($hrs)
                $title .= ' ('.$hrs.'h)';
            if ((float)$row['overtime_hours'] > 0)
                $title .= ' +OT:'.number_format((float)$row['overtime_hours'], 1).'h';
            return array($sm['class'], $sm['label'], $title, true);
        }
    }
    if (isset($leave_data[$day])) {
        return array('att-leave', 'L', _('On Leave').': '.$leave_data[$day]['leave_name'], true);
    }
    if (isset($holidays[$day])) {
        return array('att-holiday', 'HD', _('Holiday').': '.$holidays[$day], false);
    }
    if (isset($wd_map[$dow]) && !$wd_map[$dow]['is_working']) {
        return array('att-weekend', 'W', _('Weekend'), false);
    }
    return array('att-empty', '', _('No entry'), false);
}

//----------------------------------------------------------------------
// Filter defaults
//----------------------------------------------------------------------

// Pager/editor actions use the existing whole-form Ajax replacement boundary.
if ($_SERVER['REQUEST_METHOD'] === 'POST')
    $Ajax->activate('_page_body');

if (!isset($_POST['sheet_month']))
    $_POST['sheet_month'] = (int)date('n');
if (!isset($_POST['sheet_year']))
    $_POST['sheet_year'] = (int)date('Y');
if (!isset($_POST['department_id']))
    $_POST['department_id'] = 0;
if (!isset($_POST['employee_id']))
    $_POST['employee_id'] = '';

// Normalize filters before constructing dates or SQL. Arrays are never valid input.
$_POST['sheet_month'] = is_scalar($_POST['sheet_month']) ? max(1, min(12, (int)$_POST['sheet_month'])) : (int)date('n');
$_POST['sheet_year'] = is_scalar($_POST['sheet_year']) ? max(1900, min(2100, (int)$_POST['sheet_year'])) : (int)date('Y');
$_POST['department_id'] = is_scalar($_POST['department_id']) ? max(0, (int)$_POST['department_id']) : 0;
$_POST['employee_id'] = is_string($_POST['employee_id']) ? trim(attendance_sheet_post_employee_id($_POST['employee_id'])) : '';
if (isset($_POST['cell_employee_id']))
    $_POST['cell_employee_id'] = attendance_sheet_post_employee_id($_POST['cell_employee_id']);
$sheet_filters = array('sheet_month' => $_POST['sheet_month'], 'sheet_year' => $_POST['sheet_year'],
    'department_id' => $_POST['department_id'], 'employee_id' => $_POST['employee_id']);
$sheet_context_token = get_post('attendance_context');
$sheet_commands = array_intersect(array('save_cell', 'delete_cell', 'bulk_fill', 'bulk_delete'), array_keys($_POST));
// The installed selector refreshes roles while rendering, which is after writes.
// Refresh its existing current permission/connection checks before resolving any
// command context, so cached session roles cannot authorize a revoked command.
if (!empty($sheet_commands)) {
    include_once($path_to_root.'/hrm/includes/ui/employee_selector_search.inc');
    if (!hrm_employee_selector_search_scope_is_current()
        || !hrm_employee_selector_refresh_access('SA_ATTENDANCE')) {
        display_error(_('Attendance access changed. Sign in again before entering attendance.'));
        end_page();
        exit;
    }
}
$sheet_context = attendance_sheet_context($sheet_context_token);
$sheet_modal_open = false;
$sheet_cell_saved = false;
$sheet_command_allowed = false;
$sheet_selected = array();

// Ordinary submits without JavaScript also retain an unresolved editor and its values.
if (!empty($_POST['att_editing']) && !isset($_POST['discard_cell'])
    && (empty($sheet_commands) || ($sheet_context && $sheet_context['filters'] !== $sheet_filters))) {
    if ($sheet_context) {
        $sheet_filters = $sheet_context['filters'];
        foreach ($sheet_filters as $name => $value) $_POST[$name] = $value;
    }
    unset($_POST['sheet_page_action'], $_POST['sheet_page_first'], $_POST['sheet_page_prev'],
        $_POST['sheet_page_next'], $_POST['sheet_page_last'], $_POST['_sheet_page_number_update'],
        $_POST['_sheet_page_number_button'], $_POST['sheet_bottom_page_first'], $_POST['sheet_bottom_page_prev'],
        $_POST['sheet_bottom_page_next'], $_POST['sheet_bottom_page_last'], $_POST['_sheet_bottom_page_number_update'],
        $_POST['_sheet_bottom_page_number_button'], $_POST['open_cell']);
    $sheet_modal_open = true;
    display_error(_('Save or discard the attendance edit before changing page or filters.'));
    // A changed filter must never turn an implicit submit into a cell command.
    foreach ($sheet_commands as $command) unset($_POST[$command]);
    $sheet_commands = array();
}
if (isset($_POST['discard_cell'])) {
    foreach (array_keys($_POST) as $name)
        if (strpos($name, 'cell_') === 0) unset($_POST[$name]);
    $_POST['att_editing'] = 0;
}
$sel_month = $sheet_filters['sheet_month'];
$sel_year = $sheet_filters['sheet_year'];
$dept_id = $sheet_filters['department_id'];
$emp_id = $sheet_filters['employee_id'];
$sheet_scope = array('department_id' => $dept_id, 'employee_id' => $emp_id);
$sheet_page_number = $sheet_context && $sheet_context['filters'] === $sheet_filters ? $sheet_context['page'] : 1;
$sheet_requested_action = get_post('sheet_page_action', null);
foreach (array('sheet_page', 'sheet_bottom_page') as $prefix) {
    if (list_updated($prefix.'_number'))
        $sheet_requested_action = get_post($prefix.'_number', null);
    foreach (array('first', 'prev', 'next', 'last') as $action)
        if (isset($_POST[$prefix.'_'.$action])) $sheet_requested_action = $action;
}
$sheet_navigation_requested = $sheet_requested_action !== null;
if (empty($sheet_commands) && !$sheet_modal_open && $sheet_navigation_requested && $sheet_context
    && $sheet_context['filters'] === $sheet_filters) {
    $action = $sheet_requested_action;
    if ($action === 'next') $sheet_page_number++;
    elseif ($action === 'prev') $sheet_page_number--;
    elseif ($action === 'first') $sheet_page_number = 1;
    elseif ($action === 'last') $sheet_page_number = PHP_INT_MAX;
    else $sheet_page_number = $action;
}

// Commands verify full readable custody before mutation. The shared selector
// retains every existing fresh guard before rendered identity/roster output.
if (!empty($sheet_commands))
    hrm_employee_projection_require_readable();
$sheet_page = attendance_sheet_employee_page($dept_id, $emp_id, $sheet_page_number);
if (!empty($sheet_commands)) {
    $selected = isset($_POST['bulk_fill']) || isset($_POST['bulk_delete'])
        ? get_post('emp_check', array()) : array(get_post('cell_employee_id'));
    if (is_array($selected) && (isset($_POST['bulk_fill']) || isset($_POST['bulk_delete'])))
        $selected = array_map('attendance_sheet_post_employee_id', $selected);
    $sheet_selected = attendance_sheet_selected_ids($sheet_context, $sheet_filters, $sheet_page, $selected);
    $cell_command = isset($_POST['save_cell']) || isset($_POST['delete_cell']);
    $valid_cell_values = true;
    if ($cell_command)
        foreach ($_POST as $name => $value)
            if (strpos($name, 'cell_') === 0 && !is_scalar($value)) $valid_cell_values = false;
    $cell_date = get_post('cell_date');
    $parsed_cell_date = is_string($cell_date) ? DateTime::createFromFormat('!Y-m-d', $cell_date) : false;
    $valid_cell_month = !$cell_command || ($parsed_cell_date && $parsed_cell_date->format('Y-m-d') === $cell_date
        && substr($cell_date, 0, 7) === sprintf('%04d-%02d', $sel_year, $sel_month));
    $sheet_command_allowed = count($sheet_commands) === 1 && $sheet_selected !== false && $valid_cell_month && $valid_cell_values
        && !$sheet_navigation_requested && !isset($_POST['open_cell']) && !isset($_POST['discard_cell']);
    // Consume before execution, including denial, so replay cannot repeat a bulk command.
    if (is_string($sheet_context_token)) unset($_SESSION['attendance_sheet_pages'][$sheet_context_token]);
    if (!$sheet_command_allowed) {
        display_error(_('Selection expired or changed. Off-page, invalid or stale employees were rejected; review this page and select again.'));
        foreach ($sheet_commands as $command) unset($_POST[$command]);
        $sheet_modal_open = $cell_command;
    }
}
$calendar = attendance_sheet_calendar($sel_year, $sel_month);
$days_in_month = count($calendar);
$wd_map = get_working_day_map();
$holidays = get_month_holidays($sel_year, $sel_month);

//----------------------------------------------------------------------
// Modal save handler
//----------------------------------------------------------------------

if (isset($_POST['save_cell'])) {
    $cell_emp      = $_POST['cell_employee_id'];
    $cell_date_sql = $_POST['cell_date'];
    $cell_date     = sql2date($cell_date_sql);
    $cell_status   = (int)$_POST['cell_status'];
    $cell_regular  = trim((string)get_post('cell_regular'));
    $cell_ot_hours = trim((string)get_post('cell_ot_hours'));
    $cell_ot_type  = (int)get_post('cell_ot_type');
    $cell_shift    = (int)get_post('cell_shift');
    $cell_clock_in = trim((string)get_post('cell_clock_in'));
    $cell_clock_out = trim((string)get_post('cell_clock_out'));
    $cell_leave    = (int)get_post('cell_leave');
    $cell_notes    = trim((string)get_post('cell_notes'));
    $sheet_modal_open = true;
    $input_error   = false;
    $leave_flags   = hrm_get_leave_coverage_flags($cell_emp, $cell_date_sql);

    if (!preg_match('/^\d{4}-\d{2}-\d{2}$/', $cell_date_sql) || !is_date($cell_date)) {
        display_error(_('Attendance date is invalid.'));
        $input_error = true;
    }
    if (!$input_error && date_comp($cell_date, Today()) > 0) {
        display_error(_('Attendance cannot be entered for future dates.'));
        $input_error = true;
    }
    if (!$input_error && check_date_paid($cell_emp, $cell_date)) {
        display_error(_('Cannot modify attendance for a payroll-locked date.'));
        $input_error = true;
    }
    if (!$input_error && !hrm_validate_hours_value($cell_regular)) {
        display_error(_('Regular hours must be numeric (0..24) or HH:MM format.'));
        $input_error = true;
    }
    if (!$input_error && !hrm_validate_hours_value($cell_ot_hours)) {
        display_error(_('Overtime hours must be numeric (0..24) or HH:MM format.'));
        $input_error = true;
    }
    if (!$input_error && !hrm_validate_clock_time($cell_clock_in)) {
        display_error(_('Clock In must be in HH:MM format.'));
        $input_error = true;
    }
    if (!$input_error && !hrm_validate_clock_time($cell_clock_out)) {
        display_error(_('Clock Out must be in HH:MM format.'));
        $input_error = true;
    }
    if (!$input_error && $cell_ot_hours !== '' && $cell_ot_type <= 0) {
        display_error(_('Overtime type is required when overtime hours are entered.'));
        $input_error = true;
    }
    if (!$input_error && $cell_status === 3 && $cell_leave <= 0) {
        display_error(_('Leave type is required when status is On Leave.'));
        $input_error = true;
    }
    if (!$input_error && !hrm_attendance_sheet_has_input($cell_regular, $cell_ot_hours, $cell_leave, $cell_clock_in, $cell_clock_out, $cell_notes)) {
        display_error(_('Enter hours, leave, clock time, or notes, or use Delete to remove the entry.'));
        $input_error = true;
    }
    if (!$input_error && strlen($cell_notes) > 200) {
        display_error(_('Notes cannot exceed 200 characters.'));
        $input_error = true;
    }
    if (!$input_error && $leave_flags['has_multi_day']) {
        display_error(_('This day is covered by a multi-day leave request. Update it from Leave Request instead.'));
        $input_error = true;
    }
    if (!$input_error) {
        $result = hrm_fnd_005_execute_attendance_sheet_day($cell_emp, $cell_date_sql, 'save',
            $cell_regular, $cell_ot_hours, $cell_ot_type, $cell_leave, $cell_status,
            $cell_shift, $cell_clock_in, $cell_clock_out, $cell_notes, $sheet_scope);
        if ($result['saved']) {
            $sheet_cell_saved = true;
            display_notification(_('Attendance saved for ').$cell_emp.' — '.sql2date($cell_date_sql));
        } else
            display_error(_('Attendance could not be saved because lifecycle, leave or payroll custody denied the mutation.'));
    }
    $Ajax->activate('_page_body');
}

//----------------------------------------------------------------------
// Delete handler
//----------------------------------------------------------------------

if (isset($_POST['delete_cell'])) {
    $sheet_modal_open = true;
    $cell_emp      = $_POST['cell_employee_id'];
    $cell_date_sql = $_POST['cell_date'];
    $cell_date     = sql2date($cell_date_sql);
    $leave_flags   = hrm_get_leave_coverage_flags($cell_emp, $cell_date_sql);

    if ($leave_flags['has_multi_day']) {
        display_error(_('This day is covered by a multi-day leave request. Update it from Leave Request instead.'));
    } elseif (check_date_paid($cell_emp, $cell_date)) {
        display_error(_('Cannot delete attendance for a payroll-locked date.'));
    } else {
        $result = hrm_fnd_005_execute_attendance_sheet_day($cell_emp, $cell_date_sql, 'delete', '', '', 0, 0, 0, 0, '', '', '', $sheet_scope);
        if ($result['saved']) {
            $sheet_cell_saved = true;
            display_notification(_('Attendance deleted for ').$cell_emp.' — '.sql2date($cell_date_sql));
        } else
            display_error(_('Attendance could not be deleted because lifecycle, leave or payroll custody denied the mutation.'));
    }
    $Ajax->activate('_page_body');
}

//----------------------------------------------------------------------
// Bulk fill handler — mark all working days as Present with scheduled hrs
//----------------------------------------------------------------------

if (isset($_POST['bulk_fill'])) {
    $emp_checks = $sheet_selected;
    sort($emp_checks, SORT_STRING);
    if (empty($emp_checks)) {
        display_error(_('No employees selected. Please check the rows to fill.'));
    } else {
    $att_data = get_monthly_attendance($sel_year, $sel_month, $dept_id, $emp_id, $emp_checks);
    $leave_data = get_monthly_leaves($sel_year, $sel_month, $dept_id, $emp_id, $emp_checks);
    $filled = 0;
    $denied = 0;

    foreach ($emp_checks as $eid) {
        for ($d = 1; $d <= $days_in_month; $d++) {
            $dow = $calendar[$d]['dow'];
            $date_sql = $calendar[$d]['date'];
            $user_date = sql2date($date_sql);

            if (isset($wd_map[$dow]) && !$wd_map[$dow]['is_working']) continue;
            if (isset($holidays[$d])) continue;
            if (isset($att_data[$eid][$d])) continue;
            if (isset($leave_data[$eid][$d])) continue;
            if ($calendar[$d]['future']) continue;
            if (check_date_paid($eid, $user_date)) continue;

            $hours = isset($wd_map[$dow]) ? $wd_map[$dow]['work_hours'] : 8;
            $result = hrm_fnd_005_execute_attendance_sheet_day($eid, $date_sql, 'fill', $hours, '', 0, 0, 0, 0, '', '', '', $sheet_scope);
            if ($result['saved']) $filled++;
            elseif (!$result['skipped']) $denied++;
        }
    }

    if ($filled > 0)
        display_notification(sprintf(_('%d attendance records filled.'), $filled));
    elseif ($denied === 0)
        display_notification(_('No records to fill — all working days already have entries or are in the future.'));
    if ($denied > 0)
        display_error(sprintf(_('%d attendance days denied by lifecycle or payroll custody.'), $denied));
    } // end else emp_checks
    $Ajax->activate('_page_body');
}

//----------------------------------------------------------------------
// Bulk delete handler
//----------------------------------------------------------------------

if (isset($_POST['bulk_delete'])) {
    $emp_checks = $sheet_selected;
    sort($emp_checks, SORT_STRING);
    if (empty($emp_checks)) {
        display_error(_('No employees selected for bulk delete.'));
    } else {
        $deleted = 0;
        $denied = 0;
        $days_in_month_del = (int)date('t', mktime(0, 0, 0, $sel_month, 1, $sel_year));
        foreach ($emp_checks as $eid) {
            for ($d = 1; $d <= $days_in_month_del; $d++) {
                $ts = mktime(0, 0, 0, $sel_month, $d, $sel_year);
                $date_sql = date('Y-m-d', $ts);
                $user_date = sql2date($date_sql);
                if (check_date_paid($eid, $user_date)) continue;
                $result = hrm_fnd_005_execute_attendance_sheet_day($eid, $date_sql, 'bulk_delete', '', '', 0, 0, 0, 0, '', '', '', $sheet_scope);
                if ($result['saved']) $deleted++;
                else $denied++;
            }
        }
        if ($deleted > 0)
            display_notification(sprintf(_('Bulk delete completed for %d attendance days.'), $deleted));
        if ($denied > 0)
            display_error(sprintf(_('%d attendance days denied by lifecycle or payroll custody.'), $denied));
    }
    $Ajax->activate('_page_body');
}

//----------------------------------------------------------------------
// Capture one post-command identity instant for selector and roster presentation only.
//----------------------------------------------------------------------

$attendance_sheet_selector_as_of = hrm_person_worker_utc_now();
$attendance_sheet_name_batch = false;
hrm_log_restricted_employee_projection('attendance_sheet_selector');

//----------------------------------------------------------------------
// Load data for the grid
//----------------------------------------------------------------------

// Reload after commands at the same post-command identity boundary.
if (!empty($sheet_commands))
    $sheet_page = attendance_sheet_employee_page($dept_id, $emp_id, $sheet_page['page']);
$employees = $sheet_page['employees'];
$visible_employee_ids = array_column($employees, 'employee_id');
$att_data = get_monthly_attendance($sel_year, $sel_month, $dept_id, $emp_id, $visible_employee_ids);
$leave_data = get_monthly_leaves($sel_year, $sel_month, $dept_id, $emp_id, $visible_employee_ids);
if ($sheet_cell_saved) $sheet_modal_open = false;
// The ordinary-submit cell opener is also available with JavaScript disabled.
if (isset($_POST['open_cell']) && is_string($_POST['open_cell'])) {
    $open = json_decode(htmlspecialchars_decode($_POST['open_cell'], ENT_QUOTES), true);
    if (is_array($open) && count($open) === 2 && is_string($open[0])
        && in_array($open[0], $visible_employee_ids, true) && isset($calendar[(int)$open[1]])) {
        $day = (int)$open[1];
        $entry = isset($att_data[$open[0]][$day]) ? $att_data[$open[0]][$day] : array();
        $_POST['cell_employee_id'] = $open[0];
        $_POST['cell_date'] = $calendar[$day]['date'];
        foreach (array('status'=>'status', 'regular'=>'regular_hours', 'ot_hours'=>'overtime_hours',
            'ot_type'=>'overtime_type_id', 'shift'=>'shift_id', 'clock_in'=>'clock_in',
            'clock_out'=>'clock_out', 'notes'=>'notes') as $field => $column)
            $_POST['cell_'.$field] = isset($entry[$column]) ? htmlspecialchars((string)$entry[$column], ENT_QUOTES) : '';
        $_POST['cell_leave'] = isset($leave_data[$open[0]][$day]) ? $leave_data[$open[0]][$day]['leave_type_id'] : 0;
        if (empty($entry) && !empty($_POST['cell_leave'])) $_POST['cell_status'] = 3;
        $sheet_modal_open = true;
    }
}
if (!empty($employees))
    hrm_log_restricted_employee_projection('attendance_roster');

$overtime_result = get_all_overtime();
$overtime_options = array(0 => _('-- None --'));
while ($ot = db_fetch($overtime_result))
    $overtime_options[$ot['overtime_id']] = $ot['overtime_name'];

//----------------------------------------------------------------------
// Render filters
//----------------------------------------------------------------------

start_form();
unset($_POST['emp_check'], $_POST['check_all_emp']);
hidden('att_editing', $sheet_modal_open ? 1 : 0);
echo '<input type="hidden" name="att_filter_state" value="'.htmlspecialchars(json_encode(
    array_merge($sheet_filters, array('sheet_page_number' => $sheet_page['page'],
        'sheet_bottom_page_number' => $sheet_page['page']))), ENT_QUOTES).'">';
echo '<div id="att-edit-message" role="alert" data-block-message="'.htmlspecialchars(
    _('Save or discard the attendance edit before changing page or filters.'), ENT_QUOTES).'"></div>';

start_table(TABLESTYLE_NOBORDER);
start_row();
months_list_cells(_('Month:'), 'sheet_month', $sel_month, true);
years_list_cells(_('Fiscal Year:'), 'sheet_year', $sel_year);
submit_cells('sheet_apply', _('Apply filters'));
departments_list_cells(_('Department:'), 'department_id', get_post('department_id'), _('All departments'), true);

$emp_list_opts = array(
    'employee_name_batch' => array('target' => 'attendance_sheet_name_batch',
        'as_of' => $attendance_sheet_selector_as_of,
        'extra_refs' => array_column($employees, 'employee_id')),
    'async' => false,
    'format' => 'attendance_sheet_authoritative_employee_list'
);
if ($dept_id > 0)
    $emp_list_opts['where'] = array("e.department_id = ".db_escape($dept_id));
employees_list_cells(_('Employee:'), 'employee_id', $emp_id, _('All employees'), true, false, false, $emp_list_opts);

check_cells(_('Select visible page'), 'check_all_emp', null, false, _('Select/Deselect employees on this page only'));
submit_cells('bulk_fill', _('Fill checked rows'), '', _('Auto-fill all working days with scheduled hours for checked employees'), false);
submit_cells('bulk_delete', _('Delete checked rows'), '', _('Delete ALL attendance records for checked employees in the selected month'), false);
end_row();
end_table(1);

// The installed complete selector above refreshes the session's current roles.
// Bind this rendered page only after that refresh, including reused sessions.
$sheet_context_token = attendance_sheet_remember_page($sheet_filters, $sheet_page);
hidden('attendance_context', $sheet_context_token);

submit_js_confirm('bulk_delete', _('Delete ALL attendance records for checked employees in the selected month?'));

attendance_sheet_render_pagination($sheet_page);

//----------------------------------------------------------------------
// Legend
//----------------------------------------------------------------------

echo '<div class="att-legend">';
echo '<span class="att-legend-item"><span class="att-cell att-present">P</span> '._('Present').'</span>';
echo '<span class="att-legend-item"><span class="att-cell att-absent">A</span> '._('Absent').'</span>';
echo '<span class="att-legend-item"><span class="att-cell att-halfday">H</span> '._('Half Day').'</span>';
echo '<span class="att-legend-item"><span class="att-cell att-leave">L</span> '._('On Leave').'</span>';
echo '<span class="att-legend-item"><span class="att-cell att-holiday">HD</span> '._('Holiday').'</span>';
echo '<span class="att-legend-item"><span class="att-cell att-weekend">W</span> '._('Weekend').'</span>';
echo '<span class="att-legend-item"><span class="att-cell att-empty">&nbsp;&nbsp;</span> '._('No Entry').'</span>';
echo '</div>';

if (empty($employees)) {
    display_note(_('No employees found. Adjust the filters above.'));
    if (!$sheet_modal_open) {
        end_form();
        end_page();
        return;
    }
}

//----------------------------------------------------------------------
// Calendar grid
//----------------------------------------------------------------------

echo '<div class="att-sheet-scroll">';
echo '<table class="att-sheet-table tablestyle">';
echo '<thead>';
echo '<tr class="att-header">';
echo '<th class="att-emp-id att-sticky-col"><input type="checkbox" id="check_all_emp_header" onclick="toggleAllEmpChecks(this.checked)"> '._('ID').'</th>';
echo '<th class="att-emp-name att-sticky-col att-sticky-name">'._('Employee').'</th>';

for ($d = 1; $d <= $days_in_month; $d++) {
    $dow = $calendar[$d]['dow'];
    $day_abbr = $calendar[$d]['abbr'];
    $is_weekend = isset($wd_map[$dow]) && !$wd_map[$dow]['is_working'];
    $is_holiday = isset($holidays[$d]);
    $is_today = $calendar[$d]['today'];

    $cls = 'att-day-header';
    if ($is_weekend) $cls .= ' att-weekend-col';
    if ($is_holiday) $cls .= ' att-holiday-col';
    if ($is_today)   $cls .= ' att-today-col';

    $title = $calendar[$d]['title'];
    if ($is_holiday) $title .= ' — '.$holidays[$d];

    echo '<th class="'.$cls.'" title="'.htmlspecialchars($title, ENT_QUOTES).'">';
    echo '<div class="att-day-num">'.$d.'</div>';
    echo '<div class="att-day-name">'.$day_abbr.'</div>';
    echo '</th>';
}

echo '<th class="att-sum-col" title="'._('Working Days').'">'._('WD').'</th>';
echo '<th class="att-sum-col" title="'._('Present Days').'">'._('Pr').'</th>';
echo '<th class="att-sum-col" title="'._('Absent Days').'">'._('Ab').'</th>';
echo '<th class="att-sum-col" title="'._('Leave Days').'">'._('Lv').'</th>';
echo '<th class="att-sum-col" title="'._('Regular Hours').'">'._('Hrs').'</th>';
echo '<th class="att-sum-col" title="'._('Overtime Hours').'">'._('OT').'</th>';
echo '</tr>';
echo '</thead><tbody>';

$k = 0;
foreach ($employees as $emp) {
    $eid = $emp['employee_id'];
    $employee_display_name = attendance_sheet_authoritative_roster_employee_name($emp);
    $emp_att   = isset($att_data[$eid]) ? $att_data[$eid] : array();
    $emp_leave = isset($leave_data[$eid]) ? $leave_data[$eid] : array();

    $sum_wd = 0; $sum_present = 0; $sum_absent = 0; $sum_leave = 0;
    $sum_hours = 0; $sum_ot = 0;

    $row_class = ($k % 2 == 0) ? 'att-row-even' : 'att-row-odd';
    echo '<tr class="'.$row_class.'">';
    echo '<td class="att-emp-id att-sticky-col"><input type="checkbox" name="emp_check[]" value="'.htmlspecialchars($eid, ENT_QUOTES).'" class="emp-check"> '.htmlspecialchars($eid).'</td>';
    echo '<td class="att-emp-name att-sticky-col att-sticky-name">'.htmlspecialchars($employee_display_name).'</td>';

    for ($d = 1; $d <= $days_in_month; $d++) {
        $dow = $calendar[$d]['dow'];
        $date_sql = $calendar[$d]['date'];
        $is_future = $calendar[$d]['future'];
        $is_today = $calendar[$d]['today'];

        list($cell_class, $cell_label, $cell_title, $has_data) =
            get_cell_info($d, $emp_att, $emp_leave, $holidays, $dow, $wd_map, $status_map);

        // Summary counting
        $is_working = isset($wd_map[$dow]) ? $wd_map[$dow]['is_working'] : 1;
        if ($is_working && !isset($holidays[$d]) && !$is_future)
            $sum_wd++;

        if (isset($emp_att[$d])) {
            $st = (int)$emp_att[$d]['status'];
            if ($st === 0) { $sum_present++; $sum_hours += (float)$emp_att[$d]['regular_hours']; }
            elseif ($st === 1) $sum_absent++;
            elseif ($st === 2) { $sum_present += 0.5; $sum_absent += 0.5; $sum_hours += (float)$emp_att[$d]['regular_hours']; }
            elseif ($st === 3) $sum_leave++;
            $sum_ot += (float)$emp_att[$d]['overtime_hours'];
        } elseif (isset($emp_leave[$d])) {
            $sum_leave++;
        }

        $cell_classes = 'att-cell '.$cell_class;
        if ($is_today) $cell_classes .= ' att-today-col';

        $is_non_work = isset($holidays[$d]) || (isset($wd_map[$dow]) && !$wd_map[$dow]['is_working']);

        if ($is_future || $is_non_work) {
            echo '<td class="'.$cell_classes.'" title="'.htmlspecialchars($cell_title, ENT_QUOTES).'">';
            echo '<span>'.$cell_label.'</span></td>';
        } else {
            echo '<td class="'.$cell_classes.' att-clickable" title="'.htmlspecialchars($cell_title, ENT_QUOTES).'"';
            echo ' data-emp="'.htmlspecialchars($eid, ENT_QUOTES).'"';
            echo ' data-date="'.$date_sql.'"';
            echo ' data-name="'.htmlspecialchars($employee_display_name, ENT_QUOTES).'"';
            // Pre-load existing data for the modal
            if (isset($emp_att[$d])) {
                $r = $emp_att[$d];
                echo ' data-status="'.(int)$r['status'].'"';
                echo ' data-regular="'.htmlspecialchars((string)$r['regular_hours'], ENT_QUOTES).'"';
                echo ' data-ot-hours="'.htmlspecialchars((string)$r['overtime_hours'], ENT_QUOTES).'"';
                echo ' data-ot-type="'.(int)$r['overtime_type_id'].'"';
                echo ' data-shift="'.(int)$r['shift_id'].'"';
                echo ' data-clock-in="'.htmlspecialchars((string)$r['clock_in'], ENT_QUOTES).'"';
                echo ' data-clock-out="'.htmlspecialchars((string)$r['clock_out'], ENT_QUOTES).'"';
                echo ' data-notes="'.htmlspecialchars((string)$r['notes'], ENT_QUOTES).'"';
            }
            if (isset($emp_leave[$d])) {
                echo ' data-leave="'.(int)$emp_leave[$d]['leave_type_id'].'"';
                if (!isset($emp_att[$d]))
                    echo ' data-status="3"';
            }
            echo ' tabindex="-1" onclick="openAttModal(this); return false;">';
            echo '<button type="submit" name="open_cell" class="att-open-cell" value="'.htmlspecialchars(json_encode(array((string)$eid, $d)), ENT_QUOTES).'" aria-label="'.htmlspecialchars($eid.' '.$date_sql.' '.$cell_title, ENT_QUOTES).'">'.($cell_label === '' ? '&nbsp;' : $cell_label).'</button></td>';
        }
    }

    echo '<td class="att-sum-val">'.$sum_wd.'</td>';
    echo '<td class="att-sum-val">'.($sum_present > 0 ? $sum_present : '-').'</td>';
    echo '<td class="att-sum-val att-sum-absent">'.($sum_absent > 0 ? $sum_absent : '-').'</td>';
    echo '<td class="att-sum-val">'.($sum_leave > 0 ? $sum_leave : '-').'</td>';
    echo '<td class="att-sum-val">'.($sum_hours > 0 ? number_format($sum_hours, 1) : '-').'</td>';
    echo '<td class="att-sum-val">'.($sum_ot > 0 ? number_format($sum_ot, 1) : '-').'</td>';
    echo '</tr>';
    $k++;
}

echo '</tbody></table></div>';
attendance_sheet_render_pagination($sheet_page, 'bottom');

//----------------------------------------------------------------------
// Modal dialog
//----------------------------------------------------------------------

echo '<div id="att-modal-overlay" class="att-modal-overlay" style="display:'.($sheet_modal_open ? 'block' : 'none').'" onclick="closeAttModal()"></div>';
echo '<div id="att-modal" class="att-modal" role="dialog" aria-modal="true" aria-label="'._('Attendance Entry').'" style="display:'.($sheet_modal_open ? 'block' : 'none').'">';
echo '<div class="att-modal-header">';
echo '<span class="att-modal-title">'._('Attendance Entry').'</span>';
echo '<span class="att-modal-close" onclick="closeAttModal()">&times;</span>';
echo '</div>';
echo '<div class="att-modal-body">';

echo '<input type="hidden" name="cell_employee_id" id="cell_employee_id" value="'.htmlspecialchars((string)get_post('cell_employee_id'), ENT_QUOTES).'">';
echo '<input type="hidden" name="cell_date" id="cell_date" value="'.htmlspecialchars((string)get_post('cell_date'), ENT_QUOTES).'">';

echo '<table class="tablestyle2">';
echo '<tr><td class="label">'._('Employee:').'</td>';
echo '<td><strong><span id="modal_emp_label">'.htmlspecialchars((string)get_post('cell_employee_id'), ENT_QUOTES).'</span></strong></td></tr>';
echo '<tr><td class="label">'._('Date:').'</td>';
echo '<td><strong><span id="modal_date_label">'.htmlspecialchars((string)get_post('cell_date'), ENT_QUOTES).'</span></strong></td></tr>';
echo '<tr><td class="label">'._('Status:').'</td><td>';
echo array_selector('cell_status', get_post('cell_status', 0), $status_options);
echo '</td></tr>';
echo '<tr><td class="label">'._('Regular Hours:').'</td>';
echo '<td><input type="text" name="cell_regular" id="cell_regular" class="att-modal-input" maxlength="8" value="'.htmlspecialchars((string)get_post('cell_regular'), ENT_QUOTES, 'UTF-8', false).'"></td></tr>';
echo '<tr><td class="label">'._('OT Type:').'</td><td>';
echo array_selector('cell_ot_type', get_post('cell_ot_type', 0), $overtime_options);
echo '</td></tr>';
echo '<tr><td class="label">'._('OT Hours:').'</td>';
echo '<td><input type="text" name="cell_ot_hours" id="cell_ot_hours" class="att-modal-input" maxlength="8" value="'.htmlspecialchars((string)get_post('cell_ot_hours'), ENT_QUOTES, 'UTF-8', false).'"></td></tr>';

if (function_exists('work_shifts_list')) {
    echo '<tr><td class="label">'._('Shift:').'</td><td>';
    echo work_shifts_list('cell_shift', null, true, false);
    echo '</td></tr>';
}

echo '<tr><td class="label">'._('Clock In:').'</td>';
echo '<td><input type="text" name="cell_clock_in" id="cell_clock_in" class="att-modal-input" maxlength="5" placeholder="HH:MM" value="'.htmlspecialchars((string)get_post('cell_clock_in'), ENT_QUOTES, 'UTF-8', false).'"></td></tr>';
echo '<tr><td class="label">'._('Clock Out:').'</td>';
echo '<td><input type="text" name="cell_clock_out" id="cell_clock_out" class="att-modal-input" maxlength="5" placeholder="HH:MM" value="'.htmlspecialchars((string)get_post('cell_clock_out'), ENT_QUOTES, 'UTF-8', false).'"></td></tr>';
echo '<tr><td class="label">'._('Leave Type:').'</td><td>';
echo leave_types_list('cell_leave', null, true, false);
echo '</td></tr>';
echo '<tr><td class="label">'._('Notes:').'</td>';
echo '<td><textarea name="cell_notes" id="cell_notes" class="att-modal-input" rows="3" maxlength="200">'.htmlspecialchars((string)get_post('cell_notes'), ENT_QUOTES, 'UTF-8', false).'</textarea></td></tr>';
echo '</table>';

echo '</div>';
echo '<div class="att-modal-footer">';
echo '<button type="submit" name="save_cell" value="1" class="ajaxsubmit att-btn att-btn-save">'._('Save').'</button> ';
echo '<button type="submit" name="delete_cell" value="1" class="ajaxsubmit att-btn att-btn-delete" onclick="return confirm(\''._('Delete this attendance entry?').'\');">'._('Delete').'</button> ';
echo '<button type="button" id="att_modal_clear_btn" class="att-btn att-btn-cancel">'._('Reset').'</button> ';
echo '<button type="submit" name="discard_cell" id="att_modal_discard_btn" class="att-btn att-btn-cancel">'._('Discard').'</button>';
echo '</div>';
echo '</div>';

end_form();

end_page();
