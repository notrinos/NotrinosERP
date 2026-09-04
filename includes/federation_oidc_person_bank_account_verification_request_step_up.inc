<?php
/**
 * HRM-FND-004 governed Person bank-account verification-request OIDC step-up orchestration foundation.
 *
 * Internal composition only. This helper binds a fresh OIDC authorization-code
 * transaction to the currently authenticated federated durable session and to
 * one canonical approved-bank-account-verification-request action hash. The callback reuses the
 * accepted state/nonce/PKCE/replay machinery, verifies a fresh signed level-two
 * assurance candidate, and invokes the existing action-bound assurance issuer.
 *
 * This file exposes no HTTP route, performs no payroll/payment/accounting write,
 * consumes no privileged action, and accepts no browser-controlled return URL.
 */

if (!defined('FEDERATION_OIDC_BANK_ACCOUNT_VERIFICATION_REQUEST_STEP_UP_CSRF_TTL'))
    define('FEDERATION_OIDC_BANK_ACCOUNT_VERIFICATION_REQUEST_STEP_UP_CSRF_TTL', 600);
if (!defined('FEDERATION_OIDC_BANK_ACCOUNT_VERIFICATION_REQUEST_STEP_UP_TRANSACTION_TTL'))
    define('FEDERATION_OIDC_BANK_ACCOUNT_VERIFICATION_REQUEST_STEP_UP_TRANSACTION_TTL', 300);
if (!defined('FEDERATION_OIDC_BANK_ACCOUNT_VERIFICATION_REQUEST_STEP_UP_MAX_PENDING'))
    define('FEDERATION_OIDC_BANK_ACCOUNT_VERIFICATION_REQUEST_STEP_UP_MAX_PENDING', 4);
if (!defined('FEDERATION_OIDC_BANK_ACCOUNT_VERIFICATION_REQUEST_STEP_UP_DEFAULT_LEASE'))
    define('FEDERATION_OIDC_BANK_ACCOUNT_VERIFICATION_REQUEST_STEP_UP_DEFAULT_LEASE', 45);

/** Load accepted generic federation dependencies and the governed HRM command. */
function federation_oidc_bank_account_verification_request_step_up_dependencies()
{
    global $path_to_root;
    $base = dirname(__FILE__);
    $required = array(
        'federation_assurance_consumer_current_custody_valid'=>'federation_assurance_consumer.inc',
        'federation_oidc_issue_transaction'=>'federation_oidc_transaction.inc',
        'federation_oidc_load_proof'=>'federation_oidc_transaction.inc',
        'federation_oidc_client_config'=>'federation_oidc_transaction.inc',
        'federation_oidc_acquire_callback_claim'=>'federation_oidc_callback_claim.inc',
        'federation_oidc_terminalize_callback_claim'=>'federation_oidc_callback_claim.inc',
        'federation_oidc_exchange_authorization_code'=>'federation_oidc_token_exchange.inc',
        'federation_oidc_token_verifier_verify'=>'federation_oidc_token_verifier.inc',
        'federation_oidc_token_verifier_parse'=>'federation_oidc_token_verifier.inc',
        'federation_oidc_token_verifier_material_contract'=>'federation_oidc_token_material.inc',
        'federation_oidc_assurance_candidate_verify'=>'federation_oidc_assurance_candidate.inc',
        'federation_oidc_assurance_policy'=>'federation_oidc_assurance_candidate.inc',
        'federation_oidc_commit_verified_decision'=>'federation_oidc_decision.inc',
        'federation_issue_privileged_assurance'=>'federation_assurance_issuer.inc',
        'federation_control_load_bundle'=>'federation_control_material.inc',
        'federation_control_hash_equals'=>'federation_control_material.inc',
        'federation_control_valid_sha256'=>'federation_control_material.inc',
        'federation_verifier_dependency_configured_status'=>'federation_verifier_dependencies.inc'
    );
    foreach ($required as $function=>$file) {
        if (!function_exists($function)) {
            $path = $base.DIRECTORY_SEPARATOR.$file;
            if (!is_file($path)) return false;
            require_once($path);
        }
    }
    foreach ($required as $function=>$file) if (!function_exists($function)) return false;
    if (!function_exists('hrm_bank_account_verification_request_browser_successor')) {
        $root = isset($path_to_root) ? realpath($path_to_root) : false;
        $file = $root === false ? false : $root.DIRECTORY_SEPARATOR.'hrm'.DIRECTORY_SEPARATOR.'includes'.DIRECTORY_SEPARATOR.'db'.DIRECTORY_SEPARATOR.'person_bank_account_verification_request_browser_db.inc';
        if ($file === false || !is_file($file)) return false;
        include_once($file);
    }
    return function_exists('hrm_bank_account_verification_request_permission')
        && function_exists('hrm_bank_account_verification_request_browser_successor')
        && defined('HRM_BANK_ACCOUNT_VERIFICATION_REQUEST_ACTION_CLASS');
}

/** Return a bounded result with no bank plaintext, OAuth token or proof material. */
function federation_oidc_bank_account_verification_request_step_up_result($status, $reason, $extra = array())
{
    if (!in_array($status, array('authorization_required','assured','retry_later','denied'), true)
        || !is_string($reason) || $reason === '' || !is_array($extra)) return false;
    $result = array('status'=>$status,'reason'=>$reason,'retry_safe'=>$status === 'retry_later');
    foreach (array('authorization_url','expires_at_epoch','retry_after_epoch','assurance_event_id','session_id',
        'action_class','action_hash','employee_ref','person_bank_account_id','bank_account_row_version','verifier_transaction_id') as $key)
        if (array_key_exists($key, $extra)) $result[$key] = $extra[$key];
    return $result;
}

/** Reuse the existing restricted-financial bank-change permission; add no new grant. */
function federation_oidc_bank_account_verification_request_step_up_authorized()
{
    return function_exists('hrm_bank_account_verification_request_permission')
        && hrm_bank_account_verification_request_permission();
}

/** Current durable federated session id, never a browser parameter. */
function federation_oidc_bank_account_verification_request_step_up_current_session_id()
{
    return isset($_SESSION['security_session_registry_id']) ? (int)$_SESSION['security_session_registry_id'] : 0;
}

/**
 * Read the current federated session/configuration custody without mutation.
 *
 * The issuer will repeat this validation under lock before elevation. This read
 * exists only to bind the authorization start to the same durable session and
 * level-two-capable verifier configuration.
 */
function federation_oidc_bank_account_verification_request_step_up_session_context($session_id, $now_epoch)
{
    $session_id = (int)$session_id;
    if ($session_id <= 0 || !is_int($now_epoch) || $now_epoch <= 0 || !defined('TB_PREF'))
        return false;
    $sql = "SELECT s.id session_id,s.session_key_hash,s.user_id,s.role_id,s.auth_method,s.assurance_level,s.assurance_method,"
        ."s.assurance_issued_at,s.assurance_expires_at,s.idle_expires_at,s.absolute_expires_at,s.revoked_at,s.user_agent_hash,"
        ."b.verifier_decision_id,b.external_subject_link_id,d.external_subject_hash,d.verified_assurance_level decision_assurance_level,d.assurance_method decision_assurance_method,"
        ."d.verified_at decision_verified_at,d.valid_until decision_valid_until,t.verifier_config_id,c.provider_id,c.audience_hash,c.trust_material_ref_hash,c.assurance_level_cap,c.config_status,"
        ."p.issuer_hash,p.provider_status,l.user_id linked_user_id,l.link_status,l.deprovisioned_at,u.role_id current_role_id,u.inactive user_inactive,r.inactive role_inactive "
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
        ."WHERE s.id=".$session_id." LIMIT 2";
    $result = db_query($sql, 'could not read HRM-FND-004 Person bank-account verification-request step-up session custody');
    if (!$result)
        return false;
    $row = db_fetch_assoc($result);
    if (!$row || db_fetch_assoc($result) !== false
        || !federation_assurance_consumer_current_custody_valid($row, $now_epoch, false)
        || (int)$row['assurance_level'] !== 1 || (string)$row['assurance_method'] !== 'oidc_rs256'
        || (int)$row['assurance_level_cap'] < 2)
        return false;
    return $row;
}

/** Revalidate exact employee/successor/version binding without bank plaintext. */
function federation_oidc_bank_account_verification_request_step_up_action_context($company, $employee_ref, $person_bank_account_id, $row_version, $action_hash)
{
    $company = (int)$company;
    $person_bank_account_id = function_exists('hrm_bank_account_verification_request_positive_int')
        ? hrm_bank_account_verification_request_positive_int($person_bank_account_id) : false;
    $row_version = function_exists('hrm_bank_account_verification_request_positive_int')
        ? hrm_bank_account_verification_request_positive_int($row_version) : false;
    $employee_ref = is_scalar($employee_ref) ? trim((string)$employee_ref) : '';
    if ($company < 0 || $employee_ref === '' || strlen($employee_ref) > 64 || $person_bank_account_id === false
        || $row_version === false || !is_string($action_hash) || preg_match('/^[a-f0-9]{64}$/D', $action_hash) !== 1)
        return false;
    $predecessor = hrm_bank_account_verification_request_browser_successor($employee_ref, $person_bank_account_id, $row_version);
    if (!is_array($predecessor)) return false;
    return array('company'=>$company,'employee_ref'=>$employee_ref,'person_bank_account_id'=>$person_bank_account_id,
        'bank_account_row_version'=>$row_version,
        'action_class'=>HRM_BANK_ACCOUNT_VERIFICATION_REQUEST_ACTION_CLASS,'action_hash'=>$action_hash);
}

/** Parse and validate the private step-up client/policy for one current config. */
function federation_oidc_bank_account_verification_request_step_up_material_context($company, $session_context, $required_path)
{
    $company = (int)$company;
    if ($company < 0 || !is_array($session_context) || !isset($session_context['trust_material_ref_hash'], $session_context['issuer_hash'],
        $session_context['audience_hash'], $session_context['verifier_config_id']) || !is_string($required_path)
        || strlen($required_path) < 1 || strlen($required_path) > 1024 || $required_path[0] !== '/'
        || strpos($required_path, '?') !== false || strpos($required_path, '#') !== false
        || preg_match('/[\x00-\x20\x7f]/', $required_path))
        return false;
    if (!function_exists('federation_control_valid_sha256')
        || !federation_control_valid_sha256((string)$session_context['trust_material_ref_hash'])
        || !federation_control_valid_sha256((string)$session_context['issuer_hash'])
        || !federation_control_valid_sha256((string)$session_context['audience_hash']))
        return false;
    $bundle = federation_control_load_bundle($company, (string)$session_context['trust_material_ref_hash'], 'oidc', 'oidc_code_pkce_v1');
    if (!is_array($bundle) || !isset($bundle['materials']) || !is_array($bundle['materials'])
        || !isset($bundle['materials']['oidc_client_config'], $bundle['materials']['oidc_assurance_policy']))
        return false;
    $materials = $bundle['materials'];
    $client = federation_oidc_client_config($materials['oidc_client_config']);
    $policy = federation_oidc_assurance_policy($materials['oidc_assurance_policy']);
    if ($client === false || $policy === false
        || !federation_control_hash_equals(hash('sha256', $client['issuer']), (string)$session_context['issuer_hash'])
        || !federation_control_hash_equals(hash('sha256', $client['client_id']), (string)$session_context['audience_hash']))
        return false;
    $matches = array();
    foreach ($client['redirect_uris'] as $uri) {
        $parts = @parse_url($uri);
        $path = is_array($parts) && isset($parts['path']) ? (string)$parts['path'] : '/';
        if ($path === $required_path && !isset($parts['query']) && !isset($parts['fragment']))
            $matches[] = $uri;
    }
    if (count($matches) !== 1)
        return false;
    return array('redirect_uri'=>$matches[0], 'client'=>$client, 'policy'=>$policy, 'materials'=>$materials);
}

/** Add only server-derived reauthentication hints to the accepted authorization URL. */
function federation_oidc_bank_account_verification_request_step_up_authorization_url($authorization_url, $policy)
{
    if (!is_string($authorization_url) || !is_array($policy) || !isset($policy['required_acr'], $policy['max_auth_age_seconds'])
        || federation_oidc_assurance_policy(json_encode($policy)) === false)
        return false;
    $parts = @parse_url($authorization_url);
    if (!is_array($parts) || !isset($parts['scheme'], $parts['host'], $parts['query'])
        || strtolower((string)$parts['scheme']) !== 'https' || isset($parts['fragment']))
        return false;
    $query = array();
    parse_str((string)$parts['query'], $query);
    foreach (array('prompt','max_age','acr_values') as $key)
        if (array_key_exists($key, $query))
            return false;
    $query['prompt'] = 'login';
    $query['max_age'] = (string)(int)$policy['max_auth_age_seconds'];
    $query['acr_values'] = (string)$policy['required_acr'];
    $authority = (isset($parts['user']) ? rawurlencode($parts['user']).(isset($parts['pass']) ? ':'.rawurlencode($parts['pass']) : '').'@' : '')
        .$parts['host'].(isset($parts['port']) ? ':'.(int)$parts['port'] : '');
    $path = isset($parts['path']) ? $parts['path'] : '';
    return $parts['scheme'].'://'.$authority.$path.'?'.http_build_query($query, '', '&', PHP_QUERY_RFC3986);
}

/** Generate one session-confined CSRF token bound to the exact non-secret action context. */
function federation_oidc_bank_account_verification_request_step_up_issue_csrf($action, $now_epoch = null)
{
    $now = $now_epoch === null ? time() : $now_epoch;
    $session_id = federation_oidc_bank_account_verification_request_step_up_current_session_id();
    if (!is_array($action) || !isset($action['company'],$action['employee_ref'],$action['person_bank_account_id'],
        $action['bank_account_row_version'],$action['action_hash']) || !is_int($now) || $now <= 0 || $session_id <= 0
        || !federation_oidc_bank_account_verification_request_step_up_authorized()) return false;
    $bytes = function_exists('federation_oidc_random_bytes') ? federation_oidc_random_bytes(32) : false;
    if ($bytes === false && function_exists('random_bytes')) {
        try { $bytes = random_bytes(32); } catch (Exception $e) { $bytes = false; }
    }
    if (!is_string($bytes) || strlen($bytes) !== 32) return false;
    $token = function_exists('federation_oidc_base64url_encode') ? federation_oidc_base64url_encode($bytes)
        : rtrim(strtr(base64_encode($bytes), '+/', '-_'), '=');
    if (!is_string($token) || preg_match('/^[A-Za-z0-9_-]{43}$/D', $token) !== 1) return false;
    $_SESSION['federation_bank_account_verification_request_step_up_csrf'] = array(
        'token_hash'=>hash('sha256',$token),'company'=>(int)$action['company'],'session_id'=>$session_id,
        'employee_ref'=>(string)$action['employee_ref'],'person_bank_account_id'=>(int)$action['person_bank_account_id'],
        'bank_account_row_version'=>(int)$action['bank_account_row_version'],'action_hash'=>(string)$action['action_hash'],
        'expires_at_epoch'=>$now + FEDERATION_OIDC_BANK_ACCOUNT_VERIFICATION_REQUEST_STEP_UP_CSRF_TTL);
    return $token;
}

/** Consume the exact one-time action-bound CSRF token before OAuth state is issued. */
function federation_oidc_bank_account_verification_request_step_up_consume_csrf($action, $token, $now_epoch)
{
    $row = isset($_SESSION['federation_bank_account_verification_request_step_up_csrf'])
        ? $_SESSION['federation_bank_account_verification_request_step_up_csrf'] : false;
    unset($_SESSION['federation_bank_account_verification_request_step_up_csrf']);
    if (!is_array($action) || !is_array($row) || !is_string($token)
        || preg_match('/^[A-Za-z0-9_-]{43}$/D',$token)!==1 || !is_int($now_epoch) || $now_epoch <= 0) return false;
    return (int)$row['company']===(int)$action['company']
        && (int)$row['session_id']===federation_oidc_bank_account_verification_request_step_up_current_session_id()
        && (string)$row['employee_ref']===(string)$action['employee_ref']
        && (int)$row['person_bank_account_id']===(int)$action['person_bank_account_id']
        && (int)$row['bank_account_row_version']===(int)$action['bank_account_row_version']
        && (string)$row['action_hash']===(string)$action['action_hash']
        && (int)$row['expires_at_epoch']>$now_epoch
        && hash_equals((string)$row['token_hash'],hash('sha256',$token));
}

/** Drop expired/malformed pending browser correlation; all OAuth secrets remain elsewhere. */
function federation_oidc_bank_account_verification_request_step_up_prune_pending($now_epoch)
{
    if (!isset($_SESSION['federation_bank_account_verification_request_step_up_pending']) || !is_array($_SESSION['federation_bank_account_verification_request_step_up_pending'])) {
        $_SESSION['federation_bank_account_verification_request_step_up_pending'] = array();
        return;
    }
    foreach ($_SESSION['federation_bank_account_verification_request_step_up_pending'] as $state_hash=>$row) {
        if (!is_string($state_hash) || preg_match('/^[a-f0-9]{64}$/D', $state_hash) !== 1 || !is_array($row)
            || !isset($row['expires_at_epoch']) || (int)$row['expires_at_epoch'] <= (int)$now_epoch)
            unset($_SESSION['federation_bank_account_verification_request_step_up_pending'][$state_hash]);
    }
}

/** Remember only non-secret action/session correlation keyed by state hash. */
function federation_oidc_bank_account_verification_request_step_up_bind_pending($state, $session_context, $action_context, $transaction, $redirect_uri, $now_epoch)
{
    if (!is_string($state) || preg_match('/^[A-Za-z0-9_-]{43}$/D',$state)!==1 || !is_array($session_context)
        || !is_array($action_context) || !is_array($transaction) || !isset($transaction['verifier_transaction_id'],$transaction['expires_at_epoch'])
        || !is_string($redirect_uri) || !is_int($now_epoch) || $now_epoch<=0) return false;
    federation_oidc_bank_account_verification_request_step_up_prune_pending($now_epoch);
    $pending =& $_SESSION['federation_bank_account_verification_request_step_up_pending'];
    foreach ($pending as $row)
        if (is_array($row) && (int)$row['session_id']===(int)$session_context['session_id']
            && (string)$row['action_hash']===(string)$action_context['action_hash']) return false;
    if (count($pending)>=FEDERATION_OIDC_BANK_ACCOUNT_VERIFICATION_REQUEST_STEP_UP_MAX_PENDING) return false;
    $pending[hash('sha256',$state)] = array(
        'company'=>(int)$action_context['company'],'session_id'=>(int)$session_context['session_id'],
        'verifier_config_id'=>(int)$session_context['verifier_config_id'],
        'verifier_transaction_id'=>(int)$transaction['verifier_transaction_id'],
        'employee_ref'=>(string)$action_context['employee_ref'],'person_bank_account_id'=>(int)$action_context['person_bank_account_id'],
        'bank_account_row_version'=>(int)$action_context['bank_account_row_version'],
        'action_class'=>(string)$action_context['action_class'],'action_hash'=>(string)$action_context['action_hash'],
        'redirect_uri_hash'=>hash('sha256',$redirect_uri),'created_at_epoch'=>$now_epoch,
        'expires_at_epoch'=>(int)$transaction['expires_at_epoch']);
    return true;
}

/** Fetch exact pending correlation while requiring the same current session and HRM action class. */
function federation_oidc_bank_account_verification_request_step_up_pending($company, $state, $now_epoch)
{
    $company=(int)$company;
    if ($company<0 || !is_string($state) || preg_match('/^[A-Za-z0-9_-]{43}$/D',$state)!==1
        || !is_int($now_epoch) || $now_epoch<=0) return false;
    federation_oidc_bank_account_verification_request_step_up_prune_pending($now_epoch);
    $hash=hash('sha256',$state);
    if (!isset($_SESSION['federation_bank_account_verification_request_step_up_pending'][$hash])
        || !is_array($_SESSION['federation_bank_account_verification_request_step_up_pending'][$hash])) return false;
    $row=$_SESSION['federation_bank_account_verification_request_step_up_pending'][$hash];
    if ((int)$row['company']!==$company
        || (int)$row['session_id']!==federation_oidc_bank_account_verification_request_step_up_current_session_id()
        || (string)$row['action_class']!==HRM_BANK_ACCOUNT_VERIFICATION_REQUEST_ACTION_CLASS) return false;
    return $row;
}

/** Remove one pending state binding after a terminal callback outcome. */
function federation_oidc_bank_account_verification_request_step_up_forget_pending($state)
{
    if (!is_string($state))
        return;
    $hash = hash('sha256', $state);
    if (isset($_SESSION['federation_bank_account_verification_request_step_up_pending'][$hash]))
        unset($_SESSION['federation_bank_account_verification_request_step_up_pending'][$hash]);
}

/** Parse only nonce structure before the pinned verifier authenticates the JWT. */
function federation_oidc_bank_account_verification_request_step_up_nonce($jwt, $expected_nonce_hash)
{
    if (!is_string($expected_nonce_hash) || preg_match('/^[a-f0-9]{64}$/D', $expected_nonce_hash) !== 1
        || !function_exists('federation_oidc_token_verifier_parse'))
        return false;
    $parsed = federation_oidc_token_verifier_parse($jwt);
    if (!is_array($parsed) || !isset($parsed['claims']['nonce']) || !is_string($parsed['claims']['nonce'])
        || preg_match('/^[A-Za-z0-9_-]{32,128}$/D', $parsed['claims']['nonce']) !== 1)
        return false;
    $actual = hash('sha256', $parsed['claims']['nonce']);
    return federation_control_hash_equals($expected_nonce_hash, $actual) ? $parsed['claims']['nonce'] : false;
}

/** Start one authenticated action-bound OIDC reauthentication for an exact successor/hash tuple. */
function federation_oidc_start_bank_account_verification_request_step_up($company, $employee_ref, $person_bank_account_id, $row_version,
    $action_hash, $csrf_token, $required_callback_path,
    $ttl_seconds = FEDERATION_OIDC_BANK_ACCOUNT_VERIFICATION_REQUEST_STEP_UP_TRANSACTION_TTL, $now_epoch = null)
{
    global $path_to_root;
    $company=(int)$company; $ttl_seconds=(int)$ttl_seconds; $now=$now_epoch===null?time():$now_epoch;
    if ($company<0 || !is_int($now) || $now<=0 || !federation_oidc_bank_account_verification_request_step_up_authorized())
        return federation_oidc_bank_account_verification_request_step_up_result('denied','unauthorized');
    if (!federation_oidc_bank_account_verification_request_step_up_dependencies())
        return federation_oidc_bank_account_verification_request_step_up_result('retry_later','runtime_dependency_unavailable');
    if (function_exists('user_company') && (int)user_company()!==$company)
        return federation_oidc_bank_account_verification_request_step_up_result('denied','company_mismatch');
    $action=federation_oidc_bank_account_verification_request_step_up_action_context($company,$employee_ref,$person_bank_account_id,$row_version,$action_hash);
    if (!is_array($action)) return federation_oidc_bank_account_verification_request_step_up_result('denied','bank_account_successor_stale');
    if (!federation_oidc_bank_account_verification_request_step_up_consume_csrf($action,$csrf_token,$now))
        return federation_oidc_bank_account_verification_request_step_up_result('denied','csrf_rejected');
    $root=isset($path_to_root)?realpath($path_to_root):false;
    $dependency=$root===false?false:federation_verifier_dependency_configured_status($root);
    if (!is_array($dependency)||empty($dependency['available']))
        return federation_oidc_bank_account_verification_request_step_up_result('retry_later','verifier_dependency_unavailable');
    $session_id=federation_oidc_bank_account_verification_request_step_up_current_session_id();
    $session_context=federation_oidc_bank_account_verification_request_step_up_session_context($session_id,$now);
    if (!is_array($session_context)) return federation_oidc_bank_account_verification_request_step_up_result('denied','federated_session_ineligible');
    $material=federation_oidc_bank_account_verification_request_step_up_material_context($company,$session_context,$required_callback_path);
    if (!is_array($material)) return federation_oidc_bank_account_verification_request_step_up_result('retry_later','step_up_material_unavailable');
    federation_oidc_bank_account_verification_request_step_up_prune_pending($now);
    foreach ($_SESSION['federation_bank_account_verification_request_step_up_pending'] as $row)
        if (is_array($row)&&(int)$row['session_id']===$session_id&&(string)$row['action_hash']===(string)$action['action_hash'])
            return federation_oidc_bank_account_verification_request_step_up_result('denied','step_up_already_pending');
    $transaction=federation_oidc_issue_transaction($company,(int)$session_context['verifier_config_id'],(string)$material['redirect_uri'],$ttl_seconds);
    if (!is_array($transaction)||!isset($transaction['state'],$transaction['authorization_url'],$transaction['expires_at_epoch'],$transaction['verifier_transaction_id']))
        return federation_oidc_bank_account_verification_request_step_up_result('retry_later','transaction_issue_failed');
    $authorization_url=federation_oidc_bank_account_verification_request_step_up_authorization_url((string)$transaction['authorization_url'],$material['policy']);
    if ($authorization_url===false || !federation_oidc_bank_account_verification_request_step_up_bind_pending((string)$transaction['state'],$session_context,$action,$transaction,(string)$material['redirect_uri'],$now))
        return federation_oidc_bank_account_verification_request_step_up_result('denied','browser_correlation_failed');
    return federation_oidc_bank_account_verification_request_step_up_result('authorization_required','reauthentication_required',array(
        'authorization_url'=>$authorization_url,'expires_at_epoch'=>(int)$transaction['expires_at_epoch'],
        'verifier_transaction_id'=>(int)$transaction['verifier_transaction_id'],'employee_ref'=>(string)$action['employee_ref'],
        'person_bank_account_id'=>(int)$action['person_bank_account_id'],'bank_account_row_version'=>(int)$action['bank_account_row_version'],
        'action_class'=>(string)$action['action_class'],'action_hash'=>(string)$action['action_hash']));
}

/**
 * Complete one authenticated bank-account-verification-request step-up callback.
 *
 * The accepted base decision consumes/replay-protects the OIDC transaction.
 * The exact bank-account successor is then rebuilt before the existing assurance
 * issuer elevates the original durable session. No bank-account action is consumed.
 */
function federation_oidc_complete_bank_account_verification_request_step_up($company, $state, $authorization_code, $required_callback_path,
    $lease_seconds = FEDERATION_OIDC_BANK_ACCOUNT_VERIFICATION_REQUEST_STEP_UP_DEFAULT_LEASE, $now_epoch = null, $transport = null, $signature_verifier = null)
{
    global $path_to_root;
    $company = (int)$company; $lease_seconds = (int)$lease_seconds;
    $now = $now_epoch === null ? time() : $now_epoch;
    if ($company < 0 || !is_int($now) || $now <= 0 || !federation_oidc_bank_account_verification_request_step_up_authorized())
        return federation_oidc_bank_account_verification_request_step_up_result('denied', 'unauthorized');
    if (!federation_oidc_bank_account_verification_request_step_up_dependencies())
        return federation_oidc_bank_account_verification_request_step_up_result('retry_later', 'runtime_dependency_unavailable');
    if (!is_string($state) || preg_match('/^[A-Za-z0-9_-]{43}$/D', $state) !== 1
        || !is_string($authorization_code) || strlen($authorization_code) < 1
        || strlen($authorization_code) > FEDERATION_OIDC_EXCHANGE_MAX_CODE_BYTES
        || preg_match('/[\x00-\x20\x7f]/', $authorization_code) === 1
        || $lease_seconds < FEDERATION_OIDC_CALLBACK_CLAIM_MIN_LEASE || $lease_seconds > FEDERATION_OIDC_CALLBACK_CLAIM_MAX_LEASE)
        return federation_oidc_bank_account_verification_request_step_up_result('denied', 'callback_invalid');
    if (function_exists('user_company') && (int)user_company() !== $company)
        return federation_oidc_bank_account_verification_request_step_up_result('denied', 'company_mismatch');
    $pending = federation_oidc_bank_account_verification_request_step_up_pending($company, $state, $now);
    if (!is_array($pending))
        return federation_oidc_bank_account_verification_request_step_up_result('denied', 'browser_correlation_missing');

    $root = isset($path_to_root) ? realpath($path_to_root) : false;
    $dependency = $root === false ? false : federation_verifier_dependency_configured_status($root);
    if (!is_array($dependency) || empty($dependency['available']))
        return federation_oidc_bank_account_verification_request_step_up_result('retry_later', 'verifier_dependency_unavailable');
    $session_context = federation_oidc_bank_account_verification_request_step_up_session_context((int)$pending['session_id'], $now);
    if (!is_array($session_context) || (int)$session_context['verifier_config_id'] !== (int)$pending['verifier_config_id']) {
        federation_oidc_bank_account_verification_request_step_up_forget_pending($state);
        return federation_oidc_bank_account_verification_request_step_up_result('denied', 'federated_session_changed');
    }
    $material = federation_oidc_bank_account_verification_request_step_up_material_context($company, $session_context, $required_callback_path);
    if (!is_array($material) || !hash_equals((string)$pending['redirect_uri_hash'], hash('sha256', (string)$material['redirect_uri'])))
        return federation_oidc_bank_account_verification_request_step_up_result('retry_later', 'step_up_material_unavailable');

    $claim = federation_oidc_acquire_callback_claim($company, $state, $lease_seconds);
    if (!is_array($claim)) {
        federation_oidc_bank_account_verification_request_step_up_forget_pending($state);
        return federation_oidc_bank_account_verification_request_step_up_result('denied', 'callback_claim_unavailable');
    }
    $claim_token = (string)$claim['claim_token'];
    $retry_after = (int)$claim['claim_expires_at_epoch'];
    if ((int)$claim['verifier_transaction_id'] !== (int)$pending['verifier_transaction_id']
        || (int)$claim['verifier_config_id'] !== (int)$pending['verifier_config_id']) {
        federation_oidc_terminalize_callback_claim($company, $claim_token, 'callback_invalid');
        federation_oidc_bank_account_verification_request_step_up_forget_pending($state);
        return federation_oidc_bank_account_verification_request_step_up_result('denied', 'callback_context_invalid');
    }
    $proof = federation_oidc_load_proof($company, (string)$claim['proof_ref_hash'], $now);
    if (!is_array($proof) || (int)$proof['verifier_config_id'] !== (int)$claim['verifier_config_id']
        || (string)$proof['transaction_key_hash'] !== hash('sha256', $state)) {
        federation_oidc_terminalize_callback_claim($company, $claim_token, 'proof_unavailable');
        federation_oidc_bank_account_verification_request_step_up_forget_pending($state);
        return federation_oidc_bank_account_verification_request_step_up_result('denied', 'proof_unavailable');
    }
    $materials = $material['materials'];
    $contract = federation_oidc_token_verifier_material_contract($materials);
    if ($contract === false || !federation_control_hash_equals(hash('sha256', $contract['issuer']), (string)$session_context['issuer_hash'])
        || !federation_control_hash_equals(hash('sha256', $contract['client_id']), (string)$session_context['audience_hash']))
        return federation_oidc_bank_account_verification_request_step_up_result('retry_later', 'verifier_material_unavailable', array('retry_after_epoch'=>$retry_after));

    $exchange = federation_oidc_exchange_authorization_code($authorization_code, (string)$material['redirect_uri'],
        (string)$proof['code_verifier'], $materials, $transport);
    if (!is_array($exchange) || !isset($exchange['classification'])) {
        federation_oidc_terminalize_callback_claim($company, $claim_token, 'exchange_uncertain');
        federation_oidc_bank_account_verification_request_step_up_forget_pending($state);
        return federation_oidc_bank_account_verification_request_step_up_result('denied', 'exchange_uncertain');
    }
    if ((string)$exchange['classification'] === 'pre_send_failure')
        return federation_oidc_bank_account_verification_request_step_up_result('retry_later', 'exchange_pre_send_failure', array('retry_after_epoch'=>$retry_after));
    if ((string)$exchange['classification'] !== 'exchange_succeeded' || !isset($exchange['id_token'])) {
        $reason = (string)$exchange['classification'] === 'exchange_rejected' ? 'exchange_rejected' : 'exchange_uncertain';
        federation_oidc_terminalize_callback_claim($company, $claim_token, $reason);
        federation_oidc_bank_account_verification_request_step_up_forget_pending($state);
        return federation_oidc_bank_account_verification_request_step_up_result('denied', $reason);
    }

    $nonce = federation_oidc_bank_account_verification_request_step_up_nonce((string)$exchange['id_token'], (string)$claim['nonce_hash']);
    if ($nonce === false) {
        federation_oidc_terminalize_callback_claim($company, $claim_token, 'verifier_rejected');
        federation_oidc_bank_account_verification_request_step_up_forget_pending($state);
        return federation_oidc_bank_account_verification_request_step_up_result('denied', 'nonce_rejected');
    }
    $expectations = array('company_id'=>$company, 'provider_id'=>(int)$session_context['provider_id'],
        'issuer'=>(string)$contract['issuer'], 'client_id'=>(string)$contract['client_id'], 'nonce'=>$nonce);
    $base_candidate = federation_oidc_token_verifier_verify((string)$exchange['id_token'], $expectations, $materials, $now, $signature_verifier);
    $assurance_candidate = federation_oidc_assurance_candidate_verify((string)$exchange['id_token'], $expectations, $materials, $now, $signature_verifier);
    if (!is_array($base_candidate) || empty($base_candidate['verified'])
        || !is_array($assurance_candidate) || empty($assurance_candidate['verified'])) {
        federation_oidc_terminalize_callback_claim($company, $claim_token, 'verifier_rejected');
        federation_oidc_bank_account_verification_request_step_up_forget_pending($state);
        return federation_oidc_bank_account_verification_request_step_up_result('denied', 'assurance_candidate_rejected');
    }

    $decision = federation_oidc_commit_verified_decision($company, $claim_token, $authorization_code, $base_candidate);
    if (!is_array($decision))
        $decision = federation_oidc_commit_verified_decision($company, $claim_token, $authorization_code, $base_candidate);
    if (!is_array($decision) || !isset($decision['verifier_decision_id'])) {
        federation_oidc_bank_account_verification_request_step_up_forget_pending($state);
        return federation_oidc_bank_account_verification_request_step_up_result('denied', 'decision_commit_failed');
    }

    $action = federation_oidc_bank_account_verification_request_step_up_action_context($company, (string)$pending['employee_ref'], (int)$pending['person_bank_account_id'], (int)$pending['bank_account_row_version'], (string)$pending['action_hash']);
    if (!is_array($action) || (int)$action['person_bank_account_id'] !== (int)$pending['person_bank_account_id']
        || (int)$action['bank_account_row_version'] !== (int)$pending['bank_account_row_version']
        || !hash_equals((string)$action['action_hash'], (string)$pending['action_hash'])) {
        federation_oidc_bank_account_verification_request_step_up_forget_pending($state);
        return federation_oidc_bank_account_verification_request_step_up_result('denied', 'bank_account_successor_stale');
    }

    $issued = federation_issue_privileged_assurance($company, (int)$pending['session_id'], $assurance_candidate,
        (string)$pending['action_class'], (string)$pending['action_hash'], $now);
    if (!is_array($issued))
        $issued = federation_issue_privileged_assurance($company, (int)$pending['session_id'], $assurance_candidate,
            (string)$pending['action_class'], (string)$pending['action_hash'], $now);
    federation_oidc_bank_account_verification_request_step_up_forget_pending($state);
    if (!is_array($issued) || !isset($issued['assurance_event_id']))
        return federation_oidc_bank_account_verification_request_step_up_result('denied', 'assurance_issue_failed');

    return federation_oidc_bank_account_verification_request_step_up_result('assured', 'ok', array(
        'assurance_event_id'=>(int)$issued['assurance_event_id'],
        'session_id'=>(int)$pending['session_id'],
        'employee_ref'=>(string)$pending['employee_ref'],
        'person_bank_account_id'=>(int)$pending['person_bank_account_id'],
        'bank_account_row_version'=>(int)$pending['bank_account_row_version'],
        'action_class'=>(string)$pending['action_class'],
        'action_hash'=>(string)$pending['action_hash']
    ));
}
