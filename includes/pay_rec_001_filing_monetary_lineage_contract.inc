<?php
/** PAY-REC-001 filing monetary-lineage contract. Pure helpers only; no DB or close authority. */
include_once(dirname(__FILE__).'/pay_fil_001_contract.inc');
include_once(dirname(__FILE__).'/pay_fil_001_tt80_semantic_source_custody_contract.inc');
include_once(dirname(__FILE__).'/../hrm/includes/payroll/pay_core_004_contract.inc');
if (!defined('PAY_REC_001_FILING_MONETARY_LINEAGE_CONTRACT_VERSION'))
    define('PAY_REC_001_FILING_MONETARY_LINEAGE_CONTRACT_VERSION', '1.0');

function pay_rec_001_filing_monetary_fields()
{
    return array(
        'filing_taxable_income_minor'=>array('indicators'=>array('ct21','ct22','ct23'),'allowed_line_kinds'=>array('earning','information')),
        'withholding_subject_taxable_income_minor'=>array('indicators'=>array('ct26','ct27','ct28'),'allowed_line_kinds'=>array('earning','information')),
        'personal_income_tax_withheld_minor'=>array('indicators'=>array('ct29','ct30','ct31'),'allowed_line_kinds'=>array('deduction','information')),
        'foreign_insurer_nonmandatory_insurance_taxable_minor'=>array('indicators'=>array('ct24'),'allowed_line_kinds'=>array('earning','information')),
        'foreign_insurer_nonmandatory_insurance_withheld_minor'=>array('indicators'=>array('ct32'),'allowed_line_kinds'=>array('deduction','information')),
        'petroleum_contract_exempt_taxable_minor'=>array('indicators'=>array('ct25'),'allowed_line_kinds'=>array('earning','information')),
        'resolution_exempt_income_minor'=>array('indicators'=>array('ct25_1'),'allowed_line_kinds'=>array('earning','information'))
    );
}

function pay_rec_001_filing_monetary_lineage_contract()
{
    return array(
        'contract_version'=>PAY_REC_001_FILING_MONETARY_LINEAGE_CONTRACT_VERSION,
        'jurisdiction_code'=>'VN',
        'form_id'=>'05_KK_TNCN_TT80',
        'currency_code'=>'VND',
        'amount_unit'=>'minor_currency_unit',
        'rounding_policy'=>'pay_core_vnd_whole_units_exact_no_refiling_rounding_no_implicit_tolerance',
        'source_membership'=>'exact_accepted_pay_fil_source_set_result_membership',
        'monetary_lineage_mode'=>'EXACT_PAY_CORE_RESULT_LINE_TO_TT80_PERSON_FACT',
        'pay_core_result_line_membership'=>'verified_pay_core_004_result_bundle',
        'subject_binding'=>'hash_bound_payroll_relationship_to_tt80_subject_key',
        'classification_profile'=>'versioned_country_pack_element_semantic_profile',
        'classification_review_required'=>true,
        'automatic_semantic_inference_allowed'=>false,
        'nonzero_fact_requires_lineage'=>true,
        'regular_offcycle_correction_lineage_required'=>true,
        'predecessor_result_binding_required_for_correction'=>true,
        'filing_fact_rewrite_allowed'=>false,
        'payroll_result_rewrite_allowed'=>false,
        'historical_accounting_rewrite_allowed'=>false,
        'production_filing_authority_allowed'=>false,
        'runtime_close_authority_allowed'=>false,
        'monetary_fields'=>pay_rec_001_filing_monetary_fields()
    );
}

function pay_rec_001_hash($value)
{
    return pay_fil_001_hash($value);
}
function pay_rec_001_sha($value)
{
    return pay_fil_001_sha($value);
}
function pay_rec_001_uint($value)
{
    return pay_fil_001_uint($value);
}
function pay_rec_001_token($value,$max=128)
{
    return pay_fil_001_token($value,$max);
}
function pay_rec_001_fail(&$error,$code)
{
    $error=(string)$code;
    return false;
}

function pay_rec_001_validate_filing_classification_profile(array $profile,&$error=null)
{
    $error=null;
    $keys=array('jurisdiction_code','country_pack_id','country_pack_version','semantic_field','element_version_id','line_kind','effective_from','effective_to','review_evidence_sha256','profile_sha256');
    foreach($keys as $key)if(!array_key_exists($key,$profile))return pay_rec_001_fail($error,'filing_classification_profile_incomplete');
    $fields=pay_rec_001_filing_monetary_fields();
    if($profile['jurisdiction_code']!=='VN'||pay_rec_001_token($profile['country_pack_id'],96)===false||pay_rec_001_token($profile['country_pack_version'],32)===false
        ||!isset($fields[$profile['semantic_field']])||pay_rec_001_uint($profile['element_version_id'])===false
        ||!in_array($profile['line_kind'],$fields[$profile['semantic_field']]['allowed_line_kinds'],true)
        ||pay_fil_001_date($profile['effective_from'])===false
        ||($profile['effective_to']!==''&&pay_fil_001_date($profile['effective_to'])===false)
        ||($profile['effective_to']!==''&&$profile['effective_to']<$profile['effective_from'])
        ||!pay_rec_001_sha($profile['review_evidence_sha256'])||!pay_rec_001_sha($profile['profile_sha256']))
        return pay_rec_001_fail($error,'filing_classification_profile_invalid');
    $basis=$profile;unset($basis['profile_sha256']);
    if(!hash_equals(pay_rec_001_hash($basis),(string)$profile['profile_sha256']))return pay_rec_001_fail($error,'filing_classification_profile_hash_mismatch');
    return $profile;
}

/** Recompute the exact accepted PAY-FIL person-fact content hash without database access. */
function pay_rec_001_validate_tt80_person_fact(array $fact,&$error=null)
{
    $error=null;
    $fields=pay_fil_001_tt80_semantic_custody_person_fields();
    $required=array_merge($fields,array('filing_source_set_id','source_evidence_sha256','fact_sha256'));
    foreach($required as $key)if(!array_key_exists($key,$fact))return pay_rec_001_fail($error,'filing_person_fact_incomplete');
    $set_id=pay_rec_001_uint($fact['filing_source_set_id']);
    if($set_id===false||$set_id<=0||pay_rec_001_token($fact['subject_key'],128)===false
        ||!in_array((string)$fact['paid_worker'],array('0','1'),true)
        ||!in_array((string)$fact['withheld_person'],array('0','1'),true)
        ||!in_array($fact['tax_residency'],array('resident','nonresident','unknown'),true)
        ||!in_array($fact['employment_contract_term_class'],array('at_least_3_months','under_3_months','not_applicable','unknown'),true)
        ||$fact['currency_code']!=='VND'||!pay_rec_001_sha($fact['source_evidence_sha256'])||!pay_rec_001_sha($fact['fact_sha256']))
        return pay_rec_001_fail($error,'filing_person_fact_invalid');
    $amount_fields=array('filing_taxable_income_minor','withholding_subject_taxable_income_minor','personal_income_tax_withheld_minor','foreign_insurer_nonmandatory_insurance_taxable_minor','foreign_insurer_nonmandatory_insurance_withheld_minor','petroleum_contract_exempt_taxable_minor','resolution_exempt_income_minor');
    foreach($amount_fields as $field)if(pay_rec_001_uint($fact[$field])===false)return pay_rec_001_fail($error,'filing_person_fact_amount_invalid');
    $basis=array('filing_source_set_id'=>$set_id);
    foreach($fields as $field){
        $basis[$field]=in_array($field,array_merge($amount_fields,array('paid_worker','withheld_person')),true)
            ?(int)$fact[$field]:(string)$fact[$field];
    }
    $basis['source_evidence_sha256']=(string)$fact['source_evidence_sha256'];
    $expected=pay_rec_001_hash($basis);
    if($expected===false||!hash_equals($expected,(string)$fact['fact_sha256']))return pay_rec_001_fail($error,'filing_person_fact_hash_mismatch');
    return $fact;
}

/** Convert an already-rounded PAY-CORE VND amount to whole dong without rounding it again. */
function pay_rec_001_vnd_amount_minor($amount,&$error=null)
{
    $error=null;
    if(!is_int($amount)&&!is_string($amount))return pay_rec_001_fail($error,'filing_vnd_result_line_amount_invalid');
    $value=(string)$amount;
    if(!preg_match('/^(0|[1-9][0-9]*)(?:\.([0-9]+))?$/D',$value,$parts))return pay_rec_001_fail($error,'filing_vnd_result_line_amount_invalid');
    if(isset($parts[2])&&trim($parts[2],'0')!=='')return pay_rec_001_fail($error,'filing_vnd_fractional_result_line');
    $minor=pay_rec_001_uint($parts[1]);
    if($minor===false)return pay_rec_001_fail($error,'filing_vnd_result_line_amount_out_of_range');
    return $minor;
}

/** Index only canonical PAY-CORE-004 result bundles whose line-set hash verifies. */
function pay_rec_001_index_pay_core_result_bundles(array $bundles,&$results,&$lines,&$error=null)
{
    $error=null;$results=array();$lines=array();
    foreach($bundles as $bundle){
        if(!is_array($bundle))return pay_rec_001_fail($error,'filing_pay_core_result_bundle_invalid');
        foreach(array('snapshot_descriptor','result','lines','traces') as $key)if(!array_key_exists($key,$bundle))return pay_rec_001_fail($error,'filing_pay_core_result_bundle_incomplete');
        if(!is_array($bundle['snapshot_descriptor'])||!is_array($bundle['result'])||!is_array($bundle['lines'])||!is_array($bundle['traces'])
            ||!isset($bundle['snapshot_descriptor']['payroll_relationship_id'],$bundle['snapshot_descriptor']['currency'])
            ||$bundle['snapshot_descriptor']['currency']!=='VND'||!isset($bundle['result']['currency'])||$bundle['result']['currency']!=='VND')
            return pay_rec_001_fail($error,'filing_pay_core_result_bundle_invalid');
        $relationship=pay_rec_001_uint($bundle['snapshot_descriptor']['payroll_relationship_id']);
        $snapshot_sha=hrm_pay_core_004_snapshot_sha256($bundle['snapshot_descriptor']);
        if($relationship===false||$relationship<=0||$snapshot_sha===false)return pay_rec_001_fail($error,'filing_pay_core_snapshot_invalid');
        $result_sha=hrm_pay_core_004_verify_result_bundle($snapshot_sha,$bundle['result'],$bundle['lines'],$bundle['traces']);
        if($result_sha===false)return pay_rec_001_fail($error,'filing_pay_core_result_bundle_hash_mismatch');
        if(isset($results[$result_sha]))return pay_rec_001_fail($error,'filing_pay_core_result_duplicate');
        $results[$result_sha]=array('payroll_relationship_id'=>$relationship,'snapshot_sha256'=>$snapshot_sha);
        $lines[$result_sha]=array();
        foreach($bundle['lines'] as $line){
            $line_sha=hrm_pay_core_004_result_line_sha256($line);
            if($line_sha===false||!isset($line['currency'])||$line['currency']!=='VND')return pay_rec_001_fail($error,'filing_pay_core_result_line_invalid');
            if(isset($lines[$result_sha][$line_sha]))return pay_rec_001_fail($error,'filing_pay_core_result_line_duplicate');
            $lines[$result_sha][$line_sha]=$line;
        }
    }
    return true;
}

function pay_rec_001_validate_filing_lineage_row(array $row,array $profiles,array $results,array $lines,&$error=null)
{
    $error=null;
    $keys=array('tt80_person_fact_sha256','subject_key','payroll_relationship_id','semantic_field','result_sha256','result_line_sha256','element_version_id','line_kind','currency_code','amount_minor','classification_profile_sha256','run_kind','predecessor_result_sha256','subject_binding_sha256');
    foreach($keys as $key)if(!array_key_exists($key,$row))return pay_rec_001_fail($error,'filing_lineage_row_incomplete');
    $fields=pay_rec_001_filing_monetary_fields();
    $relationship=pay_rec_001_uint($row['payroll_relationship_id']);
    if(!pay_rec_001_sha($row['tt80_person_fact_sha256'])||pay_rec_001_token($row['subject_key'],128)===false||$relationship===false||$relationship<=0||!isset($fields[$row['semantic_field']])
        ||!pay_rec_001_sha($row['result_sha256'])||!pay_rec_001_sha($row['result_line_sha256'])||pay_rec_001_uint($row['element_version_id'])===false
        ||!in_array($row['line_kind'],$fields[$row['semantic_field']]['allowed_line_kinds'],true)||$row['currency_code']!=='VND'
        ||pay_rec_001_uint($row['amount_minor'])===false||!pay_rec_001_sha($row['classification_profile_sha256'])
        ||!in_array($row['run_kind'],array('regular','offcycle','correction'),true)||!pay_rec_001_sha($row['subject_binding_sha256']))
        return pay_rec_001_fail($error,'filing_lineage_row_invalid');
    if(!isset($results[$row['result_sha256']])||!isset($lines[$row['result_sha256']][$row['result_line_sha256']]))return pay_rec_001_fail($error,'filing_lineage_result_line_not_in_result');
    if((int)$results[$row['result_sha256']]['payroll_relationship_id']!==$relationship)return pay_rec_001_fail($error,'filing_lineage_payroll_relationship_mismatch');
    $source_line=$lines[$row['result_sha256']][$row['result_line_sha256']];
    if((int)$source_line['element_version_id']!==(int)$row['element_version_id']||$source_line['line_kind']!==$row['line_kind'])return pay_rec_001_fail($error,'filing_lineage_result_line_scope_mismatch');
    $source_amount=pay_rec_001_vnd_amount_minor($source_line['amount'],$amount_error);
    if($source_amount===false)return pay_rec_001_fail($error,$amount_error);
    if($source_amount!==(int)$row['amount_minor'])return pay_rec_001_fail($error,'filing_lineage_amount_source_mismatch');
    if($row['run_kind']==='correction'){
        if(!pay_rec_001_sha($row['predecessor_result_sha256']))return pay_rec_001_fail($error,'filing_correction_predecessor_required');
        if($row['predecessor_result_sha256']===$row['result_sha256']||!isset($results[$row['predecessor_result_sha256']])
            ||(int)$results[$row['predecessor_result_sha256']]['payroll_relationship_id']!==$relationship)
            return pay_rec_001_fail($error,'filing_correction_predecessor_binding_invalid');
    }elseif($row['predecessor_result_sha256']!=='')return pay_rec_001_fail($error,'filing_noncorrection_predecessor_forbidden');
    if(!isset($profiles[$row['classification_profile_sha256']]))return pay_rec_001_fail($error,'filing_classification_profile_missing');
    $profile=pay_rec_001_validate_filing_classification_profile($profiles[$row['classification_profile_sha256']],$pe);if($profile===false)return pay_rec_001_fail($error,$pe);
    if($profile['semantic_field']!==$row['semantic_field']||(int)$profile['element_version_id']!==(int)$row['element_version_id']||$profile['line_kind']!==$row['line_kind'])
        return pay_rec_001_fail($error,'filing_classification_profile_scope_mismatch');
    return $row;
}

/** Verify exact nonzero TT80 monetary facts against classified PAY-CORE result-line lineage. */
function pay_rec_001_verify_filing_monetary_lineage(array $person_facts,array $lineage_rows,array $profiles,array $pay_core_result_bundles,&$error=null)
{
    $error=null;$fields=pay_rec_001_filing_monetary_fields();$facts=array();$verified_results=array();$verified_lines=array();
    if(!pay_rec_001_index_pay_core_result_bundles($pay_core_result_bundles,$verified_results,$verified_lines,$error))return false;
    foreach($person_facts as $fact){
        if(!is_array($fact))return pay_rec_001_fail($error,'filing_person_fact_invalid');
        $fact=pay_rec_001_validate_tt80_person_fact($fact,$fact_error);if($fact===false)return pay_rec_001_fail($error,$fact_error);
        if(isset($facts[$fact['fact_sha256']]))return pay_rec_001_fail($error,'filing_person_fact_duplicate');
        foreach($fields as $field=>$spec)if(!array_key_exists($field,$fact)||pay_rec_001_uint($fact[$field])===false)return pay_rec_001_fail($error,'filing_person_fact_amount_invalid');
        $facts[$fact['fact_sha256']]=$fact;
    }
    $sums=array();$seen=array();
    foreach($lineage_rows as $row){
        if(!is_array($row))return pay_rec_001_fail($error,'filing_lineage_row_invalid');
        $valid=pay_rec_001_validate_filing_lineage_row($row,$profiles,$verified_results,$verified_lines,$le);if($valid===false)return pay_rec_001_fail($error,$le);
        if(!isset($facts[$row['tt80_person_fact_sha256']]))return pay_rec_001_fail($error,'filing_lineage_fact_missing');
        $fact=$facts[$row['tt80_person_fact_sha256']];if($fact['subject_key']!==$row['subject_key'])return pay_rec_001_fail($error,'filing_lineage_subject_mismatch');
        $key=$row['result_sha256'].'|'.$row['result_line_sha256'];if(isset($seen[$key]))return pay_rec_001_fail($error,'filing_lineage_duplicate');$seen[$key]=true;
        $sumkey=$row['tt80_person_fact_sha256'].'|'.$row['semantic_field'];if(!isset($sums[$sumkey]))$sums[$sumkey]=0;
        $amount=(int)$row['amount_minor'];if($amount>PHP_INT_MAX-$sums[$sumkey])return pay_rec_001_fail($error,'filing_monetary_lineage_sum_out_of_range');$sums[$sumkey]+=$amount;
    }
    foreach($facts as $sha=>$fact)foreach($fields as $field=>$spec){$expected=(int)$fact[$field];$actual=isset($sums[$sha.'|'.$field])?(int)$sums[$sha.'|'.$field]:0;if($expected!==$actual)return pay_rec_001_fail($error,$expected>0?'filing_monetary_lineage_amount_mismatch':'filing_monetary_lineage_unexpected_amount');}
    $result_hashes=array_keys($verified_results);sort($result_hashes,SORT_STRING);
    $receipt=array('contract_sha256'=>pay_rec_001_hash(pay_rec_001_filing_monetary_lineage_contract()),'person_fact_set_sha256'=>pay_rec_001_hash(array_keys($facts)),'lineage_row_count'=>count($lineage_rows),'lineage_set_sha256'=>pay_rec_001_hash($lineage_rows),'pay_core_result_set_sha256'=>pay_rec_001_hash($result_hashes),'classification_profile_set_sha256'=>pay_rec_001_hash(array_keys($profiles)),'currency_code'=>'VND','verification_status'=>'verified');
    $receipt['receipt_sha256']=pay_rec_001_hash($receipt);return $receipt;
}
?>
