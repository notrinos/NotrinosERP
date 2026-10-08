<?php
/**
 * PAY-GLB-001 S08: only pre-minimized, server-provided aggregate batches.
 * NOT a SQL collector, provider receiver, trusted-source registry or grant.
 * Trust of the supplying internal service must be reviewed independently;
 * accepting a well-shaped batch does not authenticate its origin.
 */
require_once __DIR__.'/pay_glb_001_snapshot.inc';

/** Revalidate the complete minimized snapshot and its non-authenticating change digest. */
function pay_glb_001_validate_collected_batch($scope, $batch)
{
    if (!pay_glb_001_context_valid($scope, 'calendar') ||
        !pay_glb_001_exact_keys($batch, array('company_id','as_of_utc',
            'entity_codes','rollup_rows','calendar_events','exception_rows',
            'collection_sha256','collection_kind')) ||
        !is_int($batch['company_id']) || $batch['company_id'] !== $scope['company_id'] ||
        $batch['as_of_utc'] !== $scope['as_of_utc'] ||
        $batch['entity_codes'] !== $scope['entity_codes'] ||
        $batch['collection_kind'] !== 'SERVER_MINIMIZED_AGGREGATES' ||
        !pay_glb_001_sha256($batch['collection_sha256'])) return false;
    // Full strict schema checks occur before any display, including all-entity
    // coverage, source timestamp/country/model coherence and stale downgrades.
    $snapshot = pay_glb_001_project_snapshot($scope, $batch['rollup_rows'],
        $batch['calendar_events'], $batch['exception_rows']);
    if ($snapshot === false) return false;
    // Deterministic change digest, NOT a signature, seal, provider admission or
    // proof that collection occurred in a single DB transaction.
    $json = json_encode(array('company_id'=>$scope['company_id'],
        'as_of_utc'=>$scope['as_of_utc'], 'entity_codes'=>$scope['entity_codes'],
        'rollup_rows'=>$batch['rollup_rows'], 'calendar_events'=>$batch['calendar_events'],
        'exception_rows'=>$batch['exception_rows']), JSON_UNESCAPED_SLASHES);
    if ($json === false || !hash_equals(hash('sha256', $json), $batch['collection_sha256']))
        return false;
    return $snapshot;
}
?>
