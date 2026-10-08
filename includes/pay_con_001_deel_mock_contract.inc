<?php
/**
 * PAY-CON-001 documentation-based Deel adapter DEVELOPMENT contract.
 *
 * This is not the external-provider admission contract. It does not change
 * the immutable 1.0.1066 provider admission/profile/mapping custody, and it
 * never grants runtime, credential, network or payroll result authority.
 */
if (!defined('PAY_CON_001_DEEL_MOCK_CONTRACT_VERSION')) {
    define('PAY_CON_001_DEEL_MOCK_CONTRACT_VERSION', '1.0.0');
}

/** Return the independent development policy; no live authority is granted. */
function pay_con_001_deel_mock_policy()
{
    return array(
        'provider_code' => 'DEEL',
        'acceptance_scope' => 'DEVELOPMENT_DOCUMENTED_MOCK_ONLY',
        'source_database_version' => '1.0.1066',
        'legacy_payroll_custody' => '1.0.544',
        'deel_api_version' => '2026-01-01',
        'documented_auth_scheme' => 'BEARER_TOKEN_REQUIRED_FOR_REAL_CALLS',
        'mock_credentials_required' => false,
        'mock_provider_contract_required' => false,
        'mock_vendor_private_evidence_required' => false,
        'fixture_only' => true,
        'synthetic_shadow_planning_allowed' => true,
        'mock_reconciliation_only' => true,
        'offline_no_network' => true,
        'real_provider_admitted' => false,
        'provider_admission_writer_allowed' => false,
        'profile_mapping_writer_allowed' => false,
        'secret_storage_allowed' => false,
        'production_activation_allowed' => false,
        'external_transport_allowed' => false,
        'inbound_webhook_listener_allowed' => false,
        'payroll_execution_allowed' => false,
        'payroll_result_authority_allowed' => false,
        'payroll_mutation_allowed' => false,
        'gl_posting_allowed' => false,
        'payment_execution_allowed' => false,
        'filing_submission_allowed' => false,
        'historical_mutation_allowed' => false
    );
}
/** Inventory official public documentation references, never admission evidence. */
function pay_con_001_deel_mock_sources()
{
    return array(
        'api_version' => 'https://developer.deel.com/api/api-versioning',
        'authentication' => 'https://developer.deel.com/api/authentication',
        'global_payroll' => 'https://developer.deel.com/api/global-payroll/introduction',
        'payroll_groups' => 'https://developer.deel.com/api/reference/endpoints/global-payroll/list-payroll-groups-v-2026-09-08',
        'g2n_csv' => 'https://developer.deel.com/api/stable/reference/endpoints/reports/get-gp-report-gross-to-net-csv',
        'rate_limits' => 'https://developer.deel.com/api/rate-limits',
        'adjustments' => 'https://developer.deel.com/api/global-payroll/adjustments',
        'idempotency' => 'https://developer.deel.com/api/best-practices',
        'webhook_security' => 'https://developer.deel.com/api/webhooks/quickstart'
    );
}
/** Describe the only permitted offline read request shapes. */
function pay_con_001_deel_mock_routes()
{
    return array(
        'payroll_groups' => array(
            'method' => 'GET',
            'path' => '/rest/payroll-groups',
            'required_parameter' => 'legalEntityId',
            'document' => 'payroll_groups',
            'read_only' => true
        ),
        'gross_to_net_csv' => array(
            'method' => 'GET',
            'path_template' => '/rest/gp/reports/{id}/gross_to_net/csv',
            'document' => 'g2n_csv',
            'read_only' => true
        )
    );
}
?>
