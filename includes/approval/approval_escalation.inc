<?php
/**********************************************************************
	NotrinosERP-1.0 Approval Workflow System
	Phase 6: Escalation Timer Logic

	Provides automatic escalation for overdue approval drafts.
	Designed to be called from a cron job or manual admin trigger.

	Escalation rules are defined per approval level:
	- escalation_days: Number of days before escalation triggers
	- escalation_to_level: Target level to escalate to (null = next level)

	PHP 5.6+ compatible.
***********************************************************************/

include_once(dirname(__FILE__) . '/db/approval_db.inc');
include_once(dirname(__FILE__) . '/db/approval_rules_db.inc');
include_once(dirname(__FILE__) . '/db/approval_history_db.inc');
include_once(dirname(__FILE__) . '/approval_notify.inc');

/**
 * Find all pending drafts that are overdue for escalation.
 *
 * Queries approval_drafts joined with approval_levels to find drafts
 * where the current level has an escalation_days > 0 and the draft
 * has been at this level longer than the escalation threshold.
 *
 * @return resource DB result set of overdue draft records
 */
function get_overdue_approval_drafts()
{
	$sql = "SELECT d.id AS draft_id, d.workflow_id, d.trans_type,
			d.current_level, d.submitted_date, d.reference, d.summary,
			d.amount, d.loc_code, d.submitted_by,
			l.escalation_days, l.escalation_to_level,
			w.name AS workflow_name,
			DATEDIFF(NOW(), COALESCE(
				(SELECT MAX(a.action_date) FROM " . TB_PREF . "approval_actions a
				 WHERE a.draft_id = d.id AND a.action_type IN ('submit', 'approve', 'escalate')
				 AND a.level = d.current_level),
				d.submitted_date
			)) AS days_at_level
		FROM " . TB_PREF . "approval_drafts d
		INNER JOIN " . TB_PREF . "approval_workflows w ON w.id = d.workflow_id
		INNER JOIN " . TB_PREF . "approval_levels l
			ON l.workflow_id = d.workflow_id AND l.level = d.current_level AND l.is_active = 1
		WHERE d.status = " . APPROVAL_STATUS_PENDING . "
		AND l.escalation_days > 0
		HAVING days_at_level >= l.escalation_days
		ORDER BY days_at_level DESC";

	return db_query($sql, 'could not retrieve overdue approval drafts');
}

/**
 * Escalate a single overdue draft to the configured escalation level.
 *
 * If escalation_to_level is set, the draft moves to that level.
 * If null, the draft moves to the next sequential level.
 * If already at the max level, the draft stays but a reminder notification is sent.
 *
 * @param array $overdue_draft Overdue draft row from get_overdue_approval_drafts()
 * @return array ['status' => string, 'message' => string, 'new_level' => int]
 */
function escalate_overdue_draft($overdue_draft)
{
	$draft_id = (int)$overdue_draft['draft_id'];
	$workflow_id = (int)$overdue_draft['workflow_id'];
	$current_level = (int)$overdue_draft['current_level'];
	$escalation_to = $overdue_draft['escalation_to_level'];
	$days_overdue = (int)$overdue_draft['days_at_level'];

	// Check if this draft was already escalated recently (prevent double-escalation)
	$last_escalation = get_last_approval_action($draft_id, 'escalate');
	if ($last_escalation) {
		$last_escalation_date = strtotime($last_escalation['action_date']);
		$hours_since = (time() - $last_escalation_date) / 3600;
		// Don't re-escalate within 24 hours
		if ($hours_since < 24) {
			return array(
				'status' => 'skipped',
				'message' => _('Already escalated recently.'),
				'new_level' => $current_level
			);
		}
	}

	$max_level = get_max_approval_level($workflow_id);

	// Determine target level
	if ($escalation_to !== null && (int)$escalation_to > 0) {
		$target_level = (int)$escalation_to;
	} else {
		$target_level = $current_level + 1;
	}

	// Clamp to max level
	if ($target_level > $max_level) {
		$target_level = $max_level;
	}

	begin_transaction();

	$comment = sprintf(
		_('Auto-escalated from level %d after %d day(s) overdue.'),
		$current_level, $days_overdue
	);

	// Record escalation action (user_id = 0 for system)
	add_approval_action($draft_id, 'escalate', 0, $current_level, $comment);

	if ($target_level !== $current_level) {
		// Move to escalation target level
		update_approval_draft_level($draft_id, $target_level);

		// Notify approvers at the new level
		notify_approval_level_approvers(
			$draft_id, $workflow_id, $target_level,
			'escalated', $overdue_draft['loc_code']
		);

		// Notify the original submitter
		notify_approval_submitter($draft_id, $overdue_draft['submitted_by'], 'escalated');

		commit_transaction();

		return array(
			'status' => 'escalated',
			'message' => sprintf(
				_('Draft #%d escalated from level %d to level %d after %d day(s).'),
				$draft_id, $current_level, $target_level, $days_overdue
			),
			'new_level' => $target_level
		);
	} else {
		// Already at max level — send reminder notification
		notify_approval_level_approvers(
			$draft_id, $workflow_id, $current_level,
			'escalated', $overdue_draft['loc_code']
		);

		commit_transaction();

		return array(
			'status' => 'reminded',
			'message' => sprintf(
				_('Draft #%d is at max level %d. Reminder sent after %d day(s).'),
				$draft_id, $current_level, $days_overdue
			),
			'new_level' => $current_level
		);
	}
}

/**
 * Process all overdue approval drafts for escalation.
 *
 * Main entry point for cron or manual escalation processing.
 *
 * @return array Summary: ['processed' => int, 'escalated' => int, 'reminded' => int, 'skipped' => int, 'details' => array]
 */
function process_approval_escalations()
{
	$result = get_overdue_approval_drafts();
	$summary = array(
		'processed' => 0,
		'escalated' => 0,
		'reminded'  => 0,
		'skipped'   => 0,
		'details'   => array()
	);

	while ($overdue = db_fetch($result)) {
		$summary['processed']++;

		$escalation_result = escalate_overdue_draft($overdue);
		$summary['details'][] = $escalation_result;

		switch ($escalation_result['status']) {
			case 'escalated':
				$summary['escalated']++;
				break;
			case 'reminded':
				$summary['reminded']++;
				break;
			case 'skipped':
				$summary['skipped']++;
				break;
		}
	}

	return $summary;
}

/**
 * Get escalation status for a specific draft.
 *
 * Returns information about whether the draft is approaching or past
 * the escalation deadline at its current level.
 *
 * @param int $draft_id Draft ID
 * @return array ['status' => string, 'has_escalation' => bool, 'escalation_days' => int,
 *               'days_at_level' => int, 'days_remaining' => int,
 *               'is_overdue' => bool, 'escalation_to_level' => int|null]
 *               status: 'ok', 'warning', 'overdue', 'no_escalation'
 */
function get_draft_escalation_status($draft_id)
{
	$draft = get_approval_draft($draft_id);

	if (!$draft || (int)$draft['status'] !== APPROVAL_STATUS_PENDING) {
		return array(
			'status' => 'no_escalation',
			'has_escalation' => false,
			'escalation_days' => 0,
			'days_at_level' => 0,
			'days_remaining' => 0,
			'is_overdue' => false,
			'escalation_to_level' => null
		);
	}

	$level = get_approval_level_by_workflow_level($draft['workflow_id'], (int)$draft['current_level']);

	if (!$level || (int)$level['escalation_days'] <= 0) {
		return array(
			'status' => 'no_escalation',
			'has_escalation' => false,
			'escalation_days' => 0,
			'days_at_level' => 0,
			'days_remaining' => 0,
			'is_overdue' => false,
			'escalation_to_level' => null
		);
	}

	// Calculate days at current level
	$sql = "SELECT DATEDIFF(NOW(), COALESCE(
			(SELECT MAX(a.action_date) FROM " . TB_PREF . "approval_actions a
			 WHERE a.draft_id = " . db_escape($draft_id) . "
			 AND a.action_type IN ('submit', 'approve', 'escalate')
			 AND a.level = " . db_escape($draft['current_level']) . "),
			" . db_escape($draft['submitted_date']) . "
		)) AS days_at_level";

	$result = db_query($sql, 'could not calculate days at level');
	$row = db_fetch($result);
	$days_at_level = (int)$row['days_at_level'];

	$escalation_days = (int)$level['escalation_days'];
	$days_remaining = max(0, $escalation_days - $days_at_level);
	$is_overdue = ($days_at_level >= $escalation_days);

	// Determine status label
	if ($is_overdue) {
		$status = 'overdue';
	} elseif ($days_remaining <= 1) {
		$status = 'warning';
	} else {
		$status = 'ok';
	}

	return array(
		'status' => $status,
		'has_escalation' => true,
		'escalation_days' => $escalation_days,
		'days_at_level' => $days_at_level,
		'days_remaining' => $days_remaining,
		'is_overdue' => $is_overdue,
		'escalation_to_level' => $level['escalation_to_level']
	);
}

/**
 * Get a summary of all pending drafts with escalation risk.
 *
 * Returns drafts sorted by urgency (overdue first, then by days remaining).
 *
 * @return array Array of draft summaries with escalation info
 */
function get_escalation_risk_summary()
{
	$sql = "SELECT d.id AS draft_id, d.workflow_id, d.trans_type,
			d.current_level, d.reference, d.summary, d.amount,
			d.submitted_date, d.submitted_by,
			w.name AS workflow_name,
			l.escalation_days, l.escalation_to_level,
			u.real_name AS submitter_name,
			DATEDIFF(NOW(), COALESCE(
				(SELECT MAX(a.action_date) FROM " . TB_PREF . "approval_actions a
				 WHERE a.draft_id = d.id AND a.action_type IN ('submit', 'approve', 'escalate')
				 AND a.level = d.current_level),
				d.submitted_date
			)) AS days_at_level
		FROM " . TB_PREF . "approval_drafts d
		INNER JOIN " . TB_PREF . "approval_workflows w ON w.id = d.workflow_id
		INNER JOIN " . TB_PREF . "approval_levels l
			ON l.workflow_id = d.workflow_id AND l.level = d.current_level AND l.is_active = 1
		LEFT JOIN " . TB_PREF . "users u ON u.id = d.submitted_by
		WHERE d.status = " . APPROVAL_STATUS_PENDING . "
		AND l.escalation_days > 0
		ORDER BY (DATEDIFF(NOW(), COALESCE(
				(SELECT MAX(a2.action_date) FROM " . TB_PREF . "approval_actions a2
				 WHERE a2.draft_id = d.id AND a2.action_type IN ('submit', 'approve', 'escalate')
				 AND a2.level = d.current_level),
				d.submitted_date
			)) - l.escalation_days) DESC";

	$result = db_query($sql, 'could not get escalation risk summary');

	$items = array();
	while ($row = db_fetch($result)) {
		$row['days_remaining'] = max(0, (int)$row['escalation_days'] - (int)$row['days_at_level']);
		$row['is_overdue'] = ((int)$row['days_at_level'] >= (int)$row['escalation_days']);
		$items[] = $row;
	}

	return $items;
}
