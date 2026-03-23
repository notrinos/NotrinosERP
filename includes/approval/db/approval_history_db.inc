<?php
/**********************************************************************
	NotrinosERP-1.0 Approval Workflow System
	Approval History DB Functions

	Provides functions for recording and querying approval actions,
	delegations, and notification records.
***********************************************************************/

// =====================================================
// Approval Actions (Audit Trail)
// =====================================================

/**
 * Record an approval action in the audit trail
 *
 * @param int    $draft_id           Draft ID
 * @param string $action_type        Action type: submit, approve, reject, delegate, escalate, cancel, edit, resubmit
 * @param int    $user_id            User performing the action
 * @param int    $level              Approval level at which action occurred
 * @param string $comments           Action comments/reason
 * @param string $draft_data_snapshot JSON snapshot if draft was edited (null otherwise)
 * @return int   Inserted action ID
 */
function add_approval_action($draft_id, $action_type, $user_id = null, $level = 0,
	$comments = null, $draft_data_snapshot = null)
{
	if ($user_id === null)
		$user_id = $_SESSION['wa_current_user']->user;

	$ip_address = isset($_SERVER['REMOTE_ADDR']) ? $_SERVER['REMOTE_ADDR'] : '';

	$sql = "INSERT INTO " . TB_PREF . "approval_actions
		(draft_id, action_type, user_id, level, comments,
		 draft_data_snapshot, action_date, ip_address)
		VALUES ("
		. db_escape($draft_id) . ", "
		. db_escape($action_type) . ", "
		. db_escape($user_id) . ", "
		. db_escape($level) . ", "
		. db_escape($comments, true) . ", "
		. db_escape($draft_data_snapshot, true) . ", "
		. "NOW(), "
		. db_escape($ip_address) . ")";

	db_query($sql, 'could not record approval action');

	return db_insert_id();
}

/**
 * Get all approval actions (history) for a draft
 *
 * @param int  $draft_id Draft ID
 * @param bool $reverse  If true, return oldest first; default is newest first
 * @return resource       DB result set
 */
function get_approval_history($draft_id, $reverse = false)
{
	$order = $reverse ? 'ASC' : 'DESC';

	$sql = "SELECT a.*, u.real_name AS user_name
		FROM " . TB_PREF . "approval_actions a
		LEFT JOIN " . TB_PREF . "users u ON u.id = a.user_id
		WHERE a.draft_id = " . db_escape($draft_id) . "
		ORDER BY a.action_date $order, a.id $order";

	return db_query($sql, 'could not retrieve approval history');
}

/**
 * Get the last action for a draft of a specific type
 *
 * @param int    $draft_id    Draft ID
 * @param string $action_type Action type to filter by (optional)
 * @return array|false        Action row or false
 */
function get_last_approval_action($draft_id, $action_type = null)
{
	$sql = "SELECT a.*, u.real_name AS user_name
		FROM " . TB_PREF . "approval_actions a
		LEFT JOIN " . TB_PREF . "users u ON u.id = a.user_id
		WHERE a.draft_id = " . db_escape($draft_id);

	if ($action_type !== null)
		$sql .= " AND a.action_type = " . db_escape($action_type);

	$sql .= " ORDER BY a.action_date DESC, a.id DESC LIMIT 1";

	$result = db_query($sql, 'could not retrieve last approval action');

	return db_fetch($result);
}

/**
 * Get all actions performed by a specific user
 *
 * @param int   $user_id User ID
 * @param array $filters Optional: from_date, to_date, action_type
 * @return resource       DB result set
 */
function get_approval_actions_by_user($user_id, $filters = array())
{
	$sql = "SELECT a.*, d.trans_type, d.reserved_trans_no, d.summary,
			d.reference, w.name AS workflow_name
		FROM " . TB_PREF . "approval_actions a
		LEFT JOIN " . TB_PREF . "approval_drafts d ON d.id = a.draft_id
		LEFT JOIN " . TB_PREF . "approval_workflows w ON w.id = d.workflow_id
		WHERE a.user_id = " . db_escape($user_id);

	if (!empty($filters['from_date']))
		$sql .= " AND a.action_date >= " . db_escape($filters['from_date']);

	if (!empty($filters['to_date']))
		$sql .= " AND a.action_date <= " . db_escape($filters['to_date']);

	if (!empty($filters['action_type']))
		$sql .= " AND a.action_type = " . db_escape($filters['action_type']);

	$sql .= " ORDER BY a.action_date DESC";

	return db_query($sql, 'could not retrieve user approval actions');
}

// =====================================================
// Approval Delegations
// =====================================================

/**
 * Add a new delegation record
 *
 * @param int    $from_user_id User delegating approval authority
 * @param int    $to_user_id   User receiving delegation
 * @param int    $trans_type   Transaction type (null = all types)
 * @param string $from_date    Delegation start date (SQL format)
 * @param string $to_date      Delegation end date (SQL format, null = indefinite)
 * @param string $reason       Reason for delegation
 * @return int   Inserted delegation ID
 */
function add_approval_delegation($from_user_id, $to_user_id, $trans_type = null,
	$from_date = null, $to_date = null, $reason = null)
{
	if ($from_date === null)
		$from_date = date('Y-m-d');

	$sql = "INSERT INTO " . TB_PREF . "approval_delegations
		(from_user_id, to_user_id, trans_type, from_date, to_date, reason,
		 is_active, created_date)
		VALUES ("
		. db_escape($from_user_id) . ", "
		. db_escape($to_user_id) . ", "
		. db_escape($trans_type, true) . ", "
		. db_escape($from_date) . ", "
		. db_escape($to_date, true) . ", "
		. db_escape($reason, true) . ", "
		. "1, NOW())";

	db_query($sql, 'could not add approval delegation');

	return db_insert_id();
}

/**
 * Deactivate a delegation
 *
 * @param int $delegation_id Delegation record ID
 * @return void
 */
function deactivate_approval_delegation($delegation_id)
{
	$sql = "UPDATE " . TB_PREF . "approval_delegations
		SET is_active = 0
		WHERE id = " . db_escape($delegation_id);

	db_query($sql, 'could not deactivate approval delegation');
}

/**
 * Get active delegations TO a specific user
 *
 * @param int $to_user_id User ID receiving delegations
 * @param int $trans_type  Optional transaction type filter
 * @return resource         DB result set
 */
function get_active_delegations_to_user($to_user_id, $trans_type = null)
{
	$sql = "SELECT d.*, u.real_name AS from_user_name
		FROM " . TB_PREF . "approval_delegations d
		LEFT JOIN " . TB_PREF . "users u ON u.id = d.from_user_id
		WHERE d.to_user_id = " . db_escape($to_user_id) . "
		AND d.is_active = 1
		AND d.from_date <= CURDATE()
		AND (d.to_date IS NULL OR d.to_date >= CURDATE())";

	if ($trans_type !== null)
		$sql .= " AND (d.trans_type IS NULL OR d.trans_type = " . db_escape($trans_type) . ")";

	return db_query($sql, 'could not retrieve active delegations');
}

/**
 * Get active delegations FROM a specific user
 *
 * @param int $from_user_id User ID who delegated
 * @return resource          DB result set
 */
function get_active_delegations_from_user($from_user_id)
{
	$sql = "SELECT d.*, u.real_name AS to_user_name
		FROM " . TB_PREF . "approval_delegations d
		LEFT JOIN " . TB_PREF . "users u ON u.id = d.to_user_id
		WHERE d.from_user_id = " . db_escape($from_user_id) . "
		AND d.is_active = 1
		AND d.from_date <= CURDATE()
		AND (d.to_date IS NULL OR d.to_date >= CURDATE())";

	return db_query($sql, 'could not retrieve delegations from user');
}

// =====================================================
// Approval Notifications
// =====================================================

/**
 * Add a notification record
 *
 * @param int    $draft_id          Draft ID
 * @param int    $user_id           Target user ID
 * @param string $notification_type Type: pending, approved, rejected, escalated, delegated
 * @return int   Inserted notification ID
 */
function add_approval_notification($draft_id, $user_id, $notification_type)
{
	$sql = "INSERT INTO " . TB_PREF . "approval_notifications
		(draft_id, user_id, notification_type, is_read, is_sent, created_date)
		VALUES ("
		. db_escape($draft_id) . ", "
		. db_escape($user_id) . ", "
		. db_escape($notification_type) . ", "
		. "0, 0, NOW())";

	db_query($sql, 'could not add approval notification');

	return db_insert_id();
}

/**
 * Mark a notification as read
 *
 * @param int $notification_id Notification record ID
 * @return void
 */
function mark_approval_notification_read($notification_id)
{
	$sql = "UPDATE " . TB_PREF . "approval_notifications
		SET is_read = 1, read_date = NOW()
		WHERE id = " . db_escape($notification_id);

	db_query($sql, 'could not mark notification as read');
}

/**
 * Mark a notification as email-sent
 *
 * @param int $notification_id Notification record ID
 * @return void
 */
function mark_approval_notification_sent($notification_id)
{
	$sql = "UPDATE " . TB_PREF . "approval_notifications
		SET is_sent = 1
		WHERE id = " . db_escape($notification_id);

	db_query($sql, 'could not mark notification as sent');
}

/**
 * Get unread notifications for a user
 *
 * @param int $user_id User ID
 * @return resource     DB result set
 */
function get_unread_approval_notifications($user_id)
{
	$sql = "SELECT n.*, d.trans_type, d.reserved_trans_no, d.summary,
			d.reference, d.amount, w.name AS workflow_name
		FROM " . TB_PREF . "approval_notifications n
		LEFT JOIN " . TB_PREF . "approval_drafts d ON d.id = n.draft_id
		LEFT JOIN " . TB_PREF . "approval_workflows w ON w.id = d.workflow_id
		WHERE n.user_id = " . db_escape($user_id) . "
		AND n.is_read = 0
		ORDER BY n.created_date DESC";

	return db_query($sql, 'could not retrieve unread notifications');
}

/**
 * Count unread notifications for a user
 *
 * @param int $user_id User ID
 * @return int         Count of unread notifications
 */
function count_unread_approval_notifications($user_id)
{
	$sql = "SELECT COUNT(*) AS cnt
		FROM " . TB_PREF . "approval_notifications
		WHERE user_id = " . db_escape($user_id) . "
		AND is_read = 0";

	$result = db_query($sql, 'could not count unread notifications');
	$row = db_fetch_row($result);

	return (int)$row[0];
}

/**
 * Get unsent email notifications (for batch email sending)
 *
 * @param int $limit Maximum number to return
 * @return resource   DB result set
 */
function get_unsent_approval_notifications($limit = 50)
{
	$sql = "SELECT n.*, d.trans_type, d.reserved_trans_no, d.summary,
			d.reference, d.amount, d.status AS draft_status,
			w.name AS workflow_name,
			u.real_name AS user_name, u.email AS user_email
		FROM " . TB_PREF . "approval_notifications n
		LEFT JOIN " . TB_PREF . "approval_drafts d ON d.id = n.draft_id
		LEFT JOIN " . TB_PREF . "approval_workflows w ON w.id = d.workflow_id
		LEFT JOIN " . TB_PREF . "users u ON u.id = n.user_id
		WHERE n.is_sent = 0
		ORDER BY n.created_date ASC
		LIMIT " . (int)$limit;

	return db_query($sql, 'could not retrieve unsent notifications');
}

/**
 * Mark all notifications for a draft as read (used when draft is completed)
 *
 * @param int $draft_id Draft ID
 * @return void
 */
function mark_all_draft_notifications_read($draft_id)
{
	$sql = "UPDATE " . TB_PREF . "approval_notifications
		SET is_read = 1, read_date = NOW()
		WHERE draft_id = " . db_escape($draft_id) . "
		AND is_read = 0";

	db_query($sql, 'could not mark all draft notifications as read');
}
