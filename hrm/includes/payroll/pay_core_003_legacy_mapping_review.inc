<?php
/**
 * Pure PAY-CORE-003 legacy element/salary mapping review.
 *
 * This contract is intentionally storage-free and non-authoritative. It never
 * converts legacy rows or infers approved payroll-input, eligibility, unit,
 * currency, worker/assignment/relationship, or rule-artifact provenance.
 */
include_once(dirname(__FILE__).'/pay_core_003_contract.inc');

if (!defined('HRM_PAY_CORE_003_LEGACY_MAPPING_REVIEW_VERSION'))
    define('HRM_PAY_CORE_003_LEGACY_MAPPING_REVIEW_VERSION', '1.0');
if (!defined('HRM_PAY_CORE_003_LEGACY_MAPPING_AUTOMATIC_CONVERSION_ALLOWED'))
    define('HRM_PAY_CORE_003_LEGACY_MAPPING_AUTOMATIC_CONVERSION_ALLOWED', false);

function hrm_pay_core_003_legacy_source_fields($kind)
{
    $fields=array(
        'pay_element'=>array('element_id','element_code','element_name','element_category','is_deduction','amount_type','default_amount','formula','account_code','employer_account','is_taxable','affects_gross','is_statutory','max_amount','min_amount','display_order','description','inactive'),
        'salary_structure'=>array('structure_id','position_id','grade_id','element_id','pay_amount','formula','effective_from','effective_to','is_active'),
        'employee_salary'=>array('salary_id','employee_id','element_id','amount','formula','effective_from','effective_to','is_active','reference')
    );
    return isset($fields[$kind])?$fields[$kind]:false;
}

function hrm_pay_core_003_legacy_source_id_field($kind)
{
    $ids=array('pay_element'=>'element_id','salary_structure'=>'structure_id','employee_salary'=>'salary_id');
    return isset($ids[$kind])?$ids[$kind]:false;
}

function hrm_pay_core_003_legacy_source_sha256($kind,$row)
{
    $fields=hrm_pay_core_003_legacy_source_fields($kind);
    if($fields===false||!is_array($row))return false;
    $ordered=array('source_kind'=>$kind);
    foreach($fields as $field){
        if(!array_key_exists($field,$row))return false;
        $value=$row[$field];
        if(!(is_null($value)||is_scalar($value)))return false;
        $ordered[$field]=$value;
    }
    $json=json_encode($ordered,JSON_UNESCAPED_SLASHES|JSON_UNESCAPED_UNICODE|JSON_PRESERVE_ZERO_FRACTION);
    return is_string($json)?hash('sha256',$json):false;
}

function hrm_pay_core_003_legacy_invalid($kind,$row,$codes)
{
    $id_field=hrm_pay_core_003_legacy_source_id_field($kind);
    return array(
        'source_kind'=>$kind,
        'legacy_source_id'=>($id_field!==false&&is_array($row)&&isset($row[$id_field]))?(int)$row[$id_field]:0,
        'source_sha256'=>hrm_pay_core_003_legacy_source_sha256($kind,$row),
        'disposition'=>'invalid_source',
        'automatic_conversion_allowed'=>false,
        'ambiguity_codes'=>$codes
    );
}

function hrm_pay_core_003_review_legacy_source($kind,$row)
{
    $fields=hrm_pay_core_003_legacy_source_fields($kind);
    if($fields===false)return hrm_pay_core_003_legacy_invalid((string)$kind,$row,array('unsupported_source_kind'));
    if(!is_array($row))return hrm_pay_core_003_legacy_invalid($kind,$row,array('source_not_array'));
    $missing=array();foreach($fields as $field)if(!array_key_exists($field,$row))$missing[]=$field;
    if($missing)return hrm_pay_core_003_legacy_invalid($kind,$row,array('missing_fields'))+array('missing_fields'=>$missing);
    $id_field=hrm_pay_core_003_legacy_source_id_field($kind);$id=filter_var($row[$id_field],FILTER_VALIDATE_INT);
    if($id===false||$id<=0)return hrm_pay_core_003_legacy_invalid($kind,$row,array('invalid_source_id'));

    $invalid=array();$ambiguity=array();
    if($kind==='pay_element'){
        if(trim((string)$row['element_code'])===''||trim((string)$row['element_name'])==='')$invalid[]='blank_element_identity';
        if(filter_var($row['element_category'],FILTER_VALIDATE_INT)===false||filter_var($row['amount_type'],FILTER_VALIDATE_INT)===false)$invalid[]='invalid_legacy_type_code';
        $ambiguity=array('stable_identity_requires_explicit_confirmation','element_category_requires_element_type_mapping','amount_type_requires_calculation_mode_mapping','default_amount_requires_version_or_input_decision','unit_and_currency_unavailable','eligibility_unavailable','approval_provenance_unavailable','source_version_unavailable','accounting_metadata_outside_pay_core_003');
        if(trim((string)$row['formula'])!=='')$ambiguity[]='formula_requires_governed_rule_artifact_review';
        else $ambiguity[]='calculation_semantics_require_explicit_review';
        if((int)$row['is_taxable']!==0||(int)$row['affects_gross']!==0||(int)$row['is_statutory']!==0)$ambiguity[]='tax_gross_statutory_semantics_require_policy_review';
        if((int)$row['inactive']!==0)$ambiguity[]='inactive_state_requires_effective_end_review';
    }elseif($kind==='salary_structure'){
        foreach(array('position_id','element_id') as $f)if(filter_var($row[$f],FILTER_VALIDATE_INT)===false||(int)$row[$f]<=0)$invalid[]='invalid_'.$f;
        if(filter_var($row['grade_id'],FILTER_VALIDATE_INT)===false||(int)$row['grade_id']<0)$invalid[]='invalid_grade_id';
        if(!hrm_pay_core_003_is_iso_date((string)$row['effective_from']))$invalid[]='invalid_effective_from';
        if($row['effective_to']!==null&&$row['effective_to']!==''&&!hrm_pay_core_003_valid_effective_range((string)$row['effective_from'],(string)$row['effective_to']))$invalid[]='invalid_effective_range';
        $ambiguity=array('legacy_element_mapping_required','position_grade_scope_requires_eligibility_review','pay_amount_requires_typed_input_or_version_decision','unit_and_currency_unavailable','payroll_relationship_ownership_unavailable','approval_provenance_unavailable');
        if(trim((string)$row['formula'])!=='')$ambiguity[]='formula_override_requires_governed_rule_artifact_review';
        if((int)$row['is_active']===0)$ambiguity[]='inactive_state_requires_effective_end_review';
    }else{
        if(trim((string)$row['employee_id'])==='')$invalid[]='blank_employee_id';
        if(filter_var($row['element_id'],FILTER_VALIDATE_INT)===false||(int)$row['element_id']<=0)$invalid[]='invalid_element_id';
        if(!hrm_pay_core_003_is_iso_date((string)$row['effective_from']))$invalid[]='invalid_effective_from';
        if($row['effective_to']!==null&&$row['effective_to']!==''&&!hrm_pay_core_003_valid_effective_range((string)$row['effective_from'],(string)$row['effective_to']))$invalid[]='invalid_effective_range';
        $ambiguity=array('legacy_element_mapping_required','employee_id_requires_worker_assignment_relationship_resolution','amount_requires_approved_typed_input_decision','unit_and_currency_unavailable','approval_provenance_unavailable','personal_salary_mode_requires_employee_context_review');
        if(trim((string)$row['formula'])!=='')$ambiguity[]='formula_override_requires_governed_rule_artifact_review';
        if(trim((string)$row['reference'])!=='')$ambiguity[]='legacy_reference_is_not_approval_evidence';
        if((int)$row['is_active']===0)$ambiguity[]='inactive_state_requires_effective_end_review';
    }
    if($invalid)return hrm_pay_core_003_legacy_invalid($kind,$row,$invalid);
    return array('source_kind'=>$kind,'legacy_source_id'=>(int)$id,'source_sha256'=>hrm_pay_core_003_legacy_source_sha256($kind,$row),'disposition'=>'manual_review','automatic_conversion_allowed'=>false,'ambiguity_codes'=>array_values(array_unique($ambiguity)));
}
