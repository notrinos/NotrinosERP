<?php
/**
 * PAY-SEC-004 OIDC authorization transaction issuance foundation.
 *
 * Internal helper only. It binds an enabled OIDC provider/configuration to the
 * accepted private control bundle and isolated verifier dependency deployment,
 * constructs an Authorization Code + PKCE S256 request, stores only hashes in
 * MariaDB/audit, and keeps the short-lived raw PKCE verifier in a separate
 * private proof store outside the application tree. It does not expose a
 * callback route, parse tokens, consume transactions, link users or issue a
 * session/assurance level.
 */

if (!defined('FEDERATION_OIDC_CLIENT_CONFIG_SCHEMA'))
    define('FEDERATION_OIDC_CLIENT_CONFIG_SCHEMA', 'notrinoserp.oidc-client-config.v1');
if (!defined('FEDERATION_OIDC_PROOF_SCHEMA'))
    define('FEDERATION_OIDC_PROOF_SCHEMA', 'notrinoserp.oidc-pkce-proof.v1');
if (!defined('FEDERATION_OIDC_TRANSACTION_DEFAULT_TTL'))
    define('FEDERATION_OIDC_TRANSACTION_DEFAULT_TTL', 300);
if (!defined('FEDERATION_OIDC_TRANSACTION_MAX_TTL'))
    define('FEDERATION_OIDC_TRANSACTION_MAX_TTL', 600);
if (!defined('FEDERATION_OIDC_MAX_CLIENT_CONFIG_BYTES'))
    define('FEDERATION_OIDC_MAX_CLIENT_CONFIG_BYTES', 32768);

function federation_oidc_base64url_encode($bytes)
{
    return rtrim(strtr(base64_encode($bytes), '+/', '-_'), '=');
}

function federation_oidc_random_bytes($length)
{
    $length = (int)$length;
    if ($length < 16 || $length > 128)
        return false;
    if (function_exists('random_bytes')) {
        try { return random_bytes($length); } catch (Exception $e) { return false; }
    }
    if (!function_exists('openssl_random_pseudo_bytes'))
        return false;
    $strong = false;
    $bytes = openssl_random_pseudo_bytes($length, $strong);
    return $strong && is_string($bytes) && strlen($bytes) === $length ? $bytes : false;
}

function federation_oidc_valid_https_uri($uri, $allow_query)
{
    if (!is_string($uri) || strlen($uri) < 9 || strlen($uri) > 2048 || preg_match('/[\x00-\x20\x7f]/', $uri))
        return false;
    $parts = @parse_url($uri);
    if (!is_array($parts) || !isset($parts['scheme']) || strtolower($parts['scheme']) !== 'https'
        || !isset($parts['host']) || $parts['host'] === '' || isset($parts['user']) || isset($parts['pass'])
        || isset($parts['fragment']) || (!$allow_query && isset($parts['query'])))
        return false;
    if (isset($parts['port']) && ((int)$parts['port'] < 1 || (int)$parts['port'] > 65535))
        return false;
    return true;
}

function federation_oidc_client_config($bytes)
{
    if (!is_string($bytes) || strlen($bytes) < 2 || strlen($bytes) > FEDERATION_OIDC_MAX_CLIENT_CONFIG_BYTES)
        return false;
    $config = json_decode($bytes, true);
    if (!is_array($config))
        return false;
    $expected = array('schema_version','issuer','authorization_endpoint','client_id','redirect_uris','scopes');
    $keys = array_keys($config);
    sort($keys); $sorted_expected = $expected; sort($sorted_expected);
    if ($keys !== $sorted_expected || $config['schema_version'] !== FEDERATION_OIDC_CLIENT_CONFIG_SCHEMA
        || !federation_oidc_valid_https_uri($config['issuer'], false)
        || !federation_oidc_valid_https_uri($config['authorization_endpoint'], false)
        || !is_string($config['client_id']) || strlen($config['client_id']) < 1 || strlen($config['client_id']) > 256
        || preg_match('/[\x00-\x1f\x7f]/', $config['client_id'])
        || !is_array($config['redirect_uris']) || count($config['redirect_uris']) < 1 || count($config['redirect_uris']) > 8
        || !is_array($config['scopes']) || count($config['scopes']) < 1 || count($config['scopes']) > 16)
        return false;
    $redirect_seen = array();
    foreach ($config['redirect_uris'] as $redirect) {
        if (!federation_oidc_valid_https_uri($redirect, true) || isset($redirect_seen[$redirect]))
            return false;
        $redirect_seen[$redirect] = true;
    }
    $scope_seen = array();
    foreach ($config['scopes'] as $scope) {
        if (!is_string($scope) || strlen($scope) < 1 || strlen($scope) > 64
            || preg_match('/^[A-Za-z0-9][A-Za-z0-9._:-]*$/D', $scope) !== 1 || isset($scope_seen[$scope]))
            return false;
        $scope_seen[$scope] = true;
    }
    if (!isset($scope_seen['openid']))
        return false;
    return $config;
}

function federation_oidc_build_authorization_url($client_config, $redirect_uri, $state, $nonce, $code_challenge)
{
    if (!is_array($client_config) || !isset($client_config['redirect_uris'])
        || !in_array($redirect_uri, $client_config['redirect_uris'], true)
        || !is_string($state) || !is_string($nonce) || !is_string($code_challenge)
        || preg_match('/^[A-Za-z0-9_-]{32,128}$/D', $state) !== 1
        || preg_match('/^[A-Za-z0-9_-]{32,128}$/D', $nonce) !== 1
        || preg_match('/^[A-Za-z0-9_-]{43}$/D', $code_challenge) !== 1)
        return false;
    $query = http_build_query(array(
        'response_type'=>'code',
        'client_id'=>$client_config['client_id'],
        'redirect_uri'=>$redirect_uri,
        'scope'=>implode(' ', $client_config['scopes']),
        'state'=>$state,
        'nonce'=>$nonce,
        'code_challenge'=>$code_challenge,
        'code_challenge_method'=>'S256'
    ), '', '&', PHP_QUERY_RFC3986);
    return $client_config['authorization_endpoint'].'?'.$query;
}

function federation_oidc_application_root()
{
    global $path_to_root;
    return isset($path_to_root) ? realpath($path_to_root) : false;
}

function federation_oidc_proof_private_root($company)
{
    global $db_connections;
    $company = (int)$company;
    if ($company < 0)
        return false;
    $application_root = federation_oidc_application_root();
    if ($application_root === false)
        return false;
    if (defined('FEDERATION_OIDC_PROOF_PRIVATE_ROOT'))
        $base = rtrim((string)FEDERATION_OIDC_PROOF_PRIVATE_ROOT, "\\/");
    elseif (isset($db_connections[$company]['federation_oidc_proof_private_root']))
        $base = rtrim((string)$db_connections[$company]['federation_oidc_proof_private_root'], "\\/");
    else {
        $web_parent = dirname($application_root);
        $base = dirname($web_parent).DIRECTORY_SEPARATOR.'.notrinoserp-private'
            .DIRECTORY_SEPARATOR.substr(hash('sha256', $application_root), 0, 16)
            .DIRECTORY_SEPARATOR.'federation-oidc-proof';
    }
    if ($base === '' || preg_match('/^(?:[A-Za-z]:[\\\\\/]|\/)/', $base) !== 1)
        return false;
    $normalized_base = rtrim(str_replace('\\', '/', $base), '/');
    $normalized_app = rtrim(str_replace('\\', '/', $application_root), '/');
    $compare_base = DIRECTORY_SEPARATOR === '\\' ? strtolower($normalized_base) : $normalized_base;
    $compare_app = DIRECTORY_SEPARATOR === '\\' ? strtolower($normalized_app) : $normalized_app;
    if ($compare_base === $compare_app || strpos($compare_base.'/', $compare_app.'/') === 0)
        return false;
    return $base.DIRECTORY_SEPARATOR.'company-'.$company;
}

function federation_oidc_private_permissions($path, $directory)
{
    if (DIRECTORY_SEPARATOR !== '/')
        return true;
    $permissions = @fileperms($path);
    if ($permissions === false || ($permissions & 0077) !== 0)
        return false;
    return $directory ? (($permissions & 0500) === 0500) : (($permissions & 0400) === 0400);
}

function federation_oidc_ensure_proof_root($company)
{
    $root = federation_oidc_proof_private_root($company);
    if ($root === false)
        return false;
    if (!is_dir($root)) {
        $old = @umask(0077);
        $created = @mkdir($root, 0700, true);
        @umask($old);
        if (!$created && !is_dir($root))
            return false;
    }
    if (is_link($root) || !is_dir($root) || !federation_oidc_private_permissions($root, true))
        return false;
    $resolved = realpath($root); $app = federation_oidc_application_root();
    if ($resolved === false || $app === false)
        return false;
    $resolved_n = rtrim(str_replace('\\', '/', $resolved), '/');
    $app_n = rtrim(str_replace('\\', '/', $app), '/');
    if (DIRECTORY_SEPARATOR === '\\') { $resolved_n = strtolower($resolved_n); $app_n = strtolower($app_n); }
    return $resolved_n !== $app_n && strpos($resolved_n.'/', $app_n.'/') !== 0 ? $root : false;
}

function federation_oidc_proof_path($company, $proof_ref_hash)
{
    if (!is_string($proof_ref_hash) || preg_match('/^[a-f0-9]{64}$/D', $proof_ref_hash) !== 1)
        return false;
    $root = federation_oidc_proof_private_root($company);
    return $root === false ? false : $root.DIRECTORY_SEPARATOR.'oidc-pkce-'.$proof_ref_hash.'.json';
}

function federation_oidc_store_proof($company, $verifier_config_id, $transaction_key_hash, $proof_ref_hash, $code_verifier, $created_epoch, $expires_epoch)
{
    $root = federation_oidc_ensure_proof_root($company);
    $path = federation_oidc_proof_path($company, $proof_ref_hash);
    if ($root === false || $path === false || !is_string($code_verifier)
        || preg_match('/^[A-Za-z0-9_-]{43}$/D', $code_verifier) !== 1
        || preg_match('/^[a-f0-9]{64}$/D', (string)$transaction_key_hash) !== 1
        || (int)$verifier_config_id <= 0 || (int)$created_epoch <= 0 || (int)$expires_epoch <= (int)$created_epoch)
        return false;
    if (file_exists($path) || is_link($path))
        return false;
    $payload = json_encode(array(
        'schema_version'=>FEDERATION_OIDC_PROOF_SCHEMA,
        'company_id'=>(int)$company,
        'verifier_config_id'=>(int)$verifier_config_id,
        'transaction_key_hash'=>$transaction_key_hash,
        'proof_ref_hash'=>$proof_ref_hash,
        'code_verifier'=>$code_verifier,
        'created_at_epoch'=>(int)$created_epoch,
        'expires_at_epoch'=>(int)$expires_epoch
    ));
    if (!is_string($payload) || strlen($payload) > 4096)
        return false;
    $suffix_bytes = federation_oidc_random_bytes(16);
    if ($suffix_bytes === false)
        return false;
    $tmp = $root.DIRECTORY_SEPARATOR.'.oidc-pkce-'.$proof_ref_hash.'.'.federation_oidc_base64url_encode($suffix_bytes).'.tmp';
    $handle = @fopen($tmp, 'xb');
    if (!$handle)
        return false;
    @chmod($tmp, 0600);
    if (function_exists('clearstatcache')) @clearstatcache(true, $tmp);
    $written = @fwrite($handle, $payload);
    if (function_exists('fflush')) @fflush($handle);
    @fclose($handle);
    if ($written !== strlen($payload) || !federation_oidc_private_permissions($tmp, false) || !@rename($tmp, $path)) {
        @unlink($tmp);
        return false;
    }
    @chmod($path, 0600);
    if (function_exists('clearstatcache')) @clearstatcache(true, $path);
    return is_file($path) && !is_link($path) && federation_oidc_private_permissions($path, false);
}

function federation_oidc_load_proof($company, $proof_ref_hash, $now_epoch)
{
    $path = federation_oidc_proof_path($company, $proof_ref_hash);
    $root = federation_oidc_proof_private_root($company);
    if ($path === false || $root === false || is_link($path) || !is_file($path)
        || !federation_oidc_private_permissions($path, false))
        return false;
    $resolved = realpath($path); $resolved_root = realpath($root);
    if ($resolved === false || $resolved_root === false)
        return false;
    $rp = str_replace('\\', '/', $resolved); $rr = rtrim(str_replace('\\', '/', $resolved_root), '/');
    if (DIRECTORY_SEPARATOR === '\\') { $rp = strtolower($rp); $rr = strtolower($rr); }
    if (strpos($rp, $rr.'/') !== 0)
        return false;
    $bytes = @file_get_contents($path);
    $proof = is_string($bytes) ? json_decode($bytes, true) : false;
    if (!is_array($proof)
        || array_keys($proof) !== array('schema_version','company_id','verifier_config_id','transaction_key_hash','proof_ref_hash','code_verifier','created_at_epoch','expires_at_epoch')
        || $proof['schema_version'] !== FEDERATION_OIDC_PROOF_SCHEMA
        || (int)$proof['company_id'] !== (int)$company
        || !is_int($proof['verifier_config_id']) || $proof['verifier_config_id'] <= 0
        || preg_match('/^[a-f0-9]{64}$/D', (string)$proof['transaction_key_hash']) !== 1
        || !is_string($proof['proof_ref_hash']) || !function_exists('federation_control_hash_equals')
        || !federation_control_hash_equals($proof['proof_ref_hash'], $proof_ref_hash)
        || preg_match('/^[A-Za-z0-9_-]{43}$/D', (string)$proof['code_verifier']) !== 1
        || !is_int($proof['created_at_epoch']) || !is_int($proof['expires_at_epoch'])
        || $proof['expires_at_epoch'] <= $proof['created_at_epoch'] || (int)$now_epoch >= $proof['expires_at_epoch'])
        return false;
    return $proof;
}

function federation_oidc_delete_proof($company, $proof_ref_hash)
{
    $path = federation_oidc_proof_path($company, $proof_ref_hash);
    if ($path === false || !file_exists($path))
        return true;
    if (is_link($path) || !is_file($path))
        return false;
    return @unlink($path);
}

function federation_oidc_cleanup_expired_proofs($company, $now_epoch, $limit)
{
    $root = federation_oidc_proof_private_root($company);
    $limit = (int)$limit;
    if ($root === false || !is_dir($root) || is_link($root) || $limit < 1 || $limit > 1000)
        return 0;
    $removed = 0; $entries = @scandir($root);
    if (!is_array($entries))
        return 0;
    foreach ($entries as $entry) {
        if ($removed >= $limit) break;
        if (preg_match('/^oidc-pkce-([a-f0-9]{64})\.json$/D', $entry, $m) !== 1) continue;
        $path = $root.DIRECTORY_SEPARATOR.$entry;
        if (is_link($path) || !is_file($path) || !federation_oidc_private_permissions($path, false)) continue;
        $bytes = @file_get_contents($path); $proof = is_string($bytes) ? json_decode($bytes, true) : false;
        if (!is_array($proof) || !isset($proof['schema_version'],$proof['proof_ref_hash'],$proof['expires_at_epoch'])
            || $proof['schema_version'] !== FEDERATION_OIDC_PROOF_SCHEMA
            || !is_string($proof['proof_ref_hash']) || !federation_control_hash_equals($proof['proof_ref_hash'], $m[1])
            || !is_int($proof['expires_at_epoch']) || (int)$now_epoch < $proof['expires_at_epoch']) continue;
        if (@unlink($path)) $removed++;
    }
    return $removed;
}

function federation_oidc_load_enabled_configuration($verifier_config_id)
{
    if ((int)$verifier_config_id <= 0 || !defined('TB_PREF'))
        return false;
    $id = (int)$verifier_config_id;
    $sql = "SELECT c.verifier_config_id,c.provider_id,c.config_version,c.verifier_profile,c.audience_hash,c.trust_material_ref_hash,c.max_clock_skew_seconds,c.assurance_level_cap,c.config_status,p.protocol,p.issuer_hash,p.provider_status,p.provider_row_version "
        ."FROM ".TB_PREF."user_federation_provider_verifier_configs c INNER JOIN ".TB_PREF."user_federation_providers p ON p.provider_id=c.provider_id "
        ."WHERE c.verifier_config_id=$id FOR UPDATE";
    $result = db_query($sql, 'could not lock enabled PAY-SEC-004 OIDC verifier configuration');
    $row = $result ? db_fetch_assoc($result) : false;
    if (!$row || (string)$row['protocol'] !== 'oidc' || (string)$row['verifier_profile'] !== 'oidc_code_pkce_v1'
        || (string)$row['provider_status'] !== 'enabled' || (string)$row['config_status'] !== 'enabled'
        || !federation_control_valid_sha256((string)$row['issuer_hash'])
        || !federation_control_valid_sha256((string)$row['audience_hash'])
        || !federation_control_valid_sha256((string)$row['trust_material_ref_hash']))
        return false;
    return $row;
}

function federation_oidc_issue_transaction($company, $verifier_config_id, $redirect_uri, $ttl_seconds)
{
    global $path_to_root;
    $company = (int)$company; $verifier_config_id = (int)$verifier_config_id; $ttl_seconds = (int)$ttl_seconds;
    if ($company < 0 || $verifier_config_id <= 0 || $ttl_seconds < 60 || $ttl_seconds > FEDERATION_OIDC_TRANSACTION_MAX_TTL
        || !federation_oidc_valid_https_uri($redirect_uri, true))
        return false;
    if (!function_exists('federation_control_load_bundle'))
        require_once(dirname(__FILE__).DIRECTORY_SEPARATOR.'federation_control_material.inc');
    if (!function_exists('federation_verifier_dependency_configured_status'))
        require_once(dirname(__FILE__).DIRECTORY_SEPARATOR.'federation_verifier_dependencies.inc');
    $root = isset($path_to_root) ? realpath($path_to_root) : false;
    if ($root === false)
        return false;
    $dependency = federation_verifier_dependency_configured_status($root);
    if (!is_array($dependency) || empty($dependency['available']))
        return false;
    if (!function_exists('append_domain_audit_event')) {
        $audit = $root.DIRECTORY_SEPARATOR.'includes'.DIRECTORY_SEPARATOR.'db'.DIRECTORY_SEPARATOR.'domain_audit_db.inc';
        if (!is_file($audit)) return false;
        require_once($audit);
    }
    if (!function_exists('append_domain_audit_event'))
        return false;

    /* Bounded recovery of proof files left by pre-commit crashes/timeouts. */
    federation_oidc_cleanup_expired_proofs($company, time(), 50);

    if (!db_query('BEGIN', 'could not start PAY-SEC-004 OIDC issuance transaction'))
        return false;
    $proof_hash = false; $committed = false;
    do {
        $config = federation_oidc_load_enabled_configuration($verifier_config_id);
        if ($config === false) break;
        $bundle = federation_control_load_bundle($company, (string)$config['trust_material_ref_hash'], 'oidc', 'oidc_code_pkce_v1');
        if (!is_array($bundle) || !isset($bundle['materials']['oidc_jwks']) || !isset($bundle['materials']['oidc_client_config'])) break;
        $client = federation_oidc_client_config($bundle['materials']['oidc_client_config']);
        if ($client === false
            || !federation_control_hash_equals(hash('sha256', $client['issuer']), (string)$config['issuer_hash'])
            || !federation_control_hash_equals(hash('sha256', $client['client_id']), (string)$config['audience_hash'])
            || !in_array($redirect_uri, $client['redirect_uris'], true)) break;

        $state_bytes=federation_oidc_random_bytes(32);$nonce_bytes=federation_oidc_random_bytes(32);$verifier_bytes=federation_oidc_random_bytes(32);$proof_ref_bytes=federation_oidc_random_bytes(32);
        if($state_bytes===false||$nonce_bytes===false||$verifier_bytes===false||$proof_ref_bytes===false)break;
        $state=federation_oidc_base64url_encode($state_bytes);$nonce=federation_oidc_base64url_encode($nonce_bytes);$code_verifier=federation_oidc_base64url_encode($verifier_bytes);
        $challenge=federation_oidc_base64url_encode(hash('sha256',$code_verifier,true));
        $transaction_hash=hash('sha256',$state);$nonce_hash=hash('sha256',$nonce);$proof_ref=federation_oidc_base64url_encode($proof_ref_bytes);$proof_hash=hash('sha256',$proof_ref);
        $correlation_hash=hash('sha256','oidc-authorization-code-v1'."\0".$verifier_config_id."\0".$state."\0".$nonce);
        $created=time();$expires=$created+$ttl_seconds;
        if(!federation_oidc_store_proof($company,$verifier_config_id,$transaction_hash,$proof_hash,$code_verifier,$created,$expires)){ $proof_hash=false; break; }

        $sql="INSERT INTO ".TB_PREF."user_federation_verifier_transactions (verifier_config_id,transaction_key_hash,correlation_hash,nonce_hash,proof_ref_hash,created_at,expires_at,consumed_at) VALUES ("
            .$verifier_config_id.",".db_escape($transaction_hash).",".db_escape($correlation_hash).",".db_escape($nonce_hash).",".db_escape($proof_hash).",".db_escape(gmdate('Y-m-d H:i:s',$created)).",".db_escape(gmdate('Y-m-d H:i:s',$expires)).",NULL)";
        if(!db_query($sql,'could not persist PAY-SEC-004 OIDC verifier transaction'))break;
        $transaction_id=(int)db_insert_id();if($transaction_id<=0)break;
        $audit_id=append_domain_audit_event(array(
            'company_id'=>$company,'legal_entity_id'=>0,'entity_type'=>'security_federation_oidc_transaction','entity_key'=>array('verifier_transaction_id'=>$transaction_id,'verifier_config_id'=>$verifier_config_id,'transaction_key_hash'=>$transaction_hash),
            'action'=>'security.federation.oidc_transaction.issued','reason_code'=>'pay_sec_004_oidc_transaction_issuance','before'=>null,
            'after'=>array('authorization_request_issued'=>true),'restricted_diff'=>array(
                'provider_id'=>(int)$config['provider_id'],'provider_row_version'=>(int)$config['provider_row_version'],'config_version'=>(int)$config['config_version'],
                'issuer_hash'=>(string)$config['issuer_hash'],'audience_hash'=>(string)$config['audience_hash'],'redirect_uri_hash'=>hash('sha256',$redirect_uri),
                'transaction_key_hash'=>$transaction_hash,'nonce_hash'=>$nonce_hash,'proof_ref_hash'=>$proof_hash,'pkce_method'=>'S256','ttl_seconds'=>$ttl_seconds,
                'raw_state_stored'=>false,'raw_nonce_stored'=>false,'raw_pkce_verifier_stored_in_database'=>false,'callback_route_added'=>false,'token_verifier_added'=>false,
                'transaction_consumer_added'=>false,'external_subject_link_writer_added'=>false,'session_assurance_issuer_added'=>false,'payment_authority_changed'=>false,'accounting_authority_changed'=>false)));
        if($audit_id===false)break;
        $authorization_url=federation_oidc_build_authorization_url($client,$redirect_uri,$state,$nonce,$challenge);if($authorization_url===false)break;
        if(!db_query('COMMIT','could not commit PAY-SEC-004 OIDC issuance transaction'))break;
        $committed=true;
        $browser_callback_bound=false;
        if (isset($_SESSION['wa_current_user']) && is_object($_SESSION['wa_current_user'])
            && method_exists($_SESSION['wa_current_user'], 'logged_in') && !$_SESSION['wa_current_user']->logged_in()
            && isset($_SESSION['wa_current_user']->company) && (int)$_SESSION['wa_current_user']->company===$company) {
            require_once(dirname(__FILE__).DIRECTORY_SEPARATOR.'federation_oidc_existing_link_callback_route.inc');
            $browser_callback_bound=federation_oidc_callback_route_bind_pending($company,$verifier_config_id,$state,$redirect_uri,$expires);
        }
        return array('verifier_transaction_id'=>$transaction_id,'authorization_url'=>$authorization_url,'state'=>$state,'expires_at_epoch'=>$expires,'browser_callback_bound'=>$browser_callback_bound);
    } while(false);
    if(!$committed) @db_query('ROLLBACK','could not roll back PAY-SEC-004 OIDC issuance transaction');
    if($proof_hash!==false) federation_oidc_delete_proof($company,$proof_hash);
    return false;
}
