<?php
/** HRM-FND-007 distinct-checker approval for one frozen legacy conversion batch. */
if (!function_exists('hrm_typed_attribute_legacy_bulk_batch_create_draft'))
    include_once($path_to_root.'/hrm/includes/db/typed_attribute_legacy_bulk_batch_command_db.inc');

function hrm_typed_attribute_legacy_bulk_batch_user_can_approve()
{
    return function_exists('user_check_access')
        && user_check_access('SA_HRM_VIEW_TYPED_ATTRIBUTE_LEGACY_BULK_CANDIDATES')
        && user_check_access('SA_HRM_APPROVE_TYPED_ATTRIBUTE_LEGACY_BATCH');
}

function hrm_typed_attribute_legacy_bulk_batch_command_from_locked_rows($batch,$items)
{
    if (!is_array($batch)||!is_array($items)||count($items)<1||count($items)>25||(int)$batch['candidate_count']!==count($items)) return false;
    $candidates=array(); foreach($items as $item){
        if((string)$item['item_state']!=='pending'||$item['conversion_id']!==null)return false;
        $candidates[]=array('employee_number'=>(int)$item['employee_number'],'worker_id'=>(int)$item['worker_id'],
            'profile_version_id'=>(int)$item['profile_version_id'],'definition_version_id'=>(int)$item['definition_version_id'],
            'candidate_digest'=>(string)$item['candidate_digest']);
    }
    $digest=hrm_typed_attribute_legacy_bulk_batch_digest((int)$batch['mapping_version_id'],(string)$batch['candidate_as_of'],$candidates);
    if($digest===false||!hash_equals($digest,(string)$batch['candidate_set_digest']))return false;
    return array('mapping_version_id'=>(int)$batch['mapping_version_id'],'candidate_as_of'=>(string)$batch['candidate_as_of'],
        'request_nonce_hash'=>(string)$batch['request_nonce_hash'],'candidate_set_digest'=>$digest,'candidate_count'=>count($candidates),'candidates'=>$candidates);
}

/** Approve one exact draft; checker must differ from maker and all candidates are revalidated again. */
function hrm_typed_attribute_legacy_bulk_batch_approve($batch_id)
{
    $batch_id=hrm_typed_attribute_legacy_mapping_positive_id($batch_id,false);
    $checker=function_exists('hrm_person_worker_actor_id')?hrm_person_worker_actor_id():0;
    if($batch_id===false||$checker<=0||!hrm_typed_attribute_legacy_bulk_batch_user_can_approve())return false;
    begin_transaction();$ok=true;$already=false;
    $q=db_query('SELECT batch_id,mapping_version_id,request_nonce_hash,candidate_set_digest,candidate_as_of,candidate_count,batch_status,'
        .'maker_id,checker_id,approved_at,expires_at,row_version FROM '.TB_PREF.'hrm_attribute_legacy_conversion_batches WHERE batch_id='.(int)$batch_id.' LIMIT 2 FOR UPDATE',
        'could not lock legacy bulk batch for approval');
    $batch=$q&&db_num_rows($q)===1?db_fetch_assoc($q):false;if(!$batch)$ok=false;
    if($ok&&(int)$batch['maker_id']===$checker)$ok=false;
    if($ok){$clock=db_query('SELECT UTC_TIMESTAMP() utc_now','could not read bulk batch approval time');$now=$clock?db_fetch_assoc($clock):false;
        if(!$now||(string)$batch['expires_at']<=(string)$now['utc_now'])$ok=false;}
    if($ok){$r=db_query('SELECT batch_item_id,employee_number,worker_id,profile_version_id,definition_version_id,candidate_digest,item_state,conversion_id FROM '
        .TB_PREF.'hrm_attribute_legacy_conversion_batch_items WHERE batch_id='.(int)$batch_id.' ORDER BY employee_number,batch_item_id FOR UPDATE',
        'could not lock legacy bulk batch items for approval');if(!$r)$ok=false;else{$items=array();while(is_array($row=db_fetch_assoc($r)))$items[]=$row;}}
    $command=$ok?hrm_typed_attribute_legacy_bulk_batch_command_from_locked_rows($batch,$items):false;if($command===false)$ok=false;
    if($ok){
        if((string)$batch['batch_status']==='approved'){
            if($batch['checker_id']===null||(int)$batch['checker_id']!==$checker||$batch['approved_at']===null||(int)$batch['row_version']!==2)$ok=false;else$already=true;
        }elseif((string)$batch['batch_status']!=='draft'||$batch['checker_id']!==null||$batch['approved_at']!==null||(int)$batch['row_version']!==1)$ok=false;
    }
    if($ok&&hrm_typed_attribute_legacy_bulk_batch_revalidate_candidates($command)===false)$ok=false;
    if($ok&&!$already){
        $ok=db_query('UPDATE '.TB_PREF.'hrm_attribute_legacy_conversion_batches SET batch_status='."'approved'".',checker_id='.(int)$checker
            .',approved_at=UTC_TIMESTAMP(),row_version=2 WHERE batch_id='.(int)$batch_id." AND batch_status='draft' AND checker_id IS NULL AND approved_at IS NULL AND row_version=1",
            'could not approve legacy Typed Attribute bulk batch')!==false && function_exists('db_num_affected_rows') && db_num_affected_rows()===1;
        if($ok){$audit=append_domain_audit_event(array('company_id'=>function_exists('user_company')?(int)user_company():0,'legal_entity_id'=>0,
            'entity_type'=>'hrm_typed_attribute_legacy_conversion_batch','entity_key'=>array('batch_id'=>(int)$batch_id,'mapping_version_id'=>(int)$batch['mapping_version_id']),
            'action'=>'hrm.typed_attribute.legacy_bulk_batch_approved','reason_code'=>'hrm_fnd_007_typed_attribute_legacy_batch_approval',
            'before'=>array('batch_status'=>'draft','checker_id'=>null),'after'=>array('batch_status'=>'approved','checker_id'=>(int)$checker),
            'restricted_diff'=>array('checker_distinct_from_maker'=>true,'candidate_set_digest_verified'=>true,'transactional_revalidation'=>true,
                'candidate_count'=>(int)$batch['candidate_count'],'plaintext_logged'=>false,'legacy_custom_data_rewritten'=>false,
                'typed_attribute_values_written'=>0,'conversion_receipts_written'=>0,'bulk_executor_activated'=>false,'payroll_consumer_activated'=>false)));
            if($audit===false)$ok=false;}
    }
    if($ok)commit_transaction();else cancel_transaction();
    return $ok?(int)$batch_id:false;
}
