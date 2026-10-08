<?php
/**
 * PAY-GLB-001 S04 — bounded, aggregate-only reconciliation exception coverage.
 * All inputs must be supplied by a future independently authorized server adapter.
 * No employee identifiers, raw evidence, balances, currency or provider credentials.
 */
require_once __DIR__.'/pay_glb_001_rollup.inc';

/** Fail the entire projection for one malformed, out-of-scope or false-authority row. */
function pay_glb_001_exception_row_valid($ctx, $row, $approved)
{
    if (!pay_glb_001_exact_keys($row, array('company_id','entity_code','country_code',
        'delivery_model','source_class','source_observed_utc','source_expires_utc',
        'evidence_sha256','open_count','review_count','resolved_count'))) return false;
    if (!is_int($row['company_id']) || $row['company_id'] !== $ctx['company_id'] ||
        !pay_glb_001_code($row['entity_code']) || !isset($approved[$row['entity_code']])) return false;
    $expected = $approved[$row['entity_code']];
    if ($expected['country_code'] !== $row['country_code'] ||
        $expected['delivery_model'] !== $row['delivery_model'] ||
        $expected['source_class'] !== $row['source_class'] ||
        !pay_glb_001_iso_utc($row['source_observed_utc']) ||
        !pay_glb_001_iso_utc($row['source_expires_utc']) ||
        !pay_glb_001_sha256($row['evidence_sha256']) ||
        $row['source_observed_utc'] > $ctx['as_of_utc'] ||
        $row['source_expires_utc'] <= $row['source_observed_utc']) return false;
    $sum = 0;
    foreach (array('open_count','review_count','resolved_count') as $field) {
        if (!is_int($row[$field]) || $row[$field] < 0 || $row[$field] > 1000000) return false;
        $sum += $row[$field];
    }
    if ($sum > 1000000) return false;
    // A third-party report must NEVER assert final resolution authority.
    if ($row['delivery_model'] !== 'NATIVE' && $row['resolved_count'] !== 0) return false;
    return true;
}

/**
 * Require exactly one bounded exception-coverage aggregate for EVERY scoped
 * rollup entity; omission is NOT evidence of zero exceptions.
 * @return array|false
 */
function pay_glb_001_project_exceptions($ctx, $rollup_rows, $exception_rows)
{
    if (!pay_glb_001_context_valid($ctx, 'rollup') || !is_array($exception_rows) ||
        count($exception_rows) > 32 ||
        array_keys($exception_rows) !== (count($exception_rows) ? range(0,count($exception_rows)-1) : array())) return false;
    $rollup = pay_glb_001_project_rollup($ctx, $rollup_rows);
    if ($rollup === false) return false;
    $approved = array();
    foreach ($rollup['country_entity_rows'] as $row) $approved[$row['entity_code']] = $row;
    if (count($approved) !== count($exception_rows)) return false;
    $seen = array(); $out = array(); $stale = 0; $internal = 0; $external = 0;
    foreach ($exception_rows as $row) {
        if (!pay_glb_001_exception_row_valid($ctx, $row, $approved)) return false;
        $entity = $row['entity_code'];
        if (isset($seen[$entity])) return false;
        $seen[$entity] = true;
        $is_stale = $approved[$entity]['stale'] || $row['source_expires_utc'] <= $ctx['as_of_utc'];
        if ($is_stale) $stale++;
        $sum = $row['open_count'] + $row['review_count'] + $row['resolved_count'];
        if (!$is_stale) {
            if ($row['delivery_model'] === 'NATIVE') $internal += $sum;
            else $external += $sum;
        }
        $out[] = array('entity_code'=>$entity,'country_code'=>$row['country_code'],
            'delivery_model'=>$row['delivery_model'],'source_class'=>$row['source_class'],
            'source_observed_utc'=>$row['source_observed_utc'],
            'source_expires_utc'=>$row['source_expires_utc'],
            'evidence_sha256'=>$row['evidence_sha256'],
            'source_state'=>$is_stale ? 'STALE_UNVERIFIED' :
                ($row['delivery_model']==='NATIVE' ? 'INTERNAL_REVIEWED' : 'EXTERNAL_UNVERIFIED'),
            // Never present stale quantities as current values.
            'reported_open'=>$is_stale ? null : $row['open_count'],
            'reported_review'=>$is_stale ? null : $row['review_count'],
            'reported_resolved'=>$is_stale ? null : $row['resolved_count']);
    }
    usort($out, function($a,$b){return strcmp($a['entity_code'], $b['entity_code']);});
    return array('scope'=>'AUTHORIZED_EXCEPTION_AGGREGATES_ONLY',
        'as_of_utc'=>$ctx['as_of_utc'],'entries'=>$out,'total_entities'=>count($out),
        'stale_coverage_sources'=>$stale,
        'fresh_internal_reported_count'=>$internal,
        'fresh_external_unverified_reported_count'=>$external,
        'absence_means_zero_exceptions'=>false,
        'provider_or_legal_resolution_authority'=>false);
}
?>
