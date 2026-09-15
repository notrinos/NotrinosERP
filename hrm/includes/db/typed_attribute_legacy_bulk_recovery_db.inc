<?php
/** HRM-FND-007 forward reauthorization for an expired, unexecuted approved bulk batch. */
if (!function_exists('hrm_typed_attribute_legacy_bulk_batch_lock'))
    include_once($path_to_root.'/hrm/includes/db/typed_attribute_legacy_bulk_execute_db.inc');

function hrm_typed_attribute_legacy_bulk_batch_user_can_recover()
{
    return function_exists('user_check_access')
        && user_check_access('SA_HRM_VIEW_TYPED_ATTRIBUTE_LEGACY_BULK_CANDIDATES')
        && user_check_access('SA_HRM_APPROVE_TYPED_ATTRIBUTE_LEGACY_BATCH')
        && user_check_access('SA_HRM_RECOVER_TYPED_ATTRIBUTE_LEGACY_BATCH');
}

/**
 * Reauthorize only an expired approved batch that still has zero execution effects.
 * A current still-valid approval is an idempotent no-op. Completed or partial state
 * is never repaired by this command.
 */
function hrm_typed_attribute_legacy_bulk_batch_recover($batch_id)
{
    $batch_id=hrm_typed_attribute_legacy_mapping_positive_id($batch_id,false);
    $actor=function_exists('hrm_person_worker_actor_id')?hrm_person_worker_actor_id():0;
    if($batch_id===false||$actor<=0||!hrm_typed_attribute_legacy_bulk_batch_user_can_recover())return false;
    begin_transaction();$ok=true;$mutated=false;
    $batch=hrm_typed_attribute_legacy_bulk_batch_lock($batch_id);if(!$batch)$ok=false;
    if($ok&&((string)$batch['batch_status']!=='approved'||$batch['checker_id']===null||(int)$batch['checker_id']===(int)$batch['maker_id']
        ||$batch['approved_at']===null||(int)$batch['row_version']<2||(int)$batch['maker_id']===$actor))$ok=false;
    $items=$ok?hrm_typed_attribute_legacy_bulk_batch_lock_items($batch_id):false;if($items===false)$ok=false;
    $command=$ok?hrm_typed_attribute_legacy_bulk_batch_command_from_locked_rows($batch,$items):false;if($command===false)$ok=false;
    if($ok){$clock=db_query('SELECT UTC_TIMESTAMP() utc_now','could not read legacy bulk batch recovery time');$now=$clock?db_fetch_assoc($clock):false;if(!$now)$ok=false;}
    if($ok&&(string)$batch['expires_at']>(string)$now['utc_now']){
        commit_transaction();return (int)$batch_id;
    }
    $custody=$ok?hrm_typed_attribute_legacy_bulk_batch_revalidate_candidates($command):false;if($custody===false)$ok=false;
    if($ok)$ok=db_query('UPDATE '.TB_PREF.'hrm_attribute_legacy_conversion_batches SET expires_at=DATE_ADD(UTC_TIMESTAMP(),INTERVAL 24 HOUR),row_version=row_version+1 '
        .'WHERE batch_id='.(int)$batch_id." AND batch_status='approved' AND expires_at<=UTC_TIMESTAMP() AND row_version=".(int)$batch['row_version'],
        'could not forward-reauthorize expired legacy Typed Attribute bulk batch')!==false
        &&function_exists('db_num_affected_rows')&&db_num_affected_rows()===1;
    if($ok){$mutated=true;$audit=append_domain_audit_event(array('company_id'=>function_exists('user_company')?(int)user_company():0,'legal_entity_id'=>0,
        'entity_type'=>'hrm_typed_attribute_legacy_conversion_batch','entity_key'=>array('batch_id'=>(int)$batch_id,'mapping_version_id'=>(int)$batch['mapping_version_id']),
        'action'=>'hrm.typed_attribute.legacy_bulk_batch_forward_reauthorized','reason_code'=>'hrm_fnd_007_typed_attribute_legacy_bulk_recovery',
        'before'=>array('batch_status'=>'approved','authorization_expired'=>true),'after'=>array('batch_status'=>'approved','authorization_window_hours'=>24),
        'restricted_diff'=>array('recovery_actor_distinct_from_maker'=>true,'candidate_set_digest_verified'=>true,'transactional_revalidation'=>true,
            'partial_execution_repair_allowed'=>false,'typed_attribute_values_written'=>0,'conversion_receipts_written'=>0,'plaintext_logged'=>false,
            'legacy_key_retained'=>true,'legacy_custom_data_rewritten'=>false,'payroll_consumer_activated'=>false)));
        if($audit===false)$ok=false;}
    if($ok)commit_transaction();else cancel_transaction();
    return $ok?(int)$batch_id:false;
}
