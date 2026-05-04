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
include_once($path_to_root.'/hrm/includes/db/holiday_db.inc');

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

if (!isset($_POST['sheet_month']))
    $_POST['sheet_month'] = (int)date('n');
if (!isset($_POST['sheet_year']))
    $_POST['sheet_year'] = (int)date('Y');
if (!isset($_POST['department_id']))
    $_POST['department_id'] = 0;
if (!isset($_POST['employee_id']))
    $_POST['employee_id'] = '';

$sel_month = (int)$_POST['sheet_month'];
$sel_year  = (int)$_POST['sheet_year'];
$dept_id   = (int)get_post('department_id');
$emp_id    = get_post('employee_id');

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
    $input_error   = false;

    if (check_date_paid($cell_emp, $cell_date)) {
        display_error(_('Cannot modify attendance for a payroll-locked date.'));
        $input_error = true;
    }
    if (!$input_error && $cell_regular !== '' && !is_numeric($cell_regular)) {
        display_error(_('Regular hours must be numeric.'));
        $input_error = true;
    }
    if (!$input_error) {
        if ($cell_leave > 0) {
            write_attendance($cell_emp, 0, 0, 1, $cell_date, $cell_leave);
            hrm_upsert_attendance_meta($cell_emp, $cell_date, 3, $cell_shift, $cell_clock_in, $cell_clock_out, $cell_notes);
        } else {
            if ($cell_regular !== '')
                write_attendance($cell_emp, 0, time_to_float($cell_regular), 1, $cell_date);
            if ($cell_ot_hours !== '' && $cell_ot_type > 0) {
                $overtime = get_overtime($cell_ot_type);
                $ot_rate = $overtime ? $overtime['pay_rate'] : 1;
                write_attendance($cell_emp, $cell_ot_type, time_to_float($cell_ot_hours), $ot_rate, $cell_date);
            }
            hrm_upsert_attendance_meta($cell_emp, $cell_date, $cell_status, $cell_shift, $cell_clock_in, $cell_clock_out, $cell_notes);
        }
        display_notification(_('Attendance saved for ').$cell_emp.' — '.sql2date($cell_date_sql));
    }
    $Ajax->activate('_page_body');
}

//----------------------------------------------------------------------
// Delete handler
//----------------------------------------------------------------------

if (isset($_POST['delete_cell'])) {
    $cell_emp      = $_POST['cell_employee_id'];
    $cell_date_sql = $_POST['cell_date'];
    $cell_date     = sql2date($cell_date_sql);

    if (check_date_paid($cell_emp, $cell_date)) {
        display_error(_('Cannot delete attendance for a payroll-locked date.'));
    } else {
        delete_attendance_record($cell_emp, $cell_date_sql);
        display_notification(_('Attendance deleted for ').$cell_emp.' — '.sql2date($cell_date_sql));
    }
    $Ajax->activate('_page_body');
}

//----------------------------------------------------------------------
// Bulk fill handler — mark all working days as Present with scheduled hrs
//----------------------------------------------------------------------

if (isset($_POST['bulk_fill'])) {
    $emp_checks = isset($_POST['emp_check']) ? $_POST['emp_check'] : array();
    if (empty($emp_checks)) {
        display_error(_('No employees selected. Please check the rows to fill.'));
    } else {
    $wd_map = get_working_day_map();
    $days_in_month = (int)date('t', mktime(0, 0, 0, $sel_month, 1, $sel_year));
    $att_data = get_monthly_attendance($sel_year, $sel_month, $dept_id, $emp_id);
    $holidays = get_month_holidays($sel_year, $sel_month);
    $filled = 0;

    foreach ($emp_checks as $eid) {
        for ($d = 1; $d <= $days_in_month; $d++) {
            $ts = mktime(0, 0, 0, $sel_month, $d, $sel_year);
            $dow = (int)date('w', $ts);
            $date_sql = date('Y-m-d', $ts);
            $user_date = sql2date($date_sql);

            if (isset($wd_map[$dow]) && !$wd_map[$dow]['is_working']) continue;
            if (isset($holidays[$d])) continue;
            if (isset($att_data[$eid][$d])) continue;
            if (strtotime($date_sql) > strtotime(date('Y-m-d'))) continue;
            if (check_date_paid($eid, $user_date)) continue;

            $hours = isset($wd_map[$dow]) ? $wd_map[$dow]['work_hours'] : 8;
            write_attendance($eid, 0, $hours, 1, $user_date);
            hrm_upsert_attendance_meta($eid, $user_date, 0, 0, '', '', '');
            $filled++;
        }
    }

    if ($filled > 0)
        display_notification(sprintf(_('%d attendance records filled.'), $filled));
    else
        display_notification(_('No records to fill — all working days already have entries or are in the future.'));
    } // end else emp_checks
    $Ajax->activate('_page_body');
}

//----------------------------------------------------------------------
// Bulk delete handler
//----------------------------------------------------------------------

if (isset($_POST['bulk_delete'])) {
    $emp_checks = isset($_POST['emp_check']) ? $_POST['emp_check'] : array();
    if (empty($emp_checks)) {
        display_error(_('No employees selected for bulk delete.'));
    } else {
        $days_in_month_del = (int)date('t', mktime(0, 0, 0, $sel_month, 1, $sel_year));
        foreach ($emp_checks as $eid) {
            for ($d = 1; $d <= $days_in_month_del; $d++) {
                $ts = mktime(0, 0, 0, $sel_month, $d, $sel_year);
                $date_sql = date('Y-m-d', $ts);
                $user_date = sql2date($date_sql);
                if (check_date_paid($eid, $user_date)) continue;
                delete_attendance_record($eid, $date_sql);
            }
        }
        display_notification(sprintf(_('Bulk delete completed for %d employee(s).'), count($emp_checks)));
    }
    $Ajax->activate('_page_body');
}

//----------------------------------------------------------------------
// Load data for the grid
//----------------------------------------------------------------------

$days_in_month = (int)date('t', mktime(0, 0, 0, $sel_month, 1, $sel_year));
$wd_map     = get_working_day_map();
$att_data   = get_monthly_attendance($sel_year, $sel_month, $dept_id, $emp_id);
$leave_data = get_monthly_leaves($sel_year, $sel_month, $dept_id, $emp_id);
$holidays   = get_month_holidays($sel_year, $sel_month);

$emp_result = get_attendance_sheet_employees($dept_id, $emp_id);
$employees = array();
while ($row = db_fetch($emp_result))
    $employees[] = $row;

$overtime_result = get_all_overtime();
$overtime_options = array(0 => _('-- None --'));
while ($ot = db_fetch($overtime_result))
    $overtime_options[$ot['overtime_id']] = $ot['overtime_name'];

//----------------------------------------------------------------------
// Render filters
//----------------------------------------------------------------------

start_form();

start_table(TABLESTYLE_NOBORDER);
start_row();
months_list_cells(_('Month:'), 'sheet_month', $sel_month, true);
years_list_cells(_('Fiscal Year:'), 'sheet_year', null);
departments_list_cells(_('Department:'), 'department_id', get_post('department_id'), _('All departments'), true);

$emp_list_opts = array('async' => false);
if ($dept_id > 0)
    $emp_list_opts['where'] = array("e.department_id = ".db_escape($dept_id));
employees_list_cells(_('Employee:'), 'employee_id', $emp_id, _('All employees'), true, false, false, $emp_list_opts);

check_cells(_('Select All Rows'), 'check_all_emp', null, false, _('Select/Deselect all employees'));
submit_cells('bulk_fill', _('Fill Working Days'), '', _('Auto-fill all working days with scheduled hours for checked employees'), false);
submit_cells('bulk_delete', _('Bulk Delete'), '', _('Delete ALL attendance records for checked employees in the selected month'), false);
end_row();
end_table(1);

submit_js_confirm('bulk_delete', _('Delete ALL attendance records for checked employees in the selected month?'));

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
    end_form();
    end_page();
    return;
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
    $ts = mktime(0, 0, 0, $sel_month, $d, $sel_year);
    $dow = (int)date('w', $ts);
    $day_abbr = substr(date('D', $ts), 0, 2);
    $is_weekend = isset($wd_map[$dow]) && !$wd_map[$dow]['is_working'];
    $is_holiday = isset($holidays[$d]);
    $is_today = (date('Y-m-d', $ts) === date('Y-m-d'));

    $cls = 'att-day-header';
    if ($is_weekend) $cls .= ' att-weekend-col';
    if ($is_holiday) $cls .= ' att-holiday-col';
    if ($is_today)   $cls .= ' att-today-col';

    $title = date('l, M j', $ts);
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
    $emp_att   = isset($att_data[$eid]) ? $att_data[$eid] : array();
    $emp_leave = isset($leave_data[$eid]) ? $leave_data[$eid] : array();

    $sum_wd = 0; $sum_present = 0; $sum_absent = 0; $sum_leave = 0;
    $sum_hours = 0; $sum_ot = 0;

    $row_class = ($k % 2 == 0) ? 'att-row-even' : 'att-row-odd';
    echo '<tr class="'.$row_class.'">';
    echo '<td class="att-emp-id att-sticky-col"><input type="checkbox" name="emp_check[]" value="'.htmlspecialchars($eid, ENT_QUOTES).'" class="emp-check"> '.htmlspecialchars($eid).'</td>';
    echo '<td class="att-emp-name att-sticky-col att-sticky-name">'.htmlspecialchars($emp['employee_name']).'</td>';

    for ($d = 1; $d <= $days_in_month; $d++) {
        $ts = mktime(0, 0, 0, $sel_month, $d, $sel_year);
        $dow = (int)date('w', $ts);
        $date_sql = date('Y-m-d', $ts);
        $is_future = (strtotime($date_sql) > strtotime(date('Y-m-d')));
        $is_today  = ($date_sql === date('Y-m-d'));

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
            echo ' data-name="'.htmlspecialchars($emp['employee_name'], ENT_QUOTES).'"';
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
            echo ' onclick="openAttModal(this)">';
            echo '<span>'.$cell_label.'</span></td>';
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

//----------------------------------------------------------------------
// Modal dialog
//----------------------------------------------------------------------

echo '<div id="att-modal-overlay" class="att-modal-overlay" style="display:none" onclick="closeAttModal()"></div>';
echo '<div id="att-modal" class="att-modal" style="display:none">';
echo '<div class="att-modal-header">';
echo '<span class="att-modal-title">'._('Attendance Entry').'</span>';
echo '<span class="att-modal-close" onclick="closeAttModal()">&times;</span>';
echo '</div>';
echo '<div class="att-modal-body">';

echo '<input type="hidden" name="cell_employee_id" id="cell_employee_id" value="">';
echo '<input type="hidden" name="cell_date" id="cell_date" value="">';

echo '<table class="tablestyle2">';
echo '<tr><td class="label">'._('Employee:').'</td>';
echo '<td><strong><span id="modal_emp_label"></span></strong></td></tr>';
echo '<tr><td class="label">'._('Date:').'</td>';
echo '<td><strong><span id="modal_date_label"></span></strong></td></tr>';
echo '<tr><td class="label">'._('Status:').'</td><td>';
echo array_selector('cell_status', 0, $status_options);
echo '</td></tr>';
echo '<tr><td class="label">'._('Regular Hours:').'</td>';
echo '<td><input type="text" name="cell_regular" id="cell_regular" class="att-modal-input" maxlength="8" value=""></td></tr>';
echo '<tr><td class="label">'._('OT Type:').'</td><td>';
echo array_selector('cell_ot_type', 0, $overtime_options);
echo '</td></tr>';
echo '<tr><td class="label">'._('OT Hours:').'</td>';
echo '<td><input type="text" name="cell_ot_hours" id="cell_ot_hours" class="att-modal-input" maxlength="8" value=""></td></tr>';

if (function_exists('work_shifts_list')) {
    echo '<tr><td class="label">'._('Shift:').'</td><td>';
    echo work_shifts_list('cell_shift', null, true, false);
    echo '</td></tr>';
}

echo '<tr><td class="label">'._('Clock In:').'</td>';
echo '<td><input type="text" name="cell_clock_in" id="cell_clock_in" class="att-modal-input" maxlength="5" placeholder="HH:MM" value=""></td></tr>';
echo '<tr><td class="label">'._('Clock Out:').'</td>';
echo '<td><input type="text" name="cell_clock_out" id="cell_clock_out" class="att-modal-input" maxlength="5" placeholder="HH:MM" value=""></td></tr>';
echo '<tr><td class="label">'._('Leave Type:').'</td><td>';
echo leave_types_list('cell_leave', null, true, false);
echo '</td></tr>';
echo '<tr><td class="label">'._('Notes:').'</td>';
echo '<td><textarea name="cell_notes" id="cell_notes" class="att-modal-input" rows="3" maxlength="200"></textarea></td></tr>';
echo '</table>';

echo '</div>';
echo '<div class="att-modal-footer">';
echo '<button type="submit" name="save_cell" value="1" class="ajaxsubmit att-btn att-btn-save">'._('Save').'</button> ';
echo '<button type="submit" name="delete_cell" value="1" class="ajaxsubmit att-btn att-btn-delete" onclick="return confirm(\''._('Delete this attendance entry?').'\');">'._('Delete').'</button> ';
echo '<button type="button" class="att-btn att-btn-cancel" onclick="closeAttModal()">'._('Cancel').'</button>';
echo '</div>';
echo '</div>';

end_form();

//----------------------------------------------------------------------
// JavaScript
//----------------------------------------------------------------------

echo '<script type="text/javascript">
function openAttModal(cell) {
    var emp  = cell.getAttribute("data-emp");
    var date = cell.getAttribute("data-date");
    var name = cell.getAttribute("data-name");

    document.getElementById("cell_employee_id").value = emp;
    document.getElementById("cell_date").value = date;
    document.getElementById("modal_emp_label").textContent = emp + " \u2014 " + name;

    var parts = date.split("-");
    document.getElementById("modal_date_label").textContent = parts[2] + "/" + parts[1] + "/" + parts[0];

    // Pre-fill from data attributes or reset
    var f = function(id, attr, def) {
        var v = cell.getAttribute(attr);
        document.getElementById(id).value = (v !== null && v !== "" && v !== "0" && v !== "0.00") ? v : def;
    };
    f("cell_regular",   "data-regular",   "");
    f("cell_ot_hours",  "data-ot-hours",  "");
    f("cell_clock_in",  "data-clock-in",  "");
    f("cell_clock_out", "data-clock-out", "");
    f("cell_notes",     "data-notes",     "");

    // Status selector
    var statusVal = cell.getAttribute("data-status");
    var statusSel = document.querySelector("[name=cell_status]");
    if (statusSel) statusSel.value = (statusVal !== null) ? statusVal : "0";

    // OT type selector
    var otVal = cell.getAttribute("data-ot-type");
    var otSel = document.querySelector("[name=cell_ot_type]");
    if (otSel) otSel.value = (otVal !== null && otVal !== "0") ? otVal : "0";

    // Shift selector
    var shiftVal = cell.getAttribute("data-shift");
    var shiftSel = document.querySelector("[name=cell_shift]");
    if (shiftSel) shiftSel.value = (shiftVal !== null && shiftVal !== "0") ? shiftVal : "";

    // Leave selector
    var lvSel = document.querySelector("[name=cell_leave]");
    if (lvSel) lvSel.value = "";

    document.getElementById("att-modal-overlay").style.display = "block";
    document.getElementById("att-modal").style.display = "block";

    // Fix Select2 widths - they are measured when modal is hidden so re-apply after visible
    if (window.jQuery) {
        setTimeout(function() {
            jQuery("#att-modal .select2-container").css("width", "100%");
        }, 20);
    }

    document.getElementById("cell_regular").focus();
}

function closeAttModal() {
    document.getElementById("att-modal-overlay").style.display = "none";
    document.getElementById("att-modal").style.display = "none";
}

document.addEventListener("keydown", function(e) {
    if (e.key === "Escape") closeAttModal();
});

function toggleAllEmpChecks(checked) {
    var boxes = document.querySelectorAll("input.emp-check");
    for (var i = 0; i < boxes.length; i++) {
        boxes[i].checked = checked;
    }
    var hdr = document.getElementById("check_all_emp_header");
    if (hdr) hdr.checked = checked;
    var flt = document.getElementsByName("check_all_emp");
    if (flt.length) flt[0].checked = checked;
}

// Bind the check_all_emp checkbox (generated by check_cells)
(function() {
    var cb = document.getElementsByName("check_all_emp");
    if (cb.length) {
        cb[0].addEventListener("click", function() {
            toggleAllEmpChecks(this.checked);
        });
    }
    var hdr = document.getElementById("check_all_emp_header");
    if (hdr) {
        hdr.addEventListener("click", function() {
            toggleAllEmpChecks(this.checked);
        });
    }
})();
</script>';

end_page();