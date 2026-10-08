<?php
/** PAY-CON-001 named-provider evidence/admission contract. */

if (!defined('PAY_CON_001_ADMISSION_CONTRACT_VERSION')) define('PAY_CON_001_ADMISSION_CONTRACT_VERSION', '1.0.0');

function pay_con_001_admission_contract()
{
    return array(
        'admission_status' => 'EVIDENCE_CUSTODY_READY_NO_PROVIDER_ADMITTED',
        'named_provider_required_for_admission' => true,
        'vendor_due_diligence_required' => true,
        'legal_review_required' => true,
        'security_review_required' => true,
        'data_residency_review_required' => true,
        'delivery_model_disclosure_required' => true,
        'provider_contract_evidence_required' => true,
        'source_provenance_required' => true,
        'independent_reviewer_required' => true,
        'independent_approver_required' => true,
        'review_hash_binding_required' => true,
        'approved_decision_required_before_profile_writer' => true,
        'mapping_approval_required_before_exchange' => true,

        'provider_admission_review_writer_allowed' => false,
        'provider_profile_writer_allowed' => false,
        'mapping_version_writer_allowed' => false,
        'provider_adapter_runtime_allowed' => false,
        'external_http_route_allowed' => false,
        'connector_secret_custody_allowed' => false,
        'outbound_network_authority_allowed' => false,
        'inbound_network_authority_allowed' => false,
        'shadow_export_runtime_allowed' => false,
        'shadow_import_runtime_allowed' => false,
        'provider_payroll_execution_authority_allowed' => false,
        'payroll_result_authority_allowed' => false,
        'payroll_state_mutation_allowed' => false,
        'gl_posting_authority_allowed' => false,
        'payment_execution_authority_allowed' => false,
        'filing_submission_authority_allowed' => false,
        'approval_bypass_allowed' => false,
        'audit_bypass_allowed' => false,
        'cross_company_authority_allowed' => false,
        'historical_payroll_accounting_filing_mutation_allowed' => false,
        'production_provider_exchange_allowed' => false
    );
}

function pay_con_001_admission_decisions()
{
    return array('APPROVED', 'REJECTED');
}
?>
