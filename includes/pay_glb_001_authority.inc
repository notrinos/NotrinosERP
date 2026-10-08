<?php
/**
 * PAY-GLB-001 S07: fail-closed server-session AND independently reviewed
 * entity/residency scope bridge. Not a route or independent grant source.
 * $grants MUST come from a trusted server-side object ACL provider, never
 * $_REQUEST, the dashboard, or a provider payload. No entity ACL provider is
 * wired by this development slice; no production endpoint is enabled.
 * A future route must first run the normal session bootstrap, including
 * durable session/revocation checks; this pure bridge does not replace it.
 */
require_once __DIR__.'/pay_glb_001_contract.inc';

/** Build a scope only for a logged-in application user and trusted reviewed grants. */
function pay_glb_001_server_scope($grants, $as_of_utc)
{
    if (!pay_glb_001_iso_utc($as_of_utc) || !is_array($grants) ||
        count($grants) < 1 || count($grants) > 32 ||
        array_keys($grants) !== range(0, count($grants)-1)) return false;
    // Authorization is checked on the actual active application session, not
    // caller-provided flags or a serialized principal.
    if (!isset($_SESSION) || !is_array($_SESSION) ||
        !isset($_SESSION['wa_current_user']) ||
        !($_SESSION['wa_current_user'] instanceof current_user) ||
        !isset($_SESSION['wa_current_user']->company) ||
        !is_int($_SESSION['wa_current_user']->company) ||
        !function_exists('user_company') || !function_exists('user_check_access')) return false;
    try {
        $company = $_SESSION['wa_current_user']->company;
        if ($_SESSION['wa_current_user']->logged_in() !== true ||
            $company < 0 || user_company() !== $company ||
            !user_check_access('SA_HRMREPORTS') ||
            !user_check_access('SA_HRM_VIEW_PAY_REC')) return false;
    } catch (Throwable $e) { return false; }
    $codes = array(); $seen = array();
    foreach ($grants as $row) {
        // A reviewed, bounded object permission and residency approval is
        // required for EACH legal entity; never infer from a company-wide role.
        if (!pay_glb_001_exact_keys($row, array('company_id','entity_code',
            'object_access','residency_decision','purpose','review_evidence_sha256',
            'approved_until_utc')) ||
            !is_int($row['company_id']) || $row['company_id'] !== $company ||
            !pay_glb_001_code($row['entity_code']) ||
            $row['object_access'] !== 'ALLOW_VIEW_AGGREGATE' ||
            $row['residency_decision'] !== 'APPROVED' ||
            $row['purpose'] !== 'PAY_GLB_001_INTERNAL_READ' ||
            !pay_glb_001_sha256($row['review_evidence_sha256']) ||
            !pay_glb_001_iso_utc($row['approved_until_utc']) ||
            $row['approved_until_utc'] <= $as_of_utc ||
            isset($seen[$row['entity_code']])) return false;
        $seen[$row['entity_code']] = true;
        $codes[] = $row['entity_code'];
    }
    sort($codes, SORT_STRING);
    $scope = array('company_id'=>$company,'entity_codes'=>$codes,
        'purpose'=>'PAY_GLB_001_INTERNAL_READ','allow_rollup'=>true,
        'allow_calendar'=>true,'as_of_utc'=>$as_of_utc);
    return pay_glb_001_context_valid($scope, 'calendar') ? $scope : false;
}
?>
