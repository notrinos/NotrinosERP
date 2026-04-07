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

/**
 * Convert user-entered hours into float value.
 *
 * Supports values in HH:MM and numeric formats.
 *
 * @param string|float|int $time Time string or numeric value.
 * @return float
 */
function time_to_float($time) {
	$time = trim((string)$time);

	if ($time === '')
		return 0.0;

	if (strpos($time, ':') !== false) {
		sscanf($time, "%d:%d", $hours, $minutes);
		return (float)$hours + ((float)$minutes / 60);
	}

	return (float)$time;
}

/**
 * Check whether attendance exists for employee/date.
 *
 * The $time_id parameter is kept for backward compatibility.
 *
 * @param string $employee_id Employee code.
 * @param int $time_id Legacy time identifier (unused in new schema).
 * @param string $date Date in user format.
 * @return bool
 */
function check_attended($employee_id, $time_id, $date) {
	$sql = "SELECT COUNT(*) FROM ".TB_PREF."attendance
		WHERE employee_id = ".db_escape($employee_id)."
		AND date = '".date2sql($date)."'";

	return check_empty_result($sql);
}

/**
 * Check whether employee has leave covering a date.
 *
 * @param string $employee_id Employee code.
 * @param string $date Date in user format.
 * @return bool
 */
function check_leave($employee_id, $date) {
	$sql_date = date2sql($date);
	$sql = "SELECT COUNT(*) FROM ".TB_PREF."leave_details
		WHERE employee_id = ".db_escape($employee_id)."
		AND '".$sql_date."' BETWEEN from_date AND to_date
		AND approval_status <> 'rejected'";

	return check_empty_result($sql);
}

/**
 * Get one attendance record by employee/date.
 *
 * @param string $employee_id Employee code.
 * @param string $date Date in user format.
 * @return array|false
 */
function get_attendance_entry($employee_id, $date) {
	$sql = "SELECT * FROM ".TB_PREF."attendance
		WHERE employee_id = ".db_escape($employee_id)."
		AND date = '".date2sql($date)."'";
	$result = db_query($sql, 'could not get attendance entry');

	return db_fetch($result);
}

/**
 * Write attendance or leave entry for one employee/day.
 *
 * Backward compatibility:
 * - $time_id = 0 writes regular hours
 * - $time_id > 0 writes overtime hours and overtime type
 * - $leave_id writes a one-day approved leave record and removes attendance row for that date
 *
 * @param string $employee_id Employee code.
 * @param int $time_id Legacy time identifier.
 * @param float|int $hours_no Hours value.
 * @param float|int $rate Rate or multiplier.
 * @param string $date Date in user format.
 * @param int|false $leave_id Leave type id or false.
 * @return void
 */
function write_attendance($employee_id, $time_id, $hours_no, $rate, $date, $leave_id=false) {
	$sql_date = date2sql($date);

	if (!empty($leave_id)) {
		$approval_user = isset($_SESSION['wa_current_user']) ? $_SESSION['wa_current_user']->loginname : '';
		$approval_date = date2sql(Today());

		$existing_leave = "SELECT id FROM ".TB_PREF."leave_details
			WHERE employee_id = ".db_escape($employee_id)."
			AND from_date = '".$sql_date."'
			AND to_date = '".$sql_date."'
			LIMIT 1";
		$existing_leave_result = db_query($existing_leave, 'Could not check leave data');
		$existing_leave_row = db_fetch($existing_leave_result);

		if ($existing_leave_row) {
			$sql = "UPDATE ".TB_PREF."leave_details SET
					leave_type_id = ".db_escape($leave_id).",
					days = 1,
					approval_status = 'approved',
					approved_by = ".db_escape($approval_user).",
					approved_date = '".$approval_date."'
				WHERE id = ".db_escape($existing_leave_row['id']);
		} else {
			$sql = "INSERT INTO ".TB_PREF."leave_details
				(employee_id, leave_type_id, from_date, to_date, days, approval_status, approved_by, approved_date)
				VALUES (
					".db_escape($employee_id).",
					".db_escape($leave_id).",
					'".$sql_date."',
					'".$sql_date."',
					1,
					'approved',
					".db_escape($approval_user).",
					'".$approval_date."'
				)";
		}
		db_query($sql, 'Could not write leave data');

		$sql = "DELETE FROM ".TB_PREF."attendance
			WHERE employee_id = ".db_escape($employee_id)."
			AND date = '".$sql_date."'";
		db_query($sql, 'Could not clear attendance data');
		return;
	}

	if (!is_numeric($hours_no))
		return;

	$hours_no = (float)$hours_no;
	$rate = (float)$rate;
	$is_overtime = ((int)$time_id > 0);
	$existing = get_attendance_entry($employee_id, $date);

	if ($existing) {
		$regular_hours = $is_overtime ? $existing['regular_hours'] : $hours_no;
		$overtime_hours = $is_overtime ? $hours_no : $existing['overtime_hours'];
		$overtime_type_id = $is_overtime ? (int)$time_id : (empty($existing['overtime_type_id']) ? 0 : (int)$existing['overtime_type_id']);
		$status = ($regular_hours > 0 || $overtime_hours > 0) ? 0 : 1;

		$sql = "UPDATE ".TB_PREF."attendance SET
				regular_hours = ".db_escape($regular_hours).",
				overtime_hours = ".db_escape($overtime_hours).",
				overtime_type_id = ".db_escape($overtime_type_id).",
				rate = ".db_escape($rate).",
				status = ".db_escape($status)."
			WHERE employee_id = ".db_escape($employee_id)."
			AND date = '".$sql_date."'";
	} else {
		$regular_hours = $is_overtime ? 0 : $hours_no;
		$overtime_hours = $is_overtime ? $hours_no : 0;
		$overtime_type_id = $is_overtime ? (int)$time_id : 0;
		$status = ($regular_hours > 0 || $overtime_hours > 0) ? 0 : 1;

		$sql = "INSERT INTO ".TB_PREF."attendance
			(employee_id, date, regular_hours, overtime_hours, overtime_type_id, rate, source, status)
			VALUES (
				".db_escape($employee_id).",
				'".$sql_date."',
				".db_escape($regular_hours).",
				".db_escape($overtime_hours).",
				".db_escape($overtime_type_id).",
				".db_escape($rate).",
				0,
				".db_escape($status)."
			)";
	}

	db_query($sql, 'Could not write attendance data');
}

/**
 * Check if date belongs to an existing payslip period for employee.
 *
 * @param string $person_id Employee code.
 * @param string $date Date in user format.
 * @return bool
 */
function check_date_paid($person_id, $date) {
	return check_empty_result(
		"SELECT COUNT(*) FROM ".TB_PREF."payslips
		WHERE employee_id = ".db_escape($person_id)."
		AND '".date2sql($date)."' BETWEEN from_date AND to_date"
	);
}

/**
 * Build attendance matrix query for inquiry screens.
 *
 * @param string $from From date in user format.
 * @param string $to To date in user format.
 * @param string $employee_id Optional employee code.
 * @param int $department Optional department id.
 * @param int $time_id 0=regular hours, >0=overtime hours.
 * @return string SQL statement.
 */
function get_attendance($from, $to, $employee_id=0, $department=0, $time_id=0) {
	$from = DateTime::createFromFormat('Y-m-d', date2sql($from));
	$to = DateTime::createFromFormat('Y-m-d', date2sql($to));
	$field = ((int)$time_id > 0) ? 'overtime_hours' : 'regular_hours';

	$sql = "SELECT emp.employee_id, CONCAT(emp.first_name,' ',emp.last_name) AS employee_name";

	while (sql_date_comp($from->format('Y-m-d'), $to->format('Y-m-d')) <= 0) {
		$column_date = $from->format('Y-m-d');
		$sql .= ", IFNULL(
				(SELECT a.".$field." FROM ".TB_PREF."attendance a
					WHERE a.employee_id = emp.employee_id AND a.date = '".$column_date."'
					LIMIT 1),
				(SELECT lt.leave_code FROM ".TB_PREF."leave_types lt
					INNER JOIN ".TB_PREF."leave_details l ON l.leave_type_id = lt.leave_id
					WHERE l.employee_id = emp.employee_id
					AND '".$column_date."' BETWEEN l.from_date AND l.to_date
					AND l.approval_status <> 'rejected'
					LIMIT 1)
			) AS '".$column_date."'";
		$from->modify('+1 day');
	}

	$sql .= " FROM ".TB_PREF."employees emp WHERE !emp.inactive";

	if (!empty($employee_id))
		$sql .= " AND emp.employee_id = ".db_escape($employee_id);

	if (!empty($department))
		$sql .= " AND emp.department_id = ".db_escape($department);

	$sql .= " ORDER BY emp.first_name, emp.last_name";

	return $sql;
}

/**
 * Fetch all attendance records for a given month, keyed by employee_id and day.
 *
 * @param int $year
 * @param int $month
 * @param int $department_id Optional department filter.
 * @param string $employee_id Optional single employee filter.
 * @return array Nested: [employee_id][day_number] => row
 */
function get_monthly_attendance($year, $month, $department_id = 0, $employee_id = '') {
	$from_sql = sprintf('%04d-%02d-01', (int)$year, (int)$month);
	$days_in_month = (int)date('t', strtotime($from_sql));
	$to_sql = sprintf('%04d-%02d-%02d', (int)$year, (int)$month, $days_in_month);

	$sql = "SELECT a.* FROM ".TB_PREF."attendance a
		INNER JOIN ".TB_PREF."employees e ON e.employee_id = a.employee_id
		WHERE a.date BETWEEN '".$from_sql."' AND '".$to_sql."'
		AND !e.inactive";

	if (!empty($department_id))
		$sql .= " AND e.department_id = ".db_escape((int)$department_id);
	if ($employee_id !== '')
		$sql .= " AND a.employee_id = ".db_escape($employee_id);

	$sql .= " ORDER BY a.employee_id, a.date";
	$result = db_query($sql, 'could not get monthly attendance');

	$data = array();
	while ($row = db_fetch($result)) {
		$day = (int)date('j', strtotime($row['date']));
		$data[$row['employee_id']][$day] = $row;
	}
	return $data;
}

/**
 * Fetch holidays that fall within a given month.
 *
 * Returns an array keyed by day number.
 *
 * @param int $year
 * @param int $month
 * @return array [day_number] => holiday_name
 */
function get_month_holidays($year, $month) {
	$from_sql = sprintf('%04d-%02d-01', (int)$year, (int)$month);
	$days_in_month = (int)date('t', strtotime($from_sql));
	$to_sql = sprintf('%04d-%02d-%02d', (int)$year, (int)$month, $days_in_month);

	$holidays = array();

	$sql = "SELECT * FROM ".TB_PREF."holidays
		WHERE (holiday_date BETWEEN '".$from_sql."' AND '".$to_sql."')
		   OR (to_date IS NOT NULL AND to_date >= '".$from_sql."' AND holiday_date <= '".$to_sql."')
		   OR (recurring = 1 AND DATE_FORMAT(holiday_date, '%m-%d') BETWEEN
		       DATE_FORMAT('".$from_sql."', '%m-%d') AND DATE_FORMAT('".$to_sql."', '%m-%d'))";

	$result = db_query($sql, 'could not get holidays');
	while ($row = db_fetch($result)) {
		$hstart = max(strtotime($from_sql), strtotime($row['holiday_date']));
		$hend = !empty($row['to_date']) ? min(strtotime($to_sql), strtotime($row['to_date'])) : $hstart;
		for ($ts = $hstart; $ts <= $hend; $ts += 86400) {
			$d = (int)date('j', $ts);
			$holidays[$d] = $row['holiday_name'];
		}
	}
	return $holidays;
}

/**
 * Build working-day map: DOW (0=Sun..6=Sat) => [is_working, work_hours].
 *
 * @return array
 */
function get_working_day_map() {
	$map = array();
	$result = db_query("SELECT * FROM ".TB_PREF."working_days ORDER BY day_of_week", 'could not get working days');
	while ($row = db_fetch($result)) {
		$map[(int)$row['day_of_week']] = array(
			'is_working' => (int)$row['is_working'],
			'work_hours' => (float)$row['work_hours']
		);
	}
	return $map;
}

/**
 * Fetch leave records covering a given month for display in the attendance sheet.
 *
 * @param int $year
 * @param int $month
 * @param int $department_id
 * @param string $employee_id
 * @return array Nested: [employee_id][day_number] => leave row
 */
/**
 * Upsert attendance metadata (status, shift, clock times, notes) for an employee/date.
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
 * Delete a single attendance record for an employee on a given SQL-format date.
 *
 * @param string $employee_id
 * @param string $date_sql  Date in Y-m-d format.
 * @return void
 */
function delete_attendance_record($employee_id, $date_sql) {
	$sql = "DELETE FROM ".TB_PREF."attendance
		WHERE employee_id = ".db_escape($employee_id)."
		AND date = '".db_escape($date_sql)."'";
	db_query($sql, 'could not delete attendance record');
}

/**
 * Get active employees for the attendance sheet, optionally filtered.
 * Returns a db result with employee_id, employee_name (for grid)
 * or employee_id only when $ids_only = true (for bulk operations).
 *
 * @param int    $department_id  0 = all departments.
 * @param string $employee_id    '' = all employees.
 * @param bool   $ids_only       When true, select only employee_id.
 * @return resource DB result
 */
function get_attendance_sheet_employees($department_id = 0, $employee_id = '', $ids_only = false) {
	if ($ids_only) {
		$select = "e.employee_id";
	} else {
		$select = "e.employee_id, CONCAT(e.first_name, ' ', e.last_name) AS employee_name";
	}

	$sql = "SELECT ".$select."
		FROM ".TB_PREF."employees e
		WHERE !e.inactive";

	if (!empty($department_id))
		$sql .= " AND e.department_id = ".db_escape((int)$department_id);
	if ($employee_id !== '')
		$sql .= " AND e.employee_id = ".db_escape($employee_id);

	if (!$ids_only)
		$sql .= " ORDER BY e.first_name, e.last_name";

	return db_query($sql, 'could not get attendance sheet employees');
}

function get_monthly_leaves($year, $month, $department_id = 0, $employee_id = '') {
	$from_sql = sprintf('%04d-%02d-01', (int)$year, (int)$month);
	$days_in_month = (int)date('t', strtotime($from_sql));
	$to_sql = sprintf('%04d-%02d-%02d', (int)$year, (int)$month, $days_in_month);

	$sql = "SELECT ld.*, lt.leave_name, lt.leave_code
		FROM ".TB_PREF."leave_details ld
		INNER JOIN ".TB_PREF."leave_types lt ON lt.leave_id = ld.leave_type_id
		INNER JOIN ".TB_PREF."employees e ON e.employee_id = ld.employee_id
		WHERE ld.from_date <= '".$to_sql."' AND ld.to_date >= '".$from_sql."'
		AND ld.approval_status <> 'rejected'
		AND !e.inactive";

	if (!empty($department_id))
		$sql .= " AND e.department_id = ".db_escape((int)$department_id);
	if ($employee_id !== '')
		$sql .= " AND ld.employee_id = ".db_escape($employee_id);

	$result = db_query($sql, 'could not get monthly leaves');
	$data = array();
	while ($row = db_fetch($result)) {
		$ls = max(strtotime($from_sql), strtotime($row['from_date']));
		$le = min(strtotime($to_sql), strtotime($row['to_date']));
		for ($ts = $ls; $ts <= $le; $ts += 86400) {
			$d = (int)date('j', $ts);
			$data[$row['employee_id']][$d] = $row;
		}
	}
	return $data;
}
