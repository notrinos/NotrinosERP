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
 * Get overtime request by id.
 *
 * @param int $request_id
 * @return array|false
 */
function get_overtime_request($request_id) {
    $sql = "SELECT r.*, ot.overtime_name,
        TRIM(CONCAT(COALESCE(e.first_name,''), ' ', COALESCE(e.last_name,''))) employee_name
        FROM ".TB_PREF."overtime_requests r
        LEFT JOIN ".TB_PREF."overtime ot ON ot.overtime_id = r.overtime_id
        LEFT JOIN ".TB_PREF."employees e ON e.employee_id = r.employee_id
        WHERE r.request_id = ".db_escape((int)$request_id);
    $result = db_query($sql, 'could not get overtime request');

    return db_fetch($result);
}

/**
 * Get overtime requests by optional filters.
 *
 * @param int|null $status
 * @param string $employee_id
 * @param string $from_date
 * @param string $to_date
 * @return resource
 */
function get_overtime_requests($status=null, $employee_id='', $from_date='', $to_date='') {
    $where = array('1=1');

    if ($status !== null && $status !== '')
        $where[] = 'r.status = '.db_escape((int)$status);

    if ($employee_id !== '' && $employee_id !== ALL_TEXT)
        $where[] = 'r.employee_id = '.db_escape($employee_id);

    if (trim((string)$from_date) !== '')
        $where[] = 'r.date >= '.db_escape(date2sql($from_date));

    if (trim((string)$to_date) !== '')
        $where[] = 'r.date <= '.db_escape(date2sql($to_date));

    $sql = "SELECT r.*, ot.overtime_name,
        TRIM(CONCAT(COALESCE(e.first_name,''), ' ', COALESCE(e.last_name,''))) employee_name
        FROM ".TB_PREF."overtime_requests r
        LEFT JOIN ".TB_PREF."overtime ot ON ot.overtime_id = r.overtime_id
        LEFT JOIN ".TB_PREF."employees e ON e.employee_id = r.employee_id
        WHERE ".implode(' AND ', $where)."
        ORDER BY r.request_date DESC, r.request_id DESC";

    return db_query($sql, 'could not get overtime requests');
}

/**
 * Create overtime request.
 *
 * @param string $employee_id
 * @param int $overtime_id
 * @param string $date
 * @param float $hours
 * @param string $reason
 * @return int
 */
function add_overtime_request($employee_id, $overtime_id, $date, $hours, $reason='') {
    $sql = "INSERT INTO ".TB_PREF."overtime_requests
        (employee_id, overtime_id, date, hours, reason, status)
        VALUES (
            ".db_escape($employee_id).",
            ".db_escape((int)$overtime_id).",
            ".db_escape(date2sql($date)).",
            ".db_escape((float)$hours).",
            ".db_escape($reason).",
            0
        )";

    db_query($sql, 'could not add overtime request');
    return (int)db_insert_id();
}

/**
 * Update pending overtime request.
 *
 * @param int $request_id
 * @param int $overtime_id
 * @param string $date
 * @param float $hours
 * @param string $reason
 * @return void
 */
function update_overtime_request($request_id, $overtime_id, $date, $hours, $reason='') {
    $sql = "UPDATE ".TB_PREF."overtime_requests SET
            overtime_id = ".db_escape((int)$overtime_id).",
            date = ".db_escape(date2sql($date)).",
            hours = ".db_escape((float)$hours).",
            reason = ".db_escape($reason)."
        WHERE request_id = ".db_escape((int)$request_id)."
            AND status = 0";

    db_query($sql, 'could not update overtime request');
}

/**
 * Set overtime request status.
 *
 * @param int $request_id
 * @param int $status 1=approved,2=rejected
 * @param string $approved_by
 * @return void
 */
function set_overtime_request_status($request_id, $status, $approved_by='') {
    $approved_by = trim((string)$approved_by);
    $approved_by_sql = ($approved_by === '') ? 'NULL' : db_escape($approved_by);

    $sql = "UPDATE ".TB_PREF."overtime_requests SET
            status = ".db_escape((int)$status).",
            approved_by = $approved_by_sql,
            approval_date = NOW()
        WHERE request_id = ".db_escape((int)$request_id);

    db_query($sql, 'could not update overtime request status');
}

/**
 * Approve overtime request and write attendance overtime hours.
 *
 * @param int $request_id
 * @param string $approved_by
 * @return void
 */
function approve_overtime_request($request_id, $approved_by='') {
    global $path_to_root;
    include_once($path_to_root.'/hrm/includes/db/attendance_db.inc');

    set_overtime_request_status($request_id, 1, $approved_by);
    $request = get_overtime_request($request_id);
    if ($request)
        write_attendance($request['employee_id'], (int)$request['overtime_id'], (float)$request['hours'], 1, sql2date($request['date']), false);
}

/**
 * Reject overtime request.
 *
 * @param int $request_id
 * @param string $approved_by
 * @return void
 */
function reject_overtime_request($request_id, $approved_by='') {
    set_overtime_request_status($request_id, 2, $approved_by);
}

/**
 * Delete overtime request.
 *
 * @param int $request_id
 * @return void
 */
function delete_overtime_request($request_id) {
    $sql = "DELETE FROM ".TB_PREF."overtime_requests WHERE request_id = ".db_escape((int)$request_id);
    db_query($sql, 'could not delete overtime request');
}

