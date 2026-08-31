<?php
/**
 * PAY-SEC-004 recovery-redemption abuse-control command and credential-reset coordinator.
 *
 * Internal only: this file adds no HTTP route and performs no account lookup from
 * browser claims. Callers provide internal numeric user/material custody plus a
 * one-way coarse-source bucket. All expected redemption failures map to one fixed
 * external code; plaintext recovery/request secrets and source identity are never
 * persisted or audited.
 */

if (!defined('ACCOUNT_RECOVERY_REDEMPTION_REQUEST_SECRET_BYTES'))
    define('ACCOUNT_RECOVERY_REDEMPTION_REQUEST_SECRET_BYTES', 32);
if (!defined('ACCOUNT_RECOVERY_REDEMPTION_REQUEST_LIFETIME_SECONDS'))
    define('ACCOUNT_RECOVERY_REDEMPTION_REQUEST_LIFETIME_SECONDS', 600);
if (!defined('ACCOUNT_RECOVERY_REDEMPTION_ACCOUNT_WINDOW_SECONDS'))
    define('ACCOUNT_RECOVERY_REDEMPTION_ACCOUNT_WINDOW_SECONDS', 900);
if (!defined('ACCOUNT_RECOVERY_REDEMPTION_ACCOUNT_FAILURE_THRESHOLD'))
    define('ACCOUNT_RECOVERY_REDEMPTION_ACCOUNT_FAILURE_THRESHOLD', 5);
if (!defined('ACCOUNT_RECOVERY_REDEMPTION_ACCOUNT_LOCKOUT_SECONDS'))
    define('ACCOUNT_RECOVERY_REDEMPTION_ACCOUNT_LOCKOUT_SECONDS', 1800);
if (!defined('ACCOUNT_RECOVERY_REDEMPTION_SOURCE_WINDOW_SECONDS'))
    define('ACCOUNT_RECOVERY_REDEMPTION_SOURCE_WINDOW_SECONDS', 900);
if (!defined('ACCOUNT_RECOVERY_REDEMPTION_SOURCE_FAILURE_THRESHOLD'))
    define('ACCOUNT_RECOVERY_REDEMPTION_SOURCE_FAILURE_THRESHOLD', 30);
if (!defined('ACCOUNT_RECOVERY_REDEMPTION_SOURCE_LOCKOUT_SECONDS'))
    define('ACCOUNT_RECOVERY_REDEMPTION_SOURCE_LOCKOUT_SECONDS', 900);
if (!defined('ACCOUNT_RECOVERY_REDEMPTION_GENERIC_FAILURE'))
    define('ACCOUNT_RECOVERY_REDEMPTION_GENERIC_FAILURE', 'recovery_request_not_accepted');

function account_recovery_redemption_failure_result()
{
    return array('ok'=>false, 'external_code'=>ACCOUNT_RECOVERY_REDEMPTION_GENERIC_FAILURE);
}

function account_recovery_redemption_positive_id($value)
{
    if (function_exists('account_recovery_material_positive_id'))
        return account_recovery_material_positive_id($value);
    if (is_int($value))
        return $value > 0 ? $value : false;
    if (!is_string($value) || preg_match('/^[1-9][0-9]*$/D', $value) !== 1)
        return false;
    $integer = (int)$value;
    return $integer > 0 && (string)$integer === $value ? $integer : false;
}

function account_recovery_redemption_source_bucket_hash_valid($hash)
{
    return is_string($hash) && preg_match('/^[a-f0-9]{64}$/D', $hash) === 1;
}

function account_recovery_redemption_request_secret_valid($secret)
{
    return is_string($secret) && preg_match('/^rr1_[a-f0-9]{64}$/D', $secret) === 1;
}

function account_recovery_redemption_generate_request_secret()
{
    try {
        if (!function_exists('random_bytes'))
            return false;
        $bytes = random_bytes(ACCOUNT_RECOVERY_REDEMPTION_REQUEST_SECRET_BYTES);
    } catch (Exception $e) {
        return false;
    }
    return is_string($bytes) && strlen($bytes) === ACCOUNT_RECOVERY_REDEMPTION_REQUEST_SECRET_BYTES
        ? 'rr1_'.bin2hex($bytes) : false;
}

function account_recovery_redemption_request_key_hash($secret)
{
    return account_recovery_redemption_request_secret_valid($secret)
        ? hash('sha256', $secret) : false;
}

function account_recovery_redemption_control_key_hash($scope, $user_id, $material_id, $source_bucket_hash)
{
    if ($scope === 'account_material') {
        $user_id = account_recovery_redemption_positive_id($user_id);
        $material_id = account_recovery_redemption_positive_id($material_id);
        if ($user_id === false || $material_id === false)
            return false;
        return hash('sha256', 'pay-sec-004:recovery-redemption:account-material:v1:'.$user_id.':'.$material_id);
    }
    if ($scope === 'coarse_source' && account_recovery_redemption_source_bucket_hash_valid($source_bucket_hash))
        return hash('sha256', 'pay-sec-004:recovery-redemption:coarse-source:v1:'.$source_bucket_hash);
    return false;
}

function account_recovery_redemption_load_dependencies()
{
    global $path_to_root;
    if (!function_exists('account_recovery_material_consume_for_reset')) {
        $root = isset($path_to_root) ? realpath($path_to_root) : false;
        if ($root === false)
            return false;
        $file = $root.DIRECTORY_SEPARATOR.'includes'.DIRECTORY_SEPARATOR.'account_recovery_material.inc';
        if (!is_file($file))
            return false;
        require_once($file);
    }
    if (!function_exists('account_recovery_material_load_audit_gateway')
        || !account_recovery_material_load_audit_gateway())
        return false;
    return function_exists('account_recovery_material_consume_for_reset')
        && function_exists('account_recovery_material_candidate_for_update')
        && function_exists('account_recovery_material_candidate_valid')
        && function_exists('account_recovery_material_active_ids_for_update')
        && function_exists('account_recovery_material_update_active_revocation')
        && function_exists('account_recovery_material_revocation_audit')
        && function_exists('session_registry_table_exists')
        && function_exists('session_registry_revoke_user_sessions')
        && function_exists('append_domain_audit_event');
}

function account_recovery_redemption_ensure_control_row($scope, $key_hash)
{
    if (!in_array($scope, array('account_material','coarse_source'), true)
        || !account_recovery_redemption_source_bucket_hash_valid($key_hash))
        return false;
    return (bool)db_query(
        "INSERT INTO ".TB_PREF."user_recovery_redemption_controls (control_scope,control_key_hash,failed_attempt_count,row_version) VALUES ("
        .db_escape($scope).','.db_escape($key_hash).",0,1) ON DUPLICATE KEY UPDATE control_key_hash=VALUES(control_key_hash)",
        'could not ensure PAY-SEC-004 recovery-redemption control custody'
    );
}

function account_recovery_redemption_control_for_update($scope, $key_hash)
{
    $result = db_query(
        "SELECT recovery_control_id,control_scope,control_key_hash,failed_attempt_count,window_started_at,last_failed_at,locked_until,last_cleared_at,row_version "
        ."FROM ".TB_PREF."user_recovery_redemption_controls WHERE control_scope=".db_escape($scope)
        ." AND control_key_hash=".db_escape($key_hash)." LIMIT 2 FOR UPDATE",
        'could not lock PAY-SEC-004 recovery-redemption control custody'
    );
    if (!$result)
        return false;
    $row = db_fetch_assoc($result);
    if (!$row)
        return null;
    return db_fetch_assoc($result) ? false : $row;
}

/** Deterministic lock order is account_material first, then coarse_source. */
function account_recovery_redemption_lock_controls($user_id, $material_id, $source_bucket_hash)
{
    $account_key = account_recovery_redemption_control_key_hash('account_material', $user_id, $material_id, $source_bucket_hash);
    $source_key = account_recovery_redemption_control_key_hash('coarse_source', $user_id, $material_id, $source_bucket_hash);
    if ($account_key === false || $source_key === false)
        return false;
    if (!account_recovery_redemption_ensure_control_row('account_material', $account_key))
        return false;
    $account = account_recovery_redemption_control_for_update('account_material', $account_key);
    if (!is_array($account))
        return false;
    if (!account_recovery_redemption_ensure_control_row('coarse_source', $source_key))
        return false;
    $source = account_recovery_redemption_control_for_update('coarse_source', $source_key);
    if (!is_array($source))
        return false;
    return array('account_material'=>$account, 'coarse_source'=>$source);
}

function account_recovery_redemption_lock_source_control($source_bucket_hash)
{
    $source_key = account_recovery_redemption_control_key_hash('coarse_source', 1, 1, $source_bucket_hash);
    if ($source_key === false || !account_recovery_redemption_ensure_control_row('coarse_source', $source_key))
        return false;
    $source = account_recovery_redemption_control_for_update('coarse_source', $source_key);
    return is_array($source) ? $source : false;
}

function account_recovery_redemption_control_is_locked($row, $now_epoch)
{
    if (!is_array($row) || !is_int($now_epoch) || $now_epoch <= 0)
        return true;
    if ($row['locked_until'] === null)
        return false;
    $locked = strtotime((string)$row['locked_until'].' UTC');
    return $locked === false ? true : $now_epoch < $locked;
}

function account_recovery_redemption_record_failure($row, $window_seconds, $threshold, $lockout_seconds, $now_epoch)
{
    if (!is_array($row) || !is_int($now_epoch) || $now_epoch <= 0)
        return false;
    $started = $row['window_started_at'] === null ? false : strtotime((string)$row['window_started_at'].' UTC');
    $count = (int)$row['failed_attempt_count'];
    if ($started === false || $started > $now_epoch || $now_epoch - $started >= (int)$window_seconds) {
        $started = $now_epoch;
        $count = 0;
    }
    $count = min(65535, $count + 1);
    $locked_until = $count >= (int)$threshold ? gmdate('Y-m-d H:i:s', $now_epoch + (int)$lockout_seconds) : null;
    $sql = "UPDATE ".TB_PREF."user_recovery_redemption_controls SET failed_attempt_count=".$count
        .",window_started_at=".db_escape(gmdate('Y-m-d H:i:s', $started))
        .",last_failed_at=".db_escape(gmdate('Y-m-d H:i:s', $now_epoch))
        .",locked_until=".($locked_until === null ? 'NULL' : db_escape($locked_until))
        .",row_version=row_version+1 WHERE recovery_control_id=".(int)$row['recovery_control_id']
        ." AND row_version=".(int)$row['row_version'];
    return db_query($sql, 'could not record PAY-SEC-004 recovery-redemption failed attempt')
        && function_exists('db_num_affected_rows') && (int)db_num_affected_rows() === 1;
}

function account_recovery_redemption_clear_account_control($row, $now_epoch)
{
    if (!is_array($row) || (string)$row['control_scope'] !== 'account_material'
        || !is_int($now_epoch) || $now_epoch <= 0)
        return false;
    $sql = "UPDATE ".TB_PREF."user_recovery_redemption_controls SET failed_attempt_count=0,window_started_at=NULL,last_failed_at=NULL,locked_until=NULL,last_cleared_at="
        .db_escape(gmdate('Y-m-d H:i:s', $now_epoch)).",row_version=row_version+1 WHERE recovery_control_id=".(int)$row['recovery_control_id']
        ." AND row_version=".(int)$row['row_version'];
    return db_query($sql, 'could not clear PAY-SEC-004 recovery-redemption account/material control')
        && function_exists('db_num_affected_rows') && (int)db_num_affected_rows() === 1;
}

function account_recovery_redemption_request_for_update($request_secret)
{
    $hash = account_recovery_redemption_request_key_hash($request_secret);
    if ($hash === false)
        return false;
    $result = db_query(
        "SELECT recovery_request_id,request_key_hash,user_id,recovery_material_id,source_bucket_hash,request_status,created_at,expires_at,terminal_at,terminal_reason,row_version "
        ."FROM ".TB_PREF."user_recovery_redemption_requests WHERE request_key_hash=".db_escape($hash)." LIMIT 2 FOR UPDATE",
        'could not lock PAY-SEC-004 recovery-redemption request custody'
    );
    if (!$result)
        return false;
    $row = db_fetch_assoc($result);
    if (!$row)
        return null;
    return db_fetch_assoc($result) ? false : $row;
}

function account_recovery_redemption_terminalize_request($row, $status, $reason, $now_epoch)
{
    $allowed = array('failed','locked','expired','succeeded');
    if (!is_array($row) || !in_array($status, $allowed, true) || !is_string($reason) || $reason === '' || strlen($reason) > 40
        || !is_int($now_epoch) || $now_epoch <= 0 || (string)$row['request_status'] !== 'pending')
        return false;
    $sql = "UPDATE ".TB_PREF."user_recovery_redemption_requests SET request_status=".db_escape($status)
        .",terminal_at=".db_escape(gmdate('Y-m-d H:i:s', $now_epoch)).",terminal_reason=".db_escape($reason)
        .",row_version=row_version+1 WHERE recovery_request_id=".(int)$row['recovery_request_id']
        ." AND row_version=".(int)$row['row_version']." AND request_status='pending'";
    return db_query($sql, 'could not terminalize PAY-SEC-004 recovery-redemption request')
        && function_exists('db_num_affected_rows') && (int)db_num_affected_rows() === 1;
}

function account_recovery_redemption_user_for_update($user_id)
{
    $user_id = account_recovery_redemption_positive_id($user_id);
    if ($user_id === false)
        return false;
    $result = db_query(
        "SELECT u.id,u.login_id,u.inactive user_inactive,r.inactive role_inactive FROM ".TB_PREF."users u "
        ."JOIN ".TB_PREF."security_roles r ON r.id=u.role_id WHERE u.id=".$user_id." LIMIT 2 FOR UPDATE",
        'could not lock PAY-SEC-004 recovery-redemption user custody'
    );
    if (!$result)
        return false;
    $row = db_fetch_assoc($result);
    if (!$row)
        return null;
    return db_fetch_assoc($result) ? false : $row;
}

function account_recovery_redemption_password_policy_valid($password, $login_id)
{
    return is_string($password) && strlen($password) >= 4 && strlen($password) <= 512
        && is_string($login_id) && strlen($login_id) >= 4 && strlen($login_id) <= 60
        && strstr($password, $login_id) === false;
}

function account_recovery_redemption_reset_audit($company, $user_id, $material_id, $request_id, $revoked_material_count)
{
    return append_domain_audit_event(array(
        'company_id'=>(int)$company,'legal_entity_id'=>0,
        'entity_type'=>'security_account_recovery_redemption',
        'entity_key'=>array('user_id'=>(int)$user_id,'recovery_request_id'=>(int)$request_id),
        'action'=>'security.account_recovery.credential_reset',
        'reason_code'=>'pay_sec_004_recovery_redemption_coordinator',
        'before'=>array('credential_reset_committed'=>false),
        'after'=>array('credential_reset_committed'=>true),
        'restricted_diff'=>array(
            'user_id'=>(int)$user_id,'recovery_material_id'=>(int)$material_id,
            'authorization'=>'verified_recovery_material','request_secret_audited'=>false,
            'recovery_secret_audited'=>false,'source_identity_audited'=>false,
            'account_material_control_cleared'=>true,'coarse_source_control_cleared'=>false,
            'remaining_recovery_material_revoked'=>(int)$revoked_material_count,
            'durable_sessions_revoked'=>true,'federated_browser_sessions_invalidated'=>true,
            'privileged_assurance_invalidated_by_session_revocation'=>true,
            'post_reset_reenrollment_required'=>true,'federation_links_mutated'=>false,
            'assurance_history_rewritten'=>false,'public_recovery_route_added'=>false,
            'anonymous_account_lookup_added'=>false,'break_glass_authority_added'=>false,
            'role_granted'=>false,'payment_authority_changed'=>false,
            'payroll_authority_changed'=>false,'accounting_authority_changed'=>false))) !== false;
}

/**
 * Create one opaque pending request under the accepted two-scope abuse gate.
 * No account existence check or browser claim lookup occurs here.
 */
function account_recovery_redemption_create_request($company, $user_id, $material_id, $source_bucket_hash, $now_epoch = null)
{
    $company = (int)$company;
    $user_id = account_recovery_redemption_positive_id($user_id);
    $material_id = account_recovery_redemption_positive_id($material_id);
    $now = $now_epoch === null ? time() : $now_epoch;
    if ($company < 0 || $user_id === false || $material_id === false || !is_int($now) || $now <= 0
        || !account_recovery_redemption_source_bucket_hash_valid($source_bucket_hash)
        || (function_exists('user_company') && (int)user_company() !== $company)
        || !function_exists('begin_transaction') || !function_exists('commit_transaction') || !function_exists('cancel_transaction'))
        return false;
    $secret = account_recovery_redemption_generate_request_secret();
    $key_hash = $secret === false ? false : account_recovery_redemption_request_key_hash($secret);
    if ($key_hash === false)
        return false;
    begin_transaction();
    $committed = false;
    $result_value = false;
    do {
        $controls = account_recovery_redemption_lock_controls($user_id, $material_id, $source_bucket_hash);
        if (!is_array($controls))
            break;
        if (account_recovery_redemption_control_is_locked($controls['account_material'], $now)
            || account_recovery_redemption_control_is_locked($controls['coarse_source'], $now))
            break;
        $created_at = gmdate('Y-m-d H:i:s', $now);
        $expires_at = gmdate('Y-m-d H:i:s', $now + ACCOUNT_RECOVERY_REDEMPTION_REQUEST_LIFETIME_SECONDS);
        $sql = "INSERT INTO ".TB_PREF."user_recovery_redemption_requests (request_key_hash,user_id,recovery_material_id,source_bucket_hash,request_status,created_at,expires_at,row_version) VALUES ("
            .db_escape($key_hash).','.$user_id.','.$material_id.','.db_escape($source_bucket_hash).",'pending',"
            .db_escape($created_at).','.db_escape($expires_at).",1)";
        if (!db_query($sql, 'could not create PAY-SEC-004 recovery-redemption request custody'))
            break;
        commit_transaction();
        $committed = true;
        $result_value = array('ok'=>true,'request_secret'=>$secret,'expires_at'=>$expires_at,
            'request_secret_persisted'=>false,'raw_source_identity_persisted'=>false);
    } while (false);
    if (!$committed)
        cancel_transaction();
    return $result_value;
}

/**
 * One outer reset transaction over abuse gate -> consume -> revoke -> sessions ->
 * restricted PAY-AUD -> credential mutation -> exact account/material clear ->
 * request terminalization. All expected recovery failures expose one generic code.
 */
function account_recovery_redemption_reset_password($company, $request_secret, $source_bucket_hash, $recovery_secret, $new_password, $now_epoch = null)
{
    $company = (int)$company;
    $now = $now_epoch === null ? time() : $now_epoch;
    $failure = account_recovery_redemption_failure_result();
    if ($company < 0 || !is_int($now) || $now <= 0
        || !account_recovery_redemption_request_secret_valid($request_secret)
        || !account_recovery_redemption_source_bucket_hash_valid($source_bucket_hash)
        || !is_string($recovery_secret) || !preg_match('/^rc1_[a-f0-9]{64}$/D', $recovery_secret)
        || !is_string($new_password) || strlen($new_password) < 4 || strlen($new_password) > 512
        || (function_exists('user_company') && (int)user_company() !== $company)
        || !account_recovery_redemption_load_dependencies()
        || !function_exists('begin_transaction') || !function_exists('commit_transaction') || !function_exists('cancel_transaction')
        || !session_registry_table_exists())
        return $failure;

    begin_transaction();
    $committed = false;
    $result_value = $failure;
    do {
        $request = account_recovery_redemption_request_for_update($request_secret);
        if ($request === false)
            break;
        if ($request === null) {
            $source = account_recovery_redemption_lock_source_control($source_bucket_hash);
            if (!is_array($source) || account_recovery_redemption_control_is_locked($source, $now)
                || !account_recovery_redemption_record_failure($source, ACCOUNT_RECOVERY_REDEMPTION_SOURCE_WINDOW_SECONDS,
                    ACCOUNT_RECOVERY_REDEMPTION_SOURCE_FAILURE_THRESHOLD, ACCOUNT_RECOVERY_REDEMPTION_SOURCE_LOCKOUT_SECONDS, $now))
                break;
            commit_transaction();
            $committed = true;
            break;
        }
        if ((string)$request['request_status'] !== 'pending') {
            commit_transaction();
            $committed = true;
            break;
        }
        $user_id = account_recovery_redemption_positive_id((int)$request['user_id']);
        $material_id = account_recovery_redemption_positive_id((int)$request['recovery_material_id']);
        if ($user_id === false || $material_id === false)
            break;
        $controls = account_recovery_redemption_lock_controls($user_id, $material_id, $source_bucket_hash);
        if (!is_array($controls))
            break;
        $expires = strtotime((string)$request['expires_at'].' UTC');
        if ($expires === false || $now >= $expires) {
            if (!account_recovery_redemption_record_failure($controls['account_material'], ACCOUNT_RECOVERY_REDEMPTION_ACCOUNT_WINDOW_SECONDS,
                    ACCOUNT_RECOVERY_REDEMPTION_ACCOUNT_FAILURE_THRESHOLD, ACCOUNT_RECOVERY_REDEMPTION_ACCOUNT_LOCKOUT_SECONDS, $now)
                || !account_recovery_redemption_record_failure($controls['coarse_source'], ACCOUNT_RECOVERY_REDEMPTION_SOURCE_WINDOW_SECONDS,
                    ACCOUNT_RECOVERY_REDEMPTION_SOURCE_FAILURE_THRESHOLD, ACCOUNT_RECOVERY_REDEMPTION_SOURCE_LOCKOUT_SECONDS, $now)
                || !account_recovery_redemption_terminalize_request($request, 'expired', 'request_expired', $now))
                break;
            commit_transaction();
            $committed = true;
            break;
        }
        if (account_recovery_redemption_control_is_locked($controls['account_material'], $now)
            || account_recovery_redemption_control_is_locked($controls['coarse_source'], $now)) {
            if (!account_recovery_redemption_terminalize_request($request, 'locked', 'bounded_lockout_active', $now))
                break;
            commit_transaction();
            $committed = true;
            break;
        }
        if (!hash_equals((string)$request['source_bucket_hash'], $source_bucket_hash)) {
            if (!account_recovery_redemption_record_failure($controls['account_material'], ACCOUNT_RECOVERY_REDEMPTION_ACCOUNT_WINDOW_SECONDS,
                    ACCOUNT_RECOVERY_REDEMPTION_ACCOUNT_FAILURE_THRESHOLD, ACCOUNT_RECOVERY_REDEMPTION_ACCOUNT_LOCKOUT_SECONDS, $now)
                || !account_recovery_redemption_record_failure($controls['coarse_source'], ACCOUNT_RECOVERY_REDEMPTION_SOURCE_WINDOW_SECONDS,
                    ACCOUNT_RECOVERY_REDEMPTION_SOURCE_FAILURE_THRESHOLD, ACCOUNT_RECOVERY_REDEMPTION_SOURCE_LOCKOUT_SECONDS, $now)
                || !account_recovery_redemption_terminalize_request($request, 'failed', 'source_bucket_mismatch', $now))
                break;
            commit_transaction();
            $committed = true;
            break;
        }

        $user = account_recovery_redemption_user_for_update($user_id);
        if ($user === false)
            break;
        if ($user === null || (int)$user['user_inactive'] !== 0 || (int)$user['role_inactive'] !== 0) {
            if (!account_recovery_redemption_record_failure($controls['account_material'], ACCOUNT_RECOVERY_REDEMPTION_ACCOUNT_WINDOW_SECONDS,
                    ACCOUNT_RECOVERY_REDEMPTION_ACCOUNT_FAILURE_THRESHOLD, ACCOUNT_RECOVERY_REDEMPTION_ACCOUNT_LOCKOUT_SECONDS, $now)
                || !account_recovery_redemption_record_failure($controls['coarse_source'], ACCOUNT_RECOVERY_REDEMPTION_SOURCE_WINDOW_SECONDS,
                    ACCOUNT_RECOVERY_REDEMPTION_SOURCE_FAILURE_THRESHOLD, ACCOUNT_RECOVERY_REDEMPTION_SOURCE_LOCKOUT_SECONDS, $now)
                || !account_recovery_redemption_terminalize_request($request, 'failed', 'material_not_accepted', $now))
                break;
            commit_transaction();
            $committed = true;
            break;
        }
        if (!account_recovery_redemption_password_policy_valid($new_password, (string)$user['login_id']))
            break; // Request remains pending; caller may retry with an acceptable password.

        $candidate = account_recovery_material_candidate_for_update($user_id, $material_id);
        if ($candidate === false)
            break;
        if (!is_array($candidate) || !account_recovery_material_candidate_valid($candidate, $user_id, $material_id, $recovery_secret, $now)) {
            if (!account_recovery_redemption_record_failure($controls['account_material'], ACCOUNT_RECOVERY_REDEMPTION_ACCOUNT_WINDOW_SECONDS,
                    ACCOUNT_RECOVERY_REDEMPTION_ACCOUNT_FAILURE_THRESHOLD, ACCOUNT_RECOVERY_REDEMPTION_ACCOUNT_LOCKOUT_SECONDS, $now)
                || !account_recovery_redemption_record_failure($controls['coarse_source'], ACCOUNT_RECOVERY_REDEMPTION_SOURCE_WINDOW_SECONDS,
                    ACCOUNT_RECOVERY_REDEMPTION_SOURCE_FAILURE_THRESHOLD, ACCOUNT_RECOVERY_REDEMPTION_SOURCE_LOCKOUT_SECONDS, $now)
                || !account_recovery_redemption_terminalize_request($request, 'failed', 'material_not_accepted', $now))
                break;
            commit_transaction();
            $committed = true;
            break;
        }

        $attempt_context = array('scope'=>'password_reset','attempt_owner'=>'caller',
            'user_id'=>$user_id,'recovery_material_id'=>$material_id,
            'rate_limit_checked'=>true,'lockout_checked'=>true);
        $consumed = account_recovery_material_consume_for_reset($company, $user_id, $material_id, $recovery_secret, $attempt_context, $now);
        if (!is_array($consumed))
            break; // System/audit/concurrency failure: roll back; do not misclassify as bad secret.

        $remaining = account_recovery_material_active_ids_for_update($user_id);
        if (!is_array($remaining))
            break;
        $remaining_count = count($remaining);
        if ($remaining_count > 0) {
            $now_at = gmdate('Y-m-d H:i:s', $now);
            if (!account_recovery_material_update_active_revocation($user_id, $user_id, 'credential_reset', $now_at)
                || !account_recovery_material_revocation_audit($company, $user_id, $user_id, 'credential_reset', $remaining_count))
                break;
        }
        if (!session_registry_revoke_user_sessions($user_id, $user_id, 'account_recovery_password_reset'))
            break;
        if (!account_recovery_redemption_reset_audit($company, $user_id, $material_id,
                (int)$request['recovery_request_id'], $remaining_count))
            break;
        $password_hash = password_hash($new_password, PASSWORD_DEFAULT);
        if (!is_string($password_hash) || $password_hash === '')
            break;
        if (!db_query("UPDATE ".TB_PREF."users SET password=".db_escape($password_hash)." WHERE id=".$user_id,
                'could not commit PAY-SEC-004 recovery-redemption credential reset')
            || !function_exists('db_num_affected_rows') || (int)db_num_affected_rows() !== 1)
            break;
        if (!account_recovery_redemption_clear_account_control($controls['account_material'], $now))
            break;
        if (!account_recovery_redemption_terminalize_request($request, 'succeeded', 'credential_reset_committed', $now))
            break;

        commit_transaction();
        $committed = true;
        $result_value = array('ok'=>true,'external_code'=>'credential_reset_completed',
            'post_reset_reenrollment_required'=>true,'durable_sessions_revoked'=>true,
            'remaining_recovery_material_revoked'=>$remaining_count,
            'account_material_control_cleared'=>true,'coarse_source_control_cleared'=>false,
            'federation_links_mutated'=>false,'assurance_history_rewritten'=>false);
    } while (false);

    if (!$committed)
        cancel_transaction();
    return $result_value;
}
