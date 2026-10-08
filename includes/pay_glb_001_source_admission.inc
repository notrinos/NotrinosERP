<?php
/**
 * PAY-GLB-001 S11. Pinned per-entity read-source admission contract, OFFLINE.
 * A model/hash assertion never proves an approved mapper, current legal entity,
 * authenticated origin, live transaction or current provider report. A future
 * trusted collector must resolve & verify those independently before routing.
 */
require_once __DIR__.'/pay_glb_001_collector_boundary.inc';

/** Compute the non-signing integrity digest of an exact source manifest. */
function pay_glb_001_source_manifest_sha256($manifest)
{
    if (!is_array($manifest)) return false;
    $value = $manifest;
    unset($value['manifest_sha256']);
    $json = json_encode($value, JSON_UNESCAPED_SLASHES);
    return $json === false ? false : hash('sha256', $json);
}

/**
 * Assert no country/model/entity drift, no extra/unmapped entities and no
 * external source falsely promoted to native/reconciled authority.
 * @return bool Strict, fully covered manifest-to-batch contract only.
 */
function pay_glb_001_source_manifest_valid($scope, $batch, $manifest)
{
    if (!pay_glb_001_context_valid($scope, 'calendar') ||
        pay_glb_001_validate_collected_batch($scope, $batch) === false ||
        !pay_glb_001_exact_keys($manifest, array('company_id','as_of_utc',
            'entity_codes','mapping_contract_version','mapping_policy_sha256',
            'collector_policy_sha256','sources','manifest_sha256')) ||
        !is_int($manifest['company_id']) ||
        $manifest['company_id'] !== $scope['company_id'] ||
        $manifest['as_of_utc'] !== $scope['as_of_utc'] ||
        $manifest['entity_codes'] !== $scope['entity_codes'] ||
        $manifest['mapping_contract_version'] !== 'GLB_AGGREGATE_MAPPING_V1' ||
        !pay_glb_001_sha256($manifest['mapping_policy_sha256']) ||
        !pay_glb_001_sha256($manifest['collector_policy_sha256']) ||
        !is_array($manifest['sources']) ||
        count($manifest['sources']) !== count($scope['entity_codes']) ||
        array_keys($manifest['sources']) !== range(0, count($manifest['sources'])-1) ||
        !pay_glb_001_sha256($manifest['manifest_sha256'])) return false;
    $rows = array();
    foreach ($batch['rollup_rows'] as $row) $rows[$row['entity_code']] = $row;
    $seen = array();
    foreach ($manifest['sources'] as $source) {
        if (!pay_glb_001_exact_keys($source, array('entity_code','country_code',
                'delivery_model','source_class','mapping_version_sha256',
                'source_contract_sha256','collected_at_utc','expires_at_utc',
                'source_state')) ||
            !pay_glb_001_code($source['entity_code']) ||
            isset($seen[$source['entity_code']]) ||
            !isset($rows[$source['entity_code']])) return false;
        $row = $rows[$source['entity_code']];
        if ($source['country_code'] !== $row['country_code'] ||
            $source['delivery_model'] !== $row['delivery_model'] ||
            $source['source_class'] !== $row['source_class'] ||
            !pay_glb_001_sha256($source['mapping_version_sha256']) ||
            !pay_glb_001_sha256($source['source_contract_sha256']) ||
            !pay_glb_001_iso_utc($source['collected_at_utc']) ||
            !pay_glb_001_iso_utc($source['expires_at_utc']) ||
            $source['collected_at_utc'] !== $row['source_observed_utc'] ||
            $source['expires_at_utc'] !== $row['source_expires_utc'] ||
            $source['collected_at_utc'] > $scope['as_of_utc'] ||
            (($row['delivery_model'] === 'NATIVE') !==
                ($source['source_state'] === 'REVIEWED_INTERNAL_AGGREGATE')) ||
            ($row['delivery_model'] !== 'NATIVE' &&
                $source['source_state'] !== 'UNVERIFIED_EXTERNAL_REPORT')) return false;
        $seen[$source['entity_code']] = true;
    }
    $actual = pay_glb_001_source_manifest_sha256($manifest);
    return count($seen) === count($rows) && $actual !== false &&
        hash_equals($actual, $manifest['manifest_sha256']);
}
?>
