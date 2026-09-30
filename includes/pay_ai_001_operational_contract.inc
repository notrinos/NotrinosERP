<?php
/**
 * PAY-AI-001 operational/provider safety contract.
 *
 * The accepted PAY-AI-001 implementation remains local and deterministic.
 * External provider/model invocation is not enabled by this contract. The
 * helpers below provide explicit fail-closed provider-failure behavior,
 * emergency disable and company opt-out hooks, and stable privacy/bias policy
 * hashes used by immutable request/output custody.
 */
if (!defined('PAY_AI_001_OPERATIONAL_CONTRACT_VERSION'))
    define('PAY_AI_001_OPERATIONAL_CONTRACT_VERSION', '1.0.0');

/** @return array */
function pay_ai_001_operational_policy()
{
    return array(
        'runtime_mode'=>'local_deterministic',
        'external_provider_execution_allowed'=>false,
        'network_transport_allowed'=>false,
        'free_text_generation_allowed'=>false,
        'raw_prompt_storage_allowed'=>false,
        'raw_output_storage_allowed'=>false,
        'provider_failure_must_abstain'=>true,
        'global_disable_supported'=>true,
        'company_opt_out_supported'=>true,
        'bias_review_required'=>true,
        'data_minimization_required'=>true,
        'authorized_grounding_required'=>true,
        'human_review_required'=>true,
    );
}

/** @return bool */
function pay_ai_001_external_provider_execution_allowed()
{
    return false;
}

/** @return bool */
function pay_ai_001_force_disabled()
{
    return defined('PAY_AI_001_FORCE_DISABLED') && PAY_AI_001_FORCE_DISABLED === true;
}

/** @return bool */
function pay_ai_001_company_opted_out()
{
    if (defined('PAY_AI_001_COMPANY_OPT_OUT') && PAY_AI_001_COMPANY_OPT_OUT === true)
        return true;
    if (!function_exists('get_company_pref'))
        return false;
    $value = get_company_pref('pay_ai_001_opt_out');
    if (is_bool($value)) return $value;
    return in_array(strtolower(trim((string)$value)), array('1','true','yes','on'), true);
}

/** @param string $database_version @return bool */
function pay_ai_001_operational_runtime_allowed_for_database_version($database_version)
{
    return version_compare((string)$database_version, '1.0.934', '>=')
        && !pay_ai_001_force_disabled()
        && !pay_ai_001_company_opted_out();
}

/** @return string */
function pay_ai_001_provider_policy_sha256()
{
    return hash('sha256', json_encode(array(
        'provider_code'=>'local_deterministic',
        'external_provider_execution_allowed'=>false,
        'network_transport_allowed'=>false,
        'residency'=>'local_application_boundary_only',
        'raw_prompt_storage_allowed'=>false,
        'raw_output_storage_allowed'=>false,
    ), JSON_UNESCAPED_SLASHES));
}

/** @return string */
function pay_ai_001_data_minimization_policy_sha256()
{
    return hash('sha256', json_encode(array(
        'aggregate_or_status_only'=>true,
        'employee_identity_allowed'=>false,
        'bank_tax_detail_allowed'=>false,
        'suppressed_values_allowed'=>false,
        'arbitrary_free_text_allowed'=>false,
    ), JSON_UNESCAPED_SLASHES));
}

/** @return string */
function pay_ai_001_bias_review_policy_sha256()
{
    return hash('sha256', json_encode(array(
        'human_review_required'=>true,
        'false_positive_capture_required'=>true,
        'autonomous_employment_decision_allowed'=>false,
        'protected_attribute_inference_allowed'=>false,
        'appeal_evidence_supported'=>true,
    ), JSON_UNESCAPED_SLASHES));
}

/**
 * Return a bounded provider-failure envelope. This function never retries,
 * falls back to another provider, or performs network I/O.
 *
 * @param string $error_code
 * @return array
 */
function pay_ai_001_provider_failure_abstention($error_code)
{
    $allowed = array('provider_unavailable','provider_timeout','provider_policy_denied','provider_disabled');
    $code = in_array((string)$error_code, $allowed, true) ? (string)$error_code : 'provider_policy_denied';
    return array(
        'generated_output_label'=>'assistive-provider-failure-abstention',
        'human_review_required'=>true,
        'provider_invoked'=>false,
        'network_used'=>false,
        'provider_failure'=>true,
        'abstained'=>true,
        'error_code'=>$code,
        'items'=>array(),
        'authoritative_write_allowed'=>false,
    );
}
?>
