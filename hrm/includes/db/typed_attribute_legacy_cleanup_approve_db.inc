<?php
/** HRM-FND-007 distinct-checker approval for one legacy-key cleanup request. No cleanup execution. */
if (!function_exists('hrm_typed_attribute_legacy_cleanup_create_draft'))
    include_once($path_to_root.'/hrm/includes/db/typed_attribute_legacy_cleanup_command_db.inc');

function hrm_typed_attribute_legacy_cleanup_user_can_approve()
{
    return function_exists('user_check_access')
        && user_check_access('SA_HRM_VIEW_TYPED_ATTRIBUTE_LEGACY_BULK_CANDIDATES')
        && user_check_access('SA_HRM_VIEW_TYPED_ATTRIBUTE_LEGACY_CONVERSION_STATUS')
        && user_check_access('SA_HRM_VIEW_TYPED_ATTRIBUTE_LEGACY_MAPPING')
        && user_check_access('SA_HRM_VIEW_TYPED_ATTRIBUTE_LEGACY_BULK_RECONCILIATION')
        && user_check_access('SA_HRM_APPROVE_TYPED_ATTRIBUTE_LEGACY_CLEANUP');
}

function hrm_typed_attribute_legacy_cleanup_approve($cleanup_request_id)
{
    $cleanup_request_id=hrm_typed_attribute_legacy_cleanup_positive_id($cleanup_request_id);
    $checker=function_exists('hrm_person_worker_actor_id')?hrm_person_worker_actor_id():0;
    if ($cleanup_request_id===false || $checker<=0 || !hrm_typed_attribute_legacy_cleanup_user_can_approve()) return false;

    begin_transaction();$ok=true;$already=false;
    $q=db_query('SELECT cleanup_request_id,batch_id,request_nonce_hash,reconciliation_digest,recovery_set_digest,key_count,request_status,'
        .'maker_id,checker_id,approved_at,expires_at,row_version FROM '.TB_PREF.'hrm_attribute_legacy_cleanup_requests WHERE cleanup_request_id='.(int)$cleanup_request_id
        .' LIMIT 2 FOR UPDATE','could not lock legacy cleanup authorization for approval');
    $row=$q&&db_num_rows($q)===1?db_fetch_assoc($q):false;
    if (!$row || (int)$row['maker_id']===$checker) $ok=false;
    if ($ok) {$t=db_query('SELECT UTC_TIMESTAMP() utc_now','could not read cleanup authorization approval time');$now=$t?db_fetch_assoc($t):false;
        if (!$now || (string)$row['expires_at'] <= (string)$now['utc_now']) $ok=false;}
    if ($ok) {
        if ((string)$row['request_status']==='approved') {
            if ($row['checker_id']===null || (int)$row['checker_id']!==$checker || $row['approved_at']===null || (int)$row['row_version']!==2) $ok=false;
            else $already=true;
        } elseif ((string)$row['request_status']!=='draft' || $row['checker_id']!==null || $row['approved_at']!==null || (int)$row['row_version']!==1) $ok=false;
    }
    $evidence=$ok?hrm_typed_attribute_legacy_cleanup_evidence((int)$row['batch_id']):false;
    if ($evidence===false || !hash_equals((string)$row['reconciliation_digest'],(string)$evidence['reconciliation_digest'])
        || !hash_equals((string)$row['recovery_set_digest'],(string)$evidence['recovery_set_digest']) || (int)$row['key_count']!==(int)$evidence['key_count']) $ok=false;
    if ($ok && !$already) {
        $ok=db_query('UPDATE '.TB_PREF.'hrm_attribute_legacy_cleanup_requests SET request_status='."'approved'".',checker_id='.(int)$checker
            .',approved_at=UTC_TIMESTAMP(),row_version=2 WHERE cleanup_request_id='.(int)$cleanup_request_id
            ." AND request_status='draft' AND checker_id IS NULL AND approved_at IS NULL AND row_version=1",
            'could not approve legacy cleanup authorization')!==false && function_exists('db_num_affected_rows') && db_num_affected_rows()===1;
        if ($ok) {$audit=append_domain_audit_event(array('company_id'=>function_exists('user_company')?(int)user_company():0,'legal_entity_id'=>0,
            'entity_type'=>'hrm_typed_attribute_legacy_cleanup_request','entity_key'=>array('cleanup_request_id'=>(int)$cleanup_request_id,'batch_id'=>(int)$row['batch_id']),
            'action'=>'hrm.typed_attribute.legacy_cleanup_approved','reason_code'=>'hrm_fnd_007_typed_attribute_legacy_cleanup_approval',
            'before'=>array('request_status'=>'draft','checker_id'=>null),'after'=>array('request_status'=>'approved','checker_id'=>(int)$checker),
            'restricted_diff'=>array('checker_distinct_from_maker'=>true,'reconciliation_revalidated'=>true,'recovery_set_digest_revalidated'=>true,
                'legacy_key_delete_executed'=>false,'legacy_custom_data_rewritten'=>false,'cleanup_executor_activated'=>false,'plaintext_logged'=>false,
                'payroll_consumer_activated'=>false,'work_authorization_delivery_activated'=>false)));
            if ($audit===false)$ok=false;}
    }
    if ($ok) commit_transaction(); else cancel_transaction();
    return $ok?(int)$cleanup_request_id:false;
}
