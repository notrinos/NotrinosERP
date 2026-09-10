<?php
/**
 * HRM-FND-005 Employee Suspension/Reinstatement browser custody helpers.
 *
 * Database access is read-only. The only mutable browser state is a per-user,
 * per-employee, per-command opaque retry key stored in the server session.
 * Business mutation remains exclusively in the fixed Suspension/Reinstatement
 * lifecycle commands and their independent final checker executors.
 */
$hrm_service_state_browser_root = isset($path_to_root) ? $path_to_root : dirname(dirname(dirname(__DIR__)));
$hrm_service_state_browser_dependencies = array(
    '/hrm/includes/db/lifecycle_suspension_command_db.inc',
    '/hrm/includes/db/lifecycle_reinstatement_command_db.inc',
    '/hrm/includes/db/employee_person_worker_db.inc',
    '/hrm/includes/db/employee_employment_db.inc',
    '/hrm/includes/db/employee_assignment_db.inc',
    '/includes/approval/db/approval_db.inc',
);
foreach ($hrm_service_state_browser_dependencies as $dependency) {
    $candidate = $hrm_service_state_browser_root.$dependency;
    if (is_file($candidate)) include_once($candidate);
}
unset($hrm_service_state_browser_root, $hrm_service_state_browser_dependencies, $dependency, $candidate);

if (!defined('HRM_LIFECYCLE_SUSPENSION_BROWSER_SESSION_NAMESPACE'))
    define('HRM_LIFECYCLE_SUSPENSION_BROWSER_SESSION_NAMESPACE', 'hrm_fnd_005_employee_suspension_browser_idempotency');
if (!defined('HRM_LIFECYCLE_REINSTATEMENT_BROWSER_SESSION_NAMESPACE'))
    define('HRM_LIFECYCLE_REINSTATEMENT_BROWSER_SESSION_NAMESPACE', 'hrm_fnd_005_employee_reinstatement_browser_idempotency');

function hrm_lifecycle_service_state_browser_employee_id($employee_id)
{
    $employee_id = trim((string)$employee_id);
    return $employee_id !== '' && strlen($employee_id) <= 20 ? $employee_id : false;
}
function hrm_lifecycle_service_state_browser_session_slot($command_type, $employee_id)
{
    return hash('sha256', (string)$command_type.'|'.(string)$employee_id);
}
function hrm_lifecycle_service_state_browser_key($namespace, $command_type, $employee_id)
{
    $employee_id = hrm_lifecycle_service_state_browser_employee_id($employee_id);
    if ($employee_id === false || !isset($_SESSION) || !is_array($_SESSION)) return false;
    if (!isset($_SESSION[$namespace]) || !is_array($_SESSION[$namespace])) $_SESSION[$namespace] = array();
    $slot = hrm_lifecycle_service_state_browser_session_slot($command_type, $employee_id);
    if (isset($_SESSION[$namespace][$slot])) {
        $existing = $_SESSION[$namespace][$slot];
        if (is_string($existing) && strlen($existing) >= 32 && strlen($existing) <= 128
            && preg_match('/^[A-Za-z0-9._:-]+$/D', $existing) === 1) return $existing;
        unset($_SESSION[$namespace][$slot]);
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
    if ($bytes === false) return false;
    $key = bin2hex($bytes);
    if (strlen($key) !== 64) return false;
    $_SESSION[$namespace][$slot] = $key;
    return $key;
}
function hrm_lifecycle_service_state_browser_forget_key($namespace, $command_type, $employee_id)
{
    $employee_id = hrm_lifecycle_service_state_browser_employee_id($employee_id);
    if ($employee_id === false || !isset($_SESSION[$namespace]) || !is_array($_SESSION[$namespace])) return;
    unset($_SESSION[$namespace][hrm_lifecycle_service_state_browser_session_slot($command_type, $employee_id)]);
    if (!$_SESSION[$namespace]) unset($_SESSION[$namespace]);
}
function hrm_lifecycle_suspension_browser_idempotency_key($employee_id)
{
    return hrm_lifecycle_service_state_browser_key(HRM_LIFECYCLE_SUSPENSION_BROWSER_SESSION_NAMESPACE, 'employee_suspension', $employee_id);
}
function hrm_lifecycle_suspension_browser_forget_idempotency_key($employee_id)
{
    hrm_lifecycle_service_state_browser_forget_key(HRM_LIFECYCLE_SUSPENSION_BROWSER_SESSION_NAMESPACE, 'employee_suspension', $employee_id);
}
function hrm_lifecycle_reinstatement_browser_idempotency_key($employee_id)
{
    return hrm_lifecycle_service_state_browser_key(HRM_LIFECYCLE_REINSTATEMENT_BROWSER_SESSION_NAMESPACE, 'employee_reinstatement', $employee_id);
}
function hrm_lifecycle_reinstatement_browser_forget_idempotency_key($employee_id)
{
    hrm_lifecycle_service_state_browser_forget_key(HRM_LIFECYCLE_REINSTATEMENT_BROWSER_SESSION_NAMESPACE, 'employee_reinstatement', $employee_id);
}

/** Resolve exact current legacy/normalized service-state custody without mutation. */
function get_hrm_lifecycle_employee_service_state_browser_snapshot($employee_id, $required_status, $permission_function)
{
    $employee_id = hrm_lifecycle_service_state_browser_employee_id($employee_id);
    if ($employee_id === false || !is_string($permission_function) || !function_exists($permission_function)
        || !$permission_function() || !function_exists('get_hrm_person_worker_binding')
        || !function_exists('get_hrm_legacy_current_employment') || !function_exists('get_hrm_legacy_current_assignment')) return false;
    $binding = get_hrm_person_worker_binding($employee_id, false);
    if (!$binding || !empty($binding['retired_at']) || (int)$binding['employee_number'] <= 0 || (int)$binding['worker_id'] <= 0) return false;
    $employee_number = (int)$binding['employee_number'];
    $employment = get_hrm_legacy_current_employment($employee_number, false);
    $assignment = get_hrm_legacy_current_assignment($employee_number, false);
    $result = db_query("SELECT employee_id,hire_date,inactive,released_date,department_id,position_id,grade_id FROM ".TB_PREF."employees "
        ."WHERE employee_number=".$employee_number." AND employee_id=".db_escape($employee_id)." LIMIT 1",
        'could not read Employee service-state browser projection');
    $employee = $result ? db_fetch_assoc($result) : false;
    if (!$employment || !$assignment || !$employee
        || (int)$employment['worker_id'] !== (int)$binding['worker_id']
        || (int)$assignment['employment_id'] !== (int)$employment['employment_id']
        || (int)$assignment['legacy_employee_number'] !== $employee_number
        || (int)$assignment['legacy_current'] !== 1 || (int)$employment['legacy_current'] !== 1
        || (int)$assignment['primary_flag'] !== 1 || (string)$assignment['assignment_type_code'] !== 'legacy_primary'
        || (string)$employment['employment_status'] !== (string)$required_status
        || ($employment['end_date'] !== null && $employment['end_date'] !== '')
        || ($assignment['effective_to'] !== null && $assignment['effective_to'] !== '')
        || (int)$employee['inactive'] !== 0 || ($employee['released_date'] !== null && $employee['released_date'] !== '')
        || (int)$employee['department_id'] !== (int)$assignment['legacy_department_id']
        || (int)$employee['position_id'] !== (int)$assignment['legacy_position_id']
        || (int)$employee['grade_id'] !== (int)$assignment['legacy_grade_id']) return false;
    return array(
        'employee_number'=>$employee_number, 'worker_id'=>(int)$binding['worker_id'],
        'employment_id'=>(int)$employment['employment_id'], 'assignment_id'=>(int)$assignment['assignment_id'],
        'employment_status'=>(string)$employment['employment_status'],
        'employment_start_date'=>isset($employment['start_date']) ? (string)$employment['start_date'] : '',
        'assignment_effective_from'=>isset($assignment['effective_from']) ? (string)$assignment['effective_from'] : '',
        'hire_date'=>isset($employee['hire_date']) ? (string)$employee['hire_date'] : '',
    );
}
function get_hrm_lifecycle_employee_suspension_browser_snapshot($employee_id)
{
    return get_hrm_lifecycle_employee_service_state_browser_snapshot($employee_id, 'active', 'hrm_lifecycle_suspension_permission');
}
function get_hrm_lifecycle_employee_reinstatement_browser_snapshot($employee_id)
{
    return get_hrm_lifecycle_employee_service_state_browser_snapshot($employee_id, 'suspended', 'hrm_lifecycle_reinstatement_permission');
}

/** Validate the complete fixed lifecycle family before treating another command as a safe foreign blocker. */
function hrm_lifecycle_service_state_browser_pending_row_is_valid($row)
{
    if (!is_array($row) || (string)$row['command_status'] !== 'pending' || (int)$row['row_version'] !== 1
        || (int)$row['approval_draft_id'] <= 0 || (int)$row['approval_row_id'] !== (int)$row['approval_draft_id']
        || (int)$row['submitted_by'] !== (int)$row['requested_by'] || (int)$row['requested_by'] <= 0
        || !defined('APPROVAL_STATUS_PENDING') || (int)$row['approval_status'] !== (int)APPROVAL_STATUS_PENDING) return false;
    $types = array(
        'employee_transfer'=>'ST_HRM_LIFECYCLE_TRANSFER', 'employee_separation'=>'ST_HRM_LIFECYCLE_SEPARATION',
        'employee_salary_revision'=>'ST_HRM_LIFECYCLE_SALARY_REVISION', 'employee_training_assignment'=>'ST_HRM_LIFECYCLE_TRAINING_ASSIGNMENT',
        'employee_confirmation'=>'ST_HRM_LIFECYCLE_CONFIRMATION', 'employee_probation_schedule'=>'ST_HRM_LIFECYCLE_PROBATION_SCHEDULE',
        'employee_promotion'=>'ST_HRM_LIFECYCLE_PROMOTION', 'employee_suspension'=>'ST_HRM_LIFECYCLE_SUSPENSION',
        'employee_reinstatement'=>'ST_HRM_LIFECYCLE_REINSTATEMENT',
    );
    $type = (string)$row['command_type'];
    return isset($types[$type]) && defined($types[$type]) && (int)$row['trans_type'] === (int)constant($types[$type]);
}
function get_hrm_lifecycle_employee_service_state_browser_status($employee_id, $command_type, $trans_type_constant, $actor_function, $permission_function)
{
    $none = array('status'=>'none','own'=>false,'lifecycle_command_id'=>0,'approval_draft_id'=>0);
    $employee_id = hrm_lifecycle_service_state_browser_employee_id($employee_id);
    $actor = is_string($actor_function) && function_exists($actor_function) ? (int)$actor_function() : 0;
    if ($employee_id === false || $actor <= 0 || !is_string($permission_function) || !function_exists($permission_function)
        || !$permission_function() || !function_exists('get_hrm_person_worker_binding')) return array_merge($none,array('status'=>'inconsistent'));
    $binding = get_hrm_person_worker_binding($employee_id, false);
    if (!$binding || !empty($binding['retired_at']) || (int)$binding['worker_id'] <= 0) return $none;
    $pending_result = db_query("SELECT c.lifecycle_command_id,c.command_type,c.command_status,c.approval_draft_id,c.row_version,c.requested_by,"
        ."d.id approval_row_id,d.trans_type,d.status approval_status,d.submitted_by FROM ".TB_PREF."hrm_lifecycle_commands c "
        ."LEFT JOIN ".TB_PREF."approval_drafts d ON d.id=c.approval_draft_id WHERE c.worker_id=".(int)$binding['worker_id']
        ." AND c.command_status='pending' ORDER BY c.lifecycle_command_id DESC", 'could not reconcile pending Employee service-state lifecycle command');
    if (!$pending_result) return array_merge($none,array('status'=>'inconsistent'));
    $pending=array(); while($row=db_fetch_assoc($pending_result)){ if(!hrm_lifecycle_service_state_browser_pending_row_is_valid($row)) return array_merge($none,array('status'=>'inconsistent')); $pending[]=$row; }
    if(count($pending)>1) return array_merge($none,array('status'=>'inconsistent'));
    if(count($pending)===1){$row=$pending[0]; if((string)$row['command_type']!==(string)$command_type || (int)$row['requested_by']!==$actor)
        return array('status'=>'blocked','own'=>false,'lifecycle_command_id'=>0,'approval_draft_id'=>0);
        return array('status'=>'pending','own'=>true,'lifecycle_command_id'=>(int)$row['lifecycle_command_id'],'approval_draft_id'=>(int)$row['approval_draft_id']);}
    if (!defined($trans_type_constant)) return array_merge($none,array('status'=>'inconsistent'));
    $terminal_result=db_query("SELECT c.lifecycle_command_id,c.command_status,c.approval_draft_id,c.row_version,c.requested_by,"
        ."d.id approval_row_id,d.trans_type,d.status approval_status,d.submitted_by FROM ".TB_PREF."hrm_lifecycle_commands c "
        ."LEFT JOIN ".TB_PREF."approval_drafts d ON d.id=c.approval_draft_id WHERE c.worker_id=".(int)$binding['worker_id']
        ." AND c.command_type=".db_escape($command_type)." AND c.requested_by=".$actor
        ." AND c.command_status IN ('completed','rejected','cancelled') ORDER BY c.lifecycle_command_id DESC LIMIT 1",
        'could not reconcile terminal Employee service-state lifecycle command');
    if(!$terminal_result) return array_merge($none,array('status'=>'inconsistent'));
    $row=db_fetch_assoc($terminal_result); if(!$row) return $none;
    $expected=array('completed'=>defined('APPROVAL_STATUS_APPROVED')?APPROVAL_STATUS_APPROVED:1,
        'rejected'=>defined('APPROVAL_STATUS_REJECTED')?APPROVAL_STATUS_REJECTED:2,
        'cancelled'=>defined('APPROVAL_STATUS_CANCELLED')?APPROVAL_STATUS_CANCELLED:3);
    $status=(string)$row['command_status'];
    if(!isset($expected[$status]) || (int)$row['row_version']!==2 || (int)$row['approval_draft_id']<=0
        || (int)$row['approval_row_id']!==(int)$row['approval_draft_id'] || (int)$row['trans_type']!==(int)constant($trans_type_constant)
        || (int)$row['submitted_by']!==(int)$row['requested_by'] || (int)$row['requested_by']!==$actor
        || (int)$row['approval_status']!==(int)$expected[$status]) return array_merge($none,array('status'=>'inconsistent'));
    return array('status'=>$status,'own'=>true,'lifecycle_command_id'=>(int)$row['lifecycle_command_id'],'approval_draft_id'=>(int)$row['approval_draft_id']);
}
function get_hrm_lifecycle_employee_suspension_browser_status($employee_id)
{
    return get_hrm_lifecycle_employee_service_state_browser_status($employee_id,'employee_suspension','ST_HRM_LIFECYCLE_SUSPENSION','hrm_lifecycle_suspension_actor_id','hrm_lifecycle_suspension_permission');
}
function get_hrm_lifecycle_employee_reinstatement_browser_status($employee_id)
{
    return get_hrm_lifecycle_employee_service_state_browser_status($employee_id,'employee_reinstatement','ST_HRM_LIFECYCLE_REINSTATEMENT','hrm_lifecycle_reinstatement_actor_id','hrm_lifecycle_reinstatement_permission');
}
