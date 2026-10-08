<?php
/** PAY-CON-001 provider-neutral external payroll connector foundation contract. */

if (!defined('PAY_CON_001_CONTRACT_VERSION')) define('PAY_CON_001_CONTRACT_VERSION', '1.0.0');
if (!defined('PAY_CON_001_SOURCE_DATABASE_VERSION')) define('PAY_CON_001_SOURCE_DATABASE_VERSION', '1.0.1063');
if (!defined('PAY_CON_001_LEGACY_PAYROLL_CONTRACT_VERSION')) define('PAY_CON_001_LEGACY_PAYROLL_CONTRACT_VERSION', '1.0.543');

/**
 * First PAY-CON batch is intentionally non-delivering. It establishes only
 * dependency, authority and immutable mapping custody boundaries. A named
 * provider, legal/vendor review and transport design are separate later work.
 */
function pay_con_001_authority_contract()
{
    return array(
        'pay_api_closed_required' => true,
        'pay_rec_closed_required' => true,
        'named_provider_due_diligence_required_before_writer' => true,
        'data_residency_review_required_before_writer' => true,
        'delivery_model_disclosure_required' => true,
        'staged_shadow_exchange_required' => true,
        'schema_validation_required' => true,
        'mapping_version_pinning_required' => true,
        'idempotency_required' => true,
        'reconciliation_required' => true,
        'scoped_secret_reference_required_before_transport' => true,
        'immutable_provider_profile_custody' => true,
        'immutable_mapping_version_custody' => true,

        'provider_profile_writer_allowed' => false,
        'mapping_version_writer_allowed' => false,
        'shadow_export_runtime_allowed' => false,
        'shadow_import_runtime_allowed' => false,
        'external_http_route_allowed' => false,
        'provider_adapter_runtime_allowed' => false,
        'outbound_network_authority_allowed' => false,
        'inbound_network_authority_allowed' => false,
        'cleartext_secret_storage_allowed' => false,
        'connector_secret_custody_allowed' => false,
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
        'automatic_role_grant_allowed' => false,
        'production_provider_exchange_allowed' => false
    );
}

function pay_con_001_delivery_models()
{
    return array('EXTERNAL_PROVIDER', 'PARTNER_OPERATED', 'MANAGED_SERVICE');
}

function pay_con_001_mapping_payload_classes()
{
    return array('HR_TIME', 'PAYROLL_RESULT', 'BALANCE', 'PAYMENT_EVIDENCE', 'FILING_EVIDENCE');
}

function pay_con_001_mapping_directions()
{
    return array('OUTBOUND', 'INBOUND');
}
?>
