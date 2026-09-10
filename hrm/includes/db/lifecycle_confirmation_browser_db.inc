<?php
/**
 * HRM-FND-005 Employee Confirmation browser cutover/recovery helpers.
 *
 * Browser state is read-only except for server-session custody of one opaque
 * idempotency key. The maintenance route must submit through
 * submit_hrm_lifecycle_employee_confirmation(); no request-controlled command
 * identity or direct confirmation_date mutation fallback is permitted.
 */
$hrm_lifecycle_confirmation_browser_root = isset($path_to_root)
    ? $path_to_root : dirname(dirname(dirname(__DIR__)));
$hrm_lifecycle_confirmation_browser_dependencies = array(
    '/hrm/includes/db/lifecycle_confirmation_command_db.inc',
    '/hrm/includes/db/employee_person_worker_db.inc',
    '/includes/approval/db/approval_db.inc',
);
foreach ($hrm_lifecycle_confirmation_browser_dependencies as $dependency) {
    $candidate = $hrm_lifecycle_confirmation_browser_root.$dependency;
    if (is_file($candidate))
        include_once($candidate);
}
unset($hrm_lifecycle_confirmation_browser_root, $hrm_lifecycle_confirmation_browser_dependencies,
    $dependency, $candidate);

if (!defined('HRM_LIFECYCLE_CONFIRMATION_BROWSER_SESSION_NAMESPACE'))
    define('HRM_LIFECYCLE_CONFIRMATION_BROWSER_SESSION_NAMESPACE', 'hrm_fnd_005_employee_confirmation_browser_idempotency');

/** @param mixed $employee_id @return string|false */
function hrm_lifecycle_confirmation_browser_employee_id($employee_id)
{
    $employee_id = trim((string)$employee_id);
    if ($employee_id === '' || strlen($employee_id) > 20)
        return false;
    return $employee_id;
}

/** @return string */
function hrm_lifecycle_confirmation_browser_session_slot($employee_id)
{
    return hash('sha256', 'employee_confirmation|'.(string)$employee_id);
}

/** @return string|false */
function hrm_lifecycle_confirmation_browser_idempotency_key($employee_id)
{
    $employee_id = hrm_lifecycle_confirmation_browser_employee_id($employee_id);
    if ($employee_id === false || !isset($_SESSION) || !is_array($_SESSION))
        return false;
    if (!isset($_SESSION[HRM_LIFECYCLE_CONFIRMATION_BROWSER_SESSION_NAMESPACE])
        || !is_array($_SESSION[HRM_LIFECYCLE_CONFIRMATION_BROWSER_SESSION_NAMESPACE]))
        $_SESSION[HRM_LIFECYCLE_CONFIRMATION_BROWSER_SESSION_NAMESPACE] = array();

    $slot = hrm_lifecycle_confirmation_browser_session_slot($employee_id);
    if (isset($_SESSION[HRM_LIFECYCLE_CONFIRMATION_BROWSER_SESSION_NAMESPACE][$slot])) {
        $existing = $_SESSION[HRM_LIFECYCLE_CONFIRMATION_BROWSER_SESSION_NAMESPACE][$slot];
        if (is_string($existing) && strlen($existing) >= 32 && strlen($existing) <= 128
            && preg_match('/^[A-Za-z0-9._:-]+$/D', $existing) === 1)
            return $existing;
        unset($_SESSION[HRM_LIFECYCLE_CONFIRMATION_BROWSER_SESSION_NAMESPACE][$slot]);
    }

    $bytes = false;
    if (function_exists('random_bytes')) {
        try { $bytes = random_bytes(32); } catch (Exception $e) { $bytes = false; }
        catch (Throwable $e) { $bytes = false; }
    }
    if ($bytes === false && function_exists('openssl_random_pseudo_bytes')) {
        $strong = false;
        $candidate = openssl_random_pseudo_bytes(32, $strong);
        if ($candidate !== false && $strong)
            $bytes = $candidate;
    }
    if ($bytes === false)
        return false;
    $key = bin2hex($bytes);
    if (strlen($key) !== 64)
        return false;
    $_SESSION[HRM_LIFECYCLE_CONFIRMATION_BROWSER_SESSION_NAMESPACE][$slot] = $key;
    return $key;
}

/** @return void */
function hrm_lifecycle_confirmation_browser_forget_idempotency_key($employee_id)
{
    $employee_id = hrm_lifecycle_confirmation_browser_employee_id($employee_id);
    if ($employee_id === false || !isset($_SESSION[HRM_LIFECYCLE_CONFIRMATION_BROWSER_SESSION_NAMESPACE])
        || !is_array($_SESSION[HRM_LIFECYCLE_CONFIRMATION_BROWSER_SESSION_NAMESPACE]))
        return;
    unset($_SESSION[HRM_LIFECYCLE_CONFIRMATION_BROWSER_SESSION_NAMESPACE][
        hrm_lifecycle_confirmation_browser_session_slot($employee_id)
    ]);
    if (!$_SESSION[HRM_LIFECYCLE_CONFIRMATION_BROWSER_SESSION_NAMESPACE])
        unset($_SESSION[HRM_LIFECYCLE_CONFIRMATION_BROWSER_SESSION_NAMESPACE]);
}

/** @param array $row @return bool */
function hrm_lifecycle_confirmation_browser_pending_row_is_valid($row)
{
    if (!is_array($row) || (string)$row['command_status'] !== 'pending'
        || (int)$row['row_version'] !== 1 || (int)$row['approval_draft_id'] <= 0
        || (int)$row['approval_row_id'] !== (int)$row['approval_draft_id']
        || (int)$row['submitted_by'] !== (int)$row['requested_by'] || (int)$row['requested_by'] <= 0
        || !defined('APPROVAL_STATUS_PENDING') || (int)$row['approval_status'] !== (int)APPROVAL_STATUS_PENDING)
        return false;

    $types = array(
        'employee_transfer'=>defined('ST_HRM_LIFECYCLE_TRANSFER') ? ST_HRM_LIFECYCLE_TRANSFER : -1,
        'employee_separation'=>defined('ST_HRM_LIFECYCLE_SEPARATION') ? ST_HRM_LIFECYCLE_SEPARATION : -1,
        'employee_salary_revision'=>defined('ST_HRM_LIFECYCLE_SALARY_REVISION') ? ST_HRM_LIFECYCLE_SALARY_REVISION : -1,
        'employee_training_assignment'=>defined('ST_HRM_LIFECYCLE_TRAINING_ASSIGNMENT') ? ST_HRM_LIFECYCLE_TRAINING_ASSIGNMENT : -1,
        'employee_promotion'=>defined('ST_HRM_LIFECYCLE_PROMOTION') ? ST_HRM_LIFECYCLE_PROMOTION : -1,
        'employee_suspension'=>defined('ST_HRM_LIFECYCLE_SUSPENSION') ? ST_HRM_LIFECYCLE_SUSPENSION : -1,
        'employee_reinstatement'=>defined('ST_HRM_LIFECYCLE_REINSTATEMENT') ? ST_HRM_LIFECYCLE_REINSTATEMENT : -1,
        HRM_LIFECYCLE_CONFIRMATION_COMMAND_TYPE=>defined('ST_HRM_LIFECYCLE_CONFIRMATION') ? ST_HRM_LIFECYCLE_CONFIRMATION : -1,
        'employee_probation_schedule'=>defined('ST_HRM_LIFECYCLE_PROBATION_SCHEDULE') ? ST_HRM_LIFECYCLE_PROBATION_SCHEDULE : -1,
    );
    $type = (string)$row['command_type'];
    return isset($types[$type]) && (int)$types[$type] > 0
        && (int)$row['trans_type'] === (int)$types[$type];
}

/**
 * Return coarse current-maker state only. Another maker's command ids, payload,
 * reason and approval comments are never returned.
 *
 * @param string $employee_id
 * @return array
 */
function get_hrm_lifecycle_employee_confirmation_browser_status($employee_id)
{
    $none = array('status'=>'none','own'=>false,'lifecycle_command_id'=>0,'approval_draft_id'=>0);
    $employee_id = hrm_lifecycle_confirmation_browser_employee_id($employee_id);
    $actor = function_exists('hrm_lifecycle_confirmation_actor_id') ? hrm_lifecycle_confirmation_actor_id() : 0;
    if ($employee_id === false || $actor <= 0 || !function_exists('hrm_lifecycle_confirmation_permission')
        || !hrm_lifecycle_confirmation_permission() || !function_exists('get_hrm_person_worker_binding'))
        return array_merge($none, array('status'=>'inconsistent'));

    $binding = get_hrm_person_worker_binding($employee_id, false);
    if (!$binding || !empty($binding['retired_at']) || (int)$binding['worker_id'] <= 0)
        return $none;

    $pending_sql = "SELECT c.lifecycle_command_id,c.command_type,c.command_status,c.approval_draft_id,c.row_version,"
        ."c.requested_by,d.id approval_row_id,d.trans_type,d.status approval_status,d.submitted_by "
        ."FROM ".TB_PREF."hrm_lifecycle_commands c LEFT JOIN ".TB_PREF."approval_drafts d ON d.id=c.approval_draft_id "
        ."WHERE c.worker_id=".(int)$binding['worker_id']." AND c.command_status='pending' ORDER BY c.lifecycle_command_id DESC";
    $pending_result = db_query($pending_sql, 'could not reconcile pending Employee lifecycle command status');
    if (!$pending_result)
        return array_merge($none, array('status'=>'inconsistent'));
    $pending = array();
    while ($row = db_fetch_assoc($pending_result)) {
        if (!hrm_lifecycle_confirmation_browser_pending_row_is_valid($row))
            return array_merge($none, array('status'=>'inconsistent'));
        $pending[] = $row;
    }
    if (count($pending) > 1)
        return array_merge($none, array('status'=>'inconsistent'));
    if (count($pending) === 1) {
        $row = $pending[0];
        if ((string)$row['command_type'] !== HRM_LIFECYCLE_CONFIRMATION_COMMAND_TYPE
            || (int)$row['requested_by'] !== $actor)
            return array('status'=>'blocked','own'=>false,'lifecycle_command_id'=>0,'approval_draft_id'=>0);
        return array('status'=>'pending','own'=>true,
            'lifecycle_command_id'=>(int)$row['lifecycle_command_id'],
            'approval_draft_id'=>(int)$row['approval_draft_id']);
    }

    $terminal_sql = "SELECT c.lifecycle_command_id,c.command_status,c.approval_draft_id,c.row_version,c.requested_by,"
        ."d.id approval_row_id,d.trans_type,d.status approval_status,d.submitted_by "
        ."FROM ".TB_PREF."hrm_lifecycle_commands c LEFT JOIN ".TB_PREF."approval_drafts d ON d.id=c.approval_draft_id "
        ."WHERE c.worker_id=".(int)$binding['worker_id']." AND c.command_type=".db_escape(HRM_LIFECYCLE_CONFIRMATION_COMMAND_TYPE)
        ." AND c.requested_by=".$actor." AND c.command_status IN ('completed','rejected','cancelled') "
        ."ORDER BY c.lifecycle_command_id DESC LIMIT 1";
    $terminal_result = db_query($terminal_sql, 'could not reconcile terminal Employee Confirmation browser status');
    if (!$terminal_result)
        return array_merge($none, array('status'=>'inconsistent'));
    $row = db_fetch_assoc($terminal_result);
    if (!$row)
        return $none;

    $expected_approval = array(
        'completed'=>defined('APPROVAL_STATUS_APPROVED') ? APPROVAL_STATUS_APPROVED : 1,
        'rejected'=>defined('APPROVAL_STATUS_REJECTED') ? APPROVAL_STATUS_REJECTED : 2,
        'cancelled'=>defined('APPROVAL_STATUS_CANCELLED') ? APPROVAL_STATUS_CANCELLED : 3,
    );
    $status = (string)$row['command_status'];
    if (!isset($expected_approval[$status]) || (int)$row['row_version'] !== 2
        || (int)$row['approval_draft_id'] <= 0 || (int)$row['approval_row_id'] !== (int)$row['approval_draft_id']
        || !defined('ST_HRM_LIFECYCLE_CONFIRMATION') || (int)$row['trans_type'] !== (int)ST_HRM_LIFECYCLE_CONFIRMATION
        || (int)$row['submitted_by'] !== (int)$row['requested_by'] || (int)$row['requested_by'] !== $actor
        || (int)$row['approval_status'] !== (int)$expected_approval[$status])
        return array_merge($none, array('status'=>'inconsistent'));

    return array('status'=>$status,'own'=>true,
        'lifecycle_command_id'=>(int)$row['lifecycle_command_id'],
        'approval_draft_id'=>(int)$row['approval_draft_id']);
}
