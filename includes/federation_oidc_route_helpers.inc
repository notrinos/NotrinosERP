<?php
/**********************************************************************
	Shared helpers for the OIDC sign-in and step-up browser routes.

	One copy of the steps that every OIDC step-up flow and browser route
	used to repeat under its own name: reading the private client and
	assurance policy for the signed-in federated session, adding the
	reauthentication hints to the provider URL, checking the returned
	nonce, sending the browser to the provider, closing a callback the
	provider answered with an error, and the no-store security headers of
	these pages. Load this file with require_once from every file that
	calls one of these functions.
***********************************************************************/
require_once(__DIR__.'/../includes/value_helpers.inc');

/* Content-Security-Policy of a step-up page whose form posts to the identity provider. */
if (!defined('FEDERATION_OIDC_ROUTE_CSP_STEP_UP'))
	define('FEDERATION_OIDC_ROUTE_CSP_STEP_UP', "default-src 'self'; frame-ancestors 'none'; base-uri 'none'; form-action 'self' https:");
/* Content-Security-Policy of a route that only redirects or answers with a short page. */
if (!defined('FEDERATION_OIDC_ROUTE_CSP_REDIRECT'))
	define('FEDERATION_OIDC_ROUTE_CSP_REDIRECT', "default-src 'none'; frame-ancestors 'none'; base-uri 'none'; form-action 'self'");
/* Content-Security-Policy of a page whose forms post back to this application only. */
if (!defined('FEDERATION_OIDC_ROUTE_CSP_LOCAL_FORM'))
	define('FEDERATION_OIDC_ROUTE_CSP_LOCAL_FORM', "default-src 'self'; frame-ancestors 'none'; base-uri 'none'; form-action 'self'");

/**
 * Read and check the private OIDC client and assurance policy of the signed-in federated session.
 *
 * The client must belong to the session's issuer and audience and have exactly one redirect URI
 * whose path is the required callback path (no query, no fragment).
 *
 * @param int    $company         Company number.
 * @param array  $session_context Federated session context with trust_material_ref_hash, issuer_hash,
 *                                audience_hash and verifier_config_id.
 * @param string $required_path   Absolute callback path, for example /hrm/manage/identifier_supersession.php.
 * @return array|false array('redirect_uri', 'client', 'policy', 'materials'), or false when anything does not match.
 */
function federation_oidc_step_up_material_context($company, $session_context, $required_path)
{
	$company = (int)$company;
	if ($company < 0 || !is_array($session_context) || !isset($session_context['trust_material_ref_hash'], $session_context['issuer_hash'],
		$session_context['audience_hash'], $session_context['verifier_config_id']) || !is_string($required_path)
		|| strlen($required_path) < 1 || strlen($required_path) > 1024 || $required_path[0] !== '/'
		|| strpos($required_path, '?') !== false || strpos($required_path, '#') !== false
		|| preg_match('/[\x00-\x20\x7f]/', $required_path))
		return false;
	if (!function_exists('is_sha256_hex')
		|| !is_sha256_hex((string)$session_context['trust_material_ref_hash'])
		|| !is_sha256_hex((string)$session_context['issuer_hash'])
		|| !is_sha256_hex((string)$session_context['audience_hash']))
		return false;
	$bundle = federation_control_load_bundle($company, (string)$session_context['trust_material_ref_hash'], 'oidc', 'oidc_code_pkce_v1');
	if (!is_array($bundle) || !isset($bundle['materials']) || !is_array($bundle['materials'])
		|| !isset($bundle['materials']['oidc_client_config'], $bundle['materials']['oidc_assurance_policy']))
		return false;
	$materials = $bundle['materials'];
	$client = federation_oidc_client_config($materials['oidc_client_config']);
	$policy = federation_oidc_assurance_policy($materials['oidc_assurance_policy']);
	if ($client === false || $policy === false
		|| !federation_control_hash_equals(hash('sha256', $client['issuer']), (string)$session_context['issuer_hash'])
		|| !federation_control_hash_equals(hash('sha256', $client['client_id']), (string)$session_context['audience_hash']))
		return false;
	$matches = array();
	foreach ($client['redirect_uris'] as $uri) {
		$parts = @parse_url($uri);
		$path = is_array($parts) && isset($parts['path']) ? (string)$parts['path'] : '/';
		if ($path === $required_path && !isset($parts['query']) && !isset($parts['fragment']))
			$matches[] = $uri;
	}
	if (count($matches) !== 1)
		return false;
	return array('redirect_uri'=>$matches[0], 'client'=>$client, 'policy'=>$policy, 'materials'=>$materials);
}

/**
 * Add the server-derived reauthentication hints (prompt=login, max_age, acr_values) to an HTTPS authorization URL.
 *
 * @param string $authorization_url Authorization URL built by the accepted transaction issuer.
 * @param array  $policy            Assurance policy with required_acr and max_auth_age_seconds.
 * @return string|false The URL with the hints, or false when the URL already carries one of them or is not HTTPS.
 */
function federation_oidc_step_up_authorization_url($authorization_url, $policy)
{
	if (!is_string($authorization_url) || !is_array($policy) || !isset($policy['required_acr'], $policy['max_auth_age_seconds'])
		|| federation_oidc_assurance_policy(json_encode($policy)) === false)
		return false;
	$parts = @parse_url($authorization_url);
	if (!is_array($parts) || !isset($parts['scheme'], $parts['host'], $parts['query'])
		|| strtolower((string)$parts['scheme']) !== 'https' || isset($parts['fragment']))
		return false;
	$query = array();
	parse_str((string)$parts['query'], $query);
	foreach (array('prompt','max_age','acr_values') as $key)
		if (array_key_exists($key, $query))
			return false;
	$query['prompt'] = 'login';
	$query['max_age'] = (string)(int)$policy['max_auth_age_seconds'];
	$query['acr_values'] = (string)$policy['required_acr'];
	$authority = (isset($parts['user']) ? rawurlencode($parts['user']).(isset($parts['pass']) ? ':'.rawurlencode($parts['pass']) : '').'@' : '')
		.$parts['host'].(isset($parts['port']) ? ':'.(int)$parts['port'] : '');
	$path = isset($parts['path']) ? $parts['path'] : '';
	return $parts['scheme'].'://'.$authority.$path.'?'.http_build_query($query, '', '&', PHP_QUERY_RFC3986);
}

/**
 * Read the nonce of an ID token and check it against the stored nonce hash, before the pinned verifier authenticates the token.
 *
 * @param string $jwt                 ID token returned by the provider.
 * @param string $expected_nonce_hash Lower-case SHA-256 hex of the nonce sent with the authorization request.
 * @return string|false The nonce, or false when it is missing, malformed or does not match.
 */
function federation_oidc_step_up_nonce($jwt, $expected_nonce_hash)
{
	if (!is_string($expected_nonce_hash) || preg_match('/^[a-f0-9]{64}$/D', $expected_nonce_hash) !== 1
		|| !function_exists('federation_oidc_token_verifier_parse'))
		return false;
	$parsed = federation_oidc_token_verifier_parse($jwt);
	if (!is_array($parsed) || !isset($parsed['claims']['nonce']) || !is_string($parsed['claims']['nonce'])
		|| preg_match('/^[A-Za-z0-9_-]{32,128}$/D', $parsed['claims']['nonce']) !== 1)
		return false;
	$actual = hash('sha256', $parsed['claims']['nonce']);
	return federation_control_hash_equals($expected_nonce_hash, $actual) ? $parsed['claims']['nonce'] : false;
}

/**
 * Send the browser to the identity provider with a 303 redirect.
 *
 * @param string $authorization_url Absolute HTTPS authorization URL.
 * @return bool false when the URL is not a valid HTTPS URI or headers were already sent.
 */
function federation_oidc_redirect_to_provider($authorization_url)
{
	if (!function_exists('federation_oidc_valid_https_uri') || !federation_oidc_valid_https_uri($authorization_url, true) || headers_sent())
		return false;
	header('HTTP/1.1 303 See Other');
	header('Location: '.$authorization_url);
	return true;
}

/**
 * Close the pending callback of a provider that answered with an error, so the state cannot be used again.
 *
 * access_denied closes it as exchange_rejected; any other error as exchange_uncertain.
 *
 * @param int    $company Company number.
 * @param string $state   State returned by the provider.
 * @param string $error   OAuth error code returned by the provider.
 * @return bool true when the callback was closed.
 */
function federation_oidc_terminalize_callback_error($company, $state, $error)
{
	if (!function_exists('federation_oidc_acquire_callback_claim'))
		require_once(dirname(__FILE__).DIRECTORY_SEPARATOR.'federation_oidc_callback_claim.inc');
	$claim = federation_oidc_acquire_callback_claim((int)$company, $state, 15);
	if (!is_array($claim) || !isset($claim['claim_token']))
		return false;
	$reason = $error === 'access_denied' ? 'exchange_rejected' : 'exchange_uncertain';
	return federation_oidc_terminalize_callback_claim((int)$company, (string)$claim['claim_token'], $reason) !== false;
}

/**
 * Send the no-store, no-referrer, no-framing security headers of an OIDC route page.
 *
 * @param string $content_security_policy One of the FEDERATION_OIDC_ROUTE_CSP_* policies.
 * @return bool false when headers were already sent.
 */
function federation_oidc_route_security_headers($content_security_policy)
{
	if (headers_sent())
		return false;
	header('Cache-Control: no-store, max-age=0');
	header('Pragma: no-cache');
	header('Referrer-Policy: no-referrer');
	header('X-Frame-Options: DENY');
	header('Content-Security-Policy: '.$content_security_policy);
	header('Permissions-Policy: geolocation=(), microphone=(), camera=()');
	header('X-Content-Type-Options: nosniff');
	return true;
}
