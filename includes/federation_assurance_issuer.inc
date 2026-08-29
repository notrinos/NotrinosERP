<?php
/**
 * PAY-SEC-004 federated privileged-assurance issuer command foundation.
 *
 * Internal command only. A fresh signed level-two OIDC candidate is bound to
 * the current active federated durable session and exactly one opaque action
 * scope. The command updates only session assurance metadata, inserts one
 * single-purpose assurance event, and appends restricted PAY-AUD evidence in
 * one transaction. It exposes no route and consumes no HR/payroll action.
 */
require_once(dirname(__FILE__).DIRECTORY_SEPARATOR.'session_registry.inc');

if (!defined('FEDERATION_ASSURANCE_ISSUER_MAX_LIFETIME'))
    define('FEDERATION_ASSURANCE_ISSUER_MAX_LIFETIME', 300);

function federation_assurance_issuer_hex64($value)
{
    return is_string($value) && preg_match('/^[a-f0-9]{64}$/D', $value) === 1;
}

function federation_assurance_issuer_action_class($value)
{
    return is_string($value) && strlen($value) >= 1 && strlen($value) <= 64
        && preg_match('/^[a-z0-9][a-z0-9._-]*$/D', $value) === 1;
}

/** Validate the non-database issuer request. No authority is granted here. */
function federation_assurance_issue_request($company, $session_id, $candidate, $action_class, $action_hash, $now_epoch = null)
{
    $company = (int)$company;
    $session_id = (int)$session_id;
    $now = $now_epoch === null ? time() : $now_epoch;
    if ($company < 0 || $session_id <= 0 || !is_int($now) || $now <= 0
        || !federation_assurance_issuer_action_class($action_class)
        || !federation_assurance_issuer_hex64($action_hash)
        || !is_array($candidate))
        return false;
    $required = array('verified','provider_id','external_subject_hash','issuer_sha256','client_id_sha256',
        'verified_assurance_level','assurance_method','auth_time_epoch','verified_at_epoch','expires_at_epoch',
        'assurance_proof_sha256','elevation_issued','assurance_event_written','recovery_factor_verified');
    foreach ($required as $key)
        if (!array_key_exists($key, $candidate))
            return false;
    if ($candidate['verified'] !== true || (int)$candidate['provider_id'] <= 0
        || (int)$candidate['verified_assurance_level'] !== 2
        || (string)$candidate['assurance_method'] !== 'oidc_acr_amr'
        || $candidate['elevation_issued'] !== false || $candidate['assurance_event_written'] !== false
        || $candidate['recovery_factor_verified'] !== false
        || !federation_assurance_issuer_hex64($candidate['external_subject_hash'])
        || !federation_assurance_issuer_hex64($candidate['issuer_sha256'])
        || !federation_assurance_issuer_hex64($candidate['client_id_sha256'])
        || !federation_assurance_issuer_hex64($candidate['assurance_proof_sha256'])
        || !is_int($candidate['auth_time_epoch']) || !is_int($candidate['verified_at_epoch'])
        || !is_int($candidate['expires_at_epoch']) || $candidate['auth_time_epoch'] <= 0
        || $candidate['verified_at_epoch'] <= 0 || $candidate['verified_at_epoch'] > $now
        || $candidate['expires_at_epoch'] <= $now
        || $candidate['expires_at_epoch'] - $now > FEDERATION_ASSURANCE_ISSUER_MAX_LIFETIME)
        return false;
    if (function_exists('user_company') && (int)user_company() !== $company)
        return false;
    return array('company'=>$company,'session_id'=>$session_id,'candidate'=>$candidate,
        'action_class'=>$action_class,'action_hash'=>$action_hash,'now_epoch'=>$now,
        'proof_hash'=>$candidate['assurance_proof_sha256']);
}

/** Lock the current durable session and its immutable federation provenance. */
function federation_assurance_issuer_custody_for_update($session_id)
{
    $session_id = (int)$session_id;
    if ($session_id <= 0 || !defined('TB_PREF'))
        return false;
    $sql = "SELECT s.id session_id,s.session_key_hash,s.user_id,s.role_id,s.auth_method,s.assurance_level,s.assurance_method,"
        ."s.assurance_issued_at,s.assurance_expires_at,s.idle_expires_at,s.absolute_expires_at,s.revoked_at,s.user_agent_hash,"
        ."b.verifier_decision_id,b.external_subject_link_id,d.external_subject_hash,d.verified_assurance_level decision_assurance_level,d.assurance_method decision_assurance_method,"
        ."t.verifier_config_id,c.provider_id,c.audience_hash,c.assurance_level_cap,c.config_status,p.issuer_hash,p.provider_status,"
        ."l.user_id linked_user_id,l.link_status,l.deprovisioned_at,u.role_id current_role_id,u.inactive user_inactive,r.inactive role_inactive "
        ."FROM ".TB_PREF."user_sessions s "
        ."INNER JOIN ".TB_PREF."user_federation_session_bindings b ON b.session_id=s.id "
        ."INNER JOIN ".TB_PREF."user_federation_verifier_decisions d ON d.verifier_decision_id=b.verifier_decision_id "
        ."INNER JOIN ".TB_PREF."user_federation_verifier_transactions t ON t.verifier_transaction_id=d.verifier_transaction_id "
        ."INNER JOIN ".TB_PREF."user_federation_provider_verifier_configs c ON c.verifier_config_id=t.verifier_config_id "
        ."INNER JOIN ".TB_PREF."user_federation_providers p ON p.provider_id=c.provider_id "
        ."INNER JOIN ".TB_PREF."user_federation_external_subject_links l ON l.external_subject_link_id=b.external_subject_link_id "
        ." AND l.provider_id=c.provider_id AND l.external_subject_hash=d.external_subject_hash "
        ."INNER JOIN ".TB_PREF."users u ON u.id=s.user_id AND u.id=l.user_id "
        ."INNER JOIN ".TB_PREF."security_roles r ON r.id=u.role_id "
        ."WHERE s.id=".$session_id." LIMIT 2 FOR UPDATE";
    $result = db_query($sql, 'could not lock PAY-SEC-004 federated assurance issuer custody');
    if (!$result)
        return false;
    $row = db_fetch_assoc($result);
    if (!$row || db_fetch_assoc($result) !== false)
        return false;
    return $row;
}

/** Load an exact proof replay record for idempotent response recovery. */
function federation_assurance_issuer_existing_event_for_update($proof_hash)
{
    if (!defined('TB_PREF') || !federation_assurance_issuer_hex64($proof_hash))
        return false;
    $result = db_query("SELECT assurance_event_id,session_id,assurance_level,assurance_method,action_class,action_hash,nonce_hash,issued_at,expires_at,consumed_at "
        ."FROM ".TB_PREF."user_session_assurance_events WHERE nonce_hash=".db_escape($proof_hash)." LIMIT 2 FOR UPDATE",
        'could not verify PAY-SEC-004 assurance proof replay state');
    if (!$result)
        return false;
    $row = db_fetch_assoc($result);
    if (!$row)
        return null;
    if (db_fetch_assoc($result) !== false)
        return false;
    return $row;
}

/** Require the live PHP session, durable secret/device, role and federation subject/provider to match. */
function federation_assurance_issuer_custody_valid($row, $request)
{
    if (!is_array($row) || !is_array($request) || !isset($_SESSION['wa_current_user'])
        || !is_object($_SESSION['wa_current_user']) || !method_exists($_SESSION['wa_current_user'], 'logged_in')
        || !$_SESSION['wa_current_user']->logged_in() || !isset($_SESSION['security_session_registry_id'], $_SESSION['security_session_secret']))
        return false;
    $candidate = $request['candidate'];
    $user = $_SESSION['wa_current_user'];
    $now = (int)$request['now_epoch'];
    $idle = strtotime((string)$row['idle_expires_at'].' UTC');
    $absolute = strtotime((string)$row['absolute_expires_at'].' UTC');
    return (int)$_SESSION['security_session_registry_id'] === (int)$request['session_id']
        && (int)$row['session_id'] === (int)$request['session_id']
        && (int)$row['user_id'] === (int)$row['linked_user_id'] && (int)$row['user_id'] === (int)$user->user
        && (int)$row['role_id'] === (int)$row['current_role_id'] && (int)$row['role_id'] === (int)$user->access
        && (string)$row['auth_method'] === 'federated_oidc' && (int)$row['decision_assurance_level'] === 1
        && (string)$row['decision_assurance_method'] === 'oidc_rs256' && (int)$row['assurance_level_cap'] >= 2
        && (string)$row['provider_status'] === 'enabled' && (string)$row['config_status'] === 'enabled'
        && (string)$row['link_status'] === 'active' && $row['deprovisioned_at'] === null
        && $row['revoked_at'] === null && (int)$row['user_inactive'] === 0 && (int)$row['role_inactive'] === 0
        && $idle !== false && $absolute !== false && $now < $idle && $now < $absolute
        && (int)$candidate['provider_id'] === (int)$row['provider_id']
        && session_registry_hash_equals((string)$candidate['external_subject_hash'], (string)$row['external_subject_hash'])
        && session_registry_hash_equals((string)$candidate['issuer_sha256'], (string)$row['issuer_hash'])
        && session_registry_hash_equals((string)$candidate['client_id_sha256'], (string)$row['audience_hash'])
        && session_registry_hash_equals((string)$row['session_key_hash'], session_registry_hash($_SESSION['security_session_secret']))
        && session_registry_hash_equals((string)$row['user_agent_hash'], session_registry_user_agent_hash());
}


/** Lock any currently usable unconsumed event for this durable session. */
function federation_assurance_issuer_active_event_for_update($session_id, $now_at)
{
    $session_id = (int)$session_id;
    if ($session_id <= 0 || !defined('TB_PREF') || !is_string($now_at))
        return false;
    $result = db_query("SELECT assurance_event_id,nonce_hash FROM ".TB_PREF."user_session_assurance_events "
        ."WHERE session_id=".$session_id." AND consumed_at IS NULL AND expires_at>".db_escape($now_at)
        ." ORDER BY assurance_event_id ASC LIMIT 2 FOR UPDATE",
        'could not lock PAY-SEC-004 active single-purpose assurance event');
    if (!$result)
        return false;
    $row = db_fetch_assoc($result);
    if (!$row)
        return null;
    if (db_fetch_assoc($result) !== false)
        return false;
    return $row;
}

function federation_assurance_issuer_audit($company, $row, $event_id, $request, $expires_at)
{
    return append_domain_audit_event(array('company_id'=>(int)$company,'legal_entity_id'=>0,
        'entity_type'=>'security_session_assurance','entity_key'=>array('assurance_event_id'=>(int)$event_id,'session_id'=>(int)$row['session_id']),
        'action'=>'security.session.assurance.issued','reason_code'=>'pay_sec_004_federated_assurance_issuer',
        'before'=>array('assurance_level'=>(int)$row['assurance_level']),
        'after'=>array('assurance_level'=>2),
        'restricted_diff'=>array('provider_id'=>(int)$row['provider_id'],'verifier_config_id'=>(int)$row['verifier_config_id'],
            'user_id'=>(int)$row['user_id'],'role_id'=>(int)$row['role_id'],'assurance_method'=>'oidc_acr_amr',
            'action_class'=>(string)$request['action_class'],'action_hash'=>(string)$request['action_hash'],
            'proof_hash'=>(string)$request['proof_hash'],'expires_at'=>(string)$expires_at,
            'single_purpose_event'=>true,'raw_oidc_claims_stored'=>false,'raw_token_stored'=>false,
            'recovery_factor_written'=>false,'break_glass_authority_added'=>false,'privileged_action_consumed'=>false,
            'role_granted'=>false,'payment_authority_changed'=>false,'payroll_authority_changed'=>false,
            'accounting_authority_changed'=>false))) !== false;
}

/**
 * Issue one short-lived, action-bound assurance event for the current session.
 *
 * The signed OIDC token digest is used as the unique replay key. One proof can
 * therefore mint at most one event. Exact uncertain-response retry is allowed;
 * another action or session with the same proof fails closed.
 */
function federation_issue_privileged_assurance($company, $session_id, $candidate, $action_class, $action_hash, $now_epoch = null)
{
    global $path_to_root;
    $request = federation_assurance_issue_request($company, $session_id, $candidate, $action_class, $action_hash, $now_epoch);
    if ($request === false)
        return false;
    if (!function_exists('append_domain_audit_event')) {
        $root = isset($path_to_root) ? realpath($path_to_root) : false;
        if ($root === false)
            return false;
        require_once($root.'/includes/db/domain_audit_db.inc');
    }
    if (!function_exists('append_domain_audit_event') || !db_query('BEGIN', 'could not start PAY-SEC-004 assurance issuance transaction'))
        return false;
    $committed = false;
    $result_value = false;
    do {
        $row = federation_assurance_issuer_custody_for_update($request['session_id']);
        if (!is_array($row) || !federation_assurance_issuer_custody_valid($row, $request))
            break;
        $absolute = strtotime((string)$row['absolute_expires_at'].' UTC');
        $idle = strtotime((string)$row['idle_expires_at'].' UTC');
        $expires_epoch = min((int)$candidate['expires_at_epoch'], $absolute, $idle, (int)$request['now_epoch'] + FEDERATION_ASSURANCE_ISSUER_MAX_LIFETIME);
        if ($expires_epoch <= (int)$request['now_epoch'])
            break;
        $expires_at = gmdate('Y-m-d H:i:s', $expires_epoch);
        $issued_at = gmdate('Y-m-d H:i:s', (int)$request['now_epoch']);
        $existing = federation_assurance_issuer_existing_event_for_update($request['proof_hash']);
        if ($existing === false)
            break;
        if (is_array($existing)) {
            $existing_expires = strtotime((string)$existing['expires_at'].' UTC');
            if ((int)$existing['session_id'] !== (int)$row['session_id'] || (int)$existing['assurance_level'] !== 2
                || (string)$existing['assurance_method'] !== 'oidc_acr_amr'
                || (string)$existing['action_class'] !== (string)$request['action_class']
                || !session_registry_hash_equals((string)$existing['action_hash'], (string)$request['action_hash'])
                || $existing['consumed_at'] !== null || $existing_expires === false || (int)$request['now_epoch'] >= $existing_expires
                || (int)$row['assurance_level'] !== 2 || (string)$row['assurance_method'] !== 'oidc_acr_amr')
                break;
            if (!db_query('COMMIT', 'could not commit PAY-SEC-004 assurance idempotent verification'))
                break;
            $committed = true;
            $result_value = array('assurance_event_id'=>(int)$existing['assurance_event_id'],'session_id'=>(int)$row['session_id'],
                'assurance_level'=>2,'assurance_method'=>'oidc_acr_amr','action_class'=>$request['action_class'],
                'action_hash'=>$request['action_hash'],'expires_at'=>(string)$existing['expires_at'],'idempotent_retry'=>true,
                'privileged_action_consumed'=>false);
            break;
        }
        $active = federation_assurance_issuer_active_event_for_update($row['session_id'], $issued_at);
        if ($active === false || is_array($active))
            break;
        $current_expiry = $row['assurance_expires_at'] === null ? false : strtotime((string)$row['assurance_expires_at'].' UTC');
        $eligible_level_one = (int)$row['assurance_level'] === 1 && (string)$row['assurance_method'] === 'oidc_rs256';
        $eligible_expired_level_two = (int)$row['assurance_level'] === 2 && (string)$row['assurance_method'] === 'oidc_acr_amr'
            && $current_expiry !== false && $current_expiry <= (int)$request['now_epoch'];
        if (!$eligible_level_one && !$eligible_expired_level_two)
            break;
        $eligible_sql = $eligible_level_one
            ? "assurance_level=1 AND assurance_method='oidc_rs256'"
            : "assurance_level=2 AND assurance_method='oidc_acr_amr' AND assurance_expires_at<=".db_escape($issued_at);
        $update = "UPDATE ".TB_PREF."user_sessions SET assurance_level=2,assurance_method='oidc_acr_amr',assurance_issued_at=".db_escape($issued_at)
            .",assurance_expires_at=".db_escape($expires_at)." WHERE id=".(int)$row['session_id']." AND revoked_at IS NULL AND (".$eligible_sql.")";
        if (!db_query($update, 'could not elevate PAY-SEC-004 durable session assurance') || !function_exists('db_num_affected_rows') || (int)db_num_affected_rows() !== 1)
            break;
        $insert = "INSERT INTO ".TB_PREF."user_session_assurance_events(session_id,assurance_level,assurance_method,action_class,action_hash,nonce_hash,issued_at,expires_at,consumed_at) VALUES ("
            .(int)$row['session_id'].",2,'oidc_acr_amr',".db_escape($request['action_class']).",".db_escape($request['action_hash']).",".db_escape($request['proof_hash']).",".db_escape($issued_at).",".db_escape($expires_at).",NULL)";
        if (!db_query($insert, 'could not append PAY-SEC-004 single-purpose assurance event'))
            break;
        $event_id = (int)db_insert_id();
        if ($event_id <= 0 || !federation_assurance_issuer_audit($request['company'], $row, $event_id, $request, $expires_at))
            break;
        if (!db_query('COMMIT', 'could not commit PAY-SEC-004 federated assurance issuance'))
            break;
        $committed = true;
        $result_value = array('assurance_event_id'=>$event_id,'session_id'=>(int)$row['session_id'],'assurance_level'=>2,
            'assurance_method'=>'oidc_acr_amr','action_class'=>$request['action_class'],'action_hash'=>$request['action_hash'],
            'expires_at'=>$expires_at,'idempotent_retry'=>false,'privileged_action_consumed'=>false);
    } while (false);
    if (!$committed)
        db_query('ROLLBACK', 'could not roll back PAY-SEC-004 federated assurance issuance');
    return $committed ? $result_value : false;
}
