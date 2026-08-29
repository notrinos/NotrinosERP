<?php
/**
 * PAY-SEC-004 private OIDC token-verifier material foundation.
 *
 * Pure validation/derivation helpers only. This file performs no network I/O,
 * database access, callback parsing, token exchange, JWT acceptance, replay
 * mutation, identity linking, login, session issuance or assurance elevation.
 */

if (!defined('FEDERATION_OIDC_TOKEN_CLIENT_CONFIG_SCHEMA'))
    define('FEDERATION_OIDC_TOKEN_CLIENT_CONFIG_SCHEMA', 'notrinoserp.oidc-token-client-config.v1');
if (!defined('FEDERATION_OIDC_TOKEN_MAX_CLIENT_CONFIG_BYTES'))
    define('FEDERATION_OIDC_TOKEN_MAX_CLIENT_CONFIG_BYTES', 32768);
if (!defined('FEDERATION_OIDC_TOKEN_MAX_JWKS_BYTES'))
    define('FEDERATION_OIDC_TOKEN_MAX_JWKS_BYTES', 262144);
if (!defined('FEDERATION_OIDC_TOKEN_MAX_RESPONSE_BYTES'))
    define('FEDERATION_OIDC_TOKEN_MAX_RESPONSE_BYTES', 262144);

function federation_oidc_token_valid_https_uri($uri)
{
    if (!is_string($uri) || strlen($uri) < 9 || strlen($uri) > 2048 || preg_match('/[\x00-\x20\x7f]/', $uri))
        return false;
    $parts = @parse_url($uri);
    if (!is_array($parts) || !isset($parts['scheme']) || strtolower($parts['scheme']) !== 'https'
        || !isset($parts['host']) || $parts['host'] === '' || isset($parts['user']) || isset($parts['pass'])
        || isset($parts['query']) || isset($parts['fragment']))
        return false;
    if (isset($parts['port']) && ((int)$parts['port'] < 1 || (int)$parts['port'] > 65535))
        return false;
    return true;
}

function federation_oidc_token_valid_client_id($value)
{
    return is_string($value) && strlen($value) >= 1 && strlen($value) <= 256
        && preg_match('/[\x00-\x1f\x7f]/', $value) !== 1;
}

/** Parse the exact private token-endpoint/client-auth successor contract. */
function federation_oidc_token_client_config($bytes)
{
    if (!is_string($bytes) || strlen($bytes) < 2 || strlen($bytes) > FEDERATION_OIDC_TOKEN_MAX_CLIENT_CONFIG_BYTES)
        return false;
    $config = json_decode($bytes, true);
    if (!is_array($config))
        return false;
    $expected = array(
        'schema_version','issuer','token_endpoint','client_id','token_endpoint_auth_method',
        'authorization_client_config_sha256','max_exchange_response_bytes','connect_timeout_seconds','total_timeout_seconds'
    );
    $keys = array_keys($config); sort($keys); $sorted_expected = $expected; sort($sorted_expected);
    if ($keys !== $sorted_expected
        || $config['schema_version'] !== FEDERATION_OIDC_TOKEN_CLIENT_CONFIG_SCHEMA
        || !federation_oidc_token_valid_https_uri($config['issuer'])
        || !federation_oidc_token_valid_https_uri($config['token_endpoint'])
        || !federation_oidc_token_valid_client_id($config['client_id'])
        || $config['token_endpoint_auth_method'] !== 'client_secret_basic'
        || !is_string($config['authorization_client_config_sha256'])
        || preg_match('/^[a-f0-9]{64}$/D', $config['authorization_client_config_sha256']) !== 1
        || !is_int($config['max_exchange_response_bytes']) || $config['max_exchange_response_bytes'] < 4096
        || $config['max_exchange_response_bytes'] > FEDERATION_OIDC_TOKEN_MAX_RESPONSE_BYTES
        || !is_int($config['connect_timeout_seconds']) || $config['connect_timeout_seconds'] < 1 || $config['connect_timeout_seconds'] > 10
        || !is_int($config['total_timeout_seconds']) || $config['total_timeout_seconds'] < $config['connect_timeout_seconds']
        || $config['total_timeout_seconds'] > 20)
        return false;
    return $config;
}

/** Parse the already-accepted authorization client config only for cross-binding. */
function federation_oidc_token_authorization_client_identity($bytes)
{
    if (!is_string($bytes) || strlen($bytes) < 2 || strlen($bytes) > 32768)
        return false;
    $config = json_decode($bytes, true);
    if (!is_array($config))
        return false;
    $expected = array('schema_version','issuer','authorization_endpoint','client_id','redirect_uris','scopes');
    $keys = array_keys($config); sort($keys); $sorted_expected = $expected; sort($sorted_expected);
    if ($keys !== $sorted_expected || $config['schema_version'] !== 'notrinoserp.oidc-client-config.v1'
        || !federation_oidc_token_valid_https_uri($config['issuer'])
        || !federation_oidc_token_valid_client_id($config['client_id']))
        return false;
    return array('issuer'=>$config['issuer'], 'client_id'=>$config['client_id']);
}

/**
 * Validate a pinned, private RS256 JWKS shape without verifying a token.
 * Remote key indirection (jku/x5u), discovery and algorithm agility are absent.
 */
function federation_oidc_token_private_jwks($bytes)
{
    if (!is_string($bytes) || strlen($bytes) < 2 || strlen($bytes) > FEDERATION_OIDC_TOKEN_MAX_JWKS_BYTES)
        return false;
    $jwks = json_decode($bytes, true);
    if (!is_array($jwks) || array_keys($jwks) !== array('keys') || !is_array($jwks['keys'])
        || count($jwks['keys']) < 1 || count($jwks['keys']) > 32)
        return false;
    $seen = array();
    foreach ($jwks['keys'] as $jwk) {
        if (!is_array($jwk)) return false;
        $expected = array('kty','kid','use','alg','n','e');
        $keys = array_keys($jwk); sort($keys); $sorted_expected = $expected; sort($sorted_expected);
        if ($keys !== $sorted_expected || $jwk['kty'] !== 'RSA' || $jwk['use'] !== 'sig' || $jwk['alg'] !== 'RS256'
            || !is_string($jwk['kid']) || strlen($jwk['kid']) < 1 || strlen($jwk['kid']) > 128
            || preg_match('/^[A-Za-z0-9._:-]+$/D', $jwk['kid']) !== 1 || isset($seen[$jwk['kid']])
            || !is_string($jwk['n']) || preg_match('/^[A-Za-z0-9_-]{2,8192}$/D', $jwk['n']) !== 1
            || !is_string($jwk['e']) || preg_match('/^[A-Za-z0-9_-]{2,32}$/D', $jwk['e']) !== 1)
            return false;
        $seen[$jwk['kid']] = true;
    }
    return $jwks;
}

/**
 * Require the exact private material set needed by a future bounded token
 * exchange/verifier. Validation is readiness only and grants no runtime authority.
 */
function federation_oidc_token_verifier_material_contract($materials)
{
    if (!is_array($materials))
        return false;
    $required = array('oidc_jwks','oidc_client_config','oidc_token_client_config','oidc_client_secret','federation_subject_hmac_key');
    $allowed = array_merge($required, array('oidc_assurance_policy'));
    $keys = array_keys($materials);
    foreach ($required as $name)
        if (!array_key_exists($name, $materials))
            return false;
    foreach ($keys as $name)
        if (!in_array($name, $allowed, true))
            return false;
    if (isset($materials['oidc_assurance_policy'])
        && (!is_string($materials['oidc_assurance_policy']) || strlen($materials['oidc_assurance_policy']) < 2
            || strlen($materials['oidc_assurance_policy']) > 16384 || strpos($materials['oidc_assurance_policy'], "\0") !== false))
        return false;

    $authorization = federation_oidc_token_authorization_client_identity($materials['oidc_client_config']);
    $token = federation_oidc_token_client_config($materials['oidc_token_client_config']);
    if ($authorization === false || $token === false
        || !function_exists('federation_control_hash_equals')
        || !federation_control_hash_equals(hash('sha256', $materials['oidc_client_config']), $token['authorization_client_config_sha256'])
        || !federation_control_hash_equals(hash('sha256', $authorization['issuer']), hash('sha256', $token['issuer']))
        || !federation_control_hash_equals(hash('sha256', $authorization['client_id']), hash('sha256', $token['client_id']))
        || federation_oidc_token_private_jwks($materials['oidc_jwks']) === false)
        return false;

    $secret = $materials['oidc_client_secret'];
    if (!is_string($secret) || strlen($secret) < 16 || strlen($secret) > 4096 || strpos($secret, "\0") !== false)
        return false;
    if (!is_string($materials['federation_subject_hmac_key']) || strlen($materials['federation_subject_hmac_key']) !== 32)
        return false;

    return array(
        'issuer'=>$token['issuer'],
        'token_endpoint'=>$token['token_endpoint'],
        'client_id'=>$token['client_id'],
        'token_endpoint_auth_method'=>$token['token_endpoint_auth_method'],
        'max_exchange_response_bytes'=>$token['max_exchange_response_bytes'],
        'connect_timeout_seconds'=>$token['connect_timeout_seconds'],
        'total_timeout_seconds'=>$token['total_timeout_seconds'],
        'jwks_sha256'=>hash('sha256', $materials['oidc_jwks']),
        'authorization_client_config_sha256'=>$token['authorization_client_config_sha256'],
        'assurance_policy_sha256'=>isset($materials['oidc_assurance_policy']) ? hash('sha256', $materials['oidc_assurance_policy']) : null
    );
}

/** Provider/company-domain-separated non-reversible subject token only. */
function federation_oidc_external_subject_hash($hmac_key, $company, $provider_id, $subject)
{
    $company = (int)$company; $provider_id = (int)$provider_id;
    if (!is_string($hmac_key) || strlen($hmac_key) !== 32 || $company < 0 || $provider_id <= 0
        || !is_string($subject) || strlen($subject) < 1 || strlen($subject) > 2048)
        return false;
    return hash_hmac('sha256', "notrinoserp:federation-subject:v1\0".$company."\0".$provider_id."\0".$subject, $hmac_key);
}
