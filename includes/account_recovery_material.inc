<?php
/**
 * PAY-SEC-004 recovery-material issuance/rotation/revocation command foundation.
 *
 * Internal-only commands over the accepted user_recovery_materials custody.
 * There is no public recovery/redemption route here. Self-service rotation is
 * limited to the currently authenticated user and requires the current local
 * credential again. SA_USERS may revoke another user's recovery material but
 * cannot issue or reveal recovery secrets for that user.
 */

if (!defined('ACCOUNT_RECOVERY_MATERIAL_TYPE'))
    define('ACCOUNT_RECOVERY_MATERIAL_TYPE', 'recovery_code');
if (!defined('ACCOUNT_RECOVERY_MATERIAL_SET_SIZE'))
    define('ACCOUNT_RECOVERY_MATERIAL_SET_SIZE', 8);
if (!defined('ACCOUNT_RECOVERY_MATERIAL_SECRET_BYTES'))
    define('ACCOUNT_RECOVERY_MATERIAL_SECRET_BYTES', 32);
if (!defined('ACCOUNT_RECOVERY_MATERIAL_LIFETIME_SECONDS'))
    define('ACCOUNT_RECOVERY_MATERIAL_LIFETIME_SECONDS', 2592000); // 30 days.
if (!defined('ACCOUNT_RECOVERY_MATERIAL_ROTATION_COOLDOWN_SECONDS'))
    define('ACCOUNT_RECOVERY_MATERIAL_ROTATION_COOLDOWN_SECONDS', 60);

function account_recovery_material_current_actor_id()
{
    if (!isset($_SESSION['wa_current_user']) || !is_object($_SESSION['wa_current_user'])
        || !method_exists($_SESSION['wa_current_user'], 'logged_in')
        || !$_SESSION['wa_current_user']->logged_in())
        return 0;
    return isset($_SESSION['wa_current_user']->user)
        ? (int)$_SESSION['wa_current_user']->user : 0;
}

function account_recovery_material_company_matches($company)
{
    $company = (int)$company;
    return $company >= 0 && function_exists('user_company')
        && (int)user_company() === $company;
}

function account_recovery_material_self_service_authorized($company, $user_id)
{
    $actor_id = account_recovery_material_current_actor_id();
    return $actor_id > 0 && $actor_id === (int)$user_id
        && account_recovery_material_company_matches($company);
}

function account_recovery_material_revoke_authorized($company, $user_id)
{
    $actor_id = account_recovery_material_current_actor_id();
    if ($actor_id <= 0 || (int)$user_id <= 0 || !account_recovery_material_company_matches($company))
        return false;
    return $actor_id === (int)$user_id
        || (function_exists('user_check_access') && user_check_access('SA_USERS'));
}

function account_recovery_material_password_matches($password, $hash)
{
    if (!is_string($password) || $password === '' || strlen($password) > 512
        || !is_string($hash) || $hash === '')
        return false;
    if (password_verify($password, $hash))
        return true;
    if (preg_match('/^[a-f0-9]{32}$/Di', $hash) !== 1)
        return false;
    $legacy = md5($password);
    return function_exists('hash_equals') ? hash_equals(strtolower($hash), $legacy)
        : strtolower($hash) === $legacy;
}

function account_recovery_material_random_bytes($length)
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

function account_recovery_material_generate_secret()
{
    $bytes = account_recovery_material_random_bytes(ACCOUNT_RECOVERY_MATERIAL_SECRET_BYTES);
    if (!is_string($bytes) || strlen($bytes) !== ACCOUNT_RECOVERY_MATERIAL_SECRET_BYTES)
        return false;
    return 'rc1_'.bin2hex($bytes);
}

function account_recovery_material_hash_secret($secret)
{
    if (!is_string($secret)
        || preg_match('/^rc1_[a-f0-9]{64}$/D', $secret) !== 1)
        return false;
    $hash = password_hash($secret, PASSWORD_DEFAULT);
    return is_string($hash) && $hash !== '' && strlen($hash) <= 255 ? $hash : false;
}

/** Lock the target account and role without exposing mutable profile claims. */
function account_recovery_material_user_for_update($user_id)
{
    $user_id = (int)$user_id;
    if ($user_id <= 0 || !defined('TB_PREF'))
        return false;
    $result = db_query("SELECT u.id user_id,u.login_id,u.password,u.role_id,u.inactive user_inactive,r.inactive role_inactive "
        ."FROM ".TB_PREF."users u INNER JOIN ".TB_PREF."security_roles r ON r.id=u.role_id "
        ."WHERE u.id=".$user_id." LIMIT 2 FOR UPDATE",
        'could not lock PAY-SEC-004 recovery-material user custody');
    if (!$result)
        return false;
    $row = db_fetch_assoc($result);
    $extra = db_fetch_assoc($result);
    if (!$row || is_array($extra))
        return false;
    return $row;
}

/** Lock the latest generation row, if one exists. */
function account_recovery_material_latest_for_update($user_id)
{
    $user_id = (int)$user_id;
    if ($user_id <= 0 || !defined('TB_PREF'))
        return false;
    $result = db_query("SELECT recovery_material_id,generation,issued_at FROM ".TB_PREF."user_recovery_materials "
        ."WHERE user_id=".$user_id." AND material_type=".db_escape(ACCOUNT_RECOVERY_MATERIAL_TYPE)
        ." ORDER BY generation DESC,material_slot ASC LIMIT 1 FOR UPDATE",
        'could not lock PAY-SEC-004 recovery-material generation custody');
    if (!$result)
        return false;
    $row = db_fetch_assoc($result);
    if (!$row)
        return null;
    $extra = db_fetch_assoc($result);
    if (is_array($extra))
        return false;
    return $row;
}

/** Lock all terminally revocable rows. */
function account_recovery_material_active_ids_for_update($user_id)
{
    $user_id = (int)$user_id;
    if ($user_id <= 0 || !defined('TB_PREF'))
        return false;
    $result = db_query("SELECT recovery_material_id FROM ".TB_PREF."user_recovery_materials "
        ."WHERE user_id=".$user_id." AND material_type=".db_escape(ACCOUNT_RECOVERY_MATERIAL_TYPE)
        ." AND consumed_at IS NULL AND revoked_at IS NULL ORDER BY recovery_material_id FOR UPDATE",
        'could not lock PAY-SEC-004 active recovery-material custody');
    if (!$result)
        return false;
    $ids = array();
    while ($row = db_fetch_assoc($result))
        $ids[] = (int)$row['recovery_material_id'];
    return $ids;
}

function account_recovery_material_revoke_reason($reason)
{
    $allowed = array('self_revoked','administrator_revoked','credential_reset','account_status_changed','account_deleted','rotated');
    return is_string($reason) && in_array($reason, $allowed, true) ? $reason : false;
}

function account_recovery_material_update_active_revocation($user_id, $actor_id, $reason, $now_at)
{
    $reason = account_recovery_material_revoke_reason($reason);
    if ((int)$user_id <= 0 || (int)$actor_id <= 0 || $reason === false || !is_string($now_at))
        return false;
    return (bool)db_query("UPDATE ".TB_PREF."user_recovery_materials SET revoked_at=".db_escape($now_at)
        .",revoked_by=".(int)$actor_id.",revocation_reason=".db_escape($reason)
        .",row_version=row_version+1 WHERE user_id=".(int)$user_id
        ." AND material_type=".db_escape(ACCOUNT_RECOVERY_MATERIAL_TYPE)
        ." AND consumed_at IS NULL AND revoked_at IS NULL",
        'could not revoke PAY-SEC-004 recovery material');
}

function account_recovery_material_load_audit_gateway()
{
    global $path_to_root;
    if (function_exists('append_domain_audit_event'))
        return true;
    $root = isset($path_to_root) ? realpath($path_to_root) : false;
    if ($root === false)
        return false;
    $file = $root.DIRECTORY_SEPARATOR.'includes'.DIRECTORY_SEPARATOR.'db'.DIRECTORY_SEPARATOR.'domain_audit_db.inc';
    if (!is_file($file))
        return false;
    require_once($file);
    return function_exists('append_domain_audit_event');
}

function account_recovery_material_rotation_audit($company, $user_id, $actor_id, $generation, $revoked_count, $issued_at, $expires_at)
{
    return append_domain_audit_event(array(
        'company_id'=>(int)$company,'legal_entity_id'=>0,
        'entity_type'=>'security_account_recovery_material',
        'entity_key'=>array('user_id'=>(int)$user_id,'generation'=>(int)$generation),
        'action'=>'security.account_recovery.material.rotated',
        'reason_code'=>'pay_sec_004_self_service_recovery_rotation',
        'before'=>array('active_material_revoked'=>(int)$revoked_count),
        'after'=>array('active_material_issued'=>ACCOUNT_RECOVERY_MATERIAL_SET_SIZE),
        'restricted_diff'=>array(
            'actor_id'=>(int)$actor_id,'user_id'=>(int)$user_id,
            'material_type'=>ACCOUNT_RECOVERY_MATERIAL_TYPE,'set_size'=>ACCOUNT_RECOVERY_MATERIAL_SET_SIZE,
            'issued_at'=>(string)$issued_at,'expires_at'=>(string)$expires_at,
            'rotation_cooldown_seconds'=>ACCOUNT_RECOVERY_MATERIAL_ROTATION_COOLDOWN_SECONDS,
            'secret_entropy_bits'=>ACCOUNT_RECOVERY_MATERIAL_SECRET_BYTES*8,
            'secret_hash_uses_password_hash'=>true,'plaintext_material_persisted'=>false,
            'plaintext_material_audited'=>false,'email_or_profile_claim_lookup_used'=>false,
            'administrator_issued_user_secret'=>false,'recovery_redemption_route_added'=>false,
            'administrator_reset_writer_added'=>false,'durable_sessions_mutated'=>false,
            'assurance_history_rewritten'=>false,'federation_links_mutated'=>false,
            'break_glass_authority_added'=>false,'role_granted'=>false,
            'payment_authority_changed'=>false,'payroll_authority_changed'=>false,
            'accounting_authority_changed'=>false))) !== false;
}

function account_recovery_material_revocation_audit($company, $user_id, $actor_id, $reason, $revoked_count)
{
    return append_domain_audit_event(array(
        'company_id'=>(int)$company,'legal_entity_id'=>0,
        'entity_type'=>'security_account_recovery_material',
        'entity_key'=>array('user_id'=>(int)$user_id),
        'action'=>'security.account_recovery.material.revoked',
        'reason_code'=>'pay_sec_004_recovery_material_revocation',
        'before'=>array('active_material'=>(int)$revoked_count),
        'after'=>array('active_material'=>0),
        'restricted_diff'=>array(
            'actor_id'=>(int)$actor_id,'user_id'=>(int)$user_id,'revocation_reason'=>(string)$reason,
            'revoked_count'=>(int)$revoked_count,'plaintext_material_persisted'=>false,
            'secret_hash_audited'=>false,'recovery_redemption_route_added'=>false,
            'administrator_reset_writer_added'=>false,'durable_sessions_mutated'=>false,
            'assurance_history_rewritten'=>false,'federation_links_mutated'=>false,
            'break_glass_authority_added'=>false,'role_granted'=>false,
            'payment_authority_changed'=>false,'payroll_authority_changed'=>false,
            'accounting_authority_changed'=>false))) !== false;
}

/**
 * Rotate the current user's recovery set after explicit local-password reauth.
 *
 * Plaintext is returned once to the authenticated caller and is never stored.
 * An uncertain committed response is not replayable because plaintext is not
 * recoverable; the safe recovery procedure is a later fresh authorized
 * rotation, which terminally revokes the unseen prior generation.
 */
function account_recovery_material_rotate_current_user($company, $current_password, $now_epoch = null)
{
    $actor_id = account_recovery_material_current_actor_id();
    $now = $now_epoch === null ? time() : $now_epoch;
    if ($actor_id <= 0 || !account_recovery_material_self_service_authorized($company, $actor_id)
        || !is_int($now) || $now <= 0 || !is_string($current_password) || $current_password === ''
        || strlen($current_password) > 512 || !account_recovery_material_load_audit_gateway()
        || !function_exists('begin_transaction') || !function_exists('commit_transaction') || !function_exists('cancel_transaction'))
        return false;

    $secrets = array();
    for ($slot = 1; $slot <= ACCOUNT_RECOVERY_MATERIAL_SET_SIZE; $slot++) {
        $secret = account_recovery_material_generate_secret();
        $hash = $secret === false ? false : account_recovery_material_hash_secret($secret);
        if ($secret === false || $hash === false)
            return false;
        $secrets[$slot] = array('secret'=>$secret,'hash'=>$hash);
    }

    begin_transaction();
    $committed = false;
    $result_value = false;
    do {
        $user = account_recovery_material_user_for_update($actor_id);
        if (!is_array($user) || (int)$user['user_inactive'] !== 0 || (int)$user['role_inactive'] !== 0
            || !account_recovery_material_password_matches($current_password, (string)$user['password']))
            break;
        $latest = account_recovery_material_latest_for_update($actor_id);
        if ($latest === false)
            break;
        $generation = 1;
        if (is_array($latest)) {
            $previous_generation = (int)$latest['generation'];
            if ($previous_generation <= 0 || $previous_generation >= 4294967295)
                break;
            $latest_epoch = strtotime((string)$latest['issued_at'].' UTC');
            if ($latest_epoch === false || $latest_epoch > $now
                || $now - $latest_epoch < ACCOUNT_RECOVERY_MATERIAL_ROTATION_COOLDOWN_SECONDS)
                break;
            $generation = $previous_generation + 1;
        }
        $active_ids = account_recovery_material_active_ids_for_update($actor_id);
        if (!is_array($active_ids))
            break;
        $issued_at = gmdate('Y-m-d H:i:s', $now);
        $expires_at = gmdate('Y-m-d H:i:s', $now + ACCOUNT_RECOVERY_MATERIAL_LIFETIME_SECONDS);
        if (count($active_ids) > 0
            && !account_recovery_material_update_active_revocation($actor_id, $actor_id, 'rotated', $issued_at))
            break;
        $issued = array();
        foreach ($secrets as $slot=>$material) {
            $sql = "INSERT INTO ".TB_PREF."user_recovery_materials(user_id,material_type,generation,material_slot,secret_hash,issued_at,expires_at,consumed_at,revoked_at,revoked_by,revocation_reason,created_by,row_version) VALUES ("
                .$actor_id.",".db_escape(ACCOUNT_RECOVERY_MATERIAL_TYPE).",".$generation.",".(int)$slot.",".db_escape($material['hash']).",".db_escape($issued_at).",".db_escape($expires_at).",NULL,NULL,NULL,NULL,".$actor_id.",1)";
            if (!db_query($sql, 'could not issue PAY-SEC-004 recovery material')) {
                $issued = false;
                break;
            }
            $material_id = function_exists('db_insert_id') ? (int)db_insert_id() : 0;
            if ($material_id <= 0) {
                $issued = false;
                break;
            }
            $issued[] = array('recovery_material_id'=>$material_id,'secret'=>$material['secret']);
        }
        if ($issued === false || count($issued) !== ACCOUNT_RECOVERY_MATERIAL_SET_SIZE)
            break;
        if (!account_recovery_material_rotation_audit($company, $actor_id, $actor_id, $generation,
            count($active_ids), $issued_at, $expires_at))
            break;
        commit_transaction();
        $committed = true;
        $result_value = array('user_id'=>$actor_id,'generation'=>$generation,
            'material_type'=>ACCOUNT_RECOVERY_MATERIAL_TYPE,'issued_at'=>$issued_at,'expires_at'=>$expires_at,
            'materials'=>$issued,'previous_active_revoked'=>count($active_ids),
            'plaintext_persisted'=>false,'redemption_route_added'=>false);
    } while (false);
    if (!$committed) {
        cancel_transaction();
        return false;
    }
    return $result_value;
}

/** Revoke all unconsumed recovery material for self or an SA_USERS target. */
function account_recovery_material_revoke_for_user($company, $user_id, $reason = null, $now_epoch = null)
{
    $user_id = (int)$user_id;
    $actor_id = account_recovery_material_current_actor_id();
    $now = $now_epoch === null ? time() : $now_epoch;
    if ($user_id <= 0 || $actor_id <= 0 || !account_recovery_material_revoke_authorized($company, $user_id)
        || !is_int($now) || $now <= 0 || !account_recovery_material_load_audit_gateway()
        || !function_exists('begin_transaction') || !function_exists('commit_transaction') || !function_exists('cancel_transaction'))
        return false;
    $is_self = $actor_id === $user_id;
    if ($reason === null)
        $reason = $is_self ? 'self_revoked' : 'administrator_revoked';
    $reason = account_recovery_material_revoke_reason($reason);
    if ($reason === false || ($is_self && $reason !== 'self_revoked')
        || (!$is_self && !in_array($reason, array('administrator_revoked','credential_reset','account_status_changed','account_deleted'), true)))
        return false;

    begin_transaction();
    $committed = false;
    $result_value = false;
    do {
        $user = account_recovery_material_user_for_update($user_id);
        if (!is_array($user))
            break;
        if ($is_self && ((int)$user['user_inactive'] !== 0 || (int)$user['role_inactive'] !== 0))
            break;
        $active_ids = account_recovery_material_active_ids_for_update($user_id);
        if (!is_array($active_ids))
            break;
        if (count($active_ids) === 0) {
            commit_transaction();
            $committed = true;
            $result_value = array('user_id'=>$user_id,'revoked_count'=>0,'reason'=>$reason,'idempotent_retry'=>true);
            break;
        }
        $now_at = gmdate('Y-m-d H:i:s', $now);
        if (!account_recovery_material_update_active_revocation($user_id, $actor_id, $reason, $now_at)
            || !account_recovery_material_revocation_audit($company, $user_id, $actor_id, $reason, count($active_ids)))
            break;
        commit_transaction();
        $committed = true;
        $result_value = array('user_id'=>$user_id,'revoked_count'=>count($active_ids),'reason'=>$reason,'idempotent_retry'=>false);
    } while (false);
    if (!$committed) {
        cancel_transaction();
        return false;
    }
    return $result_value;
}
