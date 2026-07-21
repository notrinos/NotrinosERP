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
function domain_audit_signing_key($company = null, $allow_create = false) {
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
 * Insert a domain event, export outbox row and signed checkpoint atomically.
 *
 * @param array $event
 * @return array|false
 */
function append_domain_audit_event($event) {
    if (!is_array($event)) {
        return false;
    }
    $company = isset($event['company_id']) ? (int)$event['company_id']
        : (function_exists('user_company') ? (int)user_company() : 0);
    $event['company_id'] = $company;
    $legal_entity = isset($event['legal_entity_id']) ? (int)$event['legal_entity_id'] : 0;
    if ($company < 0 || $legal_entity < 0) {
        return false;
    }
    $chain_scope = 'company:'.$company.':legal_entity:'.$legal_entity;
    $key = domain_audit_signing_key($company);
    if ($key === false) {
        return false;
    }

    begin_transaction();
    $insert_state = "INSERT IGNORE INTO ".TB_PREF."domain_audit_chain_state"
        ." (chain_scope, last_sequence, last_event_hash, updated_at) VALUES ("
        .domain_audit_db_string($chain_scope).", 0, ".domain_audit_db_string(str_repeat('0', 64)).", CURRENT_TIMESTAMP)";
    if (!db_query($insert_state, 'could not initialize domain audit chain state')) {
        cancel_transaction();
        return false;
    }
    $state_result = db_query("SELECT last_sequence, last_event_hash FROM ".TB_PREF."domain_audit_chain_state"
        ." WHERE chain_scope=".domain_audit_db_string($chain_scope)." FOR UPDATE",
        'could not lock domain audit chain state');
    $state = $state_result ? db_fetch_assoc($state_result) : false;
    if (!$state) {
        cancel_transaction();
        return false;
    }
    $last_sequence = (int)$state['last_sequence'];
    $previous_hash = (string)$state['last_event_hash'];
    $empty_chain = $last_sequence === 0 && $previous_hash === str_repeat('0', 64);
    if (!$empty_chain && $last_sequence === 0) {
        cancel_transaction();
        return false;
    }
    if (ensure_domain_audit_genesis_checkpoint($chain_scope, $key, $empty_chain) === false) {
        cancel_transaction();
        return false;
    }
    $sequence = $last_sequence + 1;
    $record = domain_audit_build_event_record($event, $previous_hash, $sequence, $key);
    if ($record === false) {
        cancel_transaction();
        return false;
    }

    $columns = array_keys($record);
    $values = array();
    foreach ($record as $name => $value) {
        if ($value === null) {
            $values[] = 'NULL';
        } elseif (in_array($name, array('chain_sequence', 'actor_user_id', 'company_id', 'legal_entity_id', 'legacy_id'), true)) {
            $values[] = (string)(int)$value;
        } else {
            $values[] = domain_audit_db_string($value);
        }
    }
    $sql = "INSERT INTO ".TB_PREF."domain_audit_events (".implode(',', $columns).") VALUES (".implode(',', $values).")";
    if (!db_query($sql, 'could not append domain audit event')) {
        cancel_transaction();
        return false;
    }
    $event_id = (int)db_insert_id();

    $outbox_payload = domain_audit_canonical_json(array_merge(array('event_id' => $event_id), $record));
    if ($outbox_payload === false) {
        cancel_transaction();
        return false;
    }
    $outbox_hash = hash('sha256', $outbox_payload);
    $outbox_sql = "INSERT INTO ".TB_PREF."domain_audit_outbox"
        ." (event_id, chain_scope, chain_sequence, payload, payload_hash, created_at) VALUES ("
        .$event_id.",".domain_audit_db_string($chain_scope).",".$sequence.","
        .domain_audit_db_string($outbox_payload).",".domain_audit_db_string($outbox_hash).",CURRENT_TIMESTAMP)";
    if (!db_query($outbox_sql, 'could not append domain audit outbox record')) {
        cancel_transaction();
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
        cancel_transaction();
        return false;
    }
    $checkpoint_sql = "INSERT INTO ".TB_PREF."domain_audit_checkpoints"
        ." (chain_scope, chain_sequence, event_hash, event_count, key_id, signature, created_at) VALUES ("
        .domain_audit_db_string($checkpoint['chain_scope']).",".(int)$checkpoint['chain_sequence'].","
        .domain_audit_db_string($checkpoint['event_hash']).",".(int)$checkpoint['event_count'].","
        .domain_audit_db_string($checkpoint['key_id']).",".domain_audit_db_string($checkpoint['signature']).","
        .domain_audit_db_string($checkpoint['created_at']).")";
    if (!db_query($checkpoint_sql, 'could not append domain audit checkpoint')) {
        cancel_transaction();
        return false;
    }

    $state_sql = "UPDATE ".TB_PREF."domain_audit_chain_state SET last_sequence=".$sequence
        .", last_event_hash=".domain_audit_db_string($record['event_hash']).", updated_at=CURRENT_TIMESTAMP"
        ." WHERE chain_scope=".domain_audit_db_string($chain_scope)
        ." AND last_sequence=".(int)$state['last_sequence']
        ." AND last_event_hash=".domain_audit_db_string($previous_hash);
    if (!db_query($state_sql, 'could not advance domain audit chain state')) {
        cancel_transaction();
        return false;
    }
    $advanced_result = db_query("SELECT last_sequence, last_event_hash FROM ".TB_PREF."domain_audit_chain_state"
        ." WHERE chain_scope=".domain_audit_db_string($chain_scope),
        'could not verify advanced domain audit chain state');
    $advanced = $advanced_result ? db_fetch_assoc($advanced_result) : false;
    if (!$advanced || (int)$advanced['last_sequence'] !== $sequence
        || !domain_audit_hash_equals((string)$advanced['last_event_hash'], (string)$record['event_hash'])
    ) {
        cancel_transaction();
        return false;
    }
    commit_transaction();

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
    $key = domain_audit_signing_key($company);
    if ($key === false) {
        return array('ok' => false, 'error' => 'audit_key_unavailable');
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
    while ($checkpoint = db_fetch_assoc($checkpoint_result)) {
        $expected_sequence = $checkpoint_count;
        $expected_hash = $expected_sequence === 0
            ? str_repeat('0', 64) : (isset($rows[$expected_sequence - 1]) ? (string)$rows[$expected_sequence - 1]['event_hash'] : '');
        if ((int)$checkpoint['chain_sequence'] !== $expected_sequence
            || (int)$checkpoint['event_count'] !== $expected_sequence
            || (string)$checkpoint['chain_scope'] !== $chain_scope
            || !domain_audit_hash_equals((string)$checkpoint['event_hash'], $expected_hash)
            || !domain_audit_verify_checkpoint($checkpoint, $key)
        ) {
            return array('ok' => false, 'error' => 'checkpoint_verification_failed', 'sequence' => $expected_sequence);
        }
        $checkpoint_count++;
    }
    if ($checkpoint_count !== count($rows) + 1) {
        return array('ok' => false, 'error' => 'checkpoint_omission');
    }

    $verified['outbox_count'] = $outbox_count;
    $verified['checkpoint_count'] = $checkpoint_count;
    return $verified;
}
