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
	$roles = get_approver_roles_at_level($workflow_id, $level, $loc_code);

	if (empty($roles))
		return 0;

	$notification_count = 0;
	$notified_users = array();

	foreach ($roles as $role_id) {
		$users = get_users_with_role($role_id);

		while ($user = db_fetch($users)) {
			// Avoid duplicate notifications to the same user
			if (in_array($user['id'], $notified_users))
				continue;

			add_approval_notification($draft_id, $user['id'], $notification_type);
			$notified_users[] = $user['id'];
			$notification_count++;
		}
	}

	return $notification_count;
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
 * Reads unsent notification records and sends emails to the
 * target users. Marks notifications as sent after processing.
 *
 * @param int $batch_size Maximum notifications to process per call
 * @return int            Number of emails sent
 */
function process_approval_email_notifications($batch_size = 50)
{
	global $path_to_root, $SysPrefs;

	$result = get_unsent_approval_notifications($batch_size);
	$sent_count = 0;

	while ($notification = db_fetch($result)) {
		if (empty($notification['user_email'])) {
			mark_approval_notification_sent($notification['id']);
			continue;
		}

		$subject = build_approval_email_subject($notification);
		$body = build_approval_email_body($notification);

		if (send_approval_email(
			$notification['user_name'],
			$notification['user_email'],
			$subject,
			$body
		)) {
			$sent_count++;
		}

		mark_approval_notification_sent($notification['id']);
	}

	return $sent_count;
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

	return $mail->send();
}
