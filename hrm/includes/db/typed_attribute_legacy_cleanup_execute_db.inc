<?php
/**
 * HRM-FND-007 all-or-nothing destructive cleanup executor for one approved request.
 *
 * This command deletes exactly one approved mapped legacy custom_data key from every
 * member of one completed conversion batch. It never updates immutable Worker profile
 * history in place: each changed Worker receives one successor retained profile and the
 * legacy Employee row is rematerialized through the accepted FND-006 projector. Cleanup
 * receipts bind the pre/post profile versions without storing legacy plaintext.
 */
if (!function_exists('hrm_typed_attribute_legacy_cleanup_approve'))
    include_once($path_to_root.'/hrm/includes/db/typed_attribute_legacy_cleanup_approve_db.inc');
if (!function_exists('hrm_employee_projection_materialize_person'))
    include_once($path_to_root.'/hrm/includes/db/employee_compatibility_projector_db.inc');

function hrm_typed_attribute_legacy_cleanup_user_can_execute()
{
    return function_exists('user_check_access')
        && user_check_access('SA_HRM_VIEW_TYPED_ATTRIBUTE_LEGACY_BULK_CANDIDATES')
        && user_check_access('SA_HRM_VIEW_TYPED_ATTRIBUTE_LEGACY_CONVERSION_STATUS')
        && user_check_access('SA_HRM_VIEW_TYPED_ATTRIBUTE_LEGACY_MAPPING')
        && user_check_access('SA_HRM_VIEW_TYPED_ATTRIBUTE_LEGACY_BULK_RECONCILIATION')
        && user_check_access('SA_HRM_EXECUTE_TYPED_ATTRIBUTE_LEGACY_CLEANUP');
}

/** Decode bounded top-level legacy JSON without returning it outside the executor. */
function hrm_typed_attribute_legacy_cleanup_decode_object($raw)
{
    if (!is_string($raw) || strlen($raw) > 1048576) return false;
    $trim=ltrim($raw); if ($trim==='' || $trim[0]!=='{') return false;
    $decoded=json_decode($raw,true);
    if (!is_array($decoded) || json_last_error()!==JSON_ERROR_NONE || count($decoded)>100) return false;
    foreach ($decoded as $key=>$value)
        if (hrm_typed_attribute_legacy_mapping_key((string)$key)===false) return false;
    return $decoded;
}

/** Remove exactly one scalar mapped key; preserve all remaining decoded values. */
function hrm_typed_attribute_legacy_cleanup_remove_key($raw,$legacy_key)
{
    $legacy_key=hrm_typed_attribute_legacy_mapping_key($legacy_key);
    $decoded=$legacy_key===false?false:hrm_typed_attribute_legacy_cleanup_decode_object($raw);
    if ($decoded===false || !array_key_exists($legacy_key,$decoded) || !is_scalar($decoded[$legacy_key])) return false;
    unset($decoded[$legacy_key]);
    $encoded=count($decoded)===0?'{}':json_encode($decoded);
    if (!is_string($encoded) || strlen($encoded)>1048576) return false;
    $round=hrm_typed_attribute_legacy_cleanup_decode_object($encoded);
    return $round!==false && $round===$decoded ? $encoded : false;
}

function hrm_typed_attribute_legacy_cleanup_request_lock($cleanup_request_id)
{
    $q=db_query('SELECT cleanup_request_id,batch_id,request_nonce_hash,reconciliation_digest,recovery_set_digest,key_count,request_status,'
        .'maker_id,checker_id,approved_at,expires_at,row_version FROM '.TB_PREF.'hrm_attribute_legacy_cleanup_requests WHERE cleanup_request_id='.(int)$cleanup_request_id
        .' LIMIT 2 FOR UPDATE','could not lock approved legacy cleanup request for execution');
    return $q&&db_num_rows($q)===1?db_fetch_assoc($q):false;
}

function hrm_typed_attribute_legacy_cleanup_batch_lock($batch_id)
{
    $q=db_query('SELECT batch_id,mapping_version_id,candidate_set_digest,candidate_as_of,candidate_count,batch_status,maker_id,checker_id,approved_at,row_version FROM '
        .TB_PREF.'hrm_attribute_legacy_conversion_batches WHERE batch_id='.(int)$batch_id.' LIMIT 2 FOR UPDATE',
        'could not lock completed legacy conversion batch for cleanup execution');
    return $q&&db_num_rows($q)===1?db_fetch_assoc($q):false;
}

function hrm_typed_attribute_legacy_cleanup_batch_items_lock($batch_id)
{
    $q=db_query('SELECT batch_item_id,employee_number,worker_id,profile_version_id,definition_version_id,candidate_digest,item_state,conversion_id FROM '
        .TB_PREF.'hrm_attribute_legacy_conversion_batch_items WHERE batch_id='.(int)$batch_id.' ORDER BY employee_number,batch_item_id FOR UPDATE',
        'could not lock converted legacy batch items for cleanup execution');
    if(!$q)return false;$rows=array();while(is_array($row=db_fetch_assoc($q)))$rows[]=$row;return $rows;
}

function hrm_typed_attribute_legacy_cleanup_mapping_lock($mapping_version_id)
{
    $q=db_query('SELECT mapping_version_id,legacy_key,version_no,predecessor_id,attribute_code,subject_type,mapping_status,effective_from,effective_to,qualified_review_ref,approved_at,approved_by FROM '
        .TB_PREF.'hrm_attribute_legacy_mapping_versions WHERE mapping_version_id='.(int)$mapping_version_id.' LIMIT 2 FOR UPDATE',
        'could not lock exact legacy mapping for cleanup execution');
    return $q&&db_num_rows($q)===1?db_fetch_assoc($q):false;
}

function hrm_typed_attribute_legacy_cleanup_receipt_digest($request,$item,$pre_profile_id,$post_profile_id)
{
    if(!is_array($request)||!is_array($item)||(int)$request['cleanup_request_id']<=0||(int)$request['batch_id']<=0
        ||(int)$item['batch_item_id']<=0||(int)$item['employee_number']<=0||(int)$item['worker_id']<=0
        ||(int)$item['conversion_id']<=0||(int)$pre_profile_id<=0||(int)$post_profile_id<=0
        ||!is_string($request['recovery_set_digest'])||preg_match('/^[a-f0-9]{64}$/D',$request['recovery_set_digest'])!==1)return false;
    return hash('sha256','hrm-fnd-007-legacy-cleanup-execution-v1|request='.(int)$request['cleanup_request_id'].'|batch='.(int)$request['batch_id']
        .'|item='.(int)$item['batch_item_id'].'|employee='.(int)$item['employee_number'].'|worker='.(int)$item['worker_id']
        .'|conversion='.(int)$item['conversion_id'].'|pre_profile='.(int)$pre_profile_id.'|post_profile='.(int)$post_profile_id
        .'|recovery='.(string)$request['recovery_set_digest']);
}

/** Verify immutable cleanup receipts and current key absence for an exact completed retry. */
function hrm_typed_attribute_legacy_cleanup_completed_matches($request)
{
    if(!is_array($request)||(string)$request['request_status']!=='completed'||$request['checker_id']===null
        ||(int)$request['checker_id']===(int)$request['maker_id']||(int)$request['row_version']<3)return false;
    $batch=hrm_typed_attribute_legacy_cleanup_batch_lock((int)$request['batch_id']);
    $items=$batch?hrm_typed_attribute_legacy_cleanup_batch_items_lock((int)$request['batch_id']):false;
    if(!$batch||!is_array($items)||(string)$batch['batch_status']!=='completed'||count($items)!==(int)$request['key_count']
        ||count($items)<1||count($items)>25)return false;
    $mapping=hrm_typed_attribute_legacy_cleanup_mapping_lock((int)$batch['mapping_version_id']);
    if(!$mapping||(string)$mapping['mapping_status']!=='approved'||(string)$mapping['subject_type']!=='worker')return false;
    foreach($items as $item){
        if((string)$item['item_state']!=='converted'||$item['conversion_id']===null)return false;
        $q=db_query('SELECT cleanup_receipt_id,cleanup_request_id,batch_item_id,employee_number,worker_id,mapping_version_id,conversion_id,'
            .'pre_cleanup_profile_version_id,post_cleanup_profile_version_id,execution_digest,executed_by FROM '.TB_PREF.'hrm_attribute_legacy_cleanup_receipts '
            .'WHERE cleanup_request_id='.(int)$request['cleanup_request_id'].' AND batch_item_id='.(int)$item['batch_item_id'].' LIMIT 2 FOR UPDATE',
            'could not validate legacy cleanup execution receipt');
        $receipt=$q&&db_num_rows($q)===1?db_fetch_assoc($q):false;if(!$receipt)return false;
        if((int)$receipt['employee_number']!==(int)$item['employee_number']||(int)$receipt['worker_id']!==(int)$item['worker_id']
            ||(int)$receipt['mapping_version_id']!==(int)$batch['mapping_version_id']||(int)$receipt['conversion_id']!==(int)$item['conversion_id']
            ||(int)$receipt['executed_by']<=0)return false;
        $expected=hrm_typed_attribute_legacy_cleanup_receipt_digest($request,$item,(int)$receipt['pre_cleanup_profile_version_id'],(int)$receipt['post_cleanup_profile_version_id']);
        if($expected===false||!hash_equals($expected,(string)$receipt['execution_digest']))return false;
        $pre=hrm_typed_attribute_legacy_bulk_reconciliation_profile((int)$receipt['pre_cleanup_profile_version_id'],(int)$item['worker_id']);
        $post=hrm_typed_attribute_legacy_bulk_reconciliation_profile((int)$receipt['post_cleanup_profile_version_id'],(int)$item['worker_id']);
        $pre_values=$pre?hrm_compatibility_profile_open($pre):false;$post_values=$post?hrm_compatibility_profile_open($post):false;
        if($pre_values===false||$post_values===false||(int)$post['predecessor_id']!==(int)$pre['profile_version_id']
            ||(int)$post['version_no']!==(int)$pre['version_no']+1)return false;
        $pre_json=hrm_typed_attribute_legacy_cleanup_decode_object($pre_values['custom_data']);
        $post_json=hrm_typed_attribute_legacy_cleanup_decode_object($post_values['custom_data']);
        if($pre_json===false||$post_json===false||!array_key_exists((string)$mapping['legacy_key'],$pre_json)
            ||array_key_exists((string)$mapping['legacy_key'],$post_json))return false;
        unset($pre_json[(string)$mapping['legacy_key']]);if($pre_json!==$post_json)return false;
        $binding=get_hrm_person_worker_binding((int)$item['employee_number']);
        $employee_q=$binding?db_query('SELECT * FROM '.TB_PREF.'employees WHERE employee_number='.(int)$item['employee_number'].' LIMIT 2 FOR UPDATE',
            'could not validate current Employee projection after cleanup'):false;
        $employee=$employee_q&&db_num_rows($employee_q)===1?db_fetch_assoc($employee_q):false;
        $current=$binding?hrm_compatibility_profile_current((int)$item['worker_id'],true):false;
        $current_values=$current?hrm_compatibility_profile_open($current):false;
        $current_json=$current_values!==false?hrm_typed_attribute_legacy_cleanup_decode_object($current_values['custom_data']):false;
        if(!$binding||(int)$binding['worker_id']!==(int)$item['worker_id']||!$employee||$current_values===false||$current_json===false
            ||!hrm_typed_attribute_legacy_custom_data_parity($employee['custom_data'],$current_values['custom_data'])
            ||array_key_exists((string)$mapping['legacy_key'],$current_json))return false;
    }
    $q=db_query('SELECT COUNT(*) n FROM '.TB_PREF.'hrm_attribute_legacy_cleanup_receipts WHERE cleanup_request_id='.(int)$request['cleanup_request_id'],
        'could not count legacy cleanup receipts');$row=$q?db_fetch_assoc($q):false;
    return $row&&(int)$row['n']===count($items);
}

/** Execute one approved cleanup request. Any failure rolls every destructive effect back. */
function hrm_typed_attribute_legacy_cleanup_execute($cleanup_request_id)
{
    $cleanup_request_id=hrm_typed_attribute_legacy_cleanup_positive_id($cleanup_request_id);
    $actor=function_exists('hrm_person_worker_actor_id')?hrm_person_worker_actor_id():0;
    if($cleanup_request_id===false||$actor<=0||!hrm_typed_attribute_legacy_cleanup_user_can_execute())return false;
    begin_transaction();$ok=true;
    $request=hrm_typed_attribute_legacy_cleanup_request_lock($cleanup_request_id);if(!$request)$ok=false;
    if($ok&&(string)$request['request_status']==='completed'){
        $ok=hrm_typed_attribute_legacy_cleanup_completed_matches($request);
        if($ok)commit_transaction();else cancel_transaction();return $ok?(int)$cleanup_request_id:false;
    }
    if($ok&&((string)$request['request_status']!=='approved'||$request['checker_id']===null
        ||(int)$request['checker_id']===(int)$request['maker_id']||(int)$request['maker_id']===$actor||(int)$request['checker_id']===$actor
        ||$request['approved_at']===null||(int)$request['row_version']<2))$ok=false;
    if($ok){$clock=db_query('SELECT UTC_TIMESTAMP() utc_now','could not read legacy cleanup execution time');$now=$clock?db_fetch_assoc($clock):false;
        if(!$now||(string)$request['expires_at']<=(string)$now['utc_now'])$ok=false;}
    $evidence=$ok?hrm_typed_attribute_legacy_cleanup_evidence((int)$request['batch_id']):false;
    if($evidence===false||!hash_equals((string)$request['reconciliation_digest'],(string)$evidence['reconciliation_digest'])
        ||!hash_equals((string)$request['recovery_set_digest'],(string)$evidence['recovery_set_digest'])
        ||(int)$request['key_count']!==(int)$evidence['key_count'])$ok=false;
    $batch=$ok?hrm_typed_attribute_legacy_cleanup_batch_lock((int)$request['batch_id']):false;
    $items=$batch?hrm_typed_attribute_legacy_cleanup_batch_items_lock((int)$request['batch_id']):false;
    if(!$batch||!is_array($items)||(string)$batch['batch_status']!=='completed'||$batch['checker_id']===null
        ||(int)$batch['checker_id']===(int)$batch['maker_id']||$batch['approved_at']===null||(int)$batch['row_version']<3
        ||count($items)!==(int)$request['key_count']||count($items)!==(int)$batch['candidate_count']||count($items)<1||count($items)>25)$ok=false;
    $mapping=$ok?hrm_typed_attribute_legacy_cleanup_mapping_lock((int)$batch['mapping_version_id']):false;
    if(!$mapping||!hrm_typed_attribute_legacy_mapping_validate_metadata($mapping)||(string)$mapping['mapping_status']!=='approved'
        ||(string)$mapping['subject_type']!=='worker'||(string)$batch['candidate_as_of']<(string)$mapping['effective_from']
        ||($mapping['effective_to']!==null&&(string)$batch['candidate_as_of']>(string)$mapping['effective_to']))$ok=false;
    if($ok){$q=db_query('SELECT cleanup_receipt_id FROM '.TB_PREF.'hrm_attribute_legacy_cleanup_receipts WHERE cleanup_request_id='.(int)$cleanup_request_id.' LIMIT 1 FOR UPDATE',
        'could not lock pre-existing cleanup receipts');if(!$q||db_num_rows($q)!==0)$ok=false;}

    // Lock and revalidate the entire frozen evidence set before the first destructive effect.
    $prepared=array();$locked_recovery=array();$locked_candidates=array();
    if($ok){foreach($items as $item){if(!$ok)break;
        if((string)$item['item_state']!=='converted'||$item['conversion_id']===null||(int)$item['conversion_id']<=0){$ok=false;break;}
        $candidate_digest=hrm_typed_attribute_legacy_bulk_candidate_digest((int)$batch['mapping_version_id'],(int)$item['worker_id'],
            (int)$item['profile_version_id'],(int)$item['definition_version_id']);
        if($candidate_digest===false||!hash_equals($candidate_digest,(string)$item['candidate_digest'])){$ok=false;break;}
        $locked_candidates[]=array('employee_number'=>(int)$item['employee_number'],'worker_id'=>(int)$item['worker_id'],
            'profile_version_id'=>(int)$item['profile_version_id'],'definition_version_id'=>(int)$item['definition_version_id'],
            'candidate_digest'=>(string)$item['candidate_digest']);

        $binding=hrm_employment_lock_binding_for_interval_policy((int)$item['employee_number']);
        if(!$binding||(int)$binding['worker_id']!==(int)$item['worker_id']){$ok=false;break;}
        $eq=db_query('SELECT * FROM '.TB_PREF.'employees WHERE employee_number='.(int)$item['employee_number'].' LIMIT 2 FOR UPDATE',
            'could not lock Employee compatibility row for legacy cleanup');
        $employee=$eq&&db_num_rows($eq)===1?db_fetch_assoc($eq):false;
        $current=$employee?hrm_compatibility_profile_current((int)$item['worker_id'],true):false;
        $retained=$current?hrm_compatibility_profile_open($current):false;
        $employee_values=$employee?hrm_compatibility_profile_values($employee):false;
        if(!$employee||$retained===false||$employee_values===false||$employee_values!==$retained){$ok=false;break;}

        $hq=db_query('SELECT * FROM '.TB_PREF.'hrm_worker_profile_versions WHERE profile_version_id='.(int)$item['profile_version_id']
            .' AND worker_id='.(int)$item['worker_id'].' LIMIT 2 FOR UPDATE','could not lock frozen retained profile for legacy cleanup');
        $historical=$hq&&db_num_rows($hq)===1?db_fetch_assoc($hq):false;
        $historical_values=$historical?hrm_compatibility_profile_open($historical):false;
        if($historical_values===false||!array_key_exists('custom_data',$historical_values)
            ||!hrm_typed_attribute_legacy_custom_data_parity($retained['custom_data'],$historical_values['custom_data'])){$ok=false;break;}

        $definition=hrm_typed_attribute_legacy_reconciliation_definition((int)$item['definition_version_id']);
        if(!is_array($definition)||(string)$definition['attribute_code']!==(string)$mapping['attribute_code']
            ||(string)$definition['allowed_subject']!=='worker'||!hrm_typed_attribute_legacy_reconciliation_user_can_view($definition)){$ok=false;break;}
        $rq=db_query('SELECT conversion_id,mapping_version_id,worker_id,profile_version_id,definition_version_id,attribute_value_id,legacy_key,execution_digest FROM '
            .TB_PREF.'hrm_attribute_legacy_conversion_receipts WHERE conversion_id='.(int)$item['conversion_id'].' LIMIT 2 FOR UPDATE',
            'could not lock legacy conversion receipt for cleanup');
        $receipt=$rq&&db_num_rows($rq)===1?db_fetch_assoc($rq):false;
        $expected_receipt=hrm_typed_attribute_legacy_conversion_receipt_digest((int)$batch['mapping_version_id'],(int)$item['worker_id'],
            (int)$item['profile_version_id'],(int)$item['definition_version_id']);
        if(!$receipt||$expected_receipt===false||!hash_equals($expected_receipt,(string)$receipt['execution_digest'])
            ||(int)$receipt['mapping_version_id']!==(int)$batch['mapping_version_id']||(int)$receipt['worker_id']!==(int)$item['worker_id']
            ||(int)$receipt['profile_version_id']!==(int)$item['profile_version_id']||(int)$receipt['definition_version_id']!==(int)$item['definition_version_id']
            ||(string)$receipt['legacy_key']!==(string)$mapping['legacy_key']||(int)$receipt['attribute_value_id']<=0){$ok=false;break;}
        $extracted=hrm_typed_attribute_legacy_conversion_extract_scalar($historical_values['custom_data'],(string)$mapping['legacy_key']);
        $canonical=$extracted===false?false:hrm_typed_attribute_value_apply_validation($definition,$extracted['value']);
        if($canonical===false){$ok=false;break;}
        $vq=db_query('SELECT attribute_value_id,definition_version_id,subject_type,subject_id,value_version_no,predecessor_id,key_id,value_ciphertext,ciphertext_sha256,source FROM '
            .TB_PREF.'hrm_attribute_values WHERE attribute_value_id='.(int)$receipt['attribute_value_id'].' LIMIT 2 FOR UPDATE',
            'could not lock converted Typed Attribute value for cleanup');
        $value=$vq&&db_num_rows($vq)===1?db_fetch_assoc($vq):false;
        if(!$value||(int)$value['definition_version_id']!==(int)$item['definition_version_id']||(string)$value['subject_type']!=='worker'
            ||(int)$value['subject_id']!==(int)$item['worker_id']||!hrm_typed_attribute_legacy_reconciliation_value_matches($value,$canonical)){$ok=false;break;}

        $new_custom=hrm_typed_attribute_legacy_cleanup_remove_key($retained['custom_data'],(string)$mapping['legacy_key']);
        if($new_custom===false){$ok=false;break;}
        $recovery_item=$item;$recovery_item['execution_digest']=(string)$receipt['execution_digest'];$locked_recovery[]=$recovery_item;
        $prepared[]=array('item'=>$item,'employee'=>$employee,'current'=>$current,'new_custom'=>$new_custom);
    }}
    if($ok){$set_digest=hrm_typed_attribute_legacy_bulk_batch_digest((int)$batch['mapping_version_id'],(string)$batch['candidate_as_of'],$locked_candidates);
        if($set_digest===false||!hash_equals($set_digest,(string)$batch['candidate_set_digest']))$ok=false;}
    if($ok){$locked_recovery_digest=hrm_typed_attribute_legacy_cleanup_recovery_digest($batch,$locked_recovery);
        $locked_reconciliation_digest=$locked_recovery_digest===false?false:hash('sha256','hrm-fnd-007-legacy-cleanup-request-v1|batch='.(int)$batch['batch_id']
            .'|mapping='.(int)$batch['mapping_version_id'].'|candidate_set='.(string)$batch['candidate_set_digest'].'|recovery='.$locked_recovery_digest
            .'|count='.(int)$batch['candidate_count']);
        if($locked_recovery_digest===false||$locked_reconciliation_digest===false
            ||!hash_equals((string)$request['recovery_set_digest'],$locked_recovery_digest)
            ||!hash_equals((string)$request['reconciliation_digest'],$locked_reconciliation_digest))$ok=false;}

    $deleted=0;
    if($ok){foreach($prepared as $row){if(!$ok)break;$item=$row['item'];$employee=$row['employee'];$current=$row['current'];$new_custom=$row['new_custom'];
        $after=$employee;$after['custom_data']=$new_custom;
        $post_profile_id=hrm_compatibility_profile_append($after,'command');
        if($post_profile_id===false||(int)$post_profile_id<=0||(int)$post_profile_id===(int)$current['profile_version_id']){$ok=false;break;}
        if(!hrm_employee_projection_materialize_person((int)$item['employee_number'])){$ok=false;break;}
        $verify_q=db_query('SELECT custom_data FROM '.TB_PREF.'employees WHERE employee_number='.(int)$item['employee_number'].' LIMIT 2 FOR UPDATE',
            'could not verify Employee projection after legacy cleanup');$verify=$verify_q&&db_num_rows($verify_q)===1?db_fetch_assoc($verify_q):false;
        if(!$verify||!hrm_typed_attribute_legacy_custom_data_parity($verify['custom_data'],$new_custom)){$ok=false;break;}
        $digest=hrm_typed_attribute_legacy_cleanup_receipt_digest($request,$item,(int)$current['profile_version_id'],(int)$post_profile_id);
        if($digest===false){$ok=false;break;}
        $ok=db_query('INSERT INTO '.TB_PREF.'hrm_attribute_legacy_cleanup_receipts '
            .'(cleanup_request_id,batch_item_id,employee_number,worker_id,mapping_version_id,conversion_id,pre_cleanup_profile_version_id,post_cleanup_profile_version_id,execution_digest,executed_by) VALUES ('
            .(int)$cleanup_request_id.','.(int)$item['batch_item_id'].','.(int)$item['employee_number'].','.(int)$item['worker_id'].','.(int)$batch['mapping_version_id'].','.(int)$item['conversion_id'].','
            .(int)$current['profile_version_id'].','.(int)$post_profile_id.','.db_escape($digest).','.(int)$actor.')',
            'could not append immutable legacy cleanup receipt')!==false;
        if($ok){$audit=append_domain_audit_event(array('company_id'=>function_exists('user_company')?(int)user_company():0,'legal_entity_id'=>0,
            'entity_type'=>'hrm_typed_attribute_legacy_cleanup','entity_key'=>array('cleanup_request_id'=>(int)$cleanup_request_id,'batch_item_id'=>(int)$item['batch_item_id'],
                'employee_number'=>(int)$item['employee_number'],'worker_id'=>(int)$item['worker_id']),
            'action'=>'hrm.typed_attribute.legacy_key_cleaned','reason_code'=>'hrm_fnd_007_typed_attribute_legacy_cleanup_executor',
            'before'=>array('legacy_key_retained'=>true),'after'=>array('legacy_key_retained'=>false),
            'restricted_diff'=>array('exact_mapping_version'=>(int)$batch['mapping_version_id'],'conversion_receipt_id'=>(int)$item['conversion_id'],
                'pre_cleanup_profile_version_id'=>(int)$current['profile_version_id'],'post_cleanup_profile_version_id'=>(int)$post_profile_id,
                'immutable_predecessor_preserved'=>true,'legacy_plaintext_logged'=>false,'unknown_legacy_keys_preserved'=>true,
                'typed_attribute_value_rewritten'=>false,'payroll_consumer_activated'=>false,'historical_payroll_accounting_rows_rewritten'=>0)));
            if($audit===false)$ok=false;}
        if($ok)++$deleted;
    }}
    if($ok&&$deleted!==(int)$request['key_count'])$ok=false;
    if($ok)$ok=db_query('UPDATE '.TB_PREF.'hrm_attribute_legacy_cleanup_requests SET request_status='."'completed'".',row_version=row_version+1 WHERE cleanup_request_id='.(int)$cleanup_request_id
        ." AND request_status='approved' AND row_version=".(int)$request['row_version'],
        'could not complete approved legacy cleanup request')!==false&&function_exists('db_num_affected_rows')&&db_num_affected_rows()===1;
    if($ok){$audit=append_domain_audit_event(array('company_id'=>function_exists('user_company')?(int)user_company():0,'legal_entity_id'=>0,
        'entity_type'=>'hrm_typed_attribute_legacy_cleanup_request','entity_key'=>array('cleanup_request_id'=>(int)$cleanup_request_id,'batch_id'=>(int)$request['batch_id']),
        'action'=>'hrm.typed_attribute.legacy_cleanup_completed','reason_code'=>'hrm_fnd_007_typed_attribute_legacy_cleanup_executor',
        'before'=>array('request_status'=>'approved'),'after'=>array('request_status'=>'completed','cleaned_key_count'=>$deleted),
        'restricted_diff'=>array('all_or_nothing_transaction'=>true,'executor_distinct_from_maker_and_checker'=>true,'reconciliation_revalidated'=>true,
            'recovery_set_digest_revalidated'=>true,'immutable_cleanup_receipts_written'=>$deleted,'worker_profile_successors_appended'=>$deleted,
            'legacy_employee_rows_rematerialized'=>$deleted,'legacy_plaintext_logged'=>false,'historical_worker_profile_rows_rewritten'=>0,
            'typed_attribute_values_rewritten'=>0,'payroll_consumers_added'=>0,'historical_payroll_accounting_rows_rewritten'=>0)));
        if($audit===false)$ok=false;}
    if($ok)commit_transaction();else cancel_transaction();
    return $ok?(int)$cleanup_request_id:false;
}
