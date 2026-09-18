<?php
/** Pure, fail-closed PAY-CORE-003 eligibility policy. It does not execute rule artifacts. */
include_once(dirname(__FILE__).'/pay_core_003_governance.inc');
if (!defined('HRM_PAY_CORE_003_ELIGIBILITY_POLICY_VERSION')) define('HRM_PAY_CORE_003_ELIGIBILITY_POLICY_VERSION','1.0');
function hrm_pay_core_003_select_eligibility_version($versions,$as_of)
{
    if (!is_array($versions)||!hrm_pay_core_003_is_iso_date($as_of)) return false;
    $matches=array();
    foreach ($versions as $v) if (hrm_pay_core_003_validate_eligibility_version($v) && $v['effective_from']<=$as_of && (empty($v['effective_to'])||$v['effective_to']>=$as_of)) $matches[]=$v;
    return count($matches)===1?$matches[0]:false;
}
function hrm_pay_core_003_evaluate_eligibility($version,$rule_artifact_sha256=null,$rule_result=null)
{
    if (!hrm_pay_core_003_validate_eligibility_version($version)) return array('eligible'=>false,'decided'=>false,'reason'=>'invalid_eligibility_version');
    if ($version['eligibility_kind']==='all') return array('eligible'=>true,'decided'=>true,'reason'=>'explicit_all');
    if (!hrm_pay_core_003_is_sha256($rule_artifact_sha256) || !hash_equals((string)$version['rule_artifact_sha256'],(string)$rule_artifact_sha256) || !is_bool($rule_result)) return array('eligible'=>false,'decided'=>false,'reason'=>'rule_evidence_missing_or_mismatched');
    return array('eligible'=>$rule_result,'decided'=>true,'reason'=>'explicit_rule_result');
}
