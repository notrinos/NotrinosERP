<?php
/**
 * PAY-SEC-004 scoped local break-glass security-recovery consumer foundation.
 *
 * Internal read-only context guard only. It recognizes one already-bound,
 * active break-glass durable session for explicitly allowlisted security-
 * recovery areas. It never adopts a browser session, grants an area/role,
 * creates or extends a lease, or authorizes payroll/payment/accounting work.
 */

if (!defined('BREAK_GLASS_SECURITY_RECOVERY_SCOPE'))
    define('BREAK_GLASS_SECURITY_RECOVERY_SCOPE', 'security_recovery');
if (!defined('BREAK_GLASS_SECURITY_RECOVERY_AUTH_METHOD'))
    define('BREAK_GLASS_SECURITY_RECOVERY_AUTH_METHOD', 'local_password');
if (!defined('BREAK_GLASS_SECURITY_RECOVERY_MAX_SECONDS'))
    define('BREAK_GLASS_SECURITY_RECOVERY_MAX_SECONDS', 900);

/** Load only the accepted session/activation custody needed for read-only checks. */
function break_glass_security_recovery_consumer_load_dependencies()
{
    global $path_to_root;
    $root = isset($path_to_root) ? realpath($path_to_root) : false;
    if ($root === false)
        return false;
    foreach (array('includes/session_registry.inc', 'includes/break_glass_activation.inc') as $relative) {
        $file = $root.DIRECTORY_SEPARATOR.str_replace('/', DIRECTORY_SEPARATOR, $relative);
        if (!is_file($file))
            return false;
        require_once($file);
    }
    return function_exists('session_registry_hash') && function_exists('session_registry_hash_equals')
        && function_exists('session_registry_user_agent_hash');
}

/**
 * Security recovery is intentionally finite. Existing role authority is still
 * required; these codes are not granted or added to the current role set.
 */
function break_glass_security_recovery_consumer_area($security_area)
{
    $allowed = array('SA_USERS', 'SA_CHGPASSWD');
    return is_string($security_area) && in_array($security_area, $allowed, true)
        ? $security_area : false;
}

/** Build the non-database current-session request without exposing raw secrets. */
function break_glass_security_recovery_consumer_request($security_area, $now_epoch=null)
{
    $area = break_glass_security_recovery_consumer_area($security_area);
    $now = $now_epoch === null ? time() : $now_epoch;
    if ($area === false || !is_int($now) || $now <= 0
        || !isset($_SESSION['wa_current_user']) || !is_object($_SESSION['wa_current_user'])
        || !method_exists($_SESSION['wa_current_user'], 'logged_in')
        || !$_SESSION['wa_current_user']->logged_in()
        || !isset($_SESSION['security_session_registry_id'], $_SESSION['security_session_secret'])
        || !is_string($_SESSION['security_session_secret'])
        || strlen($_SESSION['security_session_secret']) < 32
        || !break_glass_security_recovery_consumer_load_dependencies())
        return false;

    $user = $_SESSION['wa_current_user'];
    $company = function_exists('user_company') ? (int)user_company()
        : (isset($user->company) ? (int)$user->company : -1);
    $user_id = isset($user->user) ? (int)$user->user : 0;
    $role_id = isset($user->access) ? (int)$user->access : 0;
    $session_id = (int)$_SESSION['security_session_registry_id'];
    if ($company < 0 || $user_id <= 0 || $role_id <= 0 || $session_id <= 0
        || !function_exists('user_check_access') || !user_check_access($area))
        return false;

    return array(
        'company'=>$company,
        'user_id'=>$user_id,
        'role_id'=>$role_id,
        'security_area'=>$area,
        'session_id'=>$session_id,
        'session_key_hash'=>session_registry_hash($_SESSION['security_session_secret']),
        'user_agent_hash'=>session_registry_user_agent_hash(),
        'now_epoch'=>$now
    );
}

/**
 * Read exact active designation/lease/session/user/role custody. No row lock is
 * taken because this foundation never mutates state; concurrent terminalization
 * simply makes final validation fail closed.
 */
function break_glass_security_recovery_consumer_custody($session_id)
{
    $session_id = (int)$session_id;
    if ($session_id <= 0 || !defined('TB_PREF'))
        return false;
    $sql = "SELECT l.break_glass_activation_lease_id,l.break_glass_designation_id,l.user_id,l.scope_code,"
        ."l.required_auth_method,l.reason_code,l.activated_at,l.expires_at,l.session_id,l.active_slot,"
        ."l.terminalized_at,l.terminal_reason,l.row_version,"
        ."d.scope_code designation_scope_code,d.required_auth_method designation_auth_method,"
        ."d.max_activation_seconds,d.revoked_at designation_revoked_at,"
        ."s.session_key_hash,s.user_id session_user_id,s.role_id,s.auth_method,s.assurance_level,s.assurance_method,"
        ."s.created_at,s.last_seen_at,s.idle_expires_at,s.absolute_expires_at,s.user_agent_hash,s.revoked_at session_revoked_at,"
        ."u.role_id current_role_id,u.inactive user_inactive,r.inactive role_inactive "
        ."FROM ".TB_PREF."user_break_glass_activation_leases l "
        ."INNER JOIN ".TB_PREF."user_break_glass_designations d ON d.break_glass_designation_id=l.break_glass_designation_id "
        ."INNER JOIN ".TB_PREF."user_sessions s ON s.id=l.session_id AND s.user_id=l.user_id "
        ."INNER JOIN ".TB_PREF."users u ON u.id=l.user_id AND u.id=d.user_id "
        ."INNER JOIN ".TB_PREF."security_roles r ON r.id=u.role_id "
        ."WHERE l.session_id=".$session_id." LIMIT 2";
    $result = db_query($sql, 'could not read PAY-SEC-004 break-glass security-recovery consumer custody');
    if (!$result)
        return false;
    $row = db_fetch_assoc($result);
    if (!is_array($row) || db_fetch_assoc($result) !== false)
        return false;
    return $row;
}

/** Pure validator over one custody row and one current-session request. */
function break_glass_security_recovery_consumer_custody_valid($row, $request)
{
    if (!is_array($row) || !is_array($request))
        return false;
    if (!isset($row['session_user_id']))
        return false;
    $times = array();
    foreach (array('activated_at','expires_at','idle_expires_at','absolute_expires_at') as $field) {
        $epoch = strtotime((string)$row[$field].' UTC');
        if ($epoch === false)
            return false;
        $times[$field] = $epoch;
    }
    $duration = $times['expires_at'] - $times['activated_at'];
    $max_seconds = min(BREAK_GLASS_SECURITY_RECOVERY_MAX_SECONDS, (int)$row['max_activation_seconds']);
    return (int)$row['session_id'] === (int)$request['session_id']
        && (int)$row['session_user_id'] === (int)$request['user_id']
        && (int)$row['user_id'] === (int)$request['user_id']
        && (int)$row['role_id'] === (int)$request['role_id']
        && (int)$row['current_role_id'] === (int)$request['role_id']
        && (int)$row['user_inactive'] === 0 && (int)$row['role_inactive'] === 0
        && (string)$row['scope_code'] === BREAK_GLASS_SECURITY_RECOVERY_SCOPE
        && (string)$row['designation_scope_code'] === BREAK_GLASS_SECURITY_RECOVERY_SCOPE
        && (string)$row['required_auth_method'] === BREAK_GLASS_SECURITY_RECOVERY_AUTH_METHOD
        && (string)$row['designation_auth_method'] === BREAK_GLASS_SECURITY_RECOVERY_AUTH_METHOD
        && (string)$row['auth_method'] === BREAK_GLASS_SECURITY_RECOVERY_AUTH_METHOD
        && (int)$row['assurance_level'] === 1 && (string)$row['assurance_method'] === ''
        && (int)$row['active_slot'] === 1 && $row['terminalized_at'] === null
        && $row['designation_revoked_at'] === null && $row['session_revoked_at'] === null
        && $duration >= 1 && $max_seconds >= 1 && $duration <= $max_seconds
        && $times['activated_at'] <= (int)$request['now_epoch']
        && (int)$request['now_epoch'] < $times['expires_at']
        && (int)$request['now_epoch'] < $times['idle_expires_at']
        && $times['idle_expires_at'] <= $times['absolute_expires_at']
        && $times['absolute_expires_at'] === $times['expires_at']
        && session_registry_hash_equals((string)$row['session_key_hash'], (string)$request['session_key_hash'])
        && session_registry_hash_equals((string)$row['user_agent_hash'], (string)$request['user_agent_hash']);
}

/**
 * Return a restricted context only when existing role authority and active
 * break-glass custody both hold. Ticket hash/secret and profile data stay out.
 */
function break_glass_security_recovery_consumer_context($security_area, $now_epoch=null)
{
    $request = break_glass_security_recovery_consumer_request($security_area, $now_epoch);
    if (!is_array($request))
        return false;
    $row = break_glass_security_recovery_consumer_custody((int)$request['session_id']);
    if (!break_glass_security_recovery_consumer_custody_valid($row, $request))
        return false;
    $expires_epoch = strtotime((string)$row['expires_at'].' UTC');
    return array(
        'break_glass'=>true,
        'scope_code'=>BREAK_GLASS_SECURITY_RECOVERY_SCOPE,
        'security_area'=>$request['security_area'],
        'break_glass_designation_id'=>(int)$row['break_glass_designation_id'],
        'break_glass_activation_lease_id'=>(int)$row['break_glass_activation_lease_id'],
        'session_id'=>(int)$row['session_id'],
        'user_id'=>(int)$row['user_id'],
        'role_id'=>(int)$row['role_id'],
        'reason_code'=>(string)$row['reason_code'],
        'auth_method'=>BREAK_GLASS_SECURITY_RECOVERY_AUTH_METHOD,
        'assurance_level'=>1,
        'expires_at'=>(string)$row['expires_at'],
        'remaining_seconds'=>$expires_epoch - (int)$request['now_epoch'],
        'existing_role_permission_required'=>true,
        'role_granted'=>false,
        'security_area_granted'=>false,
        'browser_session_adopted'=>false
    );
}
