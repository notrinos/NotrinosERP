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

/**
 * Get attendance status options keyed by DB integer code.
 *
 * @return array<int,string>
 */
function hrm_attendance_status_options() {
    return array(
        0 => _('Present'),
        1 => _('Absent'),
        2 => _('Half Day'),
        3 => _('On Leave'),
        4 => _('Holiday'),
        5 => _('Weekend'),
    );
}

/**
 * Validate hours text input.
 *
 * Accepts HH:MM or numeric (0..24).
 *
 * @param string $value
 * @return bool
 */
function hrm_is_valid_hours_value($value) {
    $value = trim((string)$value);

    if ($value === '')
        return true;

    if (preg_match("/^(?(?=\d{2})(?:2[0-3]|[01][0-9])|[0-9]):[0-5][0-9]$/", $value))
        return true;

    if (is_numeric($value)) {
        $number = (float)$value;
        return ($number >= 0 && $number <= 24);
    }

    return false;
}

/**
 * Validate optional time input in HH:MM format.
 *
 * @param string $value
 * @return bool
 */
function hrm_is_valid_clock_time($value) {
    $value = trim((string)$value);

    if ($value === '')
        return true;

    return (bool)preg_match('/^(?:2[0-3]|[01]?\d):[0-5]\d$/', $value);
}

/**
 * Return whether date has approved payroll for employee.
 *
 * @param string $employee_id
 * @param string $from_date
 * @param string $to_date
 * @return bool
 */
function hrm_has_paid_date_in_range($employee_id, $from_date, $to_date) {
    $from = DateTime::createFromFormat('Y-m-d', date2sql($from_date));
    $to = DateTime::createFromFormat('Y-m-d', date2sql($to_date));

    if (!$from || !$to)
        return false;

    $to->modify('+1 day');
    $period = new DatePeriod($from, DateInterval::createFromDateString('1 day'), $to);

    foreach ($period as $dt) {
        if (check_date_paid($employee_id, sql2date($dt->format('Y-m-d'))))
            return true;
    }

    return false;
}

/**
 * Upsert meta fields on attendance row for employee/date.
 *
 * @param string $employee_id
 * @param string $date User format date.
 * @param int $status
 * @param int $shift_id
 * @param string $clock_in
 * @param string $clock_out
 * @param string $notes
 * @return void
 */
function hrm_upsert_attendance_meta($employee_id, $date, $status, $shift_id, $clock_in, $clock_out, $notes='') {
    $sql_date = date2sql($date);
    $entry = get_attendance_entry($employee_id, $date);

    $shift_sql = empty($shift_id) ? 'NULL' : db_escape($shift_id);
    $clock_in_sql = trim($clock_in) === '' ? 'NULL' : db_escape($clock_in);
    $clock_out_sql = trim($clock_out) === '' ? 'NULL' : db_escape($clock_out);
    $notes_sql = trim($notes) === '' ? 'NULL' : db_escape($notes);

    if ($entry) {
        $sql = "UPDATE ".TB_PREF."attendance SET
                status = ".db_escape((int)$status).",
                shift_id = ".$shift_sql.",
                clock_in = ".$clock_in_sql.",
                clock_out = ".$clock_out_sql.",
                notes = ".$notes_sql."
            WHERE employee_id = ".db_escape($employee_id)."
            AND date = '".$sql_date."'";
        db_query($sql, 'could not update attendance meta');
        return;
    }

    $sql = "INSERT INTO ".TB_PREF."attendance
        (employee_id, date, shift_id, clock_in, clock_out, regular_hours, overtime_hours, overtime_type_id, status, source, rate, notes)
        VALUES (
            ".db_escape($employee_id).",
            '".$sql_date."',
            ".$shift_sql.",
            ".$clock_in_sql.",
            ".$clock_out_sql.",
            0,
            0,
            NULL,
            ".db_escape((int)$status).",
            0,
            1,
            ".$notes_sql."
        )";

    db_query($sql, 'could not create attendance meta row');
}

/**
 * Collect employee rows for selected filter.
 *
 * @param int $department_id
 * @param string $employee_id
 * @return array<int,array>
 */
function hrm_get_filtered_employees($department_id, $employee_id='') {
    $sql = "SELECT e.*, CONCAT(e.first_name, ' ', e.last_name) as employee_name
        FROM ".TB_PREF."employees e
        WHERE !e.inactive";

    if (!empty($department_id))
        $sql .= " AND e.department_id = ".db_escape((int)$department_id);

    if ($employee_id !== '')
        $sql .= " AND e.employee_id = ".db_escape($employee_id);

    $sql .= " ORDER BY e.first_name, e.last_name";

    $result = db_query($sql, 'could not get employees for attendance');
    $rows = array();

    while ($row = db_fetch($result)) {
        $rows[] = $row;
    }

    return $rows;
}

/**
 * Validate attendance submit payload.
 *
 * @param array<int,array> $employees
 * @return bool
 */
function hrm_can_process_attendance($employees) {
    if (!is_date($_POST['from_date'])) {
        display_error(_('From Date is invalid.'));
        set_focus('from_date');
        return false;
    }

    if (!is_date($_POST['to_date'])) {
        display_error(_('To Date is invalid.'));
        set_focus('to_date');
        return false;
    }

    if (date_comp($_POST['from_date'], $_POST['to_date']) > 0) {
        display_error(_('From Date cannot be greater than To Date.'));
        set_focus('from_date');
        return false;
    }

    if (date_comp($_POST['from_date'], Today()) > 0 || date_comp($_POST['to_date'], Today()) > 0) {
        display_error(_('Attendance cannot be entered for future dates.'));
        set_focus('from_date');
        return false;
    }

    foreach ($employees as $employee) {
        $employee_id = $employee['employee_id'];

        if (!hrm_is_valid_hours_value(get_post($employee_id.'-regular'))) {
            display_error(_('Regular hours must be numeric (0..24) or HH:MM format.'));
            set_focus($employee_id.'-regular');
            return false;
        }

        if (!hrm_is_valid_hours_value(get_post($employee_id.'-ot_hours'))) {
            display_error(_('Overtime hours must be numeric (0..24) or HH:MM format.'));
            set_focus($employee_id.'-ot_hours');
            return false;
        }

        if (!hrm_is_valid_clock_time(get_post($employee_id.'-clock_in'))) {
            display_error(_('Clock In must be in HH:MM format.'));
            set_focus($employee_id.'-clock_in');
            return false;
        }

        if (!hrm_is_valid_clock_time(get_post($employee_id.'-clock_out'))) {
            display_error(_('Clock Out must be in HH:MM format.'));
            set_focus($employee_id.'-clock_out');
            return false;
        }

        if (get_post($employee_id.'-ot_hours') !== '' && (int)get_post($employee_id.'-ot_type') <= 0) {
            display_error(_('Overtime type is required when overtime hours are entered.'));
            set_focus($employee_id.'-ot_type');
            return false;
        }

        if (get_post($employee_id.'-regular') === '' && get_post($employee_id.'-ot_hours') === ''
            && (int)get_post($employee_id.'-leave') <= 0 && trim((string)get_post($employee_id.'-clock_in')) === ''
            && trim((string)get_post($employee_id.'-clock_out')) === '' && trim((string)get_post($employee_id.'-notes')) === '') {
            continue;
        }

        if (hrm_has_paid_date_in_range($employee_id, $_POST['from_date'], $_POST['to_date'])) {
            display_error(sprintf(_('Employee %s has payroll-approved date(s) in selected range.'), $employee_id));
            set_focus('from_date');
            return false;
        }
    }

    return true;
}

page(_($help_context = 'Attendance Entry'), false, false, '', $js);

if (!isset($_POST['from_date']))
    $_POST['from_date'] = Today();
if (!isset($_POST['to_date']))
    $_POST['to_date'] = Today();
if (!isset($_POST['department_id']))
    $_POST['department_id'] = 0;
if (!isset($_POST['employee_id']))
    $_POST['employee_id'] = '';

// When a department is selected, clear any previously selected employee that
// no longer belongs to that department so the filters stay consistent.
$dept_id = (int)get_post('department_id');
$emp_id  = get_post('employee_id');
if ($dept_id > 0 && $emp_id !== '') {
    $chk = db_query("SELECT COUNT(*) FROM ".TB_PREF."employees WHERE employee_id = ".db_escape($emp_id)." AND department_id = ".db_escape($dept_id),
        'check employee department');
    $chk_row = db_fetch_row($chk);
    if (empty($chk_row[0])) {
        $_POST['employee_id'] = '';
        $emp_id = '';
    }
}

$employees = hrm_get_filtered_employees($dept_id, $emp_id);

if (isset($_POST['bulk_regular'])) {
    $bulk_hours = get_company_pref('default_work_hours');
    foreach ($employees as $employee) {
        if (check_value('selected_'.$employee['employee_id']))
            $_POST[$employee['employee_id'].'-regular'] = $bulk_hours;
    }
    $Ajax->activate('_page_body');
}

if (isset($_POST['save_attendance'])) {
    if (hrm_can_process_attendance($employees)) {
        $saved_rows = 0;
        $statuses = hrm_attendance_status_options();

        foreach ($employees as $employee) {
            $employee_id = $employee['employee_id'];
            if (!check_value('selected_'.$employee_id))
                continue;

            $regular = trim((string)get_post($employee_id.'-regular'));
            $ot_hours = trim((string)get_post($employee_id.'-ot_hours'));
            $ot_type = (int)get_post($employee_id.'-ot_type');
            $leave_id = (int)get_post($employee_id.'-leave');
            $status = (int)get_post($employee_id.'-status');
            $shift_id = (int)get_post($employee_id.'-shift');
            $clock_in = trim((string)get_post($employee_id.'-clock_in'));
            $clock_out = trim((string)get_post($employee_id.'-clock_out'));
            $notes = trim((string)get_post($employee_id.'-notes'));

            $from = DateTime::createFromFormat('Y-m-d', date2sql($_POST['from_date']));
            $to = DateTime::createFromFormat('Y-m-d', date2sql($_POST['to_date']));
            $to->modify('+1 day');
            $period = new DatePeriod($from, DateInterval::createFromDateString('1 day'), $to);

            foreach ($period as $dt) {
                $entry_date = sql2date($dt->format('Y-m-d'));

                if ($leave_id > 0) {
                    write_attendance($employee_id, 0, 0, 1, $entry_date, $leave_id);
                    hrm_upsert_attendance_meta($employee_id, $entry_date, 3, $shift_id, $clock_in, $clock_out, $notes);
                    $saved_rows++;
                    continue;
                }

                if ($regular !== '')
                    write_attendance($employee_id, 0, time_to_float($regular), 1, $entry_date);

                if ($ot_hours !== '' && $ot_type > 0) {
                    $overtime = get_overtime($ot_type);
                    $ot_rate = $overtime ? $overtime['pay_rate'] : 1;
                    write_attendance($employee_id, $ot_type, time_to_float($ot_hours), $ot_rate, $entry_date);
                }

                if (!array_key_exists($status, $statuses))
                    $status = 0;

                hrm_upsert_attendance_meta($employee_id, $entry_date, $status, $shift_id, $clock_in, $clock_out, $notes);
                $saved_rows++;
            }
        }

        if ($saved_rows > 0)
            display_notification(_('Attendance has been saved.'));
        else
            display_notification(_('Nothing to save.'));

        $Ajax->activate('_page_body');
    }
}

start_form();

start_table(TABLESTYLE_NOBORDER);
start_row();
date_cells(_('From Date:'), 'from_date', _('Attendance start date'));
date_cells(_('To Date:'), 'to_date', _('Attendance end date'));
departments_list_cells(_('Department:'), 'department_id', get_post('department_id'), _('All departments'), true);

// Build employee dropdown options: always async=false so selecting an employee
// triggers a full _page_body refresh (same behaviour as department filter).
// When a department is active, restrict the dropdown to that department's employees.
$emp_list_opts = array('async' => false);
if ($dept_id > 0)
    $emp_list_opts['where'] = array("e.department_id = ".db_escape($dept_id));

employees_list_cells(_('Employee:'), 'employee_id', $emp_id, _('All employees'), true, false, false, $emp_list_opts);
submit_cells('bulk_regular', _('Fill Default Hours'), '', _('Fill selected rows using company default work hours'), false);
end_row();
end_table(1);

if (!db_has_employees()) {
    display_error(_('There are no employees available for attendance entry.'));
    end_form();
    end_page();
    return;
}

$overtime_result = get_all_overtime();
$overtime_options = array(0 => _('Select Overtime Type'));
while ($ot = db_fetch($overtime_result))
    $overtime_options[$ot['overtime_id']] = $ot['overtime_name'];

$status_options = hrm_attendance_status_options();

start_table(TABLESTYLE2);
$th = array(
    _('Select'),
    _('Employee ID'),
    _('Employee Name'),
    _('Shift'),
    _('Status'),
    _('Clock In'),
    _('Clock Out'),
    _('Regular Hrs'),
    _('OT Type'),
    _('OT Hrs'),
    _('Leave Type'),
    _('Notes')
);
table_header($th);

foreach ($employees as $employee) {
    $employee_id = $employee['employee_id'];

    if (!isset($_POST['selected_'.$employee_id]))
        $_POST['selected_'.$employee_id] = 1;
    if (!isset($_POST[$employee_id.'-status']))
        $_POST[$employee_id.'-status'] = 0;
    if (!isset($_POST[$employee_id.'-ot_type']))
        $_POST[$employee_id.'-ot_type'] = 0;

    start_row();
    check_cells('', 'selected_'.$employee_id, $_POST['selected_'.$employee_id]);
    label_cell($employee_id);
    label_cell($employee['employee_name']);

    if (function_exists('work_shifts_list')) {
        echo "<td>";
        echo work_shifts_list($employee_id.'-shift', get_post($employee_id.'-shift'), true, false);
        echo "</td>";
    } else
        label_cell('-');

    echo "<td>";
    echo array_selector($employee_id.'-status', get_post($employee_id.'-status'), $status_options);
    echo "</td>";

    text_cells(null, $employee_id.'-clock_in', get_post($employee_id.'-clock_in'), 5, 5);
    text_cells(null, $employee_id.'-clock_out', get_post($employee_id.'-clock_out'), 5, 5);
    text_cells(null, $employee_id.'-regular', get_post($employee_id.'-regular'), 5, 8);

    echo "<td>";
    echo array_selector($employee_id.'-ot_type', get_post($employee_id.'-ot_type'), $overtime_options);
    echo "</td>";

    text_cells(null, $employee_id.'-ot_hours', get_post($employee_id.'-ot_hours'), 5, 8);

    echo "<td>";
    echo leave_types_list($employee_id.'-leave', get_post($employee_id.'-leave'), true, false);
    echo "</td>";

    text_cells(null, $employee_id.'-notes', get_post($employee_id.'-notes'), 20, 120);
    end_row();
}

end_table(1);

submit_center('save_attendance', _('Save Attendance'), true, '', 'default');

end_form();
end_page();

