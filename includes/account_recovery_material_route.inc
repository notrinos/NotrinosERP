<?php
/**
 * PAY-SEC-004 authenticated recovery-material re-enrollment/rotation route policy.
 *
 * This helper adds only browser/session policy around the already accepted
 * account_recovery_material_* commands. It never stores plaintext recovery
 * material in PHP session state, logs, URLs or PAY-AUD.
 */
if (!defined('ACCOUNT_RECOVERY_MATERIAL_ROUTE_ACTIVE_DB_VERSION'))
    define('ACCOUNT_RECOVERY_MATERIAL_ROUTE_ACTIVE_DB_VERSION', '1.0.278');
if (!defined('ACCOUNT_RECOVERY_MATERIAL_ROUTE_CSRF_KEY'))
    define('ACCOUNT_RECOVERY_MATERIAL_ROUTE_CSRF_KEY', 'pay_sec_004_recovery_material_route_csrf');
if (!defined('ACCOUNT_RECOVERY_MATERIAL_ROUTE_CSRF_TTL'))
    define('ACCOUNT_RECOVERY_MATERIAL_ROUTE_CSRF_TTL', 600);

function account_recovery_material_route_random_bytes($length)
{
    $length = (int)$length;
    if ($length <= 0 || !function_exists('random_bytes'))
        return false;
    try {
        return random_bytes($length);
    } catch (Exception $e) {
        return false;
    }
}

function account_recovery_material_route_base64url($bytes)
{
    if (!is_string($bytes) || $bytes === '')
        return false;
    return rtrim(strtr(base64_encode($bytes), '+/', '-_'), '=');
}

/**
 * Issue one authenticated action token. Session custody keeps only its digest,
 * the current numeric user/company binding, and bounded timestamps.
 */
function account_recovery_material_route_issue_csrf($company, $user_id, $now_epoch = null)
{
    $company = (int)$company;
    $user_id = (int)$user_id;
    $now = $now_epoch === null ? time() : $now_epoch;
    if ($company < 0 || $user_id <= 0 || !is_int($now) || $now <= 0)
        return false;
    $raw = account_recovery_material_route_random_bytes(32);
    $token = $raw === false ? false : account_recovery_material_route_base64url($raw);
    if (!is_string($token) || preg_match('/^[A-Za-z0-9_-]{43}$/D', $token) !== 1)
        return false;
    $_SESSION[ACCOUNT_RECOVERY_MATERIAL_ROUTE_CSRF_KEY] = array(
        'digest'=>hash('sha256', $token),
        'company'=>$company,
        'user_id'=>$user_id,
        'issued_at_epoch'=>$now,
        'expires_at_epoch'=>$now + ACCOUNT_RECOVERY_MATERIAL_ROUTE_CSRF_TTL
    );
    return $token;
}

/** Consume exactly one route token before any recovery-material mutation. */
function account_recovery_material_route_consume_csrf($company, $user_id, $token, $now_epoch = null)
{
    $company = (int)$company;
    $user_id = (int)$user_id;
    $now = $now_epoch === null ? time() : $now_epoch;
    $entry = isset($_SESSION[ACCOUNT_RECOVERY_MATERIAL_ROUTE_CSRF_KEY])
        && is_array($_SESSION[ACCOUNT_RECOVERY_MATERIAL_ROUTE_CSRF_KEY])
        ? $_SESSION[ACCOUNT_RECOVERY_MATERIAL_ROUTE_CSRF_KEY] : false;
    unset($_SESSION[ACCOUNT_RECOVERY_MATERIAL_ROUTE_CSRF_KEY]);
    if (!is_array($entry) || !is_string($token)
        || preg_match('/^[A-Za-z0-9_-]{43}$/D', $token) !== 1
        || $company < 0 || $user_id <= 0 || !is_int($now) || $now <= 0
        || !isset($entry['digest'], $entry['company'], $entry['user_id'], $entry['issued_at_epoch'], $entry['expires_at_epoch'])
        || (int)$entry['company'] !== $company || (int)$entry['user_id'] !== $user_id
        || (int)$entry['issued_at_epoch'] <= 0 || $now < (int)$entry['issued_at_epoch']
        || $now > (int)$entry['expires_at_epoch']
        || (int)$entry['expires_at_epoch'] > (int)$entry['issued_at_epoch'] + ACCOUNT_RECOVERY_MATERIAL_ROUTE_CSRF_TTL)
        return false;
    $expected = (string)$entry['digest'];
    $actual = hash('sha256', $token);
    return function_exists('hash_equals') ? hash_equals($expected, $actual) : $expected === $actual;
}

function account_recovery_material_route_activation_ready()
{
    if (!defined('TB_PREF') || !function_exists('db_query') || !function_exists('db_fetch_assoc'))
        return false;
    $version = function_exists('get_company_pref') ? get_company_pref('version_id', true) : false;
    if ((string)$version !== ACCOUNT_RECOVERY_MATERIAL_ROUTE_ACTIVE_DB_VERSION)
        return false;
    $result = db_query(
        "SELECT COUNT(*) event_count FROM ".TB_PREF."domain_audit_events "
        ."WHERE entity_type='security_account_recovery_reenrollment_route_migration' "
        ."AND action='security.account_recovery.reenrollment_route.activated' "
        ."AND reason_code='pay_sec_004_recovery_material_reenrollment_route_foundation'",
        'could not verify PAY-SEC-004 recovery-material re-enrollment route activation'
    );
    if (!$result)
        return false;
    $row = db_fetch_assoc($result);
    return is_array($row) && (int)$row['event_count'] === 1;
}

function account_recovery_material_route_security_headers()
{
    if (headers_sent())
        return;
    header('Cache-Control: no-store, no-cache, must-revalidate, max-age=0');
    header('Pragma: no-cache');
    header('Referrer-Policy: no-referrer');
    header('X-Frame-Options: DENY');
    header('X-Content-Type-Options: nosniff');
    header('Permissions-Policy: camera=(), microphone=(), geolocation=()');
}

function account_recovery_material_route_escape($value)
{
    return htmlspecialchars((string)$value, ENT_QUOTES, 'UTF-8');
}
