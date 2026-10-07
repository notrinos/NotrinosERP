<?php
/** PAY-FIL-001 TT80 source-bound render-model activation contract. Pure helpers only. */
include_once(dirname(__FILE__).'/pay_fil_001_contract.inc');
include_once(dirname(__FILE__).'/pay_fil_001_tt80_renderer_contract.inc');
if (!defined('PAY_FIL_001_TT80_RENDER_MODEL_CONTRACT_VERSION')) define('PAY_FIL_001_TT80_RENDER_MODEL_CONTRACT_VERSION','1.0');

/** Development-only authority profile. This activates deterministic generation for local validation, never production submission. */
function pay_fil_001_tt80_render_model_activation_profile()
{
    return array(
        'contract_version'=>PAY_FIL_001_TT80_RENDER_MODEL_CONTRACT_VERSION,
        'renderer_id'=>'pay-fil-001.vn.tt80.05-kk-tncn-xml',
        'renderer_version'=>'1.0',
        'form'=>'05/KK-TNCN',
        'form_id'=>'05_KK_TNCN_TT80',
        'schema_version'=>'2.9.3',
        'development_gate'=>'OFFICIAL_LOCAL_VALIDATOR_COMPATIBILITY',
        'required_mapping_status'=>'PUBLISHED_REVIEW_BOUND',
        'required_permission'=>PAY_FIL_001_PREPARE_PERMISSION,
        'render_model_source'=>'immutable_obligation_source_set_taxpayer_profile_and_published_semantic_mapping_plus_explicit_render_context',
        'development_runtime_generation_allowed'=>true,
        'artifact_publication_allowed'=>false,
        'production_generation_allowed'=>false,
        'production_signing_allowed'=>false,
        'production_credentials_allowed'=>false,
        'production_transmission_allowed'=>false,
        'network_authority_allowed'=>false,
        'portal_submission_allowed'=>false,
        'historical_mutation_allowed'=>false,
        'automatic_semantic_inference_allowed'=>false
    );
}
function pay_fil_001_tt80_render_model_activation_contract_sha256(){return pay_fil_001_hash(pay_fil_001_tt80_render_model_activation_profile());}

function pay_fil_001_tt80_render_context_required_keys()
{
    return array('record_id','service_name','service_version','provider_info','declaration_kind','sequence_no','prepared_on','signer_name','signed_on','mst_cu','ct15','taxpayer_contact','tax_agent','extension');
}
function pay_fil_001_tt80_render_context_empty_agent()
{
    return array('tax_code'=>'','name'=>'','address'=>'','district_code'=>'','district_name'=>'','province_code'=>'','province_name'=>'','phone'=>'','fax'=>'','email'=>'','contract_number'=>'','contract_signed_on'=>'','employee_name'=>'','professional_certificate'=>'');
}
function pay_fil_001_tt80_render_context_empty_contact(){return array('district_code'=>'','district_name'=>'','phone'=>'','fax'=>'','email'=>'','business_activity'=>'');}
function pay_fil_001_tt80_render_context_empty_extension(){return array('reason_code'=>'','reason'=>'');}
function pay_fil_001_tt80_exact_key_set(array $actual,array $expected)
{
    $a=array_keys($actual);$e=array_keys($expected);sort($a,SORT_STRING);sort($e,SORT_STRING);return $a===$e;
}

/** Validate explicit non-payroll render context. Statutory indicators and taxpayer identity cannot be supplied here. */
function pay_fil_001_tt80_validate_render_context(array $c,&$error=null)
{
    $error=null;foreach(pay_fil_001_tt80_render_context_required_keys() as $k)if(!array_key_exists($k,$c)){ $error='tt80_render_context_field_missing';return false; }
    foreach(array('taxpayer','tax_office','indicators','source_binding','period','period_start','period_end','semantic_mapping_sha256') as $forbidden)if(array_key_exists($forbidden,$c)){ $error='tt80_render_context_authority_violation';return false; }
    if(pay_fil_001_tt80_text($c['record_id'],64,$error,'record_id')===false||pay_fil_001_tt80_text($c['service_name'],128,$error,'service_name')===false||pay_fil_001_tt80_text($c['service_version'],32,$error,'service_version')===false||pay_fil_001_tt80_text($c['provider_info'],160,$error,'provider_info')===false)return false;
    if(!in_array($c['declaration_kind'],array('C','B'),true)||pay_fil_001_posint($c['sequence_no'])===false||pay_fil_001_date($c['prepared_on'])===false||pay_fil_001_tt80_text($c['signer_name'],160,$error,'signer_name')===false||pay_fil_001_date($c['signed_on'])===false){$error=$error?:'tt80_render_context_header_invalid';return false;}
    if(pay_fil_001_tt80_text($c['mst_cu'],32,$error,'mst_cu')===false||!is_bool($c['ct15'])){$error=$error?:'tt80_render_context_indicator_invalid';return false;}
    if(!is_array($c['taxpayer_contact'])||!pay_fil_001_tt80_exact_key_set($c['taxpayer_contact'],pay_fil_001_tt80_render_context_empty_contact())){$error='tt80_render_context_taxpayer_contact_invalid';return false;}
    $contact=$c['taxpayer_contact'];foreach(array('district_code'=>64,'district_name'=>160,'phone'=>64,'fax'=>64,'email'=>160,'business_activity'=>240) as $k=>$max)if(pay_fil_001_tt80_text($contact[$k],$max,$error,'taxpayer_'.$k,true)===false)return false;
    if(!is_array($c['tax_agent'])||!pay_fil_001_tt80_exact_key_set($c['tax_agent'],pay_fil_001_tt80_render_context_empty_agent())){$error='tt80_render_context_tax_agent_invalid';return false;}
    foreach(array('tax_code'=>32,'name'=>200,'address'=>240,'district_code'=>64,'district_name'=>160,'province_code'=>64,'province_name'=>160,'phone'=>64,'fax'=>64,'email'=>160,'contract_number'=>80,'contract_signed_on'=>10,'employee_name'=>160,'professional_certificate'=>80) as $k=>$max)if(pay_fil_001_tt80_text($c['tax_agent'][$k],$max,$error,'tax_agent_'.$k,true)===false)return false;
    if($c['tax_agent']['contract_signed_on']!==''&&pay_fil_001_date($c['tax_agent']['contract_signed_on'])===false){$error='tt80_tax_agent_contract_date_invalid';return false;}
    if(!is_array($c['extension'])||!pay_fil_001_tt80_exact_key_set($c['extension'],pay_fil_001_tt80_render_context_empty_extension())){$error='tt80_render_context_extension_invalid';return false;}
    if(pay_fil_001_tt80_text($c['extension']['reason_code'],64,$error,'extension_reason_code',true)===false||pay_fil_001_tt80_text($c['extension']['reason'],200,$error,'extension_reason',true)===false)return false;
    $out=$c;$out['sequence_no']=pay_fil_001_posint($c['sequence_no']);return $out;
}

/** Derive exact quarterly label only from an obligation's exact start/end dates. */
function pay_fil_001_tt80_quarter_label($start,$end,&$error=null)
{
    $error=null;if(pay_fil_001_date($start)===false||pay_fil_001_date($end)===false){$error='tt80_render_period_invalid';return false;}
    $y=(int)substr($start,0,4);$m=(int)substr($start,5,2);if(substr($start,8,2)!=='01'||!in_array($m,array(1,4,7,10),true)){ $error='tt80_render_period_not_quarter';return false; }
    $q=(int)(($m-1)/3)+1;$endMonth=$q*3;$expectedEnd=date('Y-m-t',strtotime(sprintf('%04d-%02d-01',$y,$endMonth)));if($end!==$expectedEnd){$error='tt80_render_period_not_quarter';return false;}return $q.'/'.$y;
}

/** Build the normalized model from verified immutable custody and explicit context. */
function pay_fil_001_tt80_build_render_model(array $source,array $taxpayer,array $mapping,array $context,&$error=null)
{
    $error=null;$c=pay_fil_001_tt80_validate_render_context($context,$error);if($c===false)return false;
    foreach(array('company_id','legal_entity_id','obligation_sha256','source_set_sha256','currency_code','period_start','period_end') as $k)if(!array_key_exists($k,$source))return pay_fil_001_fail($error,'tt80_render_source_incomplete');
    foreach(array('taxpayer_tax_code','taxpayer_name','taxpayer_address','taxpayer_ward_code','taxpayer_ward_name','taxpayer_province_code','taxpayer_province_name','tax_office_code','tax_office_name') as $k)if(!array_key_exists($k,$taxpayer))return pay_fil_001_fail($error,'tt80_render_taxpayer_profile_incomplete');
    foreach(array('mapping_sha256','indicator_values') as $k)if(!array_key_exists($k,$mapping))return pay_fil_001_fail($error,'tt80_render_mapping_incomplete');
    if(pay_fil_001_uint($source['company_id'])===false||pay_fil_001_posint($source['legal_entity_id'])===false||!pay_fil_001_sha($source['obligation_sha256'])||!pay_fil_001_sha($source['source_set_sha256'])||$source['currency_code']!=='VND'||!pay_fil_001_sha($mapping['mapping_sha256'])||!is_array($mapping['indicator_values']))return pay_fil_001_fail($error,'tt80_render_source_binding_invalid');
    $period=pay_fil_001_tt80_quarter_label($source['period_start'],$source['period_end'],$error);if($period===false)return false;
    $ind=array('mst_cu'=>$c['mst_cu'],'ct15'=>$c['ct15']);foreach(array('ct16','ct17','ct18','ct19','ct20','ct21','ct22','ct23','ct24','ct25','ct25_1','ct26','ct27','ct28','ct29','ct30','ct31','ct32') as $k){if(!array_key_exists($k,$mapping['indicator_values']))return pay_fil_001_fail($error,'tt80_render_mapping_indicator_missing');$ind[$k]=(string)$mapping['indicator_values'][$k];}
    $contact=$c['taxpayer_contact'];
    $model=array(
        'source_binding'=>array('company_id'=>$source['company_id'],'legal_entity_id'=>$source['legal_entity_id'],'obligation_sha256'=>$source['obligation_sha256'],'source_set_sha256'=>$source['source_set_sha256'],'semantic_mapping_sha256'=>$mapping['mapping_sha256'],'currency_code'=>'VND'),
        'header'=>array('record_id'=>$c['record_id'],'service_name'=>$c['service_name'],'service_version'=>$c['service_version'],'provider_info'=>$c['provider_info'],'declaration_kind'=>$c['declaration_kind'],'sequence_no'=>$c['sequence_no'],'period'=>$period,'period_start'=>$source['period_start'],'period_end'=>$source['period_end'],'prepared_on'=>$c['prepared_on'],'signer_name'=>$c['signer_name'],'signed_on'=>$c['signed_on']),
        'taxpayer'=>array('tax_code'=>$taxpayer['taxpayer_tax_code'],'name'=>$taxpayer['taxpayer_name'],'address'=>$taxpayer['taxpayer_address'],'ward_name'=>$taxpayer['taxpayer_ward_name'],'ward_code'=>$taxpayer['taxpayer_ward_code'],'district_code'=>$contact['district_code'],'district_name'=>$contact['district_name'],'province_code'=>$taxpayer['taxpayer_province_code'],'province_name'=>$taxpayer['taxpayer_province_name'],'phone'=>$contact['phone'],'fax'=>$contact['fax'],'email'=>$contact['email'],'business_activity'=>$contact['business_activity']),
        'tax_office'=>array('code'=>$taxpayer['tax_office_code'],'name'=>$taxpayer['tax_office_name']),
        'tax_agent'=>$c['tax_agent'],'extension'=>$c['extension'],'indicators'=>$ind
    );
    return pay_fil_001_validate_tt80_render_model($model,$error);
}
?>
