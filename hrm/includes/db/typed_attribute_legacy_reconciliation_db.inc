<?php
/**
 * HRM-FND-007 read-only legacy Typed Attribute conversion reconciliation.
 *
 * Reconciliation is intentionally one Employee/Worker plus one approved mapping
 * at a time. It never returns legacy or decrypted Typed Attribute plaintext and
 * performs no mutation. The result is control/status evidence only.
 */
if (!function_exists('hrm_typed_attribute_legacy_conversion_canonical_command'))
    include_once($path_to_root.'/hrm/includes/db/typed_attribute_legacy_conversion_db.inc');

/**
 * Test whether the current user may reconcile one legacy conversion.
 *
 * @param array $definition Exact definition metadata.
 * @return bool
 */
function hrm_typed_attribute_legacy_reconciliation_user_can_view($definition)
{
    return is_array($definition)
        && function_exists('user_check_access')
        && user_check_access('SA_HRM_VIEW_TYPED_ATTRIBUTE_LEGACY_CONVERSION_STATUS')
        && user_check_access('SA_HRM_VIEW_TYPED_ATTRIBUTE_LEGACY_MAPPING')
        && hrm_typed_attribute_value_user_can_view((string)$definition['sensitivity'])
        && hrm_typed_attribute_value_user_can_access_object_scope((string)$definition['owner_scope']);
}

/**
 * Return one exact definition version without resolving to a newer lifecycle row.
 *
 * @param int $definition_version_id
 * @return array|false
 */
function hrm_typed_attribute_legacy_reconciliation_definition($definition_version_id)
{
    $definition_version_id = hrm_typed_attribute_legacy_mapping_positive_id($definition_version_id, false);
    if ($definition_version_id === false)
        return false;
    $result = db_query(
        'SELECT d.definition_id,d.attribute_code,v.definition_version_id,v.value_type,v.allowed_subject,'
        .'v.sensitivity,v.owner_scope,v.validation_json,v.definition_status,v.effective_from,v.effective_to '
        .'FROM '.TB_PREF.'hrm_attribute_definition_versions v INNER JOIN '.TB_PREF.'hrm_attribute_definitions d '
        .'ON d.definition_id=v.definition_id WHERE v.definition_version_id='.(int)$definition_version_id.' LIMIT 2',
        'could not read legacy conversion definition for reconciliation'
    );
    return $result && db_num_rows($result) === 1 ? db_fetch_assoc($result) : false;
}

/**
 * Compare one encrypted conversion value with one already-canonical legacy value.
 * Plaintext exists only in local function scope and is never returned.
 *
 * @param array $value
 * @param string $canonical_legacy_value
 * @return bool
 */
function hrm_typed_attribute_legacy_reconciliation_value_matches($value, $canonical_legacy_value)
{
    if (!is_array($value) || !is_string($canonical_legacy_value)
        || !isset($value['definition_version_id'],$value['subject_type'],$value['subject_id'],
            $value['value_version_no'],$value['key_id'],$value['value_ciphertext'],
            $value['ciphertext_sha256'],$value['source'])
        || !array_key_exists('predecessor_id', $value)
        || (string)$value['source'] !== 'legacy_convert'
        || (int)$value['value_version_no'] !== 1 || $value['predecessor_id'] !== null
        || !preg_match('/^[a-f0-9]{64}$/D', (string)$value['ciphertext_sha256'])
        || !function_exists('domain_audit_hash_equals')
        || !domain_audit_hash_equals(hash('sha256', $value['value_ciphertext']), (string)$value['ciphertext_sha256']))
        return false;
    $key = hrm_person_sensitive_key((string)$value['key_id']);
    $context = hrm_typed_attribute_crypto_context(
        (int)$value['definition_version_id'], (string)$value['subject_type'],
        (int)$value['subject_id'], (int)$value['value_version_no'], null
    );
    $plaintext = $key === false || $context === false ? false
        : hrm_person_sensitive_decrypt($value['value_ciphertext'], $context, $key);
    return is_string($plaintext) && domain_audit_hash_equals($plaintext, $canonical_legacy_value);
}

/**
 * Return a redacted fail-closed status result.
 *
 * @param string $status
 * @param array $ids
 * @return array
 */
function hrm_typed_attribute_legacy_reconciliation_result($status, $ids)
{
    return array(
        'status'=>(string)$status,
        'employee_number'=>(int)$ids['employee_number'],
        'worker_id'=>(int)$ids['worker_id'],
        'mapping_version_id'=>(int)$ids['mapping_version_id'],
        'profile_version_id'=>(int)$ids['profile_version_id'],
        'definition_version_id'=>(int)$ids['definition_version_id'],
        'conversion_id'=>(int)$ids['conversion_id'],
        'attribute_value_id'=>(int)$ids['attribute_value_id'],
        'legacy_key_retained'=>true,
        'legacy_custom_data_rewritten'=>false,
        'plaintext_exposed'=>false,
        'read_only'=>true,
    );
}

/**
 * Reconcile one current legacy key against one conversion receipt/value chain.
 *
 * Status values are redacted control states. The function never writes, locks,
 * enumerates Workers, or exposes the legacy/decrypted value.
 *
 * @param array $input employee_number + mapping_version_id only.
 * @return array|false
 */
function get_hrm_typed_attribute_legacy_conversion_reconciliation($input)
{
    $command = hrm_typed_attribute_legacy_conversion_canonical_command($input);
    if ($command === false || !function_exists('user_check_access')
        || !user_check_access('SA_HRM_VIEW_TYPED_ATTRIBUTE_LEGACY_CONVERSION_STATUS')
        || !user_check_access('SA_HRM_VIEW_TYPED_ATTRIBUTE_LEGACY_MAPPING'))
        return false;
    $ids = array(
        'employee_number'=>(int)$command['employee_number'], 'worker_id'=>0,
        'mapping_version_id'=>(int)$command['mapping_version_id'], 'profile_version_id'=>0,
        'definition_version_id'=>0, 'conversion_id'=>0, 'attribute_value_id'=>0,
    );

    $now = hrm_person_worker_utc_now();
    if ($now === false)
        return false;
    $today = substr($now, 0, 10);
    $mapping_result = db_query(
        'SELECT mapping_version_id,legacy_key,version_no,predecessor_id,attribute_code,subject_type,mapping_status,'
        .'effective_from,effective_to,qualified_review_ref,approved_at,approved_by '
        .'FROM '.TB_PREF.'hrm_attribute_legacy_mapping_versions WHERE mapping_version_id='.(int)$command['mapping_version_id'].' LIMIT 2',
        'could not read approved legacy mapping for reconciliation'
    );
    $mapping = $mapping_result && db_num_rows($mapping_result) === 1 ? db_fetch_assoc($mapping_result) : false;
    if (!$mapping || !hrm_typed_attribute_legacy_mapping_validate_metadata($mapping)
        || (string)$mapping['mapping_status'] !== 'approved' || (string)$mapping['subject_type'] !== 'worker'
        || $today < (string)$mapping['effective_from']
        || ($mapping['effective_to'] !== null && $today > (string)$mapping['effective_to']))
        return false;
    $resolved_mapping = get_hrm_typed_attribute_legacy_mapping_as_of((string)$mapping['legacy_key'], $today);
    if (!is_array($resolved_mapping)
        || (int)$resolved_mapping['mapping_version_id'] !== (int)$mapping['mapping_version_id'])
        return false;
    $current_definition = hrm_typed_attribute_definition_resolve_state((string)$mapping['attribute_code'], $now, false);
    if (!is_array($current_definition) || (string)$current_definition['allowed_subject'] !== 'worker'
        || (int)$resolved_mapping['definition_version_id'] !== (int)$current_definition['definition_version_id']
        || !hrm_typed_attribute_legacy_reconciliation_user_can_view($current_definition))
        return false;
    $ids['definition_version_id'] = (int)$current_definition['definition_version_id'];

    $binding = get_hrm_person_worker_binding($command['employee_number']);
    if (!is_array($binding) || (int)$binding['worker_id'] <= 0)
        return false;
    $ids['worker_id'] = (int)$binding['worker_id'];

    $employee_result = db_query(
        'SELECT employee_id,custom_data FROM '.TB_PREF.'employees WHERE employee_number='.(int)$command['employee_number'].' LIMIT 2',
        'could not read legacy Employee custom_data for reconciliation'
    );
    $employee = $employee_result && db_num_rows($employee_result) === 1 ? db_fetch_assoc($employee_result) : false;
    if (!$employee || (string)$employee['employee_id'] !== (string)$binding['employee_id'])
        return false;

    $profile = hrm_compatibility_profile_current((int)$binding['worker_id'], false);
    $retained = $profile ? hrm_compatibility_profile_open($profile) : false;
    if (!is_array($profile) || $retained === false || !array_key_exists('custom_data', $retained))
        return false;
    $ids['profile_version_id'] = (int)$profile['profile_version_id'];
    if (!hrm_typed_attribute_legacy_custom_data_parity($employee['custom_data'], $retained['custom_data']))
        return hrm_typed_attribute_legacy_reconciliation_result('blocked_profile_parity_drift', $ids);

    $receipt_result = db_query(
        'SELECT conversion_id,mapping_version_id,worker_id,profile_version_id,definition_version_id,'
        .'attribute_value_id,legacy_key,execution_digest FROM '.TB_PREF.'hrm_attribute_legacy_conversion_receipts '
        .'WHERE mapping_version_id='.(int)$mapping['mapping_version_id'].' AND worker_id='.(int)$binding['worker_id'].' LIMIT 2',
        'could not read legacy conversion receipt for reconciliation'
    );
    if (!$receipt_result || db_num_rows($receipt_result) > 1)
        return false;
    if (db_num_rows($receipt_result) === 0) {
        if (empty($resolved_mapping['mapping_ready_for_future_execution']))
            return hrm_typed_attribute_legacy_reconciliation_result('blocked_definition_not_active', $ids);
        $value_result = db_query(
            'SELECT a.attribute_value_id FROM '.TB_PREF.'hrm_attribute_values a INNER JOIN '
            .TB_PREF.'hrm_attribute_definition_versions v ON v.definition_version_id=a.definition_version_id '
            .'WHERE v.definition_id='.(int)$current_definition['definition_id']
            ." AND a.subject_type='worker' AND a.subject_id=".(int)$binding['worker_id'].' LIMIT 2',
            'could not inspect existing Typed Attribute value custody for reconciliation'
        );
        if (!$value_result)
            return false;
        return hrm_typed_attribute_legacy_reconciliation_result(
            db_num_rows($value_result) === 0 ? 'pending_conversion' : 'blocked_existing_value_without_receipt', $ids
        );
    }
    $receipt = db_fetch_assoc($receipt_result);
    $ids['conversion_id'] = (int)$receipt['conversion_id'];
    $ids['definition_version_id'] = (int)$receipt['definition_version_id'];
    $ids['attribute_value_id'] = (int)$receipt['attribute_value_id'];

    $definition = hrm_typed_attribute_legacy_reconciliation_definition((int)$receipt['definition_version_id']);
    if (!is_array($definition) || (string)$definition['attribute_code'] !== (string)$mapping['attribute_code']
        || (string)$definition['allowed_subject'] !== 'worker'
        || !hrm_typed_attribute_legacy_reconciliation_user_can_view($definition))
        return false;

    $expected_digest = hrm_typed_attribute_legacy_conversion_receipt_digest(
        (int)$mapping['mapping_version_id'], (int)$binding['worker_id'],
        (int)$profile['profile_version_id'], (int)$definition['definition_version_id']
    );
    if ($expected_digest === false
        || (int)$receipt['mapping_version_id'] !== (int)$mapping['mapping_version_id']
        || (int)$receipt['worker_id'] !== (int)$binding['worker_id']
        || (int)$receipt['profile_version_id'] !== (int)$profile['profile_version_id']
        || (string)$receipt['legacy_key'] !== (string)$mapping['legacy_key']
        || !domain_audit_hash_equals((string)$receipt['execution_digest'], $expected_digest))
        return hrm_typed_attribute_legacy_reconciliation_result('blocked_receipt_drift', $ids);

    $extracted = hrm_typed_attribute_legacy_conversion_extract_scalar($retained['custom_data'], (string)$mapping['legacy_key']);
    $canonical = $extracted === false ? false : hrm_typed_attribute_value_apply_validation($definition, $extracted['value']);
    if ($canonical === false)
        return hrm_typed_attribute_legacy_reconciliation_result('blocked_legacy_value_invalid', $ids);

    $value_result = db_query(
        'SELECT attribute_value_id,definition_version_id,subject_type,subject_id,value_version_no,predecessor_id,'
        .'key_id,value_ciphertext,ciphertext_sha256,source FROM '.TB_PREF.'hrm_attribute_values '
        .'WHERE attribute_value_id='.(int)$receipt['attribute_value_id'].' LIMIT 2',
        'could not read legacy-converted Typed Attribute value for reconciliation'
    );
    $value = $value_result && db_num_rows($value_result) === 1 ? db_fetch_assoc($value_result) : false;
    if (!$value || (int)$value['definition_version_id'] !== (int)$definition['definition_version_id']
        || (string)$value['subject_type'] !== 'worker' || (int)$value['subject_id'] !== (int)$binding['worker_id'])
        return hrm_typed_attribute_legacy_reconciliation_result('blocked_value_custody_drift', $ids);
    if (!hrm_typed_attribute_legacy_reconciliation_value_matches($value, $canonical))
        return hrm_typed_attribute_legacy_reconciliation_result('blocked_value_mismatch', $ids);

    return hrm_typed_attribute_legacy_reconciliation_result('reconciled', $ids);
}
