<?php
/**********************************************************************
    Copyright (C) NotrinosERP.
    Released under the terms of the GNU General Public License, GPL,
    as published by the Free Software Foundation, either version 3
    of the License, or (at your option) any later version.
***********************************************************************/

include_once($path_to_root.'/includes/db/domain_audit_db.inc');

/** Maximum number of outbox events in one custody package. */
define('DOMAIN_AUDIT_EXPORT_MAX_EVENTS', 1000);

/** Maximum serialized package size accepted by the staging writer. */
define('DOMAIN_AUDIT_EXPORT_MAX_BYTES', 16777216);

/** Smallest and largest accepted custody delivery intervals. */
define('DOMAIN_AUDIT_CUSTODY_MIN_DELAY_SECONDS', 60);
define('DOMAIN_AUDIT_CUSTODY_MAX_DELAY_SECONDS', 604800);

/** Versioned format for the external custody-export database credential. */
define('DOMAIN_AUDIT_EXPORT_CREDENTIAL_SCHEMA', 'notrinoserp.domain-audit-export-credential.v1');

/** Maximum accepted credential-file size. */
define('DOMAIN_AUDIT_EXPORT_CREDENTIAL_MAX_BYTES', 16384);

/**
 * Return whether a resolved path is inside a resolved parent path.
 *
 * @param string $path
 * @param string $parent
 * @return bool
 */
function domain_audit_export_path_is_within($path, $parent)
{
    $path = str_replace('\\', '/', (string)$path);
    $parent = rtrim(str_replace('\\', '/', (string)$parent), '/');
    if (DIRECTORY_SEPARATOR === '\\') {
        $path = strtolower($path);
        $parent = strtolower($parent);
    }
    return $path === $parent || strpos($path, $parent.'/') === 0;
}

/**
 * Return the configured external credential-file path for one company.
 *
 * @param int $company
 * @return string|false
 */
function domain_audit_export_credential_file_path($company)
{
    global $db_connections, $path_to_root;

    $company = (int)$company;
    if (!isset($db_connections[$company]['payroll_audit_export_credential_file'])) {
        return false;
    }
    $path = trim((string)$db_connections[$company]['payroll_audit_export_credential_file']);
    if ($path === '' || !preg_match('/^(?:[A-Za-z]:[\\\\\/]|\/)/', $path)
        || is_link($path) || !is_file($path) || filesize($path) > DOMAIN_AUDIT_EXPORT_CREDENTIAL_MAX_BYTES
    ) {
        return false;
    }
    $resolved = realpath($path);
    $application = realpath($path_to_root);
    $private_root = domain_audit_private_root($company);
    if ($resolved === false || $application === false || $private_root === false
        || domain_audit_export_path_is_within($resolved, $application)
        || domain_audit_export_path_is_within($resolved, $private_root)
    ) {
        return false;
    }
    if (DIRECTORY_SEPARATOR !== '\\') {
        $permissions = @fileperms($resolved);
        if ($permissions === false || (($permissions & 0007) !== 0) || (($permissions & 0020) !== 0)) {
            return false;
        }
    }
    return $resolved;
}

/**
 * Load and validate the company-bound custody-export database credential.
 *
 * The returned secret must never be logged, persisted in business data or
 * included in CLI output.
 *
 * @param int $company
 * @return array|false
 */
function domain_audit_export_credential($company)
{
    global $db_connections;

    $company = (int)$company;
    if (!isset($db_connections[$company])) {
        return false;
    }
    $path = domain_audit_export_credential_file_path($company);
    $contents = $path !== false ? @file_get_contents($path) : false;
    $credential = is_string($contents) ? json_decode($contents, true) : null;
    if (!is_array($credential)) {
        return false;
    }
    $expected_keys = array(
        'company_id', 'database_account_host', 'database_host', 'database_name',
        'database_password', 'database_port', 'database_user', 'schema_version', 'table_prefix'
    );
    $actual_keys = array_keys($credential);
    sort($expected_keys);
    sort($actual_keys);
    if ($actual_keys !== $expected_keys
        || $credential['schema_version'] !== DOMAIN_AUDIT_EXPORT_CREDENTIAL_SCHEMA
        || !is_int($credential['company_id']) || $credential['company_id'] !== $company
    ) {
        return false;
    }

    $connection = $db_connections[$company];
    $configured_port = empty($connection['port']) ? '3306' : (string)$connection['port'];
    $credential_port = (string)$credential['database_port'];
    if ((string)$credential['database_host'] !== (string)$connection['host']
        || $credential_port !== $configured_port
        || (string)$credential['database_name'] !== (string)$connection['dbname']
        || (string)$credential['table_prefix'] !== (string)$connection['tbpref']
        || preg_match('/^[A-Za-z0-9_.-]{1,80}$/', (string)$credential['database_user']) !== 1
        || preg_match('/^[A-Za-z0-9.:-]{1,255}$/', (string)$credential['database_account_host']) !== 1
        || strlen((string)$credential['database_password']) < 24
        || strlen((string)$credential['database_password']) > 1024
        || preg_match('/[\x00-\x1F\x7F]/', (string)$credential['database_password']) === 1
    ) {
        return false;
    }
    return $credential;
}

/**
 * Open the configured database using only the custody-export credential.
 *
 * This deliberately avoids set_global_connection(), which uses the broad
 * company application credential and refreshes all company preferences.
 *
 * @param array $credential
 * @return bool
 */
function domain_audit_export_open_connection($credential)
{
    global $db, $SysPrefs, $domain_audit_export_identity_active;

    $connection = array(
        'host' => $credential['database_host'],
        'port' => $credential['database_port'],
        'dbname' => $credential['database_name'],
        'dbuser' => $credential['database_user'],
        'dbpassword' => $credential['database_password'],
    );
    $candidate = false;
    if (function_exists('db_pdo_create_connection')) {
        $candidate = db_pdo_create_connection($connection, true);
    } elseif (function_exists('mysqli_connect')) {
        $candidate = @mysqli_connect(
            $connection['host'], $connection['dbuser'], $connection['dbpassword'],
            $connection['dbname'], (int)$connection['port']
        );
    } elseif (function_exists('mysql_connect')) {
        $candidate = @mysql_connect(
            $connection['host'].':'.$connection['port'],
            $connection['dbuser'], $connection['dbpassword'], true
        );
        if ($candidate && !@mysql_select_db($connection['dbname'], $candidate)) {
            @mysql_close($candidate);
            $candidate = false;
        }
    }
    if (!$candidate) {
        return false;
    }

    $db = $candidate;
    $domain_audit_export_identity_active = true;
    $SysPrefs->sql_trail = false;
    $SysPrefs->select_trail = false;
    $SysPrefs->show_sql = false;
    $SysPrefs->go_debug = 0;
    $encoding = isset($_SESSION['language']->encoding)
        ? get_mysql_encoding_name($_SESSION['language']->encoding) : 'utf8';
    $mode_ok = false;
    $encoding_ok = false;
    if (function_exists('db_pdo_apply_sql_mode') && $db instanceof PDO) {
        $mode_ok = db_pdo_apply_sql_mode($db);
        $encoding_ok = $encoding ? db_pdo_apply_charset($db, $_SESSION['language']->encoding) : false;
    } elseif (function_exists('mysqli_query') && $db instanceof mysqli) {
        $mode_ok = @mysqli_query($db, "SET sql_mode='STRICT_ALL_TABLES'") !== false;
        $encoding_ok = $encoding ? @mysqli_set_charset($db, $encoding) : false;
    } elseif (function_exists('mysql_query')) {
        $mode_ok = @mysql_query("SET sql_mode='STRICT_ALL_TABLES'", $db) !== false;
        $encoding_ok = $encoding && function_exists('mysql_set_charset')
            ? @mysql_set_charset($encoding, $db) : false;
    }
    if (!$mode_ok || !$encoding_ok) {
        if ($db instanceof PDO) {
            $db = null;
        } elseif (function_exists('mysqli_close') && $db instanceof mysqli) {
            @mysqli_close($db);
        } elseif (function_exists('mysql_close')) {
            @mysql_close($db);
        }
        $domain_audit_export_identity_active = false;
        return false;
    }
    return true;
}

/**
 * Convert CURRENT_USER() into information_schema GRANTEE notation.
 *
 * @param string $current_user
 * @return string|false
 */
function domain_audit_export_grantee($current_user)
{
    $separator = strrpos((string)$current_user, '@');
    if ($separator === false) {
        return false;
    }
    $user = substr($current_user, 0, $separator);
    $host = substr($current_user, $separator + 1);
    if ($user === '' || $host === '') {
        return false;
    }
    return "'".str_replace("'", "''", $user)."'@'".str_replace("'", "''", $host)."'";
}

/**
 * Record a normalized fail-closed identity-verification outcome.
 *
 * The CLI intentionally returns only export_identity_rejected. This internal
 * code supports configured acceptance and server-side diagnosis without
 * exposing account names, grants or credentials.
 *
 * @param string $code
 * @return bool
 */
function domain_audit_export_identity_failure($code)
{
    $GLOBALS['domain_audit_export_identity_result'] = (string)$code;
    return false;
}

/**
 * Verify the connected identity has exactly the approved five-table matrix.
 *
 * @param int $company
 * @param array $credential
 * @return bool
 */
function domain_audit_verify_export_identity($company, $credential)
{
    global $db_connections;

    $GLOBALS['domain_audit_export_identity_result'] = 'verification_started';
    $company = (int)$company;
    $identity_result = db_query('SELECT CURRENT_USER() AS connected_account, DATABASE() AS connected_database');
    $identity = $identity_result ? db_fetch_assoc($identity_result) : false;
    $expected_identity = $credential['database_user'].'@'.$credential['database_account_host'];
    if (!$identity) {
        $error_number = function_exists('db_error_no') ? (int)db_error_no() : 0;
        return domain_audit_export_identity_failure('identity_query_failed_'.$error_number);
    }
    if ($identity['connected_account'] !== $expected_identity
        || $identity['connected_database'] !== $credential['database_name']) {
        return domain_audit_export_identity_failure('identity_mismatch');
    }
    $grantee = domain_audit_export_grantee($identity['connected_account']);
    if ($grantee === false) {
        return domain_audit_export_identity_failure('identity_format_invalid');
    }

    $grant_result = db_query('SHOW GRANTS FOR CURRENT_USER()');
    if (!$grant_result) {
        return domain_audit_export_identity_failure('grant_query_failed');
    }
    while ($grant = db_fetch_row($grant_result)) {
        $line = strtoupper((string)$grant[0]);
        if (strpos($line, ' WITH GRANT OPTION') !== false
            || strpos($line, ' ON ') === false
            || strpos($line, ' ON PROCEDURE ') !== false
            || strpos($line, ' ON FUNCTION ') !== false
        ) {
            return domain_audit_export_identity_failure('grant_surface_invalid');
        }
    }

    $grantee_sql = domain_audit_db_string($grantee);
    foreach (array('SCHEMA_PRIVILEGES', 'COLUMN_PRIVILEGES') as $view) {
        $result = db_query('SELECT COUNT(*) AS privilege_count FROM information_schema.'.$view
            .' WHERE GRANTEE='.$grantee_sql);
        $row = $result ? db_fetch_assoc($result) : false;
        if (!$row || (int)$row['privilege_count'] !== 0) {
            return domain_audit_export_identity_failure(strtolower($view).'_present');
        }
    }
    $global_result = db_query('SELECT PRIVILEGE_TYPE FROM information_schema.USER_PRIVILEGES'
        .' WHERE GRANTEE='.$grantee_sql." AND PRIVILEGE_TYPE<>'USAGE'");
    if (!$global_result || db_fetch_assoc($global_result)) {
        return domain_audit_export_identity_failure('global_privilege_present');
    }

    $prefix = (string)$db_connections[$company]['tbpref'];
    $expected = array(
        $prefix.'domain_audit_chain_state' => array('SELECT'),
        $prefix.'domain_audit_events' => array('SELECT'),
        $prefix.'domain_audit_outbox' => array('SELECT'),
        $prefix.'domain_audit_checkpoints' => array('SELECT'),
        $prefix.'domain_audit_export_receipts' => array('INSERT', 'SELECT'),
    );
    $actual = array();
    $table_result = db_query('SELECT TABLE_SCHEMA,TABLE_NAME,PRIVILEGE_TYPE'
        .' FROM information_schema.TABLE_PRIVILEGES WHERE GRANTEE='.$grantee_sql
        .' ORDER BY TABLE_SCHEMA,TABLE_NAME,PRIVILEGE_TYPE');
    if (!$table_result) {
        return domain_audit_export_identity_failure('table_privilege_query_failed');
    }
    while ($privilege = db_fetch_assoc($table_result)) {
        if ($privilege['TABLE_SCHEMA'] !== $credential['database_name']) {
            return domain_audit_export_identity_failure('cross_database_privilege_present');
        }
        $table = $privilege['TABLE_NAME'];
        if (!isset($actual[$table])) {
            $actual[$table] = array();
        }
        $actual[$table][] = strtoupper($privilege['PRIVILEGE_TYPE']);
    }
    foreach ($actual as &$privileges) {
        sort($privileges);
    }
    unset($privileges);
    ksort($actual);
    ksort($expected);
    if ($actual !== $expected) {
        return domain_audit_export_identity_failure('table_privilege_matrix_mismatch');
    }
    $GLOBALS['domain_audit_export_identity_result'] = 'verified';
    return true;
}

/**
 * Activate and verify the least-privilege custody-export identity.
 *
 * @param int $company
 * @return bool
 */
function domain_audit_activate_export_identity($company)
{
    global $db, $domain_audit_export_identity_active;

    $credential = domain_audit_export_credential($company);
    if ($credential === false) {
        return domain_audit_export_identity_failure('credential_rejected');
    }
    if (!domain_audit_export_open_connection($credential)) {
        return domain_audit_export_identity_failure('connection_failed');
    }
    $verified = domain_audit_verify_export_identity($company, $credential);
    $credential['database_password'] = str_repeat("\0", strlen($credential['database_password']));
    unset($credential);
    if (!$verified) {
        if ($db instanceof PDO) {
            $db = null;
        } elseif (function_exists('mysqli_close') && $db instanceof mysqli) {
            @mysqli_close($db);
            $db = false;
        } elseif (function_exists('mysql_close')) {
            @mysql_close($db);
            $db = false;
        }
        $domain_audit_export_identity_active = false;
    }
    return $verified;
}

/**
 * Return the explicitly configured maximum age of an unreceipted event.
 *
 * @param int $company
 * @return int|false
 */
function domain_audit_custody_max_delay_seconds($company)
{
    global $db_connections;

    $company = (int)$company;
    if (defined('PAYROLL_AUDIT_CUSTODY_MAX_DELAY_SECONDS')) {
        $configured = PAYROLL_AUDIT_CUSTODY_MAX_DELAY_SECONDS;
    } elseif (isset($db_connections[$company]['payroll_audit_custody_max_delay_seconds'])) {
        $configured = $db_connections[$company]['payroll_audit_custody_max_delay_seconds'];
    } else {
        return false;
    }
    $text = (string)$configured;
    if (preg_match('/^\d+$/', $text) !== 1) {
        return false;
    }
    $seconds = (int)$text;
    if ($seconds < DOMAIN_AUDIT_CUSTODY_MIN_DELAY_SECONDS
        || $seconds > DOMAIN_AUDIT_CUSTODY_MAX_DELAY_SECONDS
    ) {
        return false;
    }
    return $seconds;
}

/**
 * Return the configured, pinned custody public-key path.
 *
 * The corresponding private key belongs to the independent custodian and
 * must never be configured in or made readable by NotrinosERP.
 *
 * @param int $company
 * @return string|false
 */
function domain_audit_custody_public_key_path($company)
{
    global $db_connections;

    $company = (int)$company;
    if (defined('PAYROLL_AUDIT_CUSTODY_PUBLIC_KEY_FILE')) {
        $path = (string)PAYROLL_AUDIT_CUSTODY_PUBLIC_KEY_FILE;
    } elseif (isset($db_connections[$company]['payroll_audit_custody_public_key_file'])) {
        $path = (string)$db_connections[$company]['payroll_audit_custody_public_key_file'];
    } else {
        return false;
    }
    if ($path === '' || !preg_match('/^(?:[A-Za-z]:[\\\\\/]|\/)/', $path)
        || is_link($path) || !is_file($path) || filesize($path) > 32768
    ) {
        return false;
    }
    $resolved = realpath($path);
    $application = realpath($GLOBALS['path_to_root']);
    $private_root = domain_audit_private_root($company);
    if ($resolved === false || $application === false || $private_root === false) {
        return false;
    }
    $resolved_normal = str_replace('\\', '/', $resolved);
    $application_normal = rtrim(str_replace('\\', '/', $application), '/');
    $private_normal = rtrim(str_replace('\\', '/', $private_root), '/');
    if ($resolved_normal === $application_normal
        || strpos($resolved_normal, $application_normal.'/') === 0
        || $resolved_normal === $private_normal
        || strpos($resolved_normal, $private_normal.'/') === 0
    ) {
        return false;
    }
    return $resolved;
}

/**
 * Load and validate the pinned RSA custody verification key.
 *
 * @param int $company
 * @return array|false Array containing PEM, resource and stable key id.
 */
function domain_audit_custody_public_key($company)
{
    if (!function_exists('openssl_pkey_get_public') || !function_exists('openssl_pkey_get_details')) {
        return false;
    }
    $path = domain_audit_custody_public_key_path($company);
    if ($path === false) {
        return false;
    }
    $pem = @file_get_contents($path);
    $resource = is_string($pem) ? @openssl_pkey_get_public($pem) : false;
    $details = $resource !== false ? @openssl_pkey_get_details($resource) : false;
    if (!is_array($details) || !isset($details['type']) || $details['type'] !== OPENSSL_KEYTYPE_RSA
        || !isset($details['bits']) || (int)$details['bits'] < 2048 || empty($details['key'])
    ) {
        return false;
    }
    $normalized = preg_replace('/\s+/', '', (string)$details['key']);
    return array(
        'pem' => (string)$details['key'],
        'resource' => $resource,
        'key_id' => hash('sha256', $normalized),
    );
}

/**
 * Return the last accepted custody receipt for a chain.
 *
 * @param string $chain_scope
 * @param bool $lock
 * @return array|false|null
 */
function domain_audit_last_custody_receipt($chain_scope, $lock = false)
{
    if (!preg_match('/^company:\d+:legal_entity:\d+$/', (string)$chain_scope)) {
        return false;
    }
    $sql = "SELECT * FROM ".TB_PREF."domain_audit_export_receipts"
        ." WHERE chain_scope=".domain_audit_db_string($chain_scope)
        ." ORDER BY sequence_end DESC LIMIT 1".($lock ? ' FOR UPDATE' : '');
    $result = db_query($sql, 'could not inspect domain audit custody receipts');
    if ($result === false) {
        return false;
    }
    $row = db_fetch_assoc($result);
    return $row ? $row : null;
}

/**
 * Build a deterministic export package from a contiguous outbox range.
 *
 * @param int $company
 * @param int $legal_entity
 * @param int $sequence_start
 * @param int $sequence_end
 * @param string $previous_receipt_hash
 * @return array|false
 */
function domain_audit_build_export_package($company, $legal_entity, $sequence_start, $sequence_end, $previous_receipt_hash)
{
    $company = (int)$company;
    $legal_entity = (int)$legal_entity;
    $sequence_start = (int)$sequence_start;
    $sequence_end = (int)$sequence_end;
    $count = $sequence_end - $sequence_start + 1;
    if ($company < 0 || $legal_entity < 0 || $sequence_start < 1 || $sequence_end < $sequence_start
        || $count > DOMAIN_AUDIT_EXPORT_MAX_EVENTS
        || !preg_match('/^[a-f0-9]{64}$/', (string)$previous_receipt_hash)
    ) {
        return false;
    }
    $verification = verify_domain_audit_chain($company, $legal_entity);
    if (empty($verification['ok']) || (int)$verification['last_sequence'] < $sequence_end) {
        return false;
    }
    $chain_scope = 'company:'.$company.':legal_entity:'.$legal_entity;
    $result = db_query("SELECT event_id,chain_sequence,payload,payload_hash FROM "
        .TB_PREF."domain_audit_outbox WHERE chain_scope=".domain_audit_db_string($chain_scope)
        ." AND chain_sequence BETWEEN ".$sequence_start." AND ".$sequence_end
        ." ORDER BY chain_sequence", 'could not read domain audit export range');
    if ($result === false) {
        return false;
    }
    $records = array();
    $expected = $sequence_start;
    while ($row = db_fetch_assoc($result)) {
        if ((int)$row['chain_sequence'] !== $expected
            || !preg_match('/^[a-f0-9]{64}$/', (string)$row['payload_hash'])
            || !domain_audit_hash_equals((string)$row['payload_hash'], hash('sha256', (string)$row['payload']))
        ) {
            return false;
        }
        $records[] = array(
            'event_id' => (int)$row['event_id'],
            'chain_sequence' => $expected,
            'payload' => (string)$row['payload'],
            'payload_hash' => (string)$row['payload_hash'],
        );
        $expected++;
    }
    if (count($records) !== $count) {
        return false;
    }
    $checkpoint_result = db_query("SELECT chain_scope,chain_sequence,event_hash,event_count,key_id,signature,created_at"
        ." FROM ".TB_PREF."domain_audit_checkpoints WHERE chain_scope=".domain_audit_db_string($chain_scope)
        ." AND chain_sequence=".$sequence_end." LIMIT 1", 'could not read domain audit export checkpoint');
    $checkpoint = $checkpoint_result ? db_fetch_assoc($checkpoint_result) : false;
    $key = domain_audit_signing_key($company);
    if (!$checkpoint || $key === false || !domain_audit_verify_checkpoint($checkpoint, $key)) {
        return false;
    }
    $package = array(
        'schema_version' => 'notrinoserp.domain-audit-export.v1',
        'company_id' => $company,
        'legal_entity_id' => $legal_entity,
        'chain_scope' => $chain_scope,
        'sequence_start' => $sequence_start,
        'sequence_end' => $sequence_end,
        'event_count' => $count,
        'previous_receipt_hash' => (string)$previous_receipt_hash,
        'outbox_records' => $records,
        'checkpoint' => array(
            'chain_sequence' => (int)$checkpoint['chain_sequence'],
            'event_hash' => (string)$checkpoint['event_hash'],
            'event_count' => (int)$checkpoint['event_count'],
            'key_id' => (string)$checkpoint['key_id'],
            'signature' => (string)$checkpoint['signature'],
            'created_at' => (string)$checkpoint['created_at'],
        ),
    );
    $json = domain_audit_canonical_json($package);
    if ($json === false || strlen($json) > DOMAIN_AUDIT_EXPORT_MAX_BYTES) {
        return false;
    }
    return array('package' => $package, 'json' => $json, 'package_hash' => hash('sha256', $json));
}

/**
 * Build the next unreceipted export package without changing database state.
 *
 * @param int $company
 * @param int $legal_entity
 * @param int $maximum_events
 * @return array|false
 */
function domain_audit_prepare_next_export($company, $legal_entity = 0, $maximum_events = DOMAIN_AUDIT_EXPORT_MAX_EVENTS)
{
    $company = (int)$company;
    $legal_entity = (int)$legal_entity;
    $maximum_events = (int)$maximum_events;
    if ($maximum_events < 1 || $maximum_events > DOMAIN_AUDIT_EXPORT_MAX_EVENTS) {
        return false;
    }
    $verification = verify_domain_audit_chain($company, $legal_entity);
    if (empty($verification['ok'])) {
        return false;
    }
    $chain_scope = 'company:'.$company.':legal_entity:'.$legal_entity;
    $last = domain_audit_last_custody_receipt($chain_scope, false);
    if ($last === false) {
        return false;
    }
    $start = $last === null ? 1 : (int)$last['sequence_end'] + 1;
    $previous_hash = $last === null ? str_repeat('0', 64) : (string)$last['receipt_hash'];
    $head = (int)$verification['last_sequence'];
    if ($start > $head) {
        return array('complete' => true, 'last_sequence' => $head, 'last_receipt_hash' => $previous_hash);
    }
    $end = min($head, $start + $maximum_events - 1);
    return domain_audit_build_export_package($company, $legal_entity, $start, $end, $previous_hash);
}

/**
 * Write a prepared package to an exclusive, private staging file.
 *
 * The staging location is not WORM custody. An independent custodian must
 * ingest these exact bytes and return its signed receipt.
 *
 * @param string $path
 * @param string $json
 * @param int $company
 * @return bool
 */
function domain_audit_write_staging_package($path, $json, $company)
{
    global $path_to_root;

    if (!is_string($json) || $json === '' || strlen($json) > DOMAIN_AUDIT_EXPORT_MAX_BYTES
        || !is_string($path) || !preg_match('/^(?:[A-Za-z]:[\\\\\/]|\/)/', $path)
        || file_exists($path) || is_link($path)
    ) {
        return false;
    }
    $parent = realpath(dirname($path));
    $application = realpath($path_to_root);
    $private_root = domain_audit_private_root((int)$company);
    $private_resolved = $private_root === false ? false : realpath($private_root);
    if ($parent === false || $application === false || is_link(dirname($path))) {
        return false;
    }
    $parent_normal = rtrim(str_replace('\\', '/', $parent), '/');
    $application_normal = rtrim(str_replace('\\', '/', $application), '/');
    $private_normal = $private_resolved === false ? '' : rtrim(str_replace('\\', '/', $private_resolved), '/');
    if ($parent_normal === $application_normal || strpos($parent_normal.'/', $application_normal.'/') === 0
        || ($private_normal !== '' && ($parent_normal === $private_normal
            || strpos($parent_normal.'/', $private_normal.'/') === 0))
    ) {
        return false;
    }
    $bytes = $json."\n";
    $handle = @fopen($path, 'x+b');
    if ($handle === false) {
        return false;
    }
    $written = fwrite($handle, $bytes);
    $flushed = fflush($handle);
    fclose($handle);
    @chmod($path, 0600);
    if ($written !== strlen($bytes) || !$flushed
        || !domain_audit_hash_equals(hash('sha256', $json), hash('sha256', rtrim((string)@file_get_contents($path), "\r\n")))
    ) {
        @unlink($path);
        return false;
    }
    return true;
}

/**
 * Validate and cryptographically authorize one external custody receipt.
 *
 * @param array $envelope
 * @param int $company
 * @param array $public_key
 * @return array|false Canonical receipt fields plus hashes.
 */
function domain_audit_verify_custody_receipt_envelope($envelope, $company, $public_key)
{
    if (!is_array($envelope) || !isset($envelope['receipt']) || !is_array($envelope['receipt'])
        || !isset($envelope['signature']) || !is_string($envelope['signature'])
        || !is_array($public_key) || empty($public_key['resource']) || empty($public_key['key_id'])
    ) {
        return false;
    }
    $receipt = $envelope['receipt'];
    $required = array(
        'schema_version', 'company_id', 'legal_entity_id', 'chain_scope', 'sequence_start',
        'sequence_end', 'event_count', 'package_hash', 'checkpoint_event_hash',
        'checkpoint_signature', 'previous_receipt_hash', 'custody_provider',
        'custody_object_token', 'custody_mode', 'received_at', 'retained_until', 'custody_key_id',
    );
    $actual_fields = array_keys($receipt);
    $expected_fields = $required;
    sort($actual_fields, SORT_STRING);
    sort($expected_fields, SORT_STRING);
    if ($actual_fields !== $expected_fields
        || (string)$receipt['schema_version'] !== 'notrinoserp.domain-audit-custody-receipt.v1'
        || (int)$receipt['company_id'] !== (int)$company
        || (int)$receipt['legal_entity_id'] < 0
        || (string)$receipt['chain_scope'] !== 'company:'.(int)$company.':legal_entity:'.(int)$receipt['legal_entity_id']
        || (int)$receipt['sequence_start'] < 1
        || (int)$receipt['sequence_end'] < (int)$receipt['sequence_start']
        || (int)$receipt['event_count'] !== (int)$receipt['sequence_end'] - (int)$receipt['sequence_start'] + 1
        || (int)$receipt['event_count'] > DOMAIN_AUDIT_EXPORT_MAX_EVENTS
        || !preg_match('/^[a-f0-9]{64}$/', (string)$receipt['package_hash'])
        || !preg_match('/^[a-f0-9]{64}$/', (string)$receipt['checkpoint_event_hash'])
        || !preg_match('/^[a-f0-9]{64}$/', (string)$receipt['checkpoint_signature'])
        || !preg_match('/^[a-f0-9]{64}$/', (string)$receipt['previous_receipt_hash'])
        || !preg_match('/^[a-z][a-z0-9_.:-]{1,63}$/', (string)$receipt['custody_provider'])
        || !preg_match('/^[a-f0-9]{64}$/', (string)$receipt['custody_object_token'])
        || (string)$receipt['custody_mode'] !== 'immutable_worm'
        || !preg_match('/^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z$/', (string)$receipt['received_at'])
        || !preg_match('/^\d{4}-\d{2}-\d{2}$/', (string)$receipt['retained_until'])
        || !domain_audit_hash_equals((string)$receipt['custody_key_id'], (string)$public_key['key_id'])
    ) {
        return false;
    }
    $received = strtotime((string)$receipt['received_at']);
    $retained = strtotime((string)$receipt['retained_until'].'T23:59:59Z');
    if ($received === false || $retained === false || $retained < $received) {
        return false;
    }
    $payload = domain_audit_canonical_json($receipt);
    $signature = base64_decode($envelope['signature'], true);
    if ($payload === false || $signature === false || strlen($signature) < 256 || strlen($signature) > 1024
        || @openssl_verify($payload, $signature, $public_key['resource'], OPENSSL_ALGO_SHA256) !== 1
    ) {
        return false;
    }
    return array(
        'receipt' => $receipt,
        'payload' => $payload,
        'signature' => $envelope['signature'],
        'receipt_hash' => hash('sha256', $payload."\n".$envelope['signature']),
    );
}

/**
 * Accept one independently signed receipt as append-only database evidence.
 *
 * Exact duplicate retries are idempotent. Out-of-order, overlapping, stale,
 * forged, or database-mismatched receipts fail closed.
 *
 * @param array $envelope
 * @param int $company
 * @return array|false
 */
function record_domain_audit_custody_receipt($envelope, $company)
{
    $company = (int)$company;
    $public_key = domain_audit_custody_public_key($company);
    $verified_receipt = $public_key === false ? false
        : domain_audit_verify_custody_receipt_envelope($envelope, $company, $public_key);
    if ($verified_receipt === false) {
        return false;
    }
    $receipt = $verified_receipt['receipt'];
    begin_transaction();
    $duplicate_result = db_query("SELECT receipt_id,receipt_hash FROM ".TB_PREF."domain_audit_export_receipts"
        ." WHERE receipt_hash=".domain_audit_db_string($verified_receipt['receipt_hash'])." LIMIT 1",
        'could not inspect duplicate domain audit custody receipt');
    $duplicate = $duplicate_result ? db_fetch_assoc($duplicate_result) : false;
    if ($duplicate) {
        commit_transaction();
        return array('receipt_id' => (int)$duplicate['receipt_id'], 'receipt_hash' => (string)$duplicate['receipt_hash'], 'idempotent' => true);
    }
    $last = domain_audit_last_custody_receipt($receipt['chain_scope'], true);
    if ($last === false) {
        cancel_transaction();
        return false;
    }
    $expected_start = $last === null ? 1 : (int)$last['sequence_end'] + 1;
    $previous_hash = $last === null ? str_repeat('0', 64) : (string)$last['receipt_hash'];
    if ((int)$receipt['sequence_start'] !== $expected_start
        || !domain_audit_hash_equals((string)$receipt['previous_receipt_hash'], $previous_hash)
    ) {
        cancel_transaction();
        return false;
    }
    $package = domain_audit_build_export_package(
        $company,
        (int)$receipt['legal_entity_id'],
        (int)$receipt['sequence_start'],
        (int)$receipt['sequence_end'],
        $previous_hash
    );
    if ($package === false
        || !domain_audit_hash_equals((string)$receipt['package_hash'], $package['package_hash'])
        || !domain_audit_hash_equals((string)$receipt['checkpoint_event_hash'], $package['package']['checkpoint']['event_hash'])
        || !domain_audit_hash_equals((string)$receipt['checkpoint_signature'], $package['package']['checkpoint']['signature'])
    ) {
        cancel_transaction();
        return false;
    }
    $received_at = str_replace(array('T', 'Z'), array(' ', ''), (string)$receipt['received_at']);
    $sql = "INSERT INTO ".TB_PREF."domain_audit_export_receipts"
        ." (company_id,chain_scope,sequence_start,sequence_end,event_count,package_hash,checkpoint_event_hash,"
        ."previous_receipt_hash,custody_provider,custody_object_token,custody_key_id,received_at,retained_until,"
        ."receipt_payload,receipt_signature,receipt_hash,recorded_at) VALUES ("
        .$company.",".domain_audit_db_string($receipt['chain_scope']).",".(int)$receipt['sequence_start'].",".(int)$receipt['sequence_end'].",".(int)$receipt['event_count'].","
        .domain_audit_db_string($receipt['package_hash']).",".domain_audit_db_string($receipt['checkpoint_event_hash']).","
        .domain_audit_db_string($previous_hash).",".domain_audit_db_string($receipt['custody_provider']).","
        .domain_audit_db_string($receipt['custody_object_token']).",".domain_audit_db_string($receipt['custody_key_id']).","
        .domain_audit_db_string($received_at).",".domain_audit_db_string($receipt['retained_until']).","
        .domain_audit_db_string($verified_receipt['payload']).",".domain_audit_db_string($verified_receipt['signature']).","
        .domain_audit_db_string($verified_receipt['receipt_hash']).",CURRENT_TIMESTAMP)";
    if (!db_query($sql, 'could not append domain audit custody receipt')) {
        cancel_transaction();
        return false;
    }
    $receipt_id = (int)db_insert_id();
    commit_transaction();
    return array('receipt_id' => $receipt_id, 'receipt_hash' => $verified_receipt['receipt_hash'], 'idempotent' => false);
}

/**
 * Reconcile every accepted custody receipt against its pinned signature and
 * the current immutable outbox/checkpoint source.
 *
 * @param int $company
 * @param int $legal_entity
 * @return array
 */
function verify_domain_audit_custody_receipts($company, $legal_entity = 0)
{
    $company = (int)$company;
    $legal_entity = (int)$legal_entity;
    $public_key = domain_audit_custody_public_key($company);
    if ($public_key === false) {
        return array('ok' => false, 'error' => 'custody_public_key_unavailable');
    }
    $chain_scope = 'company:'.$company.':legal_entity:'.$legal_entity;
    $result = db_query("SELECT * FROM ".TB_PREF."domain_audit_export_receipts WHERE chain_scope="
        .domain_audit_db_string($chain_scope)." ORDER BY sequence_start", 'could not reconcile domain audit custody receipts');
    if ($result === false) {
        return array('ok' => false, 'error' => 'custody_receipt_read_failed');
    }
    $expected_start = 1;
    $previous_hash = str_repeat('0', 64);
    $count = 0;
    while ($row = db_fetch_assoc($result)) {
        $envelope = array(
            'receipt' => json_decode((string)$row['receipt_payload'], true),
            'signature' => (string)$row['receipt_signature'],
        );
        $verified = domain_audit_verify_custody_receipt_envelope($envelope, $company, $public_key);
        if ($verified === false || (int)$row['sequence_start'] !== $expected_start
            || !domain_audit_hash_equals((string)$row['receipt_hash'], $verified['receipt_hash'])
            || !domain_audit_hash_equals((string)$row['previous_receipt_hash'], $previous_hash)
        ) {
            return array('ok' => false, 'error' => 'custody_receipt_verification_failed', 'receipt' => $count + 1);
        }
        $package = domain_audit_build_export_package(
            $company, $legal_entity, (int)$row['sequence_start'], (int)$row['sequence_end'], $previous_hash
        );
        if ($package === false || !domain_audit_hash_equals($package['package_hash'], (string)$row['package_hash'])) {
            return array('ok' => false, 'error' => 'custody_package_mismatch', 'receipt' => $count + 1);
        }
        $expected_start = (int)$row['sequence_end'] + 1;
        $previous_hash = (string)$row['receipt_hash'];
        $count++;
    }
    $chain = verify_domain_audit_chain($company, $legal_entity);
    if (empty($chain['ok'])) {
        return $chain;
    }
    return array(
        'ok' => true,
        'receipt_count' => $count,
        'last_receipted_sequence' => $expected_start - 1,
        'chain_head_sequence' => (int)$chain['last_sequence'],
        'pending_count' => (int)$chain['last_sequence'] - ($expected_start - 1),
        'last_receipt_hash' => $previous_hash,
    );
}

/**
 * Parse the database's UTC custody timestamp without inheriting local timezone.
 *
 * @param string $value
 * @return int|false
 */
function domain_audit_custody_utc_timestamp($value)
{
    if ((is_int($value) || (is_string($value) && preg_match('/^\d+$/', $value) === 1))
        && (int)$value > 0
    ) {
        return (int)$value;
    }
    $value = (string)$value;
    if (preg_match('/^\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}$/', $value) !== 1) {
        return false;
    }
    $timestamp = strtotime($value.' UTC');
    if ($timestamp === false || gmdate('Y-m-d H:i:s', $timestamp) !== $value) {
        return false;
    }
    return $timestamp;
}

/**
 * Evaluate custody freshness using already reconciled, non-sensitive facts.
 *
 * @param array $verification
 * @param string|null $oldest_pending_created_at
 * @param int|false $maximum_delay_seconds
 * @param int|null $now_timestamp
 * @return array
 */
function domain_audit_evaluate_custody_status($verification, $oldest_pending_created_at,
    $maximum_delay_seconds, $now_timestamp = null)
{
    if (!is_array($verification) || empty($verification['ok'])) {
        return array('ok' => false, 'status' => 'verification_failed', 'error' => 'custody_verification_failed');
    }
    if ($maximum_delay_seconds === false
        || (int)$maximum_delay_seconds < DOMAIN_AUDIT_CUSTODY_MIN_DELAY_SECONDS
        || (int)$maximum_delay_seconds > DOMAIN_AUDIT_CUSTODY_MAX_DELAY_SECONDS
    ) {
        return array('ok' => false, 'status' => 'configuration_error', 'error' => 'custody_max_delay_unavailable');
    }
    if (!isset($verification['pending_count']) || !isset($verification['last_receipted_sequence'])
        || !isset($verification['chain_head_sequence'])
    ) {
        return array('ok' => false, 'status' => 'verification_failed', 'error' => 'custody_verification_incomplete');
    }
    $pending = (int)$verification['pending_count'];
    $last_receipted = (int)$verification['last_receipted_sequence'];
    $chain_head = (int)$verification['chain_head_sequence'];
    if ($pending < 0 || $last_receipted < 0 || $chain_head < $last_receipted
        || $pending !== $chain_head - $last_receipted
    ) {
        return array('ok' => false, 'status' => 'verification_failed', 'error' => 'custody_verification_inconsistent');
    }
    $result = array(
        'ok' => true,
        'status' => 'current',
        'pending_count' => $pending,
        'last_receipted_sequence' => $last_receipted,
        'chain_head_sequence' => $chain_head,
        'oldest_pending_age_seconds' => 0,
        'maximum_delay_seconds' => (int)$maximum_delay_seconds,
    );
    if ($pending === 0) {
        return $result;
    }
    $oldest = domain_audit_custody_utc_timestamp($oldest_pending_created_at);
    $now = $now_timestamp === null ? time() : (int)$now_timestamp;
    if ($oldest === false || $now < 1) {
        return array('ok' => false, 'status' => 'verification_failed', 'error' => 'custody_pending_timestamp_invalid');
    }
    if ($oldest > $now) {
        return array('ok' => false, 'status' => 'clock_skew', 'error' => 'custody_clock_skew');
    }
    $result['oldest_pending_age_seconds'] = $now - $oldest;
    if ($result['oldest_pending_age_seconds'] > (int)$maximum_delay_seconds) {
        $result['ok'] = false;
        $result['status'] = 'overdue';
        $result['error'] = 'custody_delivery_overdue';
    } else {
        $result['status'] = 'pending';
    }
    return $result;
}

/**
 * Emit only normalized custody-monitor metadata to the server log.
 *
 * @param int $company
 * @param int $legal_entity
 * @param array $status
 * @return void
 */
function domain_audit_log_custody_monitor($company, $legal_entity, $status)
{
    $outcome = isset($status['status']) ? (string)$status['status'] : 'monitor_error';
    if (preg_match('/^[a-z_]{2,40}$/', $outcome) !== 1) {
        $outcome = 'monitor_error';
    }
    $record = array(
        'event' => 'domain_audit_custody_monitor',
        'company_id' => max(0, (int)$company),
        'legal_entity_id' => max(0, (int)$legal_entity),
        'status' => $outcome,
        'pending_count' => isset($status['pending_count']) ? max(0, (int)$status['pending_count']) : 0,
        'oldest_pending_age_seconds' => isset($status['oldest_pending_age_seconds'])
            ? max(0, (int)$status['oldest_pending_age_seconds']) : 0,
        'maximum_delay_seconds' => isset($status['maximum_delay_seconds'])
            ? max(0, (int)$status['maximum_delay_seconds']) : 0,
    );
    error_log(domain_audit_canonical_json($record));
}

/**
 * Reconcile custody and report whether the oldest unreceipted event is overdue.
 *
 * This is read-only. Scheduling and escalation remain deployment controls.
 *
 * @param int $company
 * @param int $legal_entity
 * @param int|null $now_timestamp
 * @return array
 */
function domain_audit_custody_status($company, $legal_entity = 0, $now_timestamp = null)
{
    $company = (int)$company;
    $legal_entity = (int)$legal_entity;
    $verification = verify_domain_audit_custody_receipts($company, $legal_entity);
    $maximum = domain_audit_custody_max_delay_seconds($company);
    $oldest_pending = null;
    if (!empty($verification['ok']) && isset($verification['pending_count'])
        && (int)$verification['pending_count'] > 0
    ) {
        $chain_scope = 'company:'.$company.':legal_entity:'.$legal_entity;
        $result = db_query("SELECT UNIX_TIMESTAMP(MIN(created_at)) AS oldest_pending_timestamp FROM "
            .TB_PREF."domain_audit_outbox WHERE chain_scope=".domain_audit_db_string($chain_scope)
            ." AND chain_sequence > ".(int)$verification['last_receipted_sequence'],
            'could not inspect domain audit custody completeness');
        $row = $result ? db_fetch_assoc($result) : false;
        if (!$row || empty($row['oldest_pending_timestamp'])) {
            $verification = array('ok' => false, 'error' => 'custody_pending_read_failed');
        } else {
            $oldest_pending = (string)$row['oldest_pending_timestamp'];
        }
    }
    $status = domain_audit_evaluate_custody_status($verification, $oldest_pending, $maximum, $now_timestamp);
    domain_audit_log_custody_monitor($company, $legal_entity, $status);
    return $status;
}
