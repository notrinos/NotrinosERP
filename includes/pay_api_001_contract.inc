<?php
/** PAY-API-001 bounded payroll-integration authority contract. */
if (!defined('PAY_API_001_CONTRACT_VERSION')) define('PAY_API_001_CONTRACT_VERSION', '1.0.873');
if (!defined('PAY_API_001_PUBLIC_READONLY_ROUTE_DB_VERSION')) define('PAY_API_001_PUBLIC_READONLY_ROUTE_DB_VERSION', '1.0.873');

/** Return whether the bounded public read-only route is allowed at one exact database version. */
function pay_api_001_public_readonly_route_allowed_for_database_version($database_version)
{
    if (!is_string($database_version) || preg_match('/^\d+\.\d+\.\d+$/D', $database_version) !== 1) return false;
    return version_compare($database_version, PAY_API_001_PUBLIC_READONLY_ROUTE_DB_VERSION, '>=');
}

/** Return the authority contract as it applies to the supplied database version. */
function pay_api_001_authority_contract($database_version=null)
{
    $route_allowed = $database_version === null
        ? pay_api_001_public_readonly_route_allowed_for_database_version(PAY_API_001_CONTRACT_VERSION)
        : pay_api_001_public_readonly_route_allowed_for_database_version((string)$database_version);
    return array(
        'versioned_resource_contract'=>true,
        'deny_by_default'=>true,
        'machine_principal_required'=>true,
        'company_scope_required'=>true,
        'object_scope_required_when_resource_declares'=>true,
        'explicit_action_scope_required'=>true,
        'immutable_resource_registry'=>true,
        'immutable_principal_scope_custody'=>true,
        'authentication_required'=>true,
        'authorization_required'=>true,
        'audit_required'=>true,
        'idempotency_required_for_commands'=>true,
        'pagination_required_for_collections'=>true,
        'rate_limit_required_before_external_route'=>true,
        'credential_verifier_hash_custody'=>true,
        'credential_revocation_custody'=>true,
        'authentication_decision_audit'=>true,
        'read_only_request_gate'=>true,
        'tls_required_for_external_route'=>true,
        'provision_request_custody'=>true,
        'independent_provision_review_custody'=>true,
        'independent_provision_approval_custody'=>true,
        'provisioning_sod_gate'=>true,
        'provision_execution_receipt_custody'=>true,
        'provisioning_execution_writer'=>true,
        'credential_provisioning_writer'=>true,
        'rate_limit_policy_custody'=>true,
        'rate_limit_decision_custody'=>true,
        'rate_limit_decision_gate'=>true,
        'atomic_rate_limit_enforcement'=>true,
        'read_only_adapter_contract'=>true,
        'machine_credential_authentication_runtime'=>true,
        'machine_read_authority_resolution'=>true,
        'read_request_audit_custody'=>true,
        'http_readonly_precontroller_contract'=>true,
        'public_readonly_http_route_contract'=>true,
        'public_route_database_version_gate'=>true,
        'query_string_credentials_allowed'=>false,
        'browser_session_as_api_credential'=>false,
        'cleartext_secret_storage'=>false,
        'credential_or_secret_custody'=>false,
        'credential_provisioning_writer_allowed'=>true,
        'provisioning_execution_writer_allowed'=>true,
        'external_http_route'=>$route_allowed,
        'business_data_dispatcher_allowed'=>false,
        'webhook_delivery'=>false,
        'webhook_receive'=>false,
        'payroll_command_authority'=>false,
        'payment_authority'=>false,
        'gl_authority'=>false,
        'statutory_filing_authority'=>false,
        'network_authority'=>false,
        'cross_company_authority'=>false,
        'historical_payroll_or_accounting_mutation'=>false,
        'generic_extension_dispatch'=>false,
    );
}
?>
