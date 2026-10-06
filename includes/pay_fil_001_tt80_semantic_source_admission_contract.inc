<?php
/** PAY-FIL-001 TT80 governed semantic source admission/review contract. Pure helpers only. */
include_once(dirname(__FILE__).'/pay_fil_001_tt80_semantic_source_custody_contract.inc');
if (!defined('PAY_FIL_001_TT80_SEMANTIC_SOURCE_ADMISSION_CONTRACT_VERSION'))
    define('PAY_FIL_001_TT80_SEMANTIC_SOURCE_ADMISSION_CONTRACT_VERSION', '1.0');

function pay_fil_001_tt80_semantic_source_admission_contract()
{
    return array(
        'contract_version'=>PAY_FIL_001_TT80_SEMANTIC_SOURCE_ADMISSION_CONTRACT_VERSION,
        'jurisdiction_code'=>'VN',
        'form'=>'05/KK-TNCN',
        'form_id'=>'05_KK_TNCN_TT80',
        'schema_version'=>'2.9.3',
        'prepare_permission'=>PAY_FIL_001_PREPARE_PERMISSION,
        'review_permission'=>PAY_FIL_001_REVIEW_PERMISSION,
        'maker_checker_required'=>true,
        'admission_entities'=>array('taxpayer_profile','person_fact','person_fact_link'),
        'review_basis'=>array(
            'exact_taxpayer_profile_sha256',
            'exact_person_fact_set_sha256',
            'exact_person_fact_link_set_sha256',
            'explicit_subject_roster_evidence_sha256',
            'expected_subject_count',
            'expected_subject_set_sha256',
            'source_evidence_set_sha256',
            'independent_reviewer_identity'
        ),
        'completeness_status_values'=>array('complete','incomplete'),
        'admission_status'=>'GOVERNED_ADMISSION_REVIEW_ENABLED_MAPPING_UNPUBLISHED',
        'semantic_mapping_status'=>'ARCHITECTURE_REJECTED_SOURCE_GAPS',
        'semantic_mapping_publication_allowed'=>false,
        'runtime_generation_allowed'=>false,
        'artifact_publication_allowed'=>false,
        'production_signing_allowed'=>false,
        'production_credentials_allowed'=>false,
        'production_transmission_allowed'=>false,
        'network_authority_allowed'=>false,
        'historical_mutation_allowed'=>false,
        'automatic_inference_allowed'=>false,
        'automatic_admission_allowed'=>false,
        'automatic_review_allowed'=>false
    );
}
function pay_fil_001_tt80_semantic_source_admission_contract_sha256()
{
    return pay_fil_001_hash(pay_fil_001_tt80_semantic_source_admission_contract());
}
?>
