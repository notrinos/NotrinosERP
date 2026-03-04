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
 * Get one leave request with related employee and leave labels.
 *
 * @param int $request_id
 * @return array|false
 */
function get_leave_request($request_id) {
    $sql = "SELECT r.*, lt.leave_name,
        TRIM(CONCAT(COALESCE(e.first_name,''), ' ', COALESCE(e.last_name,''))) employee_name
        FROM ".TB_PREF."leave_requests r
        LEFT JOIN ".TB_PREF."leave_types lt ON lt.leave_id = r.leave_id
        LEFT JOIN ".TB_PREF."employees e ON e.employee_id = r.employee_id
        WHERE r.request_id = ".db_escape((int)$request_id);
    $result = db_query($sql, 'could not get leave request');

    return db_fetch($result);
}

/**
 * Get leave request list by optional filters.
 *
 * @param int|null $status
 * @param string $employee_id
 * @param string $from_date
 * @param string $to_date
 * @return resource
 */
function get_leave_requests($status=null, $employee_id='', $from_date='', $to_date='') {
    $where = array('1=1');

    if ($status !== null && $status !== '')
        $where[] = 'r.status = '.db_escape((int)$status);

    if ($employee_id !== '' && $employee_id !== ALL_TEXT)
        $where[] = 'r.employee_id = '.db_escape($employee_id);

    if (trim((string)$from_date) !== '')
        $where[] = 'r.from_date >= '.db_escape(date2sql($from_date));

    if (trim((string)$to_date) !== '')
        $where[] = 'r.to_date <= '.db_escape(date2sql($to_date));

    $sql = "SELECT r.*, lt.leave_name,
        TRIM(CONCAT(COALESCE(e.first_name,''), ' ', COALESCE(e.last_name,''))) employee_name
        FROM ".TB_PREF."leave_requests r
        LEFT JOIN ".TB_PREF."leave_types lt ON lt.leave_id = r.leave_id
        LEFT JOIN ".TB_PREF."employees e ON e.employee_id = r.employee_id
        WHERE ".implode(' AND ', $where)."
        ORDER BY r.request_date DESC, r.request_id DESC";

    return db_query($sql, 'could not get leave requests');
}

/**
 * Create leave request.
 *
 * @param string $employee_id
 * @param int $leave_id
 * @param string $from_date
 * @param string $to_date
 * @param float $days
 * @param int $half_day
 * @param string $reason
 * @param string $doc_attachment
 * @return int
 */
function add_leave_request($employee_id, $leave_id, $from_date, $to_date, $days, $half_day=0, $reason='', $doc_attachment='') {
    $doc_attachment = trim((string)$doc_attachment);
    $doc_sql = ($doc_attachment === '') ? 'NULL' : db_escape($doc_attachment);

    $sql = "INSERT INTO ".TB_PREF."leave_requests
        (employee_id, leave_id, from_date, to_date, days, half_day, reason, doc_attachment, status)
        VALUES (
            ".db_escape($employee_id).",
            ".db_escape((int)$leave_id).",
            ".db_escape(date2sql($from_date)).",
            ".db_escape(date2sql($to_date)).",
            ".db_escape((float)$days).",
            ".db_escape((int)$half_day).",
            ".db_escape($reason).",
            $doc_sql,
            0
        )";

    db_query($sql, 'could not add leave request');
    return (int)db_insert_id();
}

/**
 * Update a pending leave request.
 *
 * @param int $request_id
 * @param int $leave_id
 * @param string $from_date
 * @param string $to_date
 * @param float $days
 * @param int $half_day
 * @param string $reason
 * @param string $doc_attachment
 * @return void
 */
function update_leave_request($request_id, $leave_id, $from_date, $to_date, $days, $half_day=0, $reason='', $doc_attachment='') {
    $doc_attachment = trim((string)$doc_attachment);
    $doc_sql = ($doc_attachment === '') ? 'NULL' : db_escape($doc_attachment);

    $sql = "UPDATE ".TB_PREF."leave_requests SET
            leave_id = ".db_escape((int)$leave_id).",
            from_date = ".db_escape(date2sql($from_date)).",
            to_date = ".db_escape(date2sql($to_date)).",
            days = ".db_escape((float)$days).",
            half_day = ".db_escape((int)$half_day).",
            reason = ".db_escape($reason).",
            doc_attachment = $doc_sql
        WHERE request_id = ".db_escape((int)$request_id)."
            AND status = 0";

    db_query($sql, 'could not update leave request');
}

/**
 * Apply request status and approval metadata.
 *
 * @param int $request_id
 * @param int $status 1=approved,2=rejected,3=cancelled
 * @param string $approved_by
 * @param string $remarks
 * @return void
 */
function set_leave_request_status($request_id, $status, $approved_by='', $remarks='') {
    $approval_user = trim((string)$approved_by);
    $approval_user_sql = ($approval_user === '') ? 'NULL' : db_escape($approval_user);
    $approval_date_sql = ((int)$status === 0) ? 'NULL' : 'NOW()';

    $sql = "UPDATE ".TB_PREF."leave_requests SET
            status = ".db_escape((int)$status).",
            approved_by = $approval_user_sql,
            approval_date = $approval_date_sql,
            approval_remarks = ".db_escape($remarks)."
        WHERE request_id = ".db_escape((int)$request_id);

    db_query($sql, 'could not update leave request status');
}

/**
 * Sync approved leave request to legacy leave_details table.
 *
 * @param int $request_id
 * @return void
 */
function sync_leave_request_to_leave_details($request_id) {
    if (!function_exists('employee_table_exists') || !employee_table_exists('leave_details'))
        return;

    $request = get_leave_request($request_id);
    if (!$request)
        return;

    $sql = "SELECT id FROM ".TB_PREF."leave_details
        WHERE employee_id = ".db_escape($request['employee_id'])."
            AND from_date = ".db_escape($request['from_date'])."
            AND to_date = ".db_escape($request['to_date'])."
        LIMIT 1";
    $result = db_query($sql, 'could not check leave_details row');
    $row = db_fetch($result);

    $approval_status = ((int)$request['status'] === 1) ? 'approved' : (((int)$request['status'] === 2) ? 'rejected' : 'pending');
    $approved_date = !empty($request['approval_date']) ? date('Y-m-d', strtotime($request['approval_date'])) : null;
    $approved_date_sql = ($approved_date === null) ? 'NULL' : db_escape($approved_date);
    $approved_by_sql = empty($request['approved_by']) ? 'NULL' : db_escape($request['approved_by']);

    if ($row) {
        $update = "UPDATE ".TB_PREF."leave_details SET
                leave_type_id = ".db_escape((int)$request['leave_id']).",
                days = ".db_escape((float)$request['days']).",
                approval_status = ".db_escape($approval_status).",
                approved_by = $approved_by_sql,
                approved_date = $approved_date_sql
            WHERE id = ".db_escape((int)$row['id']);
        db_query($update, 'could not update leave_details from request');
    } else {
        $insert = "INSERT INTO ".TB_PREF."leave_details
            (employee_id, leave_type_id, from_date, to_date, days, approval_status, approved_by, approved_date)
            VALUES (
                ".db_escape($request['employee_id']).",
                ".db_escape((int)$request['leave_id']).",
                ".db_escape($request['from_date']).",
                ".db_escape($request['to_date']).",
                ".db_escape((float)$request['days']).",
                ".db_escape($approval_status).",
                $approved_by_sql,
                $approved_date_sql
            )";
        db_query($insert, 'could not insert leave_details from request');
    }
}

/**
 * Approve leave request.
 *
 * @param int $request_id
 * @param string $approved_by
 * @param string $remarks
 * @return void
 */
function approve_leave_request($request_id, $approved_by, $remarks='') {
    set_leave_request_status($request_id, 1, $approved_by, $remarks);
    sync_leave_request_to_leave_details($request_id);
}

/**
 * Reject leave request.
 *
 * @param int $request_id
 * @param string $approved_by
 * @param string $remarks
 * @return void
 */
function reject_leave_request($request_id, $approved_by, $remarks='') {
    set_leave_request_status($request_id, 2, $approved_by, $remarks);
    sync_leave_request_to_leave_details($request_id);
}

/**
 * Cancel leave request.
 *
 * @param int $request_id
 * @param string $approved_by
 * @param string $remarks
 * @return void
 */
function cancel_leave_request($request_id, $approved_by='', $remarks='') {
    set_leave_request_status($request_id, 3, $approved_by, $remarks);
    sync_leave_request_to_leave_details($request_id);
}

