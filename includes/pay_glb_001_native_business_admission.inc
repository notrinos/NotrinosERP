<?php
/** Protected native projection/intent admission; no payroll approval or GL authority. */
require_once dirname(__FILE__).'/pay_glb_001_native_execution_admission.inc';

/** Require real current role/session and a strict default-off business flag. */
function pay_glb_001_native_business_principal($action='read')
{
    global $db_connections;
    $profiles=array('read'=>'native_business','review'=>'native_approve','project'=>'native_project','intent'=>'native_intent',
        'submit'=>'native_submit','approve'=>'native_approve','reject'=>'native_approve','cancel'=>'native_submit',
        'gl_intent'=>'native_gl_prepare','gl_reconcile'=>'native_gl_review','gl_review'=>'native_gl_review',
        'gl_authorize'=>'native_gl_post','gl_post'=>'native_gl_post',
        'gl_recovery_prepare'=>'native_gl_reverse','gl_recovery_review'=>'native_gl_reverse','gl_recover'=>'native_gl_reverse');
    if (!isset($profiles[$action])) return false;
    $p=pay_glb_001_current_principal($profiles[$action]);
    if ($p===false || !isset($db_connections[$p['company_id']]['pay_glb_001_native_business']['enabled'])
        || $db_connections[$p['company_id']]['pay_glb_001_native_business']['enabled']!==true) return false;
    if (!empty($GLOBALS['transaction_level']) && !pay_glb_001_native_execution_lock_principal($p,$profiles[$action])) return false;
    return $p;
}

/** Admit only the exact server-derived request, actor, purpose, source and residency. */
function pay_glb_001_native_business_admission(array $request,&$error=null)
{
    $error=null;
    $p=isset($request['action'])?pay_glb_001_native_business_principal($request['action']):false;
    $store=$p===false?false:pay_glb_001_read_admission_store('business');
    if ($store===false || !isset($request['request_sha256'],$request['producer_id'],$request['scope_sha256'],
        $request['selection_sha256'],$request['native_execution_sha256'],$request['source_manifest_sha256'],$request['command_key_sha256'])
        || in_array((int)$request['producer_id'],array($store['reviewer_user_id'],$store['approver_user_id']),true)) {
        $error='native_business_protected_source_required';return false;
    }
    $bound=array('request_sha256','scope_sha256','selection_sha256','native_execution_sha256','source_manifest_sha256','command_key_sha256');
    $fields=array_merge(array('record_id','actor_user_id','purpose','object_access'),$bound,
        array('residency_decision','permitted_regions','review_evidence_sha256','approved_at_utc','expires_at_utc'));
    $match=null;$seen=array();$now=gmdate('Y-m-d\TH:i:s\Z');
    foreach ($store['records'] as $lease) {
        if (!pay_glb_001_exact_keys($lease,$fields) || !pay_glb_001_code($lease['record_id']) || isset($seen[$lease['record_id']])
            || !is_int($lease['actor_user_id']) || $lease['actor_user_id']<=0
            || !in_array($lease['purpose'],array('PAY_GLB_001_NATIVE_READ','PAY_GLB_001_NATIVE_REVIEW','PAY_GLB_001_NATIVE_PROJECT','PAY_GLB_001_NATIVE_INTENT',
                'PAY_GLB_001_NATIVE_SUBMIT','PAY_GLB_001_NATIVE_APPROVE','PAY_GLB_001_NATIVE_REJECT','PAY_GLB_001_NATIVE_CANCEL',
                'PAY_GLB_001_NATIVE_GL_INTENT','PAY_GLB_001_NATIVE_GL_RECONCILE','PAY_GLB_001_NATIVE_GL_REVIEW',
                'PAY_GLB_001_NATIVE_GL_AUTHORIZE','PAY_GLB_001_NATIVE_GL_POST','PAY_GLB_001_NATIVE_GL_RECOVERY_PREPARE',
                'PAY_GLB_001_NATIVE_GL_RECOVERY_REVIEW','PAY_GLB_001_NATIVE_GL_RECOVER'),true)
            || $lease['object_access']!==str_replace('PAY_GLB_001_NATIVE_','ALLOW_',$lease['purpose'])
            || $lease['residency_decision']!=='APPROVED' || !pay_glb_001_sha256($lease['review_evidence_sha256'])
            || !pay_glb_001_iso_utc($lease['approved_at_utc']) || !pay_glb_001_iso_utc($lease['expires_at_utc'])
            || $lease['approved_at_utc']<$store['approved_at_utc'] || $lease['expires_at_utc']>$store['expires_at_utc']
            || $lease['expires_at_utc']<=$lease['approved_at_utc'] || !is_array($lease['permitted_regions'])
            || count($lease['permitted_regions'])<1 || count($lease['permitted_regions'])>32
            || array_keys($lease['permitted_regions'])!==range(0,count($lease['permitted_regions'])-1)
            || count(array_unique($lease['permitted_regions']))!==count($lease['permitted_regions'])) {
            $error='native_business_lease_invalid';return false;
        }
        $seen[$lease['record_id']]=true;
        foreach ($bound as $field) if (!pay_glb_001_sha256($lease[$field])) { $error='native_business_lease_binding_invalid';return false; }
        foreach ($lease['permitted_regions'] as $region) if (!pay_glb_001_code($region)) { $error='native_business_region_invalid';return false; }
        if ($lease['actor_user_id']!==$p['actor_user_id'] || $lease['purpose']!=='PAY_GLB_001_NATIVE_'.strtoupper($request['action'])) continue;
        if ($lease['request_sha256']!==$request['request_sha256']) continue;
        foreach ($bound as $field) if ($lease[$field]!==$request[$field]) { $error='native_business_source_mismatch';return false; }
        if ($match!==null || isset($store['revoked_ids'][$lease['record_id']]) || $lease['approved_at_utc']>$now
            || $lease['expires_at_utc']<=$now || !in_array($store['execution_region'],$lease['permitted_regions'],true)) {
            $error='native_business_lease_duplicate_revoked_or_expired';return false;
        }
        $match=$lease;
    }
    if ($match===null) { $error='native_business_exact_object_lease_required';return false; }
    return array('revision'=>$store['revision'],'store_sha256'=>$store['store_sha256'],'record_id'=>$match['record_id'],
        'actor_user_id'=>$p['actor_user_id'],'purpose'=>$match['purpose'],'reviewer_user_id'=>$store['reviewer_user_id'],
        'approver_user_id'=>$store['approver_user_id'],'review_evidence_sha256'=>$match['review_evidence_sha256'],
        'approval_evidence_sha256'=>$store['approval_evidence_sha256'],'execution_region'=>$store['execution_region'],'expires_at_utc'=>$match['expires_at_utc']);
}
