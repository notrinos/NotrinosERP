<?php
/**
 * PAY-SEC-004 public existing-link OIDC callback route policy.
 *
 * This helper contains only the HTTP/session boundary around the accepted
 * existing-link orchestration command. It never discovers a local account,
 * creates/reassigns an external-subject link, elevates assurance, accepts an
 * arbitrary redirect, or persists raw state/code/token material.
 */

if (!defined('FEDERATION_OIDC_CALLBACK_ROUTE_BINDING_KEY'))
    define('FEDERATION_OIDC_CALLBACK_ROUTE_BINDING_KEY', 'federation_oidc_existing_link_callback_bindings');
if (!defined('FEDERATION_OIDC_CALLBACK_ROUTE_ATTEMPT_KEY'))
    define('FEDERATION_OIDC_CALLBACK_ROUTE_ATTEMPT_KEY', 'federation_oidc_existing_link_callback_attempts');
if (!defined('FEDERATION_OIDC_CALLBACK_ROUTE_MAX_BINDINGS'))
    define('FEDERATION_OIDC_CALLBACK_ROUTE_MAX_BINDINGS', 4);
if (!defined('FEDERATION_OIDC_CALLBACK_ROUTE_MAX_QUERY_BYTES'))
    define('FEDERATION_OIDC_CALLBACK_ROUTE_MAX_QUERY_BYTES', 4096);
if (!defined('FEDERATION_OIDC_CALLBACK_ROUTE_MAX_CODE_BYTES'))
    define('FEDERATION_OIDC_CALLBACK_ROUTE_MAX_CODE_BYTES', 2048);
if (!defined('FEDERATION_OIDC_CALLBACK_ROUTE_MAX_ATTEMPTS'))
    define('FEDERATION_OIDC_CALLBACK_ROUTE_MAX_ATTEMPTS', 8);
if (!defined('FEDERATION_OIDC_CALLBACK_ROUTE_ATTEMPT_WINDOW'))
    define('FEDERATION_OIDC_CALLBACK_ROUTE_ATTEMPT_WINDOW', 60);

function federation_oidc_callback_route_is_anonymous_session()
{
    return isset($_SESSION['wa_current_user']) && is_object($_SESSION['wa_current_user'])
        && method_exists($_SESSION['wa_current_user'], 'logged_in')
        && !$_SESSION['wa_current_user']->logged_in();
}

function federation_oidc_callback_route_valid_state($state)
{
    return is_string($state)
        && strlen($state) >= 43 && strlen($state) <= 128
        && preg_match('/^[A-Za-z0-9_-]+$/D', $state) === 1;
}

function federation_oidc_callback_route_valid_redirect_uri($redirect_uri)
{
    if (!is_string($redirect_uri) || strlen($redirect_uri) < 9 || strlen($redirect_uri) > 2048
        || preg_match('/[\x00-\x20\x7f]/', $redirect_uri))
        return false;
    $parts = @parse_url($redirect_uri);
    return is_array($parts)
        && isset($parts['scheme'], $parts['host'], $parts['path'])
        && strtolower($parts['scheme']) === 'https'
        && $parts['host'] !== '' && $parts['path'] !== ''
        && !isset($parts['user']) && !isset($parts['pass'])
        && !isset($parts['query']) && !isset($parts['fragment']);
}

function federation_oidc_callback_route_redirect_matches_entrypoint($redirect_uri, $script_name)
{
    if (!federation_oidc_callback_route_valid_redirect_uri($redirect_uri)
        || !is_string($script_name) || $script_name === '' || $script_name[0] !== '/')
        return false;
    $parts = @parse_url($redirect_uri);
    return is_array($parts) && isset($parts['path']) && (string)$parts['path'] === $script_name;
}

function federation_oidc_callback_route_prune_bindings($now)
{
    $now = (int)$now;
    if (!isset($_SESSION[FEDERATION_OIDC_CALLBACK_ROUTE_BINDING_KEY])
        || !is_array($_SESSION[FEDERATION_OIDC_CALLBACK_ROUTE_BINDING_KEY])) {
        $_SESSION[FEDERATION_OIDC_CALLBACK_ROUTE_BINDING_KEY] = array();
        return;
    }
    foreach ($_SESSION[FEDERATION_OIDC_CALLBACK_ROUTE_BINDING_KEY] as $hash => $binding) {
        if (!is_string($hash) || preg_match('/^[a-f0-9]{64}$/D', $hash) !== 1
            || !is_array($binding) || !isset($binding['expires_at_epoch'])
            || (int)$binding['expires_at_epoch'] <= $now)
            unset($_SESSION[FEDERATION_OIDC_CALLBACK_ROUTE_BINDING_KEY][$hash]);
    }
}

/**
 * Bind an issued OIDC transaction to the current anonymous browser session.
 * Only the state hash is retained; raw state remains browser/IdP custody.
 */
function federation_oidc_callback_route_bind_pending($company, $verifier_config_id, $state, $redirect_uri, $expires_at_epoch)
{
    $company = (int)$company;
    $verifier_config_id = (int)$verifier_config_id;
    $expires_at_epoch = (int)$expires_at_epoch;
    $now = time();
    if ($company < 0 || $verifier_config_id <= 0 || $expires_at_epoch <= $now
        || $expires_at_epoch > $now + 900
        || !federation_oidc_callback_route_valid_state($state)
        || !federation_oidc_callback_route_valid_redirect_uri($redirect_uri)
        || !federation_oidc_callback_route_is_anonymous_session()
        || !isset($_SESSION['wa_current_user']->company)
        || (int)$_SESSION['wa_current_user']->company !== $company)
        return false;

    federation_oidc_callback_route_prune_bindings($now);
    $hash = hash('sha256', $state);
    $_SESSION[FEDERATION_OIDC_CALLBACK_ROUTE_BINDING_KEY][$hash] = array(
        'company_id'=>$company,
        'verifier_config_id'=>$verifier_config_id,
        'redirect_uri'=>$redirect_uri,
        'redirect_uri_hash'=>hash('sha256', $redirect_uri),
        'expires_at_epoch'=>$expires_at_epoch,
        'bound_at_epoch'=>$now
    );
    if (count($_SESSION[FEDERATION_OIDC_CALLBACK_ROUTE_BINDING_KEY]) > FEDERATION_OIDC_CALLBACK_ROUTE_MAX_BINDINGS) {
        uasort($_SESSION[FEDERATION_OIDC_CALLBACK_ROUTE_BINDING_KEY], function($a, $b) {
            $aa = isset($a['bound_at_epoch']) ? (int)$a['bound_at_epoch'] : 0;
            $bb = isset($b['bound_at_epoch']) ? (int)$b['bound_at_epoch'] : 0;
            if ($aa === $bb) return 0;
            return $aa < $bb ? -1 : 1;
        });
        while (count($_SESSION[FEDERATION_OIDC_CALLBACK_ROUTE_BINDING_KEY]) > FEDERATION_OIDC_CALLBACK_ROUTE_MAX_BINDINGS)
            array_shift($_SESSION[FEDERATION_OIDC_CALLBACK_ROUTE_BINDING_KEY]);
    }
    return true;
}

function federation_oidc_callback_route_pending_binding($state, $now)
{
    $now = (int)$now;
    if (!federation_oidc_callback_route_valid_state($state))
        return false;
    federation_oidc_callback_route_prune_bindings($now);
    $hash = hash('sha256', $state);
    if (!isset($_SESSION[FEDERATION_OIDC_CALLBACK_ROUTE_BINDING_KEY][$hash]))
        return false;
    $binding = $_SESSION[FEDERATION_OIDC_CALLBACK_ROUTE_BINDING_KEY][$hash];
    if (!is_array($binding) || !isset($binding['company_id'], $binding['verifier_config_id'], $binding['redirect_uri'], $binding['redirect_uri_hash'], $binding['expires_at_epoch'])
        || (int)$binding['company_id'] < 0 || (int)$binding['verifier_config_id'] <= 0
        || (int)$binding['expires_at_epoch'] <= $now
        || !federation_oidc_callback_route_valid_redirect_uri((string)$binding['redirect_uri'])
        || !hash_equals(hash('sha256', (string)$binding['redirect_uri']), (string)$binding['redirect_uri_hash']))
        return false;
    return $binding;
}

function federation_oidc_callback_route_clear_pending($state)
{
    if (!federation_oidc_callback_route_valid_state($state))
        return false;
    $hash = hash('sha256', $state);
    if (isset($_SESSION[FEDERATION_OIDC_CALLBACK_ROUTE_BINDING_KEY][$hash]))
        unset($_SESSION[FEDERATION_OIDC_CALLBACK_ROUTE_BINDING_KEY][$hash]);
    return true;
}

function federation_oidc_callback_route_decode_component($value)
{
    if (!is_string($value) || preg_match('/%(?![0-9A-Fa-f]{2})/', $value))
        return false;
    return rawurldecode($value);
}

/** Parse the exact callback query without using mutated/sanitized $_GET values. */
function federation_oidc_callback_route_parse_request($method, $raw_query)
{
    if ($method !== 'GET' || !is_string($raw_query) || $raw_query === ''
        || strlen($raw_query) > FEDERATION_OIDC_CALLBACK_ROUTE_MAX_QUERY_BYTES)
        return false;
    $values = array();
    foreach (explode('&', $raw_query) as $pair) {
        if ($pair === '') return false;
        $parts = explode('=', $pair, 2);
        if (count($parts) !== 2) return false;
        $key = federation_oidc_callback_route_decode_component($parts[0]);
        $value = federation_oidc_callback_route_decode_component($parts[1]);
        if ($key === false || $value === false || !in_array($key, array('state','code','error','error_description'), true)
            || isset($values[$key]))
            return false;
        $values[$key] = $value;
    }
    if (!isset($values['state']) || !federation_oidc_callback_route_valid_state($values['state']))
        return false;
    $has_code = isset($values['code']);
    $has_error = isset($values['error']);
    if ($has_code === $has_error)
        return false;
    if ($has_code) {
        if (count($values) !== 2 || !is_string($values['code']) || strlen($values['code']) < 1
            || strlen($values['code']) > FEDERATION_OIDC_CALLBACK_ROUTE_MAX_CODE_BYTES
            || preg_match('/[\x00-\x1f\x7f]/', $values['code']))
            return false;
        return array('kind'=>'code','state'=>$values['state'],'code'=>$values['code']);
    }
    if (count($values) > 3 || !is_string($values['error']) || strlen($values['error']) < 1 || strlen($values['error']) > 64
        || preg_match('/^[A-Za-z0-9_.-]+$/D', $values['error']) !== 1)
        return false;
    if (isset($values['error_description']) && (strlen($values['error_description']) > 256
        || preg_match('/[\x00-\x1f\x7f]/', $values['error_description'])))
        return false;
    return array('kind'=>'error','state'=>$values['state'],'error'=>$values['error']);
}

function federation_oidc_callback_route_consume_attempt($now)
{
    $now = (int)$now;
    $entry = isset($_SESSION[FEDERATION_OIDC_CALLBACK_ROUTE_ATTEMPT_KEY]) && is_array($_SESSION[FEDERATION_OIDC_CALLBACK_ROUTE_ATTEMPT_KEY])
        ? $_SESSION[FEDERATION_OIDC_CALLBACK_ROUTE_ATTEMPT_KEY] : array('window_start'=>$now, 'count'=>0);
    if (!isset($entry['window_start'], $entry['count']) || (int)$entry['window_start'] <= 0
        || $now - (int)$entry['window_start'] >= FEDERATION_OIDC_CALLBACK_ROUTE_ATTEMPT_WINDOW)
        $entry = array('window_start'=>$now, 'count'=>0);
    $entry['count'] = (int)$entry['count'] + 1;
    $_SESSION[FEDERATION_OIDC_CALLBACK_ROUTE_ATTEMPT_KEY] = $entry;
    return $entry['count'] <= FEDERATION_OIDC_CALLBACK_ROUTE_MAX_ATTEMPTS;
}

function federation_oidc_callback_route_reset_attempts()
{
    unset($_SESSION[FEDERATION_OIDC_CALLBACK_ROUTE_ATTEMPT_KEY]);
}

function federation_oidc_callback_route_transaction_matches_binding($state, $binding)
{
    if (!is_array($binding) || !isset($binding['verifier_config_id']) || !defined('TB_PREF')
        || !federation_oidc_callback_route_valid_state($state))
        return false;
    $state_hash = hash('sha256', $state);
    $sql = "SELECT verifier_config_id,consumed_at,expires_at FROM ".TB_PREF."user_federation_verifier_transactions WHERE transaction_key_hash=".db_escape($state_hash)." LIMIT 2";
    $result = db_query($sql, 'could not bind PAY-SEC-004 public OIDC callback to transaction');
    if (!$result) return false;
    $row = db_fetch_assoc($result);
    if (!$row || db_fetch_assoc($result) !== false)
        return false;
    return (int)$row['verifier_config_id'] === (int)$binding['verifier_config_id']
        && $row['consumed_at'] === null
        && strtotime((string)$row['expires_at'].' UTC') > time();
}

function federation_oidc_callback_route_terminalize_authorization_error($company, $state, $error)
{
    if (!function_exists('federation_oidc_acquire_callback_claim'))
        require_once(dirname(__FILE__).DIRECTORY_SEPARATOR.'federation_oidc_callback_claim.inc');
    $claim = federation_oidc_acquire_callback_claim((int)$company, $state, 15);
    if (!is_array($claim) || !isset($claim['claim_token']))
        return false;
    $reason = $error === 'access_denied' ? 'exchange_rejected' : 'exchange_uncertain';
    return federation_oidc_terminalize_callback_claim((int)$company, (string)$claim['claim_token'], $reason) !== false;
}

function federation_oidc_callback_route_security_headers()
{
    if (headers_sent()) return false;
    header('Cache-Control: no-store, max-age=0');
    header('Pragma: no-cache');
    header('Referrer-Policy: no-referrer');
    header('X-Frame-Options: DENY');
    header("Content-Security-Policy: default-src 'none'; frame-ancestors 'none'; base-uri 'none'; form-action 'none'");
    header('Permissions-Policy: geolocation=(), microphone=(), camera=()');
    header('X-Content-Type-Options: nosniff');
    return true;
}

function federation_oidc_callback_route_local_index_path($script_name, $result)
{
    if (!is_string($script_name) || substr($script_name, -strlen('/access/oidc_callback.php')) !== '/access/oidc_callback.php')
        return false;
    $base = substr($script_name, 0, -strlen('/access/oidc_callback.php'));
    $path = ($base === '' ? '' : $base).'/index.php';
    if ($result === 'authenticated') return $path;
    $fixed = array('retry'=>'retry','link_required'=>'link_required','failed'=>'failed','restart'=>'restart');
    $key = isset($fixed[$result]) ? $fixed[$result] : 'failed';
    return $path.'?federation='.$key;
}

function federation_oidc_callback_route_redirect($script_name, $result)
{
    $location = federation_oidc_callback_route_local_index_path($script_name, $result);
    if ($location === false || headers_sent()) return false;
    header('HTTP/1.1 303 See Other');
    header('Location: '.$location);
    return true;
}
