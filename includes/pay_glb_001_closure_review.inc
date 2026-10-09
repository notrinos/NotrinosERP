<?php
/**
 * PAY-GLB-001 S17 — deterministic, non-routable development closure review.
 * Even a complete inventory and valid synthetic preflight cannot establish
 * real provenance, server trust, live permissions, transactional data, a live
 * dashboard, or independent human acceptance. Therefore close always DENIES.
 */
require_once __DIR__.'/pay_glb_001_preflight.inc';
require_once __DIR__.'/pay_glb_001_closure_evidence.inc';

/**
 * Recheck current session/preflight and report externally unverified blockers.
 * Never substitute candidate receipt hashes for independent evidence admission.
 * @return array|false Minimal non-authoritative summary; no route or closure grant.
 */
function pay_glb_001_closure_review($grants, $as_of_utc, $admission,
    $entities, $batch, $manifest, $signed, $server_trusted_keys, $candidates)
{
    $prepared = pay_glb_001_preflight($grants, $as_of_utc, $admission,
        $entities, $batch, $manifest, $signed, $server_trusted_keys);
    if ($prepared === false || $prepared['preparation_valid'] !== true ||
        $prepared['close_ready'] !== false || $prepared['production_route_allowed'] !== false) return false;
    // Recompute scope, never consume request-submitted or previous-session scope.
    $scope = pay_glb_001_server_scope($grants, $as_of_utc);
    if ($scope === false) return false;
    $checklist = pay_glb_001_closure_inventory($scope, $candidates);
    if ($checklist === false) return false;
    $missing = array();
    foreach ($checklist as $entry) {
        if (!$entry['unverified_candidate_present']) $missing[] = $entry['requirement_id'];
    }
    // A hash of a checklist is an integrity change detector, NOT an admission.
    $json = json_encode($checklist, JSON_UNESCAPED_SLASHES);
    if ($json === false) return false;
    return array('state'=>'BLOCKED_EXTERNAL_ADMISSION', 'close_ready'=>false,
        'production_route_allowed'=>false, 'provider_execution_allowed'=>false,
        'evidence_authenticity_verified'=>false, 'read_only'=>true,
        'source_db_version'=>PAY_GLB_001_SOURCE_VERSION,
        'company_id'=>$scope['company_id'],
        'unverified_candidate_count'=>count($checklist)-count($missing),
        'missing_candidate_requirements'=>$missing,
        'still_unverified_requirements'=>pay_glb_001_closure_requirements(),
        'checklist_sha256'=>hash('sha256',$json));
}
?>
