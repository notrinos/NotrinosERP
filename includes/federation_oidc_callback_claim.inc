<?php
/**
 * PAY-SEC-004 OIDC callback-claim lease command foundation.
 *
 * Internal command helper only. It acquires or recovers a short-lived lease
 * over an existing unconsumed OIDC verifier transaction by hashing caller-held
 * state, stores only a hash of a newly generated opaque claim token, and can
 * terminalize the exact live lease. It performs no HTTP callback parsing,
 * authorization-code exchange, token/JWT verification, replay insertion,
 * transaction consumption, verifier-decision append, subject linking, login,
 * session issuance/revocation or assurance elevation.
 */

if (!defined('FEDERATION_OIDC_CALLBACK_CLAIM_DEFAULT_LEASE'))
    define('FEDERATION_OIDC_CALLBACK_CLAIM_DEFAULT_LEASE', 45);
if (!defined('FEDERATION_OIDC_CALLBACK_CLAIM_MIN_LEASE'))
    define('FEDERATION_OIDC_CALLBACK_CLAIM_MIN_LEASE', 15);
if (!defined('FEDERATION_OIDC_CALLBACK_CLAIM_MAX_LEASE'))
    define('FEDERATION_OIDC_CALLBACK_CLAIM_MAX_LEASE', 120);

function federation_oidc_callback_claim_token_hash($claim_token)
{
    if (!is_string($claim_token) || preg_match('/^[A-Za-z0-9_-]{43}$/D', $claim_token) !== 1)
        return false;
    return hash('sha256', $claim_token);
}

function federation_oidc_callback_state_hash($state)
{
    if (!is_string($state) || preg_match('/^[A-Za-z0-9_-]{43}$/D', $state) !== 1)
        return false;
    return hash('sha256', $state);
}

function federation_oidc_callback_claim_terminal_reasons()
{
    return array(
        'callback_invalid',
        'transaction_expired',
        'proof_unavailable',
        'provider_disabled',
        'configuration_disabled',
        'exchange_rejected',
        'exchange_uncertain',
        'verifier_rejected'
    );
}

function federation_oidc_callback_claim_valid_terminal_reason($reason)
{
    return is_string($reason) && in_array($reason, federation_oidc_callback_claim_terminal_reasons(), true);
}

function federation_oidc_callback_claim_ensure_dependencies()
{
    global $path_to_root;
    if (!function_exists('federation_oidc_random_bytes') || !function_exists('federation_oidc_base64url_encode'))
        require_once(dirname(__FILE__).DIRECTORY_SEPARATOR.'federation_oidc_transaction.inc');
    if (!function_exists('append_domain_audit_event')) {
        $root = isset($path_to_root) ? realpath($path_to_root) : false;
        if ($root === false) return false;
        $audit = $root.DIRECTORY_SEPARATOR.'includes'.DIRECTORY_SEPARATOR.'db'.DIRECTORY_SEPARATOR.'domain_audit_db.inc';
        if (!is_file($audit)) return false;
        require_once($audit);
    }
    return function_exists('federation_oidc_random_bytes')
        && function_exists('federation_oidc_base64url_encode')
        && function_exists('append_domain_audit_event');
}

function federation_oidc_callback_claim_transaction_by_state_hash($state_hash)
{
    if (!defined('TB_PREF') || !is_string($state_hash) || preg_match('/^[a-f0-9]{64}$/D', $state_hash) !== 1)
        return false;
    $sql = "SELECT t.verifier_transaction_id,t.verifier_config_id,t.transaction_key_hash,t.correlation_hash,t.nonce_hash,t.proof_ref_hash,t.created_at,t.expires_at,t.consumed_at,"
        ."c.provider_id,c.config_version,c.verifier_profile,c.config_status,p.protocol,p.provider_status,p.provider_row_version "
        ."FROM ".TB_PREF."user_federation_verifier_transactions t "
        ."INNER JOIN ".TB_PREF."user_federation_provider_verifier_configs c ON c.verifier_config_id=t.verifier_config_id "
        ."INNER JOIN ".TB_PREF."user_federation_providers p ON p.provider_id=c.provider_id "
        ."WHERE t.transaction_key_hash=".db_escape($state_hash)." LIMIT 2 FOR UPDATE";
    $result = db_query($sql, 'could not lock PAY-SEC-004 OIDC transaction for callback claim');
    if (!$result) return false;
    $row = db_fetch_assoc($result);
    if (!$row || db_fetch_assoc($result) !== false) return false;
    return $row;
}

function federation_oidc_callback_claim_row_for_update($transaction_id)
{
    $transaction_id = (int)$transaction_id;
    if ($transaction_id <= 0 || !defined('TB_PREF')) return false;
    $result = db_query("SELECT callback_claim_id,verifier_transaction_id,claim_token_hash,claim_status,claimed_at,claim_expires_at,terminal_at,terminal_reason FROM ".TB_PREF."user_federation_oidc_callback_claims WHERE verifier_transaction_id=$transaction_id LIMIT 2 FOR UPDATE", 'could not lock PAY-SEC-004 OIDC callback claim');
    if (!$result) return false;
    $row = db_fetch_assoc($result);
    if (!$row) return null;
    if (db_fetch_assoc($result) !== false) return false;
    return $row;
}

function federation_oidc_callback_claim_audit($company, $row, $claim_id, $action, $reason_code, $lease_seconds, $claim_token_hash, $recovered, $terminal_reason)
{
    if (!is_array($row) || (int)$claim_id <= 0 || !is_string($action) || !is_string($reason_code))
        return false;
    return append_domain_audit_event(array(
        'company_id'=>(int)$company,
        'legal_entity_id'=>0,
        'entity_type'=>'security_federation_oidc_callback_claim',
        'entity_key'=>array(
            'callback_claim_id'=>(int)$claim_id,
            'verifier_transaction_id'=>(int)$row['verifier_transaction_id'],
            'verifier_config_id'=>(int)$row['verifier_config_id'],
            'transaction_key_hash'=>(string)$row['transaction_key_hash']
        ),
        'action'=>$action,
        'reason_code'=>$reason_code,
        'before'=>null,
        'after'=>array('callback_claim_state_changed'=>true),
        'restricted_diff'=>array(
            'provider_id'=>(int)$row['provider_id'],
            'provider_row_version'=>(int)$row['provider_row_version'],
            'config_version'=>(int)$row['config_version'],
            'claim_token_hash'=>(string)$claim_token_hash,
            'lease_seconds'=>(int)$lease_seconds,
            'expired_lease_recovered'=>$recovered ? true : false,
            'terminal_reason'=>$terminal_reason,
            'raw_state_stored'=>false,
            'raw_claim_token_stored'=>false,
            'authorization_code_stored'=>false,
            'callback_or_acs_route_added'=>false,
            'network_request_performed'=>false,
            'token_exchange_performed'=>false,
            'token_or_assertion_accepted'=>false,
            'replay_marker_written'=>false,
            'transaction_consumed'=>false,
            'verifier_decision_written'=>false,
            'external_subject_link_written'=>false,
            'federated_login_issued'=>false,
            'session_revocation_changed'=>false,
            'assurance_elevated'=>false,
            'payment_authority_changed'=>false,
            'payroll_authority_changed'=>false,
            'accounting_authority_changed'=>false
        )
    ));
}

/**
 * Acquire an exclusive short-lived lease for an existing OIDC transaction.
 *
 * Returns a caller-held opaque claim token and non-secret identifiers. An
 * unexpired lease is never disclosed or stolen. An expired non-terminal lease
 * is recovered by rotating the token hash under the same row lock.
 */
function federation_oidc_acquire_callback_claim($company, $state, $lease_seconds)
{
    $company=(int)$company;$lease_seconds=(int)$lease_seconds;
    if ($company < 0 || $lease_seconds < FEDERATION_OIDC_CALLBACK_CLAIM_MIN_LEASE || $lease_seconds > FEDERATION_OIDC_CALLBACK_CLAIM_MAX_LEASE)
        return false;
    $state_hash=federation_oidc_callback_state_hash($state);
    if ($state_hash===false || !federation_oidc_callback_claim_ensure_dependencies()) return false;
    if (!db_query('BEGIN', 'could not start PAY-SEC-004 OIDC callback-claim lease transaction')) return false;
    $committed=false;
    do {
        $row=federation_oidc_callback_claim_transaction_by_state_hash($state_hash);
        if (!is_array($row)
            || (string)$row['protocol']!=='oidc'
            || (string)$row['verifier_profile']!=='oidc_code_pkce_v1'
            || (string)$row['provider_status']!=='enabled'
            || (string)$row['config_status']!=='enabled'
            || !is_null($row['consumed_at'])
            || !is_string($row['proof_ref_hash']) || preg_match('/^[a-f0-9]{64}$/D',(string)$row['proof_ref_hash'])!==1)
            break;
        $now=time();$transaction_expires=strtotime((string)$row['expires_at'].' UTC');
        if ($transaction_expires===false || $now >= $transaction_expires) break;
        $claim=federation_oidc_callback_claim_row_for_update((int)$row['verifier_transaction_id']);
        if ($claim===false) break;
        $recovered=false;
        if (is_array($claim)) {
            if ((string)$claim['claim_status']!=='claimed' || $claim['terminal_at']!==null || $claim['terminal_reason']!==null) break;
            $claim_expires=strtotime((string)$claim['claim_expires_at'].' UTC');
            if ($claim_expires===false || $now < $claim_expires) break;
            $recovered=true;
        }
        $token_bytes=federation_oidc_random_bytes(32);if($token_bytes===false)break;
        $claim_token=federation_oidc_base64url_encode($token_bytes);$claim_hash=federation_oidc_callback_claim_token_hash($claim_token);
        if($claim_hash===false)break;
        $claimed_at=gmdate('Y-m-d H:i:s',$now);$expires_at=gmdate('Y-m-d H:i:s',$now+$lease_seconds);
        if ($claim===null) {
            $sql="INSERT INTO ".TB_PREF."user_federation_oidc_callback_claims(verifier_transaction_id,claim_token_hash,claim_status,claimed_at,claim_expires_at,terminal_at,terminal_reason) VALUES ("
                .(int)$row['verifier_transaction_id'].",".db_escape($claim_hash).",'claimed',".db_escape($claimed_at).",".db_escape($expires_at).",NULL,NULL)";
            if(!db_query($sql,'could not create PAY-SEC-004 OIDC callback claim lease'))break;
            $claim_id=(int)db_insert_id();if($claim_id<=0)break;
            $action='security.federation.oidc_claim.acquired';
        } else {
            $claim_id=(int)$claim['callback_claim_id'];
            $sql="UPDATE ".TB_PREF."user_federation_oidc_callback_claims SET claim_token_hash=".db_escape($claim_hash).",claim_status='claimed',claimed_at=".db_escape($claimed_at).",claim_expires_at=".db_escape($expires_at).",terminal_at=NULL,terminal_reason=NULL WHERE callback_claim_id=$claim_id";
            if(!db_query($sql,'could not recover expired PAY-SEC-004 OIDC callback claim lease'))break;
            $action='security.federation.oidc_claim.reacquired';
        }
        if(federation_oidc_callback_claim_audit($company,$row,$claim_id,$action,'pay_sec_004_oidc_callback_claim_lease',$lease_seconds,$claim_hash,$recovered,null)===false)break;
        if(!db_query('COMMIT','could not commit PAY-SEC-004 OIDC callback-claim lease transaction'))break;
        $committed=true;
        return array(
            'callback_claim_id'=>$claim_id,
            'verifier_transaction_id'=>(int)$row['verifier_transaction_id'],
            'verifier_config_id'=>(int)$row['verifier_config_id'],
            'proof_ref_hash'=>(string)$row['proof_ref_hash'],
            'nonce_hash'=>(string)$row['nonce_hash'],
            'claim_token'=>$claim_token,
            'claim_expires_at_epoch'=>$now+$lease_seconds,
            'transaction_expires_at_epoch'=>$transaction_expires,
            'recovered_expired_lease'=>$recovered
        );
    } while(false);
    if(!$committed) @db_query('ROLLBACK','could not roll back PAY-SEC-004 OIDC callback-claim lease transaction');
    return false;
}

/** Terminalize only the exact still-live lease token. Terminal rows never retry. */
function federation_oidc_terminalize_callback_claim($company, $claim_token, $terminal_reason)
{
    $company=(int)$company;
    $claim_hash=federation_oidc_callback_claim_token_hash($claim_token);
    if($company<0||$claim_hash===false||!federation_oidc_callback_claim_valid_terminal_reason($terminal_reason)||!federation_oidc_callback_claim_ensure_dependencies())return false;
    if(!db_query('BEGIN','could not start PAY-SEC-004 OIDC callback-claim terminal transaction'))return false;
    $committed=false;
    do {
        $sql="SELECT cc.callback_claim_id,cc.verifier_transaction_id,cc.claim_token_hash,cc.claim_status,cc.claim_expires_at,cc.terminal_at,cc.terminal_reason,t.verifier_config_id,t.transaction_key_hash,t.nonce_hash,t.proof_ref_hash,c.provider_id,c.config_version,p.provider_row_version FROM ".TB_PREF."user_federation_oidc_callback_claims cc INNER JOIN ".TB_PREF."user_federation_verifier_transactions t ON t.verifier_transaction_id=cc.verifier_transaction_id INNER JOIN ".TB_PREF."user_federation_provider_verifier_configs c ON c.verifier_config_id=t.verifier_config_id INNER JOIN ".TB_PREF."user_federation_providers p ON p.provider_id=c.provider_id WHERE cc.claim_token_hash=".db_escape($claim_hash)." LIMIT 2 FOR UPDATE";
        $r=db_query($sql,'could not lock PAY-SEC-004 OIDC callback claim for terminalization');if(!$r)break;
        $row=db_fetch_assoc($r);if(!$row||db_fetch_assoc($r)!==false)break;
        $now=time();$claim_expires=strtotime((string)$row['claim_expires_at'].' UTC');
        if((string)$row['claim_status']!=='claimed'||$row['terminal_at']!==null||$row['terminal_reason']!==null||$claim_expires===false||$now>=$claim_expires)break;
        $claim_id=(int)$row['callback_claim_id'];$terminal_at=gmdate('Y-m-d H:i:s',$now);
        if(!db_query("UPDATE ".TB_PREF."user_federation_oidc_callback_claims SET claim_status='terminal',terminal_at=".db_escape($terminal_at).",terminal_reason=".db_escape($terminal_reason)." WHERE callback_claim_id=$claim_id",'could not terminalize PAY-SEC-004 OIDC callback claim'))break;
        if(federation_oidc_callback_claim_audit($company,$row,$claim_id,'security.federation.oidc_claim.terminalized','pay_sec_004_oidc_callback_claim_terminal',0,$claim_hash,false,$terminal_reason)===false)break;
        if(!db_query('COMMIT','could not commit PAY-SEC-004 OIDC callback-claim terminal transaction'))break;
        $committed=true;return true;
    }while(false);
    if(!$committed)@db_query('ROLLBACK','could not roll back PAY-SEC-004 OIDC callback-claim terminal transaction');
    return false;
}
