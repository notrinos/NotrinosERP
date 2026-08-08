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

	$result = db_query($sql, 'could not add approval notification');
	if ($result === false)
		return 0;

	$notification_id = (int)db_insert_id();
	return $notification_id > 0 ? $notification_id : 0;
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
 * Mark one pending notification as email-sent.
 *
 * The conditional predicate prevents a stale worker from treating an already
 * completed row as its own successful state transition.
 *
 * @param int $notification_id Notification record ID
 * @return bool True only when exactly one pending row becomes sent
 */
function mark_approval_notification_sent($notification_id)
{
	$notification_id = (int)$notification_id;
	if ($notification_id <= 0)
		return false;

	$sql = "UPDATE " . TB_PREF . "approval_notifications
		SET is_sent = 1
		WHERE id = " . db_escape($notification_id) . "
		AND is_sent = 0";

	$result = db_query($sql, 'could not mark notification as sent');
	if ($result === false)
		return false;

	return function_exists('db_num_affected_rows')
		&& (int)db_num_affected_rows() === 1;
}

/**
 * Generate a cryptographically strong notification lease token.
 *
 * @return string Lowercase 64-character token, or an empty string on failure
 */
function create_approval_notification_lease_token()
{
	$bytes = false;
	if (function_exists('random_bytes')) {
		try {
			$bytes = random_bytes(32);
		} catch (Exception $exception) {
			$bytes = false;
		}
	} elseif (function_exists('openssl_random_pseudo_bytes')) {
		$strong = false;
		$bytes = openssl_random_pseudo_bytes(32, $strong);
		if (!$strong)
			$bytes = false;
	}

	return is_string($bytes) && strlen($bytes) === 32
		? bin2hex($bytes) : '';
}

/**
 * Atomically claim the next due approval email notification.
 *
 * Claims are company-scoped through TB_PREF. Expired leases are eligible for
 * recovery. The update increments the cumulative attempt count before any
 * external delivery, and the unique token prevents another worker from using
 * the same claim concurrently.
 *
 * @param int $lease_seconds Bounded lease duration
 * @return array status=claimed|empty|storage_failed, token and optional row
 */
function claim_next_approval_email_notification($lease_seconds = 300)
{
	$lease_seconds = max(30, min(900, (int)$lease_seconds));
	$lease_token = create_approval_notification_lease_token();
	if ($lease_token === '')
		return array('status' => 'storage_failed', 'token' => '', 'row' => null);

	$sql = "UPDATE " . TB_PREF . "approval_notifications
		SET lease_token = " . db_escape($lease_token) . ",
			lease_expires_at = DATE_ADD(NOW(), INTERVAL " . $lease_seconds . " SECOND),
			delivery_attempts = delivery_attempts + 1,
			last_attempt_at = NOW()
		WHERE is_sent = 0
		AND dead_lettered_at IS NULL
		AND (next_attempt_at IS NULL OR next_attempt_at <= NOW())
		AND (lease_token IS NULL OR lease_expires_at IS NULL OR lease_expires_at <= NOW())
		ORDER BY created_date ASC, id ASC
		LIMIT 1";

	$result = db_query($sql, 'could not claim approval notification');
	if ($result === false || !function_exists('db_num_affected_rows'))
		return array('status' => 'storage_failed', 'token' => '', 'row' => null);
	if ((int)db_num_affected_rows() === 0)
		return array('status' => 'empty', 'token' => '', 'row' => null);
	if ((int)db_num_affected_rows() !== 1)
		return array('status' => 'storage_failed', 'token' => '', 'row' => null);

	$sql = "SELECT n.*, d.trans_type, d.reserved_trans_no, d.summary,
			d.reference, d.amount, d.status AS draft_status,
			w.name AS workflow_name,
			u.real_name AS user_name, u.email AS user_email
		FROM " . TB_PREF . "approval_notifications n
		LEFT JOIN " . TB_PREF . "approval_drafts d ON d.id = n.draft_id
		LEFT JOIN " . TB_PREF . "approval_workflows w ON w.id = d.workflow_id
		LEFT JOIN " . TB_PREF . "users u ON u.id = n.user_id
		WHERE n.lease_token = " . db_escape($lease_token) . "
		AND n.is_sent = 0
		AND n.dead_lettered_at IS NULL";

	$claimed = db_query($sql, 'could not retrieve claimed approval notification');
	$row = $claimed ? db_fetch($claimed) : false;
	if (!$row)
		return array('status' => 'storage_failed', 'token' => $lease_token, 'row' => null);

	return array('status' => 'claimed', 'token' => $lease_token, 'row' => $row);
}

/**
 * Complete one claimed notification after successful external delivery.
 *
 * @param int    $notification_id
 * @param string $lease_token
 * @return bool True only when the current lease owns the exact transition
 */
function complete_approval_notification_delivery($notification_id, $lease_token)
{
	$notification_id = (int)$notification_id;
	$lease_token = trim((string)$lease_token);
	if ($notification_id <= 0 || !preg_match('/^[a-f0-9]{64}$/', $lease_token))
		return false;

	$sql = "UPDATE " . TB_PREF . "approval_notifications
		SET is_sent = 1,
			sent_date = NOW(),
			lease_token = NULL,
			lease_expires_at = NULL,
			next_attempt_at = NULL,
			last_error_code = ''
		WHERE id = " . db_escape($notification_id) . "
		AND lease_token = " . db_escape($lease_token) . "
		AND is_sent = 0
		AND dead_lettered_at IS NULL";

	$result = db_query($sql, 'could not complete approval notification delivery');
	return $result !== false && function_exists('db_num_affected_rows')
		&& (int)db_num_affected_rows() === 1;
}

/**
 * Persist a bounded retry or terminal dead-letter outcome for one claim.
 *
 * @param int    $notification_id
 * @param string $lease_token
 * @param string $error_code PII-free allow-listed code
 * @param int    $delay_seconds Retry delay, ignored for dead-letter outcome
 * @param bool   $dead_letter
 * @return bool
 */
function fail_approval_notification_delivery($notification_id, $lease_token,
	$error_code, $delay_seconds, $dead_letter = false)
{
	$allowed_errors = array(
		'delivery_failed', 'missing_email', 'invalid_email', 'missing_sender', 'invalid_sender', 'invalid_bcc', 'transport_unavailable', 'transport_executable_unavailable', 'transport_encoding_unavailable', 'transport_arg_escape_unavailable', 'transport_forced_params_override', 'transport_args_incompatible', 'transport_smtp_config_invalid', 'transport_smtp_connect_failed', 'transport_smtp_start_failed', 'invalid_notification',
		'state_failed', 'operator_repair_required'
	);
	$notification_id = (int)$notification_id;
	$lease_token = trim((string)$lease_token);
	$error_code = (string)$error_code;
	$delay_seconds = max(60, min(86400, (int)$delay_seconds));
	if ($notification_id <= 0
		|| !preg_match('/^[a-f0-9]{64}$/', $lease_token)
		|| !in_array($error_code, $allowed_errors, true)
	) {
		return false;
	}

	if ($dead_letter) {
		$retry_sql = "next_attempt_at = NULL, dead_lettered_at = NOW()";
	} else {
		$retry_sql = "next_attempt_at = DATE_ADD(NOW(), INTERVAL "
			. $delay_seconds . " SECOND), dead_lettered_at = NULL";
	}

	$sql = "UPDATE " . TB_PREF . "approval_notifications
		SET " . $retry_sql . ",
			lease_token = NULL,
			lease_expires_at = NULL,
			last_error_code = " . db_escape($error_code) . "
		WHERE id = " . db_escape($notification_id) . "
		AND lease_token = " . db_escape($lease_token) . "
		AND is_sent = 0";

	$result = db_query($sql, 'could not record approval notification delivery failure');
	return $result !== false && function_exists('db_num_affected_rows')
		&& (int)db_num_affected_rows() === 1;
}

/**
 * Requeue one dead-lettered notification after authorized operator repair.
 *
 * Cumulative attempts remain intact. The attempt limit is extended by one
 * bounded five-attempt window and repair_count records the intervention.
 *
 * @param int $notification_id
 * @param int $operator_user_id Authorized operator user ID for durable audit
 * @return bool
 */
function repair_approval_notification_delivery($notification_id, $operator_user_id)
{
	$notification_id = (int)$notification_id;
	$operator_user_id = (int)$operator_user_id;
	if ($notification_id <= 0 || $operator_user_id <= 0)
		return false;

	$sql = "UPDATE " . TB_PREF . "approval_notifications
		SET attempt_limit = GREATEST(attempt_limit, delivery_attempts) + 5,
			repair_count = repair_count + 1,
			last_repaired_by = " . db_escape($operator_user_id) . ",
			last_repaired_at = NOW(),
			dead_lettered_at = NULL,
			next_attempt_at = NOW(),
			lease_token = NULL,
			lease_expires_at = NULL,
			last_error_code = ''
		WHERE id = " . db_escape($notification_id) . "
		AND is_sent = 0
		AND dead_lettered_at IS NOT NULL";

	$result = db_query($sql, 'could not repair approval notification delivery');
	return $result !== false && function_exists('db_num_affected_rows')
		&& (int)db_num_affected_rows() === 1;
}

/**
 * Return PII-free queue-state counts for authorized operators.
 *
 * @return array ready, leased, deferred, dead_lettered and sent counts
 */
function get_approval_notification_delivery_status_counts()
{
	$sql = "SELECT
		SUM(CASE WHEN is_sent = 0 AND dead_lettered_at IS NULL
			AND (next_attempt_at IS NULL OR next_attempt_at <= NOW())
			AND (lease_token IS NULL OR lease_expires_at IS NULL OR lease_expires_at <= NOW())
			THEN 1 ELSE 0 END) AS ready_count,
		SUM(CASE WHEN is_sent = 0 AND dead_lettered_at IS NULL
			AND lease_token IS NOT NULL AND lease_expires_at > NOW()
			THEN 1 ELSE 0 END) AS leased_count,
		SUM(CASE WHEN is_sent = 0 AND dead_lettered_at IS NULL
			AND next_attempt_at > NOW() THEN 1 ELSE 0 END) AS deferred_count,
		SUM(CASE WHEN is_sent = 0 AND dead_lettered_at IS NOT NULL
			THEN 1 ELSE 0 END) AS dead_lettered_count,
		SUM(CASE WHEN is_sent = 1 THEN 1 ELSE 0 END) AS sent_count
		FROM " . TB_PREF . "approval_notifications";
	$result = db_query($sql, 'could not summarize approval notification delivery state');
	$row = $result ? db_fetch($result) : false;
	return array(
		'ready' => $row ? (int)$row['ready_count'] : 0,
		'leased' => $row ? (int)$row['leased_count'] : 0,
		'deferred' => $row ? (int)$row['deferred_count'] : 0,
		'dead_lettered' => $row ? (int)$row['dead_lettered_count'] : 0,
		'sent' => $row ? (int)$row['sent_count'] : 0,
	);
}

/**
 * Get bounded PII-free dead-letter rows for authorized repair UI.
 *
 * @param int $limit
 * @return resource
 */
function get_dead_lettered_approval_notifications($limit = 50)
{
	$limit = max(1, min(100, (int)$limit));
	$sql = "SELECT id, notification_type, delivery_attempts, attempt_limit,
			repair_count, last_error_code, last_attempt_at, dead_lettered_at
		FROM " . TB_PREF . "approval_notifications
		WHERE is_sent = 0
		AND dead_lettered_at IS NOT NULL
		ORDER BY dead_lettered_at ASC, id ASC
		LIMIT " . $limit;
	return db_query($sql, 'could not retrieve dead-lettered approval notifications');
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
		AND n.dead_lettered_at IS NULL
		AND (n.next_attempt_at IS NULL OR n.next_attempt_at <= NOW())
		AND (n.lease_token IS NULL OR n.lease_expires_at IS NULL OR n.lease_expires_at <= NOW())
		ORDER BY n.created_date ASC, n.id ASC
		LIMIT " . max(0, (int)$limit);

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
