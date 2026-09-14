<?php
/** HRM-FND-007 permissioned metadata-only Work Authorization reader. */
if (!function_exists('hrm_work_authorization_validate_date'))
    include_once($path_to_root.'/hrm/includes/db/work_authorization_db.inc');
if (!function_exists('hrm_user_can_access_sensitive_field'))
    include_once($path_to_root.'/hrm/includes/hrm_security.inc');

function hrm_work_authorization_user_can_view_masked()
{
    return function_exists('user_check_access')
        && hrm_user_can_access_sensitive_field(
            HRM_FIELD_RESTRICTED_WORK_AUTHORIZATION,
            HRM_FIELD_ACTION_VIEW
        );
}

/** Pure date-window interpretation only; this is never an eligibility decision. */
function hrm_work_authorization_validity_state($valid_from, $valid_to, $as_of)
{
    if (!hrm_work_authorization_validate_date($valid_from, false)
        || !hrm_work_authorization_validate_date($valid_to, true)
        || !hrm_work_authorization_validate_date($as_of, false)
        || ($valid_to !== null && $valid_to !== '' && $valid_to < $valid_from))
        return false;
    if ($as_of < $valid_from) return 'not_yet_valid';
    if ($valid_to !== null && $valid_to !== '' && $as_of > $valid_to) return 'outside_validity_window';
    return 'within_validity_window';
}

function hrm_work_authorization_masked_worker_id($employee_ref)
{
    if (is_int($employee_ref) || (is_string($employee_ref) && preg_match('/^[1-9][0-9]*$/D', $employee_ref))) {
        $column = 'employee_number'; $value = (int)$employee_ref;
    } elseif (is_string($employee_ref) && trim($employee_ref) !== '' && strlen($employee_ref) <= 64) {
        $column = 'employee_id'; $value = trim($employee_ref);
    } else return false;
    $r = db_query('SELECT worker_id FROM '.TB_PREF.'hrm_employee_worker_map WHERE '.$column.'='.db_escape($value)
        .' AND retired_at IS NULL ORDER BY mapping_id DESC LIMIT 2', 'could not resolve masked Work Authorization Worker');
    if (!$r || db_num_rows($r) !== 1) return false;
    $row = db_fetch_assoc($r);
    return isset($row['worker_id']) && (int)$row['worker_id'] > 0 ? (int)$row['worker_id'] : false;
}

/**
 * Return current chain leaves with no ciphertext/plaintext detail fields.
 * Record/object scope remains the calling HRM surface's responsibility.
 */
function get_hrm_work_authorizations_masked_as_of($employee_ref, $as_of)
{
    if (!hrm_work_authorization_user_can_view_masked()
        || !hrm_work_authorization_validate_date($as_of, false))
        return false;
    $worker_id = hrm_work_authorization_masked_worker_id($employee_ref);
    if ($worker_id === false) return false;

    $sql = 'SELECT a.authorization_id,a.version_no,a.predecessor_id,a.authorization_type,'
        .'a.issuing_jurisdiction,a.authorization_status,a.valid_from,a.valid_to,a.masked_identifier,'
        .'a.verification_status,a.verified_at,a.employee_document_id,a.created_at '
        .'FROM '.TB_PREF.'hrm_work_authorizations a '
        .'LEFT JOIN '.TB_PREF.'hrm_work_authorizations s ON s.predecessor_id=a.authorization_id '
        .'WHERE a.worker_id='.(int)$worker_id.' AND s.authorization_id IS NULL '
        .'ORDER BY a.authorization_type,a.issuing_jurisdiction,a.version_no,a.authorization_id';
    $r = db_query($sql, 'could not read masked Work Authorization versions');
    if (!$r) return false;
    $rows = array(); $chains = array();
    while ($row = db_fetch_assoc($r)) {
        $metadata = array(
            'authorization_type'=>(string)$row['authorization_type'],
            'issuing_jurisdiction'=>(string)$row['issuing_jurisdiction'],
            'authorization_status'=>(string)$row['authorization_status'],
            'verification_status'=>(string)$row['verification_status'],
            'valid_from'=>(string)$row['valid_from'],
            'valid_to'=>$row['valid_to'] === null ? null : (string)$row['valid_to'],
        );
        if (!hrm_work_authorization_validate_metadata($metadata)) return false;
        $chain = (string)$row['authorization_type'].'|'.(string)$row['issuing_jurisdiction'];
        if (isset($chains[$chain])) return false;
        $chains[$chain] = true;
        $state = hrm_work_authorization_validity_state(
            (string)$row['valid_from'], $row['valid_to'] === null ? null : (string)$row['valid_to'], $as_of);
        if ($state === false) return false;
        $rows[] = array(
            'authorization_id'=>(int)$row['authorization_id'],
            'version_no'=>(int)$row['version_no'],
            'predecessor_id'=>$row['predecessor_id'] === null ? null : (int)$row['predecessor_id'],
            'authorization_type'=>(string)$row['authorization_type'],
            'issuing_jurisdiction'=>(string)$row['issuing_jurisdiction'],
            'authorization_status'=>(string)$row['authorization_status'],
            'verification_status'=>(string)$row['verification_status'],
            'valid_from'=>(string)$row['valid_from'],
            'valid_to'=>$row['valid_to'] === null ? null : (string)$row['valid_to'],
            'validity_state'=>$state,
            'masked_identifier'=>(string)$row['masked_identifier'],
            'verified_at'=>$row['verified_at'] === null ? null : (string)$row['verified_at'],
            'evidence_document_linked'=>$row['employee_document_id'] !== null,
        );
    }
    return $rows;
}
