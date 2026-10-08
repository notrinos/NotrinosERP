<?php
/**
 * PAY-GLB-001 S14: cryptographic NON-ROUTABLE attestation check. Exact
 * independent public-key anchors MUST come from a separately reviewed
 * server-owned trust store (absent at current source head). Never accept keys
 * from a request, manifest, provider report, a client session or this proof.
 * A successful signature alone does NOT establish object rights, trusted
 * collection or legal/provider certification.
 */
require_once __DIR__.'/pay_glb_001_source_admission.inc';
require_once __DIR__.'/pay_glb_001_access_admission.inc';

/** Canonical bytes for a domain-separated, time-bound, purpose-bound proof. */
function pay_glb_001_attestation_bytes($scope, $kind, $payload_sha256,
    $issuer_id, $expires_at_utc)
{
    if (!pay_glb_001_context_valid($scope, 'calendar') ||
        !in_array($kind, array('OBJECT_ACL','RESIDENCY','SOURCE_MAPPING'), true) ||
        !pay_glb_001_sha256($payload_sha256) ||
        !is_string($issuer_id) ||
        preg_match('/^[A-Z][A-Z0-9_-]{1,39}$/D', $issuer_id) !== 1 ||
        !pay_glb_001_iso_utc($expires_at_utc) ||
        $expires_at_utc <= $scope['as_of_utc']) return false;
    return json_encode(array('domain'=>'PAY-GLB-001-ATTESTATION-V1',
        'kind'=>$kind,'company_id'=>$scope['company_id'],
        'entity_codes'=>$scope['entity_codes'],'purpose'=>$scope['purpose'],
        'as_of_utc'=>$scope['as_of_utc'],'payload_sha256'=>$payload_sha256,
        'issuer_id'=>$issuer_id,'expires_at_utc'=>$expires_at_utc),
        JSON_UNESCAPED_SLASHES);
}

/** Parse a bounded, role-pinned RSA anchor without exposing malformed-key warnings. */
function pay_glb_001_trusted_rsa_details($anchor, $kind)
{
    if (!function_exists('openssl_pkey_get_public') ||
        !pay_glb_001_exact_keys($anchor, array('kind','public_key_pem')) ||
        $anchor['kind'] !== $kind || !is_string($anchor['public_key_pem']) ||
        strlen($anchor['public_key_pem']) > 8192) return false;
    $key = @openssl_pkey_get_public($anchor['public_key_pem']);
    if ($key === false) return false;
    $details = openssl_pkey_get_details($key);
    if (!is_array($details) || $details['type'] !== OPENSSL_KEYTYPE_RSA ||
        $details['bits'] < 2048 || !isset($details['rsa']['n'],$details['rsa']['e'])) return false;
    return $details;
}

/** Require an exact trusted role and a genuine RSA-SHA256 signature. */
function pay_glb_001_attestation_valid($scope, $kind, $payload_sha256,
    $attestation, $server_trusted_keys)
{
    if (!function_exists('openssl_verify') ||
        !pay_glb_001_exact_keys($attestation, array('kind','issuer_id',
            'expires_at_utc','signature_base64')) ||
        $attestation['kind'] !== $kind ||
        !is_string($attestation['issuer_id']) ||
        !is_array($server_trusted_keys) ||
        !isset($server_trusted_keys[$attestation['issuer_id']])) return false;
    $id = $attestation['issuer_id'];
    $anchor = $server_trusted_keys[$id];
    $details = pay_glb_001_trusted_rsa_details($anchor, $kind);
    if ($details === false || !is_string($attestation['signature_base64']) ||
        strlen($attestation['signature_base64']) > 2048 ||
        preg_match('/^[A-Za-z0-9+\/]+={0,2}$/D', $attestation['signature_base64']) !== 1) return false;
    $data = pay_glb_001_attestation_bytes($scope,$kind,$payload_sha256,$id,
        $attestation['expires_at_utc']);
    $sig = base64_decode($attestation['signature_base64'], true);
    if ($data === false || $sig === false || strlen($sig) < 128) return false;
    return openssl_verify($data, $sig, $details['key'], OPENSSL_ALGO_SHA256) === 1;
}

/**
 * Three separately pinned issuers must attest object ACL, residency and
 * source mapping. No attestation may reuse another issuer/key.
 */
function pay_glb_001_signed_bundle_valid($scope, $admission, $manifest,
    $signed, $server_trusted_keys)
{
    if (!is_array($admission) || !is_array($manifest) ||
        !pay_glb_001_exact_keys($signed, array('object_acl','residency','source_mapping')) ||
        !isset($admission['admission_sha256'],$manifest['manifest_sha256']) ||
        !pay_glb_001_sha256($admission['admission_sha256']) ||
        !pay_glb_001_sha256($manifest['manifest_sha256']) ||
        !hash_equals($admission['admission_sha256'],
            (string)pay_glb_001_access_admission_sha256($admission)) ||
        !hash_equals($manifest['manifest_sha256'],
            (string)pay_glb_001_source_manifest_sha256($manifest))) return false;
    $ids = array(); $key_fingerprints = array();
    foreach (array('object_acl'=>'OBJECT_ACL','residency'=>'RESIDENCY',
            'source_mapping'=>'SOURCE_MAPPING') as $name=>$kind) {
        if (!is_array($signed[$name]) || !isset($signed[$name]['issuer_id']) ||
            !is_string($signed[$name]['issuer_id']) ||
            isset($ids[$signed[$name]['issuer_id']])) return false;
        $ids[$signed[$name]['issuer_id']] = true;
        $payload = $kind === 'SOURCE_MAPPING' ? $manifest['manifest_sha256'] :
            $admission['admission_sha256'];
        if (!pay_glb_001_attestation_valid($scope,$kind,$payload,
                $signed[$name],$server_trusted_keys)) return false;
        $details = pay_glb_001_trusted_rsa_details(
            $server_trusted_keys[$signed[$name]['issuer_id']], $kind);
        if ($details === false) return false;
        // Identify mathematical keys, so alternate PEM whitespace/encodings
        // cannot disguise one key reused by otherwise distinct issuer IDs.
        $fingerprint = hash('sha256', base64_encode($details['rsa']['n']).':' .
            base64_encode($details['rsa']['e']));
        if (isset($key_fingerprints[$fingerprint])) return false;
        $key_fingerprints[$fingerprint] = true;
    }
    return true;
}
?>
