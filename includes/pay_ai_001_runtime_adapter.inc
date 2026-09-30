<?php
/**
 * PAY-AI-001 deterministic assistive runtime adapter.
 *
 * This runtime is deliberately local and structured. It does not invoke an
 * external model/provider, does not perform network I/O and grants no write
 * authority. Every result remains advisory and requires human review.
 */
include_once(dirname(__FILE__).'/pay_ai_001_operational_contract.inc');

if (!defined('PAY_AI_001_RUNTIME_ADAPTER_VERSION'))
    define('PAY_AI_001_RUNTIME_ADAPTER_VERSION', '1.0.0');

/** @param string $database_version @return bool */
function pay_ai_001_deterministic_runtime_allowed_for_database_version($database_version)
{
    return version_compare((string)$database_version, '1.0.933', '>=');
}

/** @param string $capability @param array $grounded @param array $baseline @param string|null $error @return array|false */
function pay_ai_001_render_deterministic_assistance($capability, array $grounded, array $baseline, &$error = null)
{
    $error = null;
    if (!in_array((string)$capability, pay_ai_001_supported_capabilities(), true)) { $error = 'assistive_capability_denied'; return false; }
    if (!isset($grounded['grounding_refs_sha256'],$baseline['baseline_sha256'],$baseline['rows']) || !is_array($baseline['rows'])) { $error = 'assistive_evidence_invalid'; return false; }
    $items = array();
    if ($capability === 'anomaly_ranking') {
        foreach ($baseline['rows'] as $row) {
            if ((int)$row['anomaly_score'] <= 0) continue;
            $items[] = array(
                'metric_key'=>$row['metric_key'],
                'reference_sha256'=>$row['reference_sha256'],
                'anomaly_score'=>(int)$row['anomaly_score'],
                'direction'=>$row['direction'],
                'magnitude_band'=>$row['magnitude_band'],
            );
        }
    } elseif ($capability === 'variance_explanation') {
        foreach ($baseline['rows'] as $row) {
            if ($row['direction'] === 'not_applicable' || $row['direction'] === 'unchanged') continue;
            $items[] = array(
                'metric_key'=>$row['metric_key'],
                'reference_sha256'=>$row['reference_sha256'],
                'direction'=>$row['direction'],
                'magnitude_band'=>$row['magnitude_band'],
                'explanation_code'=>'aggregate_changed_from_authorized_baseline',
            );
        }
    } else {
        foreach ($baseline['rows'] as $row) {
            if ($row['status_code'] !== 'missing') continue;
            $items[] = array(
                'metric_key'=>$row['metric_key'],
                'reference_sha256'=>$row['reference_sha256'],
                'suggestion_code'=>'review_authorized_source_data',
            );
        }
    }
    $output_hash = pay_ai_001_grounding_hash(array(
        'capability'=>(string)$capability,
        'grounding_refs_sha256'=>$grounded['grounding_refs_sha256'],
        'baseline_sha256'=>$baseline['baseline_sha256'],
        'items'=>$items,
    ));
    if ($output_hash === false) { $error = 'assistive_output_hash_failed'; return false; }
    return array(
        'capability'=>(string)$capability,
        'generated_output_label'=>'assistive-deterministic-not-authoritative',
        'human_review_required'=>true,
        'provider_invoked'=>false,
        'network_used'=>false,
        'authoritative_write_allowed'=>false,
        'items'=>$items,
        'grounding_refs_sha256'=>$grounded['grounding_refs_sha256'],
        'baseline_sha256'=>$baseline['baseline_sha256'],
        'generated_content_sha256'=>$output_hash,
    );
}

/** @param string $database_version @param string $capability @param array $facts @param string|null $error @return array|false */
function pay_ai_001_execute_deterministic_assistance($database_version, $capability, array $facts, &$error = null)
{
    $error = null;
    if (!pay_ai_001_deterministic_runtime_allowed_for_database_version($database_version)) { $error = 'assistive_runtime_not_accepted'; return false; }
    if (version_compare((string)$database_version, '1.0.934', '>=') && !pay_ai_001_operational_runtime_allowed_for_database_version($database_version)) {
        $error = pay_ai_001_company_opted_out() ? 'assistive_company_opted_out' : 'assistive_runtime_globally_disabled';
        return false;
    }
    if (pay_ai_001_provider_runtime_allowed_for_database_version($database_version) || pay_ai_001_external_provider_execution_allowed()) { $error = 'assistive_provider_boundary_invalid'; return false; }
    $grounded = pay_ai_001_build_grounded_input($capability, $facts, $error);
    if ($grounded === false) return false;
    $baseline = pay_ai_001_build_deterministic_baseline($grounded, $error);
    if ($baseline === false) return false;
    return pay_ai_001_render_deterministic_assistance($capability, $grounded, $baseline, $error);
}
?>
