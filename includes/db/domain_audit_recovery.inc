<?php
/**
 * PAY-AUD-001 key-inclusive backup manifest and restore verification.
 */

include_once($path_to_root.'/includes/db/domain_audit_export.inc');

define('DOMAIN_AUDIT_RECOVERY_SCHEMA', 'notrinoserp.domain-audit-recovery-manifest.v1');
define('DOMAIN_AUDIT_RECOVERY_MAX_MANIFEST_BYTES', 262144);

/**
 * Validate an external regular-file backup input.
 *
 * @param string $path
 * @param int $company
 * @return string|false
 */
function domain_audit_recovery_external_file($path, $company)
{
    global $path_to_root;

    if (!is_string($path) || !preg_match('/^(?:[A-Za-z]:[\\\\\/]|\/)/', $path)
        || is_link($path) || !is_file($path)
    ) {
        return false;
    }
    $resolved = realpath($path);
    $application = realpath($path_to_root);
    $private_root = domain_audit_private_root($company);
    return $resolved !== false && $application !== false && $private_root !== false
        && !domain_audit_export_path_is_within($resolved, $application)
        && !domain_audit_export_path_is_within($resolved, $private_root)
        ? $resolved : false;
}

/**
 * Return one non-secret hash/size record.
 *
 * @param string $name
 * @param string $path
 * @return array|false
 */
function domain_audit_recovery_file_record($name, $path)
{
    $size = @filesize($path);
    $hash = @hash_file('sha256', $path);
    if (!is_string($name) || preg_match('/^[A-Za-z0-9_.-]{1,128}$/', $name) !== 1
        || $size === false || $size < 1
        || !is_string($hash) || preg_match('/^[a-f0-9]{64}$/', $hash) !== 1
    ) {
        return false;
    }
    return array('name' => $name, 'size' => (int)$size, 'sha256' => $hash);
}

/**
 * Build the exact protected-key file inventory for a validated keyring.
 *
 * @param int $company
 * @param array $keyring
 * @param string|null $root_override
 * @return array|false
 */
function domain_audit_recovery_key_records($company, $keyring, $root_override = null)
{
    if (!is_array($keyring) || !isset($keyring['keys'])) {
        return false;
    }
    $paths = domain_audit_keyring_paths($company);
    $keys_root = $root_override === null
        ? ($paths === false ? false : $paths['keys'])
        : rtrim((string)$root_override, "\\/").DIRECTORY_SEPARATOR.'keys';
    if ($keys_root === false || !is_dir($keys_root) || is_link($keys_root)) {
        return false;
    }
    $records = array();
    if (empty($keyring['legacy_synthesized'])) {
        $record = domain_audit_recovery_file_record(
            'domain-audit-keyring-v1.json',
            $keys_root.DIRECTORY_SEPARATOR.'domain-audit-keyring-v1.json'
        );
        if ($record === false) {
            return false;
        }
        $records[] = $record;
    }
    foreach ($keyring['keys'] as $entry) {
        $record = domain_audit_recovery_file_record(
            (string)$entry['filename'],
            $keys_root.DIRECTORY_SEPARATOR.$entry['filename']
        );
        if ($record === false) {
            return false;
        }
        $records[] = $record;
    }
    usort($records, 'domain_audit_recovery_compare_file_record');
    return $records;
}

/**
 * Stable recovery-file ordering.
 *
 * @param array $left
 * @param array $right
 * @return int
 */
function domain_audit_recovery_compare_file_record($left, $right)
{
    return strcmp((string)$left['name'], (string)$right['name']);
}

/**
 * Return hashes for configured non-secret or external control files.
 *
 * @param int $company
 * @return array|false
 */
function domain_audit_recovery_control_records($company)
{
    global $db_connections;

    $configuration = isset($db_connections[$company])
        ? $db_connections[$company] : array();
    $specs = array(
        'custody_public_key' => 'payroll_audit_custody_public_key_file',
        'export_credential' => 'payroll_audit_export_credential_file',
        'review_credential' => 'payroll_audit_review_credential_file',
    );
    $records = array();
    foreach ($specs as $name => $setting) {
        if (!isset($configuration[$setting])) {
            continue;
        }
        $path = domain_audit_recovery_external_file(
            (string)$configuration[$setting], $company
        );
        $record = $path === false
            ? false : domain_audit_recovery_file_record($name, $path);
        if ($record === false) {
            return false;
        }
        $records[] = $record;
    }
    usort($records, 'domain_audit_recovery_compare_file_record');
    return $records;
}

/**
 * Build a signed recovery manifest without copying secret file contents.
 *
 * @param int $company
 * @param string $database_backup
 * @return array|false
 */
function domain_audit_build_recovery_manifest($company, $database_backup)
{
    global $db_version;

    $company = (int)$company;
    $backup_path = domain_audit_recovery_external_file($database_backup, $company);
    $keyring = domain_audit_keyring_manifest($company);
    $active_key = domain_audit_signing_key($company);
    $key_files = $keyring === false
        ? false : domain_audit_recovery_key_records($company, $keyring);
    $controls = domain_audit_recovery_control_records($company);
    if ($company < 0 || $backup_path === false || $keyring === false
        || $active_key === false || $key_files === false || $controls === false
    ) {
        return false;
    }
    $database = domain_audit_recovery_file_record(
        'company-database-backup.sql', $backup_path
    );
    if ($database === false) {
        return false;
    }
    $payload = array(
        'schema_version' => DOMAIN_AUDIT_RECOVERY_SCHEMA,
        'company_id' => $company,
        'source_database_version' => (string)$db_version,
        'created_at' => gmdate('Y-m-d H:i:s'),
        'database_backup' => $database,
        'keyring_generation' => (int)$keyring['generation'],
        'active_key_id' => (string)$keyring['active_key_id'],
        'key_files' => $key_files,
        'control_files' => $controls,
    );
    $signing_key = domain_audit_derived_key($active_key, 'recovery-manifest-v1');
    $json = domain_audit_canonical_json($payload);
    if ($signing_key === false || $json === false) {
        return false;
    }
    return array(
        'manifest' => $payload,
        'signature' => hash_hmac('sha256', $json, $signing_key),
    );
}

/**
 * Write a new recovery manifest exclusively outside protected roots.
 *
 * @param string $output_path
 * @param array $envelope
 * @param int $company
 * @return bool
 */
function domain_audit_write_recovery_manifest($output_path, $envelope, $company)
{
    global $path_to_root;

    if (!is_array($envelope)
        || !preg_match('/^(?:[A-Za-z]:[\\\\\/]|\/)/', (string)$output_path)
        || file_exists($output_path) || is_link(dirname($output_path))
    ) {
        return false;
    }
    $parent = realpath(dirname($output_path));
    $application = realpath($path_to_root);
    $private_root = domain_audit_private_root($company);
    if ($parent === false || $application === false || $private_root === false
        || domain_audit_export_path_is_within($parent, $application)
        || domain_audit_export_path_is_within($parent, $private_root)
    ) {
        return false;
    }
    $json = domain_audit_canonical_json($envelope);
    if ($json === false || strlen($json) > DOMAIN_AUDIT_RECOVERY_MAX_MANIFEST_BYTES) {
        return false;
    }
    $handle = @fopen($output_path, 'xb');
    if (!$handle) {
        return false;
    }
    $written = @fwrite($handle, $json."\n");
    $closed = @fclose($handle);
    @chmod($output_path, 0600);
    return $closed && $written === strlen($json) + 1;
}

/**
 * Verify a signed manifest against a database backup and restored key root.
 *
 * @param array $envelope
 * @param int $company
 * @param string $database_backup
 * @param string $restored_private_root
 * @return array|false
 */
function domain_audit_verify_recovery_manifest(
    $envelope,
    $company,
    $database_backup,
    $restored_private_root
) {
    if (!is_array($envelope)
        || array_keys($envelope) !== array('manifest', 'signature')
        || !is_array($envelope['manifest'])
        || preg_match('/^[a-f0-9]{64}$/', (string)$envelope['signature']) !== 1
    ) {
        return false;
    }
    $manifest = $envelope['manifest'];
    $expected_fields = array(
        'active_key_id', 'company_id', 'control_files', 'created_at',
        'database_backup', 'key_files', 'keyring_generation',
        'schema_version', 'source_database_version',
    );
    $actual_fields = array_keys($manifest);
    sort($expected_fields);
    sort($actual_fields);
    if ($actual_fields !== $expected_fields
        || $manifest['schema_version'] !== DOMAIN_AUDIT_RECOVERY_SCHEMA
        || !is_int($manifest['company_id'])
        || $manifest['company_id'] !== (int)$company
        || !domain_audit_valid_utc_timestamp($manifest['created_at'])
        || !is_int($manifest['keyring_generation'])
        || $manifest['keyring_generation'] < 1
        || preg_match('/^[a-f0-9]{16}$/',
            (string)$manifest['active_key_id']) !== 1
    ) {
        return false;
    }
    $backup_path = domain_audit_recovery_external_file($database_backup, $company);
    $database_record = $backup_path === false ? false
        : domain_audit_recovery_file_record('company-database-backup.sql', $backup_path);
    if ($database_record === false
        || $database_record !== $manifest['database_backup']
    ) {
        return false;
    }
    $root = realpath($restored_private_root);
    if ($root === false || is_link($restored_private_root)
        || !is_dir($root)
    ) {
        return false;
    }
    $keyring_path = $root.DIRECTORY_SEPARATOR.'keys'
        .DIRECTORY_SEPARATOR.'domain-audit-keyring-v1.json';
    $keyring_json = @file_get_contents($keyring_path);
    $keyring = is_string($keyring_json) ? json_decode($keyring_json, true) : null;
    if (!is_array($keyring)) {
        $legacy_path = $root.DIRECTORY_SEPARATOR.'keys'
            .DIRECTORY_SEPARATOR.'domain-audit-hmac-v1.key';
        $legacy_key = domain_audit_load_key_file($legacy_path);
        $legacy_key_id = $legacy_key === false
            ? false : domain_audit_key_id($legacy_key);
        if ($legacy_key_id !== false) {
            $keyring = array(
                'schema_version' => 'notrinoserp.domain-audit-keyring.v1',
                'company_id' => (int)$company,
                'active_key_id' => $legacy_key_id,
                'generation' => 1,
                'keys' => array(array(
                    'key_id' => $legacy_key_id,
                    'generation' => 1,
                    'filename' => 'domain-audit-hmac-v1.key',
                    'activated_at' => '',
                    'retired_at' => '',
                )),
                'legacy_synthesized' => true,
            );
        }
    }
    if (!is_array($keyring)
        || !isset($keyring['company_id'], $keyring['generation'],
            $keyring['active_key_id'], $keyring['keys'])
        || (int)$keyring['company_id'] !== (int)$company
        || (int)$keyring['generation'] !== $manifest['keyring_generation']
        || $keyring['active_key_id'] !== $manifest['active_key_id']
    ) {
        return false;
    }
    $records = domain_audit_recovery_key_records($company, $keyring, $root);
    if ($records === false || $records !== $manifest['key_files']) {
        return false;
    }
    $active_path = false;
    foreach ($keyring['keys'] as $entry) {
        if ($entry['key_id'] === $manifest['active_key_id']) {
            $active_path = $root.DIRECTORY_SEPARATOR.'keys'
                .DIRECTORY_SEPARATOR.$entry['filename'];
        }
    }
    $active_key = $active_path === false ? false
        : domain_audit_load_key_file($active_path, $manifest['active_key_id']);
    $signing_key = $active_key === false ? false
        : domain_audit_derived_key($active_key, 'recovery-manifest-v1');
    $json = domain_audit_canonical_json($manifest);
    if ($signing_key === false || $json === false
        || !domain_audit_hash_equals(
            hash_hmac('sha256', $json, $signing_key),
            (string)$envelope['signature']
        )
    ) {
        return false;
    }
    return array(
        'ok' => true,
        'company_id' => (int)$company,
        'keyring_generation' => (int)$manifest['keyring_generation'],
        'active_key_id' => (string)$manifest['active_key_id'],
        'database_backup_sha256' => (string)$database_record['sha256'],
    );
}
