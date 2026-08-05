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
	Notification System

	Handles sending email notifications and creating in-app notification
	records for approval workflow events.
***********************************************************************/

include_once(dirname(__FILE__) . '/db/approval_history_db.inc');

/**
 * Send notifications to all approvers at a given level for a draft.
 *
 * Creates in-app notification records and optionally sends email
 * to users with the required role at the specified approval level.
 *
 * @param int    $draft_id        Draft ID
 * @param int    $workflow_id     Workflow ID
 * @param int    $level           Approval level to notify
 * @param string $notification_type Notification type: pending, approved, rejected, etc.
 * @param string $loc_code        Optional location filter
 * @return int   Number of notifications created
 */
function notify_approval_level_approvers($draft_id, $workflow_id, $level,
	$notification_type = 'pending', $loc_code = null)
{
	$result = create_approval_level_notifications(
		$draft_id,
		$workflow_id,
		$level,
		$notification_type,
		$loc_code
	);

	return isset($result['count']) ? (int)$result['count'] : 0;
}

/**
 * Persist notifications for all distinct active users at an approval level.
 *
 * Recipient discovery intentionally retains the existing workflow role,
 * location, active-user, and duplicate-user rules. The detailed result lets
 * the transaction owner distinguish an empty recipient set from a persistence
 * failure without changing the legacy integer-returning wrapper above.
 *
 * This helper does not begin, commit, or roll back a transaction. Callers that
 * require atomic draft/notification persistence must own that boundary.
 *
 * @param int    $draft_id          Draft ID
 * @param int    $workflow_id       Workflow ID
 * @param int    $level             Approval level to notify
 * @param string $notification_type Notification type
 * @param string $loc_code          Optional location filter
 * @return array ['status' => created|no_recipients|storage_failed, 'count' => int]
 */
function create_approval_level_notifications($draft_id, $workflow_id, $level,
	$notification_type = 'pending', $loc_code = null)
{
	$roles = get_approver_roles_at_level($workflow_id, $level, $loc_code);

	if (empty($roles))
		return array('status' => 'no_recipients', 'count' => 0);

	$notification_count = 0;
	$notified_users = array();

	foreach ($roles as $role_id) {
		$users = get_users_with_role($role_id);

		while ($user = db_fetch($users)) {
			$user_id = isset($user['id']) ? (int)$user['id'] : 0;
			if ($user_id <= 0 || isset($notified_users[$user_id]))
				continue;

			$notification_id = add_approval_notification(
				$draft_id,
				$user_id,
				$notification_type
			);
			if ((int)$notification_id <= 0) {
				return array(
					'status' => 'storage_failed',
					'count' => $notification_count,
				);
			}

			$notified_users[$user_id] = true;
			$notification_count++;
		}
	}

	if ($notification_count === 0)
		return array('status' => 'no_recipients', 'count' => 0);

	return array('status' => 'created', 'count' => $notification_count);
}

/**
 * Notify the submitter about the outcome of their draft.
 *
 * @param int    $draft_id         Draft ID
 * @param int    $submitted_by     Submitter user ID
 * @param string $notification_type Notification type: approved, rejected, etc.
 * @return void
 */
function notify_approval_submitter($draft_id, $submitted_by, $notification_type)
{
	add_approval_notification($draft_id, $submitted_by, $notification_type);
}

/**
 * Get all users that have a specific security role.
 *
 * @param int $role_id Security role ID
 * @return resource     DB result set of users
 */
function get_users_with_role($role_id)
{
	$sql = "SELECT id, login_id, real_name, email
		FROM " . TB_PREF . "users
		WHERE role_id = " . db_escape($role_id) . "
		AND inactive = 0";

	return db_query($sql, 'could not retrieve users with role');
}

/**
 * Process and send pending email notifications.
 *
 * Backward-compatible wrapper returning only the confirmed sent count. Failed,
 * blank-recipient, invalid-row, or sent-state-update cases remain unsent for a
 * later retry or operator repair.
 *
 * @param int $batch_size Maximum notifications to process per call
 * @return int            Number of emails sent and durably marked sent
 */
function process_approval_email_notifications($batch_size = 50)
{
	$summary = process_approval_email_notifications_detailed($batch_size);
	return (int)$summary['sent'];
}

/**
 * Process pending approval emails with detailed, PII-free outcomes.
 *
 * Email delivery is external and therefore is not wrapped in a database
 * transaction. Each row is first atomically claimed with a bounded lease and
 * cumulative attempt metadata. A row is marked sent only after the sender
 * reports success and the lease-owning database update confirms exactly one
 * pending row. Failures receive deterministic capped backoff and eventually
 * dead-letter for authorized repair. A worker crash after external delivery but
 * before state confirmation can still duplicate delivery after lease expiry;
 * the lease suppresses concurrent sends but is not an exactly-once outbox.
 *
 * @param int $batch_size Maximum notifications to process per call
 * @return array          PII-free claim, delivery, retry, dead-letter and
 *                        state-transition counts
 */
function process_approval_email_notifications_detailed($batch_size = 50)
{
	$summary = array(
		'selected' => 0,
		'claimed' => 0,
		'sent' => 0,
		'delivery_failed' => 0,
		'missing_email' => 0,
		'invalid_notification' => 0,
		'state_failed' => 0,
		'retry_scheduled' => 0,
		'dead_lettered' => 0,
		'claim_failed' => 0,
	);

	$batch_size = max(0, min(500, (int)$batch_size));
	if ($batch_size === 0)
		return $summary;

	for ($processed = 0; $processed < $batch_size; $processed++) {
		$claim = claim_next_approval_email_notification(300);
		if (!isset($claim['status']) || $claim['status'] === 'storage_failed') {
			$summary['claim_failed']++;
			break;
		}
		if ($claim['status'] === 'empty')
			break;

		$notification = isset($claim['row']) && is_array($claim['row'])
			? $claim['row'] : array();
		$lease_token = isset($claim['token']) ? (string)$claim['token'] : '';
		$summary['selected']++;
		$summary['claimed']++;

		$notification_id = isset($notification['id'])
			? (int)$notification['id'] : 0;
		$attempts = isset($notification['delivery_attempts'])
			? max(1, (int)$notification['delivery_attempts']) : 1;
		$attempt_limit = isset($notification['attempt_limit'])
			? max(1, (int)$notification['attempt_limit']) : 5;
		$dead_letter = $attempts >= $attempt_limit;
		$retry_delay = approval_notification_retry_delay_seconds($attempts);

		if ($notification_id <= 0
			|| !preg_match('/^[a-f0-9]{64}$/', $lease_token)
		) {
			$summary['invalid_notification']++;
			$summary['state_failed']++;
			continue;
		}

		$recipient_email = isset($notification['user_email'])
			? trim((string)$notification['user_email']) : '';
		if ($recipient_email === '') {
			$summary['missing_email']++;
			if (!fail_approval_notification_delivery(
				$notification_id,
				$lease_token,
				'missing_email',
				$retry_delay,
				$dead_letter
			)) {
				$summary['state_failed']++;
			} elseif ($dead_letter) {
				$summary['dead_lettered']++;
			} else {
				$summary['retry_scheduled']++;
			}
			continue;
		}

		$subject = build_approval_email_subject($notification);
		$body = build_approval_email_body($notification);
		$delivered = send_approval_email(
			isset($notification['user_name'])
				? $notification['user_name'] : '',
			$recipient_email,
			$subject,
			$body
		);
		if (!$delivered) {
			$summary['delivery_failed']++;
			if (!fail_approval_notification_delivery(
				$notification_id,
				$lease_token,
				'delivery_failed',
				$retry_delay,
				$dead_letter
			)) {
				$summary['state_failed']++;
			} elseif ($dead_letter) {
				$summary['dead_lettered']++;
			} else {
				$summary['retry_scheduled']++;
			}
			continue;
		}

		if (!complete_approval_notification_delivery(
			$notification_id,
			$lease_token
		)) {
			$summary['state_failed']++;
			continue;
		}

		$summary['sent']++;
	}

	return $summary;
}

/**
 * Calculate deterministic bounded retry delay from cumulative attempt count.
 *
 * @param int $attempts
 * @return int Delay in seconds, capped at one day
 */
function approval_notification_retry_delay_seconds($attempts)
{
	$delays = array(60, 300, 1800, 7200, 86400);
	$index = max(0, min(count($delays) - 1, (int)$attempts - 1));
	return $delays[$index];
}

/**
 * Build email subject line for an approval notification.
 *
 * @param array $notification Notification record with joined draft/workflow data
 * @return string             Email subject
 */
function build_approval_email_subject($notification)
{
	global $systypes_array;

	$type_name = isset($systypes_array[$notification['trans_type']])
		? $systypes_array[$notification['trans_type']]
		: $notification['workflow_name'];

	switch ($notification['notification_type']) {
		case 'pending':
			return sprintf(_('[Approval Required] %s %s'), $type_name, $notification['reference']);
		case 'approved':
			return sprintf(_('[Approved] %s %s'), $type_name, $notification['reference']);
		case 'rejected':
			return sprintf(_('[Rejected] %s %s'), $type_name, $notification['reference']);
		case 'escalated':
			return sprintf(_('[Escalated] %s %s'), $type_name, $notification['reference']);
		case 'delegated':
			return sprintf(_('[Delegated] %s %s'), $type_name, $notification['reference']);
		default:
			return sprintf(_('[Approval] %s %s'), $type_name, $notification['reference']);
	}
}

/**
 * Build email body for an approval notification.
 *
 * @param array $notification Notification record with joined draft/workflow data
 * @return string             Email body text
 */
function build_approval_email_body($notification)
{
	global $systypes_array;

	$type_name = isset($systypes_array[$notification['trans_type']])
		? $systypes_array[$notification['trans_type']]
		: $notification['workflow_name'];

	$body = sprintf(_('Dear %s,'), $notification['user_name']) . "\n\n";

	switch ($notification['notification_type']) {
		case 'pending':
			$body .= sprintf(
				_('A %s (%s) requires your approval.'),
				$type_name,
				$notification['reference']
			);
			break;
		case 'approved':
			$body .= sprintf(
				_('Your %s (%s) has been approved.'),
				$type_name,
				$notification['reference']
			);
			break;
		case 'rejected':
			$body .= sprintf(
				_('Your %s (%s) has been rejected.'),
				$type_name,
				$notification['reference']
			);
			break;
		case 'escalated':
			$body .= sprintf(
				_('A %s (%s) has been escalated to your level for approval.'),
				$type_name,
				$notification['reference']
			);
			break;
		case 'delegated':
			$body .= sprintf(
				_('A %s (%s) has been delegated to you for approval.'),
				$type_name,
				$notification['reference']
			);
			break;
	}

	$body .= "\n\n";

	if ($notification['summary'])
		$body .= _('Summary') . ': ' . $notification['summary'] . "\n";

	if ($notification['amount'] > 0)
		$body .= _('Amount') . ': ' . number_format2($notification['amount'], 2) . "\n";

	$body .= "\n" . _('Please log in to the system to take action.') . "\n";

	return $body;
}

/**
 * Send an approval notification email.
 *
 * @param string $recipient_name  Recipient name
 * @param string $recipient_email Recipient email address
 * @param string $subject         Email subject
 * @param string $body            Email body
 * @return bool                   True on success
 */
function send_approval_email($recipient_name, $recipient_email, $subject, $body)
{
	global $path_to_root;

	require_once($path_to_root . '/reporting/includes/class.mail.inc');

	$company_name = get_company_pref('coy_name');
	$company_email = get_company_pref('email');

	if (empty($company_email))
		$company_email = 'noreply@' . (isset($_SERVER['HTTP_HOST']) ? $_SERVER['HTTP_HOST'] : 'localhost');

	$mail = new email($company_name, $company_email);
	$mail->to($recipient_name, $recipient_email);
	$mail->subject($subject);
	$mail->text($body);

	return (bool)$mail->send();
}
