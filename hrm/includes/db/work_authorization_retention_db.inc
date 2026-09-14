<?php
/**
 * HRM-FND-007 Work Authorization retention/legal-hold policy custody.
 *
 * Retention duration is owned by an explicitly approved jurisdiction policy.
 * PAY-AUD immutable governance remains the legal-hold source. This file exposes
 * no deletion, anonymization, document mutation, policy writer, or role grant.
 */
if (!function_exists('hrm_work_authorization_validate_jurisdiction'))
    include_once($path_to_root.'/hrm/includes/db/work_authorization_db.inc');
if (!function_exists('hrm_work_authorization_user_can_view_masked'))
    include_once($path_to_root.'/hrm/includes/db/work_authorization_masked_reader_db.inc');
if (!function_exists('domain_audit_governance_state'))
    include_once($path_to_root.'/includes/db/domain_audit_governance.inc');

function hrm_work_authorization_retention_policy_statuses()
{
    return array('draft','approved','retired');
}

function hrm_work_authorization_retention_policy_validate_code($value)
{
    return is_string($value)
        && preg_match('/^[a-z0-9][a-z0-9._-]{2,63}$/D', $value) === 1;
}

function hrm_work_authorization_retention_policy_validate_ref($value)
{
    return is_string($value)
        && preg_match('/^[A-Za-z0-9][A-Za-z0-9._:\/-]{2,127}$/D', $value) === 1;
}

/** Validate one immutable retention-policy version. */
function hrm_work_authorization_retention_policy_validate_metadata($row)
{
    $allowed = array('policy_code','version_no','predecessor_id','issuing_jurisdiction',
        'retention_days','anchor_event','policy_status','effective_from','effective_to',
        'qualified_review_ref','approved_at','approved_by');
    if (!is_array($row) || array_diff(array_keys($row), $allowed))
        return false;

    $version_no = isset($row['version_no']) ? (int)$row['version_no'] : 0;
    $predecessor_id = isset($row['predecessor_id']) && $row['predecessor_id'] !== null
        ? (int)$row['predecessor_id'] : null;
    $days = isset($row['retention_days']) ? (int)$row['retention_days'] : 0;
    $status = isset($row['policy_status']) ? (string)$row['policy_status'] : '';
    $approved_at = isset($row['approved_at']) ? $row['approved_at'] : null;
    $approved_by = isset($row['approved_by']) && $row['approved_by'] !== null
        ? (int)$row['approved_by'] : null;
    $review_ref = isset($row['qualified_review_ref']) ? (string)$row['qualified_review_ref'] : '';

    if (!hrm_work_authorization_retention_policy_validate_code(isset($row['policy_code']) ? $row['policy_code'] : null)
        || $version_no <= 0 || ($predecessor_id !== null && $predecessor_id <= 0)
        || !hrm_work_authorization_validate_jurisdiction(isset($row['issuing_jurisdiction']) ? $row['issuing_jurisdiction'] : null)
        || $days < 1 || $days > 36500
        || (isset($row['anchor_event']) ? (string)$row['anchor_event'] : '') !== 'authorization_valid_to'
        || !in_array($status, hrm_work_authorization_retention_policy_statuses(), true)
        || !hrm_work_authorization_validate_date(isset($row['effective_from']) ? $row['effective_from'] : null, false)
        || !hrm_work_authorization_validate_date(isset($row['effective_to']) ? $row['effective_to'] : null, true)
        || (!empty($row['effective_to']) && $row['effective_to'] < $row['effective_from']))
        return false;

    if ($status === 'approved') {
        if (!hrm_work_authorization_retention_policy_validate_ref($review_ref)
            || !is_string($approved_at)
            || preg_match('/^\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}$/D', $approved_at) !== 1
            || (($dt = DateTime::createFromFormat('!Y-m-d H:i:s', $approved_at)) === false)
            || $dt->format('Y-m-d H:i:s') !== $approved_at
            || $approved_by === null || $approved_by <= 0)
            return false;
    } elseif ($approved_at !== null || $approved_by !== null) {
        return false;
    }
    return true;
}

/** Resolve exactly one approved current retention policy for jurisdiction/date. */
function get_hrm_work_authorization_retention_policy_as_of($jurisdiction, $as_of)
{
    if (!hrm_work_authorization_validate_jurisdiction($jurisdiction)
        || !hrm_work_authorization_validate_date($as_of, false))
        return false;
    $sql = 'SELECT p.retention_policy_version_id,p.policy_code,p.version_no,p.predecessor_id,'
        .'p.issuing_jurisdiction,p.retention_days,p.anchor_event,p.policy_status,p.effective_from,'
        .'p.effective_to,p.qualified_review_ref,p.approved_at,p.approved_by '
        .'FROM '.TB_PREF.'hrm_work_authorization_retention_policy_versions p '
        .'LEFT JOIN '.TB_PREF.'hrm_work_authorization_retention_policy_versions s '
        .'ON s.predecessor_id=p.retention_policy_version_id '
        .'WHERE s.retention_policy_version_id IS NULL AND p.issuing_jurisdiction='.db_escape($jurisdiction)
        ." AND p.policy_status='approved' AND p.effective_from<=".db_escape($as_of)
        .' AND (p.effective_to IS NULL OR p.effective_to>='.db_escape($as_of).') '
        .'ORDER BY p.policy_code,p.version_no,p.retention_policy_version_id LIMIT 2';
    $r = db_query($sql, 'could not resolve Work Authorization retention policy');
    if (!$r || db_num_rows($r) !== 1)
        return false;
    $row = db_fetch_assoc($r);
    $metadata = array(
        'policy_code'=>(string)$row['policy_code'],'version_no'=>(int)$row['version_no'],
        'predecessor_id'=>$row['predecessor_id'] === null ? null : (int)$row['predecessor_id'],
        'issuing_jurisdiction'=>(string)$row['issuing_jurisdiction'],
        'retention_days'=>(int)$row['retention_days'],'anchor_event'=>(string)$row['anchor_event'],
        'policy_status'=>(string)$row['policy_status'],'effective_from'=>(string)$row['effective_from'],
        'effective_to'=>$row['effective_to'] === null ? null : (string)$row['effective_to'],
        'qualified_review_ref'=>(string)$row['qualified_review_ref'],
        'approved_at'=>$row['approved_at'] === null ? null : (string)$row['approved_at'],
        'approved_by'=>$row['approved_by'] === null ? null : (int)$row['approved_by']);
    if (!hrm_work_authorization_retention_policy_validate_metadata($metadata))
        return false;
    $metadata['retention_policy_version_id'] = (int)$row['retention_policy_version_id'];
    return $metadata;
}

/** Bounded legal-entity scopes owned by normalized Employment plus company scope 0. */
function hrm_work_authorization_retention_legal_entity_scopes($worker_id)
{
    $worker_id = (int)$worker_id;
    if ($worker_id <= 0) return false;
    $r = db_query('SELECT DISTINCT legal_entity_id FROM '.TB_PREF.'hrm_employments WHERE worker_id='.(int)$worker_id
        .' ORDER BY legal_entity_id LIMIT 33', 'could not resolve Work Authorization retention legal-entity scopes');
    if (!$r) return false;
    $out = array(0 => true);
    while ($row = db_fetch_assoc($r)) {
        $id = isset($row['legal_entity_id']) ? (int)$row['legal_entity_id'] : 0;
        if ($id <= 0 || isset($out[$id])) return false;
        $out[$id] = true;
    }
    if (count($out) > 33) return false;
    return array_keys($out);
}

/** Return only legal-hold state; PAY-AUD retention duration does not override the dedicated policy. */
function hrm_work_authorization_legal_hold_state($worker_id, $as_of)
{
    if (!hrm_work_authorization_validate_date($as_of, false)) return false;
    $scopes = hrm_work_authorization_retention_legal_entity_scopes($worker_id);
    if ($scopes === false) return false;
    $company = function_exists('user_company') ? (int)user_company() : 0;
    $timestamp = $as_of.' 23:59:59';
    $hold_count = 0;
    foreach ($scopes as $legal_entity_id) {
        $state = domain_audit_governance_state($company, (int)$legal_entity_id, $timestamp);
        if (!is_array($state) || empty($state['ok'])) return false;
        $hold_count += isset($state['active_hold_count']) ? (int)$state['active_hold_count'] : 0;
    }
    return array('scope_count'=>count($scopes),'active_hold_count'=>$hold_count,
        'legal_hold_active'=>$hold_count > 0);
}

/** Deterministic calendar retention floor; this does not authorize disposal. */
function hrm_work_authorization_retained_until($valid_to, $retention_days)
{
    if (!hrm_work_authorization_validate_date($valid_to, false)) return false;
    $retention_days = (int)$retention_days;
    if ($retention_days < 1 || $retention_days > 36500) return false;
    $dt = DateTime::createFromFormat('!Y-m-d', $valid_to, new DateTimeZone('UTC'));
    return $dt === false ? false : $dt->modify('+'.$retention_days.' days')->format('Y-m-d');
}
