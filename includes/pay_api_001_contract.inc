<?php
/** PAY-API-001 bounded payroll-integration authority contract. */
if (!defined('PAY_API_001_CONTRACT_VERSION')) define('PAY_API_001_CONTRACT_VERSION', '1.0.857');
function pay_api_001_authority_contract()
{
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
        'credential_or_secret_custody'=>false,
        'external_http_route'=>false,
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
