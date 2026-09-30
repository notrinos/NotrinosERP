<?php
/**
 * PAY-AI-001 provider-neutral assistive AI governance contract.
 *
 * This contract deliberately contains no provider invocation, network client,
 * payroll mutation, rule publication, approval, payment, GL or filing authority.
 */
if (!defined('PAY_AI_001_CONTRACT_ID'))
    define('PAY_AI_001_CONTRACT_ID', 'notrinoserp.pay.ai.assistive.v1');
if (!defined('PAY_AI_001_FOUNDATION_VERSION'))
    define('PAY_AI_001_FOUNDATION_VERSION', '1.0.0');

/** @return array */
function pay_ai_001_supported_capabilities()
{
    return array('anomaly_ranking', 'variance_explanation', 'missing_data_suggestion');
}

/** @return array */
function pay_ai_001_governance_contract()
{
    return array(
        'contract_id' => PAY_AI_001_CONTRACT_ID,
        'contract_version' => PAY_AI_001_FOUNDATION_VERSION,
        'optional' => true,
        'human_review_required' => true,
        'grounded_authorized_references_required' => true,
        'abstention_required_when_ungrounded' => true,
        'data_minimization_required' => true,
        'provider_residency_security_approval_required' => true,
        'bias_review_required' => true,
        'opt_out_respected' => true,
        'generated_output_must_be_labeled' => true,
        'authoritative_write_allowed' => false,
        'payroll_calculation_authority_allowed' => false,
        'formula_or_rule_publish_authority_allowed' => false,
        'approval_authority_allowed' => false,
        'payment_authority_allowed' => false,
        'gl_authority_allowed' => false,
        'statutory_filing_authority_allowed' => false,
        'legal_advice_allowed' => false,
        'autonomous_employment_decision_allowed' => false,
        'cross_company_authority_allowed' => false,
        'network_authority_allowed' => false,
        'provider_runtime_allowed' => false,
        'raw_prompt_storage_allowed' => false,
        'raw_output_storage_allowed' => false,
    );
}

/** @param string $database_version @return bool */
function pay_ai_001_request_custody_required_for_database_version($database_version)
{
    return version_compare((string)$database_version, '1.0.928', '>=');
}

/** @param string $database_version @return bool */
function pay_ai_001_output_custody_required_for_database_version($database_version)
{
    return version_compare((string)$database_version, '1.0.929', '>=');
}

/** @param string $database_version @return bool */
function pay_ai_001_review_custody_required_for_database_version($database_version)
{
    return version_compare((string)$database_version, '1.0.930', '>=');
}

/** @param string $database_version @return bool */
function pay_ai_001_grounding_contract_required_for_database_version($database_version)
{
    return version_compare((string)$database_version, '1.0.931', '>=');
}

/** @param string $database_version @return bool */
function pay_ai_001_deterministic_baseline_required_for_database_version($database_version)
{
    return version_compare((string)$database_version, '1.0.932', '>=');
}

/** @param string $database_version @return bool */
function pay_ai_001_deterministic_assistive_runtime_required_for_database_version($database_version)
{
    return version_compare((string)$database_version, '1.0.933', '>=');
}

/** @param string $database_version @return bool */
function pay_ai_001_provider_runtime_allowed_for_database_version($database_version)
{
    return false;
}

/** @param string $database_version @return bool */
function pay_ai_001_authoritative_write_allowed_for_database_version($database_version)
{
    return false;
}
?>
