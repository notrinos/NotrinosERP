<?php
/** PAY-RULE-006 pure migration/shadow/cutover contract. */
include_once(dirname(__FILE__).'/../payroll_formula_differential.inc');

if (!defined('HRM_PAY_RULE_006_CONTRACT_VERSION')) define('HRM_PAY_RULE_006_CONTRACT_VERSION', '1.0.523');
if (!defined('HRM_PAY_RULE_006_COMPARISON_POLICY_VERSION')) define('HRM_PAY_RULE_006_COMPARISON_POLICY_VERSION', 'exact-v1');
if (!defined('HRM_PAY_RULE_006_PRODUCTION_SCOPE')) define('HRM_PAY_RULE_006_PRODUCTION_SCOPE', 'company-department-period');

function hrm_pay_rule_006_canonical($value)
{
    if (is_array($value)) {
        if (array_keys($value) !== range(0, count($value)-1)) ksort($value, SORT_STRING);
        foreach ($value as $k=>$v) $value[$k]=hrm_pay_rule_006_canonical($v);
    }
    return $value;
}
function hrm_pay_rule_006_hash($value)
{
    return hash('sha256', json_encode(hrm_pay_rule_006_canonical($value), JSON_UNESCAPED_SLASHES|JSON_UNESCAPED_UNICODE));
}
function hrm_pay_rule_006_sha($v){return is_string($v)&&preg_match('/^[a-f0-9]{64}$/D',$v)?strtolower($v):false;}
function hrm_pay_rule_006_token($v,$max=100){return is_string($v)&&$v!==''&&strlen($v)<=$max&&preg_match('/^[A-Za-z0-9][A-Za-z0-9_.:@-]*$/D',$v)?$v:false;}
function hrm_pay_rule_006_iso_date($v){return function_exists('hrm_legacy_formula_is_iso_date')&&hrm_legacy_formula_is_iso_date($v)?$v:false;}

/** Explicit migration classes; no unsupported behavior is coerced into a target. */
function hrm_pay_rule_006_classifications()
{
    return array('exact'=>true,'translated'=>true,'manual_review'=>true,'unsupported'=>true,'prohibited'=>true,'already_governed'=>true,'duplicate_obsolete'=>true);
}
function hrm_pay_rule_006_normalize_classification($value)
{
    $value=(string)$value; return isset(hrm_pay_rule_006_classifications()[$value])?$value:false;
}
function hrm_pay_rule_006_cutover_eligible_classification($value)
{
    return in_array($value,array('exact','translated','already_governed'),true);
}

/** Build an immutable candidate envelope without returning formula source text. */
function hrm_pay_rule_006_candidate_envelope($source,$identity,$formula_hash,$classification,$reason,$translated_hash=null,$result_set_hash=null,$row_count=1)
{
    if(!isset(hrm_legacy_formula_source_catalog()[$source]))return false;
    $identity=hrm_pay_rule_006_token($identity,160);$formula_hash=hrm_pay_rule_006_sha($formula_hash);$classification=hrm_pay_rule_006_normalize_classification($classification);$reason=hrm_pay_rule_006_token($reason,120);
    if($identity===false||$formula_hash===false||$classification===false||$reason===false||(int)$row_count<1)return false;
    if($translated_hash!==null&&hrm_pay_rule_006_sha($translated_hash)===false)return false;
    if($result_set_hash!==null&&hrm_pay_rule_006_sha($result_set_hash)===false)return false;
    $data=array('source_kind'=>$source,'source_identity'=>$identity,'source_payload_sha256'=>$formula_hash,'classification_token'=>$classification,'classification_reason'=>$reason,'translated_sha256'=>$translated_hash,'result_set_sha256'=>$result_set_hash,'row_count'=>(int)$row_count);
    $data['candidate_sha256']=hrm_pay_rule_006_hash($data);return $data;
}

/** Exact comparison is the default. Tolerance must be an explicit, reviewed policy. */
function hrm_pay_rule_006_compare_decimal($legacy,$governed,$policy=null,&$detail=null)
{
    $detail=null;if(!is_numeric($legacy)||!is_numeric($governed))return false;
    $l=(float)$legacy;$g=(float)$governed;if(is_nan($l)||is_nan($g)||is_infinite($l)||is_infinite($g))return false;
    $exact=sprintf('%.17g',$l)===sprintf('%.17g',$g);
    if($exact){$detail=array('outcome'=>'exact','delta'=>'0','policy'=>HRM_PAY_RULE_006_COMPARISON_POLICY_VERSION);return true;}
    if($policy===null){$detail=array('outcome'=>'mismatch','delta'=>sprintf('%.17g',abs($l-$g)),'policy'=>HRM_PAY_RULE_006_COMPARISON_POLICY_VERSION);return false;}
    if(!is_array($policy)||empty($policy['reviewed'])||empty($policy['signed'])||!isset($policy['absolute_tolerance'])||!is_numeric($policy['absolute_tolerance']))return false;
    $tol=(float)$policy['absolute_tolerance'];if($tol<0||is_nan($tol)||is_infinite($tol))return false;
    $ok=abs($l-$g)<=$tol;
    $detail=array('outcome'=>$ok?'tolerated':'mismatch','delta'=>sprintf('%.17g',abs($l-$g)),'policy'=>isset($policy['policy_version'])?(string)$policy['policy_version']:'explicit-v1');return $ok;
}

/** Four-person cutover custody: migration maker, migration reviewer, activation approver, activation executor. */
function hrm_pay_rule_006_distinct_actors($maker,$reviewer,$approver,$executor)
{
    $ids=array((int)$maker,(int)$reviewer,(int)$approver,(int)$executor);if(min($ids)<=0)return false;return count(array_unique($ids,SORT_REGULAR))===4;
}
function hrm_pay_rule_006_validate_activation_envelope(array $v,&$error=null)
{
    $error=null;$required=array('company_id','department_id','effective_from','source_state_sha256','migration_set_sha256','shadow_evidence_sha256','jurisdiction_evidence_sha256','candidate_count','published_binding_count','compared_candidate_count','comparison_count','unresolved_count','unsupported_count','rollback_ready','maker_id','reviewer_id','approver_id','executor_id');
    foreach($required as $k)if(!array_key_exists($k,$v)){$error='missing_'.$k;return false;}
    if((int)$v['company_id']<0||(int)$v['department_id']<=0){$error='broad_scope_forbidden';return false;}
    if(hrm_pay_rule_006_iso_date($v['effective_from'])===false){$error='effective_date_invalid';return false;}
    foreach(array('source_state_sha256','migration_set_sha256','shadow_evidence_sha256','jurisdiction_evidence_sha256') as $k)if(hrm_pay_rule_006_sha($v[$k])===false){$error=$k.'_invalid';return false;}
    foreach(array('candidate_count','published_binding_count','compared_candidate_count','comparison_count','unresolved_count','unsupported_count') as $k)if((int)$v[$k]<0){$error=$k.'_invalid';return false;}
    if((int)$v['candidate_count']<1||(int)$v['published_binding_count']!==(int)$v['candidate_count']||(int)$v['compared_candidate_count']!==(int)$v['candidate_count']||(int)$v['comparison_count']<(int)$v['candidate_count']||(int)$v['unresolved_count']!==0||(int)$v['unsupported_count']!==0||$v['rollback_ready']!==true){$error='eligibility_prerequisites_unsatisfied';return false;}
    if(!hrm_pay_rule_006_distinct_actors($v['maker_id'],$v['reviewer_id'],$v['approver_id'],$v['executor_id'])){$error='segregation_of_duties_failed';return false;}
    return true;
}

/** Runtime state is derived from the immutable command chain; no mutable feature flag. */
function hrm_pay_rule_006_command_state(array $commands,&$error=null)
{
    $error=null;$expected=1;$last=null;
    foreach($commands as $row){if(!is_array($row)||(int)($row['sequence_no']??0)!==$expected){$error='command_sequence_gap';return false;}$type=(string)($row['command_type']??'');if(!in_array($type,array('activate','failback'),true)){$error='command_type_invalid';return false;}if($last!==null&&(int)($row['predecessor_command_id']??0)!==(int)$last['command_id']){$error='command_predecessor_mismatch';return false;}$last=$row;$expected++;}
    if($last===null)return 'legacy';return $last['command_type']==='activate'?'governed':'legacy';
}

function hrm_pay_rule_006_source_state_sha256()
{
    $parts=array('contract'=>HRM_PAY_RULE_006_CONTRACT_VERSION,'comparison'=>HRM_PAY_RULE_006_COMPARISON_POLICY_VERSION,'legacy_contract'=>defined('HRM_LEGACY_PAYROLL_CONTRACT_VERSION')?HRM_LEGACY_PAYROLL_CONTRACT_VERSION:'unknown');
    if(function_exists('hrm_migration_head_registry')){$r=hrm_migration_head_registry();$parts['db_version']=(string)($r['active_database_version']??'');}
    return hrm_pay_rule_006_hash($parts);
}
