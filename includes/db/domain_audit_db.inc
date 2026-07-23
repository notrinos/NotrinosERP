<?php
/**********************************************************************
    Copyright (C) NotrinosERP.
    Released under the terms of the GNU General Public License, GPL,
    as published by the Free Software Foundation, either version 3
    of the License, or (at your option) any later version.
***********************************************************************/

/**
 * Append-only HR/payroll domain-audit foundation.
 *
 * Business writers append an event, an export outbox record and a signed
 * checkpoint in the same database transaction. Event/checkpoint/outbox rows
 * are protected from UPDATE and DELETE by database triggers installed by the
 * PAY-AUD-001 migration. Mutable chain_state is only a serialization cursor;
 * it is never treated as audit evidence.
 */

/**
 * Return true when an array uses non-sequential keys.
 *
 * @param array $value
 * @return bool
 */
function domain_audit_is_associative($value) {
    if (!is_array($value) || count($value) === 0) {
        return false;
    }
    return array_keys($value) !== range(0, count($value) - 1);
}

/**
 * Normalize a value for deterministic JSON hashing.
 *
 * @param mixed $value
 * @return mixed
 */
function domain_audit_normalize_value($value) {
    if (is_object($value)) {
        $value = get_object_vars($value);
    }
    if (is_array($value)) {
        $normalized = array();
        if (domain_audit_is_associative($value)) {
            $keys = array_keys($value);
            sort($keys, SORT_STRING);
            foreach ($keys as $key) {
                $normalized[(string)$key] = domain_audit_normalize_value($value[$key]);
            }
        } else {
            foreach ($value as $item) {
                $normalized[] = domain_audit_normalize_value($item);
            }
        }
        return $normalized;
    }
    if (is_float($value)) {
        $value = rtrim(rtrim(sprintf('%.12F', $value), '0'), '.');
        return $value === '-0' ? '0' : $value;
    }
    if (is_int($value) || is_bool($value) || $value === null) {
        return $value;
    }
    return (string)$value;
}

/**
 * Encode canonical JSON without HTML or locale transformations.
 *
 * @param mixed $value
 * @return string|false
 */
function domain_audit_canonical_json($value) {
    $flags = 0;
    if (defined('JSON_UNESCAPED_SLASHES')) {
        $flags |= JSON_UNESCAPED_SLASHES;
    }
    if (defined('JSON_UNESCAPED_UNICODE')) {
        $flags |= JSON_UNESCAPED_UNICODE;
    }
    $json = json_encode(domain_audit_normalize_value($value), $flags);
    return is_string($json) ? $json : false;
}

/**
 * Quote an internal canonical string without UI HTML encoding.
 *
 * @param string $value
 * @return string
 */
function domain_audit_db_string($value) {
    $escaped = function_exists('db_escape_function')
        ? db_escape_function((string)$value) : addslashes((string)$value);
    return "'".$escaped."'";
}

/**
 * Constant-time comparison for PHP 5.6+.
 *
 * @param string $known
 * @param string $actual
 * @return bool
 */
function domain_audit_hash_equals($known, $actual) {
    if (!is_string($known) || !is_string($actual) || strlen($known) !== strlen($actual)) {
        return false;
    }
    if (function_exists('hash_equals')) {
        return hash_equals($known, $actual);
    }
    $difference = 0;
    for ($i = 0, $length = strlen($known); $i < $length; $i++) {
        $difference |= ord($known[$i]) ^ ord($actual[$i]);
    }
    return $difference === 0;
}

/**
 * Return the private audit root outside the application web root.
 *
 * @param int|null $company
 * @return string|false
 */
function domain_audit_private_root($company = null) {
    global $path_to_root, $db_connections;

    if ($company === null) {
        $company = function_exists('user_company') ? user_company() : 0;
    }
    $application_root = realpath($path_to_root);
    if ($application_root === false) {
        return false;
    }
    if (defined('PAYROLL_AUDIT_PRIVATE_ROOT')) {
        $base = rtrim((string)PAYROLL_AUDIT_PRIVATE_ROOT, "\\/");
    } elseif (isset($db_connections[$company]['payroll_audit_private_root'])) {
        $base = rtrim((string)$db_connections[$company]['payroll_audit_private_root'], "\\/");
    } else {
        $web_parent = dirname($application_root);
        $base = dirname($web_parent).DIRECTORY_SEPARATOR.'.notrinoserp-private'
            .DIRECTORY_SEPARATOR.substr(hash('sha256', $application_root), 0, 16)
            .DIRECTORY_SEPARATOR.'audit';
    }
    if ($base === '' || !preg_match('/^(?:[A-Za-z]:[\\\\\/]|\/)/', $base)) {
        return false;
    }
    $normalized_base = str_replace('\\', '/', $base);
    $normalized_app = str_replace('\\', '/', $application_root);
    if ($normalized_base === $normalized_app
        || strpos($normalized_base.'/', rtrim($normalized_app, '/').'/') === 0
    ) {
        return false;
    }
    return $base.DIRECTORY_SEPARATOR.'company-'.(int)$company;
}

/**
 * Create private audit directories.
 *
 * @param int|null $company
 * @return array|false
 */
function ensure_domain_audit_private_storage($company = null) {
    global $path_to_root;

    $root = domain_audit_private_root($company);
    if ($root === false) {
        return false;
    }
    $paths = array(
        'root' => $root,
        'keys' => $root.DIRECTORY_SEPARATOR.'keys',
        'exports' => $root.DIRECTORY_SEPARATOR.'exports',
    );
    foreach ($paths as $path) {
        if (is_link($path)) {
            return false;
        }
        if (!is_dir($path) && !@mkdir($path, 0700, true) && !is_dir($path)) {
            return false;
        }
        @chmod($path, 0700);
        $permissions = @fileperms($path);
        if (DIRECTORY_SEPARATOR === '/' && $permissions !== false && ($permissions & 0077) !== 0) {
            return false;
        }
        if (!file_exists($path.DIRECTORY_SEPARATOR.'.htaccess')) {
            @file_put_contents($path.DIRECTORY_SEPARATOR.'.htaccess', "Require all denied\nDeny from all\n");
        }
        if (!file_exists($path.DIRECTORY_SEPARATOR.'web.config')) {
            @file_put_contents($path.DIRECTORY_SEPARATOR.'web.config', '<?xml version="1.0" encoding="UTF-8"?>'
                .'<configuration><system.webServer><security><authorization>'
                .'<remove users="*" roles="" verbs="" />'
                .'<add accessType="Deny" users="*" />'
                .'</authorization></security></system.webServer></configuration>');
        }
    }
    $application_root = realpath($path_to_root);
    $resolved_root = realpath($paths['root']);
    if ($application_root === false || $resolved_root === false) {
        return false;
    }
    $normalized_app = rtrim(str_replace('\\', '/', $application_root), '/');
    $normalized_root = rtrim(str_replace('\\', '/', $resolved_root), '/');
    if ($normalized_root === $normalized_app
        || strpos($normalized_root.'/', $normalized_app.'/') === 0
    ) {
        return false;
    }
    return $paths;
}

/**
 * Load the per-company audit signing/HMAC key and create it only when explicitly allowed.
 *
 * Normal audit writes and verification must never generate a replacement key after
 * key loss. Initial key creation is reserved for the controlled migration path.
 *
 * @param int|null $company
 * @param bool $allow_create
 * @return string|false Raw 32-byte key.
 */
function domain_audit_legacy_signing_key($company = null, $allow_create = false) {
    if (defined('PAYROLL_AUDIT_SIGNING_KEY')) {
        $decoded = base64_decode((string)PAYROLL_AUDIT_SIGNING_KEY, true);
        return is_string($decoded) && strlen($decoded) === 32 ? $decoded : false;
    }
    $root = domain_audit_private_root($company);
    if ($root === false) {
        return false;
    }
    $key_path = $root.DIRECTORY_SEPARATOR.'keys'.DIRECTORY_SEPARATOR.'domain-audit-hmac-v1.key';
    if (is_file($key_path)) {
        @chmod($key_path, 0600);
        $permissions = @fileperms($key_path);
        if (DIRECTORY_SEPARATOR === '/' && $permissions !== false && ($permissions & 0077) !== 0) {
            return false;
        }
        $decoded = base64_decode(trim((string)@file_get_contents($key_path)), true);
        return is_string($decoded) && strlen($decoded) === 32 ? $decoded : false;
    }
    if (!$allow_create) {
        return false;
    }
    $paths = ensure_domain_audit_private_storage($company);
    if ($paths === false) {
        return false;
    }
    $key_path = $paths['keys'].DIRECTORY_SEPARATOR.'domain-audit-hmac-v1.key';
    if (is_file($key_path)) {
        @chmod($key_path, 0600);
        $permissions = @fileperms($key_path);
        if (DIRECTORY_SEPARATOR === '/' && $permissions !== false && ($permissions & 0077) !== 0) {
            return false;
        }
        $decoded = base64_decode(trim((string)@file_get_contents($key_path)), true);
        return is_string($decoded) && strlen($decoded) === 32 ? $decoded : false;
    }
    if (!function_exists('openssl_random_pseudo_bytes')) {
        return false;
    }
    $strong = false;
    $key = openssl_random_pseudo_bytes(32, $strong);
    if ($key === false || strlen($key) !== 32 || !$strong) {
        return false;
    }
    $handle = @fopen($key_path, 'x+b');
    if ($handle === false) {
        if (is_file($key_path)) {
            @chmod($key_path, 0600);
            $permissions = @fileperms($key_path);
            if (DIRECTORY_SEPARATOR === '/' && $permissions !== false && ($permissions & 0077) !== 0) {
                return false;
            }
            $decoded = base64_decode(trim((string)@file_get_contents($key_path)), true);
            return is_string($decoded) && strlen($decoded) === 32 ? $decoded : false;
        }
        return false;
    }
    $encoded = base64_encode($key)."\n";
    $written = fwrite($handle, $encoded);
    $flushed = fflush($handle);
    fclose($handle);
    @chmod($key_path, 0600);
    $permissions = @fileperms($key_path);
    if ($written !== strlen($encoded) || !$flushed
        || (DIRECTORY_SEPARATOR === '/' && $permissions !== false && ($permissions & 0077) !== 0)
    ) {
        @unlink($key_path);
        return false;
    }
    return $key;
}

/**
 * Return the current actor/security context without credentials or session IDs.
 *
 * @param int $company
 * @return array
 */
function domain_audit_current_security_context($company) {
    $user = isset($_SESSION['wa_current_user']) ? $_SESSION['wa_current_user'] : null;
    $role_set = array();
    if (is_object($user) && isset($user->role_set) && is_array($user->role_set)) {
        foreach ($user->role_set as $area) {
            $role_set[] = (int)$area;
        }
        sort($role_set, SORT_NUMERIC);
    }
    return array(
        'actor_user_id' => is_object($user) && isset($user->user) ? (int)$user->user : 0,
        'company_id' => (int)$company,
        'role_id' => is_object($user) && isset($user->access) ? (int)$user->access : 0,
        'role_set' => $role_set,
        'auth_method' => is_object($user) && isset($user->auth_method)
            ? (string)$user->auth_method : 'system',
    );
}

/**
 * Generate a 128-bit hexadecimal correlation ID.
 *
 * @return string|false
 */
function domain_audit_correlation_id() {
    if (!function_exists('openssl_random_pseudo_bytes')) {
        return false;
    }
    $strong = false;
    $bytes = openssl_random_pseudo_bytes(16, $strong);
    return $bytes !== false && strlen($bytes) === 16 && $strong ? bin2hex($bytes) : false;
}

/**
 * Derive a purpose-specific sub-key from the protected company key.
 *
 * @param string $key
 * @param string $purpose
 * @return string|false Raw 32-byte sub-key.
 */
function domain_audit_derived_key($key, $purpose) {
    if (!is_string($key) || strlen($key) !== 32 || !is_string($purpose) || $purpose === '') {
        return false;
    }
    return hash_hmac('sha256', $purpose, $key, true);
}

/**
 * Return a non-secret identifier for the active protected key.
 *
 * @param string $key
 * @return string|false
 */
function domain_audit_key_id($key) {
    $derived = domain_audit_derived_key($key, 'key-identifier-v1');
    return $derived === false ? false : substr(bin2hex($derived), 0, 16);
}

/**
 * Return fixed keyring paths for one company.
 *
 * @param int|null $company
 * @return array|false
 */
function domain_audit_keyring_paths($company = null) {
    $root = domain_audit_private_root($company);
    if ($root === false) {
        return false;
    }
    $keys = $root.DIRECTORY_SEPARATOR.'keys';
    return array(
        'keys' => $keys,
        'legacy_key' => $keys.DIRECTORY_SEPARATOR.'domain-audit-hmac-v1.key',
        'manifest' => $keys.DIRECTORY_SEPARATOR.'domain-audit-keyring-v1.json',
        'pending' => $keys.DIRECTORY_SEPARATOR.'domain-audit-key-rotation-v1.pending.json',
    );
}

/**
 * Load one protected 32-byte key file and verify its non-secret identifier.
 *
 * @param string $path
 * @param string|null $expected_key_id
 * @return string|false
 */
function domain_audit_load_key_file($path, $expected_key_id = null) {
    if (!is_string($path) || $path === '' || is_link($path) || !is_file($path)) {
        return false;
    }
    @chmod($path, 0600);
    $permissions = @fileperms($path);
    if (DIRECTORY_SEPARATOR === '/' && ($permissions === false || ($permissions & 0077) !== 0)) {
        return false;
    }
    $encoded = @file_get_contents($path);
    $key = is_string($encoded) ? base64_decode(trim($encoded), true) : false;
    if (!is_string($key) || strlen($key) !== 32) {
        return false;
    }
    $key_id = domain_audit_key_id($key);
    return $expected_key_id === null || domain_audit_hash_equals((string)$expected_key_id, (string)$key_id)
        ? $key : false;
}

/**
 * Validate and return the keyring manifest, or synthesize generation one for
 * an unrotated legacy installation.
 *
 * @param int|null $company
 * @return array|false
 */
function domain_audit_keyring_manifest($company = null) {
    $company = $company === null && function_exists('user_company') ? user_company() : (int)$company;
    $paths = domain_audit_keyring_paths($company);
    if ($paths === false) {
        return false;
    }
    if (!is_file($paths['manifest'])) {
        $rotation_internal = isset($GLOBALS['domain_audit_key_rotation_company'])
            && (int)$GLOBALS['domain_audit_key_rotation_company'] === (int)$company;
        if (is_file($paths['pending']) && !$rotation_internal) {
            return false;
        }
        $key = domain_audit_load_key_file($paths['legacy_key']);
        $key_id = $key === false ? false : domain_audit_key_id($key);
        if ($key_id === false) {
            return false;
        }
        return array(
            'schema_version' => 'notrinoserp.domain-audit-keyring.v1',
            'company_id' => (int)$company,
            'active_key_id' => $key_id,
            'generation' => 1,
            'keys' => array(array(
                'key_id' => $key_id,
                'generation' => 1,
                'filename' => 'domain-audit-hmac-v1.key',
                'activated_at' => '',
                'retired_at' => '',
            )),
            'legacy_synthesized' => true,
        );
    }
    if (is_link($paths['manifest']) || filesize($paths['manifest']) > 65536) {
        return false;
    }
    @chmod($paths['manifest'], 0600);
    $manifest_permissions = @fileperms($paths['manifest']);
    if (DIRECTORY_SEPARATOR === '/'
        && ($manifest_permissions === false || ($manifest_permissions & 0077) !== 0)
    ) {
        return false;
    }
    $manifest_json = @file_get_contents($paths['manifest']);
    $manifest = is_string($manifest_json) ? json_decode($manifest_json, true) : null;
    if (!is_array($manifest)) {
        return false;
    }
    $expected_fields = array('active_key_id', 'company_id', 'generation', 'keys', 'schema_version');
    $actual_fields = array_keys($manifest);
    sort($expected_fields);
    sort($actual_fields);
    if ($actual_fields !== $expected_fields
        || $manifest['schema_version'] !== 'notrinoserp.domain-audit-keyring.v1'
        || !is_int($manifest['company_id']) || $manifest['company_id'] !== (int)$company
        || !is_int($manifest['generation']) || $manifest['generation'] < 1
        || preg_match('/^[a-f0-9]{16}$/', (string)$manifest['active_key_id']) !== 1
        || !is_array($manifest['keys']) || count($manifest['keys']) !== $manifest['generation']
    ) {
        return false;
    }
    $seen = array();
    foreach ($manifest['keys'] as $index => $entry) {
        $entry_fields = is_array($entry) ? array_keys($entry) : array();
        $expected_entry_fields = array('activated_at', 'filename', 'generation', 'key_id', 'retired_at');
        sort($entry_fields);
        sort($expected_entry_fields);
        if ($entry_fields !== $expected_entry_fields
            || !is_int($entry['generation']) || $entry['generation'] !== $index + 1
            || preg_match('/^[a-f0-9]{16}$/', (string)$entry['key_id']) !== 1
            || isset($seen[$entry['key_id']])
            || preg_match('/^domain-audit-hmac-v1(?:-[a-f0-9]{16})?\.key$/', (string)$entry['filename']) !== 1
            || ($index === 0 && $entry['filename'] !== 'domain-audit-hmac-v1.key')
            || ($index > 0 && $entry['filename'] !== 'domain-audit-hmac-v1-'.$entry['key_id'].'.key')
            || ($entry['activated_at'] !== ''
                && preg_match('/^\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}$/', (string)$entry['activated_at']) !== 1)
            || ($entry['retired_at'] !== ''
                && preg_match('/^\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}$/', (string)$entry['retired_at']) !== 1)
            || ($index > 0 && $entry['activated_at'] === '')
            || ($index < count($manifest['keys']) - 1 && $entry['retired_at'] === '')
        ) {
            return false;
        }
        $seen[$entry['key_id']] = true;
        if (domain_audit_load_key_file(
            $paths['keys'].DIRECTORY_SEPARATOR.$entry['filename'],
            $entry['key_id']
        ) === false) {
            return false;
        }
    }
    for ($index = 0; $index < count($manifest['keys']) - 1; $index++) {
        if ($manifest['keys'][$index]['retired_at']
            !== $manifest['keys'][$index + 1]['activated_at']
        ) {
            return false;
        }
    }
    $last = $manifest['keys'][count($manifest['keys']) - 1];
    if ($last['key_id'] !== $manifest['active_key_id']
        || $last['generation'] !== $manifest['generation']
        || $last['retired_at'] !== ''
    ) {
        return false;
    }
    return $manifest;
}

/**
 * Load a historical or active protected key by its recorded identifier.
 *
 * @param int|null $company
 * @param string $key_id
 * @return string|false
 */
function domain_audit_key_by_id($company, $key_id) {
    if (preg_match('/^[a-f0-9]{16}$/', (string)$key_id) !== 1) {
        return false;
    }
    if (defined('PAYROLL_AUDIT_SIGNING_KEY')) {
        $key = domain_audit_legacy_signing_key($company);
        return $key !== false && domain_audit_hash_equals((string)$key_id, (string)domain_audit_key_id($key))
            ? $key : false;
    }
    $manifest = domain_audit_keyring_manifest($company);
    $paths = domain_audit_keyring_paths($company);
    if ($manifest === false || $paths === false) {
        return false;
    }
    foreach ($manifest['keys'] as $entry) {
        if (domain_audit_hash_equals((string)$entry['key_id'], (string)$key_id)) {
            return domain_audit_load_key_file(
                $paths['keys'].DIRECTORY_SEPARATOR.$entry['filename'],
                $entry['key_id']
            );
        }
    }
    return false;
}

/**
 * Return one validated keyring entry by non-secret key identifier.
 *
 * @param array $manifest
 * @param string $key_id
 * @return array|false
 */
function domain_audit_keyring_entry($manifest, $key_id) {
    if (!is_array($manifest) || !isset($manifest['keys'])
        || !is_array($manifest['keys'])
        || preg_match('/^[a-f0-9]{16}$/', (string)$key_id) !== 1
    ) {
        return false;
    }
    foreach ($manifest['keys'] as $entry) {
        if (is_array($entry)
            && domain_audit_hash_equals(
                (string)$entry['key_id'], (string)$key_id
            )
        ) {
            return $entry;
        }
    }
    return false;
}

/**
 * Check that a signed checkpoint falls inside its key generation's lifetime.
 *
 * The retirement instant is inclusive because the company rotation event is
 * itself signed by the retiring key at that exact second.
 *
 * @param array $entry
 * @param string $created_at
 * @return bool
 */
function domain_audit_checkpoint_key_time_valid($entry, $created_at) {
    if (!is_array($entry)
        || !isset($entry['activated_at'], $entry['retired_at'])
        || preg_match(
            '/^\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}$/',
            (string)$created_at
        ) !== 1
    ) {
        return false;
    }
    return ($entry['activated_at'] === ''
            || strcmp((string)$created_at, (string)$entry['activated_at']) >= 0)
        && ($entry['retired_at'] === ''
            || strcmp((string)$created_at, (string)$entry['retired_at']) <= 0);
}

/**
 * Load the active signing key while failing closed during interrupted rotation.
 *
 * Initial creation remains reserved for the controlled migration.
 *
 * @param int|null $company
 * @param bool $allow_create
 * @return string|false
 */
function domain_audit_signing_key($company = null, $allow_create = false) {
    $company = $company === null && function_exists('user_company') ? user_company() : (int)$company;
    if (defined('PAYROLL_AUDIT_SIGNING_KEY')) {
        return domain_audit_legacy_signing_key($company, $allow_create);
    }
    $paths = domain_audit_keyring_paths($company);
    if ($paths === false) {
        return false;
    }
    $rotation_internal = isset($GLOBALS['domain_audit_key_rotation_company'])
        && (int)$GLOBALS['domain_audit_key_rotation_company'] === (int)$company;
    if (is_file($paths['pending']) && !$rotation_internal) {
        return false;
    }
    if (!is_file($paths['legacy_key']) && $allow_create && !is_file($paths['manifest'])) {
        $created = domain_audit_legacy_signing_key($company, true);
        if ($created === false) {
            return false;
        }
    }
    $manifest = domain_audit_keyring_manifest($company);
    return $manifest === false ? false
        : domain_audit_key_by_id($company, $manifest['active_key_id']);
}

/**
 * Verify a checkpoint with the historical key identified in the row.
 *
 * @param array $checkpoint
 * @param int|null $company
 * @return bool
 */
function domain_audit_verify_checkpoint_with_keyring($checkpoint, $company = null) {
    if (!is_array($checkpoint) || !isset($checkpoint['key_id'])) {
        return false;
    }
    $key = domain_audit_key_by_id($company, (string)$checkpoint['key_id']);
    return $key !== false && domain_audit_verify_checkpoint($checkpoint, $key);
}

/**
 * Hash a sensitive value using a purpose-separated HMAC key.
 *
 * @param mixed $value
 * @param string $key
 * @param string $purpose
 * @return string|false
 */
function domain_audit_sensitive_hash($value, $key, $purpose) {
    $json = domain_audit_canonical_json($value);
    $purpose_key = domain_audit_derived_key($key, $purpose);
    if ($json === false || $purpose_key === false) {
        return false;
    }
    return hash_hmac('sha256', $json, $purpose_key);
}

/**
 * Validate a stable lower-case audit token.
 *
 * @param string $value
 * @return bool
 */
function domain_audit_valid_token($value) {
    return is_string($value)
        && preg_match('/^[a-z][a-z0-9_.:-]{1,63}$/', $value) === 1;
}

/**
 * Build a deterministic event row before database insertion.
 *
 * @param array $event
 * @param string $previous_hash
 * @param int $sequence
 * @param string $key
 * @return array|false
 */
function domain_audit_build_event_record($event, $previous_hash, $sequence, $key) {
    if (!is_array($event)) {
        return false;
    }
    $required = array('company_id', 'entity_type', 'entity_key', 'action', 'reason_code');
    foreach ($required as $name) {
        if (!array_key_exists($name, $event)) {
            return false;
        }
    }
    if (!domain_audit_valid_token($event['entity_type'])
        || !domain_audit_valid_token($event['action'])
        || !domain_audit_valid_token($event['reason_code'])
        || !preg_match('/^[a-f0-9]{64}$/', (string)$previous_hash)
        || (int)$sequence < 1
        || !is_string($key) || strlen($key) !== 32
        || $event['entity_key'] === null || $event['entity_key'] === '' || $event['entity_key'] === array()
    ) {
        return false;
    }
    $company = (int)$event['company_id'];
    $legal_entity = isset($event['legal_entity_id']) ? (int)$event['legal_entity_id'] : 0;
    if ($company < 0 || $legal_entity < 0) {
        return false;
    }
    $chain_scope = 'company:'.$company.':legal_entity:'.$legal_entity;
    $correlation_id = isset($event['correlation_id']) ? strtolower((string)$event['correlation_id']) : domain_audit_correlation_id();
    if (!is_string($correlation_id) || preg_match('/^[a-f0-9]{32}$/', $correlation_id) !== 1) {
        return false;
    }
    if (isset($event['security_context']) && !is_array($event['security_context'])) {
        return false;
    }
    $security_context = isset($event['security_context'])
        ? $event['security_context'] : domain_audit_current_security_context($company);
    $actor_user_id = isset($security_context['actor_user_id']) ? (int)$security_context['actor_user_id'] : 0;
    if ($actor_user_id < 0 || (isset($event['legacy_id']) && (int)$event['legacy_id'] < 0)) {
        return false;
    }
    $security_context_hash = domain_audit_sensitive_hash($security_context, $key, 'security-context-v1');
    $entity_key_hash = domain_audit_sensitive_hash($event['entity_key'], $key, 'entity-key-v1');
    $before_hash = array_key_exists('before', $event) && $event['before'] !== null
        ? domain_audit_sensitive_hash($event['before'], $key, 'restricted-before-v1') : null;
    $after_hash = array_key_exists('after', $event) && $event['after'] !== null
        ? domain_audit_sensitive_hash($event['after'], $key, 'restricted-after-v1') : null;
    if (isset($event['restricted_diff']) && !is_array($event['restricted_diff'])) {
        return false;
    }
    $restricted_diff = isset($event['restricted_diff']) ? $event['restricted_diff'] : array();
    $restricted_diff_json = domain_audit_canonical_json($restricted_diff);
    if ($security_context_hash === false || $entity_key_hash === false
        || (array_key_exists('before', $event) && $event['before'] !== null && $before_hash === false)
        || (array_key_exists('after', $event) && $event['after'] !== null && $after_hash === false)
        || $restricted_diff_json === false || strlen($restricted_diff_json) > 8192
    ) {
        return false;
    }
    $occurred_at = isset($event['occurred_at']) ? (string)$event['occurred_at'] : gmdate('Y-m-d H:i:s');
    if (preg_match('/^\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}$/', $occurred_at) !== 1) {
        return false;
    }
    $legacy_source = isset($event['legacy_source']) ? (string)$event['legacy_source'] : '';
    if ($legacy_source !== '' && !domain_audit_valid_token($legacy_source)) {
        return false;
    }
    $record = array(
        'chain_scope' => $chain_scope,
        'chain_sequence' => (int)$sequence,
        'occurred_at' => $occurred_at,
        'actor_user_id' => $actor_user_id,
        'security_context_hash' => $security_context_hash,
        'company_id' => $company,
        'legal_entity_id' => $legal_entity,
        'entity_type' => (string)$event['entity_type'],
        'entity_key_hash' => $entity_key_hash,
        'action' => (string)$event['action'],
        'reason_code' => (string)$event['reason_code'],
        'correlation_id' => $correlation_id,
        'legacy_source' => $legacy_source,
        'legacy_id' => isset($event['legacy_id']) ? (int)$event['legacy_id'] : 0,
        'before_hash' => $before_hash,
        'after_hash' => $after_hash,
        'restricted_diff' => $restricted_diff_json,
        'previous_event_hash' => (string)$previous_hash,
        'key_id' => domain_audit_key_id($key),
    );
    $payload = domain_audit_canonical_json($record);
    if ($payload === false) {
        return false;
    }
    $record['event_hash'] = hash('sha256', $payload);
    return $record;
}

/**
 * Return the exact payload used for event-hash verification.
 *
 * @param array $row
 * @return array
 */
function domain_audit_event_hash_fields($row) {
    $fields = array(
        'chain_scope', 'chain_sequence', 'occurred_at', 'actor_user_id',
        'security_context_hash', 'company_id', 'legal_entity_id', 'entity_type',
        'entity_key_hash', 'action', 'reason_code', 'correlation_id',
        'legacy_source', 'legacy_id', 'before_hash', 'after_hash',
        'restricted_diff', 'previous_event_hash', 'key_id'
    );
    $payload = array();
    foreach ($fields as $field) {
        $payload[$field] = array_key_exists($field, $row) ? $row[$field] : null;
    }
    $payload['chain_sequence'] = (int)$payload['chain_sequence'];
    $payload['actor_user_id'] = (int)$payload['actor_user_id'];
    $payload['company_id'] = (int)$payload['company_id'];
    $payload['legal_entity_id'] = (int)$payload['legal_entity_id'];
    $payload['legacy_id'] = (int)$payload['legacy_id'];
    return $payload;
}

/**
 * Verify an ordered set of event rows without database access.
 *
 * @param array $rows
 * @return array
 */
function domain_audit_verify_event_rows($rows) {
    $expected_sequence = 1;
    $previous_hash = str_repeat('0', 64);
    foreach ($rows as $row) {
        if ((int)$row['chain_sequence'] !== $expected_sequence
            || (string)$row['previous_event_hash'] !== $previous_hash
        ) {
            return array('ok' => false, 'error' => 'chain_gap', 'sequence' => $expected_sequence);
        }
        $payload = domain_audit_canonical_json(domain_audit_event_hash_fields($row));
        $computed = $payload === false ? '' : hash('sha256', $payload);
        if (!domain_audit_hash_equals((string)$row['event_hash'], $computed)) {
            return array('ok' => false, 'error' => 'event_hash_mismatch', 'sequence' => $expected_sequence);
        }
        $previous_hash = (string)$row['event_hash'];
        $expected_sequence++;
    }
    return array(
        'ok' => true,
        'count' => count($rows),
        'last_sequence' => $expected_sequence - 1,
        'last_event_hash' => $previous_hash,
    );
}

/**
 * Build and sign a checkpoint row.
 *
 * @param string $chain_scope
 * @param int $sequence
 * @param string $event_hash
 * @param int $event_count
 * @param string $key
 * @param string|null $created_at
 * @return array|false
 */
function domain_audit_build_checkpoint($chain_scope, $sequence, $event_hash, $event_count, $key, $created_at = null) {
    $sequence = (int)$sequence;
    $event_count = (int)$event_count;
    $key_id = domain_audit_key_id($key);
    $signing_key = domain_audit_derived_key($key, 'checkpoint-signature-v1');
    if (!preg_match('/^company:\d+:legal_entity:\d+$/', (string)$chain_scope)
        || !preg_match('/^[a-f0-9]{64}$/', (string)$event_hash)
        || $sequence < 0 || $event_count !== $sequence
        || $key_id === false || $signing_key === false
    ) {
        return false;
    }
    $record = array(
        'chain_scope' => (string)$chain_scope,
        'chain_sequence' => $sequence,
        'event_hash' => (string)$event_hash,
        'event_count' => $event_count,
        'key_id' => $key_id,
        'created_at' => $created_at === null ? gmdate('Y-m-d H:i:s') : (string)$created_at,
    );
    if (preg_match('/^\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}$/', $record['created_at']) !== 1) {
        return false;
    }
    $payload = domain_audit_canonical_json($record);
    if ($payload === false) {
        return false;
    }
    $record['signature'] = hash_hmac('sha256', $payload, $signing_key);
    return $record;
}

/**
 * Verify a checkpoint signature and active key identity.
 *
 * @param array $checkpoint
 * @param string $key
 * @return bool
 */
function domain_audit_verify_checkpoint($checkpoint, $key) {
    if (!is_array($checkpoint)) {
        return false;
    }
    $required = array('chain_scope', 'chain_sequence', 'event_hash', 'event_count', 'key_id', 'created_at', 'signature');
    foreach ($required as $field) {
        if (!array_key_exists($field, $checkpoint)) {
            return false;
        }
    }
    $key_id = domain_audit_key_id($key);
    $signing_key = domain_audit_derived_key($key, 'checkpoint-signature-v1');
    if ($key_id === false || $signing_key === false
        || !preg_match('/^company:\d+:legal_entity:\d+$/', (string)$checkpoint['chain_scope'])
        || (int)$checkpoint['chain_sequence'] < 0
        || !preg_match('/^[a-f0-9]{64}$/', (string)$checkpoint['event_hash'])
        || preg_match('/^\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}$/', (string)$checkpoint['created_at']) !== 1
        || !domain_audit_hash_equals((string)$checkpoint['key_id'], $key_id)
        || !preg_match('/^[a-f0-9]{64}$/', (string)$checkpoint['signature'])
    ) {
        return false;
    }
    $payload = array(
        'chain_scope' => (string)$checkpoint['chain_scope'],
        'chain_sequence' => (int)$checkpoint['chain_sequence'],
        'event_hash' => (string)$checkpoint['event_hash'],
        'event_count' => (int)$checkpoint['event_count'],
        'key_id' => (string)$checkpoint['key_id'],
        'created_at' => (string)$checkpoint['created_at'],
    );
    if ($payload['event_count'] !== $payload['chain_sequence']) {
        return false;
    }
    $json = domain_audit_canonical_json($payload);
    return $json !== false && domain_audit_hash_equals(
        (string)$checkpoint['signature'],
        hash_hmac('sha256', $json, $signing_key)
    );
}

/**
 * Write a bounded private JSON control file through an exclusive temporary file.
 *
 * @param string $path
 * @param array $record
 * @param bool $replace
 * @return bool
 */
function domain_audit_write_private_json($path, $record, $replace = false) {
    if (!is_string($path) || $path === '' || !is_array($record)) {
        return false;
    }
    $json = domain_audit_canonical_json($record);
    if ($json === false || strlen($json) > 65536 || is_link(dirname($path))) {
        return false;
    }
    if (!$replace && file_exists($path)) {
        return false;
    }
    $temporary = $path.'.new-'.getmypid().'-'.substr(hash('sha256', $json.microtime(true)), 0, 12);
    $handle = @fopen($temporary, 'x+b');
    if ($handle === false) {
        return false;
    }
    $payload = $json."\n";
    $written = fwrite($handle, $payload);
    $flushed = fflush($handle);
    fclose($handle);
    @chmod($temporary, 0600);
    $permissions = @fileperms($temporary);
    if ($written !== strlen($payload) || !$flushed
        || (DIRECTORY_SEPARATOR === '/' && ($permissions === false || ($permissions & 0077) !== 0))
    ) {
        @unlink($temporary);
        return false;
    }
    $decoded = json_decode((string)@file_get_contents($temporary), true);
    if (!is_array($decoded) || domain_audit_canonical_json($decoded) !== $json
        || (!$replace && file_exists($path)) || !@rename($temporary, $path)
    ) {
        @unlink($temporary);
        return false;
    }
    @chmod($path, 0600);
    return true;
}

/**
 * Write one newly generated protected key without overwriting an existing key.
 *
 * @param string $path
 * @param string $key
 * @return bool
 */
function domain_audit_write_key_file($path, $key) {
    if (!is_string($key) || strlen($key) !== 32 || file_exists($path)) {
        return false;
    }
    $handle = @fopen($path, 'x+b');
    if ($handle === false) {
        return false;
    }
    $payload = base64_encode($key)."\n";
    $written = fwrite($handle, $payload);
    $flushed = fflush($handle);
    fclose($handle);
    @chmod($path, 0600);
    $permissions = @fileperms($path);
    if ($written !== strlen($payload) || !$flushed
        || (DIRECTORY_SEPARATOR === '/' && ($permissions === false || ($permissions & 0077) !== 0))
        || domain_audit_load_key_file($path, domain_audit_key_id($key)) === false
    ) {
        @unlink($path);
        return false;
    }
    return true;
}

/**
 * Acquire the company key-selection mutex on the active database connection.
 *
 * The mutex is re-entrant within one PHP request so the rotation coordinator
 * can call the normal append path without creating a parallel writer.
 *
 * @param int $company
 * @return bool
 */
function domain_audit_acquire_key_lock($company) {
    $company = (int)$company;
    if (!isset($GLOBALS['domain_audit_key_lock_depth'])) {
        $GLOBALS['domain_audit_key_lock_depth'] = array();
    }
    if (!empty($GLOBALS['domain_audit_key_lock_depth'][$company])) {
        $GLOBALS['domain_audit_key_lock_depth'][$company]++;
        return true;
    }
    $name = 'notrinoserp:pay-aud:key:'.substr(hash('sha256', (string)$company), 0, 32);
    $result = db_query(
        'SELECT GET_LOCK('.domain_audit_db_string($name).',10) acquired',
        'could not acquire PAY-AUD key-selection lock'
    );
    $row = $result ? db_fetch_assoc($result) : false;
    if (!$row || (int)$row['acquired'] !== 1) {
        return false;
    }
    $GLOBALS['domain_audit_key_lock_depth'][$company] = 1;
    $GLOBALS['domain_audit_key_lock_name'][$company] = $name;
    return true;
}

/**
 * Release the company key-selection mutex.
 *
 * @param int $company
 * @return bool
 */
function domain_audit_release_key_lock($company) {
    $company = (int)$company;
    if (empty($GLOBALS['domain_audit_key_lock_depth'][$company])) {
        return false;
    }
    $GLOBALS['domain_audit_key_lock_depth'][$company]--;
    if ($GLOBALS['domain_audit_key_lock_depth'][$company] > 0) {
        return true;
    }
    $name = isset($GLOBALS['domain_audit_key_lock_name'][$company])
        ? $GLOBALS['domain_audit_key_lock_name'][$company] : '';
    unset($GLOBALS['domain_audit_key_lock_depth'][$company]);
    unset($GLOBALS['domain_audit_key_lock_name'][$company]);
    $result = $name === '' ? false : db_query(
        'SELECT RELEASE_LOCK('.domain_audit_db_string($name).') released',
        'could not release PAY-AUD key-selection lock'
    );
    $row = $result ? db_fetch_assoc($result) : false;
    return $row && (int)$row['released'] === 1;
}

/**
 * Return the authenticated operating-system identity for key administration.
 *
 * @return string|false
 */
function domain_audit_key_admin_os_identity() {
    if (function_exists('posix_geteuid') && function_exists('posix_getpwuid')) {
        $account = @posix_getpwuid(posix_geteuid());
        return is_array($account) && !empty($account['name']) ? (string)$account['name'] : false;
    }
    $user = getenv('USERNAME');
    $domain = getenv('USERDOMAIN');
    if (!is_string($user) || $user === '') {
        return false;
    }
    return is_string($domain) && $domain !== '' ? $domain.'\\'.$user : $user;
}

/**
 * Authorize the CLI key ceremony against the configured dedicated OS identity.
 *
 * @param int $company
 * @return string|false Authorized identity.
 */
function domain_audit_key_rotation_authorized($company) {
    global $db_connections;

    if (PHP_SAPI !== 'cli') {
        return false;
    }
    $expected = defined('PAYROLL_AUDIT_KEY_ADMIN_OS_USER')
        ? (string)PAYROLL_AUDIT_KEY_ADMIN_OS_USER
        : (isset($db_connections[$company]['payroll_audit_key_admin_os_user'])
            ? (string)$db_connections[$company]['payroll_audit_key_admin_os_user'] : '');
    $actual = domain_audit_key_admin_os_identity();
    if ($expected === '' || strlen($expected) > 128
        || preg_match('/^[A-Za-z0-9_.@\\\\-]+$/', $expected) !== 1
        || $actual === false || !domain_audit_hash_equals(strtolower($expected), strtolower($actual))
    ) {
        return false;
    }
    return $actual;
}

/**
 * Load and strictly validate an interrupted-rotation journal.
 *
 * @param int $company
 * @return array|false
 */
function domain_audit_key_rotation_pending($company) {
    $paths = domain_audit_keyring_paths($company);
    if ($paths === false || !is_file($paths['pending']) || is_link($paths['pending'])
        || filesize($paths['pending']) > 65536
    ) {
        return false;
    }
    @chmod($paths['pending'], 0600);
    $pending_permissions = @fileperms($paths['pending']);
    if (DIRECTORY_SEPARATOR === '/'
        && ($pending_permissions === false || ($pending_permissions & 0077) !== 0)
    ) {
        return false;
    }
    $json = @file_get_contents($paths['pending']);
    $pending = is_string($json) ? json_decode($json, true) : null;
    $expected = array(
        'actor', 'company_id', 'correlation_id', 'filename', 'generation',
        'new_key_id', 'occurred_at', 'previous_key_id', 'reason_code', 'schema_version'
    );
    $actual = is_array($pending) ? array_keys($pending) : array();
    sort($expected);
    sort($actual);
    if ($actual !== $expected
        || $pending['schema_version'] !== 'notrinoserp.domain-audit-key-rotation-pending.v1'
        || !is_int($pending['company_id']) || $pending['company_id'] !== (int)$company
        || !is_int($pending['generation']) || $pending['generation'] < 2
        || preg_match('/^[a-f0-9]{16}$/', (string)$pending['previous_key_id']) !== 1
        || preg_match('/^[a-f0-9]{16}$/', (string)$pending['new_key_id']) !== 1
        || $pending['previous_key_id'] === $pending['new_key_id']
        || $pending['filename'] !== 'domain-audit-hmac-v1-'.$pending['new_key_id'].'.key'
        || preg_match('/^[a-f0-9]{32}$/', (string)$pending['correlation_id']) !== 1
        || !domain_audit_valid_token($pending['reason_code'])
        || preg_match('/^\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}$/', (string)$pending['occurred_at']) !== 1
        || !is_string($pending['actor']) || $pending['actor'] === '' || strlen($pending['actor']) > 128
    ) {
        return false;
    }
    return $pending;
}

/**
 * Return a persisted manifest record without synthesized metadata.
 *
 * @param array $manifest
 * @return array
 */
function domain_audit_keyring_persisted_record($manifest) {
    return array(
        'schema_version' => 'notrinoserp.domain-audit-keyring.v1',
        'company_id' => (int)$manifest['company_id'],
        'active_key_id' => (string)$manifest['active_key_id'],
        'generation' => (int)$manifest['generation'],
        'keys' => $manifest['keys'],
    );
}

/**
 * Build the next manifest after a verified rotation event.
 *
 * @param array $manifest
 * @param array $pending
 * @return array|false
 */
function domain_audit_keyring_after_rotation($manifest, $pending) {
    if (!is_array($manifest) || !is_array($pending)
        || (int)$manifest['generation'] + 1 !== (int)$pending['generation']
        || !domain_audit_hash_equals(
            (string)$manifest['active_key_id'],
            (string)$pending['previous_key_id']
        )
    ) {
        return false;
    }
    $keys = $manifest['keys'];
    $last = count($keys) - 1;
    $keys[$last]['retired_at'] = (string)$pending['occurred_at'];
    $keys[] = array(
        'key_id' => (string)$pending['new_key_id'],
        'generation' => (int)$pending['generation'],
        'filename' => (string)$pending['filename'],
        'activated_at' => (string)$pending['occurred_at'],
        'retired_at' => '',
    );
    return array(
        'schema_version' => 'notrinoserp.domain-audit-keyring.v1',
        'company_id' => (int)$pending['company_id'],
        'active_key_id' => (string)$pending['new_key_id'],
        'generation' => (int)$pending['generation'],
        'keys' => $keys,
    );
}

/**
 * Inert protected checkpoint used only by configured failure-injection tests.
 *
 * @param string $step
 * @return bool
 */
function domain_audit_key_rotation_checkpoint($step) {
    return !defined('PAYROLL_AUDIT_KEY_ROTATION_TESTING')
        || PAYROLL_AUDIT_KEY_ROTATION_TESTING !== true
        || !isset($GLOBALS['domain_audit_key_rotation_test_checkpoint'])
        || !is_callable($GLOBALS['domain_audit_key_rotation_test_checkpoint'])
        || call_user_func($GLOBALS['domain_audit_key_rotation_test_checkpoint'], (string)$step);
}

/**
 * Find and validate the committed rotation event for one pending journal.
 *
 * @param array $pending
 * @return array|false|null False is mismatch; null is not committed.
 */
function domain_audit_committed_key_rotation($pending) {
    $result = db_query(
        "SELECT event_id,chain_sequence,occurred_at,action,reason_code,correlation_id,"
        ."restricted_diff,key_id FROM ".TB_PREF."domain_audit_events"
        ." WHERE chain_scope=".domain_audit_db_string('company:'.(int)$pending['company_id'].':legal_entity:0')
        ." AND correlation_id=".domain_audit_db_string($pending['correlation_id'])
        ." ORDER BY event_id LIMIT 2",
        'could not inspect PAY-AUD key-rotation evidence'
    );
    if (!$result) {
        return false;
    }
    $rows = array();
    while ($row = db_fetch_assoc($result)) {
        $rows[] = $row;
    }
    if (count($rows) === 0) {
        return null;
    }
    if (count($rows) !== 1) {
        return false;
    }
    $row = $rows[0];
    $diff = json_decode((string)$row['restricted_diff'], true);
    if ((string)$row['action'] !== 'audit.key.rotated'
        || (string)$row['reason_code'] !== (string)$pending['reason_code']
        || (string)$row['occurred_at'] !== (string)$pending['occurred_at']
        || (string)$row['key_id'] !== (string)$pending['previous_key_id']
        || !is_array($diff)
        || $diff !== array(
            'generation' => (int)$pending['generation'],
            'new_key_id' => (string)$pending['new_key_id'],
            'previous_key_id' => (string)$pending['previous_key_id'],
            'restricted_values_stored' => false,
        )
    ) {
        return false;
    }
    return $row;
}

/**
 * Rotate the company audit key and anchor the transition in the existing chain.
 *
 * @param int $company
 * @param string $reason_code
 * @return array|false
 */
function rotate_domain_audit_key($company, $reason_code) {
    $GLOBALS['domain_audit_key_rotation_result'] = 'validation_started';
    $company = (int)$company;
    $actor = domain_audit_key_rotation_authorized($company);
    if ($company < 0 || !domain_audit_valid_token($reason_code)
        || defined('PAYROLL_AUDIT_SIGNING_KEY')
    ) {
        $GLOBALS['domain_audit_key_rotation_result'] = 'validation_failed';
        return false;
    }
    if ($actor === false) {
        $GLOBALS['domain_audit_key_rotation_result'] = 'authorization_failed';
        return false;
    }
    if (!domain_audit_acquire_key_lock($company)) {
        $GLOBALS['domain_audit_key_rotation_result'] = 'key_lock_failed';
        return false;
    }
    $paths = domain_audit_keyring_paths($company);
    $manifest = domain_audit_keyring_manifest($company);
    $verification = $manifest === false ? false : verify_domain_audit_chain($company, 0);
    if ($paths === false || $manifest === false || empty($verification['ok'])
        || is_file($paths['pending']) || !function_exists('openssl_random_pseudo_bytes')
    ) {
        $GLOBALS['domain_audit_key_rotation_result'] = 'prerequisite_verification_failed';
        domain_audit_release_key_lock($company);
        return false;
    }
    $strong = false;
    $new_key = openssl_random_pseudo_bytes(32, $strong);
    $new_key_id = is_string($new_key) && strlen($new_key) === 32 && $strong
        ? domain_audit_key_id($new_key) : false;
    $duplicate_key = false;
    foreach ($manifest['keys'] as $existing_key) {
        if ($new_key_id !== false
            && domain_audit_hash_equals((string)$existing_key['key_id'], (string)$new_key_id)
        ) {
            $duplicate_key = true;
        }
    }
    if ($new_key_id === false || $duplicate_key) {
        $GLOBALS['domain_audit_key_rotation_result'] = 'key_generation_failed';
        domain_audit_release_key_lock($company);
        return false;
    }
    $filename = 'domain-audit-hmac-v1-'.$new_key_id.'.key';
    $new_key_path = $paths['keys'].DIRECTORY_SEPARATOR.$filename;
    if (!domain_audit_write_key_file($new_key_path, $new_key)) {
        $GLOBALS['domain_audit_key_rotation_result'] = 'key_write_failed';
        domain_audit_release_key_lock($company);
        return false;
    }
    $pending = array(
        'schema_version' => 'notrinoserp.domain-audit-key-rotation-pending.v1',
        'company_id' => $company,
        'previous_key_id' => (string)$manifest['active_key_id'],
        'new_key_id' => $new_key_id,
        'generation' => (int)$manifest['generation'] + 1,
        'filename' => $filename,
        'occurred_at' => gmdate('Y-m-d H:i:s'),
        'correlation_id' => domain_audit_correlation_id(),
        'reason_code' => (string)$reason_code,
        'actor' => (string)$actor,
    );
    if ($pending['correlation_id'] === false
        || !domain_audit_write_private_json($paths['pending'], $pending)
    ) {
        $GLOBALS['domain_audit_key_rotation_result'] = 'pending_journal_failed';
        @unlink($new_key_path);
        domain_audit_release_key_lock($company);
        return false;
    }
    $GLOBALS['domain_audit_key_rotation_company'] = $company;
    $appended = append_domain_audit_event(array(
        'company_id' => $company,
        'legal_entity_id' => 0,
        'entity_type' => 'domain_audit_key',
        'entity_key' => array('key_id' => $new_key_id),
        'action' => 'audit.key.rotated',
        'reason_code' => (string)$reason_code,
        'correlation_id' => $pending['correlation_id'],
        'occurred_at' => $pending['occurred_at'],
        'security_context' => array(
            'actor_user_id' => 0,
            'company_id' => $company,
            'role_id' => 0,
            'role_set' => array(),
            'auth_method' => 'key_rotation_cli',
            'os_actor' => (string)$actor,
        ),
        'restricted_diff' => array(
            'generation' => (int)$pending['generation'],
            'new_key_id' => $new_key_id,
            'previous_key_id' => (string)$manifest['active_key_id'],
            'restricted_values_stored' => false,
        ),
    ));
    unset($GLOBALS['domain_audit_key_rotation_company']);
    if ($appended === false) {
        $GLOBALS['domain_audit_key_rotation_result'] = 'event_append_failed';
        @unlink($paths['pending']);
        @unlink($new_key_path);
        domain_audit_release_key_lock($company);
        return false;
    }
    $next_manifest = domain_audit_keyring_after_rotation($manifest, $pending);
    if (!domain_audit_key_rotation_checkpoint('event_committed')
        || $next_manifest === false
    ) {
        $GLOBALS['domain_audit_key_rotation_result'] = 'manifest_activation_pending';
        domain_audit_release_key_lock($company);
        return false;
    }
    if (!domain_audit_write_private_json($paths['manifest'], $next_manifest, is_file($paths['manifest']))) {
        $GLOBALS['domain_audit_key_rotation_result'] = 'manifest_activation_pending';
        domain_audit_release_key_lock($company);
        return false;
    }
    if (!domain_audit_key_rotation_checkpoint('manifest_activated')) {
        $GLOBALS['domain_audit_key_rotation_result'] = 'manifest_activated_pending_cleanup';
        domain_audit_release_key_lock($company);
        return false;
    }
    if (!@unlink($paths['pending'])) {
        $GLOBALS['domain_audit_key_rotation_result'] = 'manifest_activated_pending_cleanup';
        domain_audit_release_key_lock($company);
        return false;
    }
    domain_audit_release_key_lock($company);
    $GLOBALS['domain_audit_key_rotation_result'] = 'rotated';
    return array(
        'company_id' => $company,
        'generation' => (int)$next_manifest['generation'],
        'previous_key_id' => (string)$pending['previous_key_id'],
        'active_key_id' => (string)$pending['new_key_id'],
        'event_id' => (int)$appended['event_id'],
        'chain_sequence' => (int)$appended['chain_sequence'],
        'correlation_id' => (string)$pending['correlation_id'],
    );
}

/**
 * Recover an interrupted rotation without rewriting committed evidence.
 *
 * @param int $company
 * @return array|false
 */
function recover_domain_audit_key_rotation($company) {
    $company = (int)$company;
    $actor = domain_audit_key_rotation_authorized($company);
    if ($actor === false || defined('PAYROLL_AUDIT_SIGNING_KEY')
        || !domain_audit_acquire_key_lock($company)
    ) {
        return false;
    }
    $paths = domain_audit_keyring_paths($company);
    $pending = domain_audit_key_rotation_pending($company);
    if ($paths === false || $pending === false) {
        domain_audit_release_key_lock($company);
        return false;
    }
    $new_key_path = $paths['keys'].DIRECTORY_SEPARATOR.$pending['filename'];
    $committed = domain_audit_committed_key_rotation($pending);
    if ($committed === false) {
        domain_audit_release_key_lock($company);
        return false;
    }
    if ($committed === null) {
        $new_key_valid = !file_exists($new_key_path)
            || domain_audit_load_key_file($new_key_path, $pending['new_key_id']) !== false;
        $removed_key = $new_key_valid
            && (!file_exists($new_key_path) || @unlink($new_key_path));
        $removed_pending = $removed_key && @unlink($paths['pending']);
        domain_audit_release_key_lock($company);
        return $removed_pending ? array('ok' => true, 'recovery' => 'aborted_uncommitted') : false;
    }
    if (domain_audit_load_key_file($new_key_path, $pending['new_key_id']) === false) {
        domain_audit_release_key_lock($company);
        return false;
    }
    $manifest = domain_audit_keyring_manifest($company);
    if ($manifest === false) {
        /*
         * The pending journal intentionally makes the normal loader fail closed.
         * Load the pre-rotation manifest directly by temporarily admitting only
         * this recovery request.
         */
        $GLOBALS['domain_audit_key_rotation_company'] = $company;
        $manifest = domain_audit_keyring_manifest($company);
        unset($GLOBALS['domain_audit_key_rotation_company']);
    }
    if ($manifest === false) {
        domain_audit_release_key_lock($company);
        return false;
    }
    $already_activated = (int)$manifest['generation'] === (int)$pending['generation']
        && domain_audit_hash_equals(
            (string)$manifest['active_key_id'],
            (string)$pending['new_key_id']
        );
    $next_manifest = $already_activated
        ? domain_audit_keyring_persisted_record($manifest)
        : domain_audit_keyring_after_rotation($manifest, $pending);
    if ($next_manifest === false
        || (!$already_activated
            && !domain_audit_write_private_json($paths['manifest'], $next_manifest, is_file($paths['manifest'])))
        || !@unlink($paths['pending'])
    ) {
        domain_audit_release_key_lock($company);
        return false;
    }
    domain_audit_release_key_lock($company);
    return array(
        'ok' => true,
        'recovery' => $already_activated ? 'finalized_pending_cleanup' : 'finalized_committed',
        'generation' => (int)$next_manifest['generation'],
        'active_key_id' => (string)$next_manifest['active_key_id'],
        'chain_sequence' => (int)$committed['chain_sequence'],
    );
}

/**
 * Ensure and verify the signed zero-event checkpoint for one locked chain.
 *
 * The caller must hold the chain_state row lock when events can be appended.
 * A missing genesis checkpoint is created only while the chain is still empty.
 *
 * @param string $chain_scope
 * @param string $key
 * @param bool $allow_create
 * @return array|false
 */
function ensure_domain_audit_genesis_checkpoint($chain_scope, $key, $allow_create = true) {
    $result = db_query("SELECT * FROM ".TB_PREF."domain_audit_checkpoints"
        ." WHERE chain_scope=".domain_audit_db_string($chain_scope)." AND chain_sequence=0 LIMIT 1",
        'could not inspect domain audit genesis checkpoint');
    $checkpoint = $result ? db_fetch_assoc($result) : false;
    if (!$checkpoint && $allow_create) {
        $record = domain_audit_build_checkpoint($chain_scope, 0, str_repeat('0', 64), 0, $key);
        if ($record === false) {
            return false;
        }
        $sql = "INSERT IGNORE INTO ".TB_PREF."domain_audit_checkpoints"
            ." (chain_scope, chain_sequence, event_hash, event_count, key_id, signature, created_at) VALUES ("
            .domain_audit_db_string($record['chain_scope']).",0,"
            .domain_audit_db_string($record['event_hash']).",0,"
            .domain_audit_db_string($record['key_id']).","
            .domain_audit_db_string($record['signature']).","
            .domain_audit_db_string($record['created_at']).")";
        if (!db_query($sql, 'could not append domain audit genesis checkpoint')) {
            return false;
        }
        $result = db_query("SELECT * FROM ".TB_PREF."domain_audit_checkpoints"
            ." WHERE chain_scope=".domain_audit_db_string($chain_scope)." AND chain_sequence=0 LIMIT 1",
            'could not reload domain audit genesis checkpoint');
        $checkpoint = $result ? db_fetch_assoc($result) : false;
    }
    if (!$checkpoint
        || (string)$checkpoint['chain_scope'] !== (string)$chain_scope
        || (int)$checkpoint['chain_sequence'] !== 0
        || (int)$checkpoint['event_count'] !== 0
        || (string)$checkpoint['event_hash'] !== str_repeat('0', 64)
        || !domain_audit_verify_checkpoint($checkpoint, $key)
    ) {
        return false;
    }
    return $checkpoint;
}

/**
 * Convert one fixed gateway argument to a safe SQL literal.
 *
 * @param mixed $value
 * @param bool $integer
 * @return string
 */
function domain_audit_gateway_sql_value($value, $integer = false) {
    if ($value === null) {
        return 'NULL';
    }
    return $integer ? (string)(int)$value : domain_audit_db_string($value);
}

/**
 * Drain stored-procedure protocol results after a gateway write.
 *
 * PDO closes non-result statements in db_query(). Mysqli requires explicit
 * draining before another statement can be issued on the same transaction.
 *
 * @return bool
 */
function domain_audit_gateway_drain_results() {
    global $db;

    if (function_exists('mysqli_more_results') && class_exists('mysqli', false)
        && $db instanceof mysqli
    ) {
        while (mysqli_more_results($db)) {
            if (!mysqli_next_result($db)) {
                return false;
            }
            $result = mysqli_store_result($db);
            if ($result instanceof mysqli_result) {
                mysqli_free_result($result);
            }
        }
        return mysqli_errno($db) === 0;
    }
    return true;
}

/**
 * Execute one fixed SQL SECURITY DEFINER audit gateway routine.
 *
 * @param string $routine Fixed allowlisted routine suffix.
 * @param array $arguments Already formatted SQL literals.
 * @param string $error_message
 * @param array $allowed_affected_rows Exact successful row counts.
 * @return bool
 */
function domain_audit_gateway_call(
    $routine,
    $arguments,
    $error_message,
    $allowed_affected_rows = array(1)
) {
    $allowed = array(
        'domain_audit_chain_initialize_v1',
        'domain_audit_event_insert_v1',
        'domain_audit_outbox_insert_v1',
        'domain_audit_checkpoint_insert_v1',
        'domain_audit_chain_advance_v1',
    );
    if (!in_array($routine, $allowed, true) || !is_array($arguments)) {
        return false;
    }
    $sql = "CALL `".TB_PREF.$routine."`(".implode(',', $arguments).")";
    $result = db_query($sql);
    $affected_rows = $result !== false && function_exists('db_num_affected_rows')
        ? (int)db_num_affected_rows() : -1;
    $drained = $result !== false && domain_audit_gateway_drain_results();
    if (!$drained || !in_array($affected_rows, $allowed_affected_rows, true)) {
        $error_code = function_exists('db_error_no') ? (int)db_error_no() : 0;
        $GLOBALS['domain_audit_gateway_result'] = $routine.'_failed_'.$error_code;
        error_log('PAY-AUD-001 '.$error_message.' ['.$routine.':'.$error_code.']');
        return false;
    }
    $GLOBALS['domain_audit_gateway_result'] = $routine.'_ok';
    return true;
}

/**
 * Initialize and lock one chain through the transaction-preserving gateway.
 *
 * @param string $chain_scope
 * @return array|false
 */
function domain_audit_gateway_lock_chain($chain_scope) {
    if (preg_match('/^company:\d+:legal_entity:\d+$/', (string)$chain_scope) !== 1) {
        return false;
    }
    $zero_hash = str_repeat('0', 64);
    if (!domain_audit_gateway_call(
        'domain_audit_chain_initialize_v1',
        array(domain_audit_db_string($chain_scope), domain_audit_db_string($zero_hash)),
        'could not initialize domain audit chain through the append identity',
        array(0, 1)
    )) {
        return false;
    }
    $result = db_query(
        "SELECT last_sequence, last_event_hash FROM `".TB_PREF."domain_audit_chain_state_read_v1`"
        ." WHERE chain_scope=".domain_audit_db_string($chain_scope)." FOR UPDATE",
        'could not lock domain audit chain through the append identity'
    );
    $state = $result ? db_fetch_assoc($result) : false;
    if (!$state || (int)$state['last_sequence'] < 0
        || preg_match('/^[a-f0-9]{64}$/', (string)$state['last_event_hash']) !== 1
    ) {
        return false;
    }
    return array(
        'last_sequence' => (int)$state['last_sequence'],
        'last_event_hash' => (string)$state['last_event_hash'],
    );
}

/**
 * Ensure and verify the signed genesis checkpoint through the gateway.
 *
 * @param string $chain_scope
 * @param string $key
 * @param bool $allow_create
 * @return array|false
 */
function domain_audit_gateway_genesis_checkpoint($chain_scope, $key, $allow_create = true) {
    $scope_parts = array();
    $company = preg_match('/^company:(\d+):legal_entity:\d+$/', (string)$chain_scope, $scope_parts) === 1
        ? (int)$scope_parts[1] : null;
    $result = db_query(
        "SELECT chain_scope, chain_sequence, event_hash, event_count, key_id, signature, created_at"
        ." FROM `".TB_PREF."domain_audit_genesis_checkpoint_read_v1`"
        ." WHERE chain_scope=".domain_audit_db_string($chain_scope)." LIMIT 1",
        'could not inspect domain audit genesis checkpoint through the append identity'
    );
    $checkpoint = $result ? db_fetch_assoc($result) : false;
    if (!$checkpoint && $allow_create) {
        $record = domain_audit_build_checkpoint($chain_scope, 0, str_repeat('0', 64), 0, $key);
        if ($record === false || !domain_audit_gateway_call(
            'domain_audit_checkpoint_insert_v1',
            array(
                domain_audit_db_string($record['chain_scope']),
                '0',
                domain_audit_db_string($record['event_hash']),
                '0',
                domain_audit_db_string($record['key_id']),
                domain_audit_db_string($record['signature']),
                domain_audit_db_string($record['created_at']),
            ),
            'could not append domain audit genesis checkpoint through the append identity'
        )) {
            return false;
        }
        $result = db_query(
            "SELECT chain_scope, chain_sequence, event_hash, event_count, key_id, signature, created_at"
            ." FROM `".TB_PREF."domain_audit_genesis_checkpoint_read_v1`"
            ." WHERE chain_scope=".domain_audit_db_string($chain_scope)." LIMIT 1",
            'could not reload domain audit genesis checkpoint through the append identity'
        );
        $checkpoint = $result ? db_fetch_assoc($result) : false;
    }
    if (!$checkpoint
        || (string)$checkpoint['chain_scope'] !== (string)$chain_scope
        || (int)$checkpoint['chain_sequence'] !== 0
        || (int)$checkpoint['event_count'] !== 0
        || (string)$checkpoint['event_hash'] !== str_repeat('0', 64)
        || ($company === null
            ? !domain_audit_verify_checkpoint($checkpoint, $key)
            : !domain_audit_verify_checkpoint_with_keyring($checkpoint, $company))
    ) {
        return false;
    }
    return $checkpoint;
}

/**
 * Append a built event row through the least-privilege definer routine.
 *
 * @param array $record
 * @return int|false
 */
function domain_audit_gateway_insert_event($record) {
    $fields = array(
        'chain_scope', 'chain_sequence', 'occurred_at', 'actor_user_id',
        'security_context_hash', 'company_id', 'legal_entity_id', 'entity_type',
        'entity_key_hash', 'action', 'reason_code', 'correlation_id',
        'legacy_source', 'legacy_id', 'before_hash', 'after_hash',
        'restricted_diff', 'previous_event_hash', 'event_hash', 'key_id',
    );
    foreach ($fields as $field) {
        if (!array_key_exists($field, $record)) {
            return false;
        }
    }
    $integer_fields = array('chain_sequence', 'actor_user_id', 'company_id', 'legal_entity_id', 'legacy_id');
    $arguments = array();
    foreach ($fields as $field) {
        $arguments[] = domain_audit_gateway_sql_value(
            $record[$field],
            in_array($field, $integer_fields, true)
        );
    }
    if (!domain_audit_gateway_call(
        'domain_audit_event_insert_v1',
        $arguments,
        'could not append domain audit event through the append identity'
    )) {
        return false;
    }
    $event_id = (int)db_insert_id();
    if ($event_id <= 0) {
        $result = db_query(
            'SELECT LAST_INSERT_ID() event_id',
            'could not resolve the appended domain audit event identifier'
        );
        $row = $result ? db_fetch_assoc($result) : false;
        $event_id = $row ? (int)$row['event_id'] : 0;
    }
    return $event_id > 0 ? $event_id : false;
}

/**
 * Append one exact outbox payload through the least-privilege definer routine.
 *
 * @param int $event_id
 * @param string $chain_scope
 * @param int $sequence
 * @param string $payload
 * @return string|false Payload SHA-256.
 */
function domain_audit_gateway_insert_outbox($event_id, $chain_scope, $sequence, $payload) {
    $payload_hash = is_string($payload) ? hash('sha256', $payload) : false;
    if ((int)$event_id <= 0 || (int)$sequence <= 0 || $payload_hash === false) {
        return false;
    }
    return domain_audit_gateway_call(
        'domain_audit_outbox_insert_v1',
        array(
            (string)(int)$event_id,
            domain_audit_db_string($chain_scope),
            (string)(int)$sequence,
            domain_audit_db_string($payload),
            domain_audit_db_string($payload_hash),
        ),
        'could not append domain audit outbox record through the append identity'
    ) ? $payload_hash : false;
}

/**
 * Append a signed non-genesis checkpoint through the definer routine.
 *
 * @param array $checkpoint
 * @return bool
 */
function domain_audit_gateway_insert_checkpoint($checkpoint) {
    $required = array(
        'chain_scope', 'chain_sequence', 'event_hash', 'event_count',
        'key_id', 'signature', 'created_at'
    );
    foreach ($required as $field) {
        if (!array_key_exists($field, $checkpoint)) {
            return false;
        }
    }
    return domain_audit_gateway_call(
        'domain_audit_checkpoint_insert_v1',
        array(
            domain_audit_db_string($checkpoint['chain_scope']),
            (string)(int)$checkpoint['chain_sequence'],
            domain_audit_db_string($checkpoint['event_hash']),
            (string)(int)$checkpoint['event_count'],
            domain_audit_db_string($checkpoint['key_id']),
            domain_audit_db_string($checkpoint['signature']),
            domain_audit_db_string($checkpoint['created_at']),
        ),
        'could not append domain audit checkpoint through the append identity'
    );
}

/**
 * Advance and verify the mutable chain cursor through the definer gateway.
 *
 * @param string $chain_scope
 * @param int $previous_sequence
 * @param string $previous_hash
 * @param int $sequence
 * @param string $event_hash
 * @return bool
 */
function domain_audit_gateway_advance_chain(
    $chain_scope,
    $previous_sequence,
    $previous_hash,
    $sequence,
    $event_hash
) {
    if (!domain_audit_gateway_call(
        'domain_audit_chain_advance_v1',
        array(
            domain_audit_db_string($chain_scope),
            (string)(int)$previous_sequence,
            domain_audit_db_string($previous_hash),
            (string)(int)$sequence,
            domain_audit_db_string($event_hash),
        ),
        'could not advance domain audit chain through the append identity'
    )) {
        return false;
    }
    $result = db_query(
        "SELECT last_sequence, last_event_hash FROM `".TB_PREF."domain_audit_chain_state_read_v1`"
        ." WHERE chain_scope=".domain_audit_db_string($chain_scope),
        'could not verify advanced domain audit chain through the append identity'
    );
    $state = $result ? db_fetch_assoc($result) : false;
    return $state
        && (int)$state['last_sequence'] === (int)$sequence
        && domain_audit_hash_equals((string)$state['last_event_hash'], (string)$event_hash);
}

/**
 * Insert a domain event, export outbox row and signed checkpoint atomically.
 *
 * @param array $event
 * @return array|false
 */
function append_domain_audit_event($event) {
    $GLOBALS['domain_audit_append_result'] = 'validation_started';
    if (!is_array($event)) {
        $GLOBALS['domain_audit_append_result'] = 'event_invalid';
        return false;
    }
    $company = isset($event['company_id']) ? (int)$event['company_id']
        : (function_exists('user_company') ? (int)user_company() : 0);
    $event['company_id'] = $company;
    $legal_entity = isset($event['legal_entity_id']) ? (int)$event['legal_entity_id'] : 0;
    if ($company < 0 || $legal_entity < 0) {
        $GLOBALS['domain_audit_append_result'] = 'scope_invalid';
        return false;
    }
    $chain_scope = 'company:'.$company.':legal_entity:'.$legal_entity;
    if (!domain_audit_acquire_key_lock($company)) {
        $GLOBALS['domain_audit_append_result'] = 'key_lock_failed';
        return false;
    }
    $key = domain_audit_signing_key($company);
    if ($key === false) {
        $GLOBALS['domain_audit_append_result'] = 'signing_key_unavailable';
        domain_audit_release_key_lock($company);
        return false;
    }

    begin_transaction();
    $state = domain_audit_gateway_lock_chain($chain_scope);
    if (!$state) {
        $GLOBALS['domain_audit_append_result'] = 'gateway_chain_lock_failed';
        cancel_transaction();
        domain_audit_release_key_lock($company);
        return false;
    }
    $last_sequence = (int)$state['last_sequence'];
    $previous_hash = (string)$state['last_event_hash'];
    $empty_chain = $last_sequence === 0 && $previous_hash === str_repeat('0', 64);
    if (!$empty_chain && $last_sequence === 0) {
        $GLOBALS['domain_audit_append_result'] = 'chain_state_invalid';
        cancel_transaction();
        domain_audit_release_key_lock($company);
        return false;
    }
    if (domain_audit_gateway_genesis_checkpoint($chain_scope, $key, $empty_chain) === false) {
        $GLOBALS['domain_audit_append_result'] = 'gateway_genesis_failed';
        cancel_transaction();
        domain_audit_release_key_lock($company);
        return false;
    }
    $sequence = $last_sequence + 1;
    $record = domain_audit_build_event_record($event, $previous_hash, $sequence, $key);
    if ($record === false) {
        $GLOBALS['domain_audit_append_result'] = 'event_build_failed';
        cancel_transaction();
        domain_audit_release_key_lock($company);
        return false;
    }

    $event_id = domain_audit_gateway_insert_event($record);
    if ($event_id === false) {
        $GLOBALS['domain_audit_append_result'] = 'gateway_event_insert_failed';
        cancel_transaction();
        domain_audit_release_key_lock($company);
        return false;
    }

    $outbox_payload = domain_audit_canonical_json(array_merge(array('event_id' => $event_id), $record));
    if ($outbox_payload === false) {
        $GLOBALS['domain_audit_append_result'] = 'outbox_payload_failed';
        cancel_transaction();
        domain_audit_release_key_lock($company);
        return false;
    }
    $outbox_hash = domain_audit_gateway_insert_outbox(
        $event_id,
        $chain_scope,
        $sequence,
        $outbox_payload
    );
    if ($outbox_hash === false) {
        $GLOBALS['domain_audit_append_result'] = 'gateway_outbox_insert_failed';
        cancel_transaction();
        domain_audit_release_key_lock($company);
        return false;
    }

    $checkpoint = domain_audit_build_checkpoint(
        $chain_scope,
        $sequence,
        $record['event_hash'],
        $sequence,
        $key,
        $record['occurred_at']
    );
    if ($checkpoint === false) {
        $GLOBALS['domain_audit_append_result'] = 'checkpoint_build_failed';
        cancel_transaction();
        domain_audit_release_key_lock($company);
        return false;
    }
    if (!domain_audit_gateway_insert_checkpoint($checkpoint)) {
        $GLOBALS['domain_audit_append_result'] = 'gateway_checkpoint_insert_failed';
        cancel_transaction();
        domain_audit_release_key_lock($company);
        return false;
    }

    if (!domain_audit_gateway_advance_chain(
        $chain_scope,
        (int)$state['last_sequence'],
        $previous_hash,
        $sequence,
        $record['event_hash']
    )) {
        $GLOBALS['domain_audit_append_result'] = 'gateway_chain_advance_failed';
        cancel_transaction();
        domain_audit_release_key_lock($company);
        return false;
    }
    commit_transaction();
    $released = domain_audit_release_key_lock($company);
    if (!$released) {
        error_log('PAY-AUD-001 committed audit event but could not release the key-selection lock');
    }
    $GLOBALS['domain_audit_append_result'] = 'appended';

    return array(
        'event_id' => $event_id,
        'chain_scope' => $chain_scope,
        'chain_sequence' => $sequence,
        'event_hash' => $record['event_hash'],
        'checkpoint_signature' => $checkpoint['signature'],
        'correlation_id' => $record['correlation_id'],
    );
}

/**
 * Verify chain continuity plus every outbox payload and signed checkpoint.
 *
 * @param int|null $company
 * @param int $legal_entity
 * @return array
 */
function verify_domain_audit_chain($company = null, $legal_entity = 0) {
    if ($company === null) {
        $company = function_exists('user_company') ? (int)user_company() : 0;
    }
    $company = (int)$company;
    $legal_entity = (int)$legal_entity;
    if ($company < 0 || $legal_entity < 0) {
        return array('ok' => false, 'error' => 'invalid_chain_scope');
    }
    $chain_scope = 'company:'.$company.':legal_entity:'.$legal_entity;
    $paths = domain_audit_keyring_paths($company);
    if ($paths === false) {
        return array('ok' => false, 'error' => 'audit_key_unavailable');
    }
    if (is_file($paths['pending'])
        && !(isset($GLOBALS['domain_audit_key_rotation_company'])
            && (int)$GLOBALS['domain_audit_key_rotation_company'] === $company)
    ) {
        return array('ok' => false, 'error' => 'audit_key_rotation_pending');
    }
    $manifest = domain_audit_keyring_manifest($company);
    if ($manifest === false) {
        return array('ok' => false, 'error' => 'audit_keyring_invalid');
    }
    if ($legal_entity !== 0) {
        $company_chain = verify_domain_audit_chain($company, 0);
        if (empty($company_chain['ok'])) {
            return array(
                'ok' => false,
                'error' => 'company_key_transition_chain_invalid',
            );
        }
    }

    $result = db_query("SELECT * FROM ".TB_PREF."domain_audit_events WHERE chain_scope="
        .domain_audit_db_string($chain_scope)." ORDER BY chain_sequence",
        'could not read domain audit events');
    if (!$result) {
        return array('ok' => false, 'error' => 'event_read_failed');
    }
    $rows = array();
    while ($row = db_fetch_assoc($result)) {
        $rows[] = $row;
    }
    $verified = domain_audit_verify_event_rows($rows);
    if (!$verified['ok']) {
        return $verified;
    }

    $state_result = db_query("SELECT last_sequence, last_event_hash FROM ".TB_PREF."domain_audit_chain_state"
        ." WHERE chain_scope=".domain_audit_db_string($chain_scope), 'could not read domain audit state');
    $state = $state_result ? db_fetch_assoc($state_result) : false;
    if (!$state || (int)$state['last_sequence'] !== (int)$verified['last_sequence']
        || !domain_audit_hash_equals((string)$state['last_event_hash'], (string)$verified['last_event_hash'])
    ) {
        return array('ok' => false, 'error' => 'chain_state_mismatch');
    }

    $outbox_result = db_query("SELECT * FROM ".TB_PREF."domain_audit_outbox WHERE chain_scope="
        .domain_audit_db_string($chain_scope)." ORDER BY chain_sequence",
        'could not reconcile domain audit outbox');
    if (!$outbox_result) {
        return array('ok' => false, 'error' => 'outbox_read_failed');
    }
    $outbox_count = 0;
    while ($outbox = db_fetch_assoc($outbox_result)) {
        if (!isset($rows[$outbox_count])) {
            return array('ok' => false, 'error' => 'outbox_extra_record');
        }
        $event_row = $rows[$outbox_count];
        $expected_record = domain_audit_event_hash_fields($event_row);
        $expected_record['event_hash'] = (string)$event_row['event_hash'];
        $expected_payload = domain_audit_canonical_json(array_merge(
            array('event_id' => (int)$event_row['event_id']),
            $expected_record
        ));
        if ($expected_payload === false
            || (int)$outbox['event_id'] !== (int)$event_row['event_id']
            || (string)$outbox['chain_scope'] !== $chain_scope
            || (int)$outbox['chain_sequence'] !== (int)$event_row['chain_sequence']
            || !domain_audit_hash_equals((string)$outbox['payload'], $expected_payload)
            || !domain_audit_hash_equals((string)$outbox['payload_hash'], hash('sha256', $expected_payload))
        ) {
            return array('ok' => false, 'error' => 'outbox_verification_failed', 'sequence' => $outbox_count + 1);
        }
        $outbox_count++;
    }
    if ($outbox_count !== count($rows)) {
        return array('ok' => false, 'error' => 'outbox_omission');
    }

    $checkpoint_result = db_query("SELECT * FROM ".TB_PREF."domain_audit_checkpoints"
        ." WHERE chain_scope=".domain_audit_db_string($chain_scope)." ORDER BY chain_sequence",
        'could not reconcile domain audit checkpoints');
    if (!$checkpoint_result) {
        return array('ok' => false, 'error' => 'checkpoint_read_failed');
    }
    $checkpoint_count = 0;
    $expected_key_id = isset($manifest['keys'][0]['key_id'])
        ? (string)$manifest['keys'][0]['key_id'] : '';
    $expected_key_generation = 1;
    $entity_key_generation = 0;
    while ($checkpoint = db_fetch_assoc($checkpoint_result)) {
        $expected_sequence = $checkpoint_count;
        $expected_hash = $expected_sequence === 0
            ? str_repeat('0', 64) : (isset($rows[$expected_sequence - 1]) ? (string)$rows[$expected_sequence - 1]['event_hash'] : '');
        $event_row = $expected_sequence > 0 && isset($rows[$expected_sequence - 1])
            ? $rows[$expected_sequence - 1] : null;
        $checkpoint_key_valid = true;
        if ($legal_entity === 0) {
            $checkpoint_key_valid =
                domain_audit_hash_equals(
                    (string)$checkpoint['key_id'], $expected_key_id
                )
                && ($event_row === null
                    || domain_audit_hash_equals(
                        (string)$event_row['key_id'], $expected_key_id
                    ));
        } else {
            $keyring_entry = domain_audit_keyring_entry(
                $manifest, (string)$checkpoint['key_id']
            );
            $generation = $keyring_entry === false
                ? 0 : (int)$keyring_entry['generation'];
            $checkpoint_key_valid = $keyring_entry !== false
                && $generation >= $entity_key_generation
                && domain_audit_checkpoint_key_time_valid(
                    $keyring_entry, (string)$checkpoint['created_at']
                )
                && ($event_row === null
                    || domain_audit_hash_equals(
                        (string)$event_row['key_id'],
                        (string)$checkpoint['key_id']
                    ));
        }
        if ((int)$checkpoint['chain_sequence'] !== $expected_sequence
            || (int)$checkpoint['event_count'] !== $expected_sequence
            || (string)$checkpoint['chain_scope'] !== $chain_scope
            || !domain_audit_hash_equals((string)$checkpoint['event_hash'], $expected_hash)
            || !$checkpoint_key_valid
            || !domain_audit_verify_checkpoint_with_keyring($checkpoint, $company)
        ) {
            return array('ok' => false, 'error' => 'checkpoint_verification_failed', 'sequence' => $expected_sequence);
        }
        if ($legal_entity !== 0) {
            $entity_key_generation = $generation;
        } elseif ($event_row !== null
            && (string)$event_row['action'] === 'audit.key.rotated'
        ) {
            $transition = json_decode((string)$event_row['restricted_diff'], true);
            if (!is_array($transition)
                || $transition !== array(
                    'generation' => $expected_key_generation + 1,
                    'new_key_id' => isset($transition['new_key_id']) ? (string)$transition['new_key_id'] : '',
                    'previous_key_id' => $expected_key_id,
                    'restricted_values_stored' => false,
                )
                || preg_match('/^[a-f0-9]{16}$/', (string)$transition['new_key_id']) !== 1
                || domain_audit_key_by_id($company, $transition['new_key_id']) === false
            ) {
                return array('ok' => false, 'error' => 'key_rotation_transition_invalid', 'sequence' => $expected_sequence);
            }
            $expected_key_id = (string)$transition['new_key_id'];
            $expected_key_generation++;
        }
        $checkpoint_count++;
    }
    if ($checkpoint_count !== count($rows) + 1) {
        return array('ok' => false, 'error' => 'checkpoint_omission');
    }
    if ($legal_entity === 0
        && !domain_audit_hash_equals(
            $expected_key_id, (string)$manifest['active_key_id']
        )
    ) {
        return array('ok' => false, 'error' => 'active_key_transition_mismatch');
    }
    if ($legal_entity === 0
        && $expected_key_generation !== (int)$manifest['generation']
    ) {
        return array('ok' => false, 'error' => 'key_generation_transition_mismatch');
    }

    $verified['outbox_count'] = $outbox_count;
    $verified['checkpoint_count'] = $checkpoint_count;
    $verified['key_generation'] = (int)$manifest['generation'];
    $verified['active_key_id'] = (string)$manifest['active_key_id'];
    return $verified;
}
