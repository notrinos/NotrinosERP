<?php
/**
 * PAY-SEC-004 pinned OIDC ID-token verifier adaptor foundation.
 *
 * Pure verification only. This file accepts one already-received ID token and
 * verifies it against the already-approved private RS256 JWKS plus exact
 * transaction bindings. It performs no token exchange, network discovery,
 * replay/transaction/decision mutation, identity linking, login/session
 * issuance, assurance elevation, payroll mutation or accounting mutation.
 */

if (!defined('FEDERATION_OIDC_TOKEN_VERIFIER_MAX_JWT_BYTES'))
    define('FEDERATION_OIDC_TOKEN_VERIFIER_MAX_JWT_BYTES', 262144);
if (!defined('FEDERATION_OIDC_TOKEN_VERIFIER_MAX_JSON_BYTES'))
    define('FEDERATION_OIDC_TOKEN_VERIFIER_MAX_JSON_BYTES', 65536);
if (!defined('FEDERATION_OIDC_TOKEN_VERIFIER_MAX_CLOCK_SKEW'))
    define('FEDERATION_OIDC_TOKEN_VERIFIER_MAX_CLOCK_SKEW', 60);
if (!defined('FEDERATION_OIDC_TOKEN_VERIFIER_PINNED_JWKS_URI'))
    define('FEDERATION_OIDC_TOKEN_VERIFIER_PINNED_JWKS_URI', 'urn:notrinoserp:pinned-private-jwks');

function federation_oidc_token_verifier_base64url_decode($value)
{
    if (!is_string($value) || $value === '' || strlen($value) > FEDERATION_OIDC_TOKEN_VERIFIER_MAX_JSON_BYTES * 2
        || preg_match('/^[A-Za-z0-9_-]+$/D', $value) !== 1 || strlen($value) % 4 === 1)
        return false;
    $padding = (4 - (strlen($value) % 4)) % 4;
    $decoded = base64_decode(strtr($value, '-_', '+/').str_repeat('=', $padding), true);
    if (!is_string($decoded) || strlen($decoded) > FEDERATION_OIDC_TOKEN_VERIFIER_MAX_JSON_BYTES)
        return false;
    $canonical = rtrim(strtr(base64_encode($decoded), '+/', '-_'), '=');
    return $canonical === $value ? $decoded : false;
}

function federation_oidc_token_verifier_parse($jwt)
{
    if (!is_string($jwt) || strlen($jwt) < 16 || strlen($jwt) > FEDERATION_OIDC_TOKEN_VERIFIER_MAX_JWT_BYTES
        || strpos($jwt, "\0") !== false)
        return false;
    $parts = explode('.', $jwt);
    if (count($parts) !== 3 || $parts[0] === '' || $parts[1] === '' || $parts[2] === ''
        || federation_oidc_token_verifier_base64url_decode($parts[2]) === false)
        return false;
    $header_bytes = federation_oidc_token_verifier_base64url_decode($parts[0]);
    $claims_bytes = federation_oidc_token_verifier_base64url_decode($parts[1]);
    if ($header_bytes === false || $claims_bytes === false)
        return false;
    $header = json_decode($header_bytes, true);
    $claims = json_decode($claims_bytes, true);
    if (!is_array($header) || !is_array($claims) || array_values($header) === $header || array_values($claims) === $claims)
        return false;

    $allowed_header = array('alg'=>true, 'kid'=>true, 'typ'=>true);
    foreach ($header as $key=>$value)
        if (!isset($allowed_header[$key]))
            return false;
    if (!isset($header['alg'], $header['kid']) || $header['alg'] !== 'RS256'
        || !is_string($header['kid']) || strlen($header['kid']) < 1 || strlen($header['kid']) > 128
        || preg_match('/^[A-Za-z0-9._:-]+$/D', $header['kid']) !== 1
        || (isset($header['typ']) && $header['typ'] !== 'JWT'))
        return false;

    return array('jwt'=>$jwt, 'header'=>$header, 'claims'=>$claims);
}

function federation_oidc_token_verifier_valid_subject($subject)
{
    return is_string($subject) && strlen($subject) >= 1 && strlen($subject) <= 255
        && preg_match('/[\x00-\x1f\x7f]/', $subject) !== 1;
}

function federation_oidc_token_verifier_audience_matches($audience, $azp, $client_id)
{
    if (!is_string($client_id) || $client_id === '')
        return false;
    if (is_string($audience)) {
        if (!function_exists('federation_control_hash_equals'))
            return false;
        if (!federation_control_hash_equals(hash('sha256', $client_id), hash('sha256', $audience)))
            return false;
        return $azp === null || (is_string($azp)
            && federation_control_hash_equals(hash('sha256', $client_id), hash('sha256', $azp)));
    }
    if (!is_array($audience) || count($audience) < 1 || count($audience) > 16)
        return false;
    $seen = array(); $matched = false;
    foreach ($audience as $value) {
        if (!is_string($value) || strlen($value) < 1 || strlen($value) > 256
            || preg_match('/[\x00-\x1f\x7f]/', $value) === 1 || isset($seen[$value]))
            return false;
        $seen[$value] = true;
        if (function_exists('federation_control_hash_equals')
            && federation_control_hash_equals(hash('sha256', $client_id), hash('sha256', $value)))
            $matched = true;
    }
    if (!$matched)
        return false;
    if (count($audience) > 1)
        return is_string($azp) && function_exists('federation_control_hash_equals')
            && federation_control_hash_equals(hash('sha256', $client_id), hash('sha256', $azp));
    return $azp === null || (is_string($azp) && function_exists('federation_control_hash_equals')
        && federation_control_hash_equals(hash('sha256', $client_id), hash('sha256', $azp)));
}

/**
 * Verify an RS256 signature with the selected library while hard-blocking all
 * network/discovery fetches. The library may only read the supplied pinned JWKS.
 */
function federation_oidc_token_verifier_library_signature($jwt, $jwks_bytes, $issuer, $client_id)
{
    if (!class_exists('Jumbojett\\OpenIDConnectClient') || !class_exists('Jumbojett\\OpenIDConnectClientException'))
        return false;
    if (!is_string($jwks_bytes) || federation_oidc_token_private_jwks($jwks_bytes) === false)
        return false;
    try {
        $client = new class($issuer, $client_id, $jwks_bytes) extends \Jumbojett\OpenIDConnectClient {
            private $pinnedJwks;
            public function __construct($issuer, $client_id, $pinned_jwks)
            {
                parent::__construct($issuer, $client_id, '', $issuer);
                $this->pinnedJwks = $pinned_jwks;
                $this->providerConfigParam(array('jwks_uri'=>FEDERATION_OIDC_TOKEN_VERIFIER_PINNED_JWKS_URI));
            }
            protected function fetchURL(string $url, string $post_body = null, array $headers = [])
            {
                if ($url === FEDERATION_OIDC_TOKEN_VERIFIER_PINNED_JWKS_URI && $post_body === null && count($headers) === 0)
                    return $this->pinnedJwks;
                throw new \Jumbojett\OpenIDConnectClientException('Network/discovery fetch denied by pinned OIDC verifier adaptor.');
            }
            public function verifyPinnedSignature($jwt)
            {
                $this->verifySignatures($jwt);
                return true;
            }
        };
        return $client->verifyPinnedSignature($jwt) === true;
    } catch (Exception $e) {
        return false;
    } catch (Error $e) {
        return false;
    }
}

/**
 * Verify one already-received ID token and return a bounded non-PII decision
 * candidate. A caller-provided signature verifier is allowed only for focused
 * unit tests; production callers omit it and use the pinned selected library.
 */
function federation_oidc_token_verifier_verify($jwt, $expectations, $materials, $now_epoch = null, $signature_verifier = null)
{
    if (!is_array($expectations) || !is_array($materials))
        return false;
    $expected_keys = array('company_id','provider_id','issuer','client_id','nonce');
    $keys = array_keys($expectations); sort($keys); $sorted = $expected_keys; sort($sorted);
    if ($keys !== $sorted || (int)$expectations['company_id'] < 0 || (int)$expectations['provider_id'] <= 0
        || !is_string($expectations['issuer']) || !federation_oidc_token_valid_https_uri($expectations['issuer'])
        || !federation_oidc_token_valid_client_id($expectations['client_id'])
        || !is_string($expectations['nonce']) || preg_match('/^[A-Za-z0-9_-]{32,128}$/D', $expectations['nonce']) !== 1)
        return false;

    $material_contract = federation_oidc_token_verifier_material_contract($materials);
    if ($material_contract === false || !function_exists('federation_control_hash_equals')
        || !federation_control_hash_equals(hash('sha256', $expectations['issuer']), hash('sha256', $material_contract['issuer']))
        || !federation_control_hash_equals(hash('sha256', $expectations['client_id']), hash('sha256', $material_contract['client_id'])))
        return false;

    $parsed = federation_oidc_token_verifier_parse($jwt);
    if ($parsed === false)
        return false;
    $claims = $parsed['claims'];
    foreach (array('iss','sub','aud','exp','iat','nonce') as $required)
        if (!array_key_exists($required, $claims))
            return false;
    if (!is_string($claims['iss']) || !federation_control_hash_equals(hash('sha256', $expectations['issuer']), hash('sha256', $claims['iss']))
        || !federation_oidc_token_verifier_valid_subject($claims['sub'])
        || !federation_oidc_token_verifier_audience_matches($claims['aud'], isset($claims['azp']) ? $claims['azp'] : null, $expectations['client_id'])
        || !is_int($claims['exp']) || !is_int($claims['iat'])
        || (isset($claims['nbf']) && !is_int($claims['nbf']))
        || !is_string($claims['nonce'])
        || !federation_control_hash_equals(hash('sha256', $expectations['nonce']), hash('sha256', $claims['nonce'])))
        return false;

    $now = $now_epoch === null ? time() : $now_epoch;
    if (!is_int($now) || $now <= 0)
        return false;
    $skew = FEDERATION_OIDC_TOKEN_VERIFIER_MAX_CLOCK_SKEW;
    if ($claims['exp'] < $now - $skew || $claims['iat'] > $now + $skew
        || $claims['exp'] <= $claims['iat']
        || (isset($claims['nbf']) && $claims['nbf'] > $now + $skew))
        return false;

    if ($signature_verifier !== null) {
        if (!is_callable($signature_verifier))
            return false;
        try {
            $signature_ok = call_user_func($signature_verifier, $parsed['jwt'], $materials['oidc_jwks'], $parsed['header']) === true;
        } catch (Exception $e) {
            $signature_ok = false;
        } catch (Error $e) {
            $signature_ok = false;
        }
    } else {
        $signature_ok = federation_oidc_token_verifier_library_signature(
            $parsed['jwt'], $materials['oidc_jwks'], $expectations['issuer'], $expectations['client_id']);
    }
    if (!$signature_ok)
        return false;

    $subject_hash = federation_oidc_external_subject_hash(
        $materials['federation_subject_hmac_key'], (int)$expectations['company_id'], (int)$expectations['provider_id'], $claims['sub']);
    if ($subject_hash === false)
        return false;

    return array(
        'verified'=>true,
        'provider_id'=>(int)$expectations['provider_id'],
        'external_subject_hash'=>$subject_hash,
        'issuer_sha256'=>hash('sha256', $claims['iss']),
        'client_id_sha256'=>hash('sha256', $expectations['client_id']),
        'nonce_sha256'=>hash('sha256', $claims['nonce']),
        'kid_sha256'=>hash('sha256', $parsed['header']['kid']),
        'algorithm'=>'RS256',
        'issued_at_epoch'=>$claims['iat'],
        'not_before_epoch'=>isset($claims['nbf']) ? $claims['nbf'] : null,
        'expires_at_epoch'=>$claims['exp'],
        'verified_at_epoch'=>$now,
        'assurance_method'=>'oidc_rs256'
    );
}
