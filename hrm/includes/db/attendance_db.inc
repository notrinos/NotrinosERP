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

function time_to_float($time) {
	if(strpos($time, ':') != false) {
		sscanf($time, "%d:%d", $hours, $minutes);
		$time_in_float = $hours + $minutes/60;
	}
	else
		$time_in_float = $time;
	
	return $time_in_float;
}

function check_attended($employee_id, $time_id, $date) {
	$sql = "SELECT COUNT(*) FROM ".TB_PREF."attendance WHERE employee_id = ".db_escape($employee_id)." AND time_id = ".db_escape($time_id)." AND date = '".date2sql($date)."'";
	
	return check_empty_result($sql);
}

function check_leave($employee_id, $date) {
	$sql = "SELECT COUNT(*) FROM ".TB_PREF."leave_details WHERE employee_id = ".db_escape($employee_id)." AND date = '".date2sql($date)."'";
	
	return check_empty_result($sql);
}

function write_attendance($employee_id, $time_id, $hours_no, $rate, $date, $leave_id=false) {

	if(!empty($leave_id)) {
		if(check_leave($employee_id, $date))
			$sql = "UPDATE ".TB_PREF."leave_details SET leave_id = ".db_escape($leave_id).", pay_rate = ".db_escape($rate)." WHERE date = '".date2sql($date)."' AND employee_id = ".db_escape($employee_id);
		else
			$sql = "INSERT INTO ".TB_PREF."leave_details (employee_id, leave_id, pay_rate, date) VALUES(".db_escape($employee_id).",".db_escape($leave_id).",".db_escape($rate).",'".date2sql($date)."')";

		$sql2 = "DELETE FROM ".TB_PREF."attendance WHERE employee_id = ".db_escape($employee_id)." AND date = '".date2sql($date)."'";
	}
	elseif(is_numeric($hours_no)) {

		if(check_attended($employee_id, $time_id, $date))
			$sql = "UPDATE ".TB_PREF."attendance SET hours_no = ".db_escape($hours_no).", rate = ".db_escape($rate)." WHERE date = '".date2sql($date)."' AND employee_id = ".db_escape($employee_id)." AND time_id = ".db_escape($time_id);
		else
			$sql = "INSERT INTO ".TB_PREF."attendance (employee_id, time_id, hours_no, rate, date) VALUES(".db_escape($employee_id).",".db_escape($time_id).",".db_escape($hours_no).",".db_escape($rate).",'".date2sql($date)."')";

		$sql2 = "DELETE FROM ".TB_PREF."leave_details WHERE employee_id = ".db_escape($employee_id)." AND date = '".date2sql($date)."'";
	}

	if(!empty($sql))
		db_query($sql, 'Could not write attendance data');
	if(!empty($sql2))
		db_query($sql2, 'Could not write attendance data');
}

function check_date_paid($person_id, $date) {
	return check_empty_result("SELECT COUNT(*) FROM ".TB_PREF."payslips WHERE employee_id = ".db_escape($person_id)." AND '".date2sql($date)."' BETWEEN from_date AND to_date");
}

function get_attendance($from, $to, $employee_id=0, $department=0, $time_id=0) {

	$from = DateTime::createFromFormat('Y-m-d', date2sql($from));
	$to = DateTime::createFromFormat('Y-m-d', date2sql($to));

	$sql = "SELECT emp.employee_id, CONCAT(emp_first_name,' ',emp_last_name) ";

	while(sql_date_comp($from->format('Y-m-d'),$to->format('Y-m-d')) <= 0) {
		$sql .= ", IFNULL((SELECT hours_no FROM ".TB_PREF."attendance WHERE ".TB_PREF."attendance.employee_id = emp.employee_id AND date = '".$from->format('Y-m-d')."' AND time_id = ".db_escape($time_id)."), (SELECT CASE WHEN lt.pay_rate >= '100' THEN CONCAT('<b style=color:green>',leave_code,'</b>') WHEN lt.pay_rate < '100' AND lt.pay_rate > '0' THEN CONCAT('<b style=color:orange>',leave_code,'</b>') WHEN lt.pay_rate <= '0' THEN CONCAT('<b style=color:red>',lt.leave_code,'</b>') END FROM ".TB_PREF."leave_types lt, ".TB_PREF."leave_details l WHERE l.leave_id = lt.leave_id AND l.date = '".$from->format('Y-m-d')."' AND l.employee_id = emp.employee_id)) as '".$from->format('Y-m-d')."'";
		$from->modify('+1 day');
	}
	$sql .= " FROM ".TB_PREF."employee emp WHERE !emp.inactive";

	if(!empty($employee_id))
		$sql .= " AND emp.employee_id = ".db_escape($employee_id);
	if(!empty($department))
		$sql .= " AND emp.department_id = ".db_escape($department);

	$sql .= " GROUP BY emp.employee_id";

	return $sql;
}
