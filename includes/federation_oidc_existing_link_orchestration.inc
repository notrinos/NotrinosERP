<?php
/**
 * PAY-SEC-004 existing-link OIDC callback/login orchestration command.
 *
 * Internal composition only. This command accepts already-parsed callback
 * state/code plus the exact configured redirect URI, acquires the accepted
 * callback lease, performs one bounded code exchange, verifies the returned
 * ID token against pinned private material, atomically commits replay/consume/
 * decision state, requires an existing explicit external-subject link, issues
 * one durable level-one federated session, and adopts it into the current
 * anonymous PHP/browser session. It exposes no public route or redirect and
 * never auto-links by email, display name, login id or another profile claim.
 */

if (!defined('FEDERATION_OIDC_EXISTING_LINK_DEFAULT_LEASE'))
    define('FEDERATION_OIDC_EXISTING_LINK_DEFAULT_LEASE', 45);

/** Load the exact already-accepted command dependencies without adding a route. */
function federation_oidc_existing_link_orchestration_dependencies()
{
    $base = dirname(__FILE__);
    $required = array(
        'federation_oidc_acquire_callback_claim'=>'federation_oidc_callback_claim.inc',
        'federation_oidc_exchange_authorization_code'=>'federation_oidc_token_exchange.inc',
        'federation_oidc_token_verifier_verify'=>'federation_oidc_token_verifier.inc',
        'federation_oidc_token_verifier_material_contract'=>'federation_oidc_token_material.inc',
        'federation_oidc_commit_verified_decision'=>'federation_oidc_decision.inc',
        'federation_issue_existing_link_session'=>'federation_session_issuer.inc',
        'federation_adopt_existing_browser_session'=>'federation_browser_session_adoption.inc',
        'federation_control_load_bundle'=>'federation_control_material.inc',
        'federation_oidc_load_proof'=>'federation_oidc_transaction.inc',
        'federation_verifier_dependency_configured_status'=>'federation_verifier_dependencies.inc'
    );
    foreach ($required as $function=>$file) {
        if (!function_exists($function)) {
            $path = $base.DIRECTORY_SEPARATOR.$file;
            if (!is_file($path)) return false;
            require_once($path);
        }
    }
    foreach ($required as $function=>$file)
        if (!function_exists($function)) return false;
    return true;
}

/** Return a bounded result that never carries callback, token or session secrets. */
function federation_oidc_existing_link_orchestration_result($status, $reason, $extra = array())
{
    $allowed_status = array('authenticated','retry_later','link_required','denied');
    if (!in_array($status, $allowed_status, true) || !is_string($reason) || $reason === '' || !is_array($extra))
        return false;
    $result = array('status'=>$status,'reason'=>$reason,'retry_safe'=>$status === 'retry_later');
    foreach (array('retry_after_epoch','session_id','user_id','role_id','assurance_level','session_cleanup_complete') as $key)
        if (array_key_exists($key, $extra)) $result[$key] = $extra[$key];
    return $result;
}

/** The successful existing-link callback must start from an anonymous browser session. */
function federation_oidc_existing_link_orchestration_anonymous()
{
    return isset($_SESSION['wa_current_user']) && is_object($_SESSION['wa_current_user'])
        && method_exists($_SESSION['wa_current_user'], 'logged_in')
        && !$_SESSION['wa_current_user']->logged_in();
}

/** Read the immutable transaction/configuration/provider binding for this claim. */
function federation_oidc_existing_link_orchestration_context($transaction_id)
{
    $transaction_id = (int)$transaction_id;
    if ($transaction_id <= 0 || !defined('TB_PREF')) return false;
    $sql = "SELECT t.verifier_transaction_id,t.verifier_config_id,t.transaction_key_hash,t.nonce_hash,t.proof_ref_hash,t.expires_at,t.consumed_at,"
        ."c.provider_id,c.verifier_profile,c.audience_hash,c.trust_material_ref_hash,c.config_status,"
        ."p.protocol,p.issuer_hash,p.provider_status "
        ."FROM ".TB_PREF."user_federation_verifier_transactions t "
        ."INNER JOIN ".TB_PREF."user_federation_provider_verifier_configs c ON c.verifier_config_id=t.verifier_config_id "
        ."INNER JOIN ".TB_PREF."user_federation_providers p ON p.provider_id=c.provider_id "
        ."WHERE t.verifier_transaction_id=$transaction_id LIMIT 2";
    $result = db_query($sql, 'could not read PAY-SEC-004 existing-link callback orchestration context');
    if (!$result) return false;
    $row = db_fetch_assoc($result);
    if (!$row || db_fetch_assoc($result) !== false) return false;
    return $row;
}

/** Generate one server-confined durable-session secret; never return it to a browser caller. */
function federation_oidc_existing_link_orchestration_session_secret()
{
    $bytes = federation_oidc_random_bytes(32);
    if ($bytes === false) return false;
    $secret = federation_oidc_base64url_encode($bytes);
    return is_string($secret) && preg_match('/^[A-Za-z0-9_-]{43}$/D', $secret) === 1 ? $secret : false;
}

/** Parse only enough untrusted token structure to bind its nonce hash before verification. */
function federation_oidc_existing_link_orchestration_nonce($jwt, $expected_nonce_hash)
{
    if (!is_string($expected_nonce_hash) || preg_match('/^[a-f0-9]{64}$/D', $expected_nonce_hash) !== 1)
        return false;
    $parsed = federation_oidc_token_verifier_parse($jwt);
    if (!is_array($parsed) || !isset($parsed['claims']['nonce']) || !is_string($parsed['claims']['nonce'])
        || preg_match('/^[A-Za-z0-9_-]{32,128}$/D', $parsed['claims']['nonce']) !== 1)
        return false;
    $actual = hash('sha256', $parsed['claims']['nonce']);
    if (!function_exists('federation_control_hash_equals')
        || !federation_control_hash_equals($expected_nonce_hash, $actual))
        return false;
    return $parsed['claims']['nonce'];
}

/** Best-effort exact cleanup for a session issued by this same orchestration attempt. */
function federation_oidc_existing_link_orchestration_cleanup_session($session_id)
{
    $session_id = (int)$session_id;
    if ($session_id <= 0 || !function_exists('session_registry_revoke_session')) return false;
    return session_registry_revoke_session($session_id, 0, 'oidc_adoption_failed') === true;
}

/**
 * Compose the accepted existing-link OIDC callback/login commands.
 *
 * Optional transport/signature callbacks are the same focused-test seams
 * already accepted by the bounded exchange and pinned verifier helpers.
 * Production callers pass null for both.
 */
function federation_oidc_orchestrate_existing_link_callback($company, $state, $authorization_code, $redirect_uri,
    $lease_seconds = FEDERATION_OIDC_EXISTING_LINK_DEFAULT_LEASE, $transport = null, $signature_verifier = null)
{
    global $path_to_root;
    $company = (int)$company; $lease_seconds = (int)$lease_seconds;
    if ($company < 0)
        return federation_oidc_existing_link_orchestration_result('denied', 'callback_invalid');
    if (!federation_oidc_existing_link_orchestration_anonymous())
        return federation_oidc_existing_link_orchestration_result('denied', 'browser_session_not_anonymous');
    if (!federation_oidc_existing_link_orchestration_dependencies())
        return federation_oidc_existing_link_orchestration_result('retry_later', 'runtime_dependency_unavailable');
    if (!is_string($state) || preg_match('/^[A-Za-z0-9_-]{43}$/D', $state) !== 1
        || !is_string($authorization_code) || strlen($authorization_code) < 1
        || strlen($authorization_code) > FEDERATION_OIDC_EXCHANGE_MAX_CODE_BYTES
        || preg_match('/[\x00-\x20\x7f]/', $authorization_code) === 1
        || !is_string($redirect_uri) || !federation_oidc_valid_https_uri($redirect_uri, true)
        || $lease_seconds < FEDERATION_OIDC_CALLBACK_CLAIM_MIN_LEASE
        || $lease_seconds > FEDERATION_OIDC_CALLBACK_CLAIM_MAX_LEASE)
        return federation_oidc_existing_link_orchestration_result('denied', 'callback_invalid');
    if (function_exists('user_company') && (int)user_company() !== $company)
        return federation_oidc_existing_link_orchestration_result('denied', 'company_mismatch');

    $root = isset($path_to_root) ? realpath($path_to_root) : false;
    if ($root === false)
        return federation_oidc_existing_link_orchestration_result('retry_later', 'runtime_dependency_unavailable');
    $dependency = federation_verifier_dependency_configured_status($root);
    if (!is_array($dependency) || empty($dependency['available']))
        return federation_oidc_existing_link_orchestration_result('retry_later', 'verifier_dependency_unavailable');

    $claim = federation_oidc_acquire_callback_claim($company, $state, $lease_seconds);
    if (!is_array($claim))
        return federation_oidc_existing_link_orchestration_result('denied', 'callback_claim_unavailable');
    $claim_token = (string)$claim['claim_token'];
    $retry_after = (int)$claim['claim_expires_at_epoch'];

    $context = federation_oidc_existing_link_orchestration_context((int)$claim['verifier_transaction_id']);
    if (!is_array($context)
        || (int)$context['verifier_config_id'] !== (int)$claim['verifier_config_id']
        || (string)$context['nonce_hash'] !== (string)$claim['nonce_hash']
        || (string)$context['proof_ref_hash'] !== (string)$claim['proof_ref_hash']
        || (string)$context['transaction_key_hash'] !== hash('sha256', $state)
        || (string)$context['protocol'] !== 'oidc'
        || (string)$context['verifier_profile'] !== 'oidc_code_pkce_v1') {
        federation_oidc_terminalize_callback_claim($company, $claim_token, 'callback_invalid');
        return federation_oidc_existing_link_orchestration_result('denied', 'callback_context_invalid');
    }
    if ((string)$context['provider_status'] !== 'enabled') {
        federation_oidc_terminalize_callback_claim($company, $claim_token, 'provider_disabled');
        return federation_oidc_existing_link_orchestration_result('denied', 'provider_disabled');
    }
    if ((string)$context['config_status'] !== 'enabled') {
        federation_oidc_terminalize_callback_claim($company, $claim_token, 'configuration_disabled');
        return federation_oidc_existing_link_orchestration_result('denied', 'configuration_disabled');
    }
    if ($context['consumed_at'] !== null) {
        federation_oidc_terminalize_callback_claim($company, $claim_token, 'callback_invalid');
        return federation_oidc_existing_link_orchestration_result('denied', 'transaction_already_consumed');
    }

    $now = time();
    $transaction_expires = strtotime((string)$context['expires_at'].' UTC');
    if ($transaction_expires === false || $now >= $transaction_expires) {
        federation_oidc_terminalize_callback_claim($company, $claim_token, 'transaction_expired');
        return federation_oidc_existing_link_orchestration_result('denied', 'transaction_expired');
    }
    $proof = federation_oidc_load_proof($company, (string)$claim['proof_ref_hash'], $now);
    if (!is_array($proof)
        || (int)$proof['verifier_config_id'] !== (int)$claim['verifier_config_id']
        || (string)$proof['transaction_key_hash'] !== hash('sha256', $state)) {
        federation_oidc_terminalize_callback_claim($company, $claim_token, 'proof_unavailable');
        return federation_oidc_existing_link_orchestration_result('denied', 'proof_unavailable');
    }

    if (!function_exists('federation_control_valid_sha256')
        || !federation_control_valid_sha256((string)$context['trust_material_ref_hash'])
        || !federation_control_valid_sha256((string)$context['issuer_hash'])
        || !federation_control_valid_sha256((string)$context['audience_hash']))
        return federation_oidc_existing_link_orchestration_result('retry_later', 'verifier_material_unavailable', array('retry_after_epoch'=>$retry_after));
    $bundle = federation_control_load_bundle($company, (string)$context['trust_material_ref_hash'], 'oidc', 'oidc_code_pkce_v1');
    if (!is_array($bundle) || !isset($bundle['materials']) || !is_array($bundle['materials']))
        return federation_oidc_existing_link_orchestration_result('retry_later', 'verifier_material_unavailable', array('retry_after_epoch'=>$retry_after));
    $materials = $bundle['materials'];
    $contract = federation_oidc_token_verifier_material_contract($materials);
    $client = isset($materials['oidc_client_config']) ? federation_oidc_client_config($materials['oidc_client_config']) : false;
    if ($contract === false || $client === false || !in_array($redirect_uri, $client['redirect_uris'], true)
        || !federation_control_hash_equals(hash('sha256', $contract['issuer']), (string)$context['issuer_hash'])
        || !federation_control_hash_equals(hash('sha256', $contract['client_id']), (string)$context['audience_hash']))
        return federation_oidc_existing_link_orchestration_result('retry_later', 'verifier_material_unavailable', array('retry_after_epoch'=>$retry_after));

    $exchange = federation_oidc_exchange_authorization_code($authorization_code, $redirect_uri, (string)$proof['code_verifier'], $materials, $transport);
    if (!is_array($exchange) || !isset($exchange['classification'])) {
        federation_oidc_terminalize_callback_claim($company, $claim_token, 'exchange_uncertain');
        return federation_oidc_existing_link_orchestration_result('denied', 'exchange_uncertain');
    }
    if ((string)$exchange['classification'] === 'pre_send_failure')
        return federation_oidc_existing_link_orchestration_result('retry_later', 'exchange_pre_send_failure', array('retry_after_epoch'=>$retry_after));
    if ((string)$exchange['classification'] === 'exchange_rejected') {
        federation_oidc_terminalize_callback_claim($company, $claim_token, 'exchange_rejected');
        return federation_oidc_existing_link_orchestration_result('denied', 'exchange_rejected');
    }
    if ((string)$exchange['classification'] !== 'exchange_succeeded' || !isset($exchange['id_token'])) {
        federation_oidc_terminalize_callback_claim($company, $claim_token, 'exchange_uncertain');
        return federation_oidc_existing_link_orchestration_result('denied', 'exchange_uncertain');
    }

    $nonce = federation_oidc_existing_link_orchestration_nonce((string)$exchange['id_token'], (string)$claim['nonce_hash']);
    if ($nonce === false) {
        federation_oidc_terminalize_callback_claim($company, $claim_token, 'verifier_rejected');
        return federation_oidc_existing_link_orchestration_result('denied', 'nonce_rejected');
    }
    $candidate = federation_oidc_token_verifier_verify((string)$exchange['id_token'], array(
        'company_id'=>$company,
        'provider_id'=>(int)$context['provider_id'],
        'issuer'=>(string)$contract['issuer'],
        'client_id'=>(string)$contract['client_id'],
        'nonce'=>$nonce
    ), $materials, $now, $signature_verifier);
    if (!is_array($candidate) || empty($candidate['verified'])) {
        federation_oidc_terminalize_callback_claim($company, $claim_token, 'verifier_rejected');
        return federation_oidc_existing_link_orchestration_result('denied', 'verifier_rejected');
    }

    /* Exact bounded retry handles a locally uncertain commit without replay broadening. */
    $decision = federation_oidc_commit_verified_decision($company, $claim_token, $authorization_code, $candidate);
    if (!is_array($decision))
        $decision = federation_oidc_commit_verified_decision($company, $claim_token, $authorization_code, $candidate);
    if (!is_array($decision) || !isset($decision['verifier_decision_id']))
        return federation_oidc_existing_link_orchestration_result('denied', 'decision_commit_failed');

    $eligibility = federation_oidc_existing_link_eligibility((int)$decision['verifier_decision_id'], $now);
    if (!is_array($eligibility) || empty($eligibility['eligible'])) {
        $reason = is_array($eligibility) && isset($eligibility['reason']) ? (string)$eligibility['reason'] : 'existing_link_ineligible';
        if ($reason === 'existing_link_required')
            return federation_oidc_existing_link_orchestration_result('link_required', 'existing_link_required');
        return federation_oidc_existing_link_orchestration_result('denied', $reason);
    }

    $session_secret = federation_oidc_existing_link_orchestration_session_secret();
    if ($session_secret === false)
        return federation_oidc_existing_link_orchestration_result('denied', 'session_secret_generation_failed');
    $user_agent_hash = session_registry_user_agent_hash();
    $issued = federation_issue_existing_link_session($company, (int)$decision['verifier_decision_id'], $session_secret, $user_agent_hash, $now);
    if (!is_array($issued))
        $issued = federation_issue_existing_link_session($company, (int)$decision['verifier_decision_id'], $session_secret, $user_agent_hash, $now);
    if (!is_array($issued) || !isset($issued['session_id'])) {
        unset($session_secret);
        return federation_oidc_existing_link_orchestration_result('denied', 'session_issuance_failed');
    }

    $adopted = federation_adopt_existing_browser_session($company, (int)$issued['session_id'], $session_secret, $now);
    if (!is_array($adopted))
        $adopted = federation_adopt_existing_browser_session($company, (int)$issued['session_id'], $session_secret, $now);
    if (!is_array($adopted) || empty($adopted['browser_session_activated'])) {
        $cleanup = federation_oidc_existing_link_orchestration_cleanup_session((int)$issued['session_id']);
        unset($session_secret);
        return federation_oidc_existing_link_orchestration_result('denied', 'browser_adoption_failed', array('session_cleanup_complete'=>$cleanup));
    }
    unset($session_secret);
    return federation_oidc_existing_link_orchestration_result('authenticated', 'ok', array(
        'session_id'=>(int)$adopted['session_id'],
        'user_id'=>(int)$adopted['user_id'],
        'role_id'=>(int)$adopted['role_id'],
        'assurance_level'=>(int)$adopted['assurance_level']
    ));
}
