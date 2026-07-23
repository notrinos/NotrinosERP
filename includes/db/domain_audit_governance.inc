<?php
/**
 * PAY-AUD-001 retention-policy and legal-hold governance.
 *
 * Governance records reuse the immutable domain-audit event/outbox/checkpoint
 * transaction. No mutable policy table or parallel audit framework is used.
 */

/**
 * Return whether a jurisdiction code is a stable non-sensitive token.
 *
 * @param string $jurisdiction_code
 * @return bool
 */
function domain_audit_valid_jurisdiction_code($jurisdiction_code)
{
    return is_string($jurisdiction_code)
        && preg_match('/^[A-Z0-9][A-Z0-9_-]{1,31}$/', $jurisdiction_code) === 1;
}

/**
 * Return whether a value is an exact UTC audit timestamp.
 *
 * @param string $value
 * @return bool
 */
function domain_audit_valid_utc_timestamp($value)
{
    if (!is_string($value)
        || preg_match('/^\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}$/', $value) !== 1
    ) {
        return false;
    }
    $date = DateTime::createFromFormat('!Y-m-d H:i:s', $value, new DateTimeZone('UTC'));
    $errors = DateTime::getLastErrors();
    return $date !== false
        && ($errors === false || ((int)$errors['warning_count'] === 0 && (int)$errors['error_count'] === 0))
        && $date->format('Y-m-d H:i:s') === $value;
}

/**
 * Return whether a value is an exact calendar date.
 *
 * @param string $value
 * @return bool
 */
function domain_audit_valid_date($value)
{
    if (!is_string($value) || preg_match('/^\d{4}-\d{2}-\d{2}$/', $value) !== 1) {
        return false;
    }
    $date = DateTime::createFromFormat('!Y-m-d', $value, new DateTimeZone('UTC'));
    $errors = DateTime::getLastErrors();
    return $date !== false
        && ($errors === false || ((int)$errors['warning_count'] === 0 && (int)$errors['error_count'] === 0))
        && $date->format('Y-m-d') === $value;
}

/**
 * Return the exact expected restricted-diff keys for one governance action.
 *
 * @param string $action
 * @return array
 */
function domain_audit_governance_diff_keys($action)
{
    if ($action === 'audit.retention_policy.set') {
        return array(
            'effective_at', 'jurisdiction_code', 'restricted_values_stored', 'retention_days'
        );
    }
    if ($action === 'audit.legal_hold.placed' || $action === 'audit.legal_hold.released') {
        return array('hold_id', 'restricted_values_stored');
    }
    return array();
}

/**
 * Validate one immutable governance event and return its minimized transition.
 *
 * @param array $row
 * @return array|false
 */
function domain_audit_governance_transition($row)
{
    if (!is_array($row)
        || !isset($row['action'], $row['restricted_diff'], $row['chain_sequence'], $row['occurred_at'])
        || (int)$row['chain_sequence'] < 1
        || !domain_audit_valid_utc_timestamp((string)$row['occurred_at'])
    ) {
        return false;
    }
    $action = (string)$row['action'];
    $expected_keys = domain_audit_governance_diff_keys($action);
    $diff = json_decode((string)$row['restricted_diff'], true);
    $actual_keys = is_array($diff) ? array_keys($diff) : array();
    sort($expected_keys);
    sort($actual_keys);
    if (empty($expected_keys) || $actual_keys !== $expected_keys
        || !array_key_exists('restricted_values_stored', $diff)
        || $diff['restricted_values_stored'] !== false
    ) {
        return false;
    }
    if ($action === 'audit.retention_policy.set') {
        if (!is_int($diff['retention_days'])
            || $diff['retention_days'] < 1 || $diff['retention_days'] > 36500
            || !domain_audit_valid_jurisdiction_code($diff['jurisdiction_code'])
            || !domain_audit_valid_utc_timestamp($diff['effective_at'])
        ) {
            return false;
        }
    } elseif (preg_match('/^[a-f0-9]{32}$/', (string)$diff['hold_id']) !== 1) {
        return false;
    }
    return array(
        'action' => $action,
        'chain_sequence' => (int)$row['chain_sequence'],
        'occurred_at' => (string)$row['occurred_at'],
        'diff' => $diff,
    );
}

/**
 * Derive the active policy and legal holds from ordered immutable events.
 *
 * @param array $rows
 * @param string $as_of UTC timestamp.
 * @return array
 */
function domain_audit_governance_state_from_rows($rows, $as_of)
{
    if (!is_array($rows) || !domain_audit_valid_utc_timestamp($as_of)) {
        return array('ok' => false, 'error' => 'governance_input_invalid');
    }
    $policy = null;
    $holds = array();
    $last_sequence = 0;
    foreach ($rows as $row) {
        $transition = domain_audit_governance_transition($row);
        if ($transition === false || $transition['chain_sequence'] <= $last_sequence) {
            return array('ok' => false, 'error' => 'governance_event_invalid');
        }
        $last_sequence = $transition['chain_sequence'];
        if ($transition['occurred_at'] > $as_of) {
            continue;
        }
        if ($transition['action'] === 'audit.retention_policy.set') {
            if ($transition['diff']['effective_at'] <= $as_of
                && ($policy === null
                    || $transition['diff']['effective_at'] > $policy['effective_at']
                    || ($transition['diff']['effective_at'] === $policy['effective_at']
                        && $transition['chain_sequence'] > $policy['chain_sequence']))
            ) {
                $policy = array(
                    'jurisdiction_code' => (string)$transition['diff']['jurisdiction_code'],
                    'retention_days' => (int)$transition['diff']['retention_days'],
                    'effective_at' => (string)$transition['diff']['effective_at'],
                    'chain_sequence' => (int)$transition['chain_sequence'],
                );
            }
            continue;
        }
        $hold_id = (string)$transition['diff']['hold_id'];
        $active = isset($holds[$hold_id]) && $holds[$hold_id]['active'];
        if ($transition['action'] === 'audit.legal_hold.placed') {
            if ($active) {
                return array('ok' => false, 'error' => 'duplicate_active_legal_hold');
            }
            $holds[$hold_id] = array(
                'hold_id' => $hold_id,
                'active' => true,
                'placed_at' => $transition['occurred_at'],
                'placed_sequence' => $transition['chain_sequence'],
            );
        } else {
            if (!$active) {
                return array('ok' => false, 'error' => 'legal_hold_release_without_active_hold');
            }
            $holds[$hold_id]['active'] = false;
            $holds[$hold_id]['released_at'] = $transition['occurred_at'];
            $holds[$hold_id]['released_sequence'] = $transition['chain_sequence'];
        }
    }
    $active_holds = array();
    foreach ($holds as $hold) {
        if ($hold['active']) {
            $active_holds[] = $hold;
        }
    }
    usort($active_holds, 'domain_audit_compare_hold_id');
    return array(
        'ok' => true,
        'policy' => $policy,
        'active_holds' => $active_holds,
        'active_hold_count' => count($active_holds),
        'governance_event_count' => count($rows),
    );
}

/**
 * Stable active-hold ordering callback.
 *
 * @param array $left
 * @param array $right
 * @return int
 */
function domain_audit_compare_hold_id($left, $right)
{
    return strcmp((string)$left['hold_id'], (string)$right['hold_id']);
}

/**
 * Read and verify governance state for one company/legal-entity chain.
 *
 * @param int $company
 * @param int $legal_entity
 * @param string|null $as_of
 * @return array
 */
function domain_audit_governance_state($company, $legal_entity = 0, $as_of = null)
{
    $company = (int)$company;
    $legal_entity = (int)$legal_entity;
    $as_of = $as_of === null ? gmdate('Y-m-d H:i:s') : (string)$as_of;
    if ($company < 0 || $legal_entity < 0 || !domain_audit_valid_utc_timestamp($as_of)) {
        return array('ok' => false, 'error' => 'invalid_governance_scope');
    }
    $verified = verify_domain_audit_chain($company, $legal_entity);
    if (empty($verified['ok'])) {
        return array('ok' => false, 'error' => 'audit_chain_verification_failed');
    }
    $scope = 'company:'.$company.':legal_entity:'.$legal_entity;
    $result = db_query(
        "SELECT chain_sequence,occurred_at,action,restricted_diff FROM ".TB_PREF."domain_audit_events"
        ." WHERE chain_scope=".domain_audit_db_string($scope)
        ." AND entity_type='domain_audit_governance'"
        ." ORDER BY chain_sequence",
        'could not read PAY-AUD governance events'
    );
    if (!$result) {
        return array('ok' => false, 'error' => 'governance_event_read_failed');
    }
    $rows = array();
    while ($row = db_fetch_assoc($result)) {
        $rows[] = $row;
    }
    return domain_audit_governance_state_from_rows($rows, $as_of);
}

/**
 * Return the minimum calendar retention date for a receipt.
 *
 * @param string $received_at UTC timestamp.
 * @param int $retention_days
 * @return string|false
 */
function domain_audit_minimum_retained_until($received_at, $retention_days)
{
    $retention_days = (int)$retention_days;
    if (!domain_audit_valid_utc_timestamp($received_at)
        || $retention_days < 1 || $retention_days > 36500
    ) {
        return false;
    }
    $date = DateTime::createFromFormat('!Y-m-d H:i:s', $received_at, new DateTimeZone('UTC'));
    if ($date === false || $date->modify('+'.$retention_days.' days') === false) {
        return false;
    }
    return $date->format('Y-m-d');
}

/**
 * Enforce the active minimum retention policy for one custody receipt.
 *
 * Active holds never shorten retention. Their external-provider enforcement is
 * surfaced by the custody monitor until deployment-specific confirmation.
 *
 * @param array $state
 * @param string $received_at
 * @param string $retained_until
 * @return bool
 */
function domain_audit_retention_allows_receipt($state, $received_at, $retained_until)
{
    if (!is_array($state) || empty($state['ok']) || empty($state['policy'])
        || !domain_audit_valid_date($retained_until)
    ) {
        return false;
    }
    $minimum = domain_audit_minimum_retained_until(
        $received_at,
        (int)$state['policy']['retention_days']
    );
    return $minimum !== false && $retained_until >= $minimum;
}

/**
 * Authorize records governance through the configured dedicated OS identity.
 *
 * @param int $company
 * @return string|false
 */
function domain_audit_records_admin_authorized($company)
{
    global $db_connections;

    if (PHP_SAPI !== 'cli') {
        return false;
    }
    $expected = defined('PAYROLL_AUDIT_RECORDS_ADMIN_OS_USER')
        ? (string)PAYROLL_AUDIT_RECORDS_ADMIN_OS_USER
        : (isset($db_connections[$company]['payroll_audit_records_admin_os_user'])
            ? (string)$db_connections[$company]['payroll_audit_records_admin_os_user'] : '');
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
 * Append one authorized retention-policy or legal-hold transition.
 *
 * @param int $company
 * @param int $legal_entity
 * @param string $action
 * @param array $details
 * @param string $reason_code
 * @return array|false
 */
function append_domain_audit_governance_event(
    $company,
    $legal_entity,
    $action,
    $details,
    $reason_code
) {
    $company = (int)$company;
    $legal_entity = (int)$legal_entity;
    $actor = domain_audit_records_admin_authorized($company);
    if ($company < 0 || $legal_entity < 0 || $actor === false
        || !domain_audit_valid_token($reason_code) || !is_array($details)
        || empty(domain_audit_governance_diff_keys($action))
    ) {
        return false;
    }
    $now = gmdate('Y-m-d H:i:s');
    $state = domain_audit_governance_state($company, $legal_entity, $now);
    if (empty($state['ok'])) {
        return false;
    }
    if ($action === 'audit.retention_policy.set') {
        $diff = array(
            'effective_at' => isset($details['effective_at']) ? (string)$details['effective_at'] : '',
            'jurisdiction_code' => isset($details['jurisdiction_code'])
                ? (string)$details['jurisdiction_code'] : '',
            'restricted_values_stored' => false,
            'retention_days' => isset($details['retention_days'])
                ? (int)$details['retention_days'] : 0,
        );
        $probe = domain_audit_governance_transition(array(
            'action' => $action,
            'restricted_diff' => domain_audit_canonical_json($diff),
            'chain_sequence' => 1,
            'occurred_at' => $now,
        ));
        if ($probe === false) {
            return false;
        }
        $entity_key = array(
            'jurisdiction_code' => $diff['jurisdiction_code'],
            'legal_entity_id' => $legal_entity,
        );
    } else {
        $hold_id = isset($details['hold_id']) ? strtolower((string)$details['hold_id']) : '';
        if (preg_match('/^[a-f0-9]{32}$/', $hold_id) !== 1) {
            return false;
        }
        $active = false;
        foreach ($state['active_holds'] as $hold) {
            if (domain_audit_hash_equals((string)$hold['hold_id'], $hold_id)) {
                $active = true;
            }
        }
        if (($action === 'audit.legal_hold.placed' && $active)
            || ($action === 'audit.legal_hold.released' && !$active)
        ) {
            return false;
        }
        $diff = array('hold_id' => $hold_id, 'restricted_values_stored' => false);
        $entity_key = array('hold_id' => $hold_id, 'legal_entity_id' => $legal_entity);
    }
    return append_domain_audit_event(array(
        'company_id' => $company,
        'legal_entity_id' => $legal_entity,
        'entity_type' => 'domain_audit_governance',
        'entity_key' => $entity_key,
        'action' => (string)$action,
        'reason_code' => (string)$reason_code,
        'security_context' => array(
            'actor_user_id' => 0,
            'company_id' => $company,
            'role_id' => 0,
            'role_set' => array(),
            'auth_method' => 'records_governance_cli',
            'os_actor' => (string)$actor,
        ),
        'restricted_diff' => $diff,
    ));
}
