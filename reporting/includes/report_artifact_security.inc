<?php
/**
 * Short-lived authorization boundary for generated report artifacts.
 *
 * Metadata remains in the authenticated session and files remain outside the
 * web document tree. This is a containment layer for the existing renderer,
 * not a durable report-job or artifact framework.
 *
 * DIM-APR-002 binds each grant to the live report authorization scope: the
 * actor, active company/connection and the current role/area fingerprint. A
 * permission revocation, role change or company switch invalidates outstanding
 * grants before the file is served.
 */
require_once dirname(__FILE__).'/report_access_scope.inc';

define('REPORT_ARTIFACT_TTL_SECONDS', 180);
define('REPORT_ARTIFACT_MAX_ACTIVE', 5);
define('REPORT_ARTIFACT_MAX_BYTES', 26214400);

/**
 * Return the authenticated actor identifier.
 *
 * @return int
 */
function report_artifact_actor_id() {
	$principal = report_access_principal();
	return $principal['actor_id'];
}

/**
 * Return the authenticated company index.
 *
 * @return int
 */
function report_artifact_company_id() {
	$principal = report_access_principal();
	if ($principal['company_id'] >= 0)
		return $principal['company_id'];
	return isset($_SESSION['wa_current_user']->company)
		? (int)$_SESSION['wa_current_user']->company : -1;
}

/**
 * Return and create the private temporary directory for this installation and company.
 *
 * @return string|false
 */
function report_artifact_private_directory() {
	$base = rtrim(sys_get_temp_dir(), '/\\');
	$install_key = substr(hash('sha256', dirname(dirname(__FILE__))), 0, 24);
	$directory = $base . DIRECTORY_SEPARATOR . 'notrinoserp-report-artifacts'
		. DIRECTORY_SEPARATOR . $install_key
		. DIRECTORY_SEPARATOR . 'company-' . report_artifact_company_id();

	if (!is_dir($directory) && !@mkdir($directory, 0700, true))
		return false;

	return is_dir($directory) ? $directory : false;
}

/**
 * Write a PII-free report-artifact security event.
 *
 * @param string $outcome
 * @param string|int $report_id
 * @param string $purpose
 * @param int $bytes
 * @return void
 */
function report_artifact_log($outcome, $report_id, $purpose, $bytes = 0) {
	$safe_outcome = preg_replace('/[^a-z0-9_]/', '', strtolower((string)$outcome));
	$safe_report = preg_replace('/[^a-z0-9_]/', '', strtolower((string)$report_id));
	$safe_purpose = preg_replace('/[^a-z0-9_]/', '', strtolower((string)$purpose));
	error_log(sprintf(
		'[REPORT_ARTIFACT_SECURITY] outcome=%s actor_id=%d company=%d report_id=%s purpose=%s bytes=%d',
		$safe_outcome === '' ? 'unknown' : $safe_outcome,
		report_artifact_actor_id(),
		report_artifact_company_id(),
		$safe_report === '' ? 'unknown' : $safe_report,
		$safe_purpose === '' ? 'unknown' : $safe_purpose,
		max(0, (int)$bytes)
	));
}

/**
 * Verify that the actor still has the bound report permission in current DB state.
 *
 * @param string $security_area
 * @param string|int|null $report_id
 * @return bool
 */
function report_artifact_has_fresh_permission($security_area, $report_id = null) {
	$options = array();
	if ($report_id !== null)
		$options['report_id'] = $report_id;
	$scope = report_access_check($security_area, 'artifact', $options);
	return !empty($scope['allowed']);
}

/**
 * Remove expired session grants and orphaned private temporary files.
 *
 * @param int|null $now
 * @return int Number of files removed.
 */
function report_artifact_cleanup($now = null) {
	$now = $now === null ? time() : (int)$now;
	$removed = 0;
	if (!isset($_SESSION['report_artifact_grants'])
		|| !is_array($_SESSION['report_artifact_grants'])) {
		$_SESSION['report_artifact_grants'] = array();
	}

	foreach ($_SESSION['report_artifact_grants'] as $token => $grant) {
		if (!is_array($grant) || !isset($grant['expires_at'])
			|| (int)$grant['expires_at'] < $now) {
			if (is_array($grant) && isset($grant['path']) && is_file($grant['path'])
				&& @unlink($grant['path'])) {
				$removed++;
			}
			unset($_SESSION['report_artifact_grants'][$token]);
		}
	}

	$directory = report_artifact_private_directory();
	if ($directory !== false && ($handle = @opendir($directory))) {
		while (($name = readdir($handle)) !== false) {
			if (!preg_match('/^[A-Za-z0-9_-]+\\.(?:xls|pdf|png)$/', $name))
				continue;
			$path = $directory . DIRECTORY_SEPARATOR . $name;
			if (is_file($path) && @filemtime($path) < $now - REPORT_ARTIFACT_TTL_SECONDS
				&& @unlink($path)) {
				$removed++;
			}
		}
		closedir($handle);
	}

	return $removed;
}

/**
 * Register a generated file for one authenticated, authorized download.
 *
 * @param string $path
 * @param string $filename
 * @param string $mime_type
 * @param string|int $report_id
 * @param string $security_area
 * @param string $purpose
 * @return string|false Opaque session-bound token.
 */
function report_artifact_register($path, $filename, $mime_type, $report_id, $security_area, $purpose) {
	report_artifact_cleanup();
	$directory = report_artifact_private_directory();
	$real_path = realpath($path);
	$real_directory = $directory === false ? false : realpath($directory);
	$allowed_mimes = array(
		'application/vnd.ms-excel' => 'xls',
		'application/pdf' => 'pdf',
	);
	$purpose = preg_replace('/[^a-z0-9_]/', '', strtolower((string)$purpose));

	$permission_scope = report_access_check($security_area, 'artifact', array('report_id' => $report_id));
	if ($real_path === false || $real_directory === false
		|| strpos($real_path, $real_directory . DIRECTORY_SEPARATOR) !== 0
		|| !isset($allowed_mimes[$mime_type])
		|| strtolower(pathinfo($real_path, PATHINFO_EXTENSION)) !== $allowed_mimes[$mime_type]
		|| !in_array($purpose, array('interactive_report_download', 'ajax_report_download'), true)
		|| empty($permission_scope['allowed'])) {
		report_artifact_log('registration_denied', $report_id, $purpose);
		return false;
	}
	$permission_fingerprint = $permission_scope['fingerprint'];
	if ($permission_fingerprint === '') {
		@unlink($real_path);
		report_artifact_log('registration_denied_fingerprint', $report_id, $purpose);
		return false;
	}

	$bytes = @filesize($real_path);
	if ($bytes === false || $bytes <= 0 || $bytes > REPORT_ARTIFACT_MAX_BYTES) {
		@unlink($real_path);
		report_artifact_log('registration_denied_size', $report_id, $purpose, $bytes === false ? 0 : $bytes);
		return false;
	}

	while (count($_SESSION['report_artifact_grants']) >= REPORT_ARTIFACT_MAX_ACTIVE) {
		$oldest_token = null;
		$oldest_time = PHP_INT_MAX;
		foreach ($_SESSION['report_artifact_grants'] as $candidate_token => $grant) {
			$created = isset($grant['created_at']) ? (int)$grant['created_at'] : 0;
			if ($created < $oldest_time) {
				$oldest_time = $created;
				$oldest_token = $candidate_token;
			}
		}
		if ($oldest_token === null)
			break;
		$oldest = $_SESSION['report_artifact_grants'][$oldest_token];
		if (isset($oldest['path']) && is_file($oldest['path']))
			@unlink($oldest['path']);
		unset($_SESSION['report_artifact_grants'][$oldest_token]);
	}

	$token = random_id(192);
	$now = time();
	$_SESSION['report_artifact_grants'][$token] = array(
		'actor_id' => report_artifact_actor_id(),
		'company_id' => report_artifact_company_id(),
		'report_id' => preg_replace('/[^a-z0-9_]/', '', strtolower((string)$report_id)),
		'purpose' => $purpose,
		'security_area' => $security_area,
		'permission_fingerprint' => $permission_fingerprint,
		'filename' => report_artifact_safe_filename($filename, $allowed_mimes[$mime_type]),
		'mime_type' => $mime_type,
		'path' => $real_path,
		'bytes' => (int)$bytes,
		'sha256' => hash_file('sha256', $real_path),
		'created_at' => $now,
		'expires_at' => $now + REPORT_ARTIFACT_TTL_SECONDS,
	);
	report_artifact_log('registered', $report_id, $purpose, $bytes);
	return $token;
}

/**
 * Normalize a client-visible report filename.
 *
 * @param string $filename
 * @param string $extension
 * @return string
 */
function report_artifact_safe_filename($filename, $extension) {
	$filename = basename(str_replace('\\', '/', (string)$filename));
	$filename = preg_replace('/[^A-Za-z0-9._-]/', '_', $filename);
	if ($filename === '' || $filename === '.' || $filename === '..')
		$filename = 'report.' . $extension;
	if (strtolower(pathinfo($filename, PATHINFO_EXTENSION)) === strtolower($extension))
		$filename = substr($filename, 0, -strlen($extension) - 1);
	$max_base_length = 179 - strlen($extension);
	$filename = rtrim(substr($filename, 0, $max_base_length), '.');
	return ($filename === '' ? 'report' : $filename) . '.' . $extension;
}

/**
 * Consume and revalidate a one-time report artifact grant.
 *
 * @param string $token
 * @return array|false
 */
function report_artifact_consume($token) {
	report_artifact_cleanup();
	if (!is_string($token) || !preg_match('/^[A-Za-z0-9_-]{24,128}$/', $token)
		|| !isset($_SESSION['report_artifact_grants'][$token])) {
		report_artifact_log('download_denied_unknown', 'unknown', 'unknown');
		return false;
	}

	$grant = $_SESSION['report_artifact_grants'][$token];
	unset($_SESSION['report_artifact_grants'][$token]);
	$permission_scope = false;
	if (is_array($grant) && isset($grant['security_area'], $grant['permission_fingerprint']))
		$permission_scope = report_access_check($grant['security_area'], 'artifact',
			array('report_id' => isset($grant['report_id']) ? $grant['report_id'] : null));
	$valid = is_array($grant)
		&& isset($grant['actor_id'], $grant['company_id'], $grant['expires_at'], $grant['path'],
			$grant['bytes'], $grant['sha256'], $grant['security_area'], $grant['purpose'],
			$grant['permission_fingerprint'])
		&& (int)$grant['actor_id'] === report_artifact_actor_id()
		&& (int)$grant['company_id'] === report_artifact_company_id()
		&& (int)$grant['expires_at'] >= time()
		&& is_array($permission_scope) && !empty($permission_scope['allowed'])
		&& $grant['permission_fingerprint'] === $permission_scope['fingerprint']
		&& is_file($grant['path'])
		&& (int)@filesize($grant['path']) === (int)$grant['bytes']
		&& hash_file('sha256', $grant['path']) === $grant['sha256'];

	if (!$valid) {
		if (is_array($grant) && isset($grant['path']) && is_file($grant['path']))
			@unlink($grant['path']);
		report_artifact_log(
			'download_denied_revalidation',
			isset($grant['report_id']) ? $grant['report_id'] : 'unknown',
			isset($grant['purpose']) ? $grant['purpose'] : 'unknown'
		);
		return false;
	}

	report_artifact_log('download_granted', $grant['report_id'], $grant['purpose'], $grant['bytes']);
	return $grant;
}

/**
 * Revoke every outstanding report artifact grant and remove its private file.
 *
 * Called when the live principal/company/role fingerprint changes. It never
 * touches a path outside the private report artifact directory.
 *
 * @param string $reason Minimized reason for the security log
 * @return int Number of grant files removed
 */
function report_artifact_revoke_all($reason = 'principal_scope_changed') {
	$removed = 0;
	if (!isset($_SESSION['report_artifact_grants']) || !is_array($_SESSION['report_artifact_grants'])) {
		$_SESSION['report_artifact_grants'] = array();
		return $removed;
	}
	$directory = realpath(report_artifact_private_directory());
	$install_directory = $directory === false ? false : realpath(dirname($directory));
	foreach ($_SESSION['report_artifact_grants'] as $token => $grant) {
		if (!is_array($grant) || !isset($grant['path']) || !is_string($grant['path']))
			continue;
		$path = realpath($grant['path']);
		if ($path !== false && $install_directory !== false
			&& strpos($path, $install_directory.DIRECTORY_SEPARATOR) === 0
			&& is_file($path) && @unlink($path))
			$removed++;
	}
	$_SESSION['report_artifact_grants'] = array();
	report_artifact_log('revoked_all', 'all', $reason, 0);
	return $removed;
}
