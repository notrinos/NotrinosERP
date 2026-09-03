<?php
/**
 * HRM-FND-004 encrypted Person bank-account compatibility shadow foundation.
 *
 * Legacy employee bank columns remain authoritative in this slice. The shadow
 * is effective-dated, encrypted, masked, unverified and unapproved. No full
 * reveal, approval, payment-election or payroll-routing authority is added.
 */

if (!function_exists('hrm_person_sensitive_active_key'))
    include_once($path_to_root.'/hrm/includes/db/employee_person_sensitive_db.inc');
if (!function_exists('get_hrm_person_worker_binding'))
    include_once($path_to_root.'/hrm/includes/db/employee_person_worker_db.inc');
if (!function_exists('hrm_user_can_access_sensitive_field'))
    include_once($path_to_root.'/hrm/includes/hrm_security.inc');
if (!function_exists('append_domain_audit_event'))
    include_once($path_to_root.'/includes/db/domain_audit_db.inc');

/** @return bool */
function hrm_person_bank_table_ready()
{
    return hrm_person_worker_table_exists('hrm_person_bank_accounts');
}

/**
 * Validate a legacy bank string without silently changing stored evidence.
 *
 * @param mixed $value
 * @param int $maximum_length
 * @return string|false
 */
function hrm_person_bank_legacy_value($value, $maximum_length = 255)
{
    $value = (string)$value;
    if (strlen($value) > (int)$maximum_length
        || preg_match('/[\x00-\x1F\x7F]/', $value))
        return false;
    return $value;
}

/** @return array|false */
function hrm_person_bank_payload_from_employee($employee)
{
    if (!is_array($employee))
        return false;
    $payload = array();
    foreach (array('bank_name','bank_branch','bank_account','bank_routing') as $field) {
        $value = hrm_person_bank_legacy_value(isset($employee[$field]) ? $employee[$field] : '');
        if ($value === false)
            return false;
        $payload[$field] = $value;
    }
    $method = isset($employee['payment_method']) ? (int)$employee['payment_method'] : 0;
    if ($method < 0 || $method > 2)
        return false;
    $payload['payment_method'] = $method;
    return $payload;
}

/** @return bool */
function hrm_person_bank_payload_has_details($payload)
{
    if (!is_array($payload))
        return false;
    foreach (array('bank_name','bank_branch','bank_account','bank_routing') as $field) {
        if (isset($payload[$field]) && trim((string)$payload[$field]) !== '')
            return true;
    }
    return false;
}

/** @return string|false */
function hrm_person_bank_payload_json($payload)
{
    if (!is_array($payload))
        return false;
    $ordered = array();
    foreach (array('bank_name','bank_branch','bank_account','bank_routing') as $field) {
        if (!array_key_exists($field, $payload))
            return false;
        $value = hrm_person_bank_legacy_value($payload[$field]);
        if ($value === false)
            return false;
        $ordered[$field] = $value;
    }
    if (!isset($payload['payment_method']) || (int)$payload['payment_method'] < 0
        || (int)$payload['payment_method'] > 2)
        return false;
    $ordered['payment_method'] = (int)$payload['payment_method'];
    $json = json_encode($ordered, JSON_UNESCAPED_SLASHES | JSON_UNESCAPED_UNICODE);
    return is_string($json) && strlen($json) <= 1024 ? $json : false;
}

/**
 * Return a MariaDB binary hex literal without routing ciphertext through
 * db_escape()/HTML encoding. Ciphertext is arbitrary bytes, not UI text.
 *
 * @param string $value
 * @return string|false
 */
function hrm_person_bank_binary_sql_literal($value)
{
    if (!is_string($value) || $value === '')
        return false;
    return "X'".bin2hex($value)."'";
}

function hrm_person_bank_context($person_id, $company = null)
{
    if ($company === null)
        $company = function_exists('user_company') ? (int)user_company() : 0;
    if ((int)$person_id <= 0)
        return false;
    return 'company='.(int)$company.';person='.(int)$person_id.';class=bank_account;type=legacy_details';
}

/**
 * Validate one stored current row and compare exact legacy evidence.
 *
 * @return int|false One exact match, zero valid-but-different, false corrupted.
 */
function hrm_person_bank_current_state($current, $person_id, $payload)
{
    if (!is_array($current) || empty($current['key_id']))
        return false;
    $key = hrm_person_sensitive_key((string)$current['key_id']);
    $context = hrm_person_bank_context($person_id);
    $expected_json = hrm_person_bank_payload_json($payload);
    if ($key === false || $context === false || $expected_json === false)
        return false;
    $plaintext = hrm_person_sensitive_decrypt((string)$current['details_ciphertext'], $context, $key);
    if ($plaintext === false)
        return false;
    $stored = json_decode($plaintext, true);
    $stored_json = hrm_person_bank_payload_json($stored);
    if ($stored_json === false || !domain_audit_hash_equals($stored_json, $plaintext))
        return false;
    $account = (string)$stored['bank_account'];
    $routing = (string)$stored['bank_routing'];
    $token = trim($account) === '' ? '' : hrm_person_sensitive_lookup_token('bank_account', $account, $key);
    if ($token === false
        || (string)$current['account_lookup_token'] !== $token
        || (string)$current['masked_account'] !== hrm_person_sensitive_mask($account)
        || (string)$current['masked_routing'] !== hrm_person_sensitive_mask($routing)
        || !isset($current['payment_method']) || (int)$current['payment_method'] !== (int)$stored['payment_method']
        || (string)$current['verification_status'] !== 'unverified'
        || (string)$current['approval_status'] !== 'unapproved'
        || (int)$current['row_version'] !== 1)
        return false;
    return domain_audit_hash_equals($plaintext, $expected_json) ? 1 : 0;
}

/**
 * Classify the one current normalized bank row without exposing protected data.
 *
 * @return string|false governed, legacy, none, or false on malformed custody.
 */
function hrm_person_bank_current_custody_state($person_id, $for_update = false)
{
    if ((int)$person_id <= 0 || !hrm_person_bank_table_ready())
        return false;
    $result = db_query("SELECT person_bank_account_id,source,supersedes_person_bank_account_id FROM "
        .TB_PREF."hrm_person_bank_accounts WHERE person_id=".(int)$person_id
        ." AND effective_to IS NULL ORDER BY person_bank_account_id DESC LIMIT 2"
        .($for_update ? " FOR UPDATE" : ""), 'could not classify current HRM Person bank account custody');
    if (!$result || db_num_rows($result) > 1)
        return false;
    if (db_num_rows($result) === 0)
        return 'none';
    $row = db_fetch_assoc($result);
    $source = isset($row['source']) ? (string)$row['source'] : '';
    $supersedes = isset($row['supersedes_person_bank_account_id'])
        && $row['supersedes_person_bank_account_id'] !== null
        ? (int)$row['supersedes_person_bank_account_id'] : 0;
    if ($source === 'governed_supersession')
        return $supersedes > 0 ? 'governed' : false;
    return $supersedes === 0 ? 'legacy' : false;
}

/**
 * Guard legacy employee-row bank mutation after governed lineage takes custody.
 *
 * Before governed supersession, legacy bank columns remain compatibility
 * authority. Once a governed current successor exists, unchanged employee
 * updates may proceed but any legacy bank/payment-method mutation is denied.
 * The returned preserve token is rechecked under the transaction lock by the
 * synchronizer to close the preflight/commit race. No bank value is logged.
 *
 * @return string|false legacy_sync, preserve_governed, or false.
 */
function hrm_person_bank_legacy_update_guard($before, $changes)
{
    $GLOBALS['hrm_person_bank_legacy_update_guard_result'] = 'invalid_input';
    if (!is_array($before) || !is_array($changes) || empty($before['employee_number']))
        return false;
    $binding = get_hrm_person_worker_binding((int)$before['employee_number'], false);
    if (!is_array($binding) || $binding['retired_at'] !== null)
        return false;
    $custody = hrm_person_bank_current_custody_state((int)$binding['person_id'], false);
    if ($custody === false)
        return false;
    $changed = false;
    foreach (array('bank_name','bank_branch','bank_account','bank_routing','payment_method') as $field) {
        if (!array_key_exists($field, $changes))
            continue;
        $prior = array_key_exists($field, $before) && $before[$field] !== null ? $before[$field] : '';
        $next = $changes[$field] !== null ? $changes[$field] : '';
        if ($field === 'payment_method') {
            $prior = (string)(int)$prior;
            $next = (string)(int)$next;
        } else {
            $prior = (string)$prior;
            $next = (string)$next;
        }
        if (!hash_equals($prior, $next)) {
            $changed = true;
            break;
        }
    }
    if ($custody === 'governed') {
        if ($changed) {
            $GLOBALS['hrm_person_bank_legacy_update_guard_result'] = 'governed_bank_legacy_mutation_denied';
            return false;
        }
        $GLOBALS['hrm_person_bank_legacy_update_guard_result'] = 'preserve_governed';
        return 'preserve_governed';
    }
    $GLOBALS['hrm_person_bank_legacy_update_guard_result'] = 'legacy_sync';
    return 'legacy_sync';
}

/**
 * Synchronize the single legacy current bank projection into encrypted history.
 *
 * @return int|false Zero no change, one version opened/closed, false failure.
 */
function synchronize_hrm_person_bank_account_value($person_id, $payload, $source, $active_key)
{
    if ((int)$person_id <= 0 || !hrm_person_bank_table_ready())
        return false;
    $json = hrm_person_bank_payload_json($payload);
    if ($json === false || !preg_match('/^[a-z][a-z0-9_]{0,31}$/D', (string)$source))
        return false;
    $result = db_query("SELECT person_bank_account_id,details_ciphertext,masked_account,masked_routing,"
        ."account_lookup_token,key_id,payment_method,verification_status,approval_status,row_version,effective_from,effective_to,source,supersedes_person_bank_account_id "
        ."FROM ".TB_PREF."hrm_person_bank_accounts WHERE person_id=".(int)$person_id
        ." AND effective_to IS NULL ORDER BY person_bank_account_id DESC LIMIT 2 FOR UPDATE",
        'could not read current HRM Person bank account shadow');
    if (!$result || db_num_rows($result) > 1)
        return false;
    $current = db_num_rows($result) === 1 ? db_fetch_assoc($result) : false;
    $governed_current = $current
        && (string)$current['source'] === 'governed_supersession'
        && isset($current['supersedes_person_bank_account_id'])
        && (int)$current['supersedes_person_bank_account_id'] > 0;
    if ($source === 'legacy_employee_compat_preserve')
        return $governed_current ? 0 : false;
    if ($governed_current && $source === 'runtime_employee')
        return false;
    if (!is_array($active_key) || empty($active_key['key_id']) || !isset($active_key['key']))
        return false;
    $has_details = hrm_person_bank_payload_has_details($payload);
    if (!$has_details) {
        if (!$current)
            return 0;
        $now = hrm_person_worker_utc_now();
        return $now !== false && db_query("UPDATE ".TB_PREF."hrm_person_bank_accounts SET effective_to="
            .db_escape($now)." WHERE person_bank_account_id=".(int)$current['person_bank_account_id']
            ." AND effective_to IS NULL", 'could not close HRM Person bank account shadow') ? 1 : false;
    }
    if ($current) {
        $state = hrm_person_bank_current_state($current, $person_id, $payload);
        if ($state === false)
            return false;
        if ($state === 1)
            return 0;
        if ($source === 'legacy_employee_seed')
            return false;
    }
    $now = $source === 'legacy_employee_seed' ? null : hrm_person_worker_utc_now();
    if ($source !== 'legacy_employee_seed' && $now === false)
        return false;
    if ($current && !db_query("UPDATE ".TB_PREF."hrm_person_bank_accounts SET effective_to="
        .db_escape($now)." WHERE person_bank_account_id=".(int)$current['person_bank_account_id']
        ." AND effective_to IS NULL", 'could not close HRM Person bank account version'))
        return false;
    $context = hrm_person_bank_context($person_id);
    $ciphertext = $context === false ? false
        : hrm_person_sensitive_encrypt($json, $context, $active_key['key']);
    $account = (string)$payload['bank_account'];
    $routing = (string)$payload['bank_routing'];
    $token = trim($account) === '' ? ''
        : hrm_person_sensitive_lookup_token('bank_account', $account, $active_key['key']);
    $ciphertext_sql = $ciphertext === false ? false : hrm_person_bank_binary_sql_literal($ciphertext);
    if ($ciphertext_sql === false || $token === false)
        return false;
    $sql = "INSERT INTO ".TB_PREF."hrm_person_bank_accounts "
        ."(person_id,details_ciphertext,masked_account,masked_routing,account_lookup_token,key_id,payment_method,"
        ."verification_status,verification_method,approval_status,row_version,effective_from,effective_to,source,created_by) VALUES ("
        .(int)$person_id.",".$ciphertext_sql.",".db_escape(hrm_person_sensitive_mask($account)).",".
        db_escape(hrm_person_sensitive_mask($routing)).",".db_escape($token).",".db_escape($active_key['key_id']).",".
        (int)$payload['payment_method'].",'unverified','','unapproved',1,".
        ($now === null ? 'NULL' : db_escape($now)).",NULL,".db_escape($source).",".hrm_person_worker_actor_id().")";
    return db_query($sql, 'could not create encrypted HRM Person bank account version') ? 1 : false;
}

/** @return array|false */
function append_hrm_person_bank_domain_audit($binding, $source)
{
    if (!is_array($binding) || empty($binding['person_id']) || empty($binding['worker_id'])
        || empty($binding['employee_number']))
        return false;
    return append_domain_audit_event(array(
        'company_id' => function_exists('user_company') ? (int)user_company() : 0,
        'legal_entity_id' => 0,
        'entity_type' => 'hrm_person_bank_account',
        'entity_key' => array('person_id'=>(int)$binding['person_id'],'worker_id'=>(int)$binding['worker_id'],
            'employee_number'=>(int)$binding['employee_number']),
        'action' => 'hrm.person_bank_account.shadow_synchronized',
        'reason_code' => 'hrm_fnd_004_person_bank_account_foundation',
        'before' => null,
        'after' => array('shadow_version_changed'=>true,'verification_status'=>'unverified','approval_status'=>'unapproved'),
        'restricted_diff' => array('source'=>preg_replace('/[^a-z0-9_]/','',strtolower((string)$source)),
            'restricted_values_stored'=>false,'payment_authority_changed'=>false),
    ));
}

/**
 * Synchronize legacy employee bank evidence into the Person shadow.
 *
 * @return bool
 */
function synchronize_hrm_person_bank_from_employee($employee, $audit = true, $source = 'runtime_employee', $allow_key_create = false)
{
    if (!is_array($employee) || empty($employee['employee_number']) || !hrm_person_bank_table_ready())
        return false;
    $binding = get_hrm_person_worker_binding((int)$employee['employee_number'], true);
    $payload = hrm_person_bank_payload_from_employee($employee);
    $active_key = $source === 'legacy_employee_compat_preserve'
        ? array() : hrm_person_sensitive_active_key(null, $allow_key_create);
    if (!$binding || $binding['retired_at'] !== null || $payload === false
        || ($source !== 'legacy_employee_compat_preserve' && $active_key === false))
        return false;
    $changed = synchronize_hrm_person_bank_account_value((int)$binding['person_id'], $payload, $source, $active_key);
    if ($changed === false)
        return false;
    return $changed === 0 || !$audit || append_hrm_person_bank_domain_audit($binding, $source) !== false;
}

/** @return string|false */
function get_hrm_person_bank_masked_as_of_sql($employee_ref, $as_of)
{
    $as_of = hrm_person_worker_normalize_as_of($as_of);
    if ($as_of === false || !hrm_person_bank_table_ready())
        return false;
    $column = is_numeric($employee_ref) ? 'employee_number' : 'employee_id';
    $value = is_numeric($employee_ref) ? (int)$employee_ref : trim((string)$employee_ref);
    if (($column === 'employee_number' && $value <= 0) || ($column === 'employee_id' && $value === ''))
        return false;
    $as_of_sql = db_escape($as_of);
    return "SELECT b.person_bank_account_id,b.masked_account,b.masked_routing,b.payment_method,"
        ."b.verification_status,b.approval_status,b.effective_from,b.effective_to,b.source "
        ."FROM ".TB_PREF."hrm_employee_worker_map m INNER JOIN ".TB_PREF."hrm_workers w ON w.worker_id=m.worker_id "
        ."INNER JOIN ".TB_PREF."hrm_person_bank_accounts b ON b.person_id=w.person_id WHERE m.".$column."=".db_escape($value)
        ." AND (m.retired_at IS NULL OR m.retired_at>".$as_of_sql.")"
        ." AND (b.effective_from IS NULL OR b.effective_from<=".$as_of_sql.")"
        ." AND (b.effective_to IS NULL OR b.effective_to>".$as_of_sql.")"
        ." ORDER BY b.effective_from DESC,b.person_bank_account_id DESC";
}

/**
 * Return masked financial projection only; there is deliberately no full reveal.
 *
 * @return array|false
 */
function get_hrm_person_bank_masked_as_of($employee_ref, $as_of)
{
    if (!function_exists('user_check_access')
        || !hrm_user_can_access_sensitive_field(HRM_FIELD_RESTRICTED_FINANCIAL, HRM_FIELD_ACTION_VIEW))
        return false;
    $sql = get_hrm_person_bank_masked_as_of_sql($employee_ref, $as_of);
    if ($sql === false)
        return false;
    $result = db_query($sql, 'could not read masked HRM Person bank account projection');
    if (!$result)
        return false;
    $rows = array();
    while ($row = db_fetch_assoc($result))
        $rows[] = $row;
    return $rows;
}
