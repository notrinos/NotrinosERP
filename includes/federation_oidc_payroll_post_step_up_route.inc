<?php
/**
 * PAY-SEC-004 authenticated public payroll-post OIDC step-up route policy.
 *
 * This is only the HTTP/session boundary around the accepted internal payroll
 * step-up orchestrator. The browser may submit one payroll period plus the
 * one-time server-issued CSRF token. Provider/configuration, callback URI,
 * durable session, action hash and local return target remain server-derived.
 */
if (!defined('FEDERATION_OIDC_PAYROLL_STEP_UP_ROUTE_MAX_BODY_BYTES'))
    define('FEDERATION_OIDC_PAYROLL_STEP_UP_ROUTE_MAX_BODY_BYTES', 1024);
if (!defined('FEDERATION_OIDC_PAYROLL_STEP_UP_ROUTE_MAX_QUERY_BYTES'))
    define('FEDERATION_OIDC_PAYROLL_STEP_UP_ROUTE_MAX_QUERY_BYTES', 8192);
if (!defined('FEDERATION_OIDC_PAYROLL_STEP_UP_ROUTE_MAX_CODE_BYTES'))
    define('FEDERATION_OIDC_PAYROLL_STEP_UP_ROUTE_MAX_CODE_BYTES', 4096);
if (!defined('FEDERATION_OIDC_PAYROLL_STEP_UP_ROUTE_ATTEMPT_KEY'))
    define('FEDERATION_OIDC_PAYROLL_STEP_UP_ROUTE_ATTEMPT_KEY', 'federation_oidc_payroll_step_up_route_attempts');
if (!defined('FEDERATION_OIDC_PAYROLL_STEP_UP_ROUTE_MAX_ATTEMPTS'))
    define('FEDERATION_OIDC_PAYROLL_STEP_UP_ROUTE_MAX_ATTEMPTS', 8);
if (!defined('FEDERATION_OIDC_PAYROLL_STEP_UP_ROUTE_ATTEMPT_WINDOW'))
    define('FEDERATION_OIDC_PAYROLL_STEP_UP_ROUTE_ATTEMPT_WINDOW', 60);

function federation_oidc_payroll_step_up_route_authenticated()
{
    return isset($_SESSION['wa_current_user']) && is_object($_SESSION['wa_current_user'])
        && method_exists($_SESSION['wa_current_user'], 'logged_in') && $_SESSION['wa_current_user']->logged_in()
        && function_exists('user_check_access') && user_check_access('SA_PAYROLLPOST');
}

function federation_oidc_payroll_step_up_route_decode_component($value, $plus_as_space)
{
    if (!is_string($value) || preg_match('/%(?![0-9A-Fa-f]{2})/', $value))
        return false;
    if ($plus_as_space)
        $value = str_replace('+', ' ', $value);
    return rawurldecode($value);
}

/** Exact HTTPS authenticated POST start parser. */
function federation_oidc_payroll_step_up_route_parse_start($method, $content_type, $raw_body)
{
    if ($method !== 'POST' || !is_string($content_type)
        || strtolower(trim(explode(';', $content_type, 2)[0])) !== 'application/x-www-form-urlencoded'
        || !is_string($raw_body) || $raw_body === '' || strlen($raw_body) > FEDERATION_OIDC_PAYROLL_STEP_UP_ROUTE_MAX_BODY_BYTES)
        return false;
    $values = array();
    foreach (explode('&', $raw_body) as $pair) {
        if ($pair === '') return false;
        $parts = explode('=', $pair, 2);
        if (count($parts) !== 2) return false;
        $key = federation_oidc_payroll_step_up_route_decode_component($parts[0], true);
        $value = federation_oidc_payroll_step_up_route_decode_component($parts[1], true);
        if ($key === false || $value === false || !in_array($key, array('period_id','step_up_csrf'), true) || isset($values[$key]))
            return false;
        $values[$key] = $value;
    }
    if (count($values) !== 2 || !isset($values['period_id'], $values['step_up_csrf'])
        || preg_match('/^[1-9][0-9]{0,9}$/D', (string)$values['period_id']) !== 1
        || preg_match('/^[A-Za-z0-9_-]{43}$/D', (string)$values['step_up_csrf']) !== 1)
        return false;
    $period_id = (int)$values['period_id'];
    if ($period_id <= 0 || (string)$period_id !== (string)$values['period_id'])
        return false;
    return array('period_id'=>$period_id, 'csrf'=>(string)$values['step_up_csrf']);
}

/** Exact callback parser; browser-selected company/period/return values are forbidden. */
function federation_oidc_payroll_step_up_route_parse_callback($method, $raw_query)
{
    if ($method !== 'GET' || !is_string($raw_query) || $raw_query === ''
        || strlen($raw_query) > FEDERATION_OIDC_PAYROLL_STEP_UP_ROUTE_MAX_QUERY_BYTES)
        return false;
    $values = array();
    foreach (explode('&', $raw_query) as $pair) {
        if ($pair === '') return false;
        $parts = explode('=', $pair, 2);
        if (count($parts) !== 2) return false;
        $key = federation_oidc_payroll_step_up_route_decode_component($parts[0], false);
        $value = federation_oidc_payroll_step_up_route_decode_component($parts[1], false);
        if ($key === false || $value === false || !in_array($key, array('state','code','error','error_description'), true)
            || isset($values[$key]))
            return false;
        $values[$key] = $value;
    }
    if (!isset($values['state']) || preg_match('/^[A-Za-z0-9_-]{43}$/D', (string)$values['state']) !== 1)
        return false;
    $has_code = isset($values['code']);
    $has_error = isset($values['error']);
    if ($has_code === $has_error)
        return false;
    if ($has_code) {
        if (count($values) !== 2 || !is_string($values['code']) || strlen($values['code']) < 1
            || strlen($values['code']) > FEDERATION_OIDC_PAYROLL_STEP_UP_ROUTE_MAX_CODE_BYTES
            || preg_match('/[\x00-\x20\x7f]/', $values['code']))
            return false;
        return array('kind'=>'code','state'=>(string)$values['state'],'code'=>(string)$values['code']);
    }
    if (count($values) > 3 || !is_string($values['error']) || strlen($values['error']) < 1 || strlen($values['error']) > 64
        || preg_match('/^[A-Za-z0-9_.-]+$/D', $values['error']) !== 1)
        return false;
    if (isset($values['error_description']) && (strlen($values['error_description']) > 256
        || preg_match('/[\x00-\x1f\x7f]/', $values['error_description'])))
        return false;
    return array('kind'=>'error','state'=>(string)$values['state'],'error'=>(string)$values['error']);
}

function federation_oidc_payroll_step_up_route_consume_attempt($now = null)
{
    $now = $now === null ? time() : (int)$now;
    if ($now <= 0) return false;
    $entry = isset($_SESSION[FEDERATION_OIDC_PAYROLL_STEP_UP_ROUTE_ATTEMPT_KEY])
        && is_array($_SESSION[FEDERATION_OIDC_PAYROLL_STEP_UP_ROUTE_ATTEMPT_KEY])
        ? $_SESSION[FEDERATION_OIDC_PAYROLL_STEP_UP_ROUTE_ATTEMPT_KEY] : array('window_start'=>$now,'count'=>0);
    if (!isset($entry['window_start'], $entry['count']) || (int)$entry['window_start'] <= 0
        || $now - (int)$entry['window_start'] >= FEDERATION_OIDC_PAYROLL_STEP_UP_ROUTE_ATTEMPT_WINDOW)
        $entry = array('window_start'=>$now,'count'=>0);
    $entry['count'] = (int)$entry['count'] + 1;
    $_SESSION[FEDERATION_OIDC_PAYROLL_STEP_UP_ROUTE_ATTEMPT_KEY] = $entry;
    return $entry['count'] <= FEDERATION_OIDC_PAYROLL_STEP_UP_ROUTE_MAX_ATTEMPTS;
}

function federation_oidc_payroll_step_up_route_reset_attempts()
{
    unset($_SESSION[FEDERATION_OIDC_PAYROLL_STEP_UP_ROUTE_ATTEMPT_KEY]);
}

function federation_oidc_payroll_step_up_route_callback_path($script_name)
{
    $suffix = '/hrm/transactions/payroll_post_step_up.php';
    if (!is_string($script_name) || substr($script_name, -strlen($suffix)) !== $suffix)
        return false;
    return $script_name;
}

function federation_oidc_payroll_step_up_route_local_path($script_name, $result, $period_id = 0)
{
    $suffix = '/hrm/transactions/payroll_post_step_up.php';
    if (!is_string($script_name) || substr($script_name, -strlen($suffix)) !== $suffix)
        return false;
    $base = substr($script_name, 0, -strlen($suffix));
    $path = ($base === '' ? '' : $base).'/hrm/transactions/payroll_post.php';
    $fixed = array('assured'=>'assured','retry'=>'retry','failed'=>'failed','restart'=>'failed');
    $status = isset($fixed[$result]) ? $fixed[$result] : 'failed';
    $period_id = (int)$period_id;
    return $path.'?step_up='.$status.($period_id > 0 ? '&StepUpPeriod='.$period_id : '');
}

function federation_oidc_payroll_step_up_route_redirect_local($script_name, $result, $period_id = 0)
{
    $location = federation_oidc_payroll_step_up_route_local_path($script_name, $result, $period_id);
    if ($location === false || headers_sent()) return false;
    header('HTTP/1.1 303 See Other');
    header('Location: '.$location);
    return true;
}

function federation_oidc_payroll_step_up_route_redirect_provider($authorization_url)
{
    if (!function_exists('federation_oidc_valid_https_uri') || !federation_oidc_valid_https_uri($authorization_url, true)
        || headers_sent())
        return false;
    header('HTTP/1.1 303 See Other');
    header('Location: '.$authorization_url);
    return true;
}

function federation_oidc_payroll_step_up_route_terminalize_error($company, $state, $error)
{
    if (!function_exists('federation_oidc_acquire_callback_claim'))
        require_once(dirname(__FILE__).DIRECTORY_SEPARATOR.'federation_oidc_callback_claim.inc');
    $claim = federation_oidc_acquire_callback_claim((int)$company, $state, 15);
    if (!is_array($claim) || !isset($claim['claim_token']))
        return false;
    $reason = $error === 'access_denied' ? 'exchange_rejected' : 'exchange_uncertain';
    return federation_oidc_terminalize_callback_claim((int)$company, (string)$claim['claim_token'], $reason) !== false;
}

function federation_oidc_payroll_step_up_route_security_headers()
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
