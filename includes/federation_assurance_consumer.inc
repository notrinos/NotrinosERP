<?php
/**
 * PAY-SEC-004 privileged-action assurance consumer foundation.
 *
 * Internal-only, transaction-participating consumer scoped to the first
 * reviewed high-risk command: approved payroll-period posting. This file does
 * not wire itself into the payroll route. A later integration slice must call
 * the consumer only after the existing authorization and idempotent already-
 * posted checks, and before any financial side effect is written.
 */
require_once(dirname(__FILE__).DIRECTORY_SEPARATOR.'session_registry.inc');

if (!defined('FEDERATION_ASSURANCE_PAYROLL_POST_ACTION_CLASS'))
    define('FEDERATION_ASSURANCE_PAYROLL_POST_ACTION_CLASS', 'hrm.payroll.period.post');
if (!defined('FEDERATION_ASSURANCE_PAYROLL_POST_HASH_CONTRACT'))
    define('FEDERATION_ASSURANCE_PAYROLL_POST_HASH_CONTRACT', 'notrinoserp.pay-sec-004.payroll-post-action');
if (!defined('FEDERATION_ASSURANCE_PAYROLL_POST_HASH_VERSION'))
    define('FEDERATION_ASSURANCE_PAYROLL_POST_HASH_VERSION', '1');

function federation_assurance_consumer_hex64($value)
{
    return is_string($value) && preg_match('/^[a-f0-9]{64}$/D', $value) === 1;
}

function federation_assurance_consumer_positive_int($value)
{
    if (is_int($value))
        return $value > 0 ? $value : false;
    if (!is_string($value) || preg_match('/^[1-9][0-9]*$/D', $value) !== 1)
        return false;
    $integer = (int)$value;
    return $integer > 0 && (string)$integer === $value ? $integer : false;
}

/**
 * Build the only action hash accepted by this foundation.
 *
 * The immutable approval draft id is included in addition to the deterministic
 * approval hash so a fresh approval of identical payroll results cannot reuse
 * a proof minted for an earlier approval decision.
 *
 * @param int $company
 * @param int $period_id
 * @param int $approval_draft_id
 * @param array $binding
 * @return string|false
 */
function federation_assurance_payroll_post_action_hash($company, $period_id, $approval_draft_id, $binding)
{
    $company = (int)$company;
    $period_id = federation_assurance_consumer_positive_int($period_id);
    $approval_draft_id = federation_assurance_consumer_positive_int($approval_draft_id);
    if ($company < 0 || $period_id === false || $approval_draft_id === false || !is_array($binding))
        return false;
    foreach (array('contract','version','approval_hash') as $key)
        if (!array_key_exists($key, $binding) || !is_string($binding[$key]))
            return false;
    if ($binding['contract'] !== 'notrinoserp.hrm.payroll.approval-binding'
        || !in_array($binding['version'], array('1.0.0','1.1.0'), true)
        || !federation_assurance_consumer_hex64($binding['approval_hash']))
        return false;

    $payload = FEDERATION_ASSURANCE_PAYROLL_POST_HASH_CONTRACT
        .'|v='.FEDERATION_ASSURANCE_PAYROLL_POST_HASH_VERSION
        .'|company='.$company
        .'|action='.FEDERATION_ASSURANCE_PAYROLL_POST_ACTION_CLASS
        .'|period_id='.$period_id
        .'|approval_draft_id='.$approval_draft_id
        .'|approval_binding_contract='.$binding['contract']
        .'|approval_binding_version='.$binding['version']
        .'|approval_hash='.$binding['approval_hash'];
    return hash('sha256', $payload);
}

/** Lock current federated-session custody required by a privileged consumer. */
function federation_assurance_consumer_custody_for_update($session_id)
{
    $session_id = (int)$session_id;
    if ($session_id <= 0 || !defined('TB_PREF'))
        return false;
    $sql = "SELECT s.id session_id,s.session_key_hash,s.user_id,s.role_id,s.auth_method,s.assurance_level,s.assurance_method,"
        ."s.assurance_issued_at,s.assurance_expires_at,s.idle_expires_at,s.absolute_expires_at,s.revoked_at,s.user_agent_hash,"
        ."b.verifier_decision_id,b.external_subject_link_id,d.verified_assurance_level decision_assurance_level,d.assurance_method decision_assurance_method,"
        ."d.verified_at decision_verified_at,d.valid_until decision_valid_until,t.verifier_config_id,c.provider_id,c.assurance_level_cap,c.config_status,"
        ."p.provider_status,l.user_id linked_user_id,l.link_status,l.deprovisioned_at,u.role_id current_role_id,u.inactive user_inactive,r.inactive role_inactive "
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
    $result = db_query($sql, 'could not lock PAY-SEC-004 privileged-action consumer custody');
    if (!$result)
        return false;
    $row = db_fetch_assoc($result);
    if (!$row || db_fetch_assoc($result) !== false)
        return false;
    return $row;
}

function federation_assurance_consumer_current_session_id()
{
    return isset($_SESSION['security_session_registry_id'])
        ? (int)$_SESSION['security_session_registry_id'] : 0;
}

function federation_assurance_consumer_current_custody_valid($row, $now_epoch, $require_level_two)
{
    if (!is_array($row) || !is_int($now_epoch) || $now_epoch <= 0
        || !isset($_SESSION['wa_current_user'], $_SESSION['security_session_secret'])
        || !is_object($_SESSION['wa_current_user'])
        || !method_exists($_SESSION['wa_current_user'], 'logged_in')
        || !$_SESSION['wa_current_user']->logged_in())
        return false;
    $user = $_SESSION['wa_current_user'];
    $idle = strtotime((string)$row['idle_expires_at'].' UTC');
    $absolute = strtotime((string)$row['absolute_expires_at'].' UTC');
    $decision_valid_until = strtotime((string)$row['decision_valid_until'].' UTC');
    if ($idle === false || $absolute === false || $decision_valid_until === false)
        return false;
    if ($now_epoch >= $idle || $now_epoch >= $absolute || $now_epoch >= $decision_valid_until)
        return false;
    if (federation_assurance_consumer_current_session_id() !== (int)$row['session_id']
        || (int)$row['user_id'] !== (int)$row['linked_user_id']
        || (int)$row['user_id'] !== (int)$user->user
        || (int)$row['role_id'] !== (int)$row['current_role_id']
        || (int)$row['role_id'] !== (int)$user->access
        || (string)$row['auth_method'] !== 'federated_oidc'
        || (int)$row['decision_assurance_level'] !== 1
        || (string)$row['decision_assurance_method'] !== 'oidc_rs256'
        || (int)$row['assurance_level_cap'] < 2
        || (string)$row['provider_status'] !== 'enabled'
        || (string)$row['config_status'] !== 'enabled'
        || (string)$row['link_status'] !== 'active'
        || $row['deprovisioned_at'] !== null
        || $row['revoked_at'] !== null
        || (int)$row['user_inactive'] !== 0
        || (int)$row['role_inactive'] !== 0
        || !session_registry_hash_equals((string)$row['session_key_hash'], session_registry_hash($_SESSION['security_session_secret']))
        || !session_registry_hash_equals((string)$row['user_agent_hash'], session_registry_user_agent_hash()))
        return false;
    if ($require_level_two) {
        $assurance_expires = $row['assurance_expires_at'] === null ? false : strtotime((string)$row['assurance_expires_at'].' UTC');
        return (int)$row['assurance_level'] === 2
            && (string)$row['assurance_method'] === 'oidc_acr_amr'
            && $assurance_expires !== false && $now_epoch < $assurance_expires;
    }
    return true;
}

/** Lock one exact action-bound assurance event. Ambiguity fails closed. */
function federation_assurance_consumer_event_for_update($session_id, $action_class, $action_hash)
{
    $session_id = (int)$session_id;
    if ($session_id <= 0 || $action_class !== FEDERATION_ASSURANCE_PAYROLL_POST_ACTION_CLASS
        || !federation_assurance_consumer_hex64($action_hash) || !defined('TB_PREF'))
        return false;
    $result = db_query("SELECT assurance_event_id,session_id,assurance_level,assurance_method,action_class,action_hash,nonce_hash,issued_at,expires_at,consumed_at "
        ."FROM ".TB_PREF."user_session_assurance_events WHERE session_id=".$session_id
        ." AND action_class=".db_escape($action_class)." AND action_hash=".db_escape($action_hash)
        ." ORDER BY assurance_event_id DESC LIMIT 2 FOR UPDATE",
        'could not lock PAY-SEC-004 privileged-action assurance event');
    if (!$result)
        return false;
    $row = db_fetch_assoc($result);
    if (!$row)
        return null;
    if (db_fetch_assoc($result) !== false)
        return false;
    return $row;
}

function federation_assurance_consumer_restore_level_one_sql($row, $now_epoch)
{
    if (!is_array($row) || !is_int($now_epoch) || $now_epoch <= 0)
        return false;
    $verified = strtotime((string)$row['decision_verified_at'].' UTC');
    $valid_until = strtotime((string)$row['decision_valid_until'].' UTC');
    $absolute = strtotime((string)$row['absolute_expires_at'].' UTC');
    if ($verified === false || $valid_until === false || $absolute === false)
        return false;
    $restored_expiry = min($valid_until, $absolute);
    if ($restored_expiry <= $now_epoch)
        return false;
    return "assurance_level=1,assurance_method='oidc_rs256',assurance_issued_at=".db_escape(gmdate('Y-m-d H:i:s', $verified))
        .",assurance_expires_at=".db_escape(gmdate('Y-m-d H:i:s', $restored_expiry));
}

function federation_assurance_consumer_audit($company, $row, $event, $action_hash, $consumed_at)
{
    return append_domain_audit_event(array(
        'company_id'=>(int)$company,
        'legal_entity_id'=>0,
        'entity_type'=>'security_session_assurance',
        'entity_key'=>array('assurance_event_id'=>(int)$event['assurance_event_id'],'session_id'=>(int)$row['session_id']),
        'action'=>'security.session.assurance.consumed',
        'reason_code'=>'pay_sec_004_privileged_action_consumer',
        'before'=>array('assurance_level'=>2),
        'after'=>array('assurance_level'=>1),
        'restricted_diff'=>array(
            'provider_id'=>(int)$row['provider_id'],
            'verifier_config_id'=>(int)$row['verifier_config_id'],
            'user_id'=>(int)$row['user_id'],
            'role_id'=>(int)$row['role_id'],
            'action_class'=>FEDERATION_ASSURANCE_PAYROLL_POST_ACTION_CLASS,
            'action_hash'=>(string)$action_hash,
            'consumed_at'=>(string)$consumed_at,
            'single_use_event_consumed'=>true,
            'session_assurance_reset_to_level_one'=>true,
            'business_action_committed'=>false,
            'raw_payroll_values_stored'=>false,
            'raw_oidc_claims_stored'=>false,
            'raw_token_stored'=>false,
            'recovery_factor_written'=>false,
            'break_glass_authority_added'=>false,
            'role_granted'=>false,
            'payment_authority_changed'=>false,
            'accounting_authority_changed'=>false
        )
    )) !== false;
}

/**
 * Consume the exact payroll-post assurance event inside the caller transaction.
 *
 * Existing SA_PAYROLLPOST authorization is checked before assurance custody.
 * The caller must already own the business transaction and must roll it back on
 * false. A savepoint ensures this helper itself never leaves a partial event or
 * session reset behind when one of its own checks/audit writes fails.
 */
function federation_consume_payroll_post_assurance($company, $period_id, $approval_draft_id, $binding, $now_epoch = null)
{
    global $transaction_level, $path_to_root;
    $company = (int)$company;
    $now = $now_epoch === null ? time() : $now_epoch;
    if ($company < 0 || !is_int($now) || $now <= 0
        || !function_exists('user_check_access') || !user_check_access('SA_PAYROLLPOST'))
        return false;
    if (function_exists('user_company') && (int)user_company() !== $company)
        return false;
    if (empty($transaction_level))
        return false;
    $action_hash = federation_assurance_payroll_post_action_hash($company, $period_id, $approval_draft_id, $binding);
    $session_id = federation_assurance_consumer_current_session_id();
    if ($action_hash === false || $session_id <= 0)
        return false;
    if (!function_exists('append_domain_audit_event')) {
        $root = isset($path_to_root) ? realpath($path_to_root) : false;
        if ($root === false)
            return false;
        require_once($root.'/includes/db/domain_audit_db.inc');
    }
    if (!function_exists('append_domain_audit_event')
        || !db_query('SAVEPOINT pay_sec_004_assurance_consumer', 'could not create PAY-SEC-004 assurance consumer savepoint'))
        return false;

    $success = false;
    $result_value = false;
    do {
        $row = federation_assurance_consumer_custody_for_update($session_id);
        if (!federation_assurance_consumer_current_custody_valid($row, $now, true))
            break;
        $event = federation_assurance_consumer_event_for_update($session_id, FEDERATION_ASSURANCE_PAYROLL_POST_ACTION_CLASS, $action_hash);
        if (!is_array($event)
            || (int)$event['session_id'] !== $session_id
            || (int)$event['assurance_level'] !== 2
            || (string)$event['assurance_method'] !== 'oidc_acr_amr'
            || (string)$event['action_class'] !== FEDERATION_ASSURANCE_PAYROLL_POST_ACTION_CLASS
            || !session_registry_hash_equals((string)$event['action_hash'], $action_hash)
            || $event['consumed_at'] !== null)
            break;
        $issued = strtotime((string)$event['issued_at'].' UTC');
        $expires = strtotime((string)$event['expires_at'].' UTC');
        $session_issued = strtotime((string)$row['assurance_issued_at'].' UTC');
        $session_expires = strtotime((string)$row['assurance_expires_at'].' UTC');
        if ($issued === false || $expires === false || $session_issued === false || $session_expires === false
            || $issued !== $session_issued || $expires !== $session_expires || $now < $issued || $now >= $expires)
            break;
        $consumed_at = gmdate('Y-m-d H:i:s', $now);
        $update_event = "UPDATE ".TB_PREF."user_session_assurance_events SET consumed_at=".db_escape($consumed_at)
            ." WHERE assurance_event_id=".(int)$event['assurance_event_id']." AND consumed_at IS NULL AND expires_at>".db_escape($consumed_at);
        if (!db_query($update_event, 'could not consume PAY-SEC-004 privileged-action assurance event')
            || !function_exists('db_num_affected_rows') || (int)db_num_affected_rows() !== 1)
            break;
        $restore = federation_assurance_consumer_restore_level_one_sql($row, $now);
        if ($restore === false)
            break;
        $update_session = "UPDATE ".TB_PREF."user_sessions SET ".$restore
            ." WHERE id=".$session_id." AND revoked_at IS NULL AND assurance_level=2 AND assurance_method='oidc_acr_amr'"
            ." AND assurance_issued_at=".db_escape((string)$row['assurance_issued_at'])
            ." AND assurance_expires_at=".db_escape((string)$row['assurance_expires_at']);
        if (!db_query($update_session, 'could not reset PAY-SEC-004 session assurance after single-use consumption')
            || (int)db_num_affected_rows() !== 1)
            break;
        if (!federation_assurance_consumer_audit($company, $row, $event, $action_hash, $consumed_at))
            break;
        $success = true;
        $result_value = array(
            'assurance_event_id'=>(int)$event['assurance_event_id'],
            'session_id'=>$session_id,
            'action_class'=>FEDERATION_ASSURANCE_PAYROLL_POST_ACTION_CLASS,
            'action_hash'=>$action_hash,
            'consumed_at'=>$consumed_at,
            'session_assurance_reset'=>true,
            'business_action_committed'=>false
        );
    } while (false);

    if (!$success) {
        @db_query('ROLLBACK TO SAVEPOINT pay_sec_004_assurance_consumer', 'could not roll back PAY-SEC-004 assurance consumer savepoint');
        @db_query('RELEASE SAVEPOINT pay_sec_004_assurance_consumer', 'could not release failed PAY-SEC-004 assurance consumer savepoint');
        return false;
    }
    if (!db_query('RELEASE SAVEPOINT pay_sec_004_assurance_consumer', 'could not release PAY-SEC-004 assurance consumer savepoint'))
        return false;
    return $result_value;
}

/**
 * Reset an expired current level-two session to its underlying accepted
 * level-one OIDC decision. Expired events remain immutable history.
 */
function federation_reset_expired_privileged_assurance($company, $session_id, $now_epoch = null)
{
    global $path_to_root;
    $company = (int)$company;
    $session_id = (int)$session_id;
    $now = $now_epoch === null ? time() : $now_epoch;
    if ($company < 0 || $session_id <= 0 || !is_int($now) || $now <= 0
        || federation_assurance_consumer_current_session_id() !== $session_id)
        return false;
    if (function_exists('user_company') && (int)user_company() !== $company)
        return false;
    if (!function_exists('append_domain_audit_event')) {
        $root = isset($path_to_root) ? realpath($path_to_root) : false;
        if ($root === false)
            return false;
        require_once($root.'/includes/db/domain_audit_db.inc');
    }
    if (!function_exists('append_domain_audit_event') || !db_query('BEGIN', 'could not start PAY-SEC-004 expired-assurance reset transaction'))
        return false;
    $committed = false;
    do {
        $row = federation_assurance_consumer_custody_for_update($session_id);
        if (!federation_assurance_consumer_current_custody_valid($row, $now, false))
            break;
        $expires = $row['assurance_expires_at'] === null ? false : strtotime((string)$row['assurance_expires_at'].' UTC');
        if ((int)$row['assurance_level'] !== 2 || (string)$row['assurance_method'] !== 'oidc_acr_amr'
            || $expires === false || $expires > $now)
            break;
        $now_at = gmdate('Y-m-d H:i:s', $now);
        $active = db_query("SELECT assurance_event_id FROM ".TB_PREF."user_session_assurance_events WHERE session_id=".$session_id
            ." AND consumed_at IS NULL AND expires_at>".db_escape($now_at)." LIMIT 1 FOR UPDATE",
            'could not verify expired PAY-SEC-004 assurance events');
        if (!$active || db_fetch_assoc($active) !== false)
            break;
        $restore = federation_assurance_consumer_restore_level_one_sql($row, $now);
        if ($restore === false)
            break;
        if (!db_query("UPDATE ".TB_PREF."user_sessions SET ".$restore." WHERE id=".$session_id
            ." AND revoked_at IS NULL AND assurance_level=2 AND assurance_method='oidc_acr_amr' AND assurance_expires_at<=".db_escape($now_at),
            'could not reset expired PAY-SEC-004 privileged assurance')
            || !function_exists('db_num_affected_rows') || (int)db_num_affected_rows() !== 1)
            break;
        if (append_domain_audit_event(array(
            'company_id'=>$company,'legal_entity_id'=>0,
            'entity_type'=>'security_session_assurance','entity_key'=>array('session_id'=>$session_id),
            'action'=>'security.session.assurance.expired_reset','reason_code'=>'pay_sec_004_privileged_action_consumer',
            'before'=>array('assurance_level'=>2),'after'=>array('assurance_level'=>1),
            'restricted_diff'=>array('expired_event_history_preserved'=>true,'session_assurance_reset_to_level_one'=>true,
                'privileged_action_consumed'=>false,'raw_oidc_claims_stored'=>false,'recovery_factor_written'=>false,
                'break_glass_authority_added'=>false,'payment_authority_changed'=>false,'payroll_authority_changed'=>false,
                'accounting_authority_changed'=>false)
        )) === false)
            break;
        if (!db_query('COMMIT', 'could not commit PAY-SEC-004 expired-assurance reset'))
            break;
        $committed = true;
    } while (false);
    if (!$committed)
        @db_query('ROLLBACK', 'could not roll back PAY-SEC-004 expired-assurance reset');
    return $committed;
}
