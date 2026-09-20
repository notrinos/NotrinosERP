<?php
/** PAY-CORE-009 minimized staging adapter for rollback-only legacy calculations. */
include_once(dirname(__FILE__).'/pay_core_009_contract.inc');

function hrm_pay_core_009_doc_scalar_fields(){return array(
    'payroll_period_id','employee_id','from_date','to_date','tran_date','comments',
    'basic_salary','total_earnings','gross_salary','total_deductions','net_salary',
    'employer_contributions','total_cost','working_days','worked_days','overtime_hours',
    'leave_days','paid_leave_days','unpaid_leave_days','payable_days','absent_days',
    'extra_absent_days','regular_hours','loan_deduction','currency_code','exchange_rate','net_salary_home'
);}
function hrm_pay_core_009_line_fields(){return array('element_id','element_name','element_category','is_deduction','amount_type','base_amount','rate','calculated_amount','final_amount','account_code','formula_used','is_taxable','display_order');}

/** Minimize staged employee data to fields needed by the existing preparation writer. */
function hrm_pay_core_009_stage_document($doc,&$error=null)
{
    $error=null;if(!is_object($doc)||empty($doc->employee_id)||!isset($doc->lines)||!is_array($doc->lines)){$error='invalid_calculation_document';return false;}
    $payload=array('contract_version'=>HRM_PAY_CORE_009_CONTRACT_VERSION,'document'=>array(),'employee'=>array(),'lines'=>array());
    foreach(hrm_pay_core_009_doc_scalar_fields() as $f){if(property_exists($doc,$f))$payload['document'][$f]=$doc->$f;}
    foreach(array('department_id','position_id','grade_id') as $f)if(isset($doc->employee[$f]))$payload['employee'][$f]=(int)$doc->employee[$f];
    foreach($doc->lines as $line){
        if(!is_object($line)){$error='invalid_calculation_line';return false;}$x=array();foreach(hrm_pay_core_009_line_fields() as $f){if(!property_exists($line,$f)){$error='incomplete_calculation_line';return false;}$x[$f]=$line->$f;}$payload['lines'][]=$x;
    }
    $json=hrm_pay_core_009_canonical_json($payload);if($json===false){$error='stage_encoding_failed';return false;}
    return array('payload'=>$payload,'payload_json'=>$json,'result_sha256'=>hash('sha256',$json));
}

function hrm_pay_core_009_rehydrate_document($payload_json,$expected_sha256,&$error=null)
{
    $error=null;if(!is_string($payload_json)||$payload_json===''||!hrm_pay_core_009_is_sha256($expected_sha256)||!hash_equals($expected_sha256,hash('sha256',$payload_json))){$error='staged_result_hash_mismatch';return false;}
    $payload=json_decode($payload_json,true);if(!is_array($payload)||($payload['contract_version']??null)!==HRM_PAY_CORE_009_CONTRACT_VERSION||!is_array($payload['document']??null)||!is_array($payload['employee']??null)||!is_array($payload['lines']??null)){$error='staged_result_invalid';return false;}
    if(!class_exists('payslip_doc')||!class_exists('payslip_line')){$error='legacy_document_class_unavailable';return false;}
    $doc=new payslip_doc();foreach(hrm_pay_core_009_doc_scalar_fields() as $f)if(array_key_exists($f,$payload['document']))$doc->$f=$payload['document'][$f];$doc->employee=(array)$payload['employee'];$doc->employee['employee_id']=$doc->employee_id;
    foreach($payload['lines'] as $x){foreach(hrm_pay_core_009_line_fields() as $f)if(!array_key_exists($f,$x)){$error='staged_line_invalid';return false;}$doc->add_line((int)$x['element_id'],(string)$x['element_name'],(int)$x['element_category'],(bool)$x['is_deduction'],(int)$x['amount_type'],(float)$x['base_amount'],(float)$x['rate'],(float)$x['calculated_amount'],(float)$x['final_amount'],(string)$x['account_code'],$x['formula_used']===null?null:(string)$x['formula_used'],(bool)$x['is_taxable'],(int)$x['display_order']);}
    return $doc;
}
?>
