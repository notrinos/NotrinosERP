<?php
/** Native CORE-007 independent finalization admission; no default grant or financial authority. */
require_once dirname(__FILE__).'/pay_glb_001_native_execution_admission.inc';

/** Authenticate existing CORE-007 and CORE-004 reviewer rights and the separate default-off flag. */
function pay_glb_001_native_correction_review_principal()
{
    global $db_connections;
    $p=pay_glb_001_current_principal('native_retro_review');
    if ($p===false || !isset($db_connections[$p['company_id']]['pay_glb_001_native_corrections']['enabled'])
        || $db_connections[$p['company_id']]['pay_glb_001_native_corrections']['enabled']!==true) return false;
    if (!empty($GLOBALS['transaction_level']) && !pay_glb_001_native_execution_lock_principal($p,'native_retro_review')) return false;
    return $p;
}

/** Bind a current private checker lease to the exact immutable maker proposal and source. */
function pay_glb_001_native_correction_admission(array $request,$operation_sha,&$error=null)
{
    $error=null;$p=pay_glb_001_native_correction_review_principal();
    if ($p===false || !pay_glb_001_sha256($operation_sha)) { $error='native_correction_principal_or_proposal_denied';return false; }
    $store=pay_glb_001_read_admission_store('correction');
    if ($store===false || (in_array((int)$request['maker_id'],array($store['reviewer_user_id'],$store['approver_user_id']),true) || in_array((int)$request['executor_id'],array($store['reviewer_user_id'],$store['approver_user_id']),true))) {
        $error='native_correction_protected_source_unavailable';return false;
    }
    $bound=array('scope_sha256','retro_plan_id','approved_plan_sha256','approval_sha256','original_result_id',
        'original_result_sha256','successor_result_id','successor_result_sha256','source_manifest_sha256','maker_id','executor_id');
    $fields=array_merge(array('record_id','actor_user_id','purpose','object_access','operation_sha256'),$bound,
        array('residency_decision','permitted_regions','review_evidence_sha256','approved_at_utc','expires_at_utc'));
    $matched=null;$seen=array();$now=gmdate('Y-m-d\TH:i:s\Z');
    foreach ($store['records'] as $lease) {
        if (!pay_glb_001_exact_keys($lease,$fields) || !pay_glb_001_code($lease['record_id']) || isset($seen[$lease['record_id']])
            || !is_int($lease['actor_user_id']) || $lease['actor_user_id']<=0
            || $lease['purpose']!=='PAY_GLB_001_NATIVE_RETRO_FINALIZE' || $lease['object_access']!=='ALLOW_FINALIZE_NATIVE'
            || !pay_glb_001_sha256($lease['operation_sha256']) || !pay_glb_001_sha256($lease['review_evidence_sha256'])
            || $lease['residency_decision']!=='APPROVED' || !is_array($lease['permitted_regions'])
            || count($lease['permitted_regions'])<1 || count($lease['permitted_regions'])>32
            || array_keys($lease['permitted_regions'])!==range(0,count($lease['permitted_regions'])-1)
            || count(array_unique($lease['permitted_regions']))!==count($lease['permitted_regions'])
            || !pay_glb_001_iso_utc($lease['approved_at_utc']) || !pay_glb_001_iso_utc($lease['expires_at_utc'])
            || $lease['approved_at_utc']<$store['approved_at_utc'] || $lease['expires_at_utc']>$store['expires_at_utc']
            || $lease['expires_at_utc']<=$lease['approved_at_utc']) { $error='native_correction_lease_invalid';return false; }
        $seen[$lease['record_id']]=true;
        foreach ($bound as $field) {
            if (!pay_glb_001_native_field_valid('command',$field,$lease[$field])) { $error='native_correction_lease_binding_invalid';return false; }
        }
        foreach ($lease['permitted_regions'] as $region) if (!pay_glb_001_code($region)) { $error='native_correction_lease_region_invalid';return false; }
        if ($lease['actor_user_id']!==$p['actor_user_id'] || $lease['operation_sha256']!==$operation_sha) continue;
        foreach ($bound as $field) if ($lease[$field]!==$request[$field]) { $error='native_correction_lease_request_mismatch';return false; }
        if ($matched!==null || in_array($lease['actor_user_id'],array((int)$request['maker_id'],(int)$request['executor_id']),true) || isset($store['revoked_ids'][$lease['record_id']])
            || $lease['approved_at_utc']>$now || $lease['expires_at_utc']<=$now
            || !in_array($store['execution_region'],$lease['permitted_regions'],true)) { $error='native_correction_lease_conflict_revoked_or_expired';return false; }
        $matched=$lease;
    }
    if ($matched===null) { $error='native_correction_exact_checker_lease_required';return false; }
    return array('revision'=>$store['revision'],'store_sha256'=>$store['store_sha256'],'record_id'=>$matched['record_id'],
        'actor_user_id'=>$p['actor_user_id'],'purpose'=>'PAY_GLB_001_NATIVE_RETRO_FINALIZE','reviewer_user_id'=>$store['reviewer_user_id'],
        'approver_user_id'=>$store['approver_user_id'],'review_evidence_sha256'=>$matched['review_evidence_sha256'],
        'approval_evidence_sha256'=>$store['approval_evidence_sha256'],'execution_region'=>$store['execution_region'],
        'expires_at_utc'=>$matched['expires_at_utc']);
}
