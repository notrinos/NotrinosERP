<?php
/**
 * PAY-SEC-004 signed OIDC assurance-candidate foundation.
 *
 * Pure verification only. This helper reuses the already-approved pinned
 * RS256 ID-token verifier and, when an explicitly provisioned private policy
 * is present, validates exact signed acr/amr/auth_time evidence. It returns a
 * bounded non-PII candidate only. It never writes a verifier decision,
 * assurance event, session, recovery factor, payroll row or accounting row.
 */

if (!defined('FEDERATION_OIDC_ASSURANCE_POLICY_SCHEMA'))
    define('FEDERATION_OIDC_ASSURANCE_POLICY_SCHEMA', 'notrinoserp.oidc-assurance-policy.v1');
if (!defined('FEDERATION_OIDC_ASSURANCE_MAX_POLICY_BYTES'))
    define('FEDERATION_OIDC_ASSURANCE_MAX_POLICY_BYTES', 16384);
if (!defined('FEDERATION_OIDC_ASSURANCE_MAX_METHODS'))
    define('FEDERATION_OIDC_ASSURANCE_MAX_METHODS', 8);

function federation_oidc_assurance_valid_token($value, $max)
{
    return is_string($value) && strlen($value) >= 1 && strlen($value) <= (int)$max
        && preg_match('/^[A-Za-z0-9][A-Za-z0-9._:\/-]*$/D', $value) === 1;
}

/** Parse the exact private policy for a future level-two federated step-up. */
function federation_oidc_assurance_policy($bytes)
{
    if (!is_string($bytes) || strlen($bytes) < 2 || strlen($bytes) > FEDERATION_OIDC_ASSURANCE_MAX_POLICY_BYTES)
        return false;
    $policy = json_decode($bytes, true);
    if (!is_array($policy))
        return false;
    $expected = array('schema_version','assurance_level','required_acr','required_amr','max_auth_age_seconds','max_assurance_lifetime_seconds');
    $keys = array_keys($policy); sort($keys); $sorted = $expected; sort($sorted);
    if ($keys !== $sorted || $policy['schema_version'] !== FEDERATION_OIDC_ASSURANCE_POLICY_SCHEMA
        || $policy['assurance_level'] !== 2
        || !federation_oidc_assurance_valid_token($policy['required_acr'], 256)
        || !is_array($policy['required_amr']) || count($policy['required_amr']) < 1
        || count($policy['required_amr']) > FEDERATION_OIDC_ASSURANCE_MAX_METHODS
        || !is_int($policy['max_auth_age_seconds']) || $policy['max_auth_age_seconds'] < 60 || $policy['max_auth_age_seconds'] > 900
        || !is_int($policy['max_assurance_lifetime_seconds']) || $policy['max_assurance_lifetime_seconds'] < 60
        || $policy['max_assurance_lifetime_seconds'] > $policy['max_auth_age_seconds'])
        return false;
    $seen = array();
    foreach ($policy['required_amr'] as $method) {
        if (!federation_oidc_assurance_valid_token($method, 64) || isset($seen[$method]))
            return false;
        $seen[$method] = true;
    }
    $methods = array_keys($seen); sort($methods, SORT_STRING); $policy['required_amr'] = $methods;
    return $policy;
}

function federation_oidc_assurance_signed_methods($claims)
{
    if (!is_array($claims) || !isset($claims['amr']) || !is_array($claims['amr'])
        || count($claims['amr']) < 1 || count($claims['amr']) > FEDERATION_OIDC_ASSURANCE_MAX_METHODS)
        return false;
    $seen = array();
    foreach ($claims['amr'] as $method) {
        if (!federation_oidc_assurance_valid_token($method, 64) || isset($seen[$method]))
            return false;
        $seen[$method] = true;
    }
    $methods = array_keys($seen); sort($methods, SORT_STRING); return $methods;
}

/**
 * Verify signed assurance inputs without granting elevated authority.
 *
 * The returned candidate is intentionally not accepted by the existing
 * atomic level-one decision/session path. A later reviewed issuer/consumer
 * must bind it to one durable session and one high-risk action before use.
 */
function federation_oidc_assurance_candidate_verify($jwt, $expectations, $materials, $now_epoch = null, $signature_verifier = null)
{
    if (!is_array($materials) || !isset($materials['oidc_assurance_policy'])
        || !function_exists('federation_oidc_token_verifier_verify')
        || !function_exists('federation_oidc_token_verifier_parse'))
        return false;
    $policy = federation_oidc_assurance_policy($materials['oidc_assurance_policy']);
    if ($policy === false)
        return false;

    $base = federation_oidc_token_verifier_verify($jwt, $expectations, $materials, $now_epoch, $signature_verifier);
    if (!is_array($base) || empty($base['verified']) || (string)$base['assurance_method'] !== 'oidc_rs256')
        return false;
    $parsed = federation_oidc_token_verifier_parse($jwt);
    if ($parsed === false || !isset($parsed['claims']) || !is_array($parsed['claims']))
        return false;
    $claims = $parsed['claims'];
    if (!isset($claims['acr'], $claims['amr'], $claims['auth_time'])
        || !is_string($claims['acr']) || !federation_oidc_assurance_valid_token($claims['acr'], 256)
        || !is_int($claims['auth_time']) || $claims['auth_time'] <= 0)
        return false;
    $methods = federation_oidc_assurance_signed_methods($claims);
    if ($methods === false || !function_exists('federation_control_hash_equals')
        || !federation_control_hash_equals(hash('sha256', $policy['required_acr']), hash('sha256', $claims['acr'])))
        return false;
    if ($methods !== $policy['required_amr'])
        return false;

    $now = $now_epoch === null ? time() : $now_epoch;
    if (!is_int($now) || $now <= 0 || $claims['auth_time'] > $now + FEDERATION_OIDC_TOKEN_VERIFIER_MAX_CLOCK_SKEW
        || $claims['auth_time'] > (int)$base['issued_at_epoch'] + FEDERATION_OIDC_TOKEN_VERIFIER_MAX_CLOCK_SKEW
        || $now - $claims['auth_time'] > $policy['max_auth_age_seconds'])
        return false;
    $expires = min((int)$base['expires_at_epoch'], $claims['auth_time'] + $policy['max_auth_age_seconds'],
        $now + $policy['max_assurance_lifetime_seconds']);
    if ($expires <= $now)
        return false;

    return array(
        'verified'=>true,
        'provider_id'=>(int)$base['provider_id'],
        'external_subject_hash'=>(string)$base['external_subject_hash'],
        'issuer_sha256'=>(string)$base['issuer_sha256'],
        'client_id_sha256'=>(string)$base['client_id_sha256'],
        'acr_sha256'=>hash('sha256', $claims['acr']),
        'amr_sha256'=>hash('sha256', implode("\0", $methods)),
        'assurance_proof_sha256'=>hash('sha256', $jwt),
        'verified_assurance_level'=>2,
        'assurance_method'=>'oidc_acr_amr',
        'auth_time_epoch'=>$claims['auth_time'],
        'verified_at_epoch'=>$now,
        'expires_at_epoch'=>$expires,
        'elevation_issued'=>false,
        'assurance_event_written'=>false,
        'recovery_factor_verified'=>false
    );
}
