<?php
/**
 * PAY-SEC-004 private federation control-material loader foundation.
 *
 * This file is intentionally not wired into login, hooks, callbacks, session
 * issuance or provider enablement. It only validates and reads an explicitly
 * provisioned, company-scoped private bundle for a future reviewed verifier.
 */

if (!defined('FEDERATION_CONTROL_MANIFEST_SCHEMA'))
    define('FEDERATION_CONTROL_MANIFEST_SCHEMA', 'notrinoserp.federation-control.v1');
if (!defined('FEDERATION_CONTROL_MAX_MANIFEST_BYTES'))
    define('FEDERATION_CONTROL_MAX_MANIFEST_BYTES', 65536);
if (!defined('FEDERATION_CONTROL_MAX_MATERIAL_BYTES'))
    define('FEDERATION_CONTROL_MAX_MATERIAL_BYTES', 1048576);

/** Constant-time hash comparison compatible with PHP 5.6. */
function federation_control_hash_equals($known, $actual)
{
    if (!is_string($known) || !is_string($actual) || strlen($known) !== strlen($actual))
        return false;
    if (function_exists('hash_equals'))
        return hash_equals($known, $actual);
    $difference = 0;
    for ($i = 0, $length = strlen($known); $i < $length; $i++)
        $difference |= ord($known[$i]) ^ ord($actual[$i]);
    return $difference === 0;
}

/** Return true only for a lower-case SHA-256 hex digest. */
function federation_control_valid_sha256($value)
{
    return is_string($value) && preg_match('/^[a-f0-9]{64}$/D', $value) === 1;
}

/** Return the application root resolved from the normal runtime context. */
function federation_control_application_root()
{
    global $path_to_root;
    return isset($path_to_root) ? realpath($path_to_root) : false;
}

/**
 * Return the company-scoped private federation root outside the web/source tree.
 * A deployment may set FEDERATION_CONTROL_PRIVATE_ROOT or the per-company
 * federation_control_private_root connection setting. No directory is created.
 */
function federation_control_private_root($company = null)
{
    global $db_connections;

    if ($company === null)
        $company = function_exists('user_company') ? user_company() : 0;
    $company = (int)$company;
    if ($company < 0)
        return false;

    $application_root = federation_control_application_root();
    if ($application_root === false)
        return false;

    if (defined('FEDERATION_CONTROL_PRIVATE_ROOT')) {
        $base = rtrim((string)FEDERATION_CONTROL_PRIVATE_ROOT, "\\/");
    } elseif (isset($db_connections[$company]['federation_control_private_root'])) {
        $base = rtrim((string)$db_connections[$company]['federation_control_private_root'], "\\/");
    } else {
        $web_parent = dirname($application_root);
        $base = dirname($web_parent).DIRECTORY_SEPARATOR.'.notrinoserp-private'
            .DIRECTORY_SEPARATOR.substr(hash('sha256', $application_root), 0, 16)
            .DIRECTORY_SEPARATOR.'federation';
    }
    if ($base === '' || preg_match('/^(?:[A-Za-z]:[\\\\\/]|\/)/', $base) !== 1)
        return false;

    $normalized_base = rtrim(str_replace('\\', '/', $base), '/');
    $normalized_app = rtrim(str_replace('\\', '/', $application_root), '/');
    if ($normalized_base === $normalized_app
        || strpos($normalized_base.'/', $normalized_app.'/') === 0)
        return false;

    return $base.DIRECTORY_SEPARATOR.'company-'.$company;
}

/** Ensure a resolved child remains inside a resolved root. */
function federation_control_path_is_within($child, $root)
{
    $child = realpath($child);
    $root = realpath($root);
    if ($child === false || $root === false)
        return false;
    $child = str_replace('\\', '/', $child);
    $root = rtrim(str_replace('\\', '/', $root), '/');
    return $child !== $root && strpos($child, $root.'/') === 0;
}

/** Enforce owner-only private storage on POSIX; Windows ACLs are deployment-owned. */
function federation_control_private_permissions($path, $directory)
{
    if (DIRECTORY_SEPARATOR !== '/')
        return true;
    $permissions = @fileperms($path);
    if ($permissions === false || ($permissions & 0077) !== 0)
        return false;
    if ($directory)
        return ($permissions & 0400) !== 0 && ($permissions & 0100) !== 0;
    return ($permissions & 0400) !== 0;
}

/** Validate one safe basename; no traversal, separators, stream wrappers or hidden names. */
function federation_control_valid_material_filename($filename)
{
    return is_string($filename)
        && strlen($filename) >= 1 && strlen($filename) <= 96
        && $filename[0] !== '.'
        && strpos($filename, '/') === false && strpos($filename, '\\') === false
        && strpos($filename, ':') === false
        && preg_match('/^[A-Za-z0-9][A-Za-z0-9._-]*$/D', $filename) === 1;
}

/** Validate bounded ASCII metadata used only to select a provisioned bundle. */
function federation_control_valid_token($value, $max)
{
    return is_string($value) && strlen($value) >= 1 && strlen($value) <= $max
        && preg_match('/^[a-z0-9][a-z0-9._-]*$/D', $value) === 1;
}

/** Load and validate the private manifest; never create, fetch or mutate material. */
function federation_control_manifest($company = null)
{
    $company = $company === null && function_exists('user_company') ? user_company() : (int)$company;
    $root = federation_control_private_root($company);
    if ($root === false || is_link($root) || !is_dir($root)
        || !federation_control_private_permissions($root, true))
        return false;

    $application_root = federation_control_application_root();
    $resolved_root = realpath($root);
    if ($application_root === false || $resolved_root === false)
        return false;
    $normalized_app = rtrim(str_replace('\\', '/', $application_root), '/');
    $normalized_root = rtrim(str_replace('\\', '/', $resolved_root), '/');
    if ($normalized_root === $normalized_app
        || strpos($normalized_root.'/', $normalized_app.'/') === 0)
        return false;

    $path = $root.DIRECTORY_SEPARATOR.'federation-control-v1.json';
    if (is_link($path) || !is_file($path) || !federation_control_path_is_within($path, $root)
        || !federation_control_private_permissions($path, false))
        return false;
    $size = @filesize($path);
    if ($size === false || $size < 2 || $size > FEDERATION_CONTROL_MAX_MANIFEST_BYTES)
        return false;
    $json = @file_get_contents($path);
    if (!is_string($json) || strlen($json) !== (int)$size)
        return false;
    $manifest = json_decode($json, true);
    if (!is_array($manifest)
        || array_keys($manifest) !== array('schema_version','company_id','generation','bundles')
        || $manifest['schema_version'] !== FEDERATION_CONTROL_MANIFEST_SCHEMA
        || !is_int($manifest['company_id']) || $manifest['company_id'] !== (int)$company
        || !is_int($manifest['generation']) || $manifest['generation'] < 1
        || !is_array($manifest['bundles']) || count($manifest['bundles']) > 64)
        return false;

    $seen_refs = array();
    foreach ($manifest['bundles'] as $bundle) {
        if (!is_array($bundle)
            || array_keys($bundle) !== array('reference','protocol','verifier_profile','status','materials')
            || !federation_control_valid_token($bundle['reference'], 96)
            || !in_array($bundle['protocol'], array('oidc','saml'), true)
            || !in_array($bundle['verifier_profile'], array('oidc_code_pkce_v1','saml_sp_acs_v1'), true)
            || ($bundle['protocol'] === 'oidc' && $bundle['verifier_profile'] !== 'oidc_code_pkce_v1')
            || ($bundle['protocol'] === 'saml' && $bundle['verifier_profile'] !== 'saml_sp_acs_v1')
            || !in_array($bundle['status'], array('active','retired'), true)
            || !is_array($bundle['materials']) || count($bundle['materials']) < 1 || count($bundle['materials']) > 8)
            return false;
        $reference_hash = hash('sha256', $bundle['reference']);
        if (isset($seen_refs[$reference_hash]))
            return false;
        $seen_refs[$reference_hash] = true;
        $seen_kinds = array();
        foreach ($bundle['materials'] as $material) {
            if (!is_array($material)
                || array_keys($material) !== array('kind','file','sha256')
                || !in_array($material['kind'], array('oidc_jwks','oidc_client_config','oidc_token_client_config','oidc_client_secret','oidc_client_private_key','federation_subject_hmac_key','saml_idp_certificate'), true)
                || isset($seen_kinds[$material['kind']])
                || !federation_control_valid_material_filename($material['file'])
                || !federation_control_valid_sha256($material['sha256']))
                return false;
            if ($bundle['protocol'] === 'oidc' && $material['kind'] === 'saml_idp_certificate')
                return false;
            if ($bundle['protocol'] === 'saml' && in_array($material['kind'], array('oidc_jwks','oidc_client_config','oidc_token_client_config','oidc_client_secret','oidc_client_private_key'), true))
                return false;
            $seen_kinds[$material['kind']] = true;
        }
        if ($bundle['protocol'] === 'oidc' && !isset($seen_kinds['oidc_jwks']))
            return false;
        if ($bundle['protocol'] === 'saml' && !isset($seen_kinds['saml_idp_certificate']))
            return false;
    }
    return $manifest;
}

/**
 * Load one exact active material bundle by the DB-held hash of its opaque reference.
 * Returns validated bytes keyed by material kind; false on every ambiguity/error.
 */
function federation_control_load_bundle($company, $reference_hash, $protocol, $verifier_profile)
{
    if (!federation_control_valid_sha256($reference_hash)
        || !in_array($protocol, array('oidc','saml'), true)
        || !in_array($verifier_profile, array('oidc_code_pkce_v1','saml_sp_acs_v1'), true))
        return false;
    $manifest = federation_control_manifest($company);
    $root = federation_control_private_root($company);
    if ($manifest === false || $root === false)
        return false;

    $selected = false;
    foreach ($manifest['bundles'] as $bundle) {
        if (!federation_control_hash_equals(hash('sha256', $bundle['reference']), $reference_hash))
            continue;
        if ($selected !== false || $bundle['status'] !== 'active'
            || $bundle['protocol'] !== $protocol || $bundle['verifier_profile'] !== $verifier_profile)
            return false;
        $selected = $bundle;
    }
    if ($selected === false)
        return false;

    $result = array();
    foreach ($selected['materials'] as $material) {
        $path = $root.DIRECTORY_SEPARATOR.$material['file'];
        if (is_link($path) || !is_file($path) || !federation_control_path_is_within($path, $root)
            || !federation_control_private_permissions($path, false))
            return false;
        $size = @filesize($path);
        if ($size === false || $size < 1 || $size > FEDERATION_CONTROL_MAX_MATERIAL_BYTES)
            return false;
        $bytes = @file_get_contents($path);
        if (!is_string($bytes) || strlen($bytes) !== (int)$size
            || !federation_control_hash_equals(hash('sha256', $bytes), $material['sha256']))
            return false;
        $result[$material['kind']] = $bytes;
    }
    return array(
        'generation'=>(int)$manifest['generation'],
        'reference_hash'=>$reference_hash,
        'protocol'=>$protocol,
        'verifier_profile'=>$verifier_profile,
        'materials'=>$result,
    );
}
