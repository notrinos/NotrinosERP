<?php
/**
 * PAY-GLB-001 S13: bound, READ-ONLY authoritative legal-entity existence/country
 * check. This is NOT an ACL, a residency approval or a payroll data collector.
 * It uses only the current application's company-prefixed HRM entity table.
 * A default legacy entity without a reviewed country must fail closed.
 */
require_once __DIR__.'/pay_glb_001_authority.inc';

/** Verify an exact full set of currently active, identified HRM legal entities. */
function pay_glb_001_legal_entity_rows_valid($scope, $rows)
{
    if (!pay_glb_001_context_valid($scope, 'calendar') || !is_array($rows) ||
        count($rows) !== count($scope['entity_codes']) ||
        array_keys($rows) !== range(0, count($rows)-1)) return false;
    $seen = array(); $ids = array();
    foreach ($rows as $row) {
        if (!pay_glb_001_exact_keys($row, array('legal_entity_id','entity_code',
                'country_code','entity_status','source')) ||
            !is_int($row['legal_entity_id']) || $row['legal_entity_id'] <= 0 ||
            !pay_glb_001_code($row['entity_code']) ||
            !in_array($row['entity_code'], $scope['entity_codes'], true) ||
            isset($seen[$row['entity_code']]) ||
            isset($ids[$row['legal_entity_id']]) ||
            !is_string($row['country_code']) ||
            preg_match('/^[A-Z]{2}$/D', $row['country_code']) !== 1 ||
            $row['entity_status'] !== 'active' ||
            !is_string($row['source']) ||
            preg_match('/^[a-z][a-z0-9_]{0,31}$/D', $row['source']) !== 1 ||
            in_array($row['source'], array('fixture','synthetic','mock','test'), true)) return false;
        $seen[$row['entity_code']] = true;
        $ids[$row['legal_entity_id']] = true;
    }
    return count($seen) === count($scope['entity_codes']);
}

/**
 * Query the current company database only after independently supplied grants
 * have passed the existing session bridge. No request data is accepted.
 * No fallback to company preference or fabricated country/model is allowed.
 * @return array|false Verified minimal rows (no company/person identifiers), or false.
 */
function pay_glb_001_read_bound_legal_entities($scope, $trusted_grants)
{
    if (!is_array($scope) || !isset($scope['as_of_utc']) ||
        pay_glb_001_server_scope($trusted_grants, $scope['as_of_utc']) !== $scope ||
        !function_exists('db_query') || !function_exists('db_fetch_assoc') ||
        !function_exists('db_escape') || !defined('TB_PREF') ||
        !function_exists('user_company') ||
        user_company() !== $scope['company_id'] ||
        !isset($_SESSION['wa_current_user']->cur_con) ||
        $_SESSION['wa_current_user']->cur_con !== $scope['company_id']) return false;
    $in = array();
    foreach ($scope['entity_codes'] as $code) {
        if (!pay_glb_001_code($code)) return false;
        $in[] = db_escape($code);
    }
    if (!$in) return false;
    // No registration, tax, name, worker, salary or evidence bytes are selected.
    $sql = 'SELECT legal_entity_id,entity_code,country_code,entity_status,source FROM '.
        TB_PREF.'hrm_legal_entities WHERE entity_code IN ('.implode(',', $in).')';
    try {
        $result = db_query($sql, 'PAY-GLB-001 legal-entity binding query failed');
        if (!$result) return false;
        $rows = array();
        while (($r = db_fetch_assoc($result)) !== false) {
            if (count($rows) >= count($scope['entity_codes'])) return false;
            if (!isset($r['legal_entity_id']) ||
                !ctype_digit((string)$r['legal_entity_id'])) return false;
            $r['legal_entity_id'] = (int)$r['legal_entity_id'];
            $rows[] = $r;
        }
        return pay_glb_001_legal_entity_rows_valid($scope, $rows) ? $rows : false;
    } catch (Throwable $e) {
        return false;
    }
}
?>
