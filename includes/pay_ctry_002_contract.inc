<?php
/**
 * PAY-CTRY-002 lifecycle/activation contract.
 *
 * PAY-CTRY-001 owns site trust and immutable verified pack artifacts.
 * PAY-CTRY-002 keeps installation distinct from governed company/legal-entity/
 * payroll-scope activation. Staging and approval evidence remain non-authoritative
 * until a later activation executor commits a reviewed activation record.
 */
if (!defined('PAY_CTRY_002_CONTRACT_VERSION')) define('PAY_CTRY_002_CONTRACT_VERSION', '1.0.1');
if (!defined('PAY_CTRY_002_BATCH_DB_VERSION')) define('PAY_CTRY_002_BATCH_DB_VERSION', '1.0.782');
if (!defined('PAY_CTRY_002_SOURCE_CUSTODY_VERSION')) define('PAY_CTRY_002_SOURCE_CUSTODY_VERSION', '1.0.456');

if (!defined('PAY_CTRY_002_MAKER_PERMISSION')) define('PAY_CTRY_002_MAKER_PERMISSION', 'SA_HRM_MANAGE_PAY_CTRY_ACTIVATION');
if (!defined('PAY_CTRY_002_CHECKER_PERMISSION')) define('PAY_CTRY_002_CHECKER_PERMISSION', 'SA_HRM_REVIEW_PAY_CTRY_ACTIVATION');
if (!defined('PAY_CTRY_002_EXECUTOR_PERMISSION')) define('PAY_CTRY_002_EXECUTOR_PERMISSION', 'SA_HRM_EXECUTE_PAY_CTRY_ACTIVATION');
if (!defined('PAY_CTRY_002_RECOVERY_PERMISSION')) define('PAY_CTRY_002_RECOVERY_PERMISSION', 'SA_HRM_RECOVER_PAY_CTRY_ACTIVATION');

function pay_ctry_002_site_operation_types()
{
    return array('install_artifact','upgrade_artifact','rollback_artifact','restore_reverify','trust_recheck','hard_revocation_propagation');
}
function pay_ctry_002_site_operation_events()
{
    return array('requested','verified','applied','failed','recovered');
}
function pay_ctry_002_activation_actions()
{
    return array('activate','upgrade','rollback','deactivate','suspend','resume','hard_revoke');
}
function pay_ctry_002_scope_types()
{
    return array('company','legal_entity','payroll_scope');
}
/** Return the only decisions allowed in an activation approval record. */
function pay_ctry_002_approval_decisions()
{
    return array('approved','rejected');
}
function pay_ctry_002_contract_invariants()
{
    return array(
        'installation_implies_activation'=>false,
        'activation_inferred_from_installation'=>false,
        'site_trust_admin_may_activate_company'=>false,
        'installer_may_self_approve_activation'=>false,
        'maker_may_execute_own_activation'=>false,
        'checker_may_execute_approved_activation'=>false,
        'historical_activation_evidence_mutable'=>false,
        'hard_revocation_allows_new_execution'=>false,
        'historical_payroll_rewrite_allowed'=>false,
        'automatic_pack_activation_allowed'=>false,
        'activation_requires_exact_pack_root'=>true,
        'activation_requires_impact_evidence'=>true,
        'activation_requires_independent_approval'=>true,
        'impact_summary_is_server_derived'=>true,
        'staging_changes_runtime_resolution'=>false,
        'approval_changes_runtime_resolution'=>false,
        'emergency_suspension_preserves_evidence'=>true
    );
}

/** Canonicalize associative arrays for deterministic JSON hashing. */
function pay_ctry_002_canonical($value)
{
    if (is_array($value)) {
        $keys=array_keys($value);$list=$keys===range(0,count($value)-1);
        if (!$list) ksort($value,SORT_STRING);
        foreach ($value as $k=>$v) $value[$k]=pay_ctry_002_canonical($v);
    }
    return $value;
}

/** Return the SHA-256 digest for a canonical descriptor value. */
function pay_ctry_002_hash($value)
{
    return hash('sha256',json_encode(pay_ctry_002_canonical($value),JSON_UNESCAPED_SLASHES|JSON_UNESCAPED_UNICODE));
}

/** Validate a lower-case hexadecimal SHA-256 digest. */
function pay_ctry_002_sha256($value)
{
    return is_string($value)&&preg_match('/^[a-f0-9]{64}$/D',$value)===1?strtolower($value):false;
}

/** Validate a bounded canonical identifier token. */
function pay_ctry_002_token($value,$max=96)
{
    return is_string($value)&&$value!==''&&strlen($value)<=$max&&preg_match('/^[A-Za-z0-9][A-Za-z0-9_.:@-]*$/D',$value)===1?$value:false;
}

/** Validate an ISO-8601 calendar date without normalizing caller input. */
function pay_ctry_002_iso_date($value)
{
    if (!is_string($value)||preg_match('/^\d{4}-\d{2}-\d{2}$/D',$value)!==1) return false;
    $p=explode('-',$value);return count($p)===3&&checkdate((int)$p[1],(int)$p[2],(int)$p[0])?$value:false;
}

/** Validate a canonical unsigned integer and return its integer value. */
function pay_ctry_002_uint($value,$allow_zero=true)
{
    if (is_int($value)) $n=$value;
    elseif (is_string($value)&&preg_match('/^(?:0|[1-9][0-9]*)$/D',$value)===1&&strlen($value)<=18) $n=(int)$value;
    else return false;
    return $n<0||(!$allow_zero&&$n===0)?false:$n;
}

/** Validate the closed company, legal-entity, or payroll-scope target shape. */
function pay_ctry_002_validate_scope(array $scope,&$error=null)
{
    $error=null;
    foreach (array('company_id','scope_type') as $k) if (!array_key_exists($k,$scope)) {$error='missing_'.$k;return false;}
    $company=pay_ctry_002_uint($scope['company_id'],true);$type=(string)$scope['scope_type'];
    if ($company===false||!in_array($type,pay_ctry_002_scope_types(),true)) {$error='scope_invalid';return false;}
    if (!isset($scope['legal_entity_id'])||$scope['legal_entity_id']==='') $legal=0; else {$legal=pay_ctry_002_uint($scope['legal_entity_id'],false);if($legal===false){$error='legal_entity_scope_invalid';return false;}}
    $payroll=isset($scope['payroll_scope_key'])?(string)$scope['payroll_scope_key']:'';
    if ($type==='company'&&($legal!==0||$payroll!=='')) {$error='company_scope_overqualified';return false;}
    if ($type==='legal_entity'&&($legal<=0||$payroll!=='')) {$error='legal_entity_scope_invalid';return false;}
    if ($type==='payroll_scope'&&($legal<0||pay_ctry_002_token($payroll,96)===false)) {$error='payroll_scope_invalid';return false;}
    return true;
}

/** Validate an inclusive effective-date interval. */
function pay_ctry_002_validate_interval($from,$to,&$error=null)
{
    $error=null;$f=pay_ctry_002_iso_date($from);$t=$to===null||$to===''?null:pay_ctry_002_iso_date($to);
    if ($f===false||($to!==null&&$to!==''&&$t===false)) {$error='effective_interval_invalid';return false;}
    if ($t!==null&&strcmp($t,$f)<0) {$error='effective_interval_reversed';return false;}
    return true;
}
/** Validate the server-derived impact descriptor and all custody digests. */
function pay_ctry_002_validate_impact_descriptor(array $v,&$error=null)
{
    $error=null;
    foreach (array('impact_key','site_installation_id','company_id','scope_type','action_type','effective_from','dependency_lock_sha256','binding_snapshot_sha256','open_run_count','open_approval_count','downstream_intent_count','blocking_finding_count','impact_payload_sha256','impact_sha256','simulated_by') as $k)
        if (!array_key_exists($k,$v)) {$error='missing_'.$k;return false;}
    if (pay_ctry_002_token($v['impact_key'],64)===false||pay_ctry_002_uint($v['site_installation_id'],false)===false||pay_ctry_002_uint($v['simulated_by'],false)===false) {$error='impact_identity_invalid';return false;}
    if (!pay_ctry_002_validate_scope($v,$error)||!in_array((string)$v['action_type'],pay_ctry_002_activation_actions(),true)||!pay_ctry_002_validate_interval($v['effective_from'],isset($v['effective_to'])?$v['effective_to']:null,$error)) {if($error===null)$error='impact_scope_or_action_invalid';return false;}
    foreach (array('dependency_lock_sha256','binding_snapshot_sha256','impact_payload_sha256','impact_sha256') as $k) if (pay_ctry_002_sha256($v[$k])===false) {$error=$k.'_invalid';return false;}
    foreach (array('open_run_count','open_approval_count','downstream_intent_count','blocking_finding_count') as $k) if (pay_ctry_002_uint($v[$k],true)===false) {$error=$k.'_invalid';return false;}
    return true;
}
/** Validate the activation proposal descriptor and its immutable digests. */
function pay_ctry_002_validate_proposal_descriptor(array $v,&$error=null)
{
    $error=null;
    foreach (array('proposal_key','site_installation_id','impact_simulation_id','company_id','scope_type','action_type','effective_from','dependency_lock_sha256','binding_snapshot_sha256','impact_sha256','proposal_sha256','maker_id') as $k)
        if (!array_key_exists($k,$v)) {$error='missing_'.$k;return false;}
    if (pay_ctry_002_token($v['proposal_key'],96)===false||pay_ctry_002_uint($v['site_installation_id'],false)===false||pay_ctry_002_uint($v['impact_simulation_id'],false)===false||pay_ctry_002_uint($v['maker_id'],false)===false) {$error='proposal_identity_invalid';return false;}
    if (!pay_ctry_002_validate_scope($v,$error)||!in_array((string)$v['action_type'],pay_ctry_002_activation_actions(),true)||!pay_ctry_002_validate_interval($v['effective_from'],isset($v['effective_to'])?$v['effective_to']:null,$error)) {if($error===null)$error='proposal_scope_or_action_invalid';return false;}
    foreach (array('dependency_lock_sha256','binding_snapshot_sha256','impact_sha256','proposal_sha256') as $k) if (pay_ctry_002_sha256($v[$k])===false) {$error=$k.'_invalid';return false;}
    return true;
}
/** Require the approval checker to be a distinct positive user identity. */
function pay_ctry_002_distinct_maker_checker($maker,$checker)
{
    $m=pay_ctry_002_uint($maker,false);$c=pay_ctry_002_uint($checker,false);return $m!==false&&$c!==false&&$m!==$c;
}
/** Validate an independent approval descriptor bound to its proposal digest. */
function pay_ctry_002_validate_approval_descriptor(array $v,$maker_id,&$error=null)
{
    $error=null;
    foreach (array('activation_proposal_id','proposal_sha256','decision_token','evidence_sha256','approval_sha256','checker_id') as $k)
        if (!array_key_exists($k,$v)) {$error='missing_'.$k;return false;}
    if (pay_ctry_002_uint($v['activation_proposal_id'],false)===false||pay_ctry_002_sha256($v['proposal_sha256'])===false||pay_ctry_002_sha256($v['evidence_sha256'])===false||pay_ctry_002_sha256($v['approval_sha256'])===false||!in_array((string)$v['decision_token'],pay_ctry_002_approval_decisions(),true)) {$error='approval_descriptor_invalid';return false;}
    if (!pay_ctry_002_distinct_maker_checker($maker_id,$v['checker_id'])) {$error='distinct_checker_required';return false;}
    return true;
}
?>
