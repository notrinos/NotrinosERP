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

/**
 * DIM-APR-002: one report/query/artifact/company authorization scope.
 *
 * The application already has page areas, role grants, a session and a
 * purpose-bound report artifact store. What it did not have was one authority
 * that answers the same authorization question for every report-like entry:
 * report center, direct report script, report redirector, saved selection,
 * drilldown and generated artifact. Hiding a menu item is not an answer.
 *
 * This file owns that answer:
 *
 *  - the principal is derived from the live session only; no company, role,
 *    report scope or owner selector is accepted from request input;
 *  - the exact area must exist in the reviewed area registry and the current
 *    database user/role must still grant its exact code and section. A stale
 *    session role is refused, so a revoked role cannot keep an old report open;
 *  - the active company index, its connection index and its stored table
 *    prefix must be internally consistent. A forged `company` request value or
 *    a switched connection is refused; the same numeric Dimension id in two
 *    companies is therefore never silently re-bound;
 *  - report identity comes from the central registry, not from a menu or a
 *    filename convention. The redirector and the direct-script bootstrap both
 *    use the same decision before any report parameter or aggregate read;
 *  - aggregate/workspace reads use an explicit required-area set and are
 *    checked before the first financial query. A denied workspace renders a
 *    distinct state, never a hidden total;
 *  - saved report selections are keyed by the current principal/company/role
 *    fingerprint, and a generated artifact grant is bound to that same
 *    fingerprint. Company or permission changes therefore invalidate old
 *    selections and old export tokens instead of replaying them;
 *  - audit evidence is minimized to outcome, area, report identity, principal
 *    and company; report values, filters, amounts, comments and credentials
 *    are never logged.
 *
 * What this authority deliberately does not do:
 *  - it does not add a permission, an automatic grant, a new SA_OPEN page or a
 *    schema object. The existing security areas remain authoritative;
 *  - it does not infer a manager scope from an owner/creator field. No such
 *    ownership grant exists in the Dimensions schema, and an owner field would
 *    never be accepted as authority;
 *  - it does not replace the report/PDF/XLS renderer or the artifact store.
 *
 * PHP 5.6 compatible syntax.
 */
require_once dirname(__FILE__).'/report_access_registry.inc';

define('REPORT_ACCESS_SCHEMA_VERSION', 1);
define('REPORT_ACCESS_ALLOWED', 'allowed');
define('REPORT_ACCESS_DENIED', 'denied');
define('REPORT_ACCESS_DISABLED', 'disabled');
define('REPORT_ACCESS_UNKNOWN', 'unknown');

/**
 * Supported report-like intents.
 *
 * @return array
 */
function report_access_intents()
{
	return array('report', 'query', 'drilldown', 'artifact', 'saved_view', 'workspace', 'export');
}

/**
 * Return the live authenticated principal snapshot.
 *
 * `cur_con` is the active database connection. Older tests and non-page callers
 * may omit it; the current company then also names the connection. No request
 * value is read here.
 *
 * @return array
 */
function report_access_principal()
{
	$user = isset($_SESSION['wa_current_user']) ? $_SESSION['wa_current_user'] : null;
	$principal = array('object' => $user, 'actor_id' => 0, 'company_id' => -1,
		'connection_id' => -1, 'role_id' => 0, 'logged_in' => false,
		'old_db' => false, 'prefix' => '');
	if (!is_object($user))
		return $principal;

	if (isset($user->user))
		$principal['actor_id'] = (int)$user->user;
	if (isset($user->company))
		$principal['company_id'] = (int)$user->company;
	if (isset($user->cur_con))
		$principal['connection_id'] = (int)$user->cur_con;
	else
		$principal['connection_id'] = $principal['company_id'];
	if (isset($user->access))
		$principal['role_id'] = (int)$user->access;
	if (isset($user->old_db))
		$principal['old_db'] = !empty($user->old_db);
	if (method_exists($user, 'logged_in'))
		$principal['logged_in'] = (bool)$user->logged_in();
	if (isset($GLOBALS['db_connections'][$principal['connection_id']]['tbpref']))
		$principal['prefix'] = (string)$GLOBALS['db_connections'][$principal['connection_id']]['tbpref'];

	return $principal;
}

/**
 * Clear the per-request user/role read cache.
 *
 * Production ends the cache with the request. Focused lifecycle tests use this
 * after mutating a synthetic role so the next decision reads current state.
 *
 * @return void
 */
function report_access_reset_cache()
{
	$GLOBALS['report_access_cache'] = array();
}

/**
 * Read the current user row once per request.
 *
 * @param int $actor_id
 * @return array|false
 */
function report_access_user_row($actor_id)
{
	if (!function_exists('get_user'))
		return false;
	if (!isset($GLOBALS['report_access_cache']['user']) || !is_array($GLOBALS['report_access_cache']['user']))
		$GLOBALS['report_access_cache']['user'] = array();
	if (!array_key_exists($actor_id, $GLOBALS['report_access_cache']['user']))
		$GLOBALS['report_access_cache']['user'][$actor_id] = get_user($actor_id);
	return $GLOBALS['report_access_cache']['user'][$actor_id];
}

/**
 * Read the current role row.
 *
 * Role state is deliberately not cached across checks: a permission revocation
 * must be observed by the very next report/artifact decision, including inside
 * a long-running worker or a focused lifecycle test.
 *
 * @param int $role_id
 * @return array|false
 */
function report_access_role_row($role_id)
{
	if (!function_exists('get_security_role'))
		return false;
	return get_security_role($role_id);
}

/**
 * Normalize a stored area/section list to unique strings.
 *
 * @param mixed $values
 * @return array
 */
function report_access_code_list($values)
{
	$codes = array();
	if (!is_array($values))
		return $codes;
	foreach ($values as $value) {
		if (!is_scalar($value))
			continue;
		$value = trim((string)$value);
		if ($value !== '')
			$codes[] = $value;
	}
	return array_values(array_unique($codes));
}

/**
 * Build one authorization result.
 *
 * @param bool $allowed
 * @param string $state
 * @param string $reason
 * @param array $fields
 * @return array
 */
function report_access_result($allowed, $state, $reason, $fields = array())
{
	$result = array('schema' => REPORT_ACCESS_SCHEMA_VERSION, 'allowed' => (bool)$allowed,
		'state' => $state, 'reason' => $reason, 'areas' => array(), 'area' => '',
		'report_id' => null, 'aggregate' => '', 'intent' => '', 'actor_id' => 0,
		'company_id' => -1, 'connection_id' => -1, 'role_id' => 0,
		'fingerprint' => '', 'prefix' => '', 'legacy_session' => false);
	foreach ($fields as $key => $value)
		$result[$key] = $value;
	return $result;
}

/**
 * Return the exact code/section of one reviewed security area.
 *
 * @param string $security_area
 * @param int $code
 * @param int $section
 * @return bool
 */
function report_access_area_code($security_area, &$code, &$section)
{
	$code = 0;
	$section = 0;
	if (!isset($GLOBALS['security_areas']) || !is_array($GLOBALS['security_areas'])
		|| !is_string($security_area) || !isset($GLOBALS['security_areas'][$security_area][0]))
		return false;
	$code = (int)$GLOBALS['security_areas'][$security_area][0];
	if ($code <= 0)
		return false;
	$section = $code & ~0xff;
	return true;
}

/**
 * Stable fingerprint of a principal/role/area grant state.
 *
 * It changes when the actor, active company/connection/prefix, role identity,
 * exact area or the role's stored area/section set changes. Saved selections and
 * artifact grants are bound to it, so revocation cannot replay old state.
 *
 * @param array $principal
 * @param array $role
 * @param array $areas
 * @return string
 */
function report_access_fingerprint_for($principal, $role, $areas)
{
	$role_areas = isset($role['areas']) ? report_access_code_list($role['areas']) : array();
	$role_sections = isset($role['sections']) ? report_access_code_list($role['sections']) : array();
	sort($role_areas);
	sort($role_sections);
	$areas = report_access_code_list($areas);
	sort($areas);
	return hash('sha256', implode('|', array(
		'dim-apr-002-v'.REPORT_ACCESS_SCHEMA_VERSION,
		(string)(int)$principal['actor_id'],
		(string)(int)$principal['company_id'],
		(string)(int)$principal['connection_id'],
		(string)(int)$principal['role_id'],
		(string)$principal['prefix'],
		implode(',', $areas),
		implode(',', $role_areas),
		implode(',', $role_sections),
	)));
}

/**
 * Return the current role/principal fingerprint even when no area is checked.
 *
 * @return string Empty when no current principal can be verified
 */
function report_access_current_fingerprint()
{
	$principal = report_access_principal();
	if (!is_object($principal['object']) || $principal['actor_id'] <= 0
		|| $principal['role_id'] <= 0 || $principal['company_id'] < 0
		|| $principal['connection_id'] < 0)
		return '';
	$user = report_access_user_row($principal['actor_id']);
	$role = report_access_role_row($principal['role_id']);
	if (!is_array($user) || !is_array($role) || !empty($user['inactive'])
		|| !empty($role['inactive']) || (int)$user['role_id'] !== $principal['role_id'])
		return '';
	return report_access_fingerprint_for($principal, $role, array());
}

/**
 * Fingerprint one exact area decision for artifact and saved-view binding.
 *
 * @param string $security_area
 * @return string
 */
function report_access_area_fingerprint($security_area)
{
	$principal = report_access_principal();
	if (!is_object($principal['object']) || $principal['actor_id'] <= 0
		|| $principal['role_id'] <= 0 || $principal['company_id'] < 0
		|| $principal['connection_id'] < 0)
		return '';
	if (!report_access_area_code($security_area, $code, $section))
		return '';
	$user = report_access_user_row($principal['actor_id']);
	$role = report_access_role_row($principal['role_id']);
	if (!is_array($user) || !is_array($role) || !empty($user['inactive'])
		|| !empty($role['inactive']) || (int)$user['role_id'] !== $principal['role_id'])
		return '';
	return report_access_fingerprint_for($principal, $role, array($security_area));
}

/**
 * Bind the current principal/company/role fingerprint to the session.
 *
 * A changed binding invalidates every outstanding report artifact and rotates
 * the saved-selection epoch. The first binding of a fresh session has nothing
 * to revoke.
 *
 * @param array $principal
 * @param string $fingerprint
 * @return void
 */
function report_access_bind_session($principal, $fingerprint)
{
	$current = array('schema' => REPORT_ACCESS_SCHEMA_VERSION,
		'actor_id' => (int)$principal['actor_id'],
		'company_id' => (int)$principal['company_id'],
		'connection_id' => (int)$principal['connection_id'],
		'fingerprint' => (string)$fingerprint);
	$previous = isset($_SESSION['report_access_binding']) ? $_SESSION['report_access_binding'] : null;

	if (is_array($previous) && $previous === $current)
		return;

	if (is_array($previous)) {
		if (function_exists('report_artifact_revoke_all'))
			report_artifact_revoke_all('principal_scope_changed');
		else
			unset($_SESSION['report_artifact_grants']);
		$_SESSION['report_access_epoch'] = sha1(uniqid('report-access-', true).mt_rand());
	} elseif (!isset($_SESSION['report_access_epoch']))
		$_SESSION['report_access_epoch'] = sha1(uniqid('report-access-', true).mt_rand());

	$_SESSION['report_access_binding'] = $current;
}

/**
 * Return the current saved-selection epoch.
 *
 * @return string
 */
function report_access_epoch()
{
	return isset($_SESSION['report_access_epoch']) && is_string($_SESSION['report_access_epoch'])
		? $_SESSION['report_access_epoch'] : '';
}

/**
 * Check one report-like action against the current principal.
 *
 * @param string $security_area Exact reviewed security area
 * @param string $intent One of report_access_intents()
 * @param array $options Optional report_id, company (requested value), purpose
 * @return array
 */
function report_access_check($security_area, $intent = 'report', $options = array())
{
	$options = is_array($options) ? $options : array();
	$fields = array(
		'area' => is_string($security_area) ? $security_area : '',
		'intent' => is_string($intent) ? $intent : '',
		'report_id' => isset($options['report_id']) ? $options['report_id'] : null,
	);
	if (!in_array($intent, report_access_intents(), true))
		return report_access_result(false, REPORT_ACCESS_DENIED, 'unknown_intent', $fields);
	if (!is_string($security_area) || $security_area === ''
		|| $security_area === 'SA_OPEN' || $security_area === 'SA_DENIED')
		return report_access_result(false, REPORT_ACCESS_DENIED, 'invalid_area', $fields);
	if (!report_access_area_code($security_area, $code, $section))
		return report_access_result(false, REPORT_ACCESS_DENIED, 'unknown_area', $fields);

	$principal = report_access_principal();
	$fields['actor_id'] = $principal['actor_id'];
	$fields['company_id'] = $principal['company_id'];
	$fields['connection_id'] = $principal['connection_id'];
	$fields['role_id'] = $principal['role_id'];
	$fields['prefix'] = $principal['prefix'];

	if (!is_object($principal['object']) || !$principal['logged_in']
		|| $principal['actor_id'] <= 0 || $principal['role_id'] <= 0)
		return report_access_result(false, REPORT_ACCESS_DENIED, 'not_authenticated', $fields);

	if (!method_exists($principal['object'], 'can_access_page'))
		return report_access_result(false, REPORT_ACCESS_UNKNOWN, 'session_unavailable', $fields);
	if (!$principal['object']->can_access_page($security_area))
		return report_access_result(false, REPORT_ACCESS_DENIED, 'session_denied', $fields);

	if ($principal['company_id'] < 0 || $principal['connection_id'] < 0)
		return report_access_result(false, REPORT_ACCESS_DENIED, 'company_unbound', $fields);
	if ($principal['company_id'] !== $principal['connection_id'])
		return report_access_result(false, REPORT_ACCESS_DENIED, 'company_connection_mismatch', $fields);
	if (array_key_exists('company', $options) && $options['company'] !== null) {
		if (!is_scalar($options['company']) || !preg_match('/^-?[0-9]{1,10}$/D', (string)$options['company'])
			|| (int)$options['company'] !== $principal['company_id'])
			return report_access_result(false, REPORT_ACCESS_DENIED, 'company_mismatch', $fields);
	}
	if (defined('SS_SADMIN') && $principal['company_id'] !== 0 && $section === SS_SADMIN)
		return report_access_result(false, REPORT_ACCESS_DENIED, 'site_admin_company_only', $fields);

	// A database upgrade or an old-style role source has no current role row to read.
	if ($principal['old_db']) {
		$fields['legacy_session'] = true;
		return report_access_result(true, REPORT_ACCESS_ALLOWED, 'legacy_upgrade_session', $fields);
	}

	if (!function_exists('get_user') || !function_exists('get_security_role'))
		return report_access_result(false, REPORT_ACCESS_UNKNOWN, 'fresh_state_unavailable', $fields);
	$user = report_access_user_row($principal['actor_id']);
	if (!is_array($user) || !isset($user['role_id']))
		return report_access_result(false, REPORT_ACCESS_UNKNOWN, 'unknown_user', $fields);
	if (!empty($user['inactive']))
		return report_access_result(false, REPORT_ACCESS_DENIED, 'inactive_user', $fields);
	if ((int)$user['role_id'] !== $principal['role_id'])
		return report_access_result(false, REPORT_ACCESS_DENIED, 'session_role_mismatch', $fields);

	$role = report_access_role_row($principal['role_id']);
	if (!is_array($role))
		return report_access_result(false, REPORT_ACCESS_UNKNOWN, 'unknown_role', $fields);
	if (!empty($role['inactive']))
		return report_access_result(false, REPORT_ACCESS_DENIED, 'inactive_role', $fields);
	$role_areas = isset($role['areas']) ? report_access_code_list($role['areas']) : array();
	$role_sections = isset($role['sections']) ? report_access_code_list($role['sections']) : array();
	if (!in_array((string)$code, $role_areas, true))
		return report_access_result(false, REPORT_ACCESS_DENIED, 'role_grant_missing', $fields);
	if (!in_array((string)$section, $role_sections, true))
		return report_access_result(false, REPORT_ACCESS_DENIED, 'role_section_missing', $fields);

	$session_codes = isset($principal['object']->role_set) && is_array($principal['object']->role_set)
		? report_access_code_list($principal['object']->role_set) : array();
	if (count($session_codes) > 0 && !in_array((string)$code, $session_codes, true))
		return report_access_result(false, REPORT_ACCESS_DENIED, 'stale_session_grant', $fields);

	if (isset($GLOBALS['SysPrefs']) && is_object($GLOBALS['SysPrefs'])
		&& property_exists($GLOBALS['SysPrefs'], 'db_ok') && !$GLOBALS['SysPrefs']->db_ok
		&& $security_area !== 'SA_SOFTWAREUPGRADE')
		return report_access_result(false, REPORT_ACCESS_DISABLED, 'system_disabled', $fields);

	$fields['fingerprint'] = report_access_fingerprint_for($principal, $role, array());
	report_access_bind_session($principal, $fields['fingerprint']);
	return report_access_result(true, REPORT_ACCESS_ALLOWED, 'permitted', $fields);
}

/**
 * Check that every required area is currently authorized.
 *
 * @param array $areas
 * @param string $intent
 * @param array $options
 * @return array
 */
function report_access_check_all($areas, $intent = 'report', $options = array())
{
	$areas = is_array($areas) ? array_values($areas) : array();
	if (count($areas) === 0)
		return report_access_result(false, REPORT_ACCESS_DENIED, 'unknown_area', array('intent' => $intent));
	$fingerprints = array();
	$last = null;
	foreach ($areas as $area) {
		$scope = report_access_check($area, $intent, $options);
		if (!$scope['allowed'])
			return $scope;
		$fingerprints[] = $scope['fingerprint'];
		$last = $scope;
	}
	$scope = is_array($last) ? $last : report_access_result(true, REPORT_ACCESS_ALLOWED, 'permitted',
		array('intent' => $intent));
	$scope['areas'] = $areas;
	// Every permitted area carries the same principal/role/company grant
	// fingerprint. Keeping that one value avoids rotating the session binding
	// between the two required areas (which would revoke outstanding artifacts).
	$scope['fingerprint'] = count($fingerprints) > 0 ? $fingerprints[0] : '';
	return $scope;
}

/**
 * Check that at least one reviewed area is currently authorized.
 *
 * Used by the report center where a document print report legitimately has a
 * single-document area or a bulk area. The execution gate always selects the
 * exact one from the registry selector.
 *
 * @param array $areas
 * @param string $intent
 * @param array $options
 * @return array
 */
function report_access_check_any($areas, $intent = 'report', $options = array())
{
	$areas = is_array($areas) ? array_values($areas) : array();
	if (count($areas) === 0)
		return report_access_result(false, REPORT_ACCESS_DENIED, 'unknown_area', array('intent' => $intent));
	$first = null;
	foreach ($areas as $area) {
		$scope = report_access_check($area, $intent, $options);
		if ($scope['allowed']) {
			$scope['areas'] = $areas;
			return $scope;
		}
		if ($first === null)
			$first = $scope;
	}
	return $first === null ? report_access_result(false, REPORT_ACCESS_DENIED, 'unknown_area',
		array('intent' => $intent)) : $first;
}

/**
 * Can the current principal open one native report from the report center?
 *
 * @param mixed $report_id
 * @param string|null $source_file
 * @return bool
 */
function report_access_can_open_report($report_id, $source_file = null)
{
	$entry = report_access_report_entry($report_id, $source_file);
	if ($entry === false || !isset($entry['areas']) || !is_array($entry['areas']))
		return false;
	$scope = report_access_check_any($entry['areas'], 'report', array('report_id' => $report_id));
	return !empty($scope['allowed']);
}

/**
 * Authorize one report redirect before the report file or any parameter is read.
 *
 * @param mixed $report_id
 * @param array $parameters Normalized PARAM_* values
 * @param string|null $source_file Resolved custom report file
 * @return array
 */
function report_access_gate_redirect($report_id, $parameters, $source_file = null)
{
	$entry = report_access_report_entry($report_id, $source_file);
	if ($entry === false) {
		$scope = report_access_result(false, REPORT_ACCESS_DENIED, 'unknown_report',
			array('report_id' => $report_id, 'intent' => 'report'));
		report_access_log_event('report_open', $scope, 'denied');
		return $scope;
	}
	$area = report_access_report_selected_area($report_id, $parameters, $source_file);
	if ($area === false) {
		$scope = report_access_result(false, REPORT_ACCESS_DENIED, 'ambiguous_report_area',
			array('report_id' => $report_id, 'intent' => 'report'));
		report_access_log_event('report_open', $scope, 'denied');
		return $scope;
	}
	$scope = report_access_check($area, 'report', array('report_id' => $report_id));
	report_access_log_event('report_open', $scope, $scope['allowed'] ? 'allowed' : 'denied');
	return $scope;
}

/**
 * Authorize one directly requested native report script.
 *
 * @param string $script_filename
 * @param string $page_security
 * @return array|null Null when the script is not a report script
 */
function report_access_gate_script($script_filename, $page_security)
{
	if (!report_access_request_is_report_script($script_filename))
		return null;
	$scope = report_access_check($page_security, 'report', array(
		'report_id' => report_access_report_script_id($script_filename)));
	report_access_log_event('report_direct_open', $scope, $scope['allowed'] ? 'allowed' : 'denied');
	return $scope;
}

/**
 * Enforce the report-script gate; denials terminate before report output/data.
 *
 * @param string $script_filename
 * @param string $page_security
 * @return array|null
 */
function report_access_enforce_report_script($script_filename, $page_security)
{
	$scope = report_access_gate_script($script_filename, $page_security);
	if ($scope !== null && !$scope['allowed'])
		report_access_emit_denial($scope);
	return $scope;
}

/**
 * Human-readable denial/disabled/unknown state.
 *
 * @param array $scope
 * @return string
 */
function report_access_denial_message($scope)
{
	$state = isset($scope['state']) ? $scope['state'] : REPORT_ACCESS_DENIED;
	if ($state === REPORT_ACCESS_DISABLED)
		return _('Report access is disabled while the database upgrade is pending.');
	if ($state === REPORT_ACCESS_UNKNOWN)
		return _('The report authorization state could not be verified.');
	return _('The security settings on your account do not permit you to access this function');
}

/**
 * Emit a generic denial and terminate without reading or rendering report data.
 *
 * @param array $scope
 * @return void
 */
function report_access_emit_denial($scope)
{
	if (!headers_sent()) {
		http_response_code(403);
		header('Cache-Control: no-store, private');
		header('X-Content-Type-Options: nosniff');
	}
	echo '<center><br><br><br><b>'.htmlspecialchars(report_access_denial_message($scope),
		ENT_QUOTES, 'UTF-8').'</b><br><br><br><br></center>';
	exit;
}

/**
 * Emit a generic bad-report-request response and terminate.
 *
 * @param string $reason
 * @return void
 */
function report_access_emit_bad_request($reason)
{
	$scope = report_access_result(false, REPORT_ACCESS_DENIED,
		preg_replace('/[^a-z0-9_]/', '', strtolower((string)$reason)));
	report_access_log_event('report_request', $scope, 'denied');
	if (!headers_sent()) {
		http_response_code(400);
		header('Cache-Control: no-store, private');
		header('X-Content-Type-Options: nosniff');
	}
	echo '<center><br><br><br><b>'._('The report request is invalid.').'</b><br><br><br><br></center>';
	exit;
}

/**
 * Write one PII-free report authorization event.
 *
 * @param string $event
 * @param array $scope
 * @param string $outcome
 * @return void
 */
function report_access_log_event($event, $scope, $outcome)
{
	$event = preg_replace('/[^a-z0-9_]/', '', strtolower((string)$event));
	$outcome = preg_replace('/[^a-z0-9_]/', '', strtolower((string)$outcome));
	$area = isset($scope['area']) ? preg_replace('/[^A-Z0-9_]/', '', (string)$scope['area']) : '';
	$report = isset($scope['report_id']) && is_scalar($scope['report_id'])
		? preg_replace('/[^a-z0-9_]/', '', strtolower((string)$scope['report_id'])) : 'unknown';
	$aggregate = isset($scope['aggregate'])
		? preg_replace('/[^a-z0-9_]/', '', strtolower((string)$scope['aggregate'])) : '';
	$reason = isset($scope['reason'])
		? preg_replace('/[^a-z0-9_]/', '', strtolower((string)$scope['reason'])) : '';
	$fingerprint = isset($scope['fingerprint']) && is_string($scope['fingerprint'])
		? substr($scope['fingerprint'], 0, 16) : '';
	error_log(sprintf(
		'[report_access] event=%s outcome=%s actor_id=%d company=%d role_id=%d area=%s report_id=%s aggregate=%s intent=%s reason=%s fingerprint=%s',
		$event === '' ? 'unknown' : $event,
		$outcome === '' ? 'unknown' : $outcome,
		isset($scope['actor_id']) ? (int)$scope['actor_id'] : 0,
		isset($scope['company_id']) ? (int)$scope['company_id'] : -1,
		isset($scope['role_id']) ? (int)$scope['role_id'] : 0,
		$area === '' ? 'none' : $area,
		$report === '' ? 'none' : $report,
		$aggregate === '' ? 'none' : $aggregate,
		isset($scope['intent']) ? $scope['intent'] : 'unknown',
		$reason === '' ? 'unknown' : $reason,
		$fingerprint === '' ? 'none' : $fingerprint
	));
}

/**
 * Return the financial/workspace aggregate requirements.
 *
 * Every aggregate is a financial report; a Dimension view grant alone is not a
 * financial report grant. The exact area is the same one the native P&L,
 * balance-sheet and trial-balance reports already require.
 *
 * @return array
 */
function report_access_aggregate_map()
{
	return array(
		'dimension_tracking_pl' => array('areas' => array('SA_DIMTRANSVIEW', 'SA_GLANALYTIC'),
			'label' => 'dimension_tracking_pl'),
		'dimension_balance_sheet_net' => array('areas' => array('SA_DIMTRANSVIEW', 'SA_GLANALYTIC'),
			'label' => 'dimension_balance_sheet_net'),
		'dimension_company_result' => array('areas' => array('SA_DIMTRANSVIEW', 'SA_GLANALYTIC'),
			'label' => 'dimension_company_result'),
	);
}

/**
 * Authorize one workspace/financial aggregate before its first query.
 *
 * @param string $aggregate Reviewed aggregate identity
 * @param array $options
 * @return array
 */
function report_access_aggregate_scope($aggregate, $options = array())
{
	$map = report_access_aggregate_map();
	if (!is_string($aggregate) || !isset($map[$aggregate])) {
		$scope = report_access_result(false, REPORT_ACCESS_UNKNOWN, 'unknown_aggregate',
			array('aggregate' => is_scalar($aggregate) ? $aggregate : '', 'intent' => 'workspace'));
		report_access_log_event('report_aggregate', $scope, 'denied');
		return $scope;
	}
	$scope = report_access_check_all($map[$aggregate]['areas'], 'workspace', $options);
	$scope['aggregate'] = $aggregate;
	$scope['areas'] = $map[$aggregate]['areas'];
	report_access_log_event('report_aggregate', $scope, $scope['allowed'] ? 'allowed' : 'denied');
	return $scope;
}

/**
 * Return the explicit user-visible state for an unauthorized aggregate.
 *
 * @param array $scope
 * @return string
 */
function report_access_aggregate_state_text($scope)
{
	$state = isset($scope['state']) ? $scope['state'] : REPORT_ACCESS_DENIED;
	if ($state === REPORT_ACCESS_DISABLED)
		return _('Financial aggregates are unavailable while the database upgrade is pending.');
	if ($state === REPORT_ACCESS_UNKNOWN)
		return _('Financial aggregate authorization could not be verified.');
	return _('Financial aggregates are not permitted for your role.');
}

/**
 * Render the denied/disabled/unknown aggregate state without any total.
 *
 * @param array $scope
 * @return void
 */
function report_access_render_aggregate_state($scope)
{
	echo "<div class='err_msg' style='padding:8px;'>".htmlspecialchars(
		report_access_aggregate_state_text($scope), ENT_QUOTES, 'UTF-8').'</div>';
}

/**
 * Key for one saved report parameter, scoped by the live grant fingerprint.
 *
 * A revoked role, a changed company/connection or a changed grant set yields a
 * different key, so the browser's stale cookie is simply not read.
 *
 * @param mixed $report_id
 * @param int|string $index
 * @return string
 */
function report_access_selection_key($report_id, $index)
{
	$scope = function_exists('report_selection_scope')
		? report_selection_scope()
		: (isset($_SESSION['wa_current_user']->user) ? (int)$_SESSION['wa_current_user']->user : 0)
			.'_'.(isset($_SESSION['wa_current_user']->cur_con)
				? (int)$_SESSION['wa_current_user']->cur_con : 0);
	$fingerprint = report_access_current_fingerprint();
	$grant = $fingerprint === '' ? 'denied' : substr($fingerprint, 0, 16);
	$epoch = report_access_epoch();
	$epoch = $epoch === '' ? 'none' : substr($epoch, 0, 8);
	return $scope.'_'.$grant.'_'.$epoch.'_'.(int)$index;
}
