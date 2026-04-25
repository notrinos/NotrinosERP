<?php
/**********************************************************************
	NotrinosERP-1.0 Approval Workflow System
	Phase 6: Delegation Workflow Logic

	Provides enhanced delegation workflows including:
	- Inline delegation from dashboard (delegate a specific draft)
	- Delegation chain resolution (A delegates to B, B delegates to C)
	- Delegation eligibility checks
	- Active delegation listing for current user

	PHP 5.6+ compatible.
***********************************************************************/

include_once(dirname(__FILE__) . '/db/approval_history_db.inc');
include_once(dirname(__FILE__) . '/db/approval_rules_db.inc');

/**
 * Check if a user is eligible to be a delegate for a specific draft.
 *
 * A user is eligible if:
 * - They are not the original submitter (unless self-approve is allowed)
 * - They have not already approved this draft at the current level
 * - They are an active user
 * - No circular delegation exists
 *
 * @param int   $draft_id     Draft ID
 * @param int   $to_user_id   Target delegate user ID
 * @param int   $from_user_id Delegating user ID
 * @return array ['eligible' => bool, 'reason' => string]
 */
function check_delegation_eligibility($draft_id, $to_user_id, $from_user_id)
{
	$draft = get_approval_draft($draft_id);

	if (!$draft) {
		return array('eligible' => false, 'reason' => _('Draft not found.'));
	}

	if ((int)$draft['status'] !== APPROVAL_STATUS_PENDING) {
		return array('eligible' => false, 'reason' => _('Draft is not pending approval.'));
	}

	// Cannot delegate to self
	if ((int)$to_user_id === (int)$from_user_id) {
		return array('eligible' => false, 'reason' => _('Cannot delegate to yourself.'));
	}

	// Cannot delegate to the original submitter unless self-approve is allowed
	if (!$draft['allow_self_approve'] && (int)$to_user_id === (int)$draft['submitted_by']) {
		return array('eligible' => false, 'reason' => _('Cannot delegate to the original submitter.'));
	}

	// Check the target user is active
	$sql = "SELECT id, inactive FROM " . TB_PREF . "users WHERE id = " . db_escape($to_user_id);
	$result = db_query($sql, 'could not check delegate user');
	$user = db_fetch($result);

	if (!$user || $user['inactive']) {
		return array('eligible' => false, 'reason' => _('Target user is not active.'));
	}

	// Check the target user hasn't already approved at this level
	if (has_user_approved_draft($draft_id, $to_user_id, (int)$draft['current_level'])) {
		return array('eligible' => false, 'reason' => _('Target user has already approved at the current level.'));
	}

	// Check for circular delegation chain
	if (has_circular_delegation($from_user_id, $to_user_id, $draft['trans_type'])) {
		return array('eligible' => false, 'reason' => _('Circular delegation detected.'));
	}

	return array('eligible' => true, 'reason' => '');
}

/**
 * Detect circular delegation chains.
 *
 * Walks the active delegation chain starting from $to_user_id to see
 * if it eventually leads back to $from_user_id.
 *
 * @param int      $from_user_id Original delegating user
 * @param int      $to_user_id   User being delegated to
 * @param int|null $trans_type    Transaction type (null checks all)
 * @param int      $max_depth    Maximum chain depth to prevent infinite loops
 * @return bool                   True if circular delegation detected
 */
function has_circular_delegation($from_user_id, $to_user_id, $trans_type = null, $max_depth = 10)
{
	$visited = array((int)$from_user_id);
	$current = (int)$to_user_id;

	for ($depth = 0; $depth < $max_depth; $depth++) {
		if (in_array($current, $visited)) {
			return true;
		}

		$visited[] = $current;

		// Check where $current delegates to
		$delegations = get_active_delegations_from_user($current);
		$found_next = false;

		while ($delegation = db_fetch($delegations)) {
			// Filter by trans_type if specified
			if ($trans_type !== null && $delegation['trans_type'] !== null
				&& (int)$delegation['trans_type'] !== (int)$trans_type) {
				continue;
			}
			$current = (int)$delegation['to_user_id'];
			$found_next = true;
			break;
		}

		if (!$found_next) {
			return false;
		}
	}

	// If we exceed max_depth, treat as circular to be safe
	return true;
}

/**
 * Resolve the final delegate in a delegation chain.
 *
 * If user A delegates to B, and B delegates to C, this function
 * returns C when called with user A.
 *
 * @param int      $user_id    Starting user ID
 * @param int|null $trans_type Transaction type filter
 * @param int      $max_depth  Maximum chain depth
 * @return int                  Final delegate user ID (or original if no delegation)
 */
function resolve_delegation_chain($user_id, $trans_type = null, $max_depth = 10)
{
	$current = (int)$user_id;
	$visited = array();

	for ($depth = 0; $depth < $max_depth; $depth++) {
		if (in_array($current, $visited)) {
			// Circular — return the last valid delegate
			return $current;
		}
		$visited[] = $current;

		$delegations = get_active_delegations_from_user($current);
		$found_next = false;

		while ($delegation = db_fetch($delegations)) {
			if ($trans_type !== null && $delegation['trans_type'] !== null
				&& (int)$delegation['trans_type'] !== (int)$trans_type) {
				continue;
			}
			$current = (int)$delegation['to_user_id'];
			$found_next = true;
			break;
		}

		if (!$found_next) {
			break;
		}
	}

	return $current;
}

/**
 * Get all users eligible to receive delegation for a specific draft.
 *
 * Returns active users who are not the submitter (unless self-approve allowed)
 * and have not already approved at the current level.
 *
 * @param int $draft_id Draft ID
 * @return array        Array of user_id => display_name
 */
function get_eligible_delegates_for_draft($draft_id)
{
	$draft = get_approval_draft($draft_id);

	if (!$draft) {
		return array();
	}

	$current_user_id = $_SESSION['wa_current_user']->user;

	$sql = "SELECT id, real_name, login_id FROM " . TB_PREF . "users
		WHERE inactive = 0
		AND id != " . db_escape($current_user_id);

	// Exclude original submitter if self-approve is not allowed
	if (!$draft['allow_self_approve']) {
		$sql .= " AND id != " . db_escape($draft['submitted_by']);
	}

	$sql .= " ORDER BY real_name";

	$result = db_query($sql, 'could not get eligible delegates');

	$users = array();
	while ($row = db_fetch($result)) {
		// Skip users who already approved at this level
		if (has_user_approved_draft($draft_id, $row['id'], (int)$draft['current_level'])) {
			continue;
		}
		$users[$row['id']] = $row['real_name'] . ' (' . $row['login_id'] . ')';
	}

	return $users;
}

/**
 * Perform inline delegation of a draft to another user.
 *
 * Creates audit trail entry and notification to the delegate.
 *
 * @param int    $draft_id     Draft ID
 * @param int    $to_user_id   Target delegate user ID
 * @param string $reason       Delegation reason
 * @return array ['status' => string, 'message' => string]
 */
function perform_draft_delegation($draft_id, $to_user_id, $reason = '')
{
	$from_user_id = $_SESSION['wa_current_user']->user;

	$eligibility = check_delegation_eligibility($draft_id, $to_user_id, $from_user_id);

	if (!$eligibility['eligible']) {
		return array('status' => 'error', 'message' => $eligibility['reason']);
	}

	$draft = get_approval_draft($draft_id);

	// Get delegate user name for the audit comment
	$sql = "SELECT real_name FROM " . TB_PREF . "users WHERE id = " . db_escape($to_user_id);
	$result = db_query($sql, 'could not get delegate name');
	$delegate_user = db_fetch($result);
	$delegate_name = $delegate_user ? $delegate_user['real_name'] : '';

	begin_transaction();

	// Record delegation action in audit trail
	$comment = sprintf(_('Delegated to %s.'), $delegate_name);
	if (!empty($reason)) {
		$comment .= ' ' . _('Reason') . ': ' . $reason;
	}

	add_approval_action(
		$draft_id, 'delegate', $from_user_id,
		(int)$draft['current_level'], $comment
	);

	// Create notification for the delegate
	add_approval_notification($draft_id, $to_user_id, 'delegated');

	commit_transaction();

	return array(
		'status' => 'delegated',
		'message' => sprintf(_('Approval has been delegated to %s.'), $delegate_name)
	);
}

/**
 * Get delegation summary for a user.
 *
 * Returns count of:
 * - Active delegations FROM the user (user delegated their approvals)
 * - Active delegations TO the user (user received delegated approvals)
 *
 * @param int $user_id User ID
 * @return array ['delegated_from' => int, 'delegated_to' => int]
 */
function get_delegation_summary($user_id)
{
	$sql_from = "SELECT COUNT(*) AS cnt FROM " . TB_PREF . "approval_delegations
		WHERE from_user_id = " . db_escape($user_id) . "
		AND is_active = 1
		AND from_date <= CURDATE()
		AND (to_date IS NULL OR to_date >= CURDATE())";

	$result = db_query($sql_from, 'could not count delegations from user');
	$row = db_fetch($result);
	$from_count = (int)$row['cnt'];

	$sql_to = "SELECT COUNT(*) AS cnt FROM " . TB_PREF . "approval_delegations
		WHERE to_user_id = " . db_escape($user_id) . "
		AND is_active = 1
		AND from_date <= CURDATE()
		AND (to_date IS NULL OR to_date >= CURDATE())";

	$result = db_query($sql_to, 'could not count delegations to user');
	$row = db_fetch($result);
	$to_count = (int)$row['cnt'];

	return array('delegated_from' => $from_count, 'delegated_to' => $to_count);
}

/**
 * Get delegation history for a specific draft.
 *
 * Queries the approval_actions table for 'delegate' actions
 * on this draft and returns user name details.
 *
 * @param int $draft_id Draft ID
 * @return array        List of delegation records with from_name, to_name, reason, delegated_date
 */
function get_draft_delegation_history($draft_id)
{
	$sql = "SELECT a.action_date AS delegated_date, a.comments AS reason,
		u_from.real_name AS from_name
		FROM " . TB_PREF . "approval_actions a
		LEFT JOIN " . TB_PREF . "users u_from ON u_from.id = a.user_id
		WHERE a.draft_id = " . db_escape($draft_id) . "
		AND a.action_type = 'delegate'
		ORDER BY a.action_date ASC";

	$result = db_query($sql, 'could not get draft delegation history');

	$delegations = array();
	while ($row = db_fetch($result)) {
		// Parse the delegate-to name from the comment (format: "Delegated to <name>. ...")
		$to_name = '';
		if (preg_match('/Delegated to ([^.]+)/', $row['reason'], $matches)) {
			$to_name = trim($matches[1]);
		}
		// Strip the standard prefix from the reason to show only user-supplied reason
		$reason = $row['reason'];
		if (preg_match('/Reason:\s*(.+)$/s', $row['reason'], $reason_matches)) {
			$reason = trim($reason_matches[1]);
		} else {
			$reason = '';
		}

		$delegations[] = array(
			'from_name' => $row['from_name'] ? $row['from_name'] : _('Unknown'),
			'to_name' => $to_name ? $to_name : _('Unknown'),
			'reason' => $reason,
			'delegated_date' => $row['delegated_date'],
		);
	}

	return $delegations;
}
