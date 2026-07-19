<?php
/**
 * Session transport and security-event helpers.
 *
 * These helpers preserve the existing PHP session architecture while making
 * the browser cookie policy and its audit evidence explicit and testable.
 */

/**
 * Return whether the current request is using HTTPS.
 *
 * The explicit value is used by SessionManager when SECURE_ONLY is configured.
 * Proxy headers are intentionally not trusted at the application boundary.
 *
 * @param bool|null $secure
 * @return bool
 */
function session_transport_is_https($secure = null)
{
	if (isset($secure))
		return (bool)$secure;

	return !empty($_SERVER['HTTPS']) && strtolower($_SERVER['HTTPS']) !== 'off';
}

/**
 * Return whether an explicitly HTTPS-only deployment may serve this request.
 *
 * Proxy headers are deliberately ignored. A TLS-terminating proxy must set
 * the trusted server HTTPS state before PHP executes.
 *
 * @param bool|null $secure_only
 * @return bool
 */
function session_https_transport_allowed($secure_only = null)
{
	if (!$secure_only)
		return true;
	return session_transport_is_https(null);
}

/**
 * Return the approved browser session SameSite policy.
 *
 * @return string
 */
function session_cookie_samesite_policy()
{
	return 'Lax';
}

/**
 * Configure the existing PHP session cookie with a PHP-version-safe policy.
 *
 * PHP 7.3 added the native SameSite option array. Supported older runtimes use
 * the established cookie-path attribute form so PHP 5.6 remains compatible.
 *
 * @param int $lifetime
 * @param string $path
 * @param string|null $domain
 * @param bool $secure
 * @return bool
 */
function session_configure_cookie_policy($lifetime, $path, $domain, $secure)
{
	$samesite = session_cookie_samesite_policy();
	if (version_compare(PHP_VERSION, '7.3.0', '>=')) {
		return session_set_cookie_params(array(
			'lifetime' => (int)$lifetime,
			'path' => $path,
			'domain' => isset($domain) ? $domain : '',
			'secure' => (bool)$secure,
			'httponly' => true,
			'samesite' => $samesite,
		));
	}

	$legacy_path = rtrim($path, '; ').'; SameSite='.$samesite;
	return session_set_cookie_params((int)$lifetime, $legacy_path, $domain, (bool)$secure, true);
}

/**
 * Emit HSTS only when PHP has positively identified an HTTPS request.
 *
 * @param bool $https
 * @return bool
 */
function session_send_transport_security_header($https)
{
	if (!$https || PHP_SAPI === 'cli' || headers_sent())
		return false;

	header('Strict-Transport-Security: max-age=31536000');
	return true;
}

/**
 * Expire the browser session cookie using the same constrained attributes.
 *
 * @return bool
 */
function session_expire_browser_cookie()
{
	if (PHP_SAPI === 'cli' || headers_sent() || session_name() === '')
		return false;

	$params = session_get_cookie_params();
	$path = isset($params['path']) && $params['path'] !== '' ? $params['path'] : '/';
	$domain = isset($params['domain']) ? $params['domain'] : '';
	$secure = !empty($params['secure']);
	if (version_compare(PHP_VERSION, '7.3.0', '>=')) {
		return setcookie(session_name(), '', array(
			'expires' => time() - 42000,
			'path' => $path,
			'domain' => $domain,
			'secure' => $secure,
			'httponly' => true,
			'samesite' => session_cookie_samesite_policy(),
		));
	}

	$legacy_path = rtrim($path, '; ').'; SameSite='.session_cookie_samesite_policy();
	return setcookie(session_name(), '', time() - 42000, $legacy_path, $domain, $secure, true);
}

/**
 * Normalize a session-security outcome for log safety.
 *
 * @param string $outcome
 * @return string
 */
function session_security_normalize_outcome($outcome)
{
	$outcome = strtolower((string)$outcome);
	$outcome = preg_replace('/[^a-z0-9_]+/', '_', $outcome);
	return trim($outcome, '_');
}

/**
 * Write PII-free session security evidence to the existing server log.
 *
 * Session IDs, credentials, login names, IP addresses, and user agents are
 * deliberately excluded. PAY-AUD-001 remains the owner of immutable evidence.
 *
 * @param string $outcome
 * @return void
 */
function session_log_security_event($outcome)
{
	$actor_id = 0;
	$company_id = 0;
	if (isset($_SESSION['wa_current_user'])) {
		$actor_id = isset($_SESSION['wa_current_user']->user)
			? (int)$_SESSION['wa_current_user']->user : 0;
		$company_id = isset($_SESSION['wa_current_user']->company)
			? (int)$_SESSION['wa_current_user']->company : 0;
	}

	error_log('[security][session] actor_id='.$actor_id
		.' company='.$company_id
		.' outcome='.session_security_normalize_outcome($outcome));
}
