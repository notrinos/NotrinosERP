<?php
/**
 * PAY-GLB-001 S19: strictly non-authoritative candidate snapshot for a future
 * durable per-actor, per-company, per-entity access and residency service.
 * This function cannot authenticate a store, establish freshness/revocation
 * completeness, grant access, or serve a production request. A future adapter
 * must query durable server-owned ACL and revocations afresh on EVERY request.
 */
require_once __DIR__.'/pay_glb_001_access_admission.inc';

/** Check lease consistency against current-session admission without granting access. */
function pay_glb_001_acl_lease_candidate($scope, $grants, $admission, $leases)
{
    if (!pay_glb_001_access_admission_valid($scope, $grants, $admission) ||
        !is_array($leases) || count($leases) !== count($scope['entity_codes']) ||
        array_keys($leases) !== range(0, count($leases)-1)) return false;
    $by_code = array();
    foreach ($grants as $grant) $by_code[$grant['entity_code']] = $grant;
    $seen = array();
    foreach ($leases as $lease) {
        if (!pay_glb_001_exact_keys($lease, array('company_id','actor_user_id',
                'entity_code','purpose','object_decision','residency_decision',
                'access_policy_sha256','residency_policy_sha256',
                'review_evidence_sha256','ledger_revision','reviewer_user_id',
                'approver_user_id','approved_at_utc','expires_at_utc','revoked_at_utc')) ||
            !is_int($lease['company_id']) || $lease['company_id'] !== $scope['company_id'] ||
            !is_int($lease['actor_user_id']) || $lease['actor_user_id'] !== $admission['actor_user_id'] ||
            !pay_glb_001_code($lease['entity_code']) ||
            !isset($by_code[$lease['entity_code']]) || isset($seen[$lease['entity_code']]) ||
            $lease['purpose'] !== 'PAY_GLB_001_INTERNAL_READ' ||
            $lease['object_decision'] !== 'ALLOW_VIEW_AGGREGATE' ||
            $lease['residency_decision'] !== 'APPROVED' ||
            $lease['access_policy_sha256'] !== $admission['access_policy_sha256'] ||
            $lease['residency_policy_sha256'] !== $admission['residency_policy_sha256'] ||
            $lease['review_evidence_sha256'] !== $by_code[$lease['entity_code']]['review_evidence_sha256'] ||
            !is_int($lease['ledger_revision']) || $lease['ledger_revision'] <= 0 ||
            !is_int($lease['reviewer_user_id']) || !is_int($lease['approver_user_id']) ||
            $lease['reviewer_user_id'] !== $admission['reviewer_user_id'] ||
            $lease['approver_user_id'] !== $admission['approver_user_id'] ||
            !pay_glb_001_iso_utc($lease['approved_at_utc']) ||
            $lease['approved_at_utc'] !== $admission['approved_at_utc'] ||
            !pay_glb_001_iso_utc($lease['expires_at_utc']) ||
            $lease['expires_at_utc'] > $admission['expires_at_utc'] ||
            $lease['expires_at_utc'] > $by_code[$lease['entity_code']]['approved_until_utc'] ||
            $lease['expires_at_utc'] <= $scope['as_of_utc'] ||
            $lease['revoked_at_utc'] !== null) return false;
        $seen[$lease['entity_code']] = true;
    }
    if (count($seen) !== count($scope['entity_codes'])) return false;
    // Deliberately never returns a boolean authorization success or session token.
    return array('candidate_consistent'=>true,'durable_ledger_verified'=>false,
        'revocation_completeness_verified'=>false,'authorize_request'=>false,
        'company_id'=>$scope['company_id'],'entity_count'=>count($seen));
}
?>
