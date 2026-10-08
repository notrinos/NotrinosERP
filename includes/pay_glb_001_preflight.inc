<?php
/**
 * PAY-GLB-001 S15: NON-ROUTABLE acceptance preflight. The only positive result
 * is that pure, independently supplied preparation is internally consistent.
 * It can never authorize a real dashboard until an audited, server-pinned
 * trust store, persistent ACL service, and trusted collector are integrated.
 */
require_once __DIR__.'/pay_glb_001_access_admission.inc';
require_once __DIR__.'/pay_glb_001_source_admission.inc';
require_once __DIR__.'/pay_glb_001_legal_entity_bridge.inc';
require_once __DIR__.'/pay_glb_001_signed_evidence.inc';

/** Validate preparation while retaining unconditional route, provider and close denials. */
function pay_glb_001_preflight($grants, $as_of_utc, $admission, $entities,
    $batch, $manifest, $signed, $server_trusted_keys)
{
    $scope = pay_glb_001_server_scope($grants, $as_of_utc);
    if ($scope === false ||
        !pay_glb_001_access_admission_valid($scope, $grants, $admission) ||
        !pay_glb_001_legal_entity_rows_valid($scope, $entities) ||
        !pay_glb_001_source_manifest_valid($scope, $batch, $manifest) ||
        !pay_glb_001_signed_bundle_valid($scope, $admission, $manifest,
            $signed, $server_trusted_keys)) return false;
    // Bind the real legal-entity country's approved row to every source row;
    // no foreign-country or synthetic mapping can pass even if correctly signed.
    $countries = array();
    foreach ($entities as $entity) $countries[$entity['entity_code']] = $entity['country_code'];
    foreach ($batch['rollup_rows'] as $row)
        if (!isset($countries[$row['entity_code']]) ||
            $countries[$row['entity_code']] !== $row['country_code']) return false;
    // Deliberately excludes HTML and any 'activation_allowed' true branch.
    return array('preparation_valid'=>true,'production_route_allowed'=>false,
        'provider_execution_allowed'=>false, 'close_ready'=>false,
        'company_id'=>$scope['company_id'], 'entity_count'=>count($entities),
        'read_only'=>true,'source_db_version'=>PAY_GLB_001_SOURCE_VERSION);
}
?>
