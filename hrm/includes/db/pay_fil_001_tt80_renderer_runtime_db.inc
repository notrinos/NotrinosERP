<?php
/** Read-only source-bound TT80 renderer readiness. Mapping publication does not activate runtime XML generation. */
/* Historical predecessor guard tokens retained for immutable source-contract verification: semantic_source_incomplete, ARCHITECTURE_REJECTED_SOURCE_GAPS. */
include_once(dirname(__FILE__).'/pay_fil_001_runtime_db.inc');
include_once(dirname(__FILE__).'/pay_fil_001_tt80_renderer_readiness_db.inc');
include_once(dirname(__FILE__).'/pay_fil_001_tt80_semantic_readiness_db.inc');
include_once(dirname(__FILE__).'/pay_fil_001_tt80_semantic_source_custody_db.inc');
include_once(dirname(__FILE__).'/pay_fil_001_tt80_semantic_source_admission_db.inc');
include_once(dirname(__FILE__).'/pay_fil_001_tt80_semantic_mapping_db.inc');

/**
 * Read the source-bound TT80 readiness state for one filing source set.
 *
 * @param int $filing_source_set_id PAY-FIL source-set identity.
 * @param string|null $error Receives a denial or database error code.
 * @return array|false Readiness diagnostics or false on access/scope failure.
 */
function pay_fil_001_tt80_renderer_readiness($filing_source_set_id, &$error=null)
{
    $error=null;if(!pay_fil_001_require_access(PAY_FIL_001_VIEW_PERMISSION,$error))return false;$company=pay_fil_001_company();if($company<0)return pay_fil_001_runtime_fail($error,'actor_or_company_missing');
    $sql='SELECT s.filing_source_set_id,s.source_set_sha256,s.currency_code,s.run_count,s.result_count,s.line_count,s.balance_count,s.liability_count,o.company_id,o.legal_entity_id,o.jurisdiction_code,o.obligation_type,o.period_start,o.period_end,o.filing_frequency,o.country_pack_id,o.country_pack_version,o.schema_id,o.schema_version,o.connector_id,o.connector_version,o.obligation_sha256 FROM '.TB_PREF.'hrm_pay_fil_001_source_sets s INNER JOIN '.TB_PREF.'hrm_pay_fil_001_obligations o ON o.filing_obligation_id=s.filing_obligation_id WHERE s.filing_source_set_id='.(int)$filing_source_set_id.' LIMIT 2';
    $s=pay_fil_001_one($sql);if(!$s||(int)$s['company_id']!==$company)return pay_fil_001_runtime_fail($error,'filing_source_set_missing_or_scope_denied');
    $blockers=array();if($s['jurisdiction_code']!=='VN')$blockers[]='jurisdiction_not_vn';if($s['obligation_type']!=='personal-income-tax-withholding')$blockers[]='obligation_type_not_tt80_withholding';if($s['currency_code']!=='VND')$blockers[]='currency_not_vnd';if($s['schema_id']!=='internal-reconciliation-only'||$s['schema_version']!=='1'||$s['connector_id']!=='none'||$s['connector_version']!=='0')$blockers[]='accepted_source_custody_contract_changed';if($s['filing_frequency']!=='quarterly')$blockers[]='filing_frequency_not_quarterly';
    $profile=pay_fil_001_tt80_renderer_profile();$semantic=pay_fil_001_tt80_semantic_contract();$custody=pay_fil_001_tt80_semantic_source_custody_contract();$admission=pay_fil_001_tt80_semantic_source_admission_contract();$mapping_contract=pay_fil_001_tt80_semantic_mapping_contract();$review_error=null;$review=pay_fil_001_tt80_semantic_source_review_status($filing_source_set_id,$review_error);$mapping_error=null;$mapping=$review===false?false:pay_fil_001_tt80_semantic_mapping_status($filing_source_set_id,$mapping_error);if($review===false)$blockers[]='semantic_source_admission_unreviewed';elseif($mapping===false)$blockers[]='semantic_mapping_unpublished';else $blockers[]='runtime_generation_not_enabled';
    return array('ready'=>count($blockers)===0,'blockers'=>$blockers,'company_id'=>$company,'legal_entity_id'=>(int)$s['legal_entity_id'],'obligation_sha256'=>(string)$s['obligation_sha256'],'source_set_sha256'=>(string)$s['source_set_sha256'],'currency_code'=>(string)$s['currency_code'],'period_start'=>(string)$s['period_start'],'period_end'=>(string)$s['period_end'],'renderer_id'=>$profile['renderer_id'],'renderer_version'=>$profile['renderer_version'],'form'=>$profile['form'],'schema_version'=>$profile['schema_version'],'semantic_mapping_status'=>$mapping===false?'UNPUBLISHED':'PUBLISHED_REVIEW_BOUND','semantic_contract_sha256'=>pay_fil_001_tt80_semantic_contract_sha256(),'semantic_source_custody_status'=>$custody['custody_status'],'semantic_source_custody_contract_sha256'=>pay_fil_001_tt80_semantic_source_custody_contract_sha256(),'semantic_source_admission_status'=>$admission['admission_status'],'semantic_source_admission_contract_sha256'=>pay_fil_001_tt80_semantic_source_admission_contract_sha256(),'semantic_source_review_status'=>$review===false?'unreviewed':'complete','semantic_source_review_sha256'=>$review===false?'':(string)$review['review_sha256'],'semantic_mapping_contract_sha256'=>pay_fil_001_tt80_semantic_mapping_contract_sha256(),'semantic_mapping_sha256'=>$mapping===false?'':(string)$mapping['mapping_sha256'],'source_gaps'=>$mapping===false?$semantic['source_gaps']:array(),'official_schema_claimed'=>false,'production_generation_allowed'=>false,'production_transmission_allowed'=>false,'submission_authorized'=>false);
}

/**
 * Attempt to render from an accepted source set, failing closed until separately reviewed runtime-generation activation.
 *
 * @param int $filing_source_set_id PAY-FIL source-set identity.
 * @param array $model Normalized candidate render model.
 * @param string|null $error Receives a blocker or validation error code.
 * @return false This entrypoint remains blocked even after semantic mapping publication.
 */
function pay_fil_001_render_tt80_from_source_set($filing_source_set_id, array $model, &$error=null)
{
    $r=pay_fil_001_tt80_renderer_readiness($filing_source_set_id,$error);if($r===false)return false;if(!$r['ready']){if(in_array('semantic_source_admission_unreviewed',$r['blockers'],true))return pay_fil_001_runtime_fail($error,'semantic_source_admission_unreviewed');if(in_array('semantic_mapping_unpublished',$r['blockers'],true))return pay_fil_001_runtime_fail($error,'semantic_mapping_unpublished');return pay_fil_001_runtime_fail($error,'runtime_generation_not_enabled');}
    $valid=pay_fil_001_validate_tt80_render_model($model,$error);if($valid===false)return false;
    if((int)$valid['source_binding']['company_id']!==(int)$r['company_id']||(int)$valid['source_binding']['legal_entity_id']!==(int)$r['legal_entity_id']||!hash_equals((string)$valid['source_binding']['obligation_sha256'],(string)$r['obligation_sha256'])||!hash_equals((string)$valid['source_binding']['source_set_sha256'],(string)$r['source_set_sha256'])||(string)$valid['source_binding']['currency_code']!==(string)$r['currency_code'])return pay_fil_001_runtime_fail($error,'tt80_source_binding_mismatch');
    return pay_fil_001_runtime_fail($error,'runtime_generation_not_enabled');
}
?>
