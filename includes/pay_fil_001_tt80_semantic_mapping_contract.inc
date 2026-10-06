<?php
/** PAY-FIL-001 TT80 deterministic semantic mapping publication contract. Pure helpers only. */
include_once(dirname(__FILE__).'/pay_fil_001_tt80_semantic_source_admission_contract.inc');
if (!defined('PAY_FIL_001_TT80_SEMANTIC_MAPPING_CONTRACT_VERSION'))
    define('PAY_FIL_001_TT80_SEMANTIC_MAPPING_CONTRACT_VERSION', '1.0');

function pay_fil_001_tt80_semantic_mapping_indicator_rules()
{
    return array(
        'ct16'=>'count(paid_worker=1)',
        'ct17'=>'count(paid_worker=1 AND tax_residency=resident AND employment_contract_term_class=at_least_3_months)',
        'ct18'=>'count(withheld_person=1)',
        'ct19'=>'count(withheld_person=1 AND tax_residency=resident)',
        'ct20'=>'count(withheld_person=1 AND tax_residency=nonresident)',
        'ct21'=>'sum(filing_taxable_income_minor)',
        'ct22'=>'sum(filing_taxable_income_minor WHERE tax_residency=resident)',
        'ct23'=>'sum(filing_taxable_income_minor WHERE tax_residency=nonresident)',
        'ct24'=>'sum(foreign_insurer_nonmandatory_insurance_taxable_minor)',
        'ct25'=>'sum(petroleum_contract_exempt_taxable_minor)',
        'ct25_1'=>'sum(resolution_exempt_income_minor)',
        'ct26'=>'sum(withholding_subject_taxable_income_minor)',
        'ct27'=>'sum(withholding_subject_taxable_income_minor WHERE tax_residency=resident)',
        'ct28'=>'sum(withholding_subject_taxable_income_minor WHERE tax_residency=nonresident)',
        'ct29'=>'sum(personal_income_tax_withheld_minor)',
        'ct30'=>'sum(personal_income_tax_withheld_minor WHERE tax_residency=resident)',
        'ct31'=>'sum(personal_income_tax_withheld_minor WHERE tax_residency=nonresident)',
        'ct32'=>'sum(foreign_insurer_nonmandatory_insurance_withheld_minor)'
    );
}
function pay_fil_001_tt80_semantic_mapping_contract()
{
    return array(
        'contract_version'=>PAY_FIL_001_TT80_SEMANTIC_MAPPING_CONTRACT_VERSION,
        'jurisdiction_code'=>'VN','form'=>'05/KK-TNCN','form_id'=>'05_KK_TNCN_TT80','schema_version'=>'2.9.3',
        'publish_permission'=>PAY_FIL_001_AUTHORIZE_PERMISSION,
        'required_review_status'=>'complete',
        'required_review_binding'=>'one_exact_immutable_tt80_semantic_review',
        'publisher_separation'=>'publisher_distinct_from_source_makers_and_reviewer',
        'indicator_rules'=>pay_fil_001_tt80_semantic_mapping_indicator_rules(),
        'mapping_status'=>'GOVERNED_DETERMINISTIC_MAPPING_PUBLICATION_ENABLED',
        'mapping_publication_allowed'=>true,
        'automatic_inference_allowed'=>false,
        'runtime_generation_allowed'=>false,
        'artifact_publication_allowed'=>false,
        'production_signing_allowed'=>false,
        'production_credentials_allowed'=>false,
        'production_transmission_allowed'=>false,
        'network_authority_allowed'=>false,
        'historical_mutation_allowed'=>false
    );
}
function pay_fil_001_tt80_semantic_mapping_contract_sha256(){return pay_fil_001_hash(pay_fil_001_tt80_semantic_mapping_contract());}
?>
