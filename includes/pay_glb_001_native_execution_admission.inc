<?php
/**
 * S39 execution-purpose admission over deployment-pinned private custody.
 * Read leases never authorize calculation. No grant writer, default grant,
 * payroll business approval, financial authority or production enablement.
 */
require_once dirname(__FILE__).'/pay_glb_001_admission_store.inc';
require_once dirname(__FILE__).'/../hrm/includes/payroll/pay_glb_001_native_execution_contract.inc';

/** Require the real durable current session, fresh execution role and explicit flag. */
function pay_glb_001_native_execution_principal()
{
    global $db_connections;
    $principal=pay_glb_001_current_principal('native_execute');
    if ($principal===false) return false;
    $company=$principal['company_id'];
    if (!isset($db_connections[$company]['pay_glb_001_native_execution']['enabled'])
        || $db_connections[$company]['pay_glb_001_native_execution']['enabled']!==true) return false;
    if (!empty($GLOBALS['transaction_level']) && !pay_glb_001_native_execution_lock_principal($principal)) return false;
    return $principal;
}

/** Lock and inspect current auth rows; a repeatable-read snapshot cannot hide revocation. */
function pay_glb_001_native_execution_lock_principal(array $principal,$purpose='native_execute')
{
    global $security_areas;
    if (!defined('TB_PREF') || !isset($_SESSION['security_session_registry_id'],$_SESSION['security_session_secret'])
        || !function_exists('db_query') || !function_exists('db_num_rows') || !function_exists('db_fetch_assoc')) return false;
    $q=db_query('SELECT u.role_id,u.inactive user_inactive,r.areas,r.sections,r.inactive role_inactive,'
        .'s.role_id session_role,s.session_key_hash,s.user_agent_hash,s.revoked_at,s.idle_expires_at,s.absolute_expires_at '
        .'FROM '.TB_PREF.'users u INNER JOIN '.TB_PREF.'security_roles r ON r.id=u.role_id '
        .'INNER JOIN '.TB_PREF.'user_sessions s ON s.user_id=u.id WHERE u.id='.(int)$principal['actor_user_id']
        .' AND s.id='.(int)$_SESSION['security_session_registry_id'].' LIMIT 2 FOR UPDATE');
    if (!$q || db_num_rows($q)!==1) return false;
    $row=db_fetch_assoc($q);$user=$_SESSION['wa_current_user'];
    if ((int)$row['user_inactive']!==0 || (int)$row['role_inactive']!==0 || $row['revoked_at']!==null
        || (int)$row['role_id']!==(int)$user->access || (int)$row['session_role']!==(int)$user->access
        || !hash_equals((string)$row['session_key_hash'],session_registry_hash($_SESSION['security_session_secret']))
        || !hash_equals((string)$row['user_agent_hash'],session_registry_user_agent_hash())
        || strtotime($row['idle_expires_at'].' UTC')<=time() || strtotime($row['absolute_expires_at'].' UTC')<=time()) return false;
    $areas=explode(';',(string)$row['areas']);$sections=explode(';',(string)$row['sections']);
    $profiles=array('native_execute'=>array('SA_HRM_EXECUTE_PAY_CORE_CALC','SA_HRM_WRITE_PAY_CORE_RESULT'),
        'native_replace'=>array('SA_HRM_EXECUTE_PAY_CORE_CALC','SA_HRM_WRITE_PAY_CORE_RESULT','SA_HRM_SEAL_PAY_CORE_RESULT','SA_HRM_EXECUTE_PAY_CORE_RETRO'),
        'native_retro_review'=>array('SA_HRM_REVIEW_PAY_CORE','SA_HRM_REVIEW_PAY_CORE_RETRO'),
        'native_business'=>array('SA_HRM_REVIEW_PAY_CORE','SA_HRM_VERIFY_PAY_CORE_RESULT','SA_HRM_VIEW_PAY_CORE_PROJECTION'),
        'native_reader'=>array('SA_HRMREPORTS','SA_HRM_VIEW_PAY_REC','SA_HRM_REVIEW_PAY_CORE','SA_HRM_VERIFY_PAY_CORE_RESULT','SA_HRM_VIEW_PAY_CORE_PROJECTION'),
        'native_project'=>array('SA_HRM_REVIEW_PAY_CORE','SA_HRM_VERIFY_PAY_CORE_RESULT','SA_HRM_VIEW_PAY_CORE_PROJECTION','SA_HRM_EXECUTE_PAY_CORE_PROJECTION'),
        'native_intent'=>array('SA_HRM_REVIEW_PAY_CORE','SA_HRM_VERIFY_PAY_CORE_RESULT','SA_HRM_VIEW_PAY_CORE_PROJECTION','SA_HRM_MANAGE_PAY_CORE_PROJECTION'),
        'native_submit'=>array('SA_HRM_REVIEW_PAY_CORE','SA_HRM_VERIFY_PAY_CORE_RESULT','SA_HRM_VIEW_PAY_CORE_PROJECTION','SA_HRM_MANAGE_PAY_CORE_PROJECTION','SA_PAYROLLAPPROVE'),
        'native_approve'=>array('SA_HRM_REVIEW_PAY_CORE','SA_HRM_VERIFY_PAY_CORE_RESULT','SA_HRM_VIEW_PAY_CORE_PROJECTION','SA_PAYROLLAPPROVE'),
        'native_gl_prepare'=>array('SA_HRM_REVIEW_PAY_CORE','SA_HRM_VERIFY_PAY_CORE_RESULT','SA_HRM_VIEW_PAY_CORE_PROJECTION','SA_HRM_MANAGE_PAY_GL_INTENT'),
        'native_gl_review'=>array('SA_HRM_REVIEW_PAY_CORE','SA_HRM_VERIFY_PAY_CORE_RESULT','SA_HRM_VIEW_PAY_CORE_PROJECTION','SA_PAY_GL_REVIEW'),
        'native_gl_post'=>array('SA_HRM_REVIEW_PAY_CORE','SA_HRM_VERIFY_PAY_CORE_RESULT','SA_HRM_VIEW_PAY_CORE_PROJECTION','SA_PAY_GL_POST'),
        'native_gl_reverse'=>array('SA_HRM_REVIEW_PAY_CORE','SA_HRM_VERIFY_PAY_CORE_RESULT','SA_HRM_VIEW_PAY_CORE_PROJECTION','SA_PAY_GL_REVERSE'),
        'native_recover'=>array('SA_HRM_EXECUTE_PAY_CORE_CALC','SA_HRM_WRITE_PAY_CORE_RESULT','SA_HRM_RECOVER_PAY_CORE_RESULT'),
        'native_select'=>array('SA_HRM_REVIEW_PAY_CORE','SA_HRM_VERIFY_PAY_CORE_RESULT'));
    if (!isset($profiles[$purpose])) return false;
    foreach ($profiles[$purpose] as $token)
        if (!isset($security_areas[$token][0]) || !in_array((string)$security_areas[$token][0],$areas,true)
            || !in_array((string)($security_areas[$token][0] & ~0xff),$sections,true)) return false;
    return true;
}

/** Admit exact server-derived native scope, source and request identity. */
function pay_glb_001_native_execution_admission(array $scope,$request_sha,$population_sha,$source_manifest_sha,&$error=null)
{
    $error=null;
    $principal=pay_glb_001_native_execution_principal();
    if ($principal===false) { $error='native_execution_disabled_or_principal_denied';return false; }
    $fields=array('company_id','legal_entity_id','payroll_group_id','group_version_id',
        'payroll_period_id','calendar_period_id','entity_code','country_code','currency_code','currency_minor_units');
    if (array_keys($scope)!==$fields || $scope['company_id']!==(string)$principal['company_id']) {
        $error='native_execution_admission_scope_invalid';return false;
    }
    foreach (array_slice($fields,0,6) as $field) {
        if (!pay_glb_001_native_field_valid('execution',$field,$scope[$field])
            || ($field!=='company_id' && pay_glb_001_native_helper_id($scope[$field],$error)===false)) {
            $error='native_execution_admission_id_invalid';return false;
        }
    }
    if (!pay_glb_001_code($scope['entity_code'])
        || !is_string($scope['country_code']) || preg_match('/^[A-Z]{2}$/D',$scope['country_code'])!==1
        || !pay_glb_001_native_field_valid('execution','currency_code',$scope['currency_code'])
        || !is_int($scope['currency_minor_units']) || $scope['currency_minor_units']<0 || $scope['currency_minor_units']>6
        || !pay_glb_001_sha256($request_sha) || !pay_glb_001_sha256($population_sha)
        || !pay_glb_001_sha256($source_manifest_sha)) {
        $error='native_execution_admission_source_invalid';return false;
    }
    $store=pay_glb_001_read_admission_store('native');
    if ($store===false) { $error='native_execution_protected_admission_unavailable';return false; }
    $lease_fields=array('record_id','actor_user_id','purpose','object_access','legal_entity_id',
        'payroll_group_id','group_version_id','payroll_period_id','calendar_period_id',
        'entity_code','country_code','currency_code','currency_minor_units','request_sha256','population_sha256',
        'source_manifest_sha256','residency_decision','permitted_regions',
        'review_evidence_sha256','approved_at_utc','expires_at_utc');
    $now=gmdate('Y-m-d\\TH:i:s\\Z');$ids=array();$matched=null;
    foreach ($store['records'] as $lease) {
        if (!pay_glb_001_exact_keys($lease,$lease_fields) || !pay_glb_001_code($lease['record_id'])
            || isset($ids[$lease['record_id']]) || !is_int($lease['actor_user_id']) || $lease['actor_user_id']<=0
            || $lease['purpose']!=='PAY_GLB_001_NATIVE_EXECUTE' || $lease['object_access']!=='ALLOW_EXECUTE_NATIVE') {
            $error='native_execution_protected_lease_invalid';return false;
        }
        $ids[$lease['record_id']]=true;
        foreach (array('legal_entity_id','payroll_group_id','group_version_id','payroll_period_id','calendar_period_id') as $field) {
            if (!pay_glb_001_native_field_valid('execution',$field,$lease[$field])) {
                $error='native_execution_protected_lease_id_invalid';return false;
            }
        }
        if (!pay_glb_001_code($lease['entity_code']) || !is_string($lease['country_code'])
            || preg_match('/^[A-Z]{2}$/D',$lease['country_code'])!==1
            || !pay_glb_001_native_field_valid('execution','currency_code',$lease['currency_code'])
            || !is_int($lease['currency_minor_units']) || $lease['currency_minor_units']<0 || $lease['currency_minor_units']>6
            || !pay_glb_001_sha256($lease['request_sha256']) || !pay_glb_001_sha256($lease['population_sha256'])
            || !pay_glb_001_sha256($lease['source_manifest_sha256']) || !pay_glb_001_sha256($lease['review_evidence_sha256'])
            || $lease['residency_decision']!=='APPROVED' || !is_array($lease['permitted_regions'])
            || count($lease['permitted_regions'])<1 || count($lease['permitted_regions'])>32
            || array_keys($lease['permitted_regions'])!==range(0,count($lease['permitted_regions'])-1)
            || count(array_unique($lease['permitted_regions']))!==count($lease['permitted_regions'])
            || !pay_glb_001_iso_utc($lease['approved_at_utc']) || !pay_glb_001_iso_utc($lease['expires_at_utc'])
            || $lease['expires_at_utc']<=$lease['approved_at_utc']
            || $lease['approved_at_utc']<$store['approved_at_utc'] || $lease['expires_at_utc']>$store['expires_at_utc']) {
            $error='native_execution_protected_lease_source_invalid';return false;
        }
        foreach ($lease['permitted_regions'] as $region) if (!pay_glb_001_code($region)) {
            $error='native_execution_protected_region_invalid';return false;
        }
        if ($lease['actor_user_id']!==$principal['actor_user_id'] || $lease['request_sha256']!==$request_sha) continue;
        $same=true;
        foreach (array_slice($fields,1) as $field) if ($lease[$field]!==$scope[$field]) $same=false;
        if (!$same) continue;
        if ($matched!==null || isset($store['revoked_ids'][$lease['record_id']])
            || $lease['population_sha256']!==$population_sha || $lease['source_manifest_sha256']!==$source_manifest_sha
            || $lease['approved_at_utc']>$now || $lease['expires_at_utc']<=$now
            || !in_array($store['execution_region'],$lease['permitted_regions'],true)) {
            $error='native_execution_lease_conflict_revoked_or_expired';return false;
        }
        $matched=$lease;
    }
    if ($matched===null) { $error='native_execution_exact_source_lease_required';return false; }
    return array('company_id'=>$principal['company_id'],'actor_user_id'=>$principal['actor_user_id'],
        'purpose'=>'PAY_GLB_001_NATIVE_EXECUTE','record_id'=>$matched['record_id'],
        'store_sha256'=>$store['store_sha256'],'revision'=>$store['revision'],
        'review_evidence_sha256'=>$matched['review_evidence_sha256'],
        'approval_evidence_sha256'=>$store['approval_evidence_sha256'],
        'reviewer_user_id'=>$store['reviewer_user_id'],'approver_user_id'=>$store['approver_user_id'],
        'execution_region'=>$store['execution_region'],'expires_at_utc'=>$matched['expires_at_utc']);
}
