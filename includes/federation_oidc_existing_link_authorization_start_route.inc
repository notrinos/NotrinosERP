<?php
/**
 * PAY-SEC-004 public existing-link OIDC authorization-start/login-entry policy.
 *
 * The browser may select only the configured company boundary. Provider,
 * verifier configuration, callback URI and authorization endpoint are selected
 * from enabled server/private-control state. No account identifier, subject,
 * email/display name, redirect URI or verifier configuration is accepted from
 * the browser.
 */

if (!defined('FEDERATION_OIDC_AUTH_START_CSRF_KEY'))
    define('FEDERATION_OIDC_AUTH_START_CSRF_KEY', 'federation_oidc_existing_link_authorization_start_csrf');
if (!defined('FEDERATION_OIDC_AUTH_START_ATTEMPT_KEY'))
    define('FEDERATION_OIDC_AUTH_START_ATTEMPT_KEY', 'federation_oidc_existing_link_authorization_start_attempts');
if (!defined('FEDERATION_OIDC_AUTH_START_CSRF_TTL'))
    define('FEDERATION_OIDC_AUTH_START_CSRF_TTL', 600);
if (!defined('FEDERATION_OIDC_AUTH_START_MAX_BODY_BYTES'))
    define('FEDERATION_OIDC_AUTH_START_MAX_BODY_BYTES', 1024);
if (!defined('FEDERATION_OIDC_AUTH_START_MAX_ATTEMPTS'))
    define('FEDERATION_OIDC_AUTH_START_MAX_ATTEMPTS', 6);
if (!defined('FEDERATION_OIDC_AUTH_START_ATTEMPT_WINDOW'))
    define('FEDERATION_OIDC_AUTH_START_ATTEMPT_WINDOW', 60);

function federation_oidc_authorization_start_base64url($bytes)
{
    return rtrim(strtr(base64_encode($bytes), '+/', '-_'), '=');
}

function federation_oidc_authorization_start_random_bytes($length)
{
    $length = (int)$length;
    if ($length < 16 || $length > 64)
        return false;
    if (function_exists('random_bytes')) {
        try { return random_bytes($length); } catch (Exception $e) { return false; }
    }
    if (!function_exists('openssl_random_pseudo_bytes'))
        return false;
    $strong = false;
    $bytes = openssl_random_pseudo_bytes($length, $strong);
    return $strong && is_string($bytes) && strlen($bytes) === $length ? $bytes : false;
}

function federation_oidc_authorization_start_is_anonymous_session()
{
    return isset($_SESSION['wa_current_user']) && is_object($_SESSION['wa_current_user'])
        && method_exists($_SESSION['wa_current_user'], 'logged_in')
        && !$_SESSION['wa_current_user']->logged_in();
}

function federation_oidc_authorization_start_issue_csrf($now = null)
{
    if (!federation_oidc_authorization_start_is_anonymous_session())
        return false;
    $now = $now === null ? time() : (int)$now;
    if ($now <= 0)
        return false;
    $bytes = federation_oidc_authorization_start_random_bytes(32);
    if ($bytes === false)
        return false;
    $token = federation_oidc_authorization_start_base64url($bytes);
    if (preg_match('/^[A-Za-z0-9_-]{43}$/D', $token) !== 1)
        return false;
    $_SESSION[FEDERATION_OIDC_AUTH_START_CSRF_KEY] = array(
        'token_hash'=>hash('sha256', $token),
        'issued_at_epoch'=>$now,
        'expires_at_epoch'=>$now + FEDERATION_OIDC_AUTH_START_CSRF_TTL
    );
    return $token;
}

function federation_oidc_authorization_start_consume_csrf($token, $now = null)
{
    $now = $now === null ? time() : (int)$now;
    $entry = isset($_SESSION[FEDERATION_OIDC_AUTH_START_CSRF_KEY])
        && is_array($_SESSION[FEDERATION_OIDC_AUTH_START_CSRF_KEY])
        ? $_SESSION[FEDERATION_OIDC_AUTH_START_CSRF_KEY] : false;
    unset($_SESSION[FEDERATION_OIDC_AUTH_START_CSRF_KEY]);
    if (!is_string($token) || preg_match('/^[A-Za-z0-9_-]{43}$/D', $token) !== 1
        || !is_array($entry) || !isset($entry['token_hash'], $entry['issued_at_epoch'], $entry['expires_at_epoch'])
        || !is_string($entry['token_hash']) || preg_match('/^[a-f0-9]{64}$/D', $entry['token_hash']) !== 1
        || (int)$entry['issued_at_epoch'] <= 0 || (int)$entry['expires_at_epoch'] <= $now
        || (int)$entry['expires_at_epoch'] > (int)$entry['issued_at_epoch'] + FEDERATION_OIDC_AUTH_START_CSRF_TTL)
        return false;
    return hash_equals((string)$entry['token_hash'], hash('sha256', $token));
}

function federation_oidc_authorization_start_decode_component($value)
{
    if (!is_string($value) || preg_match('/%(?![0-9A-Fa-f]{2})/', $value))
        return false;
    return rawurldecode(str_replace('+', ' ', $value));
}

function federation_oidc_authorization_start_parse_request($method, $content_type, $raw_body)
{
    if ($method !== 'POST' || !is_string($content_type)
        || strtolower(trim($content_type)) !== 'application/x-www-form-urlencoded'
        || !is_string($raw_body) || $raw_body === '' || strlen($raw_body) > FEDERATION_OIDC_AUTH_START_MAX_BODY_BYTES)
        return false;
    $values = array();
    foreach (explode('&', $raw_body) as $pair) {
        if ($pair === '') return false;
        $parts = explode('=', $pair, 2);
        if (count($parts) !== 2) return false;
        $key = federation_oidc_authorization_start_decode_component($parts[0]);
        $value = federation_oidc_authorization_start_decode_component($parts[1]);
        if ($key === false || $value === false
            || !in_array($key, array('company_login_name','federation_csrf'), true)
            || isset($values[$key]))
            return false;
        $values[$key] = $value;
    }
    if (count($values) !== 2 || !isset($values['company_login_name'], $values['federation_csrf'])
        || preg_match('/^[0-9]{1,4}$/D', (string)$values['company_login_name']) !== 1
        || preg_match('/^[A-Za-z0-9_-]{43}$/D', (string)$values['federation_csrf']) !== 1)
        return false;
    return array('company_id'=>(int)$values['company_login_name'], 'csrf'=>(string)$values['federation_csrf']);
}

function federation_oidc_authorization_start_consume_attempt($now = null)
{
    $now = $now === null ? time() : (int)$now;
    $entry = isset($_SESSION[FEDERATION_OIDC_AUTH_START_ATTEMPT_KEY])
        && is_array($_SESSION[FEDERATION_OIDC_AUTH_START_ATTEMPT_KEY])
        ? $_SESSION[FEDERATION_OIDC_AUTH_START_ATTEMPT_KEY] : array('window_start'=>$now,'count'=>0);
    if (!isset($entry['window_start'], $entry['count']) || (int)$entry['window_start'] <= 0
        || $now - (int)$entry['window_start'] >= FEDERATION_OIDC_AUTH_START_ATTEMPT_WINDOW)
        $entry = array('window_start'=>$now,'count'=>0);
    $entry['count'] = (int)$entry['count'] + 1;
    $_SESSION[FEDERATION_OIDC_AUTH_START_ATTEMPT_KEY] = $entry;
    return $entry['count'] <= FEDERATION_OIDC_AUTH_START_MAX_ATTEMPTS;
}

function federation_oidc_authorization_start_reset_attempts()
{
    unset($_SESSION[FEDERATION_OIDC_AUTH_START_ATTEMPT_KEY]);
}

function federation_oidc_authorization_start_enabled_configuration()
{
    if (!defined('TB_PREF'))
        return false;
    $sql = "SELECT c.verifier_config_id,c.provider_id "
        ."FROM ".TB_PREF."user_federation_provider_verifier_configs c "
        ."INNER JOIN ".TB_PREF."user_federation_providers p ON p.provider_id=c.provider_id "
        ."WHERE p.protocol='oidc' AND p.provider_status='enabled' "
        ."AND c.verifier_profile='oidc_code_pkce_v1' AND c.config_status='enabled' "
        ."AND c.assurance_level_cap=1 ORDER BY c.verifier_config_id ASC LIMIT 2";
    $result = db_query($sql, 'could not resolve PAY-SEC-004 public OIDC authorization-start configuration');
    if (!$result)
        return false;
    $row = db_fetch_assoc($result);
    if (!$row || db_fetch_assoc($result) !== false || (int)$row['verifier_config_id'] <= 0
        || (int)$row['provider_id'] <= 0)
        return false;
    return array('verifier_config_id'=>(int)$row['verifier_config_id'], 'provider_id'=>(int)$row['provider_id']);
}

function federation_oidc_authorization_start_callback_path($script_name)
{
    if (!is_string($script_name) || substr($script_name, -strlen('/access/oidc_login.php')) !== '/access/oidc_login.php')
        return false;
    $base = substr($script_name, 0, -strlen('/access/oidc_login.php'));
    return ($base === '' ? '' : $base).'/access/oidc_callback.php';
}

function federation_oidc_authorization_start_local_index_path($script_name, $result)
{
    if (!is_string($script_name) || substr($script_name, -strlen('/access/oidc_login.php')) !== '/access/oidc_login.php')
        return false;
    $base = substr($script_name, 0, -strlen('/access/oidc_login.php'));
    $path = ($base === '' ? '' : $base).'/index.php';
    $fixed = array('restart'=>'restart','failed'=>'failed');
    $key = isset($fixed[$result]) ? $fixed[$result] : 'failed';
    return $path.'?federation='.$key;
}

function federation_oidc_authorization_start_redirect_local($script_name, $result)
{
    $location = federation_oidc_authorization_start_local_index_path($script_name, $result);
    if ($location === false || headers_sent()) return false;
    header('HTTP/1.1 303 See Other');
    header('Location: '.$location);
    return true;
}

function federation_oidc_authorization_start_redirect_provider($authorization_url)
{
    if (!function_exists('federation_oidc_valid_https_uri') || !federation_oidc_valid_https_uri($authorization_url, true)
        || headers_sent())
        return false;
    header('HTTP/1.1 303 See Other');
    header('Location: '.$authorization_url);
    return true;
}

function federation_oidc_authorization_start_security_headers()
{
    if (headers_sent()) return false;
    header('Cache-Control: no-store, max-age=0');
    header('Pragma: no-cache');
    header('Referrer-Policy: no-referrer');
    header('X-Frame-Options: DENY');
    header("Content-Security-Policy: default-src 'none'; frame-ancestors 'none'; base-uri 'none'; form-action 'self'");
    header('Permissions-Policy: geolocation=(), microphone=(), camera=()');
    header('X-Content-Type-Options: nosniff');
    return true;
}
