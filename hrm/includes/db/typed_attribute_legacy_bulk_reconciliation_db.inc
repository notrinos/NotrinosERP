<?php
/**
 * HRM-FND-007 read-only reconciliation/completion evidence for one legacy conversion batch.
 *
 * This helper never rewrites employees.custom_data, never locks business rows, and never
 * returns legacy or decrypted Typed Attribute plaintext. It verifies the frozen batch,
 * historical retained-profile evidence, conversion receipts, and encrypted values only.
 */
if (!function_exists('hrm_typed_attribute_legacy_reconciliation_value_matches'))
    include_once($path_to_root.'/hrm/includes/db/typed_attribute_legacy_reconciliation_db.inc');
if (!function_exists('hrm_typed_attribute_legacy_bulk_batch_digest'))
    include_once($path_to_root.'/hrm/includes/db/typed_attribute_legacy_bulk_batch_command_db.inc');

function hrm_typed_attribute_legacy_bulk_reconciliation_user_can_view()
{
    return function_exists('user_check_access')
        && user_check_access('SA_HRM_VIEW_TYPED_ATTRIBUTE_LEGACY_BULK_CANDIDATES')
        && user_check_access('SA_HRM_VIEW_TYPED_ATTRIBUTE_LEGACY_CONVERSION_STATUS')
        && user_check_access('SA_HRM_VIEW_TYPED_ATTRIBUTE_LEGACY_MAPPING')
        && user_check_access('SA_HRM_VIEW_TYPED_ATTRIBUTE_LEGACY_BULK_RECONCILIATION');
}

function hrm_typed_attribute_legacy_bulk_reconciliation_result($status, $batch, $reconciled_count, $blocked_count)
{
    return array(
        'status'=>(string)$status,
        'batch_id'=>(int)$batch['batch_id'],
        'mapping_version_id'=>(int)$batch['mapping_version_id'],
        'candidate_count'=>(int)$batch['candidate_count'],
        'reconciled_count'=>(int)$reconciled_count,
        'blocked_count'=>(int)$blocked_count,
        'candidate_set_digest_verified'=>!in_array((string)$status, array('blocked_batch_item_count','blocked_batch_candidate_digest','blocked_batch_candidate_set_digest'), true),
        'legacy_keys_retained'=>true,
        'legacy_custom_data_rewritten'=>false,
        'plaintext_exposed'=>false,
        'read_only'=>true,
    );
}

/** Read one exact historical retained profile row without resolving to a newer profile. */
function hrm_typed_attribute_legacy_bulk_reconciliation_profile($profile_version_id, $worker_id)
{
    $q = db_query('SELECT * FROM '.TB_PREF.'hrm_worker_profile_versions WHERE profile_version_id='.(int)$profile_version_id
        .' AND worker_id='.(int)$worker_id.' LIMIT 2', 'could not read frozen retained profile for bulk reconciliation');
    return $q && db_num_rows($q) === 1 ? db_fetch_assoc($q) : false;
}

/**
 * Reconcile one completed frozen batch without mutation or plaintext disclosure.
 * Pending/approved batches remain legacy-authoritative and return pending_batch_completion.
 */
function get_hrm_typed_attribute_legacy_bulk_reconciliation($batch_id)
{
    $batch_id = hrm_typed_attribute_legacy_mapping_positive_id($batch_id, false);
    if ($batch_id === false || !hrm_typed_attribute_legacy_bulk_reconciliation_user_can_view())
        return false;

    $q = db_query('SELECT batch_id,mapping_version_id,candidate_set_digest,candidate_as_of,candidate_count,batch_status,'
        .'maker_id,checker_id,approved_at,row_version FROM '.TB_PREF.'hrm_attribute_legacy_conversion_batches WHERE batch_id='.(int)$batch_id.' LIMIT 2',
        'could not read legacy bulk batch for reconciliation');
    $batch = $q && db_num_rows($q) === 1 ? db_fetch_assoc($q) : false;
    if (!$batch || (int)$batch['candidate_count'] < 1 || (int)$batch['candidate_count'] > 25)
        return false;

    $q = db_query('SELECT batch_item_id,employee_number,worker_id,profile_version_id,definition_version_id,candidate_digest,item_state,conversion_id FROM '
        .TB_PREF.'hrm_attribute_legacy_conversion_batch_items WHERE batch_id='.(int)$batch_id.' ORDER BY employee_number,batch_item_id',
        'could not read frozen legacy bulk batch items for reconciliation');
    if (!$q)
        return false;
    $items = array();
    while (is_array($row = db_fetch_assoc($q)))
        $items[] = $row;
    if (count($items) !== (int)$batch['candidate_count'])
        return hrm_typed_attribute_legacy_bulk_reconciliation_result('blocked_batch_item_count', $batch, 0, count($items));

    $candidates = array();
    foreach ($items as $item) {
        $expected = hrm_typed_attribute_legacy_bulk_candidate_digest((int)$batch['mapping_version_id'], (int)$item['worker_id'],
            (int)$item['profile_version_id'], (int)$item['definition_version_id']);
        if ($expected === false || !hash_equals($expected, (string)$item['candidate_digest']))
            return hrm_typed_attribute_legacy_bulk_reconciliation_result('blocked_batch_candidate_digest', $batch, 0, count($items));
        $candidates[] = array(
            'employee_number'=>(int)$item['employee_number'], 'worker_id'=>(int)$item['worker_id'],
            'profile_version_id'=>(int)$item['profile_version_id'], 'definition_version_id'=>(int)$item['definition_version_id'],
            'candidate_digest'=>(string)$item['candidate_digest']
        );
    }
    $set_digest = hrm_typed_attribute_legacy_bulk_batch_digest((int)$batch['mapping_version_id'], (string)$batch['candidate_as_of'], $candidates);
    if ($set_digest === false || !hash_equals($set_digest, (string)$batch['candidate_set_digest']))
        return hrm_typed_attribute_legacy_bulk_reconciliation_result('blocked_batch_candidate_set_digest', $batch, 0, count($items));

    if ((string)$batch['batch_status'] !== 'completed') {
        if (!in_array((string)$batch['batch_status'], array('draft','approved'), true))
            return false;
        return hrm_typed_attribute_legacy_bulk_reconciliation_result('pending_batch_completion', $batch, 0, 0);
    }
    if ($batch['checker_id'] === null || (int)$batch['checker_id'] === (int)$batch['maker_id']
        || $batch['approved_at'] === null || (int)$batch['row_version'] < 3)
        return hrm_typed_attribute_legacy_bulk_reconciliation_result('blocked_batch_completion_custody', $batch, 0, count($items));

    $mq = db_query('SELECT mapping_version_id,legacy_key,version_no,predecessor_id,attribute_code,subject_type,mapping_status,effective_from,effective_to,qualified_review_ref,approved_at,approved_by FROM '
        .TB_PREF.'hrm_attribute_legacy_mapping_versions WHERE mapping_version_id='.(int)$batch['mapping_version_id'].' LIMIT 2',
        'could not read exact legacy mapping for bulk reconciliation');
    $mapping = $mq && db_num_rows($mq) === 1 ? db_fetch_assoc($mq) : false;
    if (!$mapping || !hrm_typed_attribute_legacy_mapping_validate_metadata($mapping)
        || (string)$mapping['mapping_status'] !== 'approved' || (string)$mapping['subject_type'] !== 'worker'
        || (string)$batch['candidate_as_of'] < (string)$mapping['effective_from']
        || ($mapping['effective_to'] !== null && (string)$batch['candidate_as_of'] > (string)$mapping['effective_to']))
        return false;

    $reconciled = 0;
    foreach ($items as $item) {
        if ((string)$item['item_state'] !== 'converted' || $item['conversion_id'] === null || (int)$item['conversion_id'] <= 0)
            return hrm_typed_attribute_legacy_bulk_reconciliation_result('blocked_batch_item_completion', $batch, $reconciled, count($items)-$reconciled);

        // Prove exact definition/sensitivity authority before reading or decrypting retained person data.
        $definition = hrm_typed_attribute_legacy_reconciliation_definition((int)$item['definition_version_id']);
        if (!is_array($definition) || (string)$definition['attribute_code'] !== (string)$mapping['attribute_code']
            || (string)$definition['allowed_subject'] !== 'worker' || !hrm_typed_attribute_legacy_reconciliation_user_can_view($definition))
            return false;

        $binding = get_hrm_person_worker_binding((int)$item['employee_number']);
        if (!is_array($binding) || (int)$binding['worker_id'] !== (int)$item['worker_id'])
            return hrm_typed_attribute_legacy_bulk_reconciliation_result('blocked_worker_binding_drift', $batch, $reconciled, count($items)-$reconciled);
        $employee_q = db_query('SELECT employee_id,custom_data FROM '.TB_PREF.'employees WHERE employee_number='.(int)$item['employee_number'].' LIMIT 2',
            'could not read retained legacy custom_data for bulk reconciliation');
        $employee = $employee_q && db_num_rows($employee_q) === 1 ? db_fetch_assoc($employee_q) : false;
        if (!$employee || (string)$employee['employee_id'] !== (string)$binding['employee_id'])
            return false;

        $profile = hrm_typed_attribute_legacy_bulk_reconciliation_profile((int)$item['profile_version_id'], (int)$item['worker_id']);
        $retained = $profile ? hrm_compatibility_profile_open($profile) : false;
        if (!is_array($profile) || $retained === false || !array_key_exists('custom_data', $retained))
            return hrm_typed_attribute_legacy_bulk_reconciliation_result('blocked_retained_profile_custody', $batch, $reconciled, count($items)-$reconciled);
        if (!hrm_typed_attribute_legacy_custom_data_parity($employee['custom_data'], $retained['custom_data']))
            return hrm_typed_attribute_legacy_bulk_reconciliation_result('blocked_profile_parity_drift', $batch, $reconciled, count($items)-$reconciled);

        $rq = db_query('SELECT conversion_id,mapping_version_id,worker_id,profile_version_id,definition_version_id,attribute_value_id,legacy_key,execution_digest FROM '
            .TB_PREF.'hrm_attribute_legacy_conversion_receipts WHERE conversion_id='.(int)$item['conversion_id'].' LIMIT 2',
            'could not read legacy bulk conversion receipt for reconciliation');
        $receipt = $rq && db_num_rows($rq) === 1 ? db_fetch_assoc($rq) : false;
        $expected_receipt = hrm_typed_attribute_legacy_conversion_receipt_digest((int)$batch['mapping_version_id'], (int)$item['worker_id'],
            (int)$item['profile_version_id'], (int)$item['definition_version_id']);
        if (!$receipt || $expected_receipt === false || !hash_equals($expected_receipt, (string)$receipt['execution_digest'])
            || (int)$receipt['mapping_version_id'] !== (int)$batch['mapping_version_id']
            || (int)$receipt['worker_id'] !== (int)$item['worker_id'] || (int)$receipt['profile_version_id'] !== (int)$item['profile_version_id']
            || (int)$receipt['definition_version_id'] !== (int)$item['definition_version_id'] || (string)$receipt['legacy_key'] !== (string)$mapping['legacy_key'])
            return hrm_typed_attribute_legacy_bulk_reconciliation_result('blocked_receipt_drift', $batch, $reconciled, count($items)-$reconciled);

        $extracted = hrm_typed_attribute_legacy_conversion_extract_scalar($retained['custom_data'], (string)$mapping['legacy_key']);
        $canonical = $extracted === false ? false : hrm_typed_attribute_value_apply_validation($definition, $extracted['value']);
        if ($canonical === false)
            return hrm_typed_attribute_legacy_bulk_reconciliation_result('blocked_legacy_value_invalid', $batch, $reconciled, count($items)-$reconciled);

        $vq = db_query('SELECT attribute_value_id,definition_version_id,subject_type,subject_id,value_version_no,predecessor_id,key_id,value_ciphertext,ciphertext_sha256,source FROM '
            .TB_PREF.'hrm_attribute_values WHERE attribute_value_id='.(int)$receipt['attribute_value_id'].' LIMIT 2',
            'could not read legacy bulk converted Typed Attribute value for reconciliation');
        $value = $vq && db_num_rows($vq) === 1 ? db_fetch_assoc($vq) : false;
        if (!$value || (int)$value['definition_version_id'] !== (int)$item['definition_version_id']
            || (string)$value['subject_type'] !== 'worker' || (int)$value['subject_id'] !== (int)$item['worker_id']
            || !hrm_typed_attribute_legacy_reconciliation_value_matches($value, $canonical))
            return hrm_typed_attribute_legacy_bulk_reconciliation_result('blocked_value_mismatch', $batch, $reconciled, count($items)-$reconciled);
        ++$reconciled;
    }

    return hrm_typed_attribute_legacy_bulk_reconciliation_result('reconciled_batch', $batch, $reconciled, 0);
}
