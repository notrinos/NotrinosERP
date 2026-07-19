<?php
/**
 * Durable authenticated-session inventory and revocation boundary.
 *
 * The registry stores a hash of a random per-login secret, never the PHP
 * session identifier itself. The secret survives compatibility ID rotation
 * inside the PHP session while logout, password/role changes, expiry, and
 * administrator revocation invalidate the durable record.
 */

define('SESSION_REGISTRY_IDLE_LIFETIME', 1800);
define('SESSION_REGISTRY_ABSOLUTE_LIFETIME', 36000);
define('SESSION_REGISTRY_TOUCH_INTERVAL', 60);
define('SESSION_REGISTRY_UPGRADE_VERSION', '1.0.14');

/**
 * Return whether the company session-registry table exists.
 *
 * @return bool
 */
function session_registry_table_exists()
{
	$result = @db_query("SHOW TABLES LIKE '".TB_PREF."user_sessions'");
	return $result && (int)db_num_rows($result) === 1;
}

/**
 * Return a stable SHA-256 digest for registry-bound material.
 *
 * @param string $value
 * @return string
 */
function session_registry_hash($value)
{
	return hash('sha256', (string)$value);
}

/**
 * Compare registry digests without content-dependent timing.
 *
 * @param string $known
 * @param string $provided
 * @return bool
 */
function session_registry_hash_equals($known, $provided)
{
	if (function_exists('hash_equals'))
		return hash_equals((string)$known, (string)$provided);

	$known = (string)$known;
	$provided = (string)$provided;
	if (strlen($known) !== strlen($provided))
		return false;
	$difference = 0;
	for ($i = 0; $i < strlen($known); $i++)
		$difference |= ord($known[$i]) ^ ord($provided[$i]);
	return $difference === 0;
}

/**
 * Return the current request's non-reversible user-agent fingerprint.
 *
 * @return string
 */
function session_registry_user_agent_hash()
{
	$user_agent = isset($_SERVER['HTTP_USER_AGENT']) ? $_SERVER['HTTP_USER_AGENT'] : '';
	return session_registry_hash($user_agent);
}

/**
 * Clamp the persisted idle lifetime to the approved privileged-session cap.
 *
 * @param int $requested_lifetime
 * @return int
 */
function session_registry_idle_lifetime($requested_lifetime)
{
	$requested_lifetime = (int)$requested_lifetime;
	if ($requested_lifetime <= 0)
		$requested_lifetime = SESSION_REGISTRY_IDLE_LIFETIME;
	return max(60, min($requested_lifetime, SESSION_REGISTRY_IDLE_LIFETIME));
}

/**
 * Register the successfully authenticated current user.
 *
 * A missing table is tolerated only on the source/database upgrade boundary;
 * the normal upgrade page remains reachable and the session must log in again
 * after the table is installed.
 *
 * @return bool|null True when registered, null while upgrade is pending.
 */
function session_registry_register_current_session()
{
	if (!isset($_SESSION['wa_current_user']) || !$_SESSION['wa_current_user']->logged_in())
		return false;

	if (!session_registry_table_exists()) {
		$current_version = get_company_pref('version_id', true);
		if ($current_version !== SESSION_REGISTRY_UPGRADE_VERSION) {
			session_log_security_event('session_registry_upgrade_pending');
			return null;
		}
		session_log_security_event('denied_session_registry_missing');
		return false;
	}

	$secret = random_id(256);
	if (!is_string($secret) || strlen($secret) < 32) {
		session_log_security_event('denied_session_registry_secret_failed');
		return false;
	}

	$user = $_SESSION['wa_current_user'];
	$idle_lifetime = session_registry_idle_lifetime($user->timeout);
	$auth_method = isset($user->auth_method) ? $user->auth_method : 'local_password';
	if (!in_array($auth_method, array('local_password', 'external_hook'), true))
		$auth_method = 'unknown';

	$sql = "INSERT INTO ".TB_PREF."user_sessions
		(session_key_hash, user_id, role_id, auth_method, assurance_level,
		 created_at, last_seen_at, idle_expires_at, absolute_expires_at,
		 user_agent_hash)
		VALUES (".db_escape(session_registry_hash($secret)).", ".(int)$user->user.",
		".(int)$user->access.", ".db_escape($auth_method).", 1,
		UTC_TIMESTAMP(), UTC_TIMESTAMP(),
		DATE_ADD(UTC_TIMESTAMP(), INTERVAL ".$idle_lifetime." SECOND),
		DATE_ADD(UTC_TIMESTAMP(), INTERVAL ".SESSION_REGISTRY_ABSOLUTE_LIFETIME." SECOND),
		".db_escape(session_registry_user_agent_hash()).")";
	if (!db_query($sql, 'Could not register authenticated session')) {
		session_log_security_event('denied_session_registry_write_failed');
		return false;
	}

	$_SESSION['security_session_registry_id'] = (int)db_insert_id();
	$_SESSION['security_session_secret'] = $secret;
	session_log_security_event('session_registered');
	return true;
}

/**
 * Revoke one active registry record.
 *
 * @param int $session_id
 * @param int $actor_id
 * @param string $reason
 * @return bool
 */
function session_registry_revoke_session($session_id, $actor_id, $reason)
{
	if (!session_registry_table_exists())
		return false;
	$reason = session_security_normalize_outcome($reason);
	if ($reason === '')
		$reason = 'revoked';
	$reason = substr($reason, 0, 40);

	$sql = "UPDATE ".TB_PREF."user_sessions
		SET revoked_at=COALESCE(revoked_at, UTC_TIMESTAMP()),
			revoked_by=COALESCE(revoked_by, ".(int)$actor_id."),
			revocation_reason=COALESCE(revocation_reason, ".db_escape($reason).")
		WHERE id=".(int)$session_id;
	$result = db_query($sql, 'Could not revoke authenticated session');
	if ($result)
		session_log_security_event('session_revoked_'.$reason);
	return (bool)$result;
}

/**
 * Revoke every active session for a user.
 *
 * @param int $user_id
 * @param int $actor_id
 * @param string $reason
 * @return bool
 */
function session_registry_revoke_user_sessions($user_id, $actor_id, $reason)
{
	if (!session_registry_table_exists())
		return false;
	$reason = session_security_normalize_outcome($reason);
	if ($reason === '')
		$reason = 'revoked';
	$reason = substr($reason, 0, 40);

	$sql = "UPDATE ".TB_PREF."user_sessions
		SET revoked_at=UTC_TIMESTAMP(), revoked_by=".(int)$actor_id.",
			revocation_reason=".db_escape($reason)."
		WHERE user_id=".(int)$user_id." AND revoked_at IS NULL";
	$result = db_query($sql, 'Could not revoke user sessions');
	if ($result)
		session_log_security_event('user_sessions_revoked_'.$reason);
	return (bool)$result;
}

/**
 * Revoke the current registry record when one is present.
 *
 * @param string $reason
 * @return bool
 */
function session_registry_revoke_current_session($reason)
{
	if (!isset($_SESSION['security_session_registry_id']))
		return true;
	$actor_id = isset($_SESSION['wa_current_user']->user)
		? (int)$_SESSION['wa_current_user']->user : 0;
	return session_registry_revoke_session(
		(int)$_SESSION['security_session_registry_id'],
		$actor_id,
		$reason
	);
}

/**
 * Validate role, account, device fingerprint, revocation, and expiry.
 *
 * @return bool|null True when valid, null while upgrade is pending.
 */
function session_registry_validate_current_session()
{
	if (!isset($_SESSION['wa_current_user']) || !$_SESSION['wa_current_user']->logged_in())
		return false;

	if (!session_registry_table_exists()) {
		$current_version = get_company_pref('version_id', true);
		return $current_version !== SESSION_REGISTRY_UPGRADE_VERSION ? null : false;
	}

	if (!isset($_SESSION['security_session_registry_id'])
		|| !isset($_SESSION['security_session_secret'])) {
		return false;
	}

	$user = $_SESSION['wa_current_user'];
	$sql = "SELECT s.*, u.role_id AS current_role_id, u.inactive AS user_inactive
		FROM ".TB_PREF."user_sessions s
		JOIN ".TB_PREF."users u ON u.id=s.user_id
		WHERE s.id=".(int)$_SESSION['security_session_registry_id']."
			AND s.user_id=".(int)$user->user;
	$result = db_query($sql, 'Could not validate authenticated session');
	if (!$result || db_num_rows($result) !== 1)
		return false;
	$row = db_fetch_assoc($result);

	$expected_hash = session_registry_hash($_SESSION['security_session_secret']);
	if (!session_registry_hash_equals($row['session_key_hash'], $expected_hash))
		return false;
	if (!session_registry_hash_equals($row['user_agent_hash'], session_registry_user_agent_hash()))
		return false;
	if ($row['revoked_at'] !== null || (int)$row['user_inactive'] !== 0)
		return false;
	if ((int)$row['role_id'] !== (int)$user->access
		|| (int)$row['current_role_id'] !== (int)$user->access) {
		session_registry_revoke_current_session('role_changed');
		return false;
	}

	$now = time();
	$idle_expires = strtotime($row['idle_expires_at'].' UTC');
	$absolute_expires = strtotime($row['absolute_expires_at'].' UTC');
	if ($idle_expires === false || $absolute_expires === false
		|| $now >= $idle_expires || $now >= $absolute_expires) {
		session_registry_revoke_current_session('expired');
		return false;
	}

	$idle_lifetime = session_registry_idle_lifetime($user->timeout);
	$sql = "UPDATE ".TB_PREF."user_sessions
		SET last_seen_at=UTC_TIMESTAMP(),
			idle_expires_at=LEAST(
				DATE_ADD(UTC_TIMESTAMP(), INTERVAL ".$idle_lifetime." SECOND),
				absolute_expires_at)
		WHERE id=".(int)$row['id']." AND revoked_at IS NULL
			AND last_seen_at <= DATE_SUB(UTC_TIMESTAMP(), INTERVAL ".SESSION_REGISTRY_TOUCH_INTERVAL." SECOND)";
	if (!db_query($sql, 'Could not refresh authenticated session'))
		return false;
	return true;
}

/**
 * Return sanitized session inventory rows for an authorized administrator.
 *
 * @return resource|object|bool
 */
function session_registry_get_inventory()
{
	$sql = "SELECT s.id, s.user_id, COALESCE(u.login_id, '') AS login_id,
		s.role_id, s.auth_method, s.assurance_level, s.created_at,
		s.last_seen_at, s.idle_expires_at, s.absolute_expires_at,
		s.revoked_at, s.revoked_by, s.revocation_reason
		FROM ".TB_PREF."user_sessions s
		LEFT JOIN ".TB_PREF."users u ON u.id=s.user_id
		ORDER BY (s.revoked_at IS NULL) DESC, s.last_seen_at DESC, s.id DESC
		LIMIT 500";
	return db_query($sql, 'Could not read authenticated-session inventory');
}
