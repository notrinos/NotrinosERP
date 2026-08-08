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
		'invalid_email' => 0,
		'missing_sender' => 0,
		'invalid_sender' => 0,
		'invalid_bcc' => 0,
		'transport_unavailable' => 0,
		'transport_executable_unavailable' => 0,
		'transport_encoding_unavailable' => 0,
		'transport_arg_escape_unavailable' => 0,
		'transport_forced_params_override' => 0,
		'transport_args_incompatible' => 0,
		'transport_smtp_config_invalid' => 0,
		'transport_smtp_connect_failed' => 0,
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
		$email_status = approval_notification_email_address_status($recipient_email);
		if ($email_status !== 'valid') {
			$summary[$email_status]++;
			if (!fail_approval_notification_delivery(
				$notification_id,
				$lease_token,
				$email_status,
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

		$sender_email = trim((string)get_company_pref('email'));
		$sender_status = approval_notification_sender_address_status($sender_email);
		if ($sender_status !== 'valid') {
			$summary[$sender_status]++;
			if (!fail_approval_notification_delivery(
				$notification_id,
				$lease_token,
				$sender_status,
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

		$bcc_status = approval_notification_bcc_address_status(
			get_company_pref('bcc_email')
		);
		if ($bcc_status !== 'valid') {
			$summary[$bcc_status]++;
			if (!fail_approval_notification_delivery(
				$notification_id,
				$lease_token,
				$bcc_status,
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

		$transport_status = approval_notification_mail_transport_status();
		if ($transport_status !== 'valid') {
			$summary[$transport_status]++;
			if (!fail_approval_notification_delivery(
				$notification_id,
				$lease_token,
				$transport_status,
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
		$delivery_failure_status = 'delivery_failed';
		$delivered = send_approval_email(
			isset($notification['user_name'])
				? $notification['user_name'] : '',
			$recipient_email,
			$subject,
			$body,
			$delivery_failure_status
		);
		if (!$delivered) {
			if (!isset($summary[$delivery_failure_status]))
				$delivery_failure_status = 'delivery_failed';
			$summary[$delivery_failure_status]++;
			if (!fail_approval_notification_delivery(
				$notification_id,
				$lease_token,
				$delivery_failure_status,
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
 * Classify one approval-notification recipient before provider invocation.
 *
 * Empty addresses remain separately repairable from malformed addresses. The
 * validation rejects control characters and the RFC mailbox-length ceiling
 * before using PHP's email syntax validator. This keeps header-injection and
 * other malformed poison rows out of the external mail transport while
 * retaining the existing bounded retry/dead-letter repair workflow.
 *
 * @param string $recipient_email
 * @return string valid, missing_email, or invalid_email
 */
function approval_notification_email_address_status($recipient_email)
{
	$recipient_email = trim((string)$recipient_email);
	if ($recipient_email === '')
		return 'missing_email';

	if (strlen($recipient_email) > 254
		|| preg_match('/[\x00\r\n]/', $recipient_email)
		|| filter_var($recipient_email, FILTER_VALIDATE_EMAIL) === false
	) {
		return 'invalid_email';
	}

	return 'valid';
}

/**
 * Classify the configured approval-email sender before provider invocation.
 *
 * Missing configuration remains separately repairable from malformed sender
 * data. No HTTP Host fallback is used because request headers are not trusted
 * mail identity. The same mailbox ceiling, control-character rejection, and
 * PHP syntax validation used for recipients applies to the company sender.
 *
 * @param string $sender_email
 * @return string valid, missing_sender, or invalid_sender
 */
function approval_notification_sender_address_status($sender_email)
{
	$sender_email = trim((string)$sender_email);
	if ($sender_email === '')
		return 'missing_sender';

	if (strlen($sender_email) > 254
		|| preg_match('/[\x00\r\n]/', $sender_email)
		|| filter_var($sender_email, FILTER_VALIDATE_EMAIL) === false
	) {
		return 'invalid_sender';
	}

	return 'valid';
}

/**
 * Classify the optional company-wide BCC before provider invocation.
 *
 * Blank configuration disables BCC and remains valid. A configured value must
 * be one mailbox only; malformed syntax, control characters, and over-length
 * values fail closed before the mail object can copy the value into a header.
 * The durable queue stores only the PII-free invalid_bcc outcome so an
 * authorized operator can repair configuration and use the existing retry.
 *
 * @param string $bcc_email
 * @return string valid or invalid_bcc
 */
function approval_notification_bcc_address_status($bcc_email)
{
	$bcc_email = trim((string)$bcc_email);
	if ($bcc_email === '')
		return 'valid';

	if (strlen($bcc_email) > 254
		|| preg_match('/[\x00\r\n]/', $bcc_email)
		|| filter_var($bcc_email, FILTER_VALIDATE_EMAIL) === false
	) {
		return 'invalid_bcc';
	}

	return 'valid';
}

/**
 * Classify native PHP mail transport availability before provider invocation.
 *
 * Some hardened PHP deployments disable mail() through disable_functions. PHP
 * can also expose mail() while its configured command transport points to an
 * executable that is absent or not executable. Unix-like runtimes use that
 * command path; Windows does too when sendmail_path is configured. Either state
 * would otherwise fail after a durable queue row has been claimed.
 * The queue records only bounded deployment-state tokens and retains the
 * existing retry/dead-letter and authorized-repair behavior. No command is
 * executed by this preflight, and SMTP/provider acceptance remains outside it.
 *
 * @return string valid, transport_unavailable,
 *                transport_encoding_unavailable,
 *                transport_arg_escape_unavailable,
 *                transport_forced_params_override,
 *                transport_args_incompatible,
 *                transport_smtp_config_invalid, or
 *                transport_executable_unavailable
 */
function approval_notification_mail_transport_status()
{
	if (!function_exists('mail'))
		return 'transport_unavailable';

	// The legacy mail class MIME-encodes sender/recipient names and subjects
	// during construction/use. Detect a missing or disabled iconv capability
	// before loading the class so a claimed queue row cannot fatal mid-attempt.
	if (!function_exists('iconv_mime_encode'))
		return 'transport_encoding_unavailable';

	// The legacy mail class shell-escapes the envelope sender before passing
	// the -f argument to mail(). Hardened runtimes can disable escapeshellarg;
	// fail before class construction rather than risking a fatal after claim.
	if (!function_exists('escapeshellarg'))
		return 'transport_arg_escape_unavailable';

	$sendmail_path = trim((string)ini_get('sendmail_path'));
	if (!approval_notification_mail_uses_command_transport(PHP_OS, $sendmail_path)) {
		$native_status = approval_notification_mail_native_smtp_status(
			ini_get('SMTP'),
			ini_get('smtp_port')
		);
		if ($native_status !== 'valid')
			return $native_status;

		// PHP's native Windows TSendMail resolves the local machine name before
		// it resolves or connects to the configured SMTP endpoint. Surface that
		// deterministic local prerequisite through the existing bounded transport
		// outcomes instead of letting a claimed row fall into generic delivery
		// failure after the legacy mail class is constructed.
		if (!function_exists('gethostname') || !function_exists('gethostbyname'))
			return 'transport_unavailable';
		$local_host = gethostname();
		$resolved_local_host = $local_host === false
			? '' : gethostbyname((string)$local_host);
		$local_host_status = approval_notification_mail_native_smtp_local_host_status(
			$local_host,
			$resolved_local_host
		);
		if ($local_host_status !== 'valid')
			return $local_host_status;

		// TSendMail resolves the configured SMTP endpoint through the same IPv4
		// resolver immediately before connect(). Classify an unresolved provider
		// endpoint before legacy mail-class construction while preserving the
		// provider-free boundary: this performs DNS only, never a socket/provider call.
		$smtp_host = trim((string)ini_get('SMTP'));
		$resolved_smtp_host = gethostbyname($smtp_host);
		$smtp_resolution_status = approval_notification_mail_native_smtp_resolution_status(
			$smtp_host,
			$resolved_smtp_host
		);
		if ($smtp_resolution_status !== 'valid')
			return $smtp_resolution_status;

		return approval_notification_mail_native_smtp_sender_status(
			ini_get('sendmail_from'),
			get_company_pref('email')
		);
	}

	// PHP's mail.force_extra_parameters replaces mail()'s fifth argument when
	// the configured sendmail command transport is active. sendmail_path can
	// activate that command transport on Windows too, so this protection is
	// mode-based rather than a blanket operating-system exemption.
	if (trim((string)ini_get('mail.force_extra_parameters')) !== '')
		return 'transport_forced_params_override';

	$executable = approval_notification_mail_transport_executable($sendmail_path);
	if ($executable === ''
		|| !approval_notification_mail_transport_executable_available($executable)
	) {
		return 'transport_executable_unavailable';
	}

	if (approval_notification_mail_transport_arguments_status($sendmail_path) !== 'valid')
		return 'transport_args_incompatible';

	return 'valid';
}

/**
 * Validate PHP's native Windows SMTP endpoint configuration without connecting.
 *
 * PHP's built-in SMTP transport is used only on Windows when sendmail_path is
 * blank. PHP's native TSendMail path uses an IPv4 socket/address structure and
 * rejects SMTP host values of 256 bytes or more before connecting. This preflight
 * therefore accepts only a non-empty DNS/IPv4 host without a colon, at most 255
 * bytes, plus a numeric TCP port in the valid 1..65535 range except 465. Port 465
 * is implicit-TLS message submission, while this native PHP path does not perform
 * a TLS handshake. The colon check also rejects accidental host:port input because
 * smtp_port is configured separately. This check deliberately performs no DNS lookup,
 * socket connection, authentication, TLS negotiation, or provider invocation,
 * and it never returns or persists the configured host or port.
 *
 * @param string $smtp_host Configured PHP SMTP host/IP
 * @param mixed  $smtp_port Configured PHP smtp_port
 * @return string valid or transport_smtp_config_invalid
 */
function approval_notification_mail_native_smtp_status($smtp_host, $smtp_port)
{
	$smtp_host = trim((string)$smtp_host);
	$smtp_port = trim((string)$smtp_port);

	if ($smtp_host === '' || strlen($smtp_host) >= 256
		|| strpos($smtp_host, ':') !== false
		|| preg_match('/[\x00-\x20\x7f]/', $smtp_host))
		return 'transport_smtp_config_invalid';

	if (!preg_match('/^[0-9]{1,5}$/', $smtp_port))
		return 'transport_smtp_config_invalid';

	$port = (int)$smtp_port;
	if ($port === 465)
		return 'transport_smtp_config_invalid';

	return $port >= 1 && $port <= 65535
		? 'valid' : 'transport_smtp_config_invalid';
}

/**
 * Validate PHP native Windows SMTP's local-host identity prerequisite.
 *
 * TSendMail obtains the local machine name and resolves it through the IPv4
 * host resolver before it resolves or connects to the configured SMTP server.
 * A missing/unresolvable local host therefore cannot reach provider delivery.
 * This pure classifier accepts the already-observed local host and resolver
 * result so the boundary can be tested without DNS or socket activity. The
 * runtime caller performs only the same local hostname lookup PHP will perform
 * later; it does not resolve the configured SMTP provider or open a socket.
 *
 * gethostbyname() returns its input unchanged when a non-IP hostname cannot be
 * resolved. An IPv4 literal may legitimately remain unchanged and is accepted.
 * No host value is persisted or returned in durable queue evidence.
 *
 * @param mixed  $local_host          Result from gethostname()
 * @param string $resolved_local_host Result from gethostbyname()
 * @return string valid or transport_smtp_config_invalid
 */
function approval_notification_mail_native_smtp_local_host_status($local_host,
	$resolved_local_host)
{
	$local_host = trim((string)$local_host);
	$resolved_local_host = trim((string)$resolved_local_host);

	if ($local_host === '' || strlen($local_host) >= 256
		|| preg_match('/[\x00-\x20\x7f]/', $local_host))
		return 'transport_smtp_config_invalid';

	if ($resolved_local_host === '')
		return 'transport_smtp_config_invalid';

	if ($resolved_local_host === $local_host
		&& filter_var($local_host, FILTER_VALIDATE_IP, FILTER_FLAG_IPV4) === false)
		return 'transport_smtp_config_invalid';

	return filter_var(
		$resolved_local_host,
		FILTER_VALIDATE_IP,
		FILTER_FLAG_IPV4
	) !== false ? 'valid' : 'transport_smtp_config_invalid';
}

/**
 * Validate PHP native Windows SMTP configured-provider IPv4 resolution.
 *
 * TSendMail's GetAddr() first accepts a dotted IPv4 literal and otherwise uses
 * gethostbyname() immediately before connect(). PHP's gethostbyname() returns
 * the input hostname unchanged when resolution fails. This pure classifier
 * accepts the already-observed endpoint and resolver result so failure behavior
 * is deterministic in tests. Runtime performs only the same configured-host DNS
 * lookup PHP will perform later; no socket, TLS, authentication, credential, or
 * SMTP/provider call is made and neither value is persisted in durable evidence.
 *
 * @param string $smtp_host          Configured PHP SMTP host/IP
 * @param string $resolved_smtp_host Result from gethostbyname()
 * @return string valid or transport_smtp_config_invalid
 */
function approval_notification_mail_native_smtp_resolution_status($smtp_host,
	$resolved_smtp_host)
{
	$smtp_host = trim((string)$smtp_host);
	$resolved_smtp_host = trim((string)$resolved_smtp_host);

	if ($smtp_host === '' || $resolved_smtp_host === '')
		return 'transport_smtp_config_invalid';

	if ($resolved_smtp_host === $smtp_host
		&& filter_var($smtp_host, FILTER_VALIDATE_IP, FILTER_FLAG_IPV4) === false)
		return 'transport_smtp_config_invalid';

	return filter_var(
		$resolved_smtp_host,
		FILTER_VALIDATE_IP,
		FILTER_FLAG_IPV4
	) !== false ? 'valid' : 'transport_smtp_config_invalid';
}

/**
 * Classify PHP native Windows SMTP connect() failure without a second probe.
 *
 * PHP's Windows mail() path converts MailConnect() failure into a warning whose
 * stable prefix identifies the failed provider TCP connection. The runtime
 * caller captures that warning only around the existing mail attempt, suppresses
 * the endpoint-bearing warning from generic PHP error output, and persists only
 * this bounded PII-free token. No duplicate socket probe or parallel transport is
 * introduced. Other mail failures retain the generic delivery_failed outcome.
 *
 * @param string $warning_message Captured PHP mail warning
 * @param bool   $uses_native_smtp True only for Windows native SMTP mode
 * @return string transport_smtp_connect_failed or delivery_failed
 */
function approval_notification_mail_native_smtp_failure_status($warning_message,
	$uses_native_smtp)
{
	$warning_message = (string)$warning_message;
	if (!$uses_native_smtp)
		return 'delivery_failed';

	return strpos($warning_message, 'Failed to connect to mailserver at "') !== false
		&& strpos(
			$warning_message,
			'verify your "SMTP" and "smtp_port" setting in php.ini'
		) !== false
		? 'transport_smtp_connect_failed' : 'delivery_failed';
}

/**
 * Preserve the approved envelope sender on PHP's native Windows SMTP path.
 *
 * When Windows uses its built-in SMTP transport, PHP ignores mail()'s fifth
 * command argument. A non-empty global sendmail_from value is then preferred
 * over the message From header as the SMTP reverse-path. Blank configuration
 * therefore remains compatible with the legacy company's validated From
 * header, while a configured value must exactly match that approved sender.
 * The comparison is local only: it performs no DNS, socket, credential, or
 * provider operation and never persists either mailbox value.
 *
 * @param string $sendmail_from  Configured PHP sendmail_from value
 * @param string $approved_sender Validated company sender mailbox
 * @return string valid or transport_forced_params_override
 */
function approval_notification_mail_native_smtp_sender_status($sendmail_from,
	$approved_sender)
{
	$sendmail_from = trim((string)$sendmail_from);
	$approved_sender = trim((string)$approved_sender);

	if ($sendmail_from === '' || $sendmail_from === $approved_sender)
		return 'valid';

	return 'transport_forced_params_override';
}

/**
 * Determine whether PHP mail() is using the configured command transport.
 *
 * Unix-like runtimes use sendmail_path for native mail delivery. Windows uses
 * its built-in SMTP transport only while sendmail_path is blank; PHP supports
 * sendmail_path on Windows and executes that configured command when present.
 * Keeping this decision in one pure helper prevents command-only safeguards
 * from being skipped solely because the operating system is Windows.
 *
 * @param string $os            PHP_OS-compatible operating-system token
 * @param string $sendmail_path Configured sendmail_path value
 * @return bool
 */
function approval_notification_mail_uses_command_transport($os, $sendmail_path)
{
	if (strncasecmp((string)$os, 'WIN', 3) !== 0)
		return true;

	return trim((string)$sendmail_path) !== '';
}

/**
 * Extract the directly invoked executable token from sendmail_path.
 *
 * This deliberately does not evaluate shell syntax. It accepts the common
 * direct-command forms (absolute/bare executable, optionally single- or
 * double-quoted) and fails closed for blank, control-character, or malformed
 * configuration. Arguments remain outside the returned token.
 *
 * @param string $command
 * @return string
 */
function approval_notification_mail_transport_executable($command)
{
	$command = ltrim((string)$command);
	if ($command === '' || preg_match('/[\x00\r\n]/', $command))
		return '';

	if (!preg_match('/^(?:"([^"]+)"|\'([^\']+)\'|([^\s]+))/', $command, $matches))
		return '';

	if (isset($matches[1]) && $matches[1] !== '')
		return $matches[1];
	if (isset($matches[2]) && $matches[2] !== '')
		return $matches[2];
	return isset($matches[3]) ? $matches[3] : '';
}

/**
 * Verify one executable token without invoking it.
 *
 * Absolute and relative paths containing a directory separator are checked
 * directly. Bare commands are resolved against PATH. The checks are suppressed
 * because open_basedir may itself deny inspection; denial remains fail-closed.
 *
 * @param string $executable
 * @return bool
 */
function approval_notification_mail_transport_executable_available($executable)
{
	$executable = (string)$executable;
	if ($executable === '')
		return false;

	if (strpos($executable, '/') !== false || strpos($executable, '\\') !== false)
		return @is_file($executable) && @is_executable($executable);

	$path = getenv('PATH');
	if ($path === false || $path === '')
		return false;

	foreach (explode(PATH_SEPARATOR, $path) as $directory) {
		if ($directory === '')
			continue;
		$candidate = rtrim($directory, '/\\').DIRECTORY_SEPARATOR.$executable;
		if (@is_file($candidate) && @is_executable($candidate))
			return true;
	}

	return false;
}


/**
 * Verify the minimum sendmail-compatible arguments required by PHP mail().
 *
 * Command transport receives the message through standard input. The legacy
 * mail class supplies the recipient to mail(), so the command must extract
 * recipients from message headers (-t). It must also ignore a line containing
 * only a dot (-i or the compatible -oi form) so body content cannot terminate
 * the message before PHP finishes writing it. Only exact standalone option
 * tokens are accepted; ambiguous quoted/combined forms fail closed.
 *
 * @param string $command Configured sendmail_path value
 * @return string valid or transport_args_incompatible
 */
function approval_notification_mail_transport_arguments_status($command)
{
	$command = ltrim((string)$command);
	if ($command === '' || preg_match('/[\x00\r\n]/', $command))
		return 'transport_args_incompatible';

	if (!preg_match('/^(?:"[^"]+"|\'[^\']+\'|[^\s]+)(?:\s+(.*))?$/', $command, $matches))
		return 'transport_args_incompatible';

	$argument_text = isset($matches[1]) ? trim($matches[1]) : '';
	if ($argument_text === ''
		|| strpos($argument_text, '"') !== false
		|| strpos($argument_text, "'") !== false
		|| strpos($argument_text, '\\') !== false
	) {
		return 'transport_args_incompatible';
	}

	$arguments = preg_split('/\s+/', $argument_text);
	$extracts_recipients = false;
	$dot_safe = false;
	foreach ($arguments as $argument) {
		if ($argument === '-t')
			$extracts_recipients = true;
		elseif ($argument === '-i' || $argument === '-oi')
			$dot_safe = true;
	}

	return $extracts_recipients && $dot_safe
		? 'valid' : 'transport_args_incompatible';
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
 * @param string $failure_status  PII-free failure classification on false return
 * @return bool                   True on success
 */
function send_approval_email($recipient_name, $recipient_email, $subject, $body,
	&$failure_status = null)
{
	global $path_to_root;

	$company_name = get_company_pref('coy_name');
	$company_email = trim((string)get_company_pref('email'));
	$company_bcc = get_company_pref('bcc_email');

	if (approval_notification_email_address_status($recipient_email) !== 'valid'
		|| approval_notification_sender_address_status($company_email) !== 'valid'
		|| approval_notification_bcc_address_status($company_bcc) !== 'valid'
		|| approval_notification_mail_transport_status() !== 'valid'
	) {
		return false;
	}

	require_once($path_to_root . '/reporting/includes/class.mail.inc');

	$mail = new email($company_name, $company_email);
	$mail->to($recipient_name, $recipient_email);
	$mail->subject($subject);
	$mail->text($body);

	$failure_status = 'delivery_failed';
	$sendmail_path = trim((string)ini_get('sendmail_path'));
	$uses_native_smtp = !approval_notification_mail_uses_command_transport(
		PHP_OS,
		$sendmail_path
	);
	$captured_warning = '';
	$warning_handler_installed = false;
	if ($uses_native_smtp && function_exists('set_error_handler')
		&& function_exists('restore_error_handler')) {
		set_error_handler(function ($errno, $errstr) use (&$captured_warning) {
			if ($errno === E_WARNING
				&& approval_notification_mail_native_smtp_failure_status(
					$errstr,
					true
				) === 'transport_smtp_connect_failed') {
				$captured_warning = (string)$errstr;
				return true;
			}
			return false;
		}, E_WARNING);
		$warning_handler_installed = true;
	}

	$delivered = false;
	try {
		$delivered = (bool)$mail->send();
	} finally {
		if ($warning_handler_installed)
			restore_error_handler();
	}

	if (!$delivered)
		$failure_status = approval_notification_mail_native_smtp_failure_status(
			$captured_warning,
			$uses_native_smtp
		);

	return $delivered;
}
