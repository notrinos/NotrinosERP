<?php
/**
 * HRM-FND-005 fixed Employee Suspension lifecycle command.
 *
 * This source intentionally exposes no browser route. It binds one current
 * Person/Worker/Employment/primary Assignment, requires central maker-checker
 * approval, stores only hash-bound command custody, and changes only the
 * normalized Employment service state plus append-only Employee history.
 * Legacy Employee inactive/release fields, Assignment chronology, payroll,
 * payslips, payments, bank transactions and GL are outside this command.
 */
$hrm_lifecycle_suspension_root = isset($path_to_root) ? $path_to_root : dirname(dirname(dirname(__DIR__)));
$hrm_lifecycle_suspension_dependencies = array(
    array('/includes/approval/approval_workflow.inc', 'class', 'ApprovalWorkflow'),
    array('/includes/approval/db/approval_rules_db.inc', 'function', 'get_approval_workflow_by_type'),
    array('/includes/db/domain_audit_db.inc', 'function', 'append_domain_audit_event'),
    array('/hrm/includes/db/employee_history_db.inc', 'function', 'add_employee_history'),
    array('/hrm/includes/db/employee_person_worker_db.inc', 'function', 'get_hrm_person_worker_binding'),
    array('/hrm/includes/db/employee_employment_db.inc', 'function', 'get_hrm_legacy_current_employment'),
    array('/hrm/includes/db/employee_employment_db.inc', 'function', 'execute_hrm_employment_suspension'),
    array('/hrm/includes/db/employee_assignment_db.inc', 'function', 'get_hrm_legacy_current_assignment'),
    array('/hrm/includes/hrm_constants.inc', 'constant', 'HRM_HIST_SUSPENSION'),
);
foreach ($hrm_lifecycle_suspension_dependencies as $dependency) {
    if ($dependency[1] === 'class') $available = class_exists($dependency[2]);
    elseif ($dependency[1] === 'constant') $available = defined($dependency[2]);
    else $available = function_exists($dependency[2]);
    if ($available) continue;
    $candidate = $hrm_lifecycle_suspension_root.$dependency[0];
    if (is_file($candidate)) include_once($candidate);
}
unset($hrm_lifecycle_suspension_root, $hrm_lifecycle_suspension_dependencies, $dependency, $candidate, $available);

if (!defined('HRM_LIFECYCLE_SUSPENSION_COMMAND_TYPE')) define('HRM_LIFECYCLE_SUSPENSION_COMMAND_TYPE', 'employee_suspension');
if (!defined('HRM_LIFECYCLE_SUSPENSION_HASH_CONTRACT')) define('HRM_LIFECYCLE_SUSPENSION_HASH_CONTRACT', 'notrinoserp.hrm-fnd-005.employee-suspension');
if (!defined('HRM_LIFECYCLE_SUSPENSION_HASH_VERSION')) define('HRM_LIFECYCLE_SUSPENSION_HASH_VERSION', '1');
if (!defined('HRM_LIFECYCLE_SUSPENSION_REASON')) define('HRM_LIFECYCLE_SUSPENSION_REASON', 'hrm_fnd_005_employee_suspension_command');

function hrm_lifecycle_suspension_actor_id()
{
    return isset($_SESSION['wa_current_user']) && is_object($_SESSION['wa_current_user'])
        && isset($_SESSION['wa_current_user']->user) ? (int)$_SESSION['wa_current_user']->user : 0;
}
function hrm_lifecycle_suspension_permission()
{
    return function_exists('user_check_access') && user_check_access('SA_EMPSEPARATION');
}
function hrm_lifecycle_suspension_positive_int($value)
{
    if (is_int($value)) return $value > 0 ? $value : false;
    $value = trim((string)$value);
    if (preg_match('/^[1-9][0-9]*$/D', $value) !== 1) return false;
    $number = (int)$value;
    return $number > 0 && (string)$number === $value ? $number : false;
}
function hrm_lifecycle_suspension_date($value)
{
    $value = trim((string)$value);
    if (preg_match('/^[0-9]{4}-[0-9]{2}-[0-9]{2}$/D', $value) !== 1) return false;
    $date = DateTime::createFromFormat('!Y-m-d', $value);
    $errors = DateTime::getLastErrors();
    if (!$date || (is_array($errors) && ((int)$errors['warning_count'] !== 0 || (int)$errors['error_count'] !== 0))
        || $date->format('Y-m-d') !== $value) return false;
    return $value;
}
function hrm_lifecycle_suspension_today_sql()
{
    if (function_exists('Today') && function_exists('date2sql')) {
        $today = date2sql(Today());
        if (hrm_lifecycle_suspension_date($today) !== false) return $today;
    }
    return date('Y-m-d');
}
function hrm_lifecycle_suspension_idempotency_hash($key)
{
    if (!is_string($key) || strlen($key) < 16 || strlen($key) > 128
        || preg_match('/^[A-Za-z0-9._:-]+$/D', $key) !== 1) return false;
    return hash('sha256', $key);
}
function hrm_lifecycle_suspension_workflow_is_maker_checker()
{
    if (!defined('ST_HRM_LIFECYCLE_SUSPENSION') || !class_exists('ApprovalWorkflow')
        || !ApprovalWorkflow::isRegistered(ST_HRM_LIFECYCLE_SUSPENSION)
        || !function_exists('get_approval_workflow_by_type') || !function_exists('get_approval_levels')) return false;
    $workflow = get_approval_workflow_by_type(ST_HRM_LIFECYCLE_SUSPENSION);
    if (!$workflow || (int)$workflow['is_active'] !== 1 || (int)$workflow['allow_self_approve'] !== 0) return false;
    $levels = get_approval_levels((int)$workflow['id'], true);
    if (!$levels) return false;
    $count = 0;
    while ($level = db_fetch($levels)) {
        $count++;
        if ((int)$level['is_active'] !== 1 || (int)$level['role_id'] <= 0
            || (int)$level['min_approvers'] < 1 || (float)$level['amount_threshold'] != 0.0
            || (isset($level['conditions']) && trim((string)$level['conditions']) !== '')) return false;
    }
    return $count > 0;
}
function hrm_lifecycle_suspension_employee_for_update($employee_number, $employee_id)
{
    if ((int)$employee_number <= 0 || trim((string)$employee_id) === '') return false;
    $result = db_query("SELECT employee_number,employee_id,department_id,position_id,grade_id,hire_date,inactive,released_date "
        ."FROM ".TB_PREF."employees WHERE employee_number=".(int)$employee_number
        ." AND employee_id=".db_escape((string)$employee_id)." LIMIT 1 FOR UPDATE",
        'could not lock Employee Suspension employee projection');
    return $result ? db_fetch_assoc($result) : false;
}

/**
 * Lock the latest normalized Suspension/Reinstatement history marker.
 * An empty array means this Employee has no service-state history yet; false
 * means the history query itself failed and must fail the command closed.
 *
 * @param string $employee_id
 * @return array|false
 */
function hrm_lifecycle_suspension_last_service_state_for_update($employee_id)
{
    $employee_id = trim((string)$employee_id);
    if ($employee_id === '') return false;
    $result = db_query("SELECT history_id,change_type,effective_date,reason FROM ".TB_PREF."employee_history "
        ."WHERE employee_id=".db_escape($employee_id)." AND change_type IN ("
        .db_escape(HRM_HIST_SUSPENSION).",".db_escape(HRM_HIST_REINSTATE).") "
        ."ORDER BY history_id DESC LIMIT 1 FOR UPDATE",
        'could not lock Employee Suspension lifecycle predecessor history');
    if (!$result) return false;
    $row = db_fetch_assoc($result);
    return $row ? $row : array();
}

/**
 * Require any prior service-state history to be the completed lifecycle-owned
 * Reinstatement that returned this same Worker/Employment/Assignment to active.
 * This prevents a later Suspension from being back-dated across its return edge.
 *
 * @param array $last_state
 * @param int $worker_id
 * @param int $employment_id
 * @param int $assignment_id
 * @return array|false
 */
function hrm_lifecycle_suspension_reinstatement_predecessor_for_update(
    $last_state, $worker_id, $employment_id, $assignment_id
) {
    if (!is_array($last_state) || !$last_state
        || (string)$last_state['change_type'] !== HRM_HIST_REINSTATE
        || preg_match('/^lifecycle_command:([1-9][0-9]*)$/D', (string)$last_state['reason'], $match) !== 1)
        return false;
    $command_id = (int)$match[1];
    $result = db_query("SELECT lifecycle_command_id,worker_id,employment_id,assignment_id,command_type,"
        ."command_status,effective_at,row_version FROM ".TB_PREF."hrm_lifecycle_commands "
        ."WHERE lifecycle_command_id=".$command_id." LIMIT 1 FOR UPDATE",
        'could not lock Employee Suspension Reinstatement predecessor command');
    $command = $result ? db_fetch_assoc($result) : false;
    if (!$command || (string)$command['command_type'] !== 'employee_reinstatement'
        || (string)$command['command_status'] !== 'completed' || (int)$command['row_version'] !== 2
        || (int)$command['worker_id'] !== (int)$worker_id
        || (int)$command['employment_id'] !== (int)$employment_id
        || (int)$command['assignment_id'] !== (int)$assignment_id
        || (string)$command['effective_at'] !== (string)$last_state['effective_date'].' 00:00:00')
        return false;
    return $command;
}

function hrm_lifecycle_suspension_request_hash($company, $worker_id, $employment_id, $assignment_id, $employee_number, $effective_date)
{
    foreach (array($worker_id,$employment_id,$assignment_id,$employee_number) as $value)
        if (hrm_lifecycle_suspension_positive_int((string)$value) === false) return false;
    if ((int)$company < 0 || hrm_lifecycle_suspension_date($effective_date) === false) return false;
    return hash('sha256', HRM_LIFECYCLE_SUSPENSION_HASH_CONTRACT.'|v='.HRM_LIFECYCLE_SUSPENSION_HASH_VERSION
        .'|company='.(int)$company.'|worker_id='.(int)$worker_id.'|employment_id='.(int)$employment_id
        .'|assignment_id='.(int)$assignment_id.'|employee_number='.(int)$employee_number
        .'|effective_date='.$effective_date);
}
function hrm_lifecycle_suspension_command_for_update($command_id)
{
    $command_id = hrm_lifecycle_suspension_positive_int($command_id);
    if ($command_id === false) return false;
    $result = db_query("SELECT lifecycle_command_id,worker_id,employment_id,assignment_id,command_type,command_status,"
        ."effective_at,idempotency_key_hash,request_hash,approval_draft_id,row_version,requested_at,requested_by,"
        ."completed_at,completed_by FROM ".TB_PREF."hrm_lifecycle_commands WHERE lifecycle_command_id=".$command_id
        ." LIMIT 1 FOR UPDATE", 'could not lock Employee Suspension lifecycle command');
    return $result ? db_fetch_assoc($result) : false;
}
function hrm_lifecycle_suspension_by_idempotency_for_update($worker_id, $idempotency_hash)
{
    if ((int)$worker_id <= 0 || !is_string($idempotency_hash)
        || preg_match('/^[a-f0-9]{64}$/D', $idempotency_hash) !== 1) return false;
    $result = db_query("SELECT lifecycle_command_id,worker_id,employment_id,assignment_id,command_type,command_status,"
        ."effective_at,idempotency_key_hash,request_hash,approval_draft_id,row_version,requested_at,requested_by,"
        ."completed_at,completed_by FROM ".TB_PREF."hrm_lifecycle_commands WHERE worker_id=".(int)$worker_id
        ." AND command_type=".db_escape(HRM_LIFECYCLE_SUSPENSION_COMMAND_TYPE)
        ." AND idempotency_key_hash=".db_escape($idempotency_hash)." LIMIT 1 FOR UPDATE",
        'could not lock idempotent Employee Suspension lifecycle command');
    return $result ? db_fetch_assoc($result) : false;
}
function hrm_lifecycle_suspension_other_pending_exists($worker_id)
{
    $result = db_query("SELECT lifecycle_command_id,command_type FROM ".TB_PREF."hrm_lifecycle_commands WHERE worker_id=".(int)$worker_id
        ." AND command_status='pending' ORDER BY lifecycle_command_id LIMIT 1 FOR UPDATE",
        'could not inspect pending Employee lifecycle conflict');
    return $result && db_fetch_assoc($result) ? true : false;
}
function hrm_lifecycle_suspension_audit($action, $reason_code, $command, $legal_entity_id, $restricted_diff)
{
    if (!function_exists('append_domain_audit_event') || !is_array($command)
        || (int)$command['lifecycle_command_id'] <= 0 || (int)$command['worker_id'] <= 0
        || (int)$command['employment_id'] <= 0 || (int)$command['assignment_id'] <= 0 || (int)$legal_entity_id < 0) return false;
    $safe = array_merge(array(
        'fixed_command_type'=>'employee_suspension','central_maker_checker'=>true,
        'legacy_employee_active_release_fields_changed'=>false,'assignment_rows_changed'=>0,
        'compensation_fields_changed'=>0,'payroll_calculation_invoked'=>false,
        'payroll_post_authority_changed'=>false,'payment_execution_authority_changed'=>false,
        'bank_transaction_authority_changed'=>false,'gl_accounting_authority_changed'=>false,
        'historical_payroll_accounting_rows_rewritten'=>0,'comment_text_stored'=>false,
        'free_text_service_state_reason_stored'=>false,'browser_mutation_route_added'=>false,
    ), is_array($restricted_diff) ? $restricted_diff : array());
    return append_domain_audit_event(array(
        'company_id'=>function_exists('user_company') ? (int)user_company() : 0,
        'legal_entity_id'=>(int)$legal_entity_id,'entity_type'=>'hrm_lifecycle_command',
        'entity_key'=>array('lifecycle_command_id'=>(int)$command['lifecycle_command_id'],
            'worker_id'=>(int)$command['worker_id'],'employment_id'=>(int)$command['employment_id'],
            'assignment_id'=>(int)$command['assignment_id']),
        'action'=>$action,'reason_code'=>$reason_code,'before'=>array(),
        'after'=>array('command_status'=>(string)$command['command_status'],'row_version'=>(int)$command['row_version']),
        'restricted_diff'=>$safe,
    )) !== false;
}
function hrm_lifecycle_suspension_draft_matches($draft_data, $command, $allow_terminal_version)
{
    if (!is_array($draft_data) || !is_array($command)) return false;
    $expected_keys = array('assignment_id','command_row_version','effective_date','employee_number','employment_id',
        'lifecycle_command_id','request_hash','worker_id');
    $keys = array_keys($draft_data); sort($keys, SORT_STRING);
    if ($keys !== $expected_keys) return false;
    foreach (array('lifecycle_command_id','worker_id','employment_id','assignment_id','employee_number') as $key)
        if (hrm_lifecycle_suspension_positive_int($draft_data[$key]) === false) return false;
    $draft_version = hrm_lifecycle_suspension_positive_int($draft_data['command_row_version']);
    $terminal_successor = $allow_terminal_version && (int)$command['row_version'] === 2 && $draft_version === 1;
    if ($draft_version === false || ($draft_version !== (int)$command['row_version'] && !$terminal_successor)
        || hrm_lifecycle_suspension_date($draft_data['effective_date']) === false
        || !is_string($draft_data['request_hash']) || preg_match('/^[a-f0-9]{64}$/D', $draft_data['request_hash']) !== 1) return false;
    if ((int)$draft_data['lifecycle_command_id'] !== (int)$command['lifecycle_command_id']
        || (int)$draft_data['worker_id'] !== (int)$command['worker_id']
        || (int)$draft_data['employment_id'] !== (int)$command['employment_id']
        || (int)$draft_data['assignment_id'] !== (int)$command['assignment_id']
        || (string)$draft_data['effective_date'].' 00:00:00' !== (string)$command['effective_at']
        || (string)$draft_data['request_hash'] !== (string)$command['request_hash']) return false;
    $recomputed = hrm_lifecycle_suspension_request_hash(function_exists('user_company') ? (int)user_company() : 0,
        (int)$draft_data['worker_id'],(int)$draft_data['employment_id'],(int)$draft_data['assignment_id'],
        (int)$draft_data['employee_number'],(string)$draft_data['effective_date']);
    return is_string($recomputed) && $recomputed === (string)$command['request_hash'];
}

/** Submit one fixed Employee Suspension lifecycle command. */
function submit_hrm_lifecycle_employee_suspension($employee_id, $effective_date, $idempotency_key)
{
    $employee_id = trim((string)$employee_id);
    $effective_date = hrm_lifecycle_suspension_date($effective_date);
    $idempotency_hash = hrm_lifecycle_suspension_idempotency_hash($idempotency_key);
    $actor = hrm_lifecycle_suspension_actor_id();
    $company = function_exists('user_company') ? (int)user_company() : 0;
    if ($employee_id === '' || strlen($employee_id) > 20 || $effective_date === false || $idempotency_hash === false
        || $actor <= 0 || $company < 0
        || !hrm_lifecycle_suspension_permission()
        || !function_exists('get_hrm_person_worker_binding') || !function_exists('get_hrm_legacy_current_employment')
        || !function_exists('get_hrm_legacy_current_assignment') || !function_exists('get_approval_workflow_service')) return false;
    $ok = false; $return_value = false; begin_transaction();
    do {
        $binding = get_hrm_person_worker_binding($employee_id, true);
        if (!$binding || !empty($binding['retired_at']) || (int)$binding['employee_number'] <= 0 || (int)$binding['worker_id'] <= 0) break;
        $employee_number = (int)$binding['employee_number'];
        $existing = hrm_lifecycle_suspension_by_idempotency_for_update(
            (int)$binding['worker_id'], $idempotency_hash);
        if (is_array($existing)) {
            $retry_hash = hrm_lifecycle_suspension_request_hash($company,
                (int)$existing['worker_id'], (int)$existing['employment_id'],
                (int)$existing['assignment_id'], $employee_number, $effective_date);
            if ((int)$existing['requested_by'] !== $actor
                || (string)$existing['request_hash'] !== (string)$retry_hash
                || (int)$existing['approval_draft_id'] <= 0)
                break;
            $ok = true;
            $return_value = array('lifecycle_command_id'=>(int)$existing['lifecycle_command_id'],
                'approval_draft_id'=>(int)$existing['approval_draft_id'],
                'status'=>(string)$existing['command_status'],
                'row_version'=>(int)$existing['row_version'],'request_hash'=>$retry_hash,
                'exact_retry'=>true);
            break;
        }
        if (!hrm_lifecycle_suspension_workflow_is_maker_checker()
            || strcmp($effective_date, hrm_lifecycle_suspension_today_sql()) > 0)
            break;
        $employment = get_hrm_legacy_current_employment($employee_number, true);
        $assignment = get_hrm_legacy_current_assignment($employee_number, true);
        $employee = hrm_lifecycle_suspension_employee_for_update($employee_number, $employee_id);
        if (!$employment || !$assignment || !$employee
            || (int)$employment['worker_id'] !== (int)$binding['worker_id']
            || (int)$assignment['employment_id'] !== (int)$employment['employment_id']
            || (int)$assignment['legacy_employee_number'] !== $employee_number
            || (int)$assignment['legacy_current'] !== 1 || (int)$employment['legacy_current'] !== 1
            || (int)$assignment['primary_flag'] !== 1 || (string)$assignment['assignment_type_code'] !== 'legacy_primary'
            || (int)$employee['inactive'] !== 0 || ($employee['released_date'] !== null && $employee['released_date'] !== '')
            || (string)$employment['employment_status'] !== 'active'
            || ($employment['end_date'] !== null && $employment['end_date'] !== '')
            || ($assignment['effective_to'] !== null && $assignment['effective_to'] !== '')
            || (int)$employee['department_id'] !== (int)$assignment['legacy_department_id']
            || (int)$employee['position_id'] !== (int)$assignment['legacy_position_id']
            || (int)$employee['grade_id'] !== (int)$assignment['legacy_grade_id']) break;
        if ($employment['start_date'] !== null && $employment['start_date'] !== '' && strcmp($effective_date, (string)$employment['start_date']) < 0) break;
        if ($assignment['effective_from'] !== null && $assignment['effective_from'] !== '' && strcmp($effective_date, (string)$assignment['effective_from']) < 0) break;
        if ($employee['hire_date'] !== null && $employee['hire_date'] !== '' && strcmp($effective_date, (string)$employee['hire_date']) < 0) break;
        $last_state = hrm_lifecycle_suspension_last_service_state_for_update($employee_id);
        if ($last_state === false) break;
        if ($last_state && (!hrm_lifecycle_suspension_reinstatement_predecessor_for_update(
                $last_state, (int)$binding['worker_id'], (int)$employment['employment_id'],
                (int)$assignment['assignment_id'])
            || strcmp($effective_date, (string)$last_state['effective_date']) < 0))
            break;
        $request_hash = hrm_lifecycle_suspension_request_hash($company,(int)$binding['worker_id'],(int)$employment['employment_id'],
            (int)$assignment['assignment_id'],$employee_number,$effective_date);
        if ($request_hash === false || hrm_lifecycle_suspension_other_pending_exists((int)$binding['worker_id'])) break;
        if (!db_query("INSERT INTO ".TB_PREF."hrm_lifecycle_commands "
            ."(worker_id,employment_id,assignment_id,command_type,command_status,effective_at,idempotency_key_hash,"
            ."request_hash,approval_draft_id,row_version,requested_by) VALUES ("
            .(int)$binding['worker_id'].",".(int)$employment['employment_id'].",".(int)$assignment['assignment_id'].','
            .db_escape(HRM_LIFECYCLE_SUSPENSION_COMMAND_TYPE).",'pending',".db_escape($effective_date.' 00:00:00').','
            .db_escape($idempotency_hash).','.db_escape($request_hash).",NULL,1,".$actor.")",
            'could not create Employee Suspension lifecycle command')) break;
        $command_id = (int)db_insert_id(); if ($command_id <= 0) break;
        $draft_data = array('lifecycle_command_id'=>$command_id,'command_row_version'=>1,
            'worker_id'=>(int)$binding['worker_id'],'employment_id'=>(int)$employment['employment_id'],
            'assignment_id'=>(int)$assignment['assignment_id'],'employee_number'=>$employee_number,
            'effective_date'=>$effective_date,'request_hash'=>$request_hash);
        $approval_result = get_approval_workflow_service()->submit(ST_HRM_LIFECYCLE_SUSPENSION, $draft_data, 0,
            array('summary'=>sprintf(_('Employee Suspension lifecycle command #%d'), $command_id)));
        if (!is_array($approval_result) || !isset($approval_result['status'],$approval_result['draft_id'])
            || (string)$approval_result['status'] !== 'pending' || (int)$approval_result['draft_id'] <= 0) break;
        $draft_id = (int)$approval_result['draft_id'];
        if (!db_query("UPDATE ".TB_PREF."hrm_lifecycle_commands SET approval_draft_id=".$draft_id
            ." WHERE lifecycle_command_id=".$command_id." AND approval_draft_id IS NULL AND row_version=1 AND command_status='pending'",
            'could not bind Employee Suspension lifecycle command to approval draft') || (int)db_num_affected_rows() !== 1) break;
        $command = array('lifecycle_command_id'=>$command_id,'worker_id'=>(int)$binding['worker_id'],
            'employment_id'=>(int)$employment['employment_id'],'assignment_id'=>(int)$assignment['assignment_id'],
            'command_status'=>'pending','row_version'=>1);
        if (!hrm_lifecycle_suspension_audit('hrm.lifecycle.employee_suspension.submitted', HRM_LIFECYCLE_SUSPENSION_REASON,
            $command,(int)$employment['legal_entity_id'],array('approval_draft_id'=>$draft_id,'maker_user_id'=>$actor,
                'request_hash'=>$request_hash,'idempotency_key_plaintext_stored'=>false,'effective_date_bound'=>true,
                'worker_employment_assignment_bound'=>true,'same_subject_conflict_guard'=>true,
                'employment_status_precondition'=>'active','employment_status_successor'=>'suspended'))) break;
        $ok = true;
        $return_value = array('lifecycle_command_id'=>$command_id,'approval_draft_id'=>$draft_id,
            'status'=>'pending','row_version'=>1,'request_hash'=>$request_hash,'exact_retry'=>false);
    } while (false);
    if ($ok) commit_transaction(); else cancel_transaction();
    return $ok ? $return_value : false;
}
function hrm_lifecycle_suspension_bound_draft($draft_data, $draft_id, $allow_terminal_version)
{
    $draft_id = hrm_lifecycle_suspension_positive_int($draft_id);
    if ($draft_id === false || !is_array($draft_data) || !isset($draft_data['lifecycle_command_id'])) return false;
    $command = hrm_lifecycle_suspension_command_for_update($draft_data['lifecycle_command_id']);
    if (!$command || (string)$command['command_type'] !== HRM_LIFECYCLE_SUSPENSION_COMMAND_TYPE
        || (int)$command['approval_draft_id'] !== $draft_id
        || !hrm_lifecycle_suspension_draft_matches($draft_data,$command,$allow_terminal_version)) return false;
    return $command;
}
function execute_approved_hrm_lifecycle_suspension($draft_data, $reserved_trans_no, $draft_id)
{
    if (!hrm_lifecycle_suspension_permission()) return false;
    $actor = hrm_lifecycle_suspension_actor_id();
    $command = hrm_lifecycle_suspension_bound_draft($draft_data,$draft_id,true);
    if (!$command || $actor <= 0) return false;
    if ((string)$command['command_status'] === 'completed' && (int)$command['row_version'] === 2) return (int)$command['lifecycle_command_id'];
    if ((string)$command['command_status'] !== 'pending' || (int)$command['row_version'] !== 1
        || (int)$command['requested_by'] <= 0 || (int)$command['requested_by'] === $actor) return false;
    $effective_date = (string)$draft_data['effective_date'];
    if (strcmp($effective_date, hrm_lifecycle_suspension_today_sql()) > 0) return false;
    $employee_number = (int)$draft_data['employee_number'];
    $binding = get_hrm_person_worker_binding($employee_number,true);
    $employment = get_hrm_legacy_current_employment($employee_number,true);
    $assignment = get_hrm_legacy_current_assignment($employee_number,true);
    if (!$binding || !$employment || !$assignment || !empty($binding['retired_at'])
        || (int)$binding['worker_id'] !== (int)$command['worker_id']
        || (int)$employment['employment_id'] !== (int)$command['employment_id']
        || (int)$employment['worker_id'] !== (int)$command['worker_id']
        || (int)$assignment['assignment_id'] !== (int)$command['assignment_id']
        || (int)$assignment['employment_id'] !== (int)$command['employment_id']
        || (int)$assignment['legacy_current'] !== 1 || (int)$employment['legacy_current'] !== 1
        || (int)$assignment['primary_flag'] !== 1 || (string)$assignment['assignment_type_code'] !== 'legacy_primary'
        || (string)$employment['employment_status'] !== 'active'
        || ($employment['end_date'] !== null && $employment['end_date'] !== '')
        || ($assignment['effective_to'] !== null && $assignment['effective_to'] !== '')) return false;
    $employee_id = (string)$binding['employee_id'];
    $employee = hrm_lifecycle_suspension_employee_for_update($employee_number,$employee_id);
    if (!$employee || (int)$employee['inactive'] !== 0 || ($employee['released_date'] !== null && $employee['released_date'] !== '')
        || (int)$employee['department_id'] !== (int)$assignment['legacy_department_id']
        || (int)$employee['position_id'] !== (int)$assignment['legacy_position_id']
        || (int)$employee['grade_id'] !== (int)$assignment['legacy_grade_id']) return false;
    if (($employment['start_date'] !== null && $employment['start_date'] !== '' && strcmp($effective_date,(string)$employment['start_date']) < 0)
        || ($assignment['effective_from'] !== null && $assignment['effective_from'] !== '' && strcmp($effective_date,(string)$assignment['effective_from']) < 0)
        || ($employee['hire_date'] !== null && $employee['hire_date'] !== '' && strcmp($effective_date,(string)$employee['hire_date']) < 0)) return false;
    $last_state = hrm_lifecycle_suspension_last_service_state_for_update($employee_id);
    if ($last_state === false) return false;
    if ($last_state && (!hrm_lifecycle_suspension_reinstatement_predecessor_for_update(
            $last_state, (int)$command['worker_id'], (int)$command['employment_id'],
            (int)$command['assignment_id'])
        || strcmp($effective_date, (string)$last_state['effective_date']) < 0))
        return false;
    if (!execute_hrm_employment_suspension($employee_number,(int)$employment['employment_id'],(int)$employment['worker_id'],$effective_date)) return false;
    $approved_by = isset($_SESSION['wa_current_user']->loginname) ? (string)$_SESSION['wa_current_user']->loginname : '';
    $history_id = add_employee_history($employee_id, HRM_HIST_SUSPENSION, $effective_date,
        (int)$employee['department_id'],(int)$employee['department_id'],(int)$employee['position_id'],(int)$employee['position_id'],
        (int)$employee['grade_id'],(int)$employee['grade_id'],null,null,
        'lifecycle_command:'.(int)$command['lifecycle_command_id'],$approved_by);
    if ((int)$history_id <= 0) return false;
    if (!db_query("UPDATE ".TB_PREF."hrm_lifecycle_commands SET command_status='completed',row_version=2,"
        ."completed_at=UTC_TIMESTAMP(),completed_by=".$actor." WHERE lifecycle_command_id=".(int)$command['lifecycle_command_id']
        ." AND command_status='pending' AND row_version=1 AND approval_draft_id=".(int)$draft_id,
        'could not complete Employee Suspension lifecycle command') || (int)db_num_affected_rows() !== 1) return false;
    $command['command_status']='completed'; $command['row_version']=2;
    if (!hrm_lifecycle_suspension_audit('hrm.lifecycle.employee_suspension.completed', HRM_LIFECYCLE_SUSPENSION_REASON,
        $command,(int)$employment['legal_entity_id'],array('approval_draft_id'=>(int)$draft_id,
            'maker_user_id'=>(int)$command['requested_by'],'checker_user_id'=>$actor,'maker_checker_separated'=>true,
            'employee_history_id'=>(int)$history_id,'employment_state_transition'=>'active->suspended',
            'employment_rows_mutated'=>1,'legacy_employee_rows_mutated'=>0,'assignment_rows_mutated'=>0,
            'request_hash'=>(string)$command['request_hash']))) return false;
    return (int)$command['lifecycle_command_id'];
}
function hrm_lifecycle_suspension_terminalize($draft_data, $draft_id, $status, $maker_only)
{
    if (!hrm_lifecycle_suspension_permission() || !in_array($status,array('rejected','cancelled'),true)) return false;
    $actor = hrm_lifecycle_suspension_actor_id(); $command = hrm_lifecycle_suspension_bound_draft($draft_data,$draft_id,true);
    if (!$command || $actor <= 0) return false;
    if ((string)$command['command_status'] === $status && (int)$command['row_version'] === 2) return (int)$command['lifecycle_command_id'];
    if ((string)$command['command_status'] !== 'pending' || (int)$command['row_version'] !== 1
        || ($maker_only && (int)$command['requested_by'] !== $actor)
        || (!$maker_only && (int)$command['requested_by'] === $actor)) return false;
    $employment = get_hrm_legacy_current_employment((int)$draft_data['employee_number'],true);
    if (!$employment || (int)$employment['employment_id'] !== (int)$command['employment_id']) return false;
    if (!db_query("UPDATE ".TB_PREF."hrm_lifecycle_commands SET command_status=".db_escape($status)
        .",row_version=2,completed_at=UTC_TIMESTAMP(),completed_by=".$actor
        ." WHERE lifecycle_command_id=".(int)$command['lifecycle_command_id']
        ." AND command_status='pending' AND row_version=1 AND approval_draft_id=".(int)$draft_id,
        'could not terminalize Employee Suspension lifecycle command') || (int)db_num_affected_rows() !== 1) return false;
    $command['command_status']=$status; $command['row_version']=2;
    $action = $status === 'rejected' ? 'hrm.lifecycle.employee_suspension.rejected' : 'hrm.lifecycle.employee_suspension.cancelled';
    $terminal_reason = $status === 'rejected' ? 'hrm_fnd_005_employee_suspension_command_rejection' : 'hrm_fnd_005_employee_suspension_command_cancellation';
    return hrm_lifecycle_suspension_audit($action,$terminal_reason,$command,(int)$employment['legal_entity_id'],array(
        'approval_draft_id'=>(int)$draft_id,'actor_user_id'=>$actor,'maker_only'=>$maker_only,
        'maker_checker_separated'=>$maker_only ? false : true,'business_employee_rows_mutated'=>0,
        'business_employment_rows_mutated'=>0,'business_assignment_rows_mutated'=>0,'employee_history_rows_added'=>0))
        ? (int)$command['lifecycle_command_id'] : false;
}
function reject_hrm_lifecycle_suspension_draft($draft_data, $draft_id, $comments)
{ return hrm_lifecycle_suspension_terminalize($draft_data,$draft_id,'rejected',false); }
function cancel_hrm_lifecycle_suspension_draft($draft_data, $draft_id, $comments)
{ return hrm_lifecycle_suspension_terminalize($draft_data,$draft_id,'cancelled',true); }
