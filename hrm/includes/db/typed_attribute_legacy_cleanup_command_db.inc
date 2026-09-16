<?php
/**
 * HRM-FND-007 maker request for destructive legacy-key cleanup authorization.
 *
 * This boundary records intent only. It requires completed bulk reconciliation,
 * binds exact conversion/recovery evidence, and never rewrites employees.custom_data.
 */
if (!function_exists('get_hrm_typed_attribute_legacy_bulk_reconciliation'))
    include_once($path_to_root.'/hrm/includes/db/typed_attribute_legacy_bulk_reconciliation_db.inc');
if (!function_exists('append_domain_audit_event'))
    include_once($path_to_root.'/includes/db/domain_audit_db.inc');

function hrm_typed_attribute_legacy_cleanup_positive_id($value)
{
    if (is_int($value)) return $value > 0 ? $value : false;
    if (!is_string($value) || !preg_match('/^[1-9][0-9]*$/D', $value)) return false;
    $id=(int)$value;
    return $id > 0 ? $id : false;
}

function hrm_typed_attribute_legacy_cleanup_user_can_manage()
{
    return function_exists('user_check_access')
        && user_check_access('SA_HRM_VIEW_TYPED_ATTRIBUTE_LEGACY_BULK_CANDIDATES')
        && user_check_access('SA_HRM_VIEW_TYPED_ATTRIBUTE_LEGACY_CONVERSION_STATUS')
        && user_check_access('SA_HRM_VIEW_TYPED_ATTRIBUTE_LEGACY_MAPPING')
        && user_check_access('SA_HRM_VIEW_TYPED_ATTRIBUTE_LEGACY_BULK_RECONCILIATION')
        && user_check_access('SA_HRM_MANAGE_TYPED_ATTRIBUTE_LEGACY_CLEANUP');
}

function hrm_typed_attribute_legacy_cleanup_nonce($value)
{
    if (!is_string($value) || strlen($value) < 16 || strlen($value) > 128) return false;
    return preg_match('/^[A-Za-z0-9][A-Za-z0-9_.:-]{15,127}$/D', $value) === 1 ? $value : false;
}

function hrm_typed_attribute_legacy_cleanup_recovery_digest($batch, $items)
{
    if (!is_array($batch) || !is_array($items) || count($items) < 1 || count($items) > 25
        || (int)$batch['candidate_count'] !== count($items)) return false;
    $parts=array();
    foreach ($items as $item) {
        foreach (array('employee_number','worker_id','profile_version_id','definition_version_id','candidate_digest','conversion_id','execution_digest') as $field)
            if (!array_key_exists($field,$item)) return false;
        if ((int)$item['employee_number'] <= 0 || (int)$item['worker_id'] <= 0 || (int)$item['profile_version_id'] <= 0
            || (int)$item['definition_version_id'] <= 0 || (int)$item['conversion_id'] <= 0
            || !is_string($item['candidate_digest']) || preg_match('/^[a-f0-9]{64}$/D',$item['candidate_digest']) !== 1
            || !is_string($item['execution_digest']) || preg_match('/^[a-f0-9]{64}$/D',$item['execution_digest']) !== 1) return false;
        $parts[]=(int)$item['employee_number'].':'.(int)$item['worker_id'].':'.(int)$item['profile_version_id'].':'
            .(int)$item['definition_version_id'].':'.$item['candidate_digest'].':'.(int)$item['conversion_id'].':'.$item['execution_digest'];
    }
    return hash('sha256','hrm-fnd-007-legacy-cleanup-recovery-v1|batch='.(int)$batch['batch_id'].'|mapping='.(int)$batch['mapping_version_id']
        .'|candidate_set='.(string)$batch['candidate_set_digest'].'|'.implode('|',$parts));
}

/** Build exact redacted reconciliation/recovery evidence for one completed batch. */
function hrm_typed_attribute_legacy_cleanup_evidence($batch_id)
{
    $batch_id=hrm_typed_attribute_legacy_cleanup_positive_id($batch_id);
    if ($batch_id === false) return false;
    $reconciliation=get_hrm_typed_attribute_legacy_bulk_reconciliation($batch_id);
    if (!is_array($reconciliation) || ($reconciliation['status']??'') !== 'reconciled_batch'
        || empty($reconciliation['read_only']) || empty($reconciliation['legacy_keys_retained'])
        || !empty($reconciliation['legacy_custom_data_rewritten']) || !empty($reconciliation['plaintext_exposed'])) return false;

    $q=db_query('SELECT batch_id,mapping_version_id,candidate_set_digest,candidate_count,batch_status,maker_id,checker_id,approved_at,row_version FROM '
        .TB_PREF.'hrm_attribute_legacy_conversion_batches WHERE batch_id='.(int)$batch_id.' LIMIT 2',
        'could not read completed legacy bulk batch for cleanup authorization');
    $batch=$q&&db_num_rows($q)===1?db_fetch_assoc($q):false;
    if (!$batch || (string)$batch['batch_status'] !== 'completed' || $batch['checker_id'] === null
        || (int)$batch['checker_id'] === (int)$batch['maker_id'] || $batch['approved_at'] === null
        || (int)$batch['row_version'] < 3 || (int)$batch['candidate_count'] < 1 || (int)$batch['candidate_count'] > 25) return false;

    $q=db_query('SELECT i.employee_number,i.worker_id,i.profile_version_id,i.definition_version_id,i.candidate_digest,i.item_state,i.conversion_id,'
        .'r.mapping_version_id receipt_mapping_version_id,r.worker_id receipt_worker_id,r.profile_version_id receipt_profile_version_id,'
        .'r.definition_version_id receipt_definition_version_id,r.execution_digest FROM '.TB_PREF.'hrm_attribute_legacy_conversion_batch_items i '
        .'JOIN '.TB_PREF.'hrm_attribute_legacy_conversion_receipts r ON r.conversion_id=i.conversion_id WHERE i.batch_id='.(int)$batch_id
        .' ORDER BY i.employee_number,i.batch_item_id', 'could not read completed legacy bulk batch recovery evidence');
    if (!$q) return false;
    $items=array();
    while (is_array($row=db_fetch_assoc($q))) {
        if ((string)$row['item_state'] !== 'converted' || $row['conversion_id'] === null
            || (int)$row['receipt_mapping_version_id'] !== (int)$batch['mapping_version_id']
            || (int)$row['receipt_worker_id'] !== (int)$row['worker_id']
            || (int)$row['receipt_profile_version_id'] !== (int)$row['profile_version_id']
            || (int)$row['receipt_definition_version_id'] !== (int)$row['definition_version_id']) return false;
        $items[]=$row;
    }
    if (count($items) !== (int)$batch['candidate_count']) return false;
    $recovery=hrm_typed_attribute_legacy_cleanup_recovery_digest($batch,$items);
    if ($recovery === false) return false;
    $digest=hash('sha256','hrm-fnd-007-legacy-cleanup-request-v1|batch='.(int)$batch_id.'|mapping='.(int)$batch['mapping_version_id']
        .'|candidate_set='.(string)$batch['candidate_set_digest'].'|recovery='.$recovery.'|count='.(int)$batch['candidate_count']);
    return array('batch_id'=>(int)$batch_id,'mapping_version_id'=>(int)$batch['mapping_version_id'],
        'candidate_set_digest'=>(string)$batch['candidate_set_digest'],'key_count'=>(int)$batch['candidate_count'],
        'reconciliation_digest'=>$digest,'recovery_set_digest'=>$recovery,'legacy_keys_retained'=>true,
        'legacy_custom_data_rewritten'=>false,'destructive_mutation_performed'=>false,'plaintext_exposed'=>false);
}

function hrm_typed_attribute_legacy_cleanup_canonical_command($input)
{
    if (!is_array($input) || array_diff(array_keys($input),array('batch_id','request_nonce'))
        || !array_key_exists('batch_id',$input) || !array_key_exists('request_nonce',$input)) return false;
    $batch=hrm_typed_attribute_legacy_cleanup_positive_id($input['batch_id']);
    $nonce=hrm_typed_attribute_legacy_cleanup_nonce($input['request_nonce']);
    if ($batch === false || $nonce === false) return false;
    return array('batch_id'=>$batch,'request_nonce_hash'=>hash('sha256','hrm-fnd-007-legacy-cleanup-nonce-v1|'.$nonce));
}

/** Create one maker-only cleanup authorization draft. No legacy mutation occurs. */
function hrm_typed_attribute_legacy_cleanup_create_draft($input)
{
    $command=hrm_typed_attribute_legacy_cleanup_canonical_command($input);
    $maker=function_exists('hrm_person_worker_actor_id')?hrm_person_worker_actor_id():0;
    if ($command===false || $maker<=0 || !hrm_typed_attribute_legacy_cleanup_user_can_manage()) return false;
    $evidence=hrm_typed_attribute_legacy_cleanup_evidence($command['batch_id']);
    if ($evidence===false) return false;

    begin_transaction();$ok=true;$request_id=false;
    $q=db_query('SELECT cleanup_request_id,batch_id,request_nonce_hash,reconciliation_digest,recovery_set_digest,key_count,request_status,'
        .'maker_id,checker_id,approved_at,expires_at,row_version FROM '.TB_PREF.'hrm_attribute_legacy_cleanup_requests WHERE batch_id='.(int)$command['batch_id']
        .' OR request_nonce_hash='.db_escape($command['request_nonce_hash']).' ORDER BY cleanup_request_id FOR UPDATE',
        'could not lock legacy cleanup authorization identity');
    if (!$q || db_num_rows($q)>1) $ok=false;
    if ($ok && db_num_rows($q)===1) {
        $row=db_fetch_assoc($q);
        if ((int)$row['batch_id'] !== (int)$command['batch_id'] || (string)$row['request_nonce_hash'] !== (string)$command['request_nonce_hash']
            || (string)$row['reconciliation_digest'] !== (string)$evidence['reconciliation_digest']
            || (string)$row['recovery_set_digest'] !== (string)$evidence['recovery_set_digest'] || (int)$row['key_count'] !== (int)$evidence['key_count']
            || (string)$row['request_status'] !== 'draft' || (int)$row['maker_id'] !== (int)$maker || $row['checker_id'] !== null
            || $row['approved_at'] !== null || (int)$row['row_version'] !== 1) $ok=false;
        if ($ok) {$t=db_query('SELECT UTC_TIMESTAMP() utc_now','could not read cleanup authorization retry time');$now=$t?db_fetch_assoc($t):false;
            if (!$now || (string)$row['expires_at'] <= (string)$now['utc_now']) $ok=false; else $request_id=(int)$row['cleanup_request_id'];}
    }
    if ($ok) {$fresh=hrm_typed_attribute_legacy_cleanup_evidence($command['batch_id']); if ($fresh===false
        || !hash_equals((string)$evidence['reconciliation_digest'],(string)$fresh['reconciliation_digest'])
        || !hash_equals((string)$evidence['recovery_set_digest'],(string)$fresh['recovery_set_digest'])) $ok=false; else $evidence=$fresh;}
    if ($ok && $request_id===false) {
        $ok=db_query('INSERT INTO '.TB_PREF.'hrm_attribute_legacy_cleanup_requests '
            .'(batch_id,request_nonce_hash,reconciliation_digest,recovery_set_digest,key_count,request_status,maker_id,checker_id,approved_at,expires_at,row_version) VALUES ('
            .(int)$command['batch_id'].','.db_escape($command['request_nonce_hash']).','.db_escape($evidence['reconciliation_digest']).','
            .db_escape($evidence['recovery_set_digest']).','.(int)$evidence['key_count'].','."'draft'".','.(int)$maker.',NULL,NULL,DATE_ADD(UTC_TIMESTAMP(),INTERVAL 24 HOUR),1)',
            'could not create legacy cleanup authorization draft')!==false;
        if ($ok) {$request_id=(int)db_insert_id(); if ($request_id<=0)$ok=false;}
        if ($ok) {$audit=append_domain_audit_event(array('company_id'=>function_exists('user_company')?(int)user_company():0,'legal_entity_id'=>0,
            'entity_type'=>'hrm_typed_attribute_legacy_cleanup_request','entity_key'=>array('cleanup_request_id'=>$request_id,'batch_id'=>(int)$command['batch_id']),
            'action'=>'hrm.typed_attribute.legacy_cleanup_drafted','reason_code'=>'hrm_fnd_007_typed_attribute_legacy_cleanup_draft',
            'before'=>array('cleanup_request_present'=>false),'after'=>array('cleanup_request_present'=>true,'request_status'=>'draft','key_count'=>(int)$evidence['key_count']),
            'restricted_diff'=>array('maker_only'=>true,'checker_approval_required'=>true,'reconciliation_digest_bound'=>true,'recovery_set_digest_bound'=>true,
                'authorization_window_hours'=>24,'legacy_key_delete_executed'=>false,'legacy_custom_data_rewritten'=>false,'plaintext_logged'=>false,
                'payroll_consumer_activated'=>false,'work_authorization_delivery_activated'=>false)));
            if ($audit===false)$ok=false;}
    }
    if ($ok) commit_transaction(); else cancel_transaction();
    return $ok?$request_id:false;
}
