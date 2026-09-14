<?php
/**
 * HRM-FND-007 permissioned Work Authorization expiry-attention projection.
 *
 * This is a read-only candidate feed, not a notification sender. It reuses the
 * accepted readiness decision and exposes only bounded identifiers/date metadata.
 */
if (!function_exists('get_hrm_work_authorization_readiness_as_of'))
    include_once($path_to_root.'/hrm/includes/db/work_authorization_readiness_db.inc');
if (!function_exists('hrm_work_authorization_user_can_view_masked'))
    include_once($path_to_root.'/hrm/includes/db/work_authorization_masked_reader_db.inc');

function hrm_work_authorization_expiry_attention_days($as_of, $valid_to)
{
    if (!hrm_work_authorization_validate_date($as_of, false)
        || !hrm_work_authorization_validate_date($valid_to, false)
        || $valid_to < $as_of)
        return false;
    $start = new DateTime($as_of.' 00:00:00');
    $end = new DateTime($valid_to.' 00:00:00');
    return (int)$start->diff($end)->format('%r%a');
}

/**
 * Return current Workers whose accepted readiness decision is within the
 * approved policy warning window. No email, queue, task, or audit side effect
 * is emitted by this reader.
 */
function get_hrm_work_authorization_expiry_attention_candidates($jurisdiction, $as_of)
{
    if (!hrm_work_authorization_user_can_view_masked()
        || !hrm_work_authorization_validate_jurisdiction($jurisdiction)
        || !hrm_work_authorization_validate_date($as_of, false))
        return false;

    $policy = get_hrm_work_authorization_policy_as_of($jurisdiction, $as_of);
    if (!is_array($policy) || empty($policy['policy_version_id'])
        || !isset($policy['expiry_warning_days']) || (int)$policy['expiry_warning_days'] < 1)
        return array();

    $types = isset($policy['authorization_types']) && is_array($policy['authorization_types'])
        ? $policy['authorization_types'] : array();
    if (!$types)
        return false;
    $escaped_types = array();
    foreach ($types as $type) {
        if (!hrm_work_authorization_validate_code($type, hrm_work_authorization_types()))
            return false;
        $escaped_types[] = db_escape($type);
    }

    $warning_end = (new DateTime($as_of.' 00:00:00'))
        ->modify('+'.(int)$policy['expiry_warning_days'].' days')->format('Y-m-d');
    $sql = 'SELECT DISTINCT a.worker_id FROM '.TB_PREF.'hrm_work_authorizations a '
        .'LEFT JOIN '.TB_PREF.'hrm_work_authorizations s ON s.predecessor_id=a.authorization_id '
        .'WHERE s.authorization_id IS NULL AND a.issuing_jurisdiction='.db_escape($jurisdiction)
        .' AND a.authorization_type IN ('.implode(',', $escaped_types).')'
        ." AND a.authorization_status='active' AND a.verification_status='verified'"
        .' AND a.valid_to IS NOT NULL AND a.valid_to>='.db_escape($as_of)
        .' AND a.valid_to<='.db_escape($warning_end)
        .' ORDER BY a.worker_id';
    $r = db_query($sql, 'could not resolve Work Authorization expiry-attention candidates');
    if (!$r)
        return false;

    $out = array();
    while ($row = db_fetch_assoc($r)) {
        $worker_id = isset($row['worker_id']) ? (int)$row['worker_id'] : 0;
        if ($worker_id <= 0)
            return false;
        $readiness = get_hrm_work_authorization_readiness_as_of($worker_id, $jurisdiction, $as_of);
        if (!is_array($readiness) || empty($readiness['ready'])
            || empty($readiness['expiry_attention_required']))
            continue;
        $valid_to = isset($readiness['valid_to']) ? $readiness['valid_to'] : null;
        $days = hrm_work_authorization_expiry_attention_days($as_of, $valid_to);
        if ($days === false || $days > (int)$policy['expiry_warning_days'])
            return false;
        $out[] = array(
            'worker_id'=>$worker_id,
            'authorization_id'=>(int)$readiness['authorization_id'],
            'policy_version_id'=>(int)$readiness['policy_version_id'],
            'policy_code'=>(string)$readiness['policy_code'],
            'valid_to'=>(string)$valid_to,
            'days_remaining'=>$days,
            'expiry_attention_required'=>true,
        );
    }
    return $out;
}

/**
 * Return the exact approved-policy jurisdictions effective on one date.
 *
 * More than one current approved policy for one jurisdiction is ambiguous and
 * fails closed. This list is policy-owned; no Worker/Employee identity field is
 * used to infer jurisdiction.
 */
function hrm_work_authorization_expiry_attention_jurisdictions($as_of)
{
    if (!hrm_work_authorization_user_can_view_masked()
        || !hrm_work_authorization_validate_date($as_of, false))
        return false;

    $sql = 'SELECT p.issuing_jurisdiction,COUNT(*) policy_count '
        .'FROM '.TB_PREF.'hrm_work_authorization_policy_versions p '
        .'LEFT JOIN '.TB_PREF.'hrm_work_authorization_policy_versions s '
        .'ON s.predecessor_id=p.policy_version_id '
        .'WHERE s.policy_version_id IS NULL AND p.policy_status=\'approved\' '
        .'AND p.effective_from<='.db_escape($as_of)
        .' AND (p.effective_to IS NULL OR p.effective_to>='.db_escape($as_of).') '
        .'GROUP BY p.issuing_jurisdiction ORDER BY p.issuing_jurisdiction';
    $r = db_query($sql, 'could not resolve Work Authorization expiry-attention policy jurisdictions');
    if (!$r)
        return false;

    $out = array();
    while ($row = db_fetch_assoc($r)) {
        $jurisdiction = isset($row['issuing_jurisdiction'])
            ? (string)$row['issuing_jurisdiction'] : '';
        if ((int)$row['policy_count'] !== 1
            || !hrm_work_authorization_validate_jurisdiction($jurisdiction))
            return false;
        $out[] = $jurisdiction;
    }
    return $out;
}

/**
 * Return a bounded cross-jurisdiction expiry-attention feed for authorized HR.
 *
 * This is still read-only and stateless. It is intentionally capped at 100
 * rows so the alert surface cannot become a bulk-export path.
 */
function get_hrm_work_authorization_expiry_attention_feed($as_of, $limit = 100)
{
    if (!hrm_work_authorization_user_can_view_masked()
        || !hrm_work_authorization_validate_date($as_of, false))
        return false;
    if (!is_int($limit) && !(is_string($limit) && preg_match('/^[1-9][0-9]*$/D', $limit)))
        return false;
    $limit = (int)$limit;
    if ($limit < 1 || $limit > 100)
        return false;

    $jurisdictions = hrm_work_authorization_expiry_attention_jurisdictions($as_of);
    if ($jurisdictions === false)
        return false;

    $out = array();
    foreach ($jurisdictions as $jurisdiction) {
        $rows = get_hrm_work_authorization_expiry_attention_candidates($jurisdiction, $as_of);
        if ($rows === false)
            return false;
        foreach ($rows as $row) {
            if (!is_array($row) || empty($row['expiry_attention_required']))
                return false;
            $row['issuing_jurisdiction'] = $jurisdiction;
            $out[] = $row;
        }
    }

    usort($out, function ($a, $b) {
        $days = (int)$a['days_remaining'] - (int)$b['days_remaining'];
        if ($days !== 0) return $days;
        $date = strcmp((string)$a['valid_to'], (string)$b['valid_to']);
        if ($date !== 0) return $date;
        return (int)$a['worker_id'] - (int)$b['worker_id'];
    });
    return array_slice($out, 0, $limit);
}

/**
 * Bounded count for the topbar alert category. False means fail closed.
 */
function count_hrm_work_authorization_expiry_attention($as_of)
{
    $rows = get_hrm_work_authorization_expiry_attention_feed($as_of, 100);
    return is_array($rows) ? count($rows) : false;
}
