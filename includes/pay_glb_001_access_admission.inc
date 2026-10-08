<?php
/**
 * PAY-GLB-001 S10. A strict, OFFLINE contract for future independently read
 * object/residency approval evidence. Does not load grants or authenticate
 * their issuer: SHA-256 is an integrity change detector, NOT a signature.
 * NO route may treat these records as authorization without a reviewed,
 * durable source and fresh per-request company/user/object evaluation.
 */
require_once __DIR__.'/pay_glb_001_authority.inc';

/** Compute deterministic integrity digest of an exact normalized admission record. */
function pay_glb_001_access_admission_sha256($admission)
{
    if (!is_array($admission)) return false;
    $value = $admission;
    unset($value['admission_sha256']);
    $json = json_encode($value, JSON_UNESCAPED_SLASHES);
    return $json === false ? false : hash('sha256', $json);
}

/**
 * Validate independently sourced approval evidence against the current
 * session-derived scope. This is a data CONTRACT, not authorization itself.
 * @return bool True only for an exact, in-scope, separately reviewed record.
 */
function pay_glb_001_access_admission_valid($scope, $trusted_grants, $admission)
{
    if (!pay_glb_001_context_valid($scope, 'calendar') ||
        pay_glb_001_server_scope($trusted_grants, $scope['as_of_utc']) !== $scope ||
        !pay_glb_001_exact_keys($admission, array('company_id','actor_user_id',
            'entity_codes','purpose','as_of_utc','policy_version','access_policy_sha256',
            'residency_policy_sha256','approval_source_sha256','reviewer_user_id',
            'approver_user_id','reviewed_at_utc','approved_at_utc','expires_at_utc',
            'decision','admission_sha256'))) return false;
    if (!isset($_SESSION['wa_current_user']) ||
        !($_SESSION['wa_current_user'] instanceof current_user) ||
        !is_int($_SESSION['wa_current_user']->user) ||
        $_SESSION['wa_current_user']->user <= 0 ||
        $_SESSION['wa_current_user']->logged_in() !== true ||
        $admission['actor_user_id'] !== $_SESSION['wa_current_user']->user ||
        !is_int($admission['company_id']) ||
        $admission['company_id'] !== $scope['company_id'] ||
        $admission['entity_codes'] !== $scope['entity_codes'] ||
        $admission['as_of_utc'] !== $scope['as_of_utc'] ||
        $admission['purpose'] !== 'PAY_GLB_001_INTERNAL_READ' ||
        $admission['policy_version'] !== 'GLB_OBJECT_RESIDENCY_V1' ||
        $admission['decision'] !== 'APPROVED' ||
        !pay_glb_001_sha256($admission['access_policy_sha256']) ||
        !pay_glb_001_sha256($admission['residency_policy_sha256']) ||
        !pay_glb_001_sha256($admission['approval_source_sha256']) ||
        !is_int($admission['reviewer_user_id']) ||
        !is_int($admission['approver_user_id']) ||
        $admission['reviewer_user_id'] <= 0 || $admission['approver_user_id'] <= 0 ||
        $admission['reviewer_user_id'] === $admission['approver_user_id'] ||
        $admission['reviewer_user_id'] === $admission['actor_user_id'] ||
        $admission['approver_user_id'] === $admission['actor_user_id'] ||
        !pay_glb_001_iso_utc($admission['reviewed_at_utc']) ||
        !pay_glb_001_iso_utc($admission['approved_at_utc']) ||
        !pay_glb_001_iso_utc($admission['expires_at_utc']) ||
        $admission['reviewed_at_utc'] > $admission['approved_at_utc'] ||
        $admission['approved_at_utc'] > $scope['as_of_utc'] ||
        $admission['expires_at_utc'] <= $scope['as_of_utc'] ||
        !pay_glb_001_sha256($admission['admission_sha256'])) return false;
    $actual = pay_glb_001_access_admission_sha256($admission);
    return $actual !== false && hash_equals($actual, $admission['admission_sha256']);
}
?>
