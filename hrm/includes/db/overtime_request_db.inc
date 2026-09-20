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
 * Get and lock one overtime request for a guarded terminal transition.
 *
 * @param int $request_id
 * @return array|false
 */
function get_overtime_request_for_update($request_id) {
    $sql = "SELECT * FROM ".TB_PREF."overtime_requests
        WHERE request_id = ".db_escape((int)$request_id)."
        FOR UPDATE";
    $result = db_query($sql, 'could not lock overtime request');

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
 * @param int $status 1=approved,2=rejected,3=cancelled
 * @param string $approved_by
 * @param int|null $expected_status Optional current status required for the update
 * @return bool
 */
function set_overtime_request_status(
    $request_id,
    $status,
    $approved_by='',
    $expected_status=null
) {
    $approved_by = trim((string)$approved_by);
    $approved_by_sql = ($approved_by === '') ? 'NULL' : db_escape($approved_by);

    $sql = "UPDATE ".TB_PREF."overtime_requests SET
            status = ".db_escape((int)$status).",
            approved_by = $approved_by_sql,
            approval_date = NOW()
        WHERE request_id = ".db_escape((int)$request_id);
    if ($expected_status !== null)
        $sql .= " AND status = ".db_escape((int)$expected_status);

    $result = db_query($sql, 'could not update overtime request status');
    if (!$result)
        return false;

    return $expected_status === null || db_num_affected_rows() === 1;
}

/**
 * Approve an overtime request under HRM-TIM-002 custody.
 *
 * Request approval is authorization evidence only. It MUST NOT create worked
 * time, approved-time, overtime totals, rate/multiplier evidence, or payroll
 * input. Actual worked/approved time remains HRM-TIM-001 custody and is
 * qualified separately under an explicit governed policy.
 *
 */
function approve_overtime_request($request_id, $approved_by='') {
    global $path_to_root, $transaction_level;
    if (empty($transaction_level))
        return false;
    if (!function_exists('get_hrm_lifecycle_attendance_mutation_guard_for_update'))
        include_once($path_to_root.'/hrm/includes/db/lifecycle_attendance_mutation_guard_db.inc');
    include_once($path_to_root.'/hrm/includes/db/hrm_tim_002_governance_db.inc');

    $request = get_overtime_request_for_update($request_id);
    if (!$request
        || (int)$request['status'] !== 0
        || trim((string)$request['employee_id']) === ''
        || (int)$request['overtime_id'] <= 0
        || (float)$request['hours'] <= 0
        || (float)$request['hours'] > 24
        || trim((string)$request['date']) === ''
    ) return false;

    // Preserve predecessor lifecycle/payroll immutability checks even though
    // TIM-002 no longer mutates attendance as a side effect of request approval.
    $guard = get_hrm_lifecycle_attendance_mutation_guard_for_update(
        $request['employee_id'], $request['date'], 'attendance_write');
    if (!is_array($guard) || !isset($guard['can_mutate']) || $guard['can_mutate'] !== true)
        return false;

    $draft_id = isset($GLOBALS['hrm_tim_002_overtime_approval_draft_id'])
        ? (int)$GLOBALS['hrm_tim_002_overtime_approval_draft_id'] : null;
    $tim2_error = null;
    $receipt = hrm_tim_002_record_overtime_request_approval(
        $request_id,
        trim((string)$approved_by) === '' ? 'approval_system' : $approved_by,
        $draft_id,
        $tim2_error
    );
    if ($receipt === false)
        return false;

    return set_overtime_request_status($request_id, 1, $approved_by, 0);
}

/**
 * Reject overtime request.
 *
 * @param int $request_id
 * @param string $approved_by
 * @param int|null $expected_status Optional current status required for the update
 * @return bool
 */
function reject_overtime_request(
    $request_id,
    $approved_by='',
    $expected_status=null
) {
    return set_overtime_request_status(
        $request_id,
        2,
        $approved_by,
        $expected_status
    );
}

/**
 * Cancel overtime request.
 *
 * @param int $request_id
 * @param string $approved_by
 * @param int|null $expected_status Optional current status required for the update
 * @return bool
 */
function cancel_overtime_request(
    $request_id,
    $approved_by='',
    $expected_status=null
) {
    return set_overtime_request_status(
        $request_id,
        3,
        $approved_by,
        $expected_status
    );
}

/**
 * Delete overtime request.
 *
 * @param int $request_id
 * @param int|null $expected_status Optional current status required for deletion
 * @return bool
 */
function delete_overtime_request($request_id, $expected_status=null) {
    $sql = "DELETE FROM ".TB_PREF."overtime_requests
        WHERE request_id = ".db_escape((int)$request_id);
    if ($expected_status !== null)
        $sql .= " AND status = ".db_escape((int)$expected_status);

    $result = db_query($sql, 'could not delete overtime request');
    if (!$result)
        return false;

    return $expected_status === null || db_num_affected_rows() === 1;
}
