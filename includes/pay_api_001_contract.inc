<?php
/** PAY-API-001 bounded payroll-integration authority contract. */
if (!defined('PAY_API_001_CONTRACT_VERSION')) define('PAY_API_001_CONTRACT_VERSION', '1.0.888');
if (!defined('PAY_API_001_PUBLIC_READONLY_ROUTE_DB_VERSION')) define('PAY_API_001_PUBLIC_READONLY_ROUTE_DB_VERSION', '1.0.873');
if (!defined('PAY_API_001_BUSINESS_DATA_DISPATCH_DB_VERSION')) define('PAY_API_001_BUSINESS_DATA_DISPATCH_DB_VERSION', '1.0.876');
if (!defined('PAY_API_001_WEBHOOK_OUTBOX_CUSTODY_DB_VERSION')) define('PAY_API_001_WEBHOOK_OUTBOX_CUSTODY_DB_VERSION', '1.0.878');
if (!defined('PAY_API_001_WEBHOOK_DELIVERY_AUDIT_CUSTODY_DB_VERSION')) define('PAY_API_001_WEBHOOK_DELIVERY_AUDIT_CUSTODY_DB_VERSION', '1.0.879');
if (!defined('PAY_API_001_WEBHOOK_TARGET_LIFECYCLE_CUSTODY_DB_VERSION')) define('PAY_API_001_WEBHOOK_TARGET_LIFECYCLE_CUSTODY_DB_VERSION', '1.0.880');
if (!defined('PAY_API_001_WEBHOOK_SECRET_REFERENCE_CUSTODY_DB_VERSION')) define('PAY_API_001_WEBHOOK_SECRET_REFERENCE_CUSTODY_DB_VERSION', '1.0.881');
if (!defined('PAY_API_001_WEBHOOK_DELIVERY_POLICY_LEASE_CUSTODY_DB_VERSION')) define('PAY_API_001_WEBHOOK_DELIVERY_POLICY_LEASE_CUSTODY_DB_VERSION', '1.0.882');
if (!defined('PAY_API_001_WEBHOOK_EGRESS_TLS_POLICY_CUSTODY_DB_VERSION')) define('PAY_API_001_WEBHOOK_EGRESS_TLS_POLICY_CUSTODY_DB_VERSION', '1.0.883');
if (!defined('PAY_API_001_WEBHOOK_SIGNING_PROFILE_CUSTODY_DB_VERSION')) define('PAY_API_001_WEBHOOK_SIGNING_PROFILE_CUSTODY_DB_VERSION', '1.0.884');
if (!defined('PAY_API_001_WEBHOOK_RESPONSE_REPLAY_REPROCESS_CUSTODY_DB_VERSION')) define('PAY_API_001_WEBHOOK_RESPONSE_REPLAY_REPROCESS_CUSTODY_DB_VERSION', '1.0.885');
if (!defined('PAY_API_001_WEBHOOK_CONFIGURATION_WRITER_DB_VERSION')) define('PAY_API_001_WEBHOOK_CONFIGURATION_WRITER_DB_VERSION', '1.0.886');
if (!defined('PAY_API_001_WEBHOOK_ENQUEUE_WRITER_DB_VERSION')) define('PAY_API_001_WEBHOOK_ENQUEUE_WRITER_DB_VERSION', '1.0.887');
if (!defined('PAY_API_001_WEBHOOK_CLAIM_WRITER_DB_VERSION')) define('PAY_API_001_WEBHOOK_CLAIM_WRITER_DB_VERSION', '1.0.888');

/** Return whether the bounded public read-only route is allowed at one exact database version. */
function pay_api_001_public_readonly_route_allowed_for_database_version($database_version)
{
    if (!is_string($database_version) || preg_match('/^\d+\.\d+\.\d+$/D', $database_version) !== 1) return false;
    return version_compare($database_version, PAY_API_001_PUBLIC_READONLY_ROUTE_DB_VERSION, '>=');
}

/** Return whether one bounded business-data dispatcher is allowed at the database version. */
function pay_api_001_business_data_dispatch_allowed_for_database_version($database_version)
{
    if (!is_string($database_version) || preg_match('/^\d+\.\d+\.\d+$/D', $database_version) !== 1) return false;
    return version_compare($database_version, PAY_API_001_BUSINESS_DATA_DISPATCH_DB_VERSION, '>=');
}

/** Return whether immutable outbound webhook target/event/outbox custody must exist. */
function pay_api_001_webhook_outbox_custody_required_for_database_version($database_version)
{
    if (!is_string($database_version) || preg_match('/^\d+\.\d+\.\d+$/D', $database_version) !== 1) return false;
    return version_compare($database_version, PAY_API_001_WEBHOOK_OUTBOX_CUSTODY_DB_VERSION, '>=');
}

/** Return whether immutable delivery-attempt and dead-letter audit custody must exist. */
function pay_api_001_webhook_delivery_audit_custody_required_for_database_version($database_version)
{
    if (!is_string($database_version) || preg_match('/^\d+\.\d+\.\d+$/D', $database_version) !== 1) return false;
    return version_compare($database_version, PAY_API_001_WEBHOOK_DELIVERY_AUDIT_CUSTODY_DB_VERSION, '>=');
}

/** Return whether immutable webhook target lifecycle/supersession custody must exist. */
function pay_api_001_webhook_target_lifecycle_custody_required_for_database_version($database_version)
{
    if (!is_string($database_version) || preg_match('/^\d+\.\d+\.\d+$/D', $database_version) !== 1) return false;
    return version_compare($database_version, PAY_API_001_WEBHOOK_TARGET_LIFECYCLE_CUSTODY_DB_VERSION, '>=');
}

/** Return whether opaque secret-reference and rotation custody must exist. */
function pay_api_001_webhook_secret_reference_custody_required_for_database_version($database_version)
{
    if (!is_string($database_version) || preg_match('/^\d+\.\d+\.\d+$/D', $database_version) !== 1) return false;
    return version_compare($database_version, PAY_API_001_WEBHOOK_SECRET_REFERENCE_CUSTODY_DB_VERSION, '>=');
}

/** Return whether immutable delivery-policy and claim/lease custody must exist. */
function pay_api_001_webhook_delivery_policy_lease_custody_required_for_database_version($database_version)
{
    if (!is_string($database_version) || preg_match('/^\d+\.\d+\.\d+$/D', $database_version) !== 1) return false;
    return version_compare($database_version, PAY_API_001_WEBHOOK_DELIVERY_POLICY_LEASE_CUSTODY_DB_VERSION, '>=');
}

/** Return whether immutable outbound egress/TLS policy custody must exist. */
function pay_api_001_webhook_egress_tls_policy_custody_required_for_database_version($database_version)
{
    if (!is_string($database_version) || preg_match('/^\d+\.\d+\.\d+$/D', $database_version) !== 1) return false;
    return version_compare($database_version, PAY_API_001_WEBHOOK_EGRESS_TLS_POLICY_CUSTODY_DB_VERSION, '>=');
}

/** Return whether immutable canonical payload/signing profile custody must exist. */
function pay_api_001_webhook_signing_profile_custody_required_for_database_version($database_version)
{
    if (!is_string($database_version) || preg_match('/^\d+\.\d+\.\d+$/D', $database_version) !== 1) return false;
    return version_compare($database_version, PAY_API_001_WEBHOOK_SIGNING_PROFILE_CUSTODY_DB_VERSION, '>=');
}

/** Return whether minimized provider-response, replay and reprocess custody must exist. */
function pay_api_001_webhook_response_replay_reprocess_custody_required_for_database_version($database_version)
{
    if (!is_string($database_version) || preg_match('/^\d+\.\d+\.\d+$/D', $database_version) !== 1) return false;
    return version_compare($database_version, PAY_API_001_WEBHOOK_RESPONSE_REPLAY_REPROCESS_CUSTODY_DB_VERSION, '>=');
}

/** Return whether governed webhook configuration provisioning is allowed. */
function pay_api_001_webhook_configuration_writer_allowed_for_database_version($database_version)
{
    if (!is_string($database_version) || preg_match('/^\d+\.\d+\.\d+$/D', $database_version) !== 1) return false;
    return version_compare($database_version, PAY_API_001_WEBHOOK_CONFIGURATION_WRITER_DB_VERSION, '>=');
}

/** Return whether the bounded payroll-summary webhook enqueue writer is allowed. */
function pay_api_001_webhook_enqueue_writer_allowed_for_database_version($database_version)
{
    if (!is_string($database_version) || preg_match('/^\d+\.\d+\.\d+$/D', $database_version) !== 1) return false;
    return version_compare($database_version, PAY_API_001_WEBHOOK_ENQUEUE_WRITER_DB_VERSION, '>=');
}

/** Return whether the append-only atomic webhook claim/lease writer is allowed. */
function pay_api_001_webhook_claim_writer_allowed_for_database_version($database_version)
{
    if (!is_string($database_version) || preg_match('/^\d+\.\d+\.\d+$/D', $database_version) !== 1) return false;
    return version_compare($database_version, PAY_API_001_WEBHOOK_CLAIM_WRITER_DB_VERSION, '>=');
}

/** Return the authority contract as it applies to the supplied database version. */
function pay_api_001_authority_contract($database_version=null)
{
    $effective_version = $database_version === null ? PAY_API_001_CONTRACT_VERSION : (string)$database_version;
    $route_allowed = pay_api_001_public_readonly_route_allowed_for_database_version($effective_version);
    $business_dispatch_allowed = pay_api_001_business_data_dispatch_allowed_for_database_version($effective_version);
    $webhook_outbox_custody = pay_api_001_webhook_outbox_custody_required_for_database_version($effective_version);
    $webhook_delivery_audit_custody = pay_api_001_webhook_delivery_audit_custody_required_for_database_version($effective_version);
    $webhook_target_lifecycle_custody = pay_api_001_webhook_target_lifecycle_custody_required_for_database_version($effective_version);
    $webhook_secret_reference_custody = pay_api_001_webhook_secret_reference_custody_required_for_database_version($effective_version);
    $webhook_delivery_policy_lease_custody = pay_api_001_webhook_delivery_policy_lease_custody_required_for_database_version($effective_version);
    $webhook_egress_tls_policy_custody = pay_api_001_webhook_egress_tls_policy_custody_required_for_database_version($effective_version);
    $webhook_signing_profile_custody = pay_api_001_webhook_signing_profile_custody_required_for_database_version($effective_version);
    $webhook_response_replay_reprocess_custody = pay_api_001_webhook_response_replay_reprocess_custody_required_for_database_version($effective_version);
    $webhook_configuration_writer_allowed = pay_api_001_webhook_configuration_writer_allowed_for_database_version($effective_version);
    $webhook_enqueue_writer_allowed = pay_api_001_webhook_enqueue_writer_allowed_for_database_version($effective_version);
    $webhook_claim_writer_allowed = pay_api_001_webhook_claim_writer_allowed_for_database_version($effective_version);
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
        'business_data_minimization_contract'=>true,
        'read_response_audit_custody'=>true,
        'payroll_period_summary_contract'=>true,
        'business_data_dispatcher_contract'=>true,
        'outbound_webhook_contract'=>true,
        'webhook_target_contract'=>true,
        'webhook_event_contract'=>true,
        'webhook_outbox_contract'=>true,
        'webhook_delivery_attempt_contract'=>true,
        'webhook_dead_letter_contract'=>true,
        'webhook_target_lifecycle_contract'=>true,
        'webhook_secret_reference_contract'=>true,
        'webhook_secret_rotation_contract'=>true,
        'webhook_delivery_policy_contract'=>true,
        'webhook_claim_lease_contract'=>true,
        'webhook_egress_tls_policy_contract'=>true,
        'webhook_canonical_payload_contract'=>true,
        'webhook_signing_profile_contract'=>true,
        'webhook_provider_response_evidence_contract'=>true,
        'webhook_replay_defense_contract'=>true,
        'webhook_reprocess_authorization_contract'=>true,
        'webhook_configuration_writer_contract'=>true,
        'webhook_enqueue_writer_contract'=>true,
        'webhook_claim_writer_contract'=>true,
        'query_string_credentials_allowed'=>false,
        'browser_session_as_api_credential'=>false,
        'cleartext_secret_storage'=>false,
        'credential_or_secret_custody'=>false,
        'credential_provisioning_writer_allowed'=>true,
        'provisioning_execution_writer_allowed'=>true,
        'external_http_route'=>$route_allowed,
        'business_data_dispatcher_allowed'=>$business_dispatch_allowed,
        'authorized_business_payload_allowed'=>$business_dispatch_allowed,
        'webhook_outbox_custody_required'=>$webhook_outbox_custody,
        'webhook_delivery_audit_custody_required'=>$webhook_delivery_audit_custody,
        'webhook_target_lifecycle_custody_required'=>$webhook_target_lifecycle_custody,
        'webhook_secret_reference_custody_required'=>$webhook_secret_reference_custody,
        'webhook_delivery_policy_lease_custody_required'=>$webhook_delivery_policy_lease_custody,
        'webhook_egress_tls_policy_custody_required'=>$webhook_egress_tls_policy_custody,
        'webhook_signing_profile_custody_required'=>$webhook_signing_profile_custody,
        'webhook_response_replay_reprocess_custody_required'=>$webhook_response_replay_reprocess_custody,
        'webhook_configuration_writer_allowed'=>$webhook_configuration_writer_allowed,
        'webhook_enqueue_writer_allowed'=>$webhook_enqueue_writer_allowed,
        'webhook_target_lifecycle_writer_allowed'=>$webhook_configuration_writer_allowed,
        'webhook_secret_resolver_allowed'=>false,
        'webhook_claim_writer_allowed'=>$webhook_claim_writer_allowed,
        'webhook_retry_executor_allowed'=>false,
        'webhook_reprocess_executor_allowed'=>false,
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
