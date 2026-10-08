<?php
/**
 * PAY-GLB-001 S02 — pure read-model projection from ALREADY AUTHORIZED,
 * strictly minimized normalized operational evidence; no storage/IO/SQL.
 */
require_once __DIR__.'/pay_glb_001_contract.inc';

/** Fail closed on unrecognized keys, types, scope, future observations or fabricated freshness. */
function pay_glb_001_rollup_row_valid($ctx, $row)
{
    if (!pay_glb_001_exact_keys($row, array('company_id','entity_code','country_code',
        'delivery_model','operational_status','reconciliation_status','source_observed_utc',
        'source_expires_utc','evidence_sha256','source_class'))) return false;
    if (!is_int($row['company_id']) || $row['company_id'] !== $ctx['company_id'] ||
        !pay_glb_001_code($row['entity_code']) ||
        !in_array($row['entity_code'], $ctx['entity_codes'], true) ||
        !is_string($row['country_code']) || preg_match('/^[A-Z]{2}$/D', $row['country_code']) !== 1 ||
        !in_array($row['delivery_model'], pay_glb_001_models(), true) ||
        !in_array($row['operational_status'], array('READY','ATTENTION','BLOCKED','UNKNOWN'), true) ||
        !in_array($row['reconciliation_status'], array('RECONCILED','OUTSTANDING','UNVERIFIED'), true) ||
        !pay_glb_001_iso_utc($row['source_observed_utc']) ||
        !pay_glb_001_iso_utc($row['source_expires_utc']) ||
        !pay_glb_001_sha256($row['evidence_sha256']) ||
        !in_array($row['source_class'], array('INTERNAL_AGGREGATE','EXTERNAL_UNVERIFIED'), true)) return false;
    if ($row['source_observed_utc'] > $ctx['as_of_utc'] ||
        $row['source_expires_utc'] <= $row['source_observed_utc']) return false;
    if (($row['delivery_model'] === 'NATIVE' && $row['source_class'] !== 'INTERNAL_AGGREGATE') ||
        ($row['delivery_model'] !== 'NATIVE' && $row['source_class'] !== 'EXTERNAL_UNVERIFIED')) return false;
    // An external partner/managed/model source never inherits native-ready or reconciled authority.
    if ($row['delivery_model'] !== 'NATIVE' &&
        ($row['operational_status'] === 'READY' || $row['reconciliation_status'] === 'RECONCILED')) return false;
    return true;
}

/**
 * @return array|false A company-scoped, deterministic aggregate-only result.
 * Invalid/unauthorized source input aborts the WHOLE projection (no partial disclosure).
 */
function pay_glb_001_project_rollup($ctx, $rows)
{
    if (!pay_glb_001_context_valid($ctx,'rollup') || !is_array($rows) ||
        count($rows) > 32 || array_keys($rows) !== (count($rows) ? range(0,count($rows)-1) : array())) return false;
    $out = array();
    $seen = array();
    $counts = array('NATIVE'=>0,'EXTERNAL_PROVIDER'=>0,'PARTNER_OPERATED'=>0,'MANAGED_SERVICE'=>0);
    $warnings = 0;
    foreach ($rows as $row) {
        if (!pay_glb_001_rollup_row_valid($ctx,$row)) return false;
        $key = $row['entity_code']; // One approved model/country per legal-entity scope.
        if (isset($seen[$key])) return false;
        $seen[$key] = true;
        $stale = $row['source_expires_utc'] <= $ctx['as_of_utc'];
        if ($stale) $warnings++;
        $counts[$row['delivery_model']]++;
        $out[] = array(
            'entity_code' => $row['entity_code'], 'country_code' => $row['country_code'],
            'delivery_model' => $row['delivery_model'],
            'status' => $stale ? 'UNKNOWN_STALE' : $row['operational_status'],
            'reconciliation' => $stale ? 'UNVERIFIED_STALE' : $row['reconciliation_status'],
            'source_class' => $row['source_class'],
            'source_observed_utc' => $row['source_observed_utc'],
            'source_expires_utc' => $row['source_expires_utc'],
            'stale' => $stale, 'evidence_sha256' => $row['evidence_sha256']
        );
    }
    usort($out, function($a,$b){
        return strcmp($a['entity_code'].'|'.$a['country_code'].'|'.$a['delivery_model'],
                      $b['entity_code'].'|'.$b['country_code'].'|'.$b['delivery_model']);
    });
    return array(
        'scope' => 'AUTHORIZED_AGGREGATES_ONLY',
        'as_of_utc' => $ctx['as_of_utc'],
        'country_entity_rows' => $out,
        'delivery_model_counts' => $counts,
        'stale_sources' => $warnings,
        'total_entities' => count($out),
        'live_provider_or_legal_compliance_authority' => false
    );
}
?>
