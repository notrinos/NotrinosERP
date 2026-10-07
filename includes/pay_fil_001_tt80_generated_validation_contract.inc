<?php
/** PAY-FIL-001 generated TT80 payload validation contract. Pure/hash-only helpers; no production filing authority. */
include_once(dirname(__FILE__).'/pay_fil_001_tt80_render_model_contract.inc');
if (!defined('PAY_FIL_001_TT80_GENERATED_VALIDATION_CONTRACT_VERSION')) define('PAY_FIL_001_TT80_GENERATED_VALIDATION_CONTRACT_VERSION','1.0');

/** Return the exact authority ceiling for generated-payload development validation. */
function pay_fil_001_tt80_generated_validation_profile()
{
    return array(
        'contract_version'=>PAY_FIL_001_TT80_GENERATED_VALIDATION_CONTRACT_VERSION,
        'form'=>'05/KK-TNCN',
        'form_id'=>'05_KK_TNCN_TT80',
        'declaration_code'=>'864',
        'schema_version'=>'2.9.3',
        'schema_resource'=>'05_KK_TNCN_TT80_293.xsd',
        'required_generation_authority'=>'LOCAL_DEVELOPMENT_VALIDATOR_ONLY',
        'development_gate'=>'OFFICIAL_LOCAL_VALIDATOR_COMPATIBILITY',
        'evidence_mode'=>'HASH_BOUND_GENERATED_PAYLOAD_OBSERVATION',
        'raw_payload_persistence_allowed'=>false,
        'artifact_publication_allowed'=>false,
        'official_schema_claimed'=>false,
        'production_acceptance_claimed'=>false,
        'production_generation_allowed'=>false,
        'production_signing_allowed'=>false,
        'production_credentials_allowed'=>false,
        'production_transmission_allowed'=>false,
        'network_authority_allowed'=>false,
        'portal_submission_allowed'=>false,
        'historical_mutation_allowed'=>false
    );
}
function pay_fil_001_tt80_generated_validation_contract_sha256(){return pay_fil_001_hash(pay_fil_001_tt80_generated_validation_profile());}

/** Validate one in-memory source-bound generation result without persisting the payload. */
function pay_fil_001_tt80_validate_generated_candidate(array $generated,&$error=null)
{
    $error=null;
    foreach(array('payload','payload_sha256','byte_length','renderer_id','renderer_version','schema_id','schema_version','media_type','model_sha256','semantic_mapping_sha256','semantic_review_sha256','activation_contract_sha256','generation_authority','development_runtime_generation_allowed','artifact_publication_allowed','production_generation_allowed','production_signing_allowed','production_credentials_allowed','production_transmission_allowed','network_authority_allowed','portal_submission_allowed','historical_mutation_allowed') as $k)
        if(!array_key_exists($k,$generated))return pay_fil_001_fail($error,'tt80_generated_candidate_incomplete');
    $profile=pay_fil_001_tt80_generated_validation_profile();
    if(!is_string($generated['payload'])||$generated['payload']===''||strlen($generated['payload'])!==(int)$generated['byte_length']
        ||!pay_fil_001_sha($generated['payload_sha256'])||!hash_equals(hash('sha256',$generated['payload']),(string)$generated['payload_sha256'])
        ||!pay_fil_001_sha($generated['model_sha256'])||!pay_fil_001_sha($generated['semantic_mapping_sha256'])||!pay_fil_001_sha($generated['semantic_review_sha256'])||!pay_fil_001_sha($generated['activation_contract_sha256']))
        return pay_fil_001_fail($error,'tt80_generated_candidate_binding_invalid');
    if($generated['renderer_id']!=='pay-fil-001.vn.tt80.05-kk-tncn-xml'||$generated['renderer_version']!=='1.0'||$generated['schema_id']!==$profile['form_id']||$generated['schema_version']!==$profile['schema_version']||$generated['media_type']!=='application/xml')
        return pay_fil_001_fail($error,'tt80_generated_candidate_identity_invalid');
    if($generated['generation_authority']!==$profile['required_generation_authority']||$generated['development_runtime_generation_allowed']!==true)
        return pay_fil_001_fail($error,'tt80_generated_candidate_authority_invalid');
    foreach(array('artifact_publication_allowed','production_generation_allowed','production_signing_allowed','production_credentials_allowed','production_transmission_allowed','network_authority_allowed','portal_submission_allowed','historical_mutation_allowed') as $k)
        if($generated[$k]!==false)return pay_fil_001_fail($error,'tt80_generated_candidate_authority_escalation');
    return $generated;
}

/** Produce a minimized hash-only receipt from a generated payload and a local validator observation. */
function pay_fil_001_tt80_generated_validation_receipt(array $generated,array $observation,&$error=null)
{
    $g=pay_fil_001_tt80_validate_generated_candidate($generated,$error);if($g===false)return false;
    $required=array('schema_resource','schema_resource_sha256','repository_xsd_validation','local_validator','local_validator_version','local_validator_result','observed_payload_sha256');
    foreach($required as $k)if(!array_key_exists($k,$observation))return pay_fil_001_fail($error,'tt80_generated_validation_observation_incomplete');
    $profile=pay_fil_001_tt80_generated_validation_profile();
    if($observation['schema_resource']!==$profile['schema_resource']||!pay_fil_001_sha($observation['schema_resource_sha256'])||$observation['repository_xsd_validation']!=='PASS'
        ||!is_string($observation['local_validator'])||trim($observation['local_validator'])===''||!is_string($observation['local_validator_version'])||trim($observation['local_validator_version'])===''||$observation['local_validator_result']!=='PASS'
        ||!pay_fil_001_sha($observation['observed_payload_sha256'])||!hash_equals((string)$g['payload_sha256'],(string)$observation['observed_payload_sha256']))
        return pay_fil_001_fail($error,'tt80_generated_validation_observation_invalid');
    $receipt=array(
        'contract_sha256'=>pay_fil_001_tt80_generated_validation_contract_sha256(),
        'payload_sha256'=>$g['payload_sha256'],
        'byte_length'=>(int)$g['byte_length'],
        'model_sha256'=>$g['model_sha256'],
        'semantic_mapping_sha256'=>$g['semantic_mapping_sha256'],
        'semantic_review_sha256'=>$g['semantic_review_sha256'],
        'activation_contract_sha256'=>$g['activation_contract_sha256'],
        'renderer_id'=>$g['renderer_id'],'renderer_version'=>$g['renderer_version'],'schema_id'=>$g['schema_id'],'schema_version'=>$g['schema_version'],
        'schema_resource'=>$observation['schema_resource'],'schema_resource_sha256'=>$observation['schema_resource_sha256'],
        'repository_xsd_validation'=>'PASS','local_validator'=>$observation['local_validator'],'local_validator_version'=>$observation['local_validator_version'],'local_validator_result'=>'PASS',
        'development_gate'=>$profile['development_gate'],'evidence_mode'=>$profile['evidence_mode'],
        'raw_payload_persisted'=>false,'artifact_publication_allowed'=>false,'production_acceptance_claimed'=>false,'production_transmission_allowed'=>false,'portal_submission_allowed'=>false
    );
    $receipt['receipt_sha256']=pay_fil_001_hash($receipt);
    return $receipt;
}
?>
