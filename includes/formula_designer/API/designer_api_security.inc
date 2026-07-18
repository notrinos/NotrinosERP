<?php

/**
 * Return the existing page-security areas allowed to use a designer module.
 *
 * The designer currently has production hosts only in HRM and purchasing.
 * Unknown modules intentionally have no authorization path.
 *
 * @param string $module
 * @return array
 */
function formula_designer_api_allowed_areas($module)
{
	$areas = array(
		'hrm' => array('SA_EMPLOYEE', 'SA_PAYELEMENT', 'SA_SALARYSTRUCTURE'),
		'purchasing' => array('SA_VENDOREVALUATION'),
	);

	$module = strtolower(trim((string)$module));
	return isset($areas[$module]) ? $areas[$module] : array();
}

/**
 * Check the current principal against the existing designer-host permissions.
 *
 * @param string $module
 * @return bool
 */
function formula_designer_api_user_can_access($module)
{
	if (!function_exists('user_check_access'))
		return false;

	foreach (formula_designer_api_allowed_areas($module) as $area) {
		if (user_check_access($area))
			return true;
	}

	return false;
}

/**
 * Return the only request method permitted for a controller endpoint.
 *
 * @param string $endpoint
 * @return string
 */
function formula_designer_api_required_method($endpoint)
{
	$methods = array(
		'fields' => 'GET',
		'functions' => 'GET',
		'templates' => 'GET',
		'validate' => 'POST',
		'explain' => 'POST',
		'ai' => 'POST',
	);

	$endpoint = strtolower(trim((string)$endpoint));
	return isset($methods[$endpoint]) ? $methods[$endpoint] : '';
}

/**
 * Apply a per-session request ceiling to the contained designer surface.
 *
 * Metadata reads allow 120 requests per minute per endpoint; formula-bearing
 * POST operations allow 60. The bucket contains no formula, prompt, or PII.
 *
 * @param string $module
 * @param string $endpoint
 * @param string $method
 * @param int|null $now
 * @return bool
 */
function formula_designer_api_rate_limit_allowed($module, $endpoint, $method, $now = null)
{
	$now = $now === null ? time() : (int)$now;
	$method = strtoupper((string)$method);
	$limit = $method === 'POST' ? 60 : 120;
	$key = preg_replace('/[^a-z0-9_-]/', '', strtolower((string)$module))
		. ':' . preg_replace('/[^a-z0-9_-]/', '', strtolower((string)$endpoint))
		. ':' . $method;
	if (!isset($_SESSION['formula_designer_rate_limits'])
		|| !is_array($_SESSION['formula_designer_rate_limits'])) {
		$_SESSION['formula_designer_rate_limits'] = array();
	}
	if (!isset($_SESSION['formula_designer_rate_limits'][$key])
		|| !is_array($_SESSION['formula_designer_rate_limits'][$key])
		|| !isset($_SESSION['formula_designer_rate_limits'][$key]['window'])
		|| $now - (int)$_SESSION['formula_designer_rate_limits'][$key]['window'] >= 60) {
		$_SESSION['formula_designer_rate_limits'][$key] = array('window' => $now, 'count' => 0);
	}

	if ((int)$_SESSION['formula_designer_rate_limits'][$key]['count'] >= $limit)
		return false;

	$_SESSION['formula_designer_rate_limits'][$key]['count']++;
	return true;
}

/**
 * Write PII-free containment evidence for designer API access decisions.
 *
 * @param string $module
 * @param string $endpoint
 * @param string $outcome
 * @param string $method
 * @return void
 */
function formula_designer_api_log_access($module, $endpoint, $outcome, $method)
{
	$actor_id = isset($_SESSION['wa_current_user']->user)
		? (int)$_SESSION['wa_current_user']->user
		: 0;
	$company = isset($_SESSION['wa_current_user']->company)
		? (int)$_SESSION['wa_current_user']->company
		: -1;
	$normalize = function ($value) {
		$value = strtolower((string)$value);
		return preg_replace('/[^a-z0-9_-]/', '', $value);
	};

	error_log(sprintf(
		'[formula_designer_security] actor_id=%d company=%d module=%s endpoint=%s outcome=%s method=%s',
		$actor_id,
		$company,
		$normalize($module),
		$normalize($endpoint),
		$normalize($outcome),
		strtoupper($normalize($method))
	));
}

/**
 * Emit a contained JSON response without leaking bootstrap error markup.
 *
 * @param array $payload
 * @param int $status_code
 * @return void
 */
function formula_designer_api_json_response(array $payload, $status_code)
{
	global $before_box, $messages;
	$before_box = '';
	$messages = array();
	while (ob_get_level() > 0)
		ob_end_clean();

	if (!headers_sent()) {
		http_response_code((int)$status_code);
		header('Content-Type: application/json; charset=UTF-8');
		header('Cache-Control: no-store, no-cache, must-revalidate, max-age=0');
		header('X-Content-Type-Options: nosniff');
	}

	echo json_encode($payload);
	exit;
}
