<?php
/**
 * PAY-SEC-004 public recovery-redemption route policy.
 *
 * Browser claims are deliberately narrow: configured company, exact login ID,
 * and exact recovery-material ID. Email/display-name lookup, forwarded-address
 * trust, recovery/request secrets in URLs/session state, and authenticated use
 * are prohibited. All unresolved claims receive the same continuation shape.
 */
if (!defined('ACCOUNT_RECOVERY_ROUTE_CSRF_KEY'))
    define('ACCOUNT_RECOVERY_ROUTE_CSRF_KEY', 'pay_sec_004_account_recovery_route_csrf');
if (!defined('ACCOUNT_RECOVERY_ROUTE_ATTEMPT_KEY'))
    define('ACCOUNT_RECOVERY_ROUTE_ATTEMPT_KEY', 'pay_sec_004_account_recovery_route_attempts');
if (!defined('ACCOUNT_RECOVERY_ROUTE_CSRF_TTL'))
    define('ACCOUNT_RECOVERY_ROUTE_CSRF_TTL', 600);
if (!defined('ACCOUNT_RECOVERY_ROUTE_MAX_BODY_BYTES'))
    define('ACCOUNT_RECOVERY_ROUTE_MAX_BODY_BYTES', 4096);
if (!defined('ACCOUNT_RECOVERY_ROUTE_ATTEMPT_WINDOW'))
    define('ACCOUNT_RECOVERY_ROUTE_ATTEMPT_WINDOW', 60);
if (!defined('ACCOUNT_RECOVERY_ROUTE_MAX_ATTEMPTS'))
    define('ACCOUNT_RECOVERY_ROUTE_MAX_ATTEMPTS', 12);
if (!defined('ACCOUNT_RECOVERY_ROUTE_SOURCE_DOMAIN'))
    define('ACCOUNT_RECOVERY_ROUTE_SOURCE_DOMAIN', 'pay-sec-004:account-recovery:source-bucket:v1:');
if (!defined('ACCOUNT_RECOVERY_ROUTE_ACTIVE_DB_VERSION'))
    define('ACCOUNT_RECOVERY_ROUTE_ACTIVE_DB_VERSION', '1.0.277');
if (!defined('ACCOUNT_RECOVERY_ROUTE_ACTIVATION_ACTION'))
    define('ACCOUNT_RECOVERY_ROUTE_ACTIVATION_ACTION', 'security.account_recovery.public_route.activated');

function account_recovery_route_is_anonymous_session()
{
    return isset($_SESSION['wa_current_user']) && is_object($_SESSION['wa_current_user'])
        && method_exists($_SESSION['wa_current_user'], 'logged_in')
        && !$_SESSION['wa_current_user']->logged_in();
}

function account_recovery_route_random_bytes($length)
{
    $length = (int)$length;
    if ($length < 16 || $length > 64 || !function_exists('random_bytes'))
        return false;
    try { return random_bytes($length); } catch (Exception $e) { return false; }
}

function account_recovery_route_base64url($bytes)
{
    return rtrim(strtr(base64_encode($bytes), '+/', '-_'), '=');
}

function account_recovery_route_issue_csrf($now = null)
{
    if (!account_recovery_route_is_anonymous_session())
        return false;
    $now = $now === null ? time() : (int)$now;
    $bytes = account_recovery_route_random_bytes(32);
    if ($now <= 0 || $bytes === false)
        return false;
    $token = account_recovery_route_base64url($bytes);
    if (preg_match('/^[A-Za-z0-9_-]{43}$/D', $token) !== 1)
        return false;
    $_SESSION[ACCOUNT_RECOVERY_ROUTE_CSRF_KEY] = array(
        'token_hash'=>hash('sha256', $token),
        'issued_at_epoch'=>$now,
        'expires_at_epoch'=>$now + ACCOUNT_RECOVERY_ROUTE_CSRF_TTL
    );
    return $token;
}

function account_recovery_route_consume_csrf($token, $now = null)
{
    $now = $now === null ? time() : (int)$now;
    $entry = isset($_SESSION[ACCOUNT_RECOVERY_ROUTE_CSRF_KEY]) && is_array($_SESSION[ACCOUNT_RECOVERY_ROUTE_CSRF_KEY])
        ? $_SESSION[ACCOUNT_RECOVERY_ROUTE_CSRF_KEY] : false;
    unset($_SESSION[ACCOUNT_RECOVERY_ROUTE_CSRF_KEY]);
    if (!is_string($token) || preg_match('/^[A-Za-z0-9_-]{43}$/D', $token) !== 1
        || !is_array($entry) || !isset($entry['token_hash'],$entry['issued_at_epoch'],$entry['expires_at_epoch'])
        || !is_string($entry['token_hash']) || preg_match('/^[a-f0-9]{64}$/D', $entry['token_hash']) !== 1
        || (int)$entry['issued_at_epoch'] <= 0 || (int)$entry['expires_at_epoch'] <= $now
        || (int)$entry['expires_at_epoch'] > (int)$entry['issued_at_epoch'] + ACCOUNT_RECOVERY_ROUTE_CSRF_TTL)
        return false;
    return hash_equals((string)$entry['token_hash'], hash('sha256', $token));
}

function account_recovery_route_consume_attempt($now = null)
{
    $now = $now === null ? time() : (int)$now;
    if ($now <= 0) return false;
    $entry = isset($_SESSION[ACCOUNT_RECOVERY_ROUTE_ATTEMPT_KEY]) && is_array($_SESSION[ACCOUNT_RECOVERY_ROUTE_ATTEMPT_KEY])
        ? $_SESSION[ACCOUNT_RECOVERY_ROUTE_ATTEMPT_KEY] : array('window_start'=>$now,'count'=>0);
    if (!isset($entry['window_start'],$entry['count']) || (int)$entry['window_start'] <= 0
        || $now - (int)$entry['window_start'] >= ACCOUNT_RECOVERY_ROUTE_ATTEMPT_WINDOW)
        $entry = array('window_start'=>$now,'count'=>0);
    $entry['count'] = (int)$entry['count'] + 1;
    $_SESSION[ACCOUNT_RECOVERY_ROUTE_ATTEMPT_KEY] = $entry;
    return $entry['count'] <= ACCOUNT_RECOVERY_ROUTE_MAX_ATTEMPTS;
}

function account_recovery_route_reset_attempts()
{
    unset($_SESSION[ACCOUNT_RECOVERY_ROUTE_ATTEMPT_KEY]);
}

function account_recovery_route_decode_component($value)
{
    if (!is_string($value) || preg_match('/%(?![0-9A-Fa-f]{2})/', $value))
        return false;
    return rawurldecode(str_replace('+', ' ', $value));
}

function account_recovery_route_login_id_valid($value)
{
    return is_string($value) && strlen($value) >= 4 && strlen($value) <= 60
        && trim($value) === $value && preg_match('/[\x00-\x1F\x7F]/', $value) !== 1;
}

function account_recovery_route_parse_request($method, $content_type, $raw_body)
{
    if ($method !== 'POST' || !is_string($content_type)
        || strtolower(trim($content_type)) !== 'application/x-www-form-urlencoded'
        || !is_string($raw_body) || $raw_body === '' || strlen($raw_body) > ACCOUNT_RECOVERY_ROUTE_MAX_BODY_BYTES)
        return false;
    $values = array();
    foreach (explode('&', $raw_body) as $pair) {
        if ($pair === '') return false;
        $parts = explode('=', $pair, 2);
        if (count($parts) !== 2) return false;
        $key = account_recovery_route_decode_component($parts[0]);
        $value = account_recovery_route_decode_component($parts[1]);
        if ($key === false || $value === false || isset($values[$key])) return false;
        $values[$key] = $value;
    }
    if (!isset($values['action']) || !is_string($values['action'])) return false;
    if ($values['action'] === 'begin') {
        $required = array('action','company_login_name','login_id','recovery_material_id','recovery_csrf');
        if (count($values) !== count($required) || count(array_diff(array_keys($values), $required)) !== 0
            || preg_match('/^[0-9]{1,4}$/D', (string)$values['company_login_name']) !== 1
            || !account_recovery_route_login_id_valid($values['login_id'])
            || preg_match('/^[1-9][0-9]{0,19}$/D', (string)$values['recovery_material_id']) !== 1
            || preg_match('/^[A-Za-z0-9_-]{43}$/D', (string)$values['recovery_csrf']) !== 1)
            return false;
        return array('action'=>'begin','company_id'=>(int)$values['company_login_name'],
            'login_id'=>(string)$values['login_id'],'material_id'=>(string)$values['recovery_material_id'],
            'csrf'=>(string)$values['recovery_csrf']);
    }
    if ($values['action'] === 'complete') {
        $required = array('action','company_login_name','request_handle','recovery_secret','new_password','new_password_confirm','recovery_csrf');
        if (count($values) !== count($required) || count(array_diff(array_keys($values), $required)) !== 0
            || preg_match('/^[0-9]{1,4}$/D', (string)(isset($values['company_login_name']) ? $values['company_login_name'] : '')) !== 1
            || preg_match('/^rr1_[a-f0-9]{64}$/D', (string)(isset($values['request_handle']) ? $values['request_handle'] : '')) !== 1
            || preg_match('/^rc1_[a-f0-9]{64}$/D', (string)(isset($values['recovery_secret']) ? $values['recovery_secret'] : '')) !== 1
            || !is_string(isset($values['new_password']) ? $values['new_password'] : null) || strlen($values['new_password']) < 4 || strlen($values['new_password']) > 512
            || !is_string(isset($values['new_password_confirm']) ? $values['new_password_confirm'] : null) || strlen($values['new_password_confirm']) < 4 || strlen($values['new_password_confirm']) > 512
            || preg_match('/^[A-Za-z0-9_-]{43}$/D', (string)(isset($values['recovery_csrf']) ? $values['recovery_csrf'] : '')) !== 1)
            return false;
        return array('action'=>'complete','company_id'=>(int)$values['company_login_name'],
            'request_handle'=>(string)$values['request_handle'],'recovery_secret'=>(string)$values['recovery_secret'],
            'new_password'=>(string)$values['new_password'],'new_password_confirm'=>(string)$values['new_password_confirm'],
            'csrf'=>(string)$values['recovery_csrf']);
    }
    return false;
}

/** Direct socket source only: REMOTE_ADDR. Forwarded/X-Forwarded-For/X-Real-IP are intentionally ignored. */
function account_recovery_route_source_bucket_hash($remote_addr)
{
    if (!is_string($remote_addr) || $remote_addr === '') return false;
    $packed = @inet_pton($remote_addr);
    if ($packed === false) return false;
    if (strlen($packed) === 4) {
        $bucket = substr($packed, 0, 3)."\x00"; // IPv4 /24
        $family = 'v4/24:';
    } elseif (strlen($packed) === 16) {
        $bucket = substr($packed, 0, 8).str_repeat("\x00", 8); // IPv6 /64
        $family = 'v6/64:';
    } else return false;
    return hash('sha256', ACCOUNT_RECOVERY_ROUTE_SOURCE_DOMAIN.$family.bin2hex($bucket));
}

function account_recovery_route_activation_ready()
{
    if (!defined('TB_PREF') || !function_exists('db_query') || !function_exists('db_fetch_assoc')) return false;
    $version = function_exists('get_company_pref') ? get_company_pref('version_id', true) : false;
    if ((string)$version !== ACCOUNT_RECOVERY_ROUTE_ACTIVE_DB_VERSION) return false;
    $result = db_query("SELECT COUNT(*) event_count FROM ".TB_PREF."domain_audit_events WHERE entity_type='security_account_recovery_public_route_migration' AND action=".db_escape(ACCOUNT_RECOVERY_ROUTE_ACTIVATION_ACTION)." AND reason_code='pay_sec_004_recovery_redemption_public_route_foundation'", 'could not verify PAY-SEC-004 public recovery route activation');
    if (!$result) return false;
    $row = db_fetch_assoc($result);
    return is_array($row) && (int)$row['event_count'] === 1;
}

/** Exact active local-user/material claim resolver. Never accepts email/display-name claims. */
function account_recovery_route_resolve_claim($login_id, $material_id, $now_epoch = null)
{
    $now = $now_epoch === null ? time() : (int)$now_epoch;
    if (!defined('TB_PREF') || !account_recovery_route_login_id_valid($login_id)
        || preg_match('/^[1-9][0-9]{0,19}$/D', (string)$material_id) !== 1 || $now <= 0)
        return false;
    $id = (int)$material_id;
    if ($id <= 0 || (string)$id !== (string)$material_id) return false;
    $now_at = gmdate('Y-m-d H:i:s', $now);
    $sql = "SELECT u.id user_id,m.recovery_material_id FROM ".TB_PREF."users u "
        ."INNER JOIN ".TB_PREF."security_roles r ON r.id=u.role_id "
        ."INNER JOIN ".TB_PREF."user_recovery_materials m ON m.user_id=u.id "
        ."WHERE u.login_id=".db_escape($login_id)." AND u.inactive=0 AND r.inactive=0 "
        ."AND m.recovery_material_id=".$id." AND m.material_type='recovery_code' "
        ."AND m.consumed_at IS NULL AND m.revoked_at IS NULL AND m.issued_at<=".db_escape($now_at)
        ." AND m.expires_at>".db_escape($now_at)." LIMIT 2";
    $result = db_query($sql, 'could not resolve PAY-SEC-004 recovery claim');
    if (!$result) return false;
    $row = db_fetch_assoc($result);
    if (!$row || db_fetch_assoc($result)) return null;
    return array('user_id'=>(int)$row['user_id'],'recovery_material_id'=>(int)$row['recovery_material_id']);
}

function account_recovery_route_record_unresolved_claim_failure($source_bucket_hash, $now_epoch = null)
{
    $now = $now_epoch === null ? time() : (int)$now_epoch;
    if (!account_recovery_redemption_source_bucket_hash_valid($source_bucket_hash) || $now <= 0
        || !function_exists('begin_transaction') || !function_exists('commit_transaction') || !function_exists('cancel_transaction'))
        return false;
    begin_transaction();
    $committed = false;
    do {
        $source = account_recovery_redemption_lock_source_control($source_bucket_hash);
        if (!is_array($source)) break;
        if (!account_recovery_redemption_control_is_locked($source, $now)) {
            if (!account_recovery_redemption_record_failure($source, ACCOUNT_RECOVERY_REDEMPTION_SOURCE_WINDOW_SECONDS,
                    ACCOUNT_RECOVERY_REDEMPTION_SOURCE_FAILURE_THRESHOLD, ACCOUNT_RECOVERY_REDEMPTION_SOURCE_LOCKOUT_SECONDS, $now))
                break;
        }
        commit_transaction();
        $committed = true;
    } while (false);
    if (!$committed) cancel_transaction();
    return $committed;
}

function account_recovery_route_issue_request($company, $login_id, $material_id, $source_bucket_hash, $now_epoch = null)
{
    $claim = account_recovery_route_resolve_claim($login_id, $material_id, $now_epoch);
    if (!is_array($claim)) {
        account_recovery_route_record_unresolved_claim_failure($source_bucket_hash, $now_epoch);
        return false;
    }
    return account_recovery_redemption_create_request((int)$company, (int)$claim['user_id'],
        (int)$claim['recovery_material_id'], $source_bucket_hash, $now_epoch);
}

function account_recovery_route_fake_request_handle()
{
    return account_recovery_redemption_generate_request_secret();
}

function account_recovery_route_security_headers()
{
    if (headers_sent()) return;
    header('Cache-Control: no-store, no-cache, must-revalidate, max-age=0');
    header('Pragma: no-cache');
    header('Referrer-Policy: no-referrer');
    header('X-Frame-Options: DENY');
    header("Content-Security-Policy: default-src 'none'; style-src 'self'; img-src 'self'; form-action 'self'; frame-ancestors 'none'; base-uri 'none'");
    header('Permissions-Policy: camera=(), microphone=(), geolocation=()');
    header('X-Content-Type-Options: nosniff');
}

function account_recovery_route_local_index($script_name)
{
    $script_name = is_string($script_name) ? str_replace('\\','/',$script_name) : '';
    $base = preg_replace('#/access/account_recovery\.php$#D', '', $script_name);
    if (!is_string($base) || $base === $script_name || strpos($base, "\r") !== false || strpos($base, "\n") !== false)
        $base = '..';
    return rtrim($base, '/').'/index.php';
}

function account_recovery_route_escape($value)
{
    return htmlspecialchars((string)$value, ENT_QUOTES, 'UTF-8');
}
