<?php
/**
 * HRM-FND-005 fixed Employee Probation Schedule lifecycle command.
 *
 * This command owns only the scheduled probation boundary stored in
 * employees.probation_end_date. It may establish a missing future schedule or
 * extend an existing still-current/future schedule after independent central
 * approval. It does not represent probation completion/failure and does not
 * alter confirmation, employment type/status, Employment/Assignment chronology,
 * compensation, payroll, payment, bank, or accounting data.
 */
$hrm_lifecycle_probation_schedule_root = isset($path_to_root)
    ? $path_to_root : dirname(dirname(dirname(__DIR__)));
$hrm_lifecycle_probation_schedule_dependencies = array(
    array('/includes/approval/approval_workflow.inc', 'class', 'ApprovalWorkflow'),
    array('/includes/approval/db/approval_rules_db.inc', 'function', 'get_approval_workflow_by_type'),
    array('/includes/approval/db/approval_db.inc', 'function', 'get_approval_draft_for_update'),
    array('/includes/db/domain_audit_db.inc', 'function', 'append_domain_audit_event'),
    array('/hrm/includes/db/employee_db.inc', 'function', 'update_employee'),
    array('/hrm/includes/db/employee_person_worker_db.inc', 'function', 'get_hrm_person_worker_binding'),
    array('/hrm/includes/db/employee_employment_db.inc', 'function', 'get_hrm_legacy_current_employment'),
    array('/hrm/includes/db/employee_assignment_db.inc', 'function', 'get_hrm_legacy_current_assignment'),
);
foreach ($hrm_lifecycle_probation_schedule_dependencies as $dependency) {
    $available = $dependency[1] === 'class'
        ? class_exists($dependency[2]) : function_exists($dependency[2]);
    if ($available)
        continue;
    $candidate = $hrm_lifecycle_probation_schedule_root.$dependency[0];
    if (is_file($candidate))
        include_once($candidate);
}
unset($hrm_lifecycle_probation_schedule_root, $hrm_lifecycle_probation_schedule_dependencies,
    $dependency, $candidate, $available);

if (!defined('HRM_LIFECYCLE_PROBATION_SCHEDULE_COMMAND_TYPE'))
    define('HRM_LIFECYCLE_PROBATION_SCHEDULE_COMMAND_TYPE', 'employee_probation_schedule');
if (!defined('HRM_LIFECYCLE_PROBATION_SCHEDULE_HASH_CONTRACT'))
    define('HRM_LIFECYCLE_PROBATION_SCHEDULE_HASH_CONTRACT', 'notrinoserp.hrm-fnd-005.employee-probation-schedule');
if (!defined('HRM_LIFECYCLE_PROBATION_SCHEDULE_REASON'))
    define('HRM_LIFECYCLE_PROBATION_SCHEDULE_REASON', 'hrm_fnd_005_employee_probation_schedule_command');

/** @return int */
function hrm_lifecycle_probation_schedule_actor_id()
{
    return isset($_SESSION['wa_current_user']) && is_object($_SESSION['wa_current_user'])
        && isset($_SESSION['wa_current_user']->user) ? (int)$_SESSION['wa_current_user']->user : 0;
}

/** @return bool */
function hrm_lifecycle_probation_schedule_permission()
{
    return function_exists('user_check_access') && user_check_access('SA_EMPLOYEE');
}

/** @param mixed $value @return int|false */
function hrm_lifecycle_probation_schedule_positive_int($value)
{
    if (is_int($value))
        return $value > 0 ? $value : false;
    $value = trim((string)$value);
    if (preg_match('/^[1-9][0-9]*$/D', $value) !== 1)
        return false;
    $number = (int)$value;
    return $number > 0 && (string)$number === $value ? $number : false;
}

/** @param mixed $value @return string|false */
function hrm_lifecycle_probation_schedule_date($value)
{
    $value = trim((string)$value);
    if (preg_match('/^[0-9]{4}-[0-9]{2}-[0-9]{2}$/D', $value) !== 1)
        return false;
    $date = DateTime::createFromFormat('!Y-m-d', $value);
    $errors = DateTime::getLastErrors();
    if (!$date || (is_array($errors)
        && ((int)$errors['warning_count'] !== 0 || (int)$errors['error_count'] !== 0))
        || $date->format('Y-m-d') !== $value)
        return false;
    return $value;
}

/** @param mixed $value @return string|false */
function hrm_lifecycle_probation_schedule_reason_value($value)
{
    $value = trim(strip_tags((string)$value));
    return $value !== '' && strlen($value) <= 500 ? $value : false;
}

/** @param mixed $key @return string|false */
function hrm_lifecycle_probation_schedule_idempotency_hash($key)
{
    if (!is_string($key) || strlen($key) < 16 || strlen($key) > 128
        || preg_match('/^[A-Za-z0-9._:-]+$/D', $key) !== 1)
        return false;
    return hash('sha256', $key);
}

/** @return bool */
function hrm_lifecycle_probation_schedule_workflow_is_maker_checker()
{
    if (!defined('ST_HRM_LIFECYCLE_PROBATION_SCHEDULE') || !class_exists('ApprovalWorkflow')
        || !ApprovalWorkflow::isRegistered(ST_HRM_LIFECYCLE_PROBATION_SCHEDULE)
        || !function_exists('get_approval_workflow_by_type') || !function_exists('get_approval_levels'))
        return false;
    $workflow = get_approval_workflow_by_type(ST_HRM_LIFECYCLE_PROBATION_SCHEDULE);
    if (!$workflow || (int)$workflow['is_active'] !== 1 || (int)$workflow['allow_self_approve'] !== 0)
        return false;
    $levels = get_approval_levels((int)$workflow['id'], true);
    if (!$levels)
        return false;
    $count = 0;
    while ($level = db_fetch($levels)) {
        $count++;
        if ((int)$level['is_active'] !== 1 || (int)$level['role_id'] <= 0
            || (int)$level['min_approvers'] < 1 || (float)$level['amount_threshold'] != 0.0
            || (isset($level['conditions']) && trim((string)$level['conditions']) !== ''))
            return false;
    }
    return $count > 0;
}

/** @param int $employee_number @param string $employee_id @return array|false */
function hrm_lifecycle_probation_schedule_employee_for_update($employee_number, $employee_id)
{
    $result = db_query("SELECT employee_number,employee_id,hire_date,confirmation_date,probation_end_date,"
        ."employment_type,department_id,position_id,grade_id,inactive,released_date FROM ".TB_PREF."employees "
        ."WHERE employee_number=".(int)$employee_number." AND employee_id=".db_escape((string)$employee_id)
        ." LIMIT 1 FOR UPDATE", 'could not lock Employee Probation Schedule employee');
    return $result ? db_fetch_assoc($result) : false;
}

/** @param array $employee @return string|false */
function hrm_lifecycle_probation_schedule_employee_fingerprint($employee)
{
    if (!is_array($employee) || (int)$employee['employee_number'] <= 0
        || trim((string)$employee['employee_id']) === '')
        return false;
    return hash('sha256', implode('|', array(
        'employee-probation-schedule-v1', (int)$employee['employee_number'], (string)$employee['employee_id'],
        (string)$employee['hire_date'], (string)$employee['confirmation_date'],
        (string)$employee['probation_end_date'], (int)$employee['employment_type'],
        (int)$employee['department_id'], (int)$employee['position_id'], (int)$employee['grade_id'],
        (int)$employee['inactive'], (string)$employee['released_date'],
    )));
}

/** @return string|false */
function hrm_lifecycle_probation_schedule_request_hash($company, $worker_id, $employment_id, $assignment_id,
    $employee_number, $employee_fingerprint, $effective_date, $reason_hash)
{
    if ((int)$company < 0 || (int)$worker_id <= 0 || (int)$employment_id <= 0
        || (int)$assignment_id <= 0 || (int)$employee_number <= 0
        || !is_string($employee_fingerprint)
        || preg_match('/^[a-f0-9]{64}$/D', $employee_fingerprint) !== 1
        || hrm_lifecycle_probation_schedule_date($effective_date) === false
        || !is_string($reason_hash) || preg_match('/^[a-f0-9]{64}$/D', $reason_hash) !== 1)
        return false;
    return hash('sha256', HRM_LIFECYCLE_PROBATION_SCHEDULE_HASH_CONTRACT
        .'|company='.(int)$company.'|worker_id='.(int)$worker_id
        .'|employment_id='.(int)$employment_id.'|assignment_id='.(int)$assignment_id
        .'|employee_number='.(int)$employee_number.'|employee_fingerprint='.$employee_fingerprint
        .'|effective_date='.$effective_date.'|reason_hash='.$reason_hash);
}

/** @param int $command_id @return array|false */
function hrm_lifecycle_probation_schedule_command_for_update($command_id)
{
    $command_id = hrm_lifecycle_probation_schedule_positive_int($command_id);
    if ($command_id === false)
        return false;
    $result = db_query("SELECT lifecycle_command_id,worker_id,employment_id,assignment_id,command_type,"
        ."command_status,effective_at,idempotency_key_hash,request_hash,approval_draft_id,row_version,"
        ."requested_by,completed_by FROM ".TB_PREF."hrm_lifecycle_commands WHERE lifecycle_command_id="
        .$command_id." LIMIT 1 FOR UPDATE", 'could not lock Employee Probation Schedule command');
    return $result ? db_fetch_assoc($result) : false;
}

/** @param int $worker_id @param string $idempotency_hash @return array|false */
function hrm_lifecycle_probation_schedule_by_idempotency_for_update($worker_id, $idempotency_hash)
{
    $result = db_query("SELECT lifecycle_command_id,worker_id,employment_id,assignment_id,command_type,"
        ."command_status,effective_at,idempotency_key_hash,request_hash,approval_draft_id,row_version,"
        ."requested_by,completed_by FROM ".TB_PREF."hrm_lifecycle_commands WHERE worker_id=".(int)$worker_id
        ." AND command_type=".db_escape(HRM_LIFECYCLE_PROBATION_SCHEDULE_COMMAND_TYPE)
        ." AND idempotency_key_hash=".db_escape($idempotency_hash)." LIMIT 1 FOR UPDATE",
        'could not lock idempotent Employee Probation Schedule command');
    return $result ? db_fetch_assoc($result) : false;
}

/** @param int $worker_id @return bool */
function hrm_lifecycle_probation_schedule_other_pending_exists($worker_id)
{
    $result = db_query("SELECT lifecycle_command_id FROM ".TB_PREF."hrm_lifecycle_commands WHERE worker_id="
        .(int)$worker_id." AND command_status='pending' ORDER BY lifecycle_command_id LIMIT 1 FOR UPDATE",
        'could not inspect pending Employee lifecycle command');
    return $result && db_fetch_assoc($result) ? true : false;
}

/** @return bool */
function hrm_lifecycle_probation_schedule_audit($action, $command, $legal_entity_id, $restricted_diff)
{
    if (!function_exists('append_domain_audit_event') || !is_array($command))
        return false;
    $safe = array_merge(array(
        'fixed_command_type'=>HRM_LIFECYCLE_PROBATION_SCHEDULE_COMMAND_TYPE,
        'central_maker_checker'=>true, 'reason_plaintext_audited'=>false,
        'confirmation_date_fields_changed'=>0, 'probation_date_fields_changed'=>0,
        'employment_type_fields_changed'=>0, 'employee_status_fields_changed'=>0,
        'employment_rows_changed'=>0, 'assignment_rows_changed'=>0,
        'compensation_fields_changed'=>0, 'payroll_rows_changed'=>0,
        'payment_rows_changed'=>0, 'bank_rows_changed'=>0, 'gl_accounting_rows_changed'=>0,
    ), is_array($restricted_diff) ? $restricted_diff : array());
    return append_domain_audit_event(array(
        'company_id'=>function_exists('user_company') ? (int)user_company() : 0,
        'legal_entity_id'=>(int)$legal_entity_id, 'entity_type'=>'hrm_lifecycle_command',
        'entity_key'=>array('lifecycle_command_id'=>(int)$command['lifecycle_command_id'],
            'worker_id'=>(int)$command['worker_id'], 'employment_id'=>(int)$command['employment_id'],
            'assignment_id'=>(int)$command['assignment_id']),
        'action'=>$action, 'reason_code'=>HRM_LIFECYCLE_PROBATION_SCHEDULE_REASON,
        'before'=>array(), 'after'=>array('command_status'=>(string)$command['command_status'],
            'row_version'=>(int)$command['row_version']), 'restricted_diff'=>$safe,
    )) !== false;
}

/** @return bool */
function hrm_lifecycle_probation_schedule_draft_matches($draft_data, $command, $allow_terminal_version)
{
    if (!is_array($draft_data) || !is_array($command))
        return false;
    $expected = array('assignment_id','command_row_version','effective_date','employee_fingerprint',
        'employee_number','employment_id','lifecycle_command_id','reason','reason_hash','request_hash','worker_id');
    $keys = array_keys($draft_data);
    sort($keys, SORT_STRING);
    if ($keys !== $expected)
        return false;
    $draft_version = hrm_lifecycle_probation_schedule_positive_int($draft_data['command_row_version']);
    $terminal = $allow_terminal_version && (int)$command['row_version'] === 2 && $draft_version === 1;
    $reason = hrm_lifecycle_probation_schedule_reason_value($draft_data['reason']);
    if ($draft_version === false || ($draft_version !== (int)$command['row_version'] && !$terminal)
        || $reason === false || hash('sha256', $reason) !== (string)$draft_data['reason_hash']
        || (int)$draft_data['lifecycle_command_id'] !== (int)$command['lifecycle_command_id']
        || (int)$draft_data['worker_id'] !== (int)$command['worker_id']
        || (int)$draft_data['employment_id'] !== (int)$command['employment_id']
        || (int)$draft_data['assignment_id'] !== (int)$command['assignment_id']
        || (string)$draft_data['effective_date'].' 00:00:00' !== (string)$command['effective_at'])
        return false;
    $hash = hrm_lifecycle_probation_schedule_request_hash(function_exists('user_company') ? user_company() : 0,
        $draft_data['worker_id'], $draft_data['employment_id'], $draft_data['assignment_id'],
        $draft_data['employee_number'], $draft_data['employee_fingerprint'],
        $draft_data['effective_date'], $draft_data['reason_hash']);
    return $hash !== false && $hash === (string)$command['request_hash']
        && $hash === (string)$draft_data['request_hash'];
}

/**
 * Recover one exact submit retry from the immutable approval draft.
 *
 * A completed command has already changed probation_end_date, which is part of
 * the original employee fingerprint. Exact retry therefore validates the
 * original request against the locked approval draft rather than recomputing a
 * new fingerprint from the mutated employee row.
 *
 * @return array|false
 */
function hrm_lifecycle_probation_schedule_retry_matches($existing, $employee_number,
    $effective_date, $reason, $actor)
{
    if (!is_array($existing) || (int)$existing['approval_draft_id'] <= 0
        || (int)$existing['requested_by'] !== (int)$actor
        || !function_exists('get_approval_draft_for_update'))
        return false;
    $draft = get_approval_draft_for_update((int)$existing['approval_draft_id']);
    if (!$draft || !isset($draft['draft_data']))
        return false;
    $data = json_decode(html_entity_decode((string)$draft['draft_data'], ENT_QUOTES, 'UTF-8'), true);
    if (!is_array($data)
        || (int)$data['employee_number'] !== (int)$employee_number
        || (string)$data['effective_date'] !== (string)$effective_date
        || (string)$data['reason'] !== (string)$reason
        || !hrm_lifecycle_probation_schedule_draft_matches($data, $existing, true))
        return false;
    return array('lifecycle_command_id'=>(int)$existing['lifecycle_command_id'],
        'approval_draft_id'=>(int)$existing['approval_draft_id'],
        'status'=>(string)$existing['command_status'],
        'row_version'=>(int)$existing['row_version'], 'exact_retry'=>true);
}

/**
 * Submit one Employee Probation Schedule establishment/extension for central maker/checker approval.
 *
 * @return array|false
 */
function submit_hrm_lifecycle_employee_probation_schedule($employee_id, $effective_date, $reason, $idempotency_key)
{
    $employee_id = trim((string)$employee_id);
    $effective_date = hrm_lifecycle_probation_schedule_date($effective_date);
    $reason = hrm_lifecycle_probation_schedule_reason_value($reason);
    $idempotency_hash = hrm_lifecycle_probation_schedule_idempotency_hash($idempotency_key);
    $actor = hrm_lifecycle_probation_schedule_actor_id();
    if ($employee_id === '' || strlen($employee_id) > 20
        || preg_match('/^[A-Za-z0-9_-]+$/D', $employee_id) !== 1
        || $effective_date === false || $reason === false || $idempotency_hash === false || $actor <= 0
        || strcmp($effective_date, gmdate('Y-m-d')) <= 0
        || !hrm_lifecycle_probation_schedule_permission()
        || !hrm_lifecycle_probation_schedule_workflow_is_maker_checker()
        || !function_exists('get_hrm_person_worker_binding')
        || !function_exists('get_hrm_legacy_current_employment')
        || !function_exists('get_hrm_legacy_current_assignment')
        || !function_exists('get_approval_workflow_service'))
        return false;

    $ok = false;
    $return_value = false;
    begin_transaction();
    do {
        $binding = get_hrm_person_worker_binding($employee_id, true);
        if (!$binding || !empty($binding['retired_at']) || (int)$binding['employee_number'] <= 0)
            break;
        $employee_number = (int)$binding['employee_number'];
        $employment = get_hrm_legacy_current_employment($employee_number, true);
        $assignment = get_hrm_legacy_current_assignment($employee_number, true);
        $employee = hrm_lifecycle_probation_schedule_employee_for_update($employee_number, $employee_id);
        if (!$employment || !$assignment || !$employee
            || (int)$employment['worker_id'] !== (int)$binding['worker_id']
            || (int)$assignment['employment_id'] !== (int)$employment['employment_id']
            || (int)$assignment['legacy_employee_number'] !== $employee_number
            || (int)$assignment['legacy_current'] !== 1 || (int)$employment['legacy_current'] !== 1
            || (int)$assignment['primary_flag'] !== 1
            || (string)$assignment['assignment_type_code'] !== 'legacy_primary'
            || (int)$employee['department_id'] !== (int)$assignment['legacy_department_id']
            || (int)$employee['position_id'] !== (int)$assignment['legacy_position_id']
            || (int)$employee['grade_id'] !== (int)$assignment['legacy_grade_id']
            || (int)$employee['inactive'] !== 0 || !empty($employee['released_date'])
            || !empty($employee['confirmation_date']) || empty($employee['hire_date'])
            || strcmp($effective_date, (string)$employee['hire_date']) < 0)
            break;
        if (!empty($employment['start_date']) && strcmp($effective_date, (string)$employment['start_date']) < 0)
            break;
        if (!empty($employment['end_date']) && strcmp($effective_date, (string)$employment['end_date']) > 0)
            break;
        if (!empty($assignment['effective_from']) && strcmp($effective_date, (string)$assignment['effective_from']) < 0)
            break;
        if (!empty($assignment['effective_to']) && strcmp($effective_date, (string)$assignment['effective_to']) > 0)
            break;

        $existing = hrm_lifecycle_probation_schedule_by_idempotency_for_update(
            $binding['worker_id'], $idempotency_hash);
        if (is_array($existing)) {
            if ((int)$existing['employment_id'] !== (int)$employment['employment_id']
                || (int)$existing['assignment_id'] !== (int)$assignment['assignment_id'])
                break;
            $retry = hrm_lifecycle_probation_schedule_retry_matches(
                $existing, $employee_number, $effective_date, $reason, $actor);
            if ($retry === false)
                break;
            $ok = true;
            $return_value = $retry;
            break;
        }

        $current_schedule = trim((string)$employee['probation_end_date']);
        if ($current_schedule !== '') {
            if (hrm_lifecycle_probation_schedule_date($current_schedule) === false
                || strcmp($current_schedule, gmdate('Y-m-d')) < 0
                || strcmp($effective_date, $current_schedule) <= 0)
                break;
        }
        $fingerprint = hrm_lifecycle_probation_schedule_employee_fingerprint($employee);
        if ($fingerprint === false || hrm_lifecycle_probation_schedule_other_pending_exists($binding['worker_id']))
            break;
        $reason_hash = hash('sha256', $reason);
        $request_hash = hrm_lifecycle_probation_schedule_request_hash(
            function_exists('user_company') ? user_company() : 0,
            $binding['worker_id'], $employment['employment_id'], $assignment['assignment_id'],
            $employee_number, $fingerprint, $effective_date, $reason_hash);
        if ($request_hash === false)
            break;
        if (!db_query("INSERT INTO ".TB_PREF."hrm_lifecycle_commands (worker_id,employment_id,assignment_id,"
            ."command_type,command_status,effective_at,idempotency_key_hash,request_hash,approval_draft_id,"
            ."row_version,requested_by) VALUES (".(int)$binding['worker_id'].','
            .(int)$employment['employment_id'].','.(int)$assignment['assignment_id'].','
            .db_escape(HRM_LIFECYCLE_PROBATION_SCHEDULE_COMMAND_TYPE).",'pending',"
            .db_escape($effective_date.' 00:00:00').','.db_escape($idempotency_hash).','
            .db_escape($request_hash).",NULL,1,".$actor.")", 'could not create Employee Probation Schedule command'))
            break;
        $command_id = (int)db_insert_id();
        if ($command_id <= 0)
            break;
        $draft_data = array('lifecycle_command_id'=>$command_id, 'command_row_version'=>1,
            'worker_id'=>(int)$binding['worker_id'], 'employment_id'=>(int)$employment['employment_id'],
            'assignment_id'=>(int)$assignment['assignment_id'], 'employee_number'=>$employee_number,
            'employee_fingerprint'=>$fingerprint, 'effective_date'=>$effective_date,
            'reason'=>$reason, 'reason_hash'=>$reason_hash, 'request_hash'=>$request_hash);
        $approval_result = get_approval_workflow_service()->submit(
            ST_HRM_LIFECYCLE_PROBATION_SCHEDULE, $draft_data, 0,
            array('summary'=>sprintf(_('Employee probation schedule lifecycle command #%d'), $command_id)));
        if (!is_array($approval_result) || !isset($approval_result['status'], $approval_result['draft_id'])
            || (string)$approval_result['status'] !== 'pending' || (int)$approval_result['draft_id'] <= 0)
            break;
        $draft_id = (int)$approval_result['draft_id'];
        if (!db_query("UPDATE ".TB_PREF."hrm_lifecycle_commands SET approval_draft_id=".$draft_id
            ." WHERE lifecycle_command_id=".$command_id
            ." AND approval_draft_id IS NULL AND row_version=1 AND command_status='pending'",
            'could not bind Employee Probation Schedule approval') || (int)db_num_affected_rows() !== 1)
            break;
        $command = array('lifecycle_command_id'=>$command_id, 'worker_id'=>(int)$binding['worker_id'],
            'employment_id'=>(int)$employment['employment_id'],
            'assignment_id'=>(int)$assignment['assignment_id'], 'command_status'=>'pending', 'row_version'=>1);
        if (!hrm_lifecycle_probation_schedule_audit('hrm.lifecycle.probation_schedule.submitted', $command,
            (int)$employment['legal_entity_id'], array('approval_draft_id'=>$draft_id,
                'maker_user_id'=>$actor, 'employee_fingerprint_bound'=>true,
                'reason_hash_bound'=>true, 'same_subject_conflict_guard'=>true,
                'schedule_operation'=>$current_schedule === '' ? 'establish' : 'extend',
                'existing_employee_route_redirected'=>false)))
            break;
        $ok = true;
        $return_value = array('lifecycle_command_id'=>$command_id, 'approval_draft_id'=>$draft_id,
            'status'=>'pending', 'row_version'=>1, 'exact_retry'=>false);
    } while (false);
    if ($ok)
        commit_transaction();
    else
        cancel_transaction();
    return $ok ? $return_value : false;
}

/** @return array|false */
function hrm_lifecycle_probation_schedule_bound_draft($draft_data, $draft_id, $allow_terminal_version)
{
    $draft_id = hrm_lifecycle_probation_schedule_positive_int($draft_id);
    if ($draft_id === false || !is_array($draft_data) || !isset($draft_data['lifecycle_command_id']))
        return false;
    $command = hrm_lifecycle_probation_schedule_command_for_update($draft_data['lifecycle_command_id']);
    if (!$command || (string)$command['command_type'] !== HRM_LIFECYCLE_PROBATION_SCHEDULE_COMMAND_TYPE
        || (int)$command['approval_draft_id'] !== $draft_id
        || !hrm_lifecycle_probation_schedule_draft_matches($draft_data, $command, $allow_terminal_version))
        return false;
    return $command;
}

/** @return int|false */
function execute_approved_hrm_lifecycle_probation_schedule($draft_data, $reserved_trans_no, $draft_id)
{
    if (!hrm_lifecycle_probation_schedule_permission())
        return false;
    $actor = hrm_lifecycle_probation_schedule_actor_id();
    $command = hrm_lifecycle_probation_schedule_bound_draft($draft_data, $draft_id, true);
    if (!$command || $actor <= 0 || (int)$command['requested_by'] === $actor)
        return false;
    if ((string)$command['command_status'] === 'completed' && (int)$command['row_version'] === 2)
        return (int)$command['lifecycle_command_id'];
    if ((string)$command['command_status'] !== 'pending' || (int)$command['row_version'] !== 1)
        return false;

    $binding = get_hrm_person_worker_binding((int)$draft_data['employee_number'], true);
    $employment = get_hrm_legacy_current_employment((int)$draft_data['employee_number'], true);
    $assignment = get_hrm_legacy_current_assignment((int)$draft_data['employee_number'], true);
    if (!$binding || !$employment || !$assignment
        || (int)$binding['worker_id'] !== (int)$command['worker_id']
        || (int)$employment['employment_id'] !== (int)$command['employment_id']
        || (int)$employment['worker_id'] !== (int)$binding['worker_id']
        || (int)$assignment['assignment_id'] !== (int)$command['assignment_id']
        || (int)$assignment['employment_id'] !== (int)$employment['employment_id']
        || (int)$assignment['legacy_current'] !== 1 || (int)$employment['legacy_current'] !== 1
        || (int)$assignment['primary_flag'] !== 1
        || (string)$assignment['assignment_type_code'] !== 'legacy_primary')
        return false;
    $employee_id = (string)$binding['employee_id'];
    $employee = hrm_lifecycle_probation_schedule_employee_for_update(
        (int)$draft_data['employee_number'], $employee_id);
    $effective_date = (string)$draft_data['effective_date'];
    if (!$employee || (int)$employee['inactive'] !== 0 || !empty($employee['released_date'])
        || !empty($employee['confirmation_date']) || empty($employee['hire_date'])
        || (int)$employee['department_id'] !== (int)$assignment['legacy_department_id']
        || (int)$employee['position_id'] !== (int)$assignment['legacy_position_id']
        || (int)$employee['grade_id'] !== (int)$assignment['legacy_grade_id']
        || hrm_lifecycle_probation_schedule_employee_fingerprint($employee)
            !== (string)$draft_data['employee_fingerprint']
        || strcmp($effective_date, gmdate('Y-m-d')) <= 0
        || strcmp($effective_date, (string)$employee['hire_date']) < 0)
        return false;
    $current_schedule = trim((string)$employee['probation_end_date']);
    if ($current_schedule !== '' && (hrm_lifecycle_probation_schedule_date($current_schedule) === false
        || strcmp($current_schedule, gmdate('Y-m-d')) < 0
        || strcmp($effective_date, $current_schedule) <= 0))
        return false;
    if (!empty($employment['start_date']) && strcmp($effective_date, (string)$employment['start_date']) < 0)
        return false;
    if (!empty($employment['end_date']) && strcmp($effective_date, (string)$employment['end_date']) > 0)
        return false;
    if (!empty($assignment['effective_from']) && strcmp($effective_date, (string)$assignment['effective_from']) < 0)
        return false;
    if (!empty($assignment['effective_to']) && strcmp($effective_date, (string)$assignment['effective_to']) > 0)
        return false;

    if (!update_employee($employee_id, array('probation_end_date'=>$effective_date)))
        return false;
    $written = hrm_lifecycle_probation_schedule_employee_for_update(
        (int)$draft_data['employee_number'], $employee_id);
    if (!$written || (string)$written['probation_end_date'] !== $effective_date
        || (string)$written['confirmation_date'] !== (string)$employee['confirmation_date']
        || (string)$written['hire_date'] !== (string)$employee['hire_date']
        || (int)$written['employment_type'] !== (int)$employee['employment_type']
        || (int)$written['department_id'] !== (int)$employee['department_id']
        || (int)$written['position_id'] !== (int)$employee['position_id']
        || (int)$written['grade_id'] !== (int)$employee['grade_id']
        || (int)$written['inactive'] !== (int)$employee['inactive']
        || (string)$written['released_date'] !== (string)$employee['released_date'])
        return false;
    if (!db_query("UPDATE ".TB_PREF."hrm_lifecycle_commands SET command_status='completed',row_version=2,"
        ."completed_at=UTC_TIMESTAMP(),completed_by=".$actor." WHERE lifecycle_command_id="
        .(int)$command['lifecycle_command_id']." AND command_status='pending' AND row_version=1"
        ." AND approval_draft_id=".(int)$draft_id,
        'could not complete Employee Probation Schedule command') || (int)db_num_affected_rows() !== 1)
        return false;
    $command['command_status'] = 'completed';
    $command['row_version'] = 2;
    if (!hrm_lifecycle_probation_schedule_audit('hrm.lifecycle.probation_schedule.completed', $command,
        (int)$employment['legal_entity_id'], array('approval_draft_id'=>(int)$draft_id,
            'maker_user_id'=>(int)$command['requested_by'], 'checker_user_id'=>$actor,
            'maker_checker_separated'=>true, 'probation_date_fields_changed'=>1,
            'confirmation_date_fields_changed'=>0,
            'schedule_operation'=>$current_schedule === '' ? 'establish' : 'extend',
            'employee_fingerprint_revalidated'=>true, 'request_hash'=>(string)$command['request_hash'])))
        return false;
    return (int)$command['lifecycle_command_id'];
}

/** @return int|false */
function hrm_lifecycle_probation_schedule_terminalize($draft_data, $draft_id, $status, $maker_only)
{
    if (!hrm_lifecycle_probation_schedule_permission()
        || !in_array($status, array('rejected','cancelled'), true))
        return false;
    $actor = hrm_lifecycle_probation_schedule_actor_id();
    $command = hrm_lifecycle_probation_schedule_bound_draft($draft_data, $draft_id, true);
    if (!$command || $actor <= 0)
        return false;
    if ((string)$command['command_status'] === $status && (int)$command['row_version'] === 2)
        return (int)$command['lifecycle_command_id'];
    if ((string)$command['command_status'] !== 'pending' || (int)$command['row_version'] !== 1
        || ($maker_only && (int)$command['requested_by'] !== $actor)
        || (!$maker_only && (int)$command['requested_by'] === $actor))
        return false;
    $employment = get_hrm_legacy_current_employment((int)$draft_data['employee_number'], true);
    if (!$employment || (int)$employment['employment_id'] !== (int)$command['employment_id'])
        return false;
    if (!db_query("UPDATE ".TB_PREF."hrm_lifecycle_commands SET command_status=".db_escape($status)
        .",row_version=2,completed_at=UTC_TIMESTAMP(),completed_by=".$actor
        ." WHERE lifecycle_command_id=".(int)$command['lifecycle_command_id']
        ." AND command_status='pending' AND row_version=1 AND approval_draft_id=".(int)$draft_id,
        'could not terminalize Employee Probation Schedule command') || (int)db_num_affected_rows() !== 1)
        return false;
    $command['command_status'] = $status;
    $command['row_version'] = 2;
    return hrm_lifecycle_probation_schedule_audit('hrm.lifecycle.probation_schedule.'.$status, $command,
        (int)$employment['legal_entity_id'], array('approval_draft_id'=>(int)$draft_id,
            'terminal_actor_user_id'=>$actor, 'maker_only'=>$maker_only,
            'probation_date_fields_changed'=>0, 'confirmation_date_fields_changed'=>0))
        ? (int)$command['lifecycle_command_id'] : false;
}

/** @return int|false */
function reject_hrm_lifecycle_probation_schedule_draft($draft_data, $draft_id, $comments)
{
    return hrm_lifecycle_probation_schedule_terminalize($draft_data, $draft_id, 'rejected', false);
}

/** @return int|false */
function cancel_hrm_lifecycle_probation_schedule_draft($draft_data, $draft_id, $comments)
{
    return hrm_lifecycle_probation_schedule_terminalize($draft_data, $draft_id, 'cancelled', true);
}
