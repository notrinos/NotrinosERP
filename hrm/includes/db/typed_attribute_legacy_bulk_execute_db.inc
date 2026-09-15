<?php
/**
 * HRM-FND-007 all-or-nothing executor for one approved legacy conversion batch.
 *
 * The executor never rewrites retained employees.custom_data. It locks the exact
 * frozen batch, revalidates every candidate, then appends all encrypted values,
 * conversion receipts, item links and audit evidence inside one transaction.
 * Any failure rolls the entire batch back to its pre-execution approved state.
 */
if (!function_exists('hrm_typed_attribute_legacy_bulk_batch_command_from_locked_rows'))
    include_once($path_to_root.'/hrm/includes/db/typed_attribute_legacy_bulk_batch_approve_db.inc');
if (!function_exists('hrm_typed_attribute_legacy_conversion_receipt_digest'))
    include_once($path_to_root.'/hrm/includes/db/typed_attribute_legacy_conversion_db.inc');

function hrm_typed_attribute_legacy_bulk_batch_user_can_execute()
{
    return function_exists('user_check_access')
        && user_check_access('SA_HRM_VIEW_TYPED_ATTRIBUTE_LEGACY_BULK_CANDIDATES')
        && user_check_access('SA_HRM_EXECUTE_TYPED_ATTRIBUTE_LEGACY_CONVERSION')
        && user_check_access('SA_HRM_EXECUTE_TYPED_ATTRIBUTE_LEGACY_BATCH');
}

function hrm_typed_attribute_legacy_bulk_batch_lock($batch_id)
{
    $q=db_query('SELECT batch_id,mapping_version_id,request_nonce_hash,candidate_set_digest,candidate_as_of,candidate_count,batch_status,'
        .'maker_id,checker_id,approved_at,expires_at,row_version FROM '.TB_PREF.'hrm_attribute_legacy_conversion_batches WHERE batch_id='.(int)$batch_id.' LIMIT 2 FOR UPDATE',
        'could not lock approved legacy bulk batch for execution');
    return $q&&db_num_rows($q)===1?db_fetch_assoc($q):false;
}

function hrm_typed_attribute_legacy_bulk_batch_lock_items($batch_id)
{
    $q=db_query('SELECT batch_item_id,employee_number,worker_id,profile_version_id,definition_version_id,candidate_digest,item_state,conversion_id FROM '
        .TB_PREF.'hrm_attribute_legacy_conversion_batch_items WHERE batch_id='.(int)$batch_id.' ORDER BY employee_number,batch_item_id FOR UPDATE',
        'could not lock approved legacy bulk batch items for execution');
    if(!$q)return false;
    $rows=array();while(is_array($row=db_fetch_assoc($q)))$rows[]=$row;
    return $rows;
}

function hrm_typed_attribute_legacy_bulk_batch_completed_matches($batch,$items)
{
    if(!is_array($batch)||!is_array($items)||(string)$batch['batch_status']!=='completed'
        ||(int)$batch['candidate_count']!==count($items)||count($items)<1||count($items)>25
        ||$batch['checker_id']===null||(int)$batch['checker_id']===(int)$batch['maker_id']
        ||$batch['approved_at']===null||(int)$batch['row_version']<3)return false;
    $mq=db_query('SELECT mapping_version_id,legacy_key FROM '.TB_PREF.'hrm_attribute_legacy_mapping_versions WHERE mapping_version_id='.(int)$batch['mapping_version_id'].' LIMIT 2 FOR UPDATE',
        'could not validate completed legacy bulk conversion mapping');
    $mapping=$mq&&db_num_rows($mq)===1?db_fetch_assoc($mq):false;if(!$mapping)return false;
    $candidates=array();
    foreach($items as $item){
        if((string)$item['item_state']!=='converted'||$item['conversion_id']===null||(int)$item['conversion_id']<=0)return false;
        $expected_candidate=hrm_typed_attribute_legacy_bulk_candidate_digest((int)$batch['mapping_version_id'],(int)$item['worker_id'],
            (int)$item['profile_version_id'],(int)$item['definition_version_id']);
        if($expected_candidate===false||!hash_equals($expected_candidate,(string)$item['candidate_digest']))return false;
        $candidates[]=array('employee_number'=>(int)$item['employee_number'],'worker_id'=>(int)$item['worker_id'],
            'profile_version_id'=>(int)$item['profile_version_id'],'definition_version_id'=>(int)$item['definition_version_id'],
            'candidate_digest'=>(string)$item['candidate_digest']);
        $q=db_query('SELECT conversion_id,mapping_version_id,worker_id,profile_version_id,definition_version_id,attribute_value_id,legacy_key,execution_digest,converted_by FROM '
            .TB_PREF.'hrm_attribute_legacy_conversion_receipts WHERE conversion_id='.(int)$item['conversion_id'].' LIMIT 2 FOR UPDATE',
            'could not validate completed legacy bulk conversion receipt');
        $receipt=$q&&db_num_rows($q)===1?db_fetch_assoc($q):false;
        $expected_receipt=hrm_typed_attribute_legacy_conversion_receipt_digest((int)$batch['mapping_version_id'],(int)$item['worker_id'],
            (int)$item['profile_version_id'],(int)$item['definition_version_id']);
        if(!$receipt||$expected_receipt===false||!hash_equals($expected_receipt,(string)$receipt['execution_digest'])
            ||(int)$receipt['mapping_version_id']!==(int)$batch['mapping_version_id']||(int)$receipt['worker_id']!==(int)$item['worker_id']
            ||(int)$receipt['profile_version_id']!==(int)$item['profile_version_id']||(int)$receipt['definition_version_id']!==(int)$item['definition_version_id']
            ||(string)$receipt['legacy_key']!==(string)$mapping['legacy_key']||(int)$receipt['attribute_value_id']<=0||(int)$receipt['converted_by']<=0)return false;
        $v=db_query('SELECT attribute_value_id,definition_version_id,subject_type,subject_id,value_version_no,predecessor_id,source FROM '
            .TB_PREF.'hrm_attribute_values WHERE attribute_value_id='.(int)$receipt['attribute_value_id'].' LIMIT 2 FOR UPDATE',
            'could not validate completed legacy bulk Typed Attribute value');
        $value=$v&&db_num_rows($v)===1?db_fetch_assoc($v):false;
        if(!$value||(int)$value['definition_version_id']!==(int)$item['definition_version_id']
            ||(string)$value['subject_type']!=='worker'||(int)$value['subject_id']!==(int)$item['worker_id']
            ||(int)$value['value_version_no']!==1||$value['predecessor_id']!==null||(string)$value['source']!=='legacy_convert')return false;
    }
    $set_digest=hrm_typed_attribute_legacy_bulk_batch_digest((int)$batch['mapping_version_id'],(string)$batch['candidate_as_of'],$candidates);
    return $set_digest!==false&&hash_equals($set_digest,(string)$batch['candidate_set_digest']);
}

/** Execute exactly one approved batch. Exact completed retry is idempotent. */
function hrm_typed_attribute_legacy_bulk_batch_execute($batch_id)
{
    $batch_id=hrm_typed_attribute_legacy_mapping_positive_id($batch_id,false);
    $actor=function_exists('hrm_person_worker_actor_id')?hrm_person_worker_actor_id():0;
    if($batch_id===false||$actor<=0||!hrm_typed_attribute_legacy_bulk_batch_user_can_execute())return false;

    begin_transaction();$ok=true;$batch=hrm_typed_attribute_legacy_bulk_batch_lock($batch_id);$items=false;
    if(!$batch)$ok=false;
    if($ok){$items=hrm_typed_attribute_legacy_bulk_batch_lock_items($batch_id);if($items===false)$ok=false;}
    if($ok&&(string)$batch['batch_status']==='completed'){
        $ok=hrm_typed_attribute_legacy_bulk_batch_completed_matches($batch,$items);
        if($ok)commit_transaction();else cancel_transaction();
        return $ok?(int)$batch_id:false;
    }
    if($ok&&((string)$batch['batch_status']!=='approved'||$batch['checker_id']===null
        ||(int)$batch['checker_id']===(int)$batch['maker_id']||$batch['approved_at']===null||(int)$batch['row_version']<2))$ok=false;
    if($ok){$clock=db_query('SELECT UTC_TIMESTAMP() utc_now','could not read legacy bulk batch execution time');$now=$clock?db_fetch_assoc($clock):false;
        if(!$now||(string)$batch['expires_at']<=(string)$now['utc_now'])$ok=false;}
    $command=$ok?hrm_typed_attribute_legacy_bulk_batch_command_from_locked_rows($batch,$items):false;
    if($command===false)$ok=false;
    $custody=$ok?hrm_typed_attribute_legacy_bulk_batch_revalidate_candidates($command):false;
    if($custody===false||!isset($custody['mapping'],$custody['definition'],$custody['locked']))$ok=false;
    if($ok&&!hrm_typed_attribute_legacy_conversion_user_can_execute($custody['definition']))$ok=false;
    $active_key=$ok?hrm_person_sensitive_active_key(null,false):false;
    if($ok&&(!is_array($active_key)||empty($active_key['key_id'])||empty($active_key['key'])))$ok=false;
    $now=$ok?hrm_person_worker_utc_now():false;if($ok&&$now===false)$ok=false;

    $conversion_ids=array();
    if($ok){
        $locked_by_worker=array();foreach($custody['locked'] as $locked)$locked_by_worker[(int)$locked['candidate']['worker_id']]=$locked;
        foreach($items as $item){
            if(!$ok)break;
            $worker_id=(int)$item['worker_id'];$locked=isset($locked_by_worker[$worker_id])?$locked_by_worker[$worker_id]:false;
            if(!$locked||(string)$item['item_state']!=='pending'||$item['conversion_id']!==null)$ok=false;
            $extracted=$ok?hrm_typed_attribute_legacy_conversion_extract_scalar($locked['retained_custom_data'],(string)$custody['mapping']['legacy_key']):false;
            $canonical=$extracted===false?false:hrm_typed_attribute_value_apply_validation($custody['definition'],$extracted['value']);
            if($canonical===false)$ok=false;
            $context=$ok?hrm_typed_attribute_crypto_context((int)$item['definition_version_id'],'worker',$worker_id,1,null):false;
            $cipher=$context===false?false:hrm_person_sensitive_encrypt($canonical,$context,$active_key['key']);if($cipher===false)$ok=false;
            if($ok){$hash=hash('sha256',$cipher);$ok=db_query('INSERT INTO '.TB_PREF.'hrm_attribute_values '
                .'(definition_version_id,subject_type,subject_id,value_version_no,predecessor_id,effective_from,effective_to,key_id,value_ciphertext,ciphertext_sha256,source,created_by) VALUES ('
                .(int)$item['definition_version_id'].",'worker',".$worker_id.',1,NULL,'.db_escape($now).',NULL,'.db_escape($active_key['key_id']).','
                .db_escape($cipher).','.db_escape($hash).",'legacy_convert',".(int)$actor.')',
                'could not append bulk legacy-converted encrypted Typed Attribute value')!==false;}
            $attribute_value_id=$ok?(int)db_insert_id():0;
            $digest=$ok?hrm_typed_attribute_legacy_conversion_receipt_digest((int)$batch['mapping_version_id'],$worker_id,
                (int)$item['profile_version_id'],(int)$item['definition_version_id']):false;
            if($ok&&($attribute_value_id<=0||$digest===false))$ok=false;
            if($ok)$ok=db_query('INSERT INTO '.TB_PREF.'hrm_attribute_legacy_conversion_receipts '
                .'(mapping_version_id,worker_id,profile_version_id,definition_version_id,attribute_value_id,legacy_key,execution_digest,converted_by) VALUES ('
                .(int)$batch['mapping_version_id'].','.$worker_id.','.(int)$item['profile_version_id'].','.(int)$item['definition_version_id'].','
                .$attribute_value_id.','.db_escape($custody['mapping']['legacy_key']).','.db_escape($digest).','.(int)$actor.')',
                'could not append bulk legacy Typed Attribute conversion receipt')!==false;
            $conversion_id=$ok?(int)db_insert_id():0;if($ok&&$conversion_id<=0)$ok=false;
            if($ok)$ok=db_query('UPDATE '.TB_PREF.'hrm_attribute_legacy_conversion_batch_items SET item_state='."'converted'".',conversion_id='.$conversion_id
                .' WHERE batch_item_id='.(int)$item['batch_item_id']." AND item_state='pending' AND conversion_id IS NULL",
                'could not bind bulk legacy conversion receipt to frozen batch item')!==false
                &&function_exists('db_num_affected_rows')&&db_num_affected_rows()===1;
            if($ok){$audit=append_domain_audit_event(array('company_id'=>function_exists('user_company')?(int)user_company():0,'legal_entity_id'=>0,
                'entity_type'=>'hrm_typed_attribute_legacy_conversion','entity_key'=>array('conversion_id'=>$conversion_id,'batch_id'=>(int)$batch_id,
                    'mapping_version_id'=>(int)$batch['mapping_version_id'],'worker_id'=>$worker_id,'attribute_value_id'=>$attribute_value_id),
                'action'=>'hrm.typed_attribute.legacy_value_converted','reason_code'=>'hrm_fnd_007_typed_attribute_legacy_bulk_execution',
                'before'=>array('typed_attribute_value_present'=>false),'after'=>array('typed_attribute_value_present'=>true,'source'=>'legacy_convert'),
                'restricted_diff'=>array('bulk_batch_id'=>(int)$batch_id,'exact_fnd_006_profile_parity'=>true,'approved_mapping_exact'=>true,
                    'active_definition_exact'=>true,'scalar_key_only'=>true,'plaintext_logged'=>false,'encrypted_value'=>true,
                    'receipt_idempotency'=>true,'legacy_key_retained'=>true,'legacy_custom_data_rewritten'=>false,'payroll_consumer_activated'=>false)));
                if($audit===false)$ok=false;}
            if($ok)$conversion_ids[]=$conversion_id;
        }
    }
    if($ok&&count($conversion_ids)!==(int)$batch['candidate_count'])$ok=false;
    if($ok)$ok=db_query('UPDATE '.TB_PREF.'hrm_attribute_legacy_conversion_batches SET batch_status='."'completed'".',row_version=row_version+1 WHERE batch_id='.(int)$batch_id
        ." AND batch_status='approved' AND row_version=".(int)$batch['row_version'],
        'could not complete approved legacy Typed Attribute bulk batch')!==false
        &&function_exists('db_num_affected_rows')&&db_num_affected_rows()===1;
    if($ok){$audit=append_domain_audit_event(array('company_id'=>function_exists('user_company')?(int)user_company():0,'legal_entity_id'=>0,
        'entity_type'=>'hrm_typed_attribute_legacy_conversion_batch','entity_key'=>array('batch_id'=>(int)$batch_id,'mapping_version_id'=>(int)$batch['mapping_version_id']),
        'action'=>'hrm.typed_attribute.legacy_bulk_batch_completed','reason_code'=>'hrm_fnd_007_typed_attribute_legacy_bulk_execution',
        'before'=>array('batch_status'=>'approved'),'after'=>array('batch_status'=>'completed','converted_count'=>count($conversion_ids)),
        'restricted_diff'=>array('all_or_nothing_transaction'=>true,'candidate_set_digest_verified'=>true,'transactional_revalidation'=>true,
            'encrypted_values_written'=>count($conversion_ids),'conversion_receipts_written'=>count($conversion_ids),'plaintext_logged'=>false,
            'legacy_key_retained'=>true,'legacy_custom_data_rewritten'=>false,'payroll_consumer_activated'=>false,'historical_payroll_accounting_rows_rewritten'=>0)));
        if($audit===false)$ok=false;}
    if($ok)commit_transaction();else cancel_transaction();
    return $ok?(int)$batch_id:false;
}
