<?php
/** PAY-AI-001 deterministic aggregate anomaly/variance baseline. */
if (!defined('PAY_AI_001_BASELINE_CONTRACT_VERSION'))
    define('PAY_AI_001_BASELINE_CONTRACT_VERSION', '1.0.0');

/** @param float $value @return string */
function pay_ai_001_baseline_direction($value)
{
    if ($value > 0) return 'increase';
    if ($value < 0) return 'decrease';
    return 'unchanged';
}

/** @param float $absolute_delta @param float|null $percent_delta @return string */
function pay_ai_001_baseline_magnitude_band($absolute_delta, $percent_delta)
{
    $pct = $percent_delta === null ? 0.0 : abs((float)$percent_delta);
    if ($absolute_delta >= 10 || $pct >= 25.0) return 'high';
    if ($absolute_delta >= 3 || $pct >= 10.0) return 'medium';
    if ($absolute_delta > 0 || $pct > 0.0) return 'low';
    return 'none';
}

/** @param array $grounded @param string|null $error @return array|false */
function pay_ai_001_build_deterministic_baseline(array $grounded, &$error = null)
{
    $error = null;
    if (!isset($grounded['facts']) || !is_array($grounded['facts']) || !isset($grounded['grounding_refs_sha256'])) {
        $error = 'baseline_grounding_invalid';
        return false;
    }
    $rows = array();
    foreach ($grounded['facts'] as $fact) {
        if (!is_array($fact) || !isset($fact['metric_key'],$fact['reference_sha256'],$fact['value_kind'],$fact['status_code'])) {
            $error = 'baseline_fact_invalid';
            return false;
        }
        $kind = (string)$fact['value_kind'];
        $status = (string)$fact['status_code'];
        $row = array(
            'metric_key'=>(string)$fact['metric_key'],
            'reference_sha256'=>(string)$fact['reference_sha256'],
            'status_code'=>$status,
            'direction'=>'not_applicable',
            'absolute_delta'=>null,
            'percent_delta'=>null,
            'anomaly_score'=>0,
            'magnitude_band'=>'none',
            'requires_human_review'=>true,
        );
        if ($status === 'suppressed' || $status === 'missing') {
            // Preserve only the status-only evidence. Never derive a delta or
            // score from values attached to a hidden fact.
            $rows[] = $row;
            continue;
        }
        if (($kind === 'count' || $kind === 'ratio') && $status === 'available' && array_key_exists('current_value',$fact) && array_key_exists('baseline_value',$fact) && $fact['baseline_value'] !== null) {
            $current = (float)$fact['current_value'];
            $baseline = (float)$fact['baseline_value'];
            $delta = $current - $baseline;
            $absolute = abs($delta);
            $pct = abs($baseline) > 0.000000001 ? ($delta / abs($baseline)) * 100.0 : ($absolute > 0 ? null : 0.0);
            $score_basis = $pct === null ? min(10000.0, $absolute * 100.0) : min(10000.0, abs($pct) * 100.0);
            $row['direction'] = pay_ai_001_baseline_direction($delta);
            $row['absolute_delta'] = round($absolute, 6);
            $row['percent_delta'] = $pct === null ? null : round($pct, 6);
            $row['anomaly_score'] = (int)round($score_basis);
            $row['magnitude_band'] = pay_ai_001_baseline_magnitude_band($absolute, $pct);
        }
        $rows[] = $row;
    }
    usort($rows, function($left, $right) {
        if ($left['anomaly_score'] !== $right['anomaly_score']) return $left['anomaly_score'] > $right['anomaly_score'] ? -1 : 1;
        return strcmp($left['metric_key'].'|'.$left['reference_sha256'], $right['metric_key'].'|'.$right['reference_sha256']);
    });
    $hash = function_exists('pay_ai_001_grounding_hash') ? pay_ai_001_grounding_hash(array('grounding_refs_sha256'=>$grounded['grounding_refs_sha256'],'rows'=>$rows)) : false;
    if ($hash === false) { $error = 'baseline_hash_failed'; return false; }
    return array('rows'=>$rows,'baseline_sha256'=>$hash,'algorithm'=>'deterministic-aggregate-delta-v1');
}
?>
