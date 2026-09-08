<?php
/**
 * Restricted browser recovery for the fixed Employee Training Assignment.
 */
if (!defined('HRM_LIFECYCLE_TRAINING_ASSIGNMENT_BROWSER_SESSION_NAMESPACE'))
    define('HRM_LIFECYCLE_TRAINING_ASSIGNMENT_BROWSER_SESSION_NAMESPACE', 'hrm_lifecycle_training_assignment_idempotency');

/** @param mixed $employee_id @return string|false */
function hrm_lifecycle_training_assignment_browser_employee_id($employee_id)
{
    $employee_id = trim((string)$employee_id);
    if ($employee_id === '' || strlen($employee_id) > 20
        || preg_match('/^[A-Za-z0-9_-]+$/D', $employee_id) !== 1)
        return false;
    return $employee_id;
}

/** @param string $employee_id @return string */
function hrm_lifecycle_training_assignment_browser_session_slot($employee_id)
{
    return hash('sha256', 'employee_training_assignment|'.(string)$employee_id);
}

/** @param string $employee_id @return string|false */
function hrm_lifecycle_training_assignment_browser_idempotency_key($employee_id)
{
    $employee_id = hrm_lifecycle_training_assignment_browser_employee_id($employee_id);
    if ($employee_id === false || !isset($_SESSION) || !is_array($_SESSION))
        return false;
    if (!isset($_SESSION[HRM_LIFECYCLE_TRAINING_ASSIGNMENT_BROWSER_SESSION_NAMESPACE])
        || !is_array($_SESSION[HRM_LIFECYCLE_TRAINING_ASSIGNMENT_BROWSER_SESSION_NAMESPACE]))
        $_SESSION[HRM_LIFECYCLE_TRAINING_ASSIGNMENT_BROWSER_SESSION_NAMESPACE] = array();
    $slot = hrm_lifecycle_training_assignment_browser_session_slot($employee_id);
    if (isset($_SESSION[HRM_LIFECYCLE_TRAINING_ASSIGNMENT_BROWSER_SESSION_NAMESPACE][$slot])) {
        $key = $_SESSION[HRM_LIFECYCLE_TRAINING_ASSIGNMENT_BROWSER_SESSION_NAMESPACE][$slot];
        if (is_string($key) && strlen($key) === 64 && preg_match('/^[a-f0-9]{64}$/D', $key) === 1)
            return $key;
        unset($_SESSION[HRM_LIFECYCLE_TRAINING_ASSIGNMENT_BROWSER_SESSION_NAMESPACE][$slot]);
    }
    $bytes = false;
    if (function_exists('random_bytes')) {
        try { $bytes = random_bytes(32); } catch (Exception $e) { $bytes = false; }
        catch (Throwable $e) { $bytes = false; }
    }
    if ($bytes === false && function_exists('openssl_random_pseudo_bytes')) {
        $strong = false; $candidate = openssl_random_pseudo_bytes(32, $strong);
        if ($candidate !== false && $strong) $bytes = $candidate;
    }
    if ($bytes === false)
        return false;
    $key = bin2hex($bytes);
    $_SESSION[HRM_LIFECYCLE_TRAINING_ASSIGNMENT_BROWSER_SESSION_NAMESPACE][$slot] = $key;
    return $key;
}

/** @param string $employee_id @return void */
function hrm_lifecycle_training_assignment_browser_forget_idempotency_key($employee_id)
{
    $employee_id = hrm_lifecycle_training_assignment_browser_employee_id($employee_id);
    if ($employee_id === false || !isset($_SESSION[HRM_LIFECYCLE_TRAINING_ASSIGNMENT_BROWSER_SESSION_NAMESPACE])
        || !is_array($_SESSION[HRM_LIFECYCLE_TRAINING_ASSIGNMENT_BROWSER_SESSION_NAMESPACE]))
        return;
    unset($_SESSION[HRM_LIFECYCLE_TRAINING_ASSIGNMENT_BROWSER_SESSION_NAMESPACE][
        hrm_lifecycle_training_assignment_browser_session_slot($employee_id)]);
    if (!$_SESSION[HRM_LIFECYCLE_TRAINING_ASSIGNMENT_BROWSER_SESSION_NAMESPACE])
        unset($_SESSION[HRM_LIFECYCLE_TRAINING_ASSIGNMENT_BROWSER_SESSION_NAMESPACE]);
}

/** @param array $row @return bool */
function hrm_lifecycle_training_assignment_browser_pending_row_is_valid($row)
{
    if (!is_array($row) || (string)$row['command_status'] !== 'pending' || (int)$row['row_version'] !== 1
        || (int)$row['approval_draft_id'] <= 0 || (int)$row['approval_row_id'] !== (int)$row['approval_draft_id']
        || (int)$row['submitted_by'] !== (int)$row['requested_by'] || (int)$row['requested_by'] <= 0
        || !defined('APPROVAL_STATUS_PENDING') || (int)$row['approval_status'] !== (int)APPROVAL_STATUS_PENDING)
        return false;
    $types = array(
        'employee_transfer'=>defined('ST_HRM_LIFECYCLE_TRANSFER') ? ST_HRM_LIFECYCLE_TRANSFER : -1,
        'employee_separation'=>defined('ST_HRM_LIFECYCLE_SEPARATION') ? ST_HRM_LIFECYCLE_SEPARATION : -1,
        'employee_salary_revision'=>defined('ST_HRM_LIFECYCLE_SALARY_REVISION') ? ST_HRM_LIFECYCLE_SALARY_REVISION : -1,
        'employee_confirmation'=>defined('ST_HRM_LIFECYCLE_CONFIRMATION') ? ST_HRM_LIFECYCLE_CONFIRMATION : -1,
        'employee_probation_schedule'=>defined('ST_HRM_LIFECYCLE_PROBATION_SCHEDULE') ? ST_HRM_LIFECYCLE_PROBATION_SCHEDULE : -1,
        HRM_LIFECYCLE_TRAINING_ASSIGNMENT_COMMAND_TYPE=>defined('ST_HRM_LIFECYCLE_TRAINING_ASSIGNMENT')
            ? ST_HRM_LIFECYCLE_TRAINING_ASSIGNMENT : -1,
    );
    return isset($types[(string)$row['command_type']])
        && (int)$row['trans_type'] === (int)$types[(string)$row['command_type']];
}

/**
 * Return coarse own-command status without selecting draft payload or comments.
 *
 * @param string $employee_id
 * @return array
 */
function get_hrm_lifecycle_employee_training_assignment_browser_status($employee_id)
{
    $none = array('status'=>'none','own'=>false);
    $employee_id = hrm_lifecycle_training_assignment_browser_employee_id($employee_id);
    $actor = function_exists('hrm_lifecycle_training_assignment_actor_id')
        ? hrm_lifecycle_training_assignment_actor_id() : 0;
    if ($employee_id === false || $actor <= 0 || !hrm_lifecycle_training_assignment_permission()
        || !function_exists('get_hrm_person_worker_binding'))
        return array('status'=>'inconsistent','own'=>false);
    $binding = get_hrm_person_worker_binding($employee_id, false);
    if (!$binding || !empty($binding['retired_at']) || (int)$binding['worker_id'] <= 0)
        return $none;

    $result = db_query("SELECT c.command_type,c.command_status,c.approval_draft_id,c.row_version,c.requested_by,"
        ."d.id approval_row_id,d.trans_type,d.status approval_status,d.submitted_by FROM "
        .TB_PREF."hrm_lifecycle_commands c LEFT JOIN ".TB_PREF."approval_drafts d ON d.id=c.approval_draft_id "
        ."WHERE c.worker_id=".(int)$binding['worker_id']." AND c.command_status='pending' ORDER BY c.lifecycle_command_id DESC",
        'could not reconcile pending Employee Training Assignment');
    if (!$result)
        return array('status'=>'inconsistent','own'=>false);
    $pending = array();
    while ($row = db_fetch_assoc($result)) {
        if (!hrm_lifecycle_training_assignment_browser_pending_row_is_valid($row))
            return array('status'=>'inconsistent','own'=>false);
        $pending[] = $row;
    }
    if (count($pending) > 1)
        return array('status'=>'inconsistent','own'=>false);
    if (count($pending) === 1) {
        $row = $pending[0];
        if ((string)$row['command_type'] !== HRM_LIFECYCLE_TRAINING_ASSIGNMENT_COMMAND_TYPE
            || (int)$row['requested_by'] !== $actor)
            return array('status'=>'blocked','own'=>false);
        return array('status'=>'pending','own'=>true);
    }

    $result = db_query("SELECT c.command_status,c.approval_draft_id,c.row_version,c.requested_by,"
        ."d.id approval_row_id,d.trans_type,d.status approval_status,d.submitted_by FROM "
        .TB_PREF."hrm_lifecycle_commands c LEFT JOIN ".TB_PREF."approval_drafts d ON d.id=c.approval_draft_id "
        ."WHERE c.worker_id=".(int)$binding['worker_id']." AND c.command_type="
        .db_escape(HRM_LIFECYCLE_TRAINING_ASSIGNMENT_COMMAND_TYPE)." AND c.requested_by=".$actor
        ." AND c.command_status IN ('completed','rejected','cancelled') ORDER BY c.lifecycle_command_id DESC LIMIT 1",
        'could not reconcile terminal Employee Training Assignment');
    $row = $result ? db_fetch_assoc($result) : false;
    if (!$result)
        return array('status'=>'inconsistent','own'=>false);
    if (!$row)
        return $none;
    $expected = array('completed'=>defined('APPROVAL_STATUS_APPROVED') ? APPROVAL_STATUS_APPROVED : 1,
        'rejected'=>defined('APPROVAL_STATUS_REJECTED') ? APPROVAL_STATUS_REJECTED : 2,
        'cancelled'=>defined('APPROVAL_STATUS_CANCELLED') ? APPROVAL_STATUS_CANCELLED : 3);
    $status = (string)$row['command_status'];
    if (!isset($expected[$status]) || (int)$row['row_version'] !== 2
        || (int)$row['approval_draft_id'] <= 0 || (int)$row['approval_row_id'] !== (int)$row['approval_draft_id']
        || !defined('ST_HRM_LIFECYCLE_TRAINING_ASSIGNMENT')
        || (int)$row['trans_type'] !== (int)ST_HRM_LIFECYCLE_TRAINING_ASSIGNMENT
        || (int)$row['submitted_by'] !== $actor || (int)$row['requested_by'] !== $actor
        || (int)$row['approval_status'] !== (int)$expected[$status])
        return array('status'=>'inconsistent','own'=>false);
    return array('status'=>$status,'own'=>true);
}
