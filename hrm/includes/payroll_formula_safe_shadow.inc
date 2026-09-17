<?php
/**********************************************************************
    PAY-RULE-002 non-production shadow governance.
    Production payroll routing is intentionally absent.
***********************************************************************/
include_once(dirname(__FILE__).'/payroll_formula_safe_profile.inc');
include_once(dirname(__FILE__).'/payroll_formula_differential.inc');

if (!defined('HRM_PAYROLL_FORMULA_SAFE_PUBLICATION_ID'))
    define('HRM_PAYROLL_FORMULA_SAFE_PUBLICATION_ID', 'notrinoserp.hrm.payroll.formula_safe_publication');
if (!defined('HRM_PAYROLL_FORMULA_SAFE_PUBLICATION_VERSION'))
    define('HRM_PAYROLL_FORMULA_SAFE_PUBLICATION_VERSION', '1.0.0');

function hrm_payroll_formula_safe_hash_is_valid($hash)
{
    return is_string($hash) && preg_match('/^[a-f0-9]{64}$/D',$hash)===1;
}

function hrm_payroll_formula_safe_token_is_valid($token,$max=120)
{
    return is_string($token) && strlen($token)>0 && strlen($token)<=$max
        && preg_match('/^[A-Za-z0-9_.:@-]+$/D',$token)===1;
}

function hrm_payroll_formula_safe_iso_date($date)
{
    if(!is_string($date)||!preg_match('/^[0-9]{4}-[0-9]{2}-[0-9]{2}$/D',$date))return false;
    $p=explode('-',$date);return checkdate((int)$p[1],(int)$p[2],(int)$p[0]);
}

/** Validate default-deny redacted shadow publication registry. */
function hrm_payroll_formula_safe_validate_publication_registry(array $registry,array $bindings,&$error=null)
{
    $error=null;$top=array('schema_version','contract','governance','safe_profile_sha256','pay_rule_001_target_profile_sha256','differential_corpus_sha256','exception_register_sha256','entries');
    if(!hrm_payroll_formula_safe_has_only_keys($registry,$top)){$error='safe_publication_unknown_field';return false;}
    foreach($top as $k)if(!array_key_exists($k,$registry)){$error='safe_publication_missing_field';return false;}
    if($registry['schema_version']!==1||$registry['contract']!==array('id'=>HRM_PAYROLL_FORMULA_SAFE_PUBLICATION_ID,'version'=>HRM_PAYROLL_FORMULA_SAFE_PUBLICATION_VERSION)){$error='safe_publication_contract_invalid';return false;}
    $expectedGov=array('environment'=>'non-production-shadow-only','default_deny'=>true,'required_status'=>'approved_for_shadow','author_reviewer_publisher_distinct'=>true,'shadow_executor_distinct'=>true,'production_execution_forbidden'=>true,'formula_text_forbidden'=>true,'payroll_context_forbidden'=>true);
    if($registry['governance']!==$expectedGov){$error='safe_publication_governance_invalid';return false;}
    $map=array('safe_profile_sha256'=>'safe_profile_sha256','pay_rule_001_target_profile_sha256'=>'pay_rule_001_target_profile_sha256','differential_corpus_sha256'=>'differential_corpus_sha256','exception_register_sha256'=>'exception_register_sha256');
    foreach($map as $field=>$binding){if(!isset($bindings[$binding])||!hrm_payroll_formula_safe_hash_is_valid($bindings[$binding])||!hash_equals($bindings[$binding],(string)$registry[$field])){$error='safe_publication_binding_mismatch';return false;}}
    if(!is_array($registry['entries'])){$error='safe_publication_entries_invalid';return false;}
    $index=array();$sources=hrm_legacy_formula_source_catalog();
    foreach($registry['entries'] as $entry){
        $allowed=array('source_formula_sha256','target_formula_sha256','source_scope','classification','differential_reason','result_set_sha256','status','author_id','reviewer_id','publisher_id','shadow_executor_id','approved_on','review_due_on','evidence_id');
        if(!is_array($entry)||!hrm_payroll_formula_safe_has_only_keys($entry,$allowed)){$error='safe_publication_entry_schema_invalid';return false;}
        foreach($allowed as $k)if(!array_key_exists($k,$entry)){$error='safe_publication_entry_schema_invalid';return false;}
        if(!hrm_payroll_formula_safe_hash_is_valid($entry['source_formula_sha256'])||!hrm_payroll_formula_safe_hash_is_valid($entry['target_formula_sha256'])||!hrm_payroll_formula_safe_hash_is_valid($entry['result_set_sha256'])||!isset($sources[$entry['source_scope']])){$error='safe_publication_entry_value_invalid';return false;}
        if(!in_array($entry['classification'],array('exact','translated'),true)||!in_array($entry['differential_reason'],array('equivalent_without_translation','decimal_literal_normalized'),true)||$entry['status']!=='approved_for_shadow'){$error='safe_publication_entry_value_invalid';return false;}
        if(($entry['classification']==='exact'&&$entry['differential_reason']!=='equivalent_without_translation')||($entry['classification']==='translated'&&$entry['differential_reason']!=='decimal_literal_normalized')){$error='safe_publication_classification_mismatch';return false;}
        $ids=array($entry['author_id'],$entry['reviewer_id'],$entry['publisher_id'],$entry['shadow_executor_id']);foreach($ids as $id)if(!hrm_payroll_formula_safe_token_is_valid($id,100)){$error='safe_publication_principal_invalid';return false;}if(count(array_unique($ids,SORT_STRING))!==4){$error='safe_publication_role_separation_invalid';return false;}
        if(!hrm_payroll_formula_safe_iso_date($entry['approved_on'])||!hrm_payroll_formula_safe_iso_date($entry['review_due_on'])||$entry['approved_on']>$entry['review_due_on']||!hrm_payroll_formula_safe_token_is_valid($entry['evidence_id'])){$error='safe_publication_evidence_invalid';return false;}
        $h=$entry['source_formula_sha256'];$s=$entry['source_scope'];if(isset($index[$h][$s])){$error='safe_publication_duplicate';return false;}if(!isset($index[$h]))$index[$h]=array();$index[$h][$s]=$entry;
    }
    return $index;
}

function hrm_payroll_formula_safe_publication_bindings()
{
    global $path_to_root;
    return array(
        'safe_profile_sha256'=>hash_file('sha256',$path_to_root.'/hrm/config/payroll_formula_safe_profile.json'),
        'pay_rule_001_target_profile_sha256'=>hash_file('sha256',$path_to_root.'/hrm/config/payroll_formula_target_profile.json'),
        'differential_corpus_sha256'=>hash_file('sha256',$path_to_root.'/hrm/config/payroll_formula_differential_corpus.json'),
        'exception_register_sha256'=>hash_file('sha256',$path_to_root.'/hrm/config/payroll_formula_exception_register.json')
    );
}

/** Convert synthetic PAY-RULE-001 numeric context into explicit typed strings. */
function hrm_payroll_formula_safe_from_legacy_synthetic_context(array $values)
{
    $out=array();$contract=hrm_payroll_formula_safe_variable_contract();
    foreach($contract as $name=>$spec){
        if($spec['type']==='date')continue;
        if(!array_key_exists($name,$values)||(!is_int($values[$name])&&!is_float($values[$name])))throw new RuntimeException('safe_differential_context_invalid');
        $out[$name]=number_format((float)$values[$name],6,'.','');
    }
    $out['PERIOD_START']='2000-01-01';$out['PERIOD_END']='2000-01-31';return $out;
}

/** Target callback for the accepted synthetic differential harness. */
function hrm_payroll_formula_safe_differential_target($formula,array $values)
{
    $typed=hrm_payroll_formula_safe_from_legacy_synthetic_context($values);$receipt=null;$error=null;$result=hrm_payroll_formula_safe_evaluate($formula,$typed,$receipt,$error);if($result===false)throw new RuntimeException($error===null?'safe_differential_target_failed':$error);if($result->unit()!=='currency'&&$result->unit()!=='scalar'&&$result->unit()!=='day'&&$result->unit()!=='hour')throw new RuntimeException('safe_differential_result_unit_unsupported');return (float)$result->decimalString();
}

/** Run PAY-RULE-001 legacy-vs-safe-profile differential on fixed synthetic contexts. */
function hrm_payroll_formula_safe_differential_classify($formula,array $contexts)
{
    if(!function_exists('hrm_legacy_formula_evaluate_for_differential')){
        global $path_to_root;include_once($path_to_root.'/hrm/includes/payroll_engine.inc');
    }
    return hrm_legacy_formula_differential_classify($formula,$contexts,'hrm_legacy_formula_evaluate_for_differential','hrm_payroll_formula_safe_differential_target');
}

function hrm_payroll_formula_safe_shadow_log($outcome,$reason,$source_scope,$formula_hash,$executor)
{
    error_log(json_encode(array('contract'=>HRM_PAYROLL_FORMULA_SAFE_PUBLICATION_ID,'version'=>HRM_PAYROLL_FORMULA_SAFE_PUBLICATION_VERSION,'action'=>'payroll_formula_safe_shadow','outcome'=>$outcome==='allowed'?'allowed':'denied','reason'=>hrm_payroll_formula_safe_token_is_valid($reason,100)?$reason:'invalid_reason','source'=>isset(hrm_legacy_formula_source_catalog()[$source_scope])?$source_scope:'unknown','formula_sha256'=>hrm_payroll_formula_safe_hash_is_valid($formula_hash)?$formula_hash:hash('sha256',''),'executor_fingerprint'=>hash('sha256','pay-rule-002-shadow:'.(string)$executor))));
}

/**
 * Execute a reviewed formula only in explicit CLI non-production shadow mode.
 * No production payroll caller references this function.
 */
function hrm_payroll_formula_safe_shadow_evaluate($source_formula,$source_scope,array $typed_context,$executor_id,array $registry,array $corpus,&$receipt=null,&$error=null,$as_of=null)
{
    $receipt=null;$error=null;$source_hash=hash('sha256',trim((string)$source_formula));
    $deny=function($reason)use(&$error,$source_scope,$source_hash,$executor_id){$error=$reason;hrm_payroll_formula_safe_shadow_log('denied',$reason,$source_scope,$source_hash,$executor_id);return false;};
    if(PHP_SAPI!=='cli'||!defined('HRM_PAYROLL_FORMULA_SAFE_NONPRODUCTION_SHADOW')||HRM_PAYROLL_FORMULA_SAFE_NONPRODUCTION_SHADOW!==true)return $deny('safe_shadow_gate_required');
    if(!isset(hrm_legacy_formula_source_catalog()[$source_scope])||!hrm_payroll_formula_safe_token_is_valid($executor_id,100))return $deny('safe_shadow_actor_or_scope_invalid');
    $bindings=hrm_payroll_formula_safe_publication_bindings();$index=hrm_payroll_formula_safe_validate_publication_registry($registry,$bindings,$error);if($index===false)return $deny($error);if(!isset($index[$source_hash][$source_scope]))return $deny('safe_shadow_publication_missing');$entry=$index[$source_hash][$source_scope];
    $as_of=$as_of===null?gmdate('Y-m-d'):$as_of;if(!hrm_payroll_formula_safe_iso_date($as_of)||$entry['approved_on']>$as_of||$entry['review_due_on']<$as_of)return $deny('safe_shadow_publication_inactive');if(!hash_equals($entry['shadow_executor_id'],$executor_id))return $deny('safe_shadow_executor_denied');
    $target_formula=$entry['classification']==='translated'?hrm_legacy_formula_translate_for_facade($source_formula):hrm_legacy_formula_normalize($source_formula);if($target_formula===false||!hash_equals($entry['target_formula_sha256'],hash('sha256',$target_formula)))return $deny('safe_shadow_target_hash_mismatch');
    $validated=hrm_legacy_formula_validate_corpus($corpus,$error);if($validated===false)return $deny($error);$diff=hrm_payroll_formula_safe_differential_classify($source_formula,$validated['migration_contexts']);if($diff['classification']!==$entry['classification']||$diff['reason']!==$entry['differential_reason']||!hash_equals($entry['result_set_sha256'],$diff['result_set_hash']))return $deny('safe_shadow_differential_mismatch');
    $evalReceipt=null;$value=hrm_payroll_formula_safe_evaluate($target_formula,$typed_context,$evalReceipt,$error);if($value===false)return $deny($error===null?'safe_shadow_execution_failed':$error);
    $receipt=array('contract'=>array('id'=>HRM_PAYROLL_FORMULA_SAFE_PUBLICATION_ID,'version'=>HRM_PAYROLL_FORMULA_SAFE_PUBLICATION_VERSION),'mode'=>'non-production-shadow','source_scope'=>$source_scope,'source_formula_sha256'=>$source_hash,'target_formula_sha256'=>$entry['target_formula_sha256'],'classification'=>$entry['classification'],'differential_reason'=>$entry['differential_reason'],'result_set_sha256'=>$entry['result_set_sha256'],'safe_profile_sha256'=>$bindings['safe_profile_sha256'],'evidence_id'=>$entry['evidence_id'],'executor_fingerprint'=>hash('sha256','pay-rule-002-shadow:'.$executor_id),'executed_on'=>$as_of,'result_type'=>$value->type(),'result_unit'=>$value->unit());
    hrm_payroll_formula_safe_shadow_log('allowed','safe_shadow_approved',$source_scope,$source_hash,$executor_id);return $value;
}
