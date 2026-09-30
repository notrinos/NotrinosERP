<?php
/**
 * PAY-AI-001 permission-filtered grounding contract.
 *
 * Only already-authorized, aggregate or status facts may enter the assistive
 * runtime. The contract rejects free text, employee identifiers, payroll
 * detail, bank/tax data, formulas and arbitrary fields before any provider
 * boundary can be considered.
 */
if (!defined('PAY_AI_001_GROUNDING_CONTRACT_VERSION'))
    define('PAY_AI_001_GROUNDING_CONTRACT_VERSION', '1.0.0');

/** @return array */
function pay_ai_001_grounding_reference_types()
{
    return array('hrm_anl_aggregate', 'pay_core_validation_summary', 'pay_core_reconciliation_summary');
}

/** @return array */
function pay_ai_001_grounding_metric_allowlist()
{
    return array(
        'workforce.active_headcount.total',
        'workforce.active_headcount.department',
        'workforce.active_headcount.employment_type',
        'workforce.active_headcount.department_employment_type',
        'payroll.validation.status',
        'payroll.reconciliation.status',
    );
}

/** @return array */
function pay_ai_001_grounding_status_allowlist()
{
    return array('available', 'suppressed', 'pass', 'warning', 'missing', 'not_applicable');
}

/** @param mixed $value @return string|false */
function pay_ai_001_grounding_hash($value)
{
    $normalized = pay_ai_001_grounding_normalize($value);
    $json = json_encode($normalized, JSON_UNESCAPED_SLASHES | JSON_UNESCAPED_UNICODE | JSON_PRESERVE_ZERO_FRACTION);
    return is_string($json) ? hash('sha256', $json) : false;
}

/** @param mixed $value @return mixed */
function pay_ai_001_grounding_normalize($value)
{
    if (!is_array($value)) return $value;
    $keys = array_keys($value);
    $assoc = $keys !== range(0, count($keys) - 1);
    if ($assoc) {
        sort($keys, SORT_STRING);
        $out = array();
        foreach ($keys as $key) $out[(string)$key] = pay_ai_001_grounding_normalize($value[$key]);
        return $out;
    }
    $out = array();
    foreach ($value as $item) $out[] = pay_ai_001_grounding_normalize($item);
    return $out;
}

/** @param mixed $value @return bool */
function pay_ai_001_grounding_is_finite_number($value)
{
    if (!is_int($value) && !is_float($value)) return false;
    $number = (float)$value;
    return is_finite($number) && abs($number) <= 1000000000;
}

/** @param array $fact @param string|null $error @return array|false */
function pay_ai_001_grounding_validate_fact(array $fact, &$error = null)
{
    $error = null;
    $allowed_keys = array('reference_type','reference_sha256','metric_key','value_kind','current_value','baseline_value','status_code','authorized','opted_out');
    foreach (array_keys($fact) as $key) {
        if (!in_array((string)$key, $allowed_keys, true)) {
            $error = 'grounding_field_denied';
            return false;
        }
    }
    foreach (array('reference_type','reference_sha256','metric_key','value_kind','status_code','authorized','opted_out') as $required) {
        if (!array_key_exists($required, $fact)) {
            $error = 'grounding_field_missing';
            return false;
        }
    }
    if ($fact['authorized'] !== true) { $error = 'grounding_reference_unauthorized'; return false; }
    if ($fact['opted_out'] !== false) { $error = 'grounding_reference_opted_out'; return false; }
    if (!in_array((string)$fact['reference_type'], pay_ai_001_grounding_reference_types(), true)) { $error = 'grounding_reference_type_denied'; return false; }
    if (!preg_match('/^[a-f0-9]{64}$/D', (string)$fact['reference_sha256'])) { $error = 'grounding_reference_hash_invalid'; return false; }
    if (!in_array((string)$fact['metric_key'], pay_ai_001_grounding_metric_allowlist(), true)) { $error = 'grounding_metric_denied'; return false; }
    if (!in_array((string)$fact['value_kind'], array('count','ratio','status'), true)) { $error = 'grounding_value_kind_denied'; return false; }
    if (!in_array((string)$fact['status_code'], pay_ai_001_grounding_status_allowlist(), true)) { $error = 'grounding_status_denied'; return false; }

    $kind = (string)$fact['value_kind'];
    $status = (string)$fact['status_code'];
    $current = array_key_exists('current_value', $fact) ? $fact['current_value'] : null;
    $baseline = array_key_exists('baseline_value', $fact) ? $fact['baseline_value'] : null;
    if ($kind === 'status') {
        if ($current !== null || $baseline !== null) { $error = 'grounding_status_value_forbidden'; return false; }
    } elseif ($status === 'suppressed' || $status === 'missing' || $status === 'not_applicable') {
        if ($current !== null || $baseline !== null) { $error = 'grounding_hidden_value_forbidden'; return false; }
    } else {
        if (!pay_ai_001_grounding_is_finite_number($current)) { $error = 'grounding_current_value_invalid'; return false; }
        if ($baseline !== null && !pay_ai_001_grounding_is_finite_number($baseline)) { $error = 'grounding_baseline_value_invalid'; return false; }
        if ($kind === 'count' && (((float)$current < 0) || ($baseline !== null && (float)$baseline < 0))) { $error = 'grounding_count_negative'; return false; }
        if ($kind === 'ratio' && (abs((float)$current) > 1000000 || ($baseline !== null && abs((float)$baseline) > 1000000))) { $error = 'grounding_ratio_out_of_range'; return false; }
    }

    return array(
        'reference_type'=>(string)$fact['reference_type'],
        'reference_sha256'=>(string)$fact['reference_sha256'],
        'metric_key'=>(string)$fact['metric_key'],
        'value_kind'=>$kind,
        'current_value'=>$current,
        'baseline_value'=>$baseline,
        'status_code'=>$status,
    );
}

/** @param string $capability @param array $facts @param string|null $error @return array|false */
function pay_ai_001_build_grounded_input($capability, array $facts, &$error = null)
{
    $error = null;
    if (!function_exists('pay_ai_001_supported_capabilities') || !in_array((string)$capability, pay_ai_001_supported_capabilities(), true)) {
        $error = 'grounding_capability_denied';
        return false;
    }
    if (!$facts || count($facts) > 64) { $error = 'grounding_fact_count_invalid'; return false; }
    $safe = array();
    foreach ($facts as $fact) {
        if (!is_array($fact)) { $error = 'grounding_fact_invalid'; return false; }
        $validated = pay_ai_001_grounding_validate_fact($fact, $error);
        if ($validated === false) return false;
        $safe[] = $validated;
    }
    usort($safe, function($left, $right) {
        $a = $left['metric_key'].'|'.$left['reference_sha256'];
        $b = $right['metric_key'].'|'.$right['reference_sha256'];
        return strcmp($a, $b);
    });
    $hash = pay_ai_001_grounding_hash(array('capability'=>(string)$capability, 'facts'=>$safe));
    if ($hash === false) { $error = 'grounding_hash_failed'; return false; }
    return array(
        'capability'=>(string)$capability,
        'facts'=>$safe,
        'fact_count'=>count($safe),
        'grounding_refs_sha256'=>$hash,
        'contains_raw_prompt'=>false,
        'contains_raw_output'=>false,
        'contains_employee_identity'=>false,
        'contains_bank_or_tax_detail'=>false,
    );
}
?>
