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
$path_to_root  = '..';

include_once($path_to_root.'/includes/session.inc');

$js = '';
if ($SysPrefs->use_popup_windows)
	$js .= get_js_open_window(900, 500);
if (user_use_date_picker())
	$js .= get_js_date_picker();

include_once($path_to_root.'/includes/ui.inc');
include_once($path_to_root.'/hrm/includes/db/attendance_db.inc');
include_once($path_to_root.'/hrm/includes/db/employee_db.inc');
include_once($path_to_root.'/hrm/includes/db/overtime_db.inc');

//--------------------------------------------------------------------------

function can_process() {
	global $employees;

	if(!is_date($_POST['from_date'])) {
		display_error(_('The entered date is invalid.'));
		set_focus('from_date');
		return false;
	}
	elseif(!is_date($_POST['to_date'])) {
		display_error(_('The entered date is invalid.'));
		set_focus('to_date');
		return false;
	}
	elseif(date_comp($_POST['from_date'], Today()) > 0) {

		display_error(_('Cannot make attendance for the date in the future.'));
		set_focus('from_date');
		return false;
	}
	elseif(date_comp($_POST['to_date'], Today()) > 0) {

		display_error(_('Cannot make attendance for the date in the future.'));
		set_focus('to_date');
		return false;
	}
	
	foreach($employees as $emp) {

		$employee_id = $emp['employee_id'];
		$err = _('Attendance input data must be greater than 0, less than 24 hours and formatted in <b>HH:MM</b> or <b>Integer</b>, example - 02:25 , 2:25, 8, 23:59 ...');
		
		if(strlen($_POST[$employee_id.'-0']) != 0 && (!preg_match("/^(?(?=\d{2})(?:2[0-3]|[01][0-9])|[0-9]):[0-5][0-9]$/", $_POST[$employee_id.'-0']) && (!is_numeric($_POST[$employee_id.'-0']) || $_POST[$employee_id.'-0'] >= 24 || $_POST[$employee_id.'-0'] <= 0)) && empty($_POST[$employee_id.'-leave'])) {

			display_error($err);
			set_focus($employee_id.'-0');
			return false;
		}
		foreach(get_all_overtime() as $ot) {

			$ot_id = $ot['overtime_id'];
			
			if(strlen($_POST[$employee_id.'-'.$ot_id]) != 0 && (!preg_match("/^(?(?=\d{2})(?:2[0-3]|[01][0-9])|[0-9]):[0-5][0-9]$/", $_POST[$employee_id.'-'.$ot_id]) && (!is_numeric($_POST[$employee_id.'-'.$ot_id]) || $_POST[$employee_id.'-'.$ot_id] >= 24 || $_POST[$employee_id.'-'.$ot_id] <= 0)) && empty($_POST[$employee_id.'-leave'])) {
				
				display_error($err);
				set_focus($employee_id.'-'.$ot_id);
				return false;
			}
		}
	}
	return true;
}

function write_attendance_range($employee_id, $time_type, $value, $rate, $from, $to, $leave=false) {

	$from = date2sql($from);
	$to = date2sql($to);
	$begin = new DateTime($from);
	$end = new DateTime($to);
	$end = $end->modify('+1 day');
	$interval = DateInterval::createFromDateString('1 day');
	$period = new DatePeriod($begin, $interval, $end);
	$weekend = get_company_pref('weekend_day');

	foreach ($period as $dt) {
		$day = $dt->format('Y-m-d');
		$day = sql2date($day);

		if($dt->format('N') != $weekend)
			write_attendance($employee_id, $time_type, $value, $rate, $day, $leave);
	}
}

function check_paid_in_range($employee_id, $from, $to) {

	// $from = date2sql($from);
	// $to = date2sql($to);
	// $begin = new DateTime($from);
	// $end = new DateTime($to);
	// $end = $end->modify('+1 day');
	// $interval = DateInterval::createFromDateString('1 day');
	// $period = new DatePeriod($begin, $interval, $end);

	// foreach ($period as $dt) {
	// 	$day = $dt->format('Y-m-d');
	// 	$day = sql2date($day);
	// 	if(check_date_paid($employee_id, $day))
	// 		return true;
	// }
	return false;
}

//--------------------------------------------------------------------------

page(_($help_context = 'Employee Attendance'), false, false, '', $js);

start_form();

start_table(TABLESTYLE_NOBORDER);
start_row();
date_cells(_('From:'), 'from_date', _('Attendance date begin'));
date_cells(_('To:'), 'to_date', _('Aattendance date end'));
departments_list_cells(_('For department:'), 'department_id', null, _('All departments'), true);
submit_cells('bulk', _('Bulk'), '', _('Record all as regular work'), true);
end_row();
end_table(1);

start_table(TABLESTYLE2);
$initial_cols = array(_('ID'), _('Employee'), _('Regular time'));
$overtimes = get_all_overtime();
$remaining_cols = array();
$overtime_id    = array();
$k=0;
while($overtime = db_fetch($overtimes)) {
	$remaining_cols[$k] = $overtime['overtime_name'];
	$overtime_id[$k] = $overtime['overtime_id'];
	$k++;
}
$remaining_cols[] = _('Leave Type');

$th = array_merge($initial_cols, $remaining_cols);
$employees = get_employees(get_post('department_id'));
$emp_ids = array();

$k = 0;
foreach ($employees as $emp) {
	$emp_ids[$k] = $emp['employee_id'];
	$k++;
}

if(isset($_POST['bulk'])) {
	foreach($emp_ids as $employee_id) {
		if(get_post($employee_id) == 1)
			$_POST[$employee_id.'-0'] = get_company_pref('default_work_hours');
		else
			$_POST[$employee_id.'-0'] = '';
	}
	$Ajax->activate('_page_body');
}

table_header($th);

foreach($employees as $employee) {
	
	start_row();
	label_cell(checkbox(null, $employee['employee_id'], isset($_POST[$employee['employee_id']]) ? $_POST[$employee['employee_id']] : 1).$employee['employee_id']);
	label_cell($employee['first_name'].' '.$employee['last_name']);
	$name1 = $employee['employee_id'].'-0';
	text_cells(null, $name1, null, 5, 5);
	
	$i=0;
	while($i < count($remaining_cols) - 1) {
		$name2 = $employee['employee_id'].'-'.$overtime_id[$i];
		text_cells(null, $name2, null, 5, 5);
		$i++;
	}
	leave_types_list_cells(null, $employee['employee_id'].'-leave', null, false, _('Select Leave Type'));
	end_row();
}

end_table(1);
	
submit_center('submit', _('Save attendance'), true, '', 'default');

//--------------------------------------------------------------------------

if(!db_has_employees())
	display_error(_('There are no employees for attendance.'));

if(isset($_POST['submit'])) {
	
	if(!can_process())
		return;
	
	$att_items = 0;
	foreach($emp_ids as $employee_id) {
		
		if($_POST[$employee_id.'-0'] && check_paid_in_range($employee_id, $_POST['from_date'], $_POST['to_date'])) {
			
			display_error(_('The selected date range includes a date that has been approved, please select another date range.'));
			set_focus('from_date');
			exit();
		}
		elseif(!empty($_POST[$employee_id.'-leave'])) {
			$emp_leave = $_POST[$employee_id.'-leave'];
			$leave_rate = get_leave_type($emp_leave)['pay_rate'];
			$att_items ++;
			write_attendance_range($employee_id, 0, 0, $leave_rate, $_POST['from_date'], $_POST['to_date'], $emp_leave);
		}
		else {

			if(strlen($_POST[$employee_id.'-0']) > 0)
				$att_items ++;
			
			write_attendance_range($employee_id, 0, time_to_float($_POST[$employee_id.'-0']), 1, $_POST['from_date'], $_POST['to_date']);

			foreach($overtime_id as $ot) {
				$rate = get_overtime($ot)['pay_rate'];
				if(strlen($_POST[$employee_id.'-'.$ot]) > 0)
					$att_items ++;
				write_attendance_range($employee_id, $ot, time_to_float($_POST[$employee_id.'-'.$ot]), $rate, $_POST['from_date'], $_POST['to_date']);
			}
		}
	}
	if($att_items > 0)
		display_notification(_('Attendance has been saved.'));
	else
		display_notification(_('Nothing added'));
	$Ajax->activate('_page_body');
}

end_form();
end_page();
