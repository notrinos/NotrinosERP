<?php
/**
 * HRM-FND-007 read-only post-cleanup completion evidence.
 *
 * The projection proves immutable cleanup receipt/profile custody without returning
 * legacy plaintext or granting cleanup execution authority. It is intentionally
 * bounded to one cleanup request and one accepted conversion batch (max 25 items).
 */
if (!function_exists('hrm_typed_attribute_legacy_mapping_positive_id'))
    include_once($path_to_root.'/hrm/includes/db/typed_attribute_legacy_mapping_command_db.inc');
if (!function_exists('hrm_typed_attribute_legacy_reconciliation_definition'))
    include_once($path_to_root.'/hrm/includes/db/typed_attribute_legacy_reconciliation_db.inc');
if (!function_exists('hrm_typed_attribute_legacy_bulk_reconciliation_profile'))
    include_once($path_to_root.'/hrm/includes/db/typed_attribute_legacy_bulk_reconciliation_db.inc');

function hrm_typed_attribute_legacy_cleanup_completion_user_can_view()
{
    return function_exists('user_check_access')
        && user_check_access('SA_HRM_VIEW_TYPED_ATTRIBUTE_LEGACY_BULK_CANDIDATES')
        && user_check_access('SA_HRM_VIEW_TYPED_ATTRIBUTE_LEGACY_CONVERSION_STATUS')
        && user_check_access('SA_HRM_VIEW_TYPED_ATTRIBUTE_LEGACY_MAPPING')
        && user_check_access('SA_HRM_VIEW_TYPED_ATTRIBUTE_LEGACY_BULK_RECONCILIATION')
        && user_check_access('SA_HRM_VIEW_TYPED_ATTRIBUTE_LEGACY_CLEANUP_COMPLETION');
}

function hrm_typed_attribute_legacy_cleanup_completion_decode_object($raw)
{
    if (!is_string($raw) || strlen($raw) > 1048576)
        return false;
    $trim = ltrim($raw);
    if ($trim === '' || $trim[0] !== '{')
        return false;
    $decoded = json_decode($raw, true);
    if (!is_array($decoded) || json_last_error() !== JSON_ERROR_NONE || count($decoded) > 100)
        return false;
    foreach ($decoded as $key=>$value)
        if (!is_string($key) || preg_match('/^[A-Za-z][A-Za-z0-9_.:-]{0,127}$/D', $key) !== 1)
            return false;
    return $decoded;
}

function hrm_typed_attribute_legacy_cleanup_completion_receipt_digest($request, $item, $pre_profile_id, $post_profile_id)
{
    if (!is_array($request) || !is_array($item)
        || (int)$request['cleanup_request_id'] <= 0 || (int)$request['batch_id'] <= 0
        || (int)$item['batch_item_id'] <= 0 || (int)$item['employee_number'] <= 0
        || (int)$item['worker_id'] <= 0 || (int)$item['conversion_id'] <= 0
        || (int)$pre_profile_id <= 0 || (int)$post_profile_id <= 0
        || !isset($request['recovery_set_digest'])
        || preg_match('/^[a-f0-9]{64}$/D', (string)$request['recovery_set_digest']) !== 1)
        return false;
    return hash('sha256', 'hrm-fnd-007-legacy-cleanup-execution-v1|request='.(int)$request['cleanup_request_id']
        .'|batch='.(int)$request['batch_id'].'|item='.(int)$item['batch_item_id'].'|employee='.(int)$item['employee_number']
        .'|worker='.(int)$item['worker_id'].'|conversion='.(int)$item['conversion_id'].'|pre_profile='.(int)$pre_profile_id
        .'|post_profile='.(int)$post_profile_id.'|recovery='.(string)$request['recovery_set_digest']);
}


function hrm_typed_attribute_legacy_cleanup_completion_recovery_digest($batch, $items)
{
    if (!is_array($batch) || !is_array($items) || count($items) < 1 || count($items) > 25
        || (int)$batch['candidate_count'] !== count($items))
        return false;
    $parts = array();
    foreach ($items as $item) {
        foreach (array('employee_number','worker_id','profile_version_id','definition_version_id','candidate_digest','conversion_id','conversion_execution_digest') as $field)
            if (!array_key_exists($field, $item)) return false;
        if ((int)$item['employee_number'] <= 0 || (int)$item['worker_id'] <= 0 || (int)$item['profile_version_id'] <= 0
            || (int)$item['definition_version_id'] <= 0 || (int)$item['conversion_id'] <= 0
            || preg_match('/^[a-f0-9]{64}$/D', (string)$item['candidate_digest']) !== 1
            || preg_match('/^[a-f0-9]{64}$/D', (string)$item['conversion_execution_digest']) !== 1)
            return false;
        $parts[]=(int)$item['employee_number'].':'.(int)$item['worker_id'].':'.(int)$item['profile_version_id'].':'
            .(int)$item['definition_version_id'].':'.(string)$item['candidate_digest'].':'.(int)$item['conversion_id'].':'
            .(string)$item['conversion_execution_digest'];
    }
    return hash('sha256', 'hrm-fnd-007-legacy-cleanup-recovery-v1|batch='.(int)$batch['batch_id'].'|mapping='.(int)$batch['mapping_version_id']
        .'|candidate_set='.(string)$batch['candidate_set_digest'].'|'.implode('|', $parts));
}

function hrm_typed_attribute_legacy_cleanup_completion_result($status, $request, $verified, $blocked)
{
    return array(
        'status'=>(string)$status,
        'cleanup_request_id'=>(int)$request['cleanup_request_id'],
        'batch_id'=>(int)$request['batch_id'],
        'expected_item_count'=>(int)$request['key_count'],
        'verified_receipt_count'=>(int)$verified,
        'blocked_item_count'=>(int)$blocked,
        'legacy_plaintext_returned'=>false,
        'mutation_performed'=>false,
        'payroll_consumer_activated'=>false,
        'durable_work_authorization_delivery_activated'=>false
    );
}

/** Return redacted evidence for exactly one cleanup request; never mutates rows. */
function get_hrm_typed_attribute_legacy_cleanup_completion($cleanup_request_id)
{
    $cleanup_request_id = hrm_typed_attribute_legacy_mapping_positive_id($cleanup_request_id, false);
    if ($cleanup_request_id === false || !hrm_typed_attribute_legacy_cleanup_completion_user_can_view())
        return false;

    $q = db_query('SELECT cleanup_request_id,batch_id,reconciliation_digest,recovery_set_digest,key_count,request_status,maker_id,checker_id,approved_at,expires_at,row_version FROM '
        .TB_PREF.'hrm_attribute_legacy_cleanup_requests WHERE cleanup_request_id='.(int)$cleanup_request_id.' LIMIT 2',
        'could not read legacy cleanup request completion evidence');
    $request = $q && db_num_rows($q) === 1 ? db_fetch_assoc($q) : false;
    if (!$request || (int)$request['key_count'] < 1 || (int)$request['key_count'] > 25)
        return false;
    if ((string)$request['request_status'] !== 'completed') {
        if (!in_array((string)$request['request_status'], array('draft','approved'), true))
            return false;
        return hrm_typed_attribute_legacy_cleanup_completion_result('pending_cleanup_execution', $request, 0, (int)$request['key_count']);
    }
    if ($request['checker_id'] === null || (int)$request['checker_id'] === (int)$request['maker_id']
        || $request['approved_at'] === null || (int)$request['row_version'] < 3)
        return hrm_typed_attribute_legacy_cleanup_completion_result('blocked_request_custody', $request, 0, (int)$request['key_count']);

    $bq = db_query('SELECT batch_id,mapping_version_id,candidate_set_digest,candidate_as_of,candidate_count,batch_status,maker_id,checker_id,approved_at,row_version FROM '
        .TB_PREF.'hrm_attribute_legacy_conversion_batches WHERE batch_id='.(int)$request['batch_id'].' LIMIT 2',
        'could not read cleanup conversion batch completion evidence');
    $batch = $bq && db_num_rows($bq) === 1 ? db_fetch_assoc($bq) : false;
    if (!$batch || (string)$batch['batch_status'] !== 'completed' || (int)$batch['candidate_count'] !== (int)$request['key_count']
        || (int)$batch['candidate_count'] < 1 || (int)$batch['candidate_count'] > 25)
        return hrm_typed_attribute_legacy_cleanup_completion_result('blocked_batch_custody', $request, 0, (int)$request['key_count']);

    $mq = db_query('SELECT mapping_version_id,legacy_key,attribute_code,subject_type,mapping_status,effective_from,effective_to FROM '
        .TB_PREF.'hrm_attribute_legacy_mapping_versions WHERE mapping_version_id='.(int)$batch['mapping_version_id'].' LIMIT 2',
        'could not read cleanup mapping completion evidence');
    $mapping = $mq && db_num_rows($mq) === 1 ? db_fetch_assoc($mq) : false;
    if (!$mapping || (string)$mapping['mapping_status'] !== 'approved' || (string)$mapping['subject_type'] !== 'worker')
        return false;

    $iq = db_query('SELECT i.batch_item_id,i.employee_number,i.worker_id,i.profile_version_id,i.definition_version_id,i.candidate_digest,i.item_state,i.conversion_id,'
        .'r.mapping_version_id receipt_mapping_version_id,r.worker_id receipt_worker_id,r.profile_version_id receipt_profile_version_id,'
        .'r.definition_version_id receipt_definition_version_id,r.execution_digest conversion_execution_digest FROM '
        .TB_PREF.'hrm_attribute_legacy_conversion_batch_items i INNER JOIN '.TB_PREF.'hrm_attribute_legacy_conversion_receipts r ON r.conversion_id=i.conversion_id '
        .'WHERE i.batch_id='.(int)$batch['batch_id'].' ORDER BY i.employee_number,i.batch_item_id',
        'could not read cleanup batch items and conversion receipt completion evidence');
    if (!$iq)
        return false;
    $items = array();
    while (is_array($item = db_fetch_assoc($iq)))
        $items[] = $item;
    if (count($items) !== (int)$request['key_count'])
        return hrm_typed_attribute_legacy_cleanup_completion_result('blocked_item_count', $request, 0, count($items));
    $candidate_set = hrm_typed_attribute_legacy_bulk_batch_digest((int)$batch['mapping_version_id'], (string)$batch['candidate_as_of'], $items);
    if ($candidate_set === false || preg_match('/^[a-f0-9]{64}$/D', (string)$batch['candidate_set_digest']) !== 1
        || !hash_equals($candidate_set, (string)$batch['candidate_set_digest']))
        return hrm_typed_attribute_legacy_cleanup_completion_result('blocked_batch_candidate_set_digest', $request, 0, count($items));
    foreach ($items as $item)
        if ((int)$item['receipt_mapping_version_id'] !== (int)$batch['mapping_version_id']
            || (int)$item['receipt_worker_id'] !== (int)$item['worker_id']
            || (int)$item['receipt_profile_version_id'] !== (int)$item['profile_version_id']
            || (int)$item['receipt_definition_version_id'] !== (int)$item['definition_version_id'])
            return hrm_typed_attribute_legacy_cleanup_completion_result('blocked_conversion_receipt_custody', $request, 0, count($items));
    $recovery = hrm_typed_attribute_legacy_cleanup_completion_recovery_digest($batch, $items);
    $reconciliation = $recovery === false ? false : hash('sha256', 'hrm-fnd-007-legacy-cleanup-request-v1|batch='.(int)$batch['batch_id']
        .'|mapping='.(int)$batch['mapping_version_id'].'|candidate_set='.(string)$batch['candidate_set_digest'].'|recovery='.$recovery
        .'|count='.(int)$batch['candidate_count']);
    if ($recovery === false || $reconciliation === false
        || preg_match('/^[a-f0-9]{64}$/D', (string)$request['recovery_set_digest']) !== 1
        || preg_match('/^[a-f0-9]{64}$/D', (string)$request['reconciliation_digest']) !== 1
        || !hash_equals($recovery, (string)$request['recovery_set_digest'])
        || !hash_equals($reconciliation, (string)$request['reconciliation_digest']))
        return hrm_typed_attribute_legacy_cleanup_completion_result('blocked_request_evidence_digest', $request, 0, count($items));

    $verified = 0;
    foreach ($items as $item) {
        if ((string)$item['item_state'] !== 'converted' || $item['conversion_id'] === null || (int)$item['conversion_id'] <= 0)
            return hrm_typed_attribute_legacy_cleanup_completion_result('blocked_item_state', $request, $verified, count($items)-$verified);

        // Authorization is proven before retained-profile decryption.
        $definition = hrm_typed_attribute_legacy_reconciliation_definition((int)$item['definition_version_id']);
        if (!is_array($definition) || (string)$definition['attribute_code'] !== (string)$mapping['attribute_code']
            || (string)$definition['allowed_subject'] !== 'worker'
            || !hrm_typed_attribute_legacy_reconciliation_user_can_view($definition))
            return false;

        $rq = db_query('SELECT cleanup_receipt_id,cleanup_request_id,batch_item_id,employee_number,worker_id,mapping_version_id,conversion_id,pre_cleanup_profile_version_id,post_cleanup_profile_version_id,execution_digest,executed_by FROM '
            .TB_PREF.'hrm_attribute_legacy_cleanup_receipts WHERE cleanup_request_id='.(int)$request['cleanup_request_id']
            .' AND batch_item_id='.(int)$item['batch_item_id'].' LIMIT 2', 'could not read immutable cleanup receipt completion evidence');
        $receipt = $rq && db_num_rows($rq) === 1 ? db_fetch_assoc($rq) : false;
        if (!$receipt || (int)$receipt['employee_number'] !== (int)$item['employee_number']
            || (int)$receipt['worker_id'] !== (int)$item['worker_id']
            || (int)$receipt['mapping_version_id'] !== (int)$batch['mapping_version_id']
            || (int)$receipt['conversion_id'] !== (int)$item['conversion_id']
            || (int)$receipt['executed_by'] <= 0
            || (int)$receipt['executed_by'] === (int)$request['maker_id']
            || (int)$receipt['executed_by'] === (int)$request['checker_id'])
            return hrm_typed_attribute_legacy_cleanup_completion_result('blocked_receipt_custody', $request, $verified, count($items)-$verified);
        $expected = hrm_typed_attribute_legacy_cleanup_completion_receipt_digest($request, $item,
            (int)$receipt['pre_cleanup_profile_version_id'], (int)$receipt['post_cleanup_profile_version_id']);
        if ($expected === false || !hash_equals($expected, (string)$receipt['execution_digest']))
            return hrm_typed_attribute_legacy_cleanup_completion_result('blocked_receipt_digest', $request, $verified, count($items)-$verified);

        $pre = hrm_typed_attribute_legacy_bulk_reconciliation_profile((int)$receipt['pre_cleanup_profile_version_id'], (int)$item['worker_id']);
        $post = hrm_typed_attribute_legacy_bulk_reconciliation_profile((int)$receipt['post_cleanup_profile_version_id'], (int)$item['worker_id']);
        $pre_values = $pre ? hrm_compatibility_profile_open($pre) : false;
        $post_values = $post ? hrm_compatibility_profile_open($post) : false;
        if ($pre_values === false || $post_values === false || !isset($pre_values['custom_data']) || !isset($post_values['custom_data'])
            || (int)$post['predecessor_id'] !== (int)$pre['profile_version_id'] || (int)$post['version_no'] !== (int)$pre['version_no'] + 1)
            return hrm_typed_attribute_legacy_cleanup_completion_result('blocked_profile_lineage', $request, $verified, count($items)-$verified);
        $before = hrm_typed_attribute_legacy_cleanup_completion_decode_object((string)$pre_values['custom_data']);
        $after = hrm_typed_attribute_legacy_cleanup_completion_decode_object((string)$post_values['custom_data']);
        if ($before === false || $after === false || !array_key_exists((string)$mapping['legacy_key'], $before)
            || array_key_exists((string)$mapping['legacy_key'], $after))
            return hrm_typed_attribute_legacy_cleanup_completion_result('blocked_cleanup_effect', $request, $verified, count($items)-$verified);
        unset($before[(string)$mapping['legacy_key']]);
        if ($before !== $after)
            return hrm_typed_attribute_legacy_cleanup_completion_result('blocked_unknown_key_drift', $request, $verified, count($items)-$verified);

        $binding = get_hrm_person_worker_binding((int)$item['employee_number']);
        $eq = $binding ? db_query('SELECT employee_id,custom_data FROM '.TB_PREF.'employees WHERE employee_number='.(int)$item['employee_number'].' LIMIT 2',
            'could not read current Employee cleanup completion projection') : false;
        $employee = $eq && db_num_rows($eq) === 1 ? db_fetch_assoc($eq) : false;
        $current = $binding ? hrm_compatibility_profile_current((int)$item['worker_id'], false) : false;
        $current_values = $current ? hrm_compatibility_profile_open($current) : false;
        $current_json = is_array($current_values) && isset($current_values['custom_data'])
            ? hrm_typed_attribute_legacy_cleanup_completion_decode_object((string)$current_values['custom_data']) : false;
        if (!$binding || (int)$binding['worker_id'] !== (int)$item['worker_id'] || !$employee || $current_values === false || $current_json === false
            || !hrm_typed_attribute_legacy_custom_data_parity((string)$employee['custom_data'], (string)$current_values['custom_data'])
            || array_key_exists((string)$mapping['legacy_key'], $current_json))
            return hrm_typed_attribute_legacy_cleanup_completion_result('blocked_current_projection', $request, $verified, count($items)-$verified);
        ++$verified;
    }

    $cq = db_query('SELECT COUNT(*) n FROM '.TB_PREF.'hrm_attribute_legacy_cleanup_receipts WHERE cleanup_request_id='.(int)$request['cleanup_request_id'],
        'could not count cleanup completion receipts');
    $count = $cq ? db_fetch_assoc($cq) : false;
    if (!$count || (int)$count['n'] !== count($items))
        return hrm_typed_attribute_legacy_cleanup_completion_result('blocked_receipt_count', $request, $verified, count($items)-$verified);
    return hrm_typed_attribute_legacy_cleanup_completion_result('completed_cleanup', $request, $verified, 0);
}
