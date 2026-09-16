<?php
/** HRM-ESS-001 reauthenticated, CSRF-protected current-user self-change browser boundary. */
if (!defined('HRM_ESS_SELF_CHANGE_ROUTE_DB_VERSION')) define('HRM_ESS_SELF_CHANGE_ROUTE_DB_VERSION','1.0.450');
if (!defined('HRM_ESS_SELF_CHANGE_ROUTE_ATTEMPT_KEY')) define('HRM_ESS_SELF_CHANGE_ROUTE_ATTEMPT_KEY','hrm_ess_self_change_route_attempts');
if (!defined('HRM_ESS_SELF_CHANGE_ROUTE_MAX_ATTEMPTS')) define('HRM_ESS_SELF_CHANGE_ROUTE_MAX_ATTEMPTS',5);
if (!defined('HRM_ESS_SELF_CHANGE_ROUTE_ATTEMPT_WINDOW')) define('HRM_ESS_SELF_CHANGE_ROUTE_ATTEMPT_WINDOW',300);

function hrm_ess_self_change_route_ready()
{
    return function_exists('get_company_pref')
        && version_compare((string)get_company_pref('version_id', true), HRM_ESS_SELF_CHANGE_ROUTE_DB_VERSION, '>=');
}

function hrm_ess_self_change_route_authenticated()
{
    return isset($_SESSION['wa_current_user']) && is_object($_SESSION['wa_current_user'])
        && method_exists($_SESSION['wa_current_user'],'logged_in') && $_SESSION['wa_current_user']->logged_in()
        && function_exists('hrm_ess_self_change_user_can_request') && hrm_ess_self_change_user_can_request()
        && function_exists('get_hrm_ess_current_self_scope') && is_array(get_hrm_ess_current_self_scope());
}

function hrm_ess_self_change_route_security_headers()
{
    if (headers_sent()) return false;
    header('Cache-Control: no-store, no-cache, must-revalidate, max-age=0');
    header('Pragma: no-cache');
    header('Referrer-Policy: no-referrer');
    header('X-Content-Type-Options: nosniff');
    header("Content-Security-Policy: frame-ancestors 'self'");
    return true;
}

function hrm_ess_self_change_route_consume_attempt($now = null)
{
    $now = $now === null ? time() : (int)$now;
    if ($now <= 0) return false;
    $entry = isset($_SESSION[HRM_ESS_SELF_CHANGE_ROUTE_ATTEMPT_KEY]) && is_array($_SESSION[HRM_ESS_SELF_CHANGE_ROUTE_ATTEMPT_KEY])
        ? $_SESSION[HRM_ESS_SELF_CHANGE_ROUTE_ATTEMPT_KEY] : array('window_start'=>$now,'count'=>0);
    if (!isset($entry['window_start'],$entry['count']) || (int)$entry['window_start'] <= 0
        || $now-(int)$entry['window_start'] >= HRM_ESS_SELF_CHANGE_ROUTE_ATTEMPT_WINDOW)
        $entry = array('window_start'=>$now,'count'=>0);
    $entry['count'] = (int)$entry['count'] + 1;
    $_SESSION[HRM_ESS_SELF_CHANGE_ROUTE_ATTEMPT_KEY] = $entry;
    return $entry['count'] <= HRM_ESS_SELF_CHANGE_ROUTE_MAX_ATTEMPTS;
}

function hrm_ess_self_change_route_reset_attempts()
{
    unset($_SESSION[HRM_ESS_SELF_CHANGE_ROUTE_ATTEMPT_KEY]);
}

/** Reauthenticate only the already-authenticated local-password session actor. */
function hrm_ess_self_change_route_reauthenticate_local($password)
{
    if (!hrm_ess_self_change_route_authenticated() || !is_string($password) || $password === '' || strlen($password) > 512)
        return false;
    $user = $_SESSION['wa_current_user'];
    if (!isset($user->auth_method) || (string)$user->auth_method !== 'local_password')
        return false;
    $actor = function_exists('hrm_ess_self_change_actor_id') ? (int)hrm_ess_self_change_actor_id() : 0;
    if ($actor <= 0) return false;
    $result = db_query('SELECT id,password,inactive FROM '.TB_PREF.'users WHERE id='.$actor.' LIMIT 2',
        'could not reauthenticate ESS self-change actor');
    if (!$result || db_num_rows($result) !== 1) return false;
    $row = db_fetch_assoc($result);
    if (!$row || (int)$row['id'] !== $actor || (int)$row['inactive'] !== 0 || !is_string($row['password']) || $row['password'] === '')
        return false;
    if (password_verify($password, (string)$row['password'])) return true;
    if (preg_match('/^[a-f0-9]{32}$/Di', (string)$row['password']) !== 1) return false;
    $legacy = md5($password);
    return function_exists('hash_equals') ? hash_equals(strtolower((string)$row['password']), $legacy)
        : strtolower((string)$row['password']) === $legacy;
}

function hrm_ess_self_change_route_nonce()
{
    try { $bytes = random_bytes(24); } catch (Exception $e) { return false; }
    return is_string($bytes) && strlen($bytes) === 24 ? bin2hex($bytes) : false;
}

/** Return only the current actor's bounded contact/private projection. */
function hrm_ess_self_change_route_current_values()
{
    $scope = get_hrm_ess_current_self_scope();
    if (!is_array($scope)) return false;
    $current = hrm_employee_projection_build((int)$scope['employee_number']);
    if (!is_array($current)) return false;
    $out = array();
    foreach (hrm_ess_self_change_allowed_fields() as $field)
        $out[$field] = array_key_exists($field,$current) && $current[$field] !== null ? (string)$current[$field] : '';
    return $out;
}

/** Return at most one open request owned by the current session actor; no encrypted payload is exposed. */
function hrm_ess_self_change_route_open_request()
{
    $actor = hrm_ess_self_change_actor_id();
    $scope = get_hrm_ess_current_self_scope();
    if ($actor <= 0 || !is_array($scope) || (int)$scope['user_id'] !== $actor) return false;
    $result = db_query('SELECT request_id,request_status,expires_at,row_version,created_at FROM '.TB_PREF.'hrm_ess_self_change_requests '
        .'WHERE user_id='.$actor.' AND worker_id='.(int)$scope['worker_id']." AND request_status IN ('draft','approved') "
        .'AND expires_at>UTC_TIMESTAMP() ORDER BY request_id LIMIT 2', 'could not read current ESS self-change request');
    if (!$result) return false;
    if (db_num_rows($result) === 0) return null;
    if (db_num_rows($result) !== 1) return false;
    $row = db_fetch_assoc($result);
    return $row ? array('request_id'=>(int)$row['request_id'],'request_status'=>(string)$row['request_status'],
        'expires_at'=>(string)$row['expires_at'],'row_version'=>(int)$row['row_version'],'created_at'=>(string)$row['created_at']) : false;
}
