<?php
/**
 * PAY-GLB-001 S05 — pure cross-view snapshot composition, NOT a DB transaction.
 * Every scoped entity needs a rollup AND explicit exception-coverage row.
 * Hash below is a change detector, NOT a signature or proof of admission.
 */
require_once __DIR__.'/pay_glb_001_calendar.inc';
require_once __DIR__.'/pay_glb_001_exceptions.inc';

/** @return array|false Deterministic, all-or-nothing internal read-model snapshot. */
function pay_glb_001_project_snapshot($ctx, $rollup_rows, $events, $exception_rows)
{
    if (!pay_glb_001_context_valid($ctx, 'calendar')) return false;
    $rollup = pay_glb_001_project_rollup($ctx, $rollup_rows);
    if ($rollup === false || count($rollup['country_entity_rows']) !== count($ctx['entity_codes'])) return false;
    $calendar = pay_glb_001_project_calendar($ctx, $rollup_rows, $events);
    if ($calendar === false) return false;
    $exceptions = pay_glb_001_project_exceptions($ctx, $rollup_rows, $exception_rows);
    if ($exceptions === false || $exceptions['total_entities'] !== $rollup['total_entities']) return false;
    // Revalidated projections are already in canonical order. Do not hash raw employee/provider material.
    $canonical = json_encode(array('source_version'=>PAY_GLB_001_SOURCE_VERSION,
        'custody_version'=>PAY_GLB_001_CUSTODY_VERSION,
        'as_of_utc'=>$ctx['as_of_utc'], 'rollup'=>$rollup, 'calendar'=>$calendar,
        'exceptions'=>$exceptions), JSON_UNESCAPED_SLASHES);
    if ($canonical === false) return false;
    return array('scope'=>'AUTHORIZED_OFFLINE_SNAPSHOT_ONLY','as_of_utc'=>$ctx['as_of_utc'],
        'source_version'=>PAY_GLB_001_SOURCE_VERSION,
        'custody_version'=>PAY_GLB_001_CUSTODY_VERSION,
        'projection_sha256'=>hash('sha256', $canonical),
        'rollup'=>$rollup,'calendar'=>$calendar,'exceptions'=>$exceptions,
        'snapshot_is_signed_or_authenticated'=>false,
        'database_snapshot_or_live_collection_performed'=>false,
        'legal_deadline_or_provider_authority'=>false);
}
?>
