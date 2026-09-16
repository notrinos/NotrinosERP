<?php
/** HRM-FND-007 explicit forward reauthorization for expired, unexecuted cleanup requests. */
if (!function_exists('hrm_typed_attribute_legacy_cleanup_request_lock'))
    include_once($path_to_root.'/hrm/includes/db/typed_attribute_legacy_cleanup_execute_db.inc');

function hrm_typed_attribute_legacy_cleanup_user_can_recover()
{
    return function_exists('user_check_access')
        && user_check_access('SA_HRM_VIEW_TYPED_ATTRIBUTE_LEGACY_BULK_CANDIDATES')
        && user_check_access('SA_HRM_VIEW_TYPED_ATTRIBUTE_LEGACY_CONVERSION_STATUS')
        && user_check_access('SA_HRM_VIEW_TYPED_ATTRIBUTE_LEGACY_MAPPING')
        && user_check_access('SA_HRM_VIEW_TYPED_ATTRIBUTE_LEGACY_BULK_RECONCILIATION')
        && user_check_access('SA_HRM_APPROVE_TYPED_ATTRIBUTE_LEGACY_CLEANUP')
        && user_check_access('SA_HRM_RECOVER_TYPED_ATTRIBUTE_LEGACY_CLEANUP');
}

/**
 * Reauthorize only an expired approved request with zero destructive effects.
 * Completed or partial cleanup is never repaired here; transaction rollback and
 * immutable predecessor profiles remain the recovery boundary for execution failure.
 */
function hrm_typed_attribute_legacy_cleanup_recover($cleanup_request_id)
{
    $cleanup_request_id=hrm_typed_attribute_legacy_cleanup_positive_id($cleanup_request_id);
    $actor=function_exists('hrm_person_worker_actor_id')?hrm_person_worker_actor_id():0;
    if($cleanup_request_id===false||$actor<=0||!hrm_typed_attribute_legacy_cleanup_user_can_recover())return false;
    begin_transaction();$ok=true;
    $request=hrm_typed_attribute_legacy_cleanup_request_lock($cleanup_request_id);if(!$request)$ok=false;
    if($ok&&((string)$request['request_status']!=='approved'||$request['checker_id']===null
        ||(int)$request['checker_id']===(int)$request['maker_id']||(int)$request['maker_id']===$actor||(int)$request['checker_id']===$actor
        ||$request['approved_at']===null||(int)$request['row_version']<2))$ok=false;
    if($ok){$q=db_query('SELECT cleanup_receipt_id FROM '.TB_PREF.'hrm_attribute_legacy_cleanup_receipts WHERE cleanup_request_id='.(int)$cleanup_request_id.' LIMIT 1 FOR UPDATE',
        'could not verify zero cleanup effects before forward recovery');if(!$q||db_num_rows($q)!==0)$ok=false;}
    $evidence=$ok?hrm_typed_attribute_legacy_cleanup_evidence((int)$request['batch_id']):false;
    if($evidence===false||!hash_equals((string)$request['reconciliation_digest'],(string)$evidence['reconciliation_digest'])
        ||!hash_equals((string)$request['recovery_set_digest'],(string)$evidence['recovery_set_digest'])
        ||(int)$request['key_count']!==(int)$evidence['key_count'])$ok=false;
    if($ok){$clock=db_query('SELECT UTC_TIMESTAMP() utc_now','could not read legacy cleanup recovery time');$now=$clock?db_fetch_assoc($clock):false;if(!$now)$ok=false;}
    if($ok&&(string)$request['expires_at']>(string)$now['utc_now']){commit_transaction();return (int)$cleanup_request_id;}
    if($ok)$ok=db_query('UPDATE '.TB_PREF.'hrm_attribute_legacy_cleanup_requests SET expires_at=DATE_ADD(UTC_TIMESTAMP(),INTERVAL 24 HOUR),row_version=row_version+1 '
        .'WHERE cleanup_request_id='.(int)$cleanup_request_id." AND request_status='approved' AND expires_at<=UTC_TIMESTAMP() AND row_version=".(int)$request['row_version'],
        'could not forward-reauthorize expired legacy cleanup request')!==false&&function_exists('db_num_affected_rows')&&db_num_affected_rows()===1;
    if($ok){$audit=append_domain_audit_event(array('company_id'=>function_exists('user_company')?(int)user_company():0,'legal_entity_id'=>0,
        'entity_type'=>'hrm_typed_attribute_legacy_cleanup_request','entity_key'=>array('cleanup_request_id'=>(int)$cleanup_request_id,'batch_id'=>(int)$request['batch_id']),
        'action'=>'hrm.typed_attribute.legacy_cleanup_forward_reauthorized','reason_code'=>'hrm_fnd_007_typed_attribute_legacy_cleanup_recovery',
        'before'=>array('request_status'=>'approved','authorization_expired'=>true),'after'=>array('request_status'=>'approved','authorization_window_hours'=>24),
        'restricted_diff'=>array('recovery_actor_distinct_from_maker_and_checker'=>true,'zero_cleanup_receipts_required'=>true,'reconciliation_revalidated'=>true,
            'recovery_set_digest_revalidated'=>true,'partial_cleanup_repair_allowed'=>false,'legacy_keys_deleted_by_recovery'=>0,
            'worker_profiles_written_by_recovery'=>0,'typed_attribute_values_written_by_recovery'=>0,'historical_payroll_accounting_rows_rewritten'=>0)));
        if($audit===false)$ok=false;}
    if($ok)commit_transaction();else cancel_transaction();
    return $ok?(int)$cleanup_request_id:false;
}
