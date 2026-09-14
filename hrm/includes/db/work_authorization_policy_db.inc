<?php
/**
 * HRM-FND-007 Work Authorization approved policy-pack custody.
 *
 * Policy metadata is intentionally non-sensitive and contains no legacy
 * identity fields, individual identifiers, or free-form restriction data.
 * A policy pack is only eligible for readiness evaluation after explicit
 * qualified-review approval. This file exposes no policy writer or UI route.
 */
if (!function_exists('hrm_work_authorization_types'))
    include_once($path_to_root.'/hrm/includes/db/work_authorization_db.inc');

/** @return array */
function hrm_work_authorization_policy_statuses()
{
    return array('draft','approved','retired');
}

/**
 * Validate a stable policy code.
 *
 * @param mixed $value
 * @return bool
 */
function hrm_work_authorization_policy_validate_code($value)
{
    return is_string($value)
        && preg_match('/^[a-z0-9][a-z0-9._-]{2,63}$/D', $value) === 1;
}

/**
 * Validate a qualified-review reference without accepting free-form prose.
 *
 * @param mixed $value
 * @return bool
 */
function hrm_work_authorization_policy_validate_review_ref($value)
{
    return is_string($value)
        && preg_match('/^[A-Za-z0-9][A-Za-z0-9._:\/-]{2,127}$/D', $value) === 1;
}

/**
 * Canonicalize the authorization-type allowlist stored in a policy pack.
 *
 * The list is a set of alternatives. Ordering is canonicalized only for
 * deterministic custody and has no preference semantics.
 *
 * @param mixed $value Array or JSON array
 * @return array|false
 */
function hrm_work_authorization_policy_canonical_types($value)
{
    if (is_string($value)) {
        $decoded = json_decode($value, true);
        if (!is_array($decoded))
            return false;
        $value = $decoded;
    }
    if (!is_array($value) || count($value) < 1 || count($value) > 8)
        return false;

    $out = array();
    foreach ($value as $type) {
        if (!hrm_work_authorization_validate_code($type, hrm_work_authorization_types()))
            return false;
        $out[$type] = true;
    }
    $out = array_keys($out);
    sort($out, SORT_STRING);
    return $out;
}

/**
 * Validate one policy-version metadata row.
 *
 * Approved rows require explicit qualified-review provenance and an approving
 * actor/time. Core readiness requirements are not configurable here.
 *
 * @param mixed $row
 * @return bool
 */
function hrm_work_authorization_policy_validate_metadata($row)
{
    $allowed = array('policy_code','version_no','predecessor_id','issuing_jurisdiction',
        'authorization_types_json','expiry_warning_days','policy_status',
        'effective_from','effective_to','qualified_review_ref','approved_at','approved_by');
    if (!is_array($row) || array_diff(array_keys($row), $allowed))
        return false;

    $version_no = isset($row['version_no']) ? (int)$row['version_no'] : 0;
    $predecessor_id = isset($row['predecessor_id']) && $row['predecessor_id'] !== null
        ? (int)$row['predecessor_id'] : null;
    $warning = isset($row['expiry_warning_days']) ? (int)$row['expiry_warning_days'] : -1;
    $status = isset($row['policy_status']) ? $row['policy_status'] : null;
    $review_ref = isset($row['qualified_review_ref']) ? (string)$row['qualified_review_ref'] : '';
    $approved_at = isset($row['approved_at']) ? $row['approved_at'] : null;
    $approved_by = isset($row['approved_by']) && $row['approved_by'] !== null
        ? (int)$row['approved_by'] : null;

    if (!hrm_work_authorization_policy_validate_code(isset($row['policy_code']) ? $row['policy_code'] : null)
        || $version_no <= 0 || ($predecessor_id !== null && $predecessor_id <= 0)
        || !hrm_work_authorization_validate_jurisdiction(
            isset($row['issuing_jurisdiction']) ? $row['issuing_jurisdiction'] : null)
        || hrm_work_authorization_policy_canonical_types(
            isset($row['authorization_types_json']) ? $row['authorization_types_json'] : null) === false
        || $warning < 0 || $warning > 365
        || !in_array($status, hrm_work_authorization_policy_statuses(), true)
        || !hrm_work_authorization_validate_date(
            isset($row['effective_from']) ? $row['effective_from'] : null, false)
        || !hrm_work_authorization_validate_date(
            isset($row['effective_to']) ? $row['effective_to'] : null, true)
        || (!empty($row['effective_to']) && $row['effective_to'] < $row['effective_from']))
        return false;

    if ($status === 'approved') {
        if (!hrm_work_authorization_policy_validate_review_ref($review_ref)
            || !is_string($approved_at)
            || preg_match('/^\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}$/D', $approved_at) !== 1
            || (($approved_dt = DateTime::createFromFormat('!Y-m-d H:i:s', $approved_at)) === false)
            || $approved_dt->format('Y-m-d H:i:s') !== $approved_at
            || $approved_by === null || $approved_by <= 0)
            return false;
    } elseif ($approved_at !== null || $approved_by !== null) {
        return false;
    }
    return true;
}

/**
 * Return one approved current policy leaf effective for a jurisdiction/date.
 *
 * Missing or overlapping approved policies fail closed. The returned row is
 * canonicalized and includes authorization_types as an array.
 *
 * @param string $jurisdiction
 * @param string $as_of SQL date
 * @return array|false
 */
function get_hrm_work_authorization_policy_as_of($jurisdiction, $as_of)
{
    if (!hrm_work_authorization_validate_jurisdiction($jurisdiction)
        || !hrm_work_authorization_validate_date($as_of, false))
        return false;

    $sql = 'SELECT p.policy_version_id,p.policy_code,p.version_no,p.predecessor_id,'
        .'p.issuing_jurisdiction,p.authorization_types_json,p.expiry_warning_days,'
        .'p.policy_status,p.effective_from,p.effective_to,p.qualified_review_ref,'
        .'p.approved_at,p.approved_by '
        .'FROM '.TB_PREF.'hrm_work_authorization_policy_versions p '
        .'LEFT JOIN '.TB_PREF.'hrm_work_authorization_policy_versions s '
        .'ON s.predecessor_id=p.policy_version_id '
        .'WHERE s.policy_version_id IS NULL '
        .'AND p.issuing_jurisdiction='.db_escape($jurisdiction)
        ." AND p.policy_status='approved'"
        .' AND p.effective_from<='.db_escape($as_of)
        .' AND (p.effective_to IS NULL OR p.effective_to>='.db_escape($as_of).') '
        .'ORDER BY p.policy_code,p.version_no,p.policy_version_id LIMIT 2';
    $result = db_query($sql, 'could not resolve Work Authorization readiness policy');
    if (!$result || db_num_rows($result) !== 1)
        return false;

    $row = db_fetch_assoc($result);
    $metadata = array(
        'policy_code'=>(string)$row['policy_code'],
        'version_no'=>(int)$row['version_no'],
        'predecessor_id'=>$row['predecessor_id'] === null ? null : (int)$row['predecessor_id'],
        'issuing_jurisdiction'=>(string)$row['issuing_jurisdiction'],
        'authorization_types_json'=>(string)$row['authorization_types_json'],
        'expiry_warning_days'=>(int)$row['expiry_warning_days'],
        'policy_status'=>(string)$row['policy_status'],
        'effective_from'=>(string)$row['effective_from'],
        'effective_to'=>$row['effective_to'] === null ? null : (string)$row['effective_to'],
        'qualified_review_ref'=>(string)$row['qualified_review_ref'],
        'approved_at'=>$row['approved_at'] === null ? null : (string)$row['approved_at'],
        'approved_by'=>$row['approved_by'] === null ? null : (int)$row['approved_by'],
    );
    if (!hrm_work_authorization_policy_validate_metadata($metadata))
        return false;
    $types = hrm_work_authorization_policy_canonical_types($metadata['authorization_types_json']);
    if ($types === false)
        return false;

    $metadata['policy_version_id'] = (int)$row['policy_version_id'];
    $metadata['authorization_types'] = $types;
    return $metadata;
}
