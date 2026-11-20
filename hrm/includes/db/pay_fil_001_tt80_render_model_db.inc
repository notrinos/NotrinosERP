<?php
/** PAY-FIL-001 TT80 source-bound render-model construction and local-development generation activation. */
include_once(dirname(__FILE__).'/pay_fil_001_tt80_semantic_mapping_db.inc');
include_once($path_to_root.'/includes/pay_fil_001_tt80_render_model_contract.inc');

function pay_fil_001_tt80_render_model_contract_required_for_database_version($version){return version_compare((string)$version,'1.0.1040','>=');}
function pay_fil_001_tt80_render_model_runtime_required_for_database_version($version){return version_compare((string)$version,'1.0.1041','>=');}
function pay_fil_001_tt80_runtime_generation_required_for_database_version($version){return version_compare((string)$version,'1.0.1042','>=');}

/** Construct a model only from immutable published mapping/profile custody plus explicit non-payroll context. */
function pay_fil_001_tt80_construct_render_model($filing_source_set_id,array $context,&$error=null)
{
    $error=null;if(!pay_fil_001_require_access(PAY_FIL_001_PREPARE_PERMISSION,$error))return false;$company=pay_fil_001_company();$set_id=pay_fil_001_posint($filing_source_set_id);if($company<0||$set_id===false)return pay_fil_001_runtime_fail($error,'tt80_render_model_scope_invalid');
    $s=pay_fil_001_one('SELECT s.filing_source_set_id,s.filing_obligation_id,s.source_set_sha256,s.currency_code,o.company_id,o.legal_entity_id,o.jurisdiction_code,o.obligation_type,o.period_start,o.period_end,o.filing_frequency,o.obligation_sha256 FROM '.TB_PREF.'hrm_pay_fil_001_source_sets s INNER JOIN '.TB_PREF.'hrm_pay_fil_001_obligations o ON o.filing_obligation_id=s.filing_obligation_id WHERE s.filing_source_set_id='.(int)$set_id.' LIMIT 2');
    if(!$s||(int)$s['company_id']!==$company||$s['jurisdiction_code']!=='VN'||$s['obligation_type']!=='personal-income-tax-withholding'||$s['filing_frequency']!=='quarterly'||$s['currency_code']!=='VND')return pay_fil_001_runtime_fail($error,'tt80_render_model_source_scope_invalid');
    $m=pay_fil_001_tt80_semantic_mapping_status($set_id,$me);if($m===false)return pay_fil_001_runtime_fail($error,$me?:'semantic_mapping_unpublished');if(!hash_equals((string)$m['mapping_contract_sha256'],(string)pay_fil_001_tt80_semantic_mapping_contract_sha256()))return pay_fil_001_runtime_fail($error,'tt80_render_model_mapping_contract_mismatch');
    $ind=json_decode((string)$m['indicator_values_json'],true);if(!is_array($ind)||!hash_equals((string)$m['indicator_values_sha256'],(string)pay_fil_001_hash($ind)))return pay_fil_001_runtime_fail($error,'tt80_render_model_mapping_payload_invalid');
    $review=pay_fil_001_one('SELECT filing_source_set_id,completeness_status,review_sha256,taxpayer_profile_sha256,person_fact_set_sha256 FROM '.TB_PREF.'hrm_pay_fil_001_tt80_semantic_reviews WHERE tt80_semantic_review_id='.(int)$m['tt80_semantic_review_id'].' LIMIT 2');if(!$review||(int)$review['filing_source_set_id']!==$set_id||(string)$review['completeness_status']!=='complete'||!hash_equals((string)$review['review_sha256'],(string)$m['review_sha256'])||!hash_equals((string)$review['taxpayer_profile_sha256'],(string)$m['taxpayer_profile_sha256'])||!hash_equals((string)$review['person_fact_set_sha256'],(string)$m['person_fact_set_sha256']))return pay_fil_001_runtime_fail($error,'tt80_render_model_review_binding_invalid');
    $p=pay_fil_001_one('SELECT * FROM '.TB_PREF.'hrm_pay_fil_001_tt80_taxpayer_profiles WHERE profile_sha256='.db_escape($m['taxpayer_profile_sha256']).' LIMIT 2');if(!$p||(int)$p['filing_obligation_id']!==(int)$s['filing_obligation_id']||!hash_equals((string)$p['profile_sha256'],(string)pay_fil_001_tt80_semantic_source_profile_hash_from_row($p)))return pay_fil_001_runtime_fail($error,'tt80_render_model_taxpayer_profile_invalid');
    $source=array('company_id'=>(int)$s['company_id'],'legal_entity_id'=>(int)$s['legal_entity_id'],'obligation_sha256'=>(string)$s['obligation_sha256'],'source_set_sha256'=>(string)$s['source_set_sha256'],'currency_code'=>(string)$s['currency_code'],'period_start'=>(string)$s['period_start'],'period_end'=>(string)$s['period_end']);$mapping=array('mapping_sha256'=>(string)$m['mapping_sha256'],'indicator_values'=>$ind);
    $model=pay_fil_001_tt80_build_render_model($source,$p,$mapping,$context,$error);if($model===false)return false;return array('model'=>$model,'model_sha256'=>pay_fil_001_hash($model),'mapping_sha256'=>(string)$m['mapping_sha256'],'review_sha256'=>(string)$m['review_sha256'],'activation_contract_sha256'=>pay_fil_001_tt80_render_model_activation_contract_sha256());
}

/** Local-development XML generation only. No persistence, signing, credentials, network, or submission authority. */
function pay_fil_001_generate_tt80_local_validation_xml($filing_source_set_id,array $context,&$error=null)
{
    $built=pay_fil_001_tt80_construct_render_model($filing_source_set_id,$context,$error);if($built===false)return false;$render=pay_fil_001_render_tt80_xml_candidate($built['model'],$error);if($render===false)return false;if(!hash_equals((string)$built['mapping_sha256'],(string)$built['model']['source_binding']['semantic_mapping_sha256']))return pay_fil_001_runtime_fail($error,'tt80_generation_mapping_binding_mismatch');
    $render['model_sha256']=$built['model_sha256'];$render['semantic_mapping_sha256']=$built['mapping_sha256'];$render['semantic_review_sha256']=$built['review_sha256'];$render['activation_contract_sha256']=$built['activation_contract_sha256'];$render['generation_authority']='LOCAL_DEVELOPMENT_VALIDATOR_ONLY';$render['development_runtime_generation_allowed']=true;$render['artifact_publication_allowed']=false;$render['production_generation_allowed']=false;$render['production_signing_allowed']=false;$render['production_credentials_allowed']=false;$render['production_transmission_allowed']=false;$render['network_authority_allowed']=false;$render['portal_submission_allowed']=false;$render['historical_mutation_allowed']=false;return $render;
}

?>
