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

/**********************************************************************
	NotrinosERP-1.0 Approval Workflow System
	Core Draft CRUD Operations

	Provides create, read, update functions for approval_drafts table.
	All queries use db_escape() for SQL injection prevention.
***********************************************************************/

/** Draft status constants */
define('APPROVAL_STATUS_PENDING',   0);
define('APPROVAL_STATUS_APPROVED',  1);
define('APPROVAL_STATUS_REJECTED',  2);
define('APPROVAL_STATUS_CANCELLED', 3);
define('APPROVAL_STATUS_EXPIRED',   4);
define('APPROVAL_STATUS_DELEGATED', 5);

/** Draft status labels */
$approval_status_labels = array(
	APPROVAL_STATUS_PENDING   => _('Pending'),
	APPROVAL_STATUS_APPROVED  => _('Approved'),
	APPROVAL_STATUS_REJECTED  => _('Rejected'),
	APPROVAL_STATUS_CANCELLED => _('Cancelled'),
	APPROVAL_STATUS_EXPIRED   => _('Expired'),
	APPROVAL_STATUS_DELEGATED => _('Delegated'),
);

/**
 * Insert a new draft record into approval_drafts
 *
 * @param int    $workflow_id       Workflow ID
 * @param int    $trans_type        Transaction type constant (ST_*)
 * @param int    $reserved_trans_no Reserved transaction number
 * @param string $draft_data        JSON-encoded transaction data
 * @param float  $amount            Transaction amount
 * @param string $currency          Currency code (e.g., 'USD')
 * @param int    $person_type_id    Person type (customer/supplier/employee)
 * @param string $person_id         Person identifier
 * @param int    $submitted_by      User ID who submitted
 * @param int    $workflow_version  Snapshot of workflow version at submission
 * @param string $reference         Generated reference string
 * @param string $summary           Human-readable summary
 * @param string $loc_code          Location code (optional)
 * @return int   Inserted draft ID
 */
function add_approval_draft($workflow_id, $trans_type, $reserved_trans_no, $draft_data,
	$amount = 0, $currency = null, $person_type_id = null, $person_id = null,
	$submitted_by = null, $workflow_version = 1, $reference = null, $summary = null, $loc_code = null)
{
	if ($submitted_by === null)
		$submitted_by = $_SESSION['wa_current_user']->user;

	$sql = "INSERT INTO " . TB_PREF . "approval_drafts
		(workflow_id, trans_type, reserved_trans_no, draft_data, amount, currency,
		 person_type_id, person_id, current_level, status, submitted_by, submitted_date,
		 workflow_version, reference, summary, loc_code)
		VALUES ("
		. db_escape($workflow_id) . ", "
		. db_escape($trans_type) . ", "
		. db_escape($reserved_trans_no) . ", "
		. db_escape($draft_data) . ", "
		. db_escape($amount) . ", "
		. db_escape($currency, true) . ", "
		. db_escape($person_type_id, true) . ", "
		. db_escape($person_id, true) . ", "
		. "1, "
		. APPROVAL_STATUS_PENDING . ", "
		. db_escape($submitted_by) . ", "
		. "NOW(), "
		. db_escape($workflow_version) . ", "
		. db_escape($reference, true) . ", "
		. db_escape($summary, true) . ", "
		. db_escape($loc_code, true) . ")";

	db_query($sql, 'could not insert approval draft');

	return db_insert_id();
}

/**
 * Retrieve a single draft record by ID
 *
 * @param int $draft_id Draft ID
 * @return array|false  Draft row or false if not found
 */
function get_approval_draft($draft_id)
{
	$sql = "SELECT d.*, w.name AS workflow_name, w.require_comments_on_reject,
			w.require_comments_on_approve, w.allow_edit_on_approve, w.allow_self_approve
		FROM " . TB_PREF . "approval_drafts d
		LEFT JOIN " . TB_PREF . "approval_workflows w ON w.id = d.workflow_id
		WHERE d.id = " . db_escape($draft_id);

	$result = db_query($sql, 'could not retrieve approval draft');

	return db_fetch($result);
}

/**
 * Retrieve a draft by its reserved transaction number and type
 *
 * @param int $trans_type Transaction type
 * @param int $trans_no   Reserved transaction number
 * @return array|false    Draft row or false
 */
function get_approval_draft_by_trans($trans_type, $trans_no)
{
	$sql = "SELECT d.*, w.name AS workflow_name
		FROM " . TB_PREF . "approval_drafts d
		LEFT JOIN " . TB_PREF . "approval_workflows w ON w.id = d.workflow_id
		WHERE d.trans_type = " . db_escape($trans_type) . "
		AND d.reserved_trans_no = " . db_escape($trans_no);

	$result = db_query($sql, 'could not retrieve approval draft by trans');

	return db_fetch($result);
}

/**
 * Update draft status
 *
 * @param int $draft_id Draft ID
 * @param int $status   New status (use APPROVAL_STATUS_* constants)
 * @return void
 */
function update_approval_draft_status($draft_id, $status)
{
	$completed_date = in_array($status, array(
		APPROVAL_STATUS_APPROVED, APPROVAL_STATUS_REJECTED,
		APPROVAL_STATUS_CANCELLED, APPROVAL_STATUS_EXPIRED
	)) ? ", completed_date = NOW()" : "";

	$sql = "UPDATE " . TB_PREF . "approval_drafts
		SET status = " . db_escape($status) . $completed_date . "
		WHERE id = " . db_escape($draft_id);

	db_query($sql, 'could not update approval draft status');
}

/**
 * Update the current approval level of a draft
 *
 * @param int $draft_id Draft ID
 * @param int $level    New current level
 * @return void
 */
function update_approval_draft_level($draft_id, $level)
{
	$sql = "UPDATE " . TB_PREF . "approval_drafts
		SET current_level = " . db_escape($level) . "
		WHERE id = " . db_escape($draft_id);

	db_query($sql, 'could not update approval draft level');
}

/**
 * Update approved_trans_no after successful execution
 *
 * @param int $draft_id Draft ID
 * @param int $trans_no The actual posted transaction number
 * @return void
 */
function update_approval_draft_trans_no($draft_id, $trans_no)
{
	$sql = "UPDATE " . TB_PREF . "approval_drafts
		SET approved_trans_no = " . db_escape($trans_no) . "
		WHERE id = " . db_escape($draft_id);

	db_query($sql, 'could not update approval draft trans_no');
}

/**
 * Update draft data (when draft is edited during approval)
 *
 * @param int    $draft_id   Draft ID
 * @param string $draft_data New JSON-encoded data
 * @param float  $amount     Updated amount
 * @param string $summary    Updated summary
 * @return void
 */
function update_approval_draft_data($draft_id, $draft_data, $amount = null, $summary = null)
{
	$sql = "UPDATE " . TB_PREF . "approval_drafts SET "
		. "draft_data = " . db_escape($draft_data);

	if ($amount !== null)
		$sql .= ", amount = " . db_escape($amount);
	if ($summary !== null)
		$sql .= ", summary = " . db_escape($summary);

	$sql .= " WHERE id = " . db_escape($draft_id);

	db_query($sql, 'could not update approval draft data');
}

/**
 * Get pending drafts for a specific user based on their role
 *
 * @param int   $user_id   User ID
 * @param int   $role_id   Role ID of the user
 * @param array $filters   Optional filters: trans_type, from_date, to_date, status
 * @return resource         DB result set
 */
function get_pending_approval_drafts_for_user($user_id, $role_id, $filters = array())
{
	$sql = "SELECT d.*, w.name AS workflow_name,
			u.real_name AS submitted_by_name
		FROM " . TB_PREF . "approval_drafts d
		LEFT JOIN " . TB_PREF . "approval_workflows w ON w.id = d.workflow_id
		LEFT JOIN " . TB_PREF . "users u ON u.id = d.submitted_by
		INNER JOIN " . TB_PREF . "approval_levels al
			ON al.workflow_id = d.workflow_id
			AND al.level = d.current_level
			AND al.role_id = " . db_escape($role_id) . "
			AND al.is_active = 1
		WHERE d.status = " . APPROVAL_STATUS_PENDING;

	if (!empty($filters['trans_type']))
		$sql .= " AND d.trans_type = " . db_escape($filters['trans_type']);

	if (!empty($filters['from_date']))
		$sql .= " AND d.submitted_date >= " . db_escape($filters['from_date']);

	if (!empty($filters['to_date']))
		$sql .= " AND d.submitted_date <= " . db_escape($filters['to_date']);

	$sql .= " ORDER BY d.submitted_date DESC";

	return db_query($sql, 'could not retrieve pending approval drafts');
}

/**
 * Get all drafts submitted by a specific user
 *
 * @param int   $user_id User ID
 * @param array $filters Optional filters: status, trans_type, from_date, to_date
 * @return resource       DB result set
 */
function get_approval_drafts_by_submitter($user_id, $filters = array())
{
	$sql = "SELECT d.*, w.name AS workflow_name
		FROM " . TB_PREF . "approval_drafts d
		LEFT JOIN " . TB_PREF . "approval_workflows w ON w.id = d.workflow_id
		WHERE d.submitted_by = " . db_escape($user_id);

	if (isset($filters['status']) && $filters['status'] !== '')
		$sql .= " AND d.status = " . db_escape($filters['status']);

	if (!empty($filters['trans_type']))
		$sql .= " AND d.trans_type = " . db_escape($filters['trans_type']);

	if (!empty($filters['from_date']))
		$sql .= " AND d.submitted_date >= " . db_escape($filters['from_date']);

	if (!empty($filters['to_date']))
		$sql .= " AND d.submitted_date <= " . db_escape($filters['to_date']);

	$sql .= " ORDER BY d.submitted_date DESC";

	return db_query($sql, 'could not retrieve user approval drafts');
}

/**
 * Count pending drafts for a user's role (for dashboard badge)
 *
 * @param int $role_id Role ID
 * @return int         Count of pending drafts
 */
function count_pending_approval_drafts($role_id)
{
	$sql = "SELECT COUNT(DISTINCT d.id) AS cnt
		FROM " . TB_PREF . "approval_drafts d
		INNER JOIN " . TB_PREF . "approval_levels al
			ON al.workflow_id = d.workflow_id
			AND al.level = d.current_level
			AND al.role_id = " . db_escape($role_id) . "
			AND al.is_active = 1
		WHERE d.status = " . APPROVAL_STATUS_PENDING;

	$result = db_query($sql, 'could not count pending approval drafts');
	$row = db_fetch_row($result);

	return (int)$row[0];
}

/**
 * Count approvals already collected at a given level for a draft
 *
 * @param int $draft_id Draft ID
 * @param int $level    Approval level
 * @return int          Number of approvals at this level
 */
function count_approval_actions_at_level($draft_id, $level)
{
	$sql = "SELECT COUNT(*) AS cnt
		FROM " . TB_PREF . "approval_actions
		WHERE draft_id = " . db_escape($draft_id) . "
		AND level = " . db_escape($level) . "
		AND action_type = 'approve'";

	$result = db_query($sql, 'could not count approval actions at level');
	$row = db_fetch_row($result);

	return (int)$row[0];
}

/**
 * Check if a specific user has already approved a draft at a given level
 *
 * @param int $draft_id Draft ID
 * @param int $user_id  User ID
 * @param int $level    Approval level
 * @return bool         True if already approved
 */
function has_user_approved_draft($draft_id, $user_id, $level)
{
	$sql = "SELECT COUNT(*) AS cnt
		FROM " . TB_PREF . "approval_actions
		WHERE draft_id = " . db_escape($draft_id) . "
		AND user_id = " . db_escape($user_id) . "
		AND level = " . db_escape($level) . "
		AND action_type = 'approve'";

	$result = db_query($sql, 'could not check user approval status');
	$row = db_fetch_row($result);

	return $row[0] > 0;
}
