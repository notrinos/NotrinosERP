<?php
/**
 * HRM-FND-007 restricted read-only Work Authorization retention review.
 *
 * This projection never authorizes deletion. It exposes whether the approved
 * retention floor has elapsed, whether any PAY-AUD legal hold is active, and
 * whether linked evidence is retention-locked so a records reviewer can make a
 * later explicit disposition decision under a separately approved command.
 */
if (!function_exists('get_hrm_work_authorization_retention_policy_as_of'))
    include_once($path_to_root.'/hrm/includes/db/work_authorization_retention_db.inc');

function hrm_work_authorization_retention_review_state($row, $policy, $hold, $as_of)
{
    if (!is_array($row) || !is_array($policy) || !is_array($hold)
        || !hrm_work_authorization_validate_date($as_of, false))
        return false;
    if (empty($row['evidence_document_linked']) || empty($row['evidence_retention_locked']))
        return 'evidence_retention_incomplete';
    if (!empty($hold['legal_hold_active']))
        return 'legal_hold_active';
    if (empty($row['valid_to']))
        return 'open_ended_authorization';
    $retained_until = hrm_work_authorization_retained_until(
        (string)$row['valid_to'], (int)$policy['retention_days']);
    if ($retained_until === false)
        return false;
    return $as_of <= $retained_until
        ? 'retention_period_active'
        : 'retention_floor_elapsed_review_required';
}

/**
 * Return a bounded cross-worker retention-review feed.
 *
 * No permit/sponsor/restriction plaintext, document identifier, recipient data,
 * mutation command, or bulk export is exposed. Missing/ambiguous policy or
 * governance state fails closed for the complete feed.
 */
function get_hrm_work_authorization_retention_review_feed($as_of, $limit = 100)
{
    if (!hrm_work_authorization_user_can_view_masked()
        || !hrm_work_authorization_validate_date($as_of, false))
        return false;
    if (!is_int($limit) && !(is_string($limit) && preg_match('/^[1-9][0-9]*$/D', $limit)))
        return false;
    $limit = (int)$limit;
    if ($limit < 1 || $limit > 100)
        return false;

    $sql = 'SELECT a.authorization_id,a.worker_id,a.authorization_type,a.issuing_jurisdiction,'
        .'a.authorization_status,a.verification_status,a.valid_from,a.valid_to,'
        .'a.employee_document_id,d.retention_locked evidence_retention_locked '
        .'FROM '.TB_PREF.'hrm_work_authorizations a '
        .'LEFT JOIN '.TB_PREF.'hrm_work_authorizations s ON s.predecessor_id=a.authorization_id '
        .'LEFT JOIN '.TB_PREF.'employee_documents d ON d.doc_id=a.employee_document_id '
        .'WHERE s.authorization_id IS NULL '
        .'ORDER BY a.worker_id,a.authorization_type,a.issuing_jurisdiction,a.authorization_id LIMIT 101';
    $r = db_query($sql, 'could not read Work Authorization retention-review candidates');
    if (!$r) return false;

    $out = array(); $chains = array();
    while ($row = db_fetch_assoc($r)) {
        if (count($out) >= $limit)
            break;
        $worker_id = isset($row['worker_id']) ? (int)$row['worker_id'] : 0;
        $authorization_id = isset($row['authorization_id']) ? (int)$row['authorization_id'] : 0;
        $jurisdiction = isset($row['issuing_jurisdiction']) ? (string)$row['issuing_jurisdiction'] : '';
        $type = isset($row['authorization_type']) ? (string)$row['authorization_type'] : '';
        $metadata = array(
            'authorization_type'=>$type,'issuing_jurisdiction'=>$jurisdiction,
            'authorization_status'=>isset($row['authorization_status'])?(string)$row['authorization_status']:'',
            'verification_status'=>isset($row['verification_status'])?(string)$row['verification_status']:'',
            'valid_from'=>isset($row['valid_from'])?(string)$row['valid_from']:'',
            'valid_to'=>$row['valid_to']===null?null:(string)$row['valid_to']);
        if ($worker_id <= 0 || $authorization_id <= 0 || !hrm_work_authorization_validate_metadata($metadata))
            return false;
        $chain = $worker_id.'|'.$type.'|'.$jurisdiction;
        if (isset($chains[$chain])) return false;
        $chains[$chain] = true;

        $policy = get_hrm_work_authorization_retention_policy_as_of($jurisdiction, $as_of);
        $hold = hrm_work_authorization_legal_hold_state($worker_id, $as_of);
        if (!is_array($policy) || !is_array($hold)) return false;
        $review_row = array(
            'authorization_id'=>$authorization_id,'worker_id'=>$worker_id,
            'authorization_type'=>$type,'issuing_jurisdiction'=>$jurisdiction,
            'authorization_status'=>$metadata['authorization_status'],
            'verification_status'=>$metadata['verification_status'],
            'valid_to'=>$metadata['valid_to'],
            'retention_policy_version_id'=>(int)$policy['retention_policy_version_id'],
            'retention_policy_code'=>(string)$policy['policy_code'],
            'retention_days'=>(int)$policy['retention_days'],
            'evidence_document_linked'=>$row['employee_document_id'] !== null,
            'evidence_retention_locked'=>$row['employee_document_id'] !== null
                && isset($row['evidence_retention_locked']) && (int)$row['evidence_retention_locked'] === 1,
            'governance_scope_count'=>(int)$hold['scope_count'],
            'active_hold_count'=>(int)$hold['active_hold_count'],
            'disposition_authorized'=>false);
        $review_row['retained_until'] = $metadata['valid_to'] === null ? null
            : hrm_work_authorization_retained_until($metadata['valid_to'], (int)$policy['retention_days']);
        $state = hrm_work_authorization_retention_review_state($review_row, $policy, $hold, $as_of);
        if ($state === false) return false;
        $review_row['review_state'] = $state;
        $out[] = $review_row;
    }
    return $out;
}
