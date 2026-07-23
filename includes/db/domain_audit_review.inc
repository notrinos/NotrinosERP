<?php
/**
 * PAY-AUD-001 separately authorized, redacted audit review.
 */

include_once($path_to_root.'/includes/db/domain_audit_export.inc');

define('DOMAIN_AUDIT_REVIEW_CREDENTIAL_SCHEMA',
    'notrinoserp.domain-audit-review-credential.v1');
define('DOMAIN_AUDIT_REVIEW_MAX_ROWS', 500);

/**
 * Return the protected external review credential path.
 *
 * @param int $company
 * @return string|false
 */
function domain_audit_review_credential_file_path($company)
{
    global $db_connections, $path_to_root;

    $company = (int)$company;
    if (!isset($db_connections[$company]['payroll_audit_review_credential_file'])) {
        return false;
    }
    $path = trim((string)$db_connections[$company]['payroll_audit_review_credential_file']);
    if ($path === '' || !preg_match('/^(?:[A-Za-z]:[\\\\\/]|\/)/', $path)
        || is_link($path) || !is_file($path)
        || filesize($path) > DOMAIN_AUDIT_EXPORT_CREDENTIAL_MAX_BYTES
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
        if ($permissions === false || (($permissions & 0007) !== 0)
            || (($permissions & 0020) !== 0)
        ) {
            return false;
        }
    }
    return $resolved;
}

/**
 * Load a strict company-bound audit-review database credential.
 *
 * @param int $company
 * @return array|false
 */
function domain_audit_review_credential($company)
{
    global $db_connections;

    $company = (int)$company;
    if (!isset($db_connections[$company])) {
        return false;
    }
    $path = domain_audit_review_credential_file_path($company);
    $contents = $path !== false ? @file_get_contents($path) : false;
    $credential = is_string($contents) ? json_decode($contents, true) : null;
    $expected_keys = array(
        'company_id', 'database_account_host', 'database_host', 'database_name',
        'database_password', 'database_port', 'database_user', 'schema_version', 'table_prefix'
    );
    $actual_keys = is_array($credential) ? array_keys($credential) : array();
    sort($expected_keys);
    sort($actual_keys);
    if (!is_array($credential) || $actual_keys !== $expected_keys
        || $credential['schema_version'] !== DOMAIN_AUDIT_REVIEW_CREDENTIAL_SCHEMA
        || !is_int($credential['company_id']) || $credential['company_id'] !== $company
    ) {
        return false;
    }
    $connection = $db_connections[$company];
    $configured_port = empty($connection['port']) ? '3306' : (string)$connection['port'];
    if ((string)$credential['database_host'] !== (string)$connection['host']
        || (string)$credential['database_port'] !== $configured_port
        || (string)$credential['database_name'] !== (string)$connection['dbname']
        || (string)$credential['table_prefix'] !== (string)$connection['tbpref']
        || preg_match('/^[A-Za-z0-9_.-]{1,80}$/',
            (string)$credential['database_user']) !== 1
        || preg_match('/^[A-Za-z0-9.:-]{1,255}$/',
            (string)$credential['database_account_host']) !== 1
        || strlen((string)$credential['database_password']) < 24
        || strlen((string)$credential['database_password']) > 1024
        || preg_match('/[\x00-\x1F\x7F]/',
            (string)$credential['database_password']) === 1
    ) {
        return false;
    }
    return $credential;
}

/**
 * Record one normalized review-identity failure.
 *
 * @param string $code
 * @return bool
 */
function domain_audit_review_identity_failure($code)
{
    $GLOBALS['domain_audit_review_identity_result'] = (string)$code;
    return false;
}

/**
 * Verify that the current database identity can select only the review view.
 *
 * @param int $company
 * @param array $credential
 * @return bool
 */
function domain_audit_verify_review_identity($company, $credential)
{
    global $db_connections;

    $GLOBALS['domain_audit_review_identity_result'] = 'verification_started';
    $company = (int)$company;
    $identity_result = db_query(
        'SELECT CURRENT_USER() AS connected_account, DATABASE() AS connected_database'
    );
    $identity = $identity_result ? db_fetch_assoc($identity_result) : false;
    $expected_identity = $credential['database_user'].'@'
        .$credential['database_account_host'];
    if (!$identity || $identity['connected_account'] !== $expected_identity
        || $identity['connected_database'] !== $credential['database_name']
    ) {
        return domain_audit_review_identity_failure('identity_mismatch');
    }
    $grantee = domain_audit_export_grantee($identity['connected_account']);
    if ($grantee === false) {
        return domain_audit_review_identity_failure('identity_format_invalid');
    }
    $grant_result = db_query('SHOW GRANTS FOR CURRENT_USER()');
    if (!$grant_result) {
        return domain_audit_review_identity_failure('grant_query_failed');
    }
    while ($grant = db_fetch_row($grant_result)) {
        $line = strtoupper((string)$grant[0]);
        if (strpos($line, ' WITH GRANT OPTION') !== false
            || strpos($line, ' ON ') === false
            || strpos($line, ' ON PROCEDURE ') !== false
            || strpos($line, ' ON FUNCTION ') !== false
        ) {
            return domain_audit_review_identity_failure('grant_surface_invalid');
        }
    }
    $grantee_sql = domain_audit_db_string($grantee);
    foreach (array('SCHEMA_PRIVILEGES', 'COLUMN_PRIVILEGES') as $view) {
        $result = db_query(
            'SELECT COUNT(*) AS privilege_count FROM information_schema.'.$view
            .' WHERE GRANTEE='.$grantee_sql
        );
        $row = $result ? db_fetch_assoc($result) : false;
        if (!$row || (int)$row['privilege_count'] !== 0) {
            return domain_audit_review_identity_failure(
                strtolower($view).'_present'
            );
        }
    }
    $global = db_query(
        'SELECT PRIVILEGE_TYPE FROM information_schema.USER_PRIVILEGES'
        .' WHERE GRANTEE='.$grantee_sql." AND PRIVILEGE_TYPE<>'USAGE'"
    );
    if (!$global || db_fetch_assoc($global)) {
        return domain_audit_review_identity_failure('global_privilege_present');
    }
    $actual = array();
    $table_result = db_query(
        'SELECT TABLE_SCHEMA,TABLE_NAME,PRIVILEGE_TYPE'
        .' FROM information_schema.TABLE_PRIVILEGES WHERE GRANTEE='.$grantee_sql
        .' ORDER BY TABLE_SCHEMA,TABLE_NAME,PRIVILEGE_TYPE'
    );
    if (!$table_result) {
        return domain_audit_review_identity_failure('table_privilege_query_failed');
    }
    while ($privilege = db_fetch_assoc($table_result)) {
        if ($privilege['TABLE_SCHEMA'] !== $credential['database_name']) {
            return domain_audit_review_identity_failure(
                'cross_database_privilege_present'
            );
        }
        $actual[$privilege['TABLE_NAME']][] = strtoupper(
            $privilege['PRIVILEGE_TYPE']
        );
    }
    foreach ($actual as &$privileges) {
        sort($privileges);
    }
    unset($privileges);
    ksort($actual);
    $expected = array(
        (string)$db_connections[$company]['tbpref'].'domain_audit_review_v1'
            => array('SELECT'),
    );
    if ($actual !== $expected) {
        return domain_audit_review_identity_failure(
            'table_privilege_matrix_mismatch'
        );
    }
    $GLOBALS['domain_audit_review_identity_result'] = 'verified';
    return true;
}

/**
 * Activate the exact read-only audit-review database identity.
 *
 * @param int $company
 * @return bool
 */
function domain_audit_activate_review_identity($company)
{
    global $db, $domain_audit_export_identity_active;

    $credential = domain_audit_review_credential($company);
    if ($credential === false) {
        return domain_audit_review_identity_failure('credential_rejected');
    }
    if (!domain_audit_export_open_connection($credential)) {
        return domain_audit_review_identity_failure('connection_failed');
    }
    $verified = domain_audit_verify_review_identity($company, $credential);
    $credential['database_password'] = str_repeat(
        "\0", strlen($credential['database_password'])
    );
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
 * Authorize audit review through the configured dedicated OS identity.
 *
 * @param int $company
 * @return string|false
 */
function domain_audit_reviewer_authorized($company)
{
    global $db_connections;

    if (PHP_SAPI !== 'cli') {
        return false;
    }
    $expected = defined('PAYROLL_AUDIT_REVIEWER_OS_USER')
        ? (string)PAYROLL_AUDIT_REVIEWER_OS_USER
        : (isset($db_connections[$company]['payroll_audit_reviewer_os_user'])
            ? (string)$db_connections[$company]['payroll_audit_reviewer_os_user'] : '');
    $actual = domain_audit_key_admin_os_identity();
    if ($expected === '' || strlen($expected) > 128
        || preg_match('/^[A-Za-z0-9_.@\\\\-]+$/', $expected) !== 1
        || $actual === false
        || !domain_audit_hash_equals(strtolower($expected), strtolower($actual))
    ) {
        return false;
    }
    return $actual;
}

/**
 * Validate and normalize one redacted review filter.
 *
 * @param array $filter
 * @return array|false
 */
function domain_audit_review_filter($filter)
{
    if (!is_array($filter)
        || !isset($filter['company_id'], $filter['legal_entity_id'])
        || (int)$filter['company_id'] < 0 || (int)$filter['legal_entity_id'] < 0
    ) {
        return false;
    }
    $normalized = array(
        'company_id' => (int)$filter['company_id'],
        'legal_entity_id' => (int)$filter['legal_entity_id'],
        'sequence_start' => isset($filter['sequence_start'])
            ? (int)$filter['sequence_start'] : 1,
        'sequence_end' => isset($filter['sequence_end'])
            ? (int)$filter['sequence_end'] : 2147483647,
        'occurred_from' => isset($filter['occurred_from'])
            ? (string)$filter['occurred_from'] : '',
        'occurred_to' => isset($filter['occurred_to'])
            ? (string)$filter['occurred_to'] : '',
        'entity_type' => isset($filter['entity_type'])
            ? (string)$filter['entity_type'] : '',
        'action' => isset($filter['action']) ? (string)$filter['action'] : '',
        'reason_code' => isset($filter['reason_code'])
            ? (string)$filter['reason_code'] : '',
        'correlation_id' => isset($filter['correlation_id'])
            ? strtolower((string)$filter['correlation_id']) : '',
        'limit' => isset($filter['limit']) ? (int)$filter['limit'] : 100,
    );
    if ($normalized['sequence_start'] < 1
        || $normalized['sequence_end'] < $normalized['sequence_start']
        || $normalized['limit'] < 1
        || $normalized['limit'] > DOMAIN_AUDIT_REVIEW_MAX_ROWS
    ) {
        return false;
    }
    foreach (array('entity_type', 'action', 'reason_code') as $token) {
        if ($normalized[$token] !== ''
            && !domain_audit_valid_token($normalized[$token])
        ) {
            return false;
        }
    }
    foreach (array('occurred_from', 'occurred_to') as $timestamp) {
        if ($normalized[$timestamp] !== ''
            && !domain_audit_valid_utc_timestamp($normalized[$timestamp])
        ) {
            return false;
        }
    }
    if ($normalized['occurred_from'] !== '' && $normalized['occurred_to'] !== ''
        && $normalized['occurred_from'] > $normalized['occurred_to']
    ) {
        return false;
    }
    if ($normalized['correlation_id'] !== ''
        && preg_match('/^[a-f0-9]{32}$/',
            $normalized['correlation_id']) !== 1
    ) {
        return false;
    }
    return $normalized;
}

/**
 * Append immutable evidence of an authorized review request.
 *
 * @param array $filter
 * @param string $reason_code
 * @param string $reviewer
 * @return array|false
 */
function append_domain_audit_review_access($filter, $reason_code, $reviewer)
{
    $filter = domain_audit_review_filter($filter);
    if ($filter === false || !domain_audit_valid_token($reason_code)
        || !is_string($reviewer) || $reviewer === ''
    ) {
        return false;
    }
    $request_id = domain_audit_correlation_id();
    $diff = $filter;
    $diff['restricted_values_stored'] = false;
    return append_domain_audit_event(array(
        'company_id' => $filter['company_id'],
        'legal_entity_id' => $filter['legal_entity_id'],
        'entity_type' => 'domain_audit_review',
        'entity_key' => array('review_request_id' => $request_id),
        'action' => 'audit.review.performed',
        'reason_code' => $reason_code,
        'correlation_id' => $request_id,
        'security_context' => array(
            'actor_user_id' => 0,
            'company_id' => $filter['company_id'],
            'role_id' => 0,
            'role_set' => array(),
            'auth_method' => 'audit_review_cli',
            'os_actor' => $reviewer,
        ),
        'restricted_diff' => $diff,
    ));
}

/**
 * Search the fixed redacted review view with bounded filters.
 *
 * @param array $filter
 * @return array|false
 */
function domain_audit_review_search($filter)
{
    $filter = domain_audit_review_filter($filter);
    if ($filter === false) {
        return false;
    }
    $scope = 'company:'.$filter['company_id']
        .':legal_entity:'.$filter['legal_entity_id'];
    $where = array(
        'chain_scope='.domain_audit_db_string($scope),
        'company_id='.(int)$filter['company_id'],
        'legal_entity_id='.(int)$filter['legal_entity_id'],
        'chain_sequence>='.(int)$filter['sequence_start'],
        'chain_sequence<='.(int)$filter['sequence_end'],
    );
    foreach (array('entity_type', 'action', 'reason_code', 'correlation_id') as $name) {
        if ($filter[$name] !== '') {
            $where[] = $name.'='.domain_audit_db_string($filter[$name]);
        }
    }
    if ($filter['occurred_from'] !== '') {
        $where[] = 'occurred_at>='.domain_audit_db_string($filter['occurred_from']);
    }
    if ($filter['occurred_to'] !== '') {
        $where[] = 'occurred_at<='.domain_audit_db_string($filter['occurred_to']);
    }
    $result = db_query(
        "SELECT event_id,chain_scope,chain_sequence,occurred_at,actor_user_id,"
        ."company_id,legal_entity_id,entity_type,action,reason_code,correlation_id,"
        ."previous_event_hash,event_hash,key_id"
        ." FROM ".TB_PREF."domain_audit_review_v1"
        ." WHERE ".implode(' AND ', $where)
        ." ORDER BY chain_sequence LIMIT ".(int)$filter['limit'],
        'could not execute redacted domain audit review'
    );
    if (!$result) {
        return false;
    }
    $rows = array();
    while ($row = db_fetch_assoc($result)) {
        $rows[] = array(
            'event_id' => (int)$row['event_id'],
            'chain_scope' => (string)$row['chain_scope'],
            'chain_sequence' => (int)$row['chain_sequence'],
            'occurred_at' => (string)$row['occurred_at'],
            'actor_user_id' => (int)$row['actor_user_id'],
            'company_id' => (int)$row['company_id'],
            'legal_entity_id' => (int)$row['legal_entity_id'],
            'entity_type' => (string)$row['entity_type'],
            'action' => (string)$row['action'],
            'reason_code' => (string)$row['reason_code'],
            'correlation_id' => (string)$row['correlation_id'],
            'previous_event_hash' => (string)$row['previous_event_hash'],
            'event_hash' => (string)$row['event_hash'],
            'key_id' => (string)$row['key_id'],
        );
    }
    return $rows;
}
