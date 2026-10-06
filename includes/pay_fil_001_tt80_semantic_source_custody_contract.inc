<?php
/** PAY-FIL-001 TT80 additive semantic source-of-truth custody contract. Pure helpers only. */
include_once(dirname(__FILE__).'/pay_fil_001_tt80_semantic_contract.inc');
if (!defined('PAY_FIL_001_TT80_SEMANTIC_SOURCE_CUSTODY_CONTRACT_VERSION'))
    define('PAY_FIL_001_TT80_SEMANTIC_SOURCE_CUSTODY_CONTRACT_VERSION', '1.0');

/** Exact taxpayer facts accepted by the additive custody design; values must be explicit, never inferred from payroll hashes. */
function pay_fil_001_tt80_semantic_custody_taxpayer_fields()
{
    return array(
        'taxpayer_tax_code','taxpayer_name','taxpayer_address','taxpayer_ward_code','taxpayer_ward_name',
        'taxpayer_province_code','taxpayer_province_name','tax_office_code','tax_office_name'
    );
}

/** Exact person-level facts accepted by the additive custody design. */
function pay_fil_001_tt80_semantic_custody_person_fields()
{
    return array(
        'subject_key','paid_worker','withheld_person','tax_residency','employment_contract_term_class',
        'filing_taxable_income_minor','withholding_subject_taxable_income_minor','personal_income_tax_withheld_minor',
        'foreign_insurer_nonmandatory_insurance_taxable_minor','foreign_insurer_nonmandatory_insurance_withheld_minor',
        'petroleum_contract_exempt_taxable_minor','resolution_exempt_income_minor','currency_code'
    );
}

/** Source-of-truth custody design. This authorizes custody only, not statutory mapping publication. */
function pay_fil_001_tt80_semantic_source_custody_contract()
{
    return array(
        'contract_version'=>PAY_FIL_001_TT80_SEMANTIC_SOURCE_CUSTODY_CONTRACT_VERSION,
        'jurisdiction_code'=>'VN',
        'form'=>'05/KK-TNCN',
        'form_id'=>'05_KK_TNCN_TT80',
        'schema_version'=>'2.9.3',
        'taxpayer_fields'=>pay_fil_001_tt80_semantic_custody_taxpayer_fields(),
        'person_fields'=>pay_fil_001_tt80_semantic_custody_person_fields(),
        'tax_residency_values'=>array('resident','nonresident','unknown'),
        'employment_contract_term_values'=>array('at_least_3_months','under_3_months','not_applicable','unknown'),
        'required_provenance'=>array(
            'existing_filing_obligation_or_source_set',
            'explicit_source_evidence_sha256',
            'server_actor_identity',
            'server_capture_timestamp',
            'immutable_content_hash'
        ),
        'person_fact_binding'=>'one_or_more_existing_pay_fil_source_links',
        'custody_status'=>'CUSTODY_DESIGN_ACCEPTED_ADMISSION_REVIEW_NOT_ENABLED',
        'semantic_mapping_status'=>'ARCHITECTURE_REJECTED_SOURCE_GAPS',
        'semantic_mapping_publication_allowed'=>false,
        'runtime_generation_allowed'=>false,
        'artifact_publication_allowed'=>false,
        'production_signing_allowed'=>false,
        'production_transmission_allowed'=>false,
        'network_authority_allowed'=>false,
        'historical_mutation_allowed'=>false,
        'automatic_inference_allowed'=>false
    );
}
function pay_fil_001_tt80_semantic_source_custody_contract_sha256()
{
    return pay_fil_001_hash(pay_fil_001_tt80_semantic_source_custody_contract());
}
?>