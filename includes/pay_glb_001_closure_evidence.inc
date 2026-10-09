<?php
/**
 * PAY-GLB-001 S16 — non-authoritative closure-evidence handoff inventory.
 * Hashes are change detectors, never signatures, grant decisions, or proof that
 * an external issuer, source collector or human has actually been admitted.
 * No live closure evidence is installed, read from disk, or persisted here.
 */
require_once __DIR__.'/pay_glb_001_contract.inc';

/** Exact, deliberately non-extensible requirements for the Phase-7 closure handoff. */
function pay_glb_001_closure_requirements()
{
    return array('DURABLE_OBJECT_ACL_AND_RESIDENCY',
        'PINNED_REVOKABLE_TRUST_KEYS',
        'LEGAL_ENTITY_MODEL_APPROVAL',
        'TRANSACTIONAL_PROVENANCE_COLLECTOR',
        'AUTHENTICATED_DASHBOARD_DENIAL_TESTS',
        'OWNER_WAMP_RECOVERY_REHEARSAL',
        'INDEPENDENT_BUSINESS_SECURITY_LEGAL_SIGNOFF');
}

/**
 * Accept only minimally described UNVERIFIED candidate references. No field
 * can assert verified/approved/closed or carry PII, credentials, raw provider
 * responses or operator claims. This is an inventory, not an evidence verifier.
 * @return array|false Canonical non-sensitive checklist, or fail closed.
 */
function pay_glb_001_closure_inventory($scope, $candidates)
{
    if (!pay_glb_001_context_valid($scope, 'calendar') || !is_array($candidates) ||
        count($candidates) > count(pay_glb_001_closure_requirements()) ||
        (count($candidates) && array_keys($candidates) !== range(0, count($candidates)-1))) return false;
    $seen = array();
    $requirements = pay_glb_001_closure_requirements();
    foreach ($candidates as $candidate) {
        if (!pay_glb_001_exact_keys($candidate, array('requirement_id',
                'reference_sha256', 'observed_at_utc', 'expires_at_utc', 'status')) ||
            !is_string($candidate['requirement_id']) ||
            !in_array($candidate['requirement_id'], $requirements, true) ||
            isset($seen[$candidate['requirement_id']]) ||
            $candidate['status'] !== 'UNVERIFIED_CANDIDATE' ||
            !pay_glb_001_sha256($candidate['reference_sha256']) ||
            !pay_glb_001_iso_utc($candidate['observed_at_utc']) ||
            !pay_glb_001_iso_utc($candidate['expires_at_utc']) ||
            $candidate['observed_at_utc'] > $scope['as_of_utc'] ||
            $candidate['expires_at_utc'] <= $scope['as_of_utc'] ||
            $candidate['observed_at_utc'] >= $candidate['expires_at_utc']) return false;
        $seen[$candidate['requirement_id']] = true;
    }
    // No arbitrary source-supplied "PASS" strings or identifiers escape.
    $checklist = array();
    foreach ($requirements as $requirement) {
        $checklist[] = array('requirement_id'=>$requirement,
            'unverified_candidate_present'=>isset($seen[$requirement]),
            'externally_verified'=>false);
    }
    return $checklist;
}
?>
