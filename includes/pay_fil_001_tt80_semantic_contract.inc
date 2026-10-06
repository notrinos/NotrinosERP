<?php
/** PAY-FIL-001 TT80 statutory semantic contract and explicit source-gap rejection. Pure helpers only. */
include_once(dirname(__FILE__).'/pay_fil_001_contract.inc');
if (!defined('PAY_FIL_001_TT80_SEMANTIC_CONTRACT_VERSION')) define('PAY_FIL_001_TT80_SEMANTIC_CONTRACT_VERSION', '1.0');

/** Exact TT80 indicator meanings required before any payroll-derived filing model may be generated. */
function pay_fil_001_tt80_semantic_indicators()
{
    return array(
        'ct16'=>array('unit'=>'person','meaning'=>'workers_paid_salary_or_wages_in_period','equation'=>null,'facts'=>array('paid_worker_identity')),
        'ct17'=>array('unit'=>'person','meaning'=>'resident_workers_with_employment_contract_at_least_three_months','equation'=>null,'facts'=>array('paid_worker_identity','tax_residency','employment_contract_duration')),
        'ct18'=>array('unit'=>'person','meaning'=>'persons_with_personal_income_tax_withheld','equation'=>'ct19+ct20','facts'=>array('withheld_person_identity','tax_residency')),
        'ct19'=>array('unit'=>'person','meaning'=>'resident_persons_with_personal_income_tax_withheld','equation'=>null,'facts'=>array('withheld_person_identity','tax_residency')),
        'ct20'=>array('unit'=>'person','meaning'=>'nonresident_persons_with_personal_income_tax_withheld','equation'=>null,'facts'=>array('withheld_person_identity','tax_residency')),
        'ct21'=>array('unit'=>'VND','meaning'=>'taxable_income_paid_to_persons','equation'=>'ct22+ct23','facts'=>array('filing_taxable_income','tax_residency')),
        'ct22'=>array('unit'=>'VND','meaning'=>'taxable_income_paid_to_resident_persons','equation'=>null,'facts'=>array('filing_taxable_income','tax_residency')),
        'ct23'=>array('unit'=>'VND','meaning'=>'taxable_income_paid_to_nonresident_persons','equation'=>null,'facts'=>array('filing_taxable_income','tax_residency')),
        'ct24'=>array('unit'=>'VND','meaning'=>'taxable_foreign_insurer_life_or_other_nonmandatory_insurance_premiums_for_workers','equation'=>null,'facts'=>array('foreign_insurer_nonmandatory_insurance_taxable_income')),
        'ct25'=>array('unit'=>'VND','meaning'=>'taxable_income_exempt_under_petroleum_contract','equation'=>null,'facts'=>array('petroleum_contract_exempt_taxable_income')),
        'ct25_1'=>array('unit'=>'VND','meaning'=>'personal_income_tax_exempt_income_under_resolution','equation'=>null,'facts'=>array('resolution_exempt_income')),
        'ct26'=>array('unit'=>'VND','meaning'=>'taxable_income_paid_to_persons_subject_to_withholding','equation'=>'ct27+ct28','facts'=>array('withholding_subject_taxable_income','tax_residency')),
        'ct27'=>array('unit'=>'VND','meaning'=>'taxable_income_paid_to_resident_persons_subject_to_withholding','equation'=>null,'facts'=>array('withholding_subject_taxable_income','tax_residency')),
        'ct28'=>array('unit'=>'VND','meaning'=>'taxable_income_paid_to_nonresident_persons_subject_to_withholding','equation'=>null,'facts'=>array('withholding_subject_taxable_income','tax_residency')),
        'ct29'=>array('unit'=>'VND','meaning'=>'personal_income_tax_withheld','equation'=>'ct30+ct31','facts'=>array('personal_income_tax_withheld_amount','tax_residency')),
        'ct30'=>array('unit'=>'VND','meaning'=>'personal_income_tax_withheld_from_resident_persons','equation'=>null,'facts'=>array('personal_income_tax_withheld_amount','tax_residency')),
        'ct31'=>array('unit'=>'VND','meaning'=>'personal_income_tax_withheld_from_nonresident_persons','equation'=>null,'facts'=>array('personal_income_tax_withheld_amount','tax_residency')),
        'ct32'=>array('unit'=>'VND','meaning'=>'personal_income_tax_withheld_on_foreign_insurer_life_or_other_nonmandatory_insurance_premiums','equation'=>null,'facts'=>array('foreign_insurer_nonmandatory_insurance_withheld_tax'))
    );
}

/** Taxpayer/legal-entity fields that must be source-bound rather than inferred from payroll hashes. */
function pay_fil_001_tt80_semantic_taxpayer_facts()
{
    return array('taxpayer_tax_code','taxpayer_name','taxpayer_address','taxpayer_ward_code','taxpayer_ward_name','taxpayer_province_code','taxpayer_province_name','tax_office_code','tax_office_name');
}

/** Facts absent from the accepted PAY-FIL/PAY-CORE custody contract at 1.0.1027. */
function pay_fil_001_tt80_semantic_source_gaps()
{
    return array(
        'taxpayer_legal_entity_profile_not_bound_to_filing_source_custody',
        'paid_worker_identity_not_bound_to_filing_source_links',
        'tax_residency_not_bound_to_filing_source_links',
        'employment_contract_duration_not_bound_to_filing_source_links',
        'pit_withheld_person_identity_not_bound_to_filing_source_links',
        'filing_taxable_income_not_typed_in_pay_core_result_contract',
        'withholding_subject_taxable_income_not_typed_in_pay_core_result_contract',
        'pit_withheld_amount_not_typed_by_filing_semantic_in_pay_core_result_contract',
        'foreign_insurer_nonmandatory_insurance_income_not_typed',
        'foreign_insurer_nonmandatory_insurance_withheld_tax_not_typed',
        'petroleum_contract_exempt_income_not_typed',
        'resolution_exempt_income_not_typed'
    );
}

/** Publication decision for the exact current architecture. */
function pay_fil_001_tt80_semantic_contract()
{
    return array(
        'contract_version'=>PAY_FIL_001_TT80_SEMANTIC_CONTRACT_VERSION,
        'form'=>'05/KK-TNCN',
        'form_id'=>'05_KK_TNCN_TT80',
        'schema_version'=>'2.9.3',
        'indicator_contract'=>pay_fil_001_tt80_semantic_indicators(),
        'taxpayer_required_facts'=>pay_fil_001_tt80_semantic_taxpayer_facts(),
        'source_gaps'=>pay_fil_001_tt80_semantic_source_gaps(),
        'source_mapping_status'=>'ARCHITECTURE_REJECTED_SOURCE_GAPS',
        'runtime_generation_allowed'=>false,
        'artifact_publication_allowed'=>false,
        'production_signing_allowed'=>false,
        'production_transmission_allowed'=>false,
        'network_authority_allowed'=>false,
        'historical_mutation_allowed'=>false
    );
}

/** Stable content address for the semantic contract. */
function pay_fil_001_tt80_semantic_contract_sha256(){return pay_fil_001_hash(pay_fil_001_tt80_semantic_contract());}
?>
