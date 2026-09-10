<?php
/**
 * HRM-FND-005 Employee Promotion browser route/recovery helpers.
 *
 * This source is database read-only except for server-session custody of the
 * opaque idempotency key. Promotion mutation remains exclusively owned by
 * submit_hrm_lifecycle_employee_promotion() and its final checker executor.
 */
$hrm_lifecycle_promotion_browser_root = isset($path_to_root)
    ? $path_to_root : dirname(dirname(dirname(__DIR__)));
$hrm_lifecycle_promotion_browser_dependencies = array(
    '/hrm/includes/db/lifecycle_promotion_command_db.inc',
    '/hrm/includes/db/employee_person_worker_db.inc',
    '/hrm/includes/db/employee_employment_db.inc',
    '/hrm/includes/db/employee_assignment_db.inc',
    '/hrm/includes/db/employee_db.inc',
    '/includes/approval/db/approval_db.inc',
);
foreach ($hrm_lifecycle_promotion_browser_dependencies as $dependency) {
    $candidate = $hrm_lifecycle_promotion_browser_root.$dependency;
    if (is_file($candidate))
        include_once($candidate);
}
unset($hrm_lifecycle_promotion_browser_root, $hrm_lifecycle_promotion_browser_dependencies, $dependency, $candidate);

if (!defined('HRM_LIFECYCLE_PROMOTION_BROWSER_SESSION_NAMESPACE'))
    define('HRM_LIFECYCLE_PROMOTION_BROWSER_SESSION_NAMESPACE', 'hrm_fnd_005_employee_promotion_browser_idempotency');

/** @return string|false */
function hrm_lifecycle_promotion_browser_employee_id($employee_id)
{
    $employee_id = trim((string)$employee_id);
    if ($employee_id === '' || strlen($employee_id) > 20)
        return false;
    return $employee_id;
}

/** @return string */
function hrm_lifecycle_promotion_browser_session_slot($employee_id)
{
    return hash('sha256', 'employee_promotion|'.(string)$employee_id);
}

/** @return string|false */
function hrm_lifecycle_promotion_browser_idempotency_key($employee_id)
{
    $employee_id = hrm_lifecycle_promotion_browser_employee_id($employee_id);
    if ($employee_id === false || !isset($_SESSION) || !is_array($_SESSION))
        return false;
    if (!isset($_SESSION[HRM_LIFECYCLE_PROMOTION_BROWSER_SESSION_NAMESPACE])
        || !is_array($_SESSION[HRM_LIFECYCLE_PROMOTION_BROWSER_SESSION_NAMESPACE]))
        $_SESSION[HRM_LIFECYCLE_PROMOTION_BROWSER_SESSION_NAMESPACE] = array();

    $slot = hrm_lifecycle_promotion_browser_session_slot($employee_id);
    if (isset($_SESSION[HRM_LIFECYCLE_PROMOTION_BROWSER_SESSION_NAMESPACE][$slot])) {
        $existing = $_SESSION[HRM_LIFECYCLE_PROMOTION_BROWSER_SESSION_NAMESPACE][$slot];
        if (is_string($existing) && strlen($existing) >= 32 && strlen($existing) <= 128
            && preg_match('/^[A-Za-z0-9._:-]+$/D', $existing) === 1)
            return $existing;
        unset($_SESSION[HRM_LIFECYCLE_PROMOTION_BROWSER_SESSION_NAMESPACE][$slot]);
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
    $_SESSION[HRM_LIFECYCLE_PROMOTION_BROWSER_SESSION_NAMESPACE][$slot] = $key;
    return $key;
}

/** @return void */
function hrm_lifecycle_promotion_browser_forget_idempotency_key($employee_id)
{
    $employee_id = hrm_lifecycle_promotion_browser_employee_id($employee_id);
    if ($employee_id === false || !isset($_SESSION[HRM_LIFECYCLE_PROMOTION_BROWSER_SESSION_NAMESPACE])
        || !is_array($_SESSION[HRM_LIFECYCLE_PROMOTION_BROWSER_SESSION_NAMESPACE]))
        return;
    unset($_SESSION[HRM_LIFECYCLE_PROMOTION_BROWSER_SESSION_NAMESPACE][
        hrm_lifecycle_promotion_browser_session_slot($employee_id)
    ]);
    if (!$_SESSION[HRM_LIFECYCLE_PROMOTION_BROWSER_SESSION_NAMESPACE])
        unset($_SESSION[HRM_LIFECYCLE_PROMOTION_BROWSER_SESSION_NAMESPACE]);
}

/**
 * Resolve only the current assignment fields needed to build the accepted
 * Promotion command request. Department is server-owned and never accepted as
 * browser mutation input.
 *
 * @return array|false
 */
function get_hrm_lifecycle_employee_promotion_browser_snapshot($employee_id)
{
    $employee_id = hrm_lifecycle_promotion_browser_employee_id($employee_id);
    if ($employee_id === false || !function_exists('hrm_lifecycle_promotion_permission')
        || !hrm_lifecycle_promotion_permission() || !function_exists('get_hrm_person_worker_binding')
        || !function_exists('get_hrm_legacy_current_employment')
        || !function_exists('get_hrm_legacy_current_assignment')
        || !function_exists('get_employee_assignment_context_projection'))
        return false;
    $binding = get_hrm_person_worker_binding($employee_id, false);
    if (!$binding || !empty($binding['retired_at']) || (int)$binding['employee_number'] <= 0
        || (int)$binding['worker_id'] <= 0)
        return false;
    $employee_number = (int)$binding['employee_number'];
    $employment = get_hrm_legacy_current_employment($employee_number, false);
    $assignment = get_hrm_legacy_current_assignment($employee_number, false);
    $employee = get_employee_assignment_context_projection($employee_id);
    if (!$employment || !$assignment || !$employee
        || (int)$employment['worker_id'] !== (int)$binding['worker_id']
        || (int)$assignment['employment_id'] !== (int)$employment['employment_id']
        || (int)$assignment['legacy_employee_number'] !== $employee_number
        || (int)$assignment['legacy_current'] !== 1 || (int)$employment['legacy_current'] !== 1
        || (int)$assignment['primary_flag'] !== 1
        || (string)$assignment['assignment_type_code'] !== 'legacy_primary'
        || (int)$employee['department_id'] <= 0 || (int)$employee['position_id'] <= 0 || (int)$employee['grade_id'] <= 0
        || (int)$employee['department_id'] !== (int)$assignment['legacy_department_id']
        || (int)$employee['position_id'] !== (int)$assignment['legacy_position_id']
        || (int)$employee['grade_id'] !== (int)$assignment['legacy_grade_id'])
        return false;
    return array(
        'employee_number'=>$employee_number,
        'worker_id'=>(int)$binding['worker_id'],
        'employment_id'=>(int)$employment['employment_id'],
        'assignment_id'=>(int)$assignment['assignment_id'],
        'department_id'=>(int)$employee['department_id'],
        'position_id'=>(int)$employee['position_id'],
        'grade_id'=>(int)$employee['grade_id'],
        'assignment_effective_from'=>isset($assignment['effective_from']) ? (string)$assignment['effective_from'] : '',
        'employment_start_date'=>isset($employment['start_date']) ? (string)$employment['start_date'] : '',
        'employment_end_date'=>isset($employment['end_date']) ? (string)$employment['end_date'] : '',
    );
}

/** @return bool */
function hrm_lifecycle_promotion_browser_pending_row_is_valid($row)
{
    if (!is_array($row) || (string)$row['command_status'] !== 'pending'
        || (int)$row['row_version'] !== 1 || (int)$row['approval_draft_id'] <= 0
        || (int)$row['approval_row_id'] !== (int)$row['approval_draft_id']
        || (int)$row['submitted_by'] !== (int)$row['requested_by'] || (int)$row['requested_by'] <= 0
        || !defined('APPROVAL_STATUS_PENDING') || (int)$row['approval_status'] !== (int)APPROVAL_STATUS_PENDING)
        return false;
    $types = array(
        'employee_transfer'=>'ST_HRM_LIFECYCLE_TRANSFER',
        'employee_separation'=>'ST_HRM_LIFECYCLE_SEPARATION',
        'employee_salary_revision'=>'ST_HRM_LIFECYCLE_SALARY_REVISION',
        'employee_training_assignment'=>'ST_HRM_LIFECYCLE_TRAINING_ASSIGNMENT',
        'employee_confirmation'=>'ST_HRM_LIFECYCLE_CONFIRMATION',
        'employee_probation_schedule'=>'ST_HRM_LIFECYCLE_PROBATION_SCHEDULE',
        HRM_LIFECYCLE_PROMOTION_COMMAND_TYPE=>'ST_HRM_LIFECYCLE_PROMOTION',
    );
    $type = (string)$row['command_type'];
    if (!isset($types[$type]) || !defined($types[$type]))
        return false;
    return (int)$row['trans_type'] === (int)constant($types[$type]);
}

/**
 * Reconcile coarse browser-visible Promotion state. Foreign pending command
 * identifiers and payload are never exposed.
 *
 * @return array
 */
function get_hrm_lifecycle_employee_promotion_browser_status($employee_id)
{
    $none = array('status'=>'none','own'=>false,'lifecycle_command_id'=>0,'approval_draft_id'=>0);
    $employee_id = hrm_lifecycle_promotion_browser_employee_id($employee_id);
    $actor = function_exists('hrm_lifecycle_promotion_actor_id') ? hrm_lifecycle_promotion_actor_id() : 0;
    if ($employee_id === false || $actor <= 0 || !function_exists('hrm_lifecycle_promotion_permission')
        || !hrm_lifecycle_promotion_permission() || !function_exists('get_hrm_person_worker_binding'))
        return array_merge($none, array('status'=>'inconsistent'));
    $binding = get_hrm_person_worker_binding($employee_id, false);
    if (!$binding || !empty($binding['retired_at']) || (int)$binding['worker_id'] <= 0)
        return $none;

    $pending_sql = "SELECT c.lifecycle_command_id,c.command_type,c.command_status,c.approval_draft_id,c.row_version,"
        ."c.requested_by,d.id approval_row_id,d.trans_type,d.status approval_status,d.submitted_by "
        ."FROM ".TB_PREF."hrm_lifecycle_commands c LEFT JOIN ".TB_PREF."approval_drafts d ON d.id=c.approval_draft_id "
        ."WHERE c.worker_id=".(int)$binding['worker_id']." AND c.command_status='pending' ORDER BY c.lifecycle_command_id DESC";
    $pending_result = db_query($pending_sql, 'could not reconcile pending Employee Promotion lifecycle command status');
    if (!$pending_result)
        return array_merge($none, array('status'=>'inconsistent'));
    $pending = array();
    while ($row = db_fetch_assoc($pending_result)) {
        if (!hrm_lifecycle_promotion_browser_pending_row_is_valid($row))
            return array_merge($none, array('status'=>'inconsistent'));
        $pending[] = $row;
    }
    if (count($pending) > 1)
        return array_merge($none, array('status'=>'inconsistent'));
    if (count($pending) === 1) {
        $row = $pending[0];
        if ((string)$row['command_type'] !== HRM_LIFECYCLE_PROMOTION_COMMAND_TYPE
            || (int)$row['requested_by'] !== $actor)
            return array('status'=>'blocked','own'=>false,'lifecycle_command_id'=>0,'approval_draft_id'=>0);
        return array('status'=>'pending','own'=>true,
            'lifecycle_command_id'=>(int)$row['lifecycle_command_id'],
            'approval_draft_id'=>(int)$row['approval_draft_id']);
    }

    $terminal_sql = "SELECT c.lifecycle_command_id,c.command_status,c.approval_draft_id,c.row_version,c.requested_by,"
        ."d.id approval_row_id,d.trans_type,d.status approval_status,d.submitted_by "
        ."FROM ".TB_PREF."hrm_lifecycle_commands c LEFT JOIN ".TB_PREF."approval_drafts d ON d.id=c.approval_draft_id "
        ."WHERE c.worker_id=".(int)$binding['worker_id']." AND c.command_type=".db_escape(HRM_LIFECYCLE_PROMOTION_COMMAND_TYPE)
        ." AND c.requested_by=".$actor." AND c.command_status IN ('completed','rejected','cancelled') "
        ."ORDER BY c.lifecycle_command_id DESC LIMIT 1";
    $terminal_result = db_query($terminal_sql, 'could not reconcile terminal Employee Promotion browser command status');
    if (!$terminal_result)
        return array_merge($none, array('status'=>'inconsistent'));
    $row = db_fetch_assoc($terminal_result);
    if (!$row)
        return $none;
    $expected = array(
        'completed'=>defined('APPROVAL_STATUS_APPROVED') ? APPROVAL_STATUS_APPROVED : 1,
        'rejected'=>defined('APPROVAL_STATUS_REJECTED') ? APPROVAL_STATUS_REJECTED : 2,
        'cancelled'=>defined('APPROVAL_STATUS_CANCELLED') ? APPROVAL_STATUS_CANCELLED : 3,
    );
    $status = (string)$row['command_status'];
    if (!isset($expected[$status]) || (int)$row['row_version'] !== 2
        || (int)$row['approval_draft_id'] <= 0 || (int)$row['approval_row_id'] !== (int)$row['approval_draft_id']
        || !defined('ST_HRM_LIFECYCLE_PROMOTION') || (int)$row['trans_type'] !== (int)ST_HRM_LIFECYCLE_PROMOTION
        || (int)$row['submitted_by'] !== (int)$row['requested_by'] || (int)$row['requested_by'] !== $actor
        || (int)$row['approval_status'] !== (int)$expected[$status])
        return array_merge($none, array('status'=>'inconsistent'));
    return array('status'=>$status,'own'=>true,
        'lifecycle_command_id'=>(int)$row['lifecycle_command_id'],
        'approval_draft_id'=>(int)$row['approval_draft_id']);
}
