<?php
/**
 * HRM-FND-005 fixed Employee Training Assignment lifecycle command.
 *
 * The command binds one active Worker/Employment/primary Assignment to one
 * active training-course version and delegates final insertion to the existing
 * training writer only after independent central approval. It is intentionally
 * not a generic training, checklist, recruitment, or employee-status workflow.
 */
$hrm_lifecycle_training_root = isset($path_to_root)
    ? $path_to_root : dirname(dirname(dirname(__DIR__)));
$hrm_lifecycle_training_dependencies = array(
    array('/includes/approval/approval_workflow.inc', 'class', 'ApprovalWorkflow'),
    array('/includes/approval/db/approval_rules_db.inc', 'function', 'get_approval_workflow_by_type'),
    array('/includes/db/domain_audit_db.inc', 'function', 'append_domain_audit_event'),
    array('/hrm/includes/db/training_db.inc', 'function', 'add_employee_training'),
    array('/hrm/includes/db/employee_person_worker_db.inc', 'function', 'get_hrm_person_worker_binding'),
    array('/hrm/includes/db/employee_employment_db.inc', 'function', 'get_hrm_legacy_current_employment'),
    array('/hrm/includes/db/employee_assignment_db.inc', 'function', 'get_hrm_legacy_current_assignment'),
);
foreach ($hrm_lifecycle_training_dependencies as $dependency) {
    if ($dependency[1] === 'class')
        $available = class_exists($dependency[2]);
    else
        $available = function_exists($dependency[2]);
    if ($available)
        continue;
    $candidate = $hrm_lifecycle_training_root.$dependency[0];
    if (is_file($candidate))
        include_once($candidate);
}
unset($hrm_lifecycle_training_root, $hrm_lifecycle_training_dependencies, $dependency, $candidate, $available);

if (!defined('HRM_LIFECYCLE_TRAINING_ASSIGNMENT_COMMAND_TYPE'))
    define('HRM_LIFECYCLE_TRAINING_ASSIGNMENT_COMMAND_TYPE', 'employee_training_assignment');
if (!defined('HRM_LIFECYCLE_TRAINING_ASSIGNMENT_HASH_CONTRACT'))
    define('HRM_LIFECYCLE_TRAINING_ASSIGNMENT_HASH_CONTRACT', 'notrinoserp.hrm-fnd-005.employee-training-assignment');
if (!defined('HRM_LIFECYCLE_TRAINING_ASSIGNMENT_REASON'))
    define('HRM_LIFECYCLE_TRAINING_ASSIGNMENT_REASON', 'hrm_fnd_005_employee_training_assignment_command');

/** @return int */
function hrm_lifecycle_training_assignment_actor_id()
{
    return isset($_SESSION['wa_current_user']) && is_object($_SESSION['wa_current_user'])
        && isset($_SESSION['wa_current_user']->user) ? (int)$_SESSION['wa_current_user']->user : 0;
}

/** @return bool */
function hrm_lifecycle_training_assignment_permission()
{
    return function_exists('user_check_access') && user_check_access('SA_HRSETTINGS');
}

/** @param mixed $value @return int|false */
function hrm_lifecycle_training_assignment_positive_int($value)
{
    if (is_int($value))
        return $value > 0 ? $value : false;
    $value = trim((string)$value);
    if (preg_match('/^[1-9][0-9]*$/D', $value) !== 1)
        return false;
    $number = (int)$value;
    return $number > 0 && (string)$number === $value ? $number : false;
}

/** @param mixed $value @return int|false */
function hrm_lifecycle_training_assignment_status($value)
{
    $value = is_int($value) ? $value : trim((string)$value);
    if (!in_array($value, array(0, 1, 2, 3, '0', '1', '2', '3'), true))
        return false;
    return (int)$value;
}

/** @param mixed $value @param bool $allow_empty @param bool $nonnegative @return string|false */
function hrm_lifecycle_training_assignment_decimal($value, $allow_empty, $nonnegative)
{
    $value = trim((string)$value);
    if ($allow_empty && $value === '')
        return '';
    if (preg_match('/^-?(?:0|[1-9][0-9]{0,11})(?:\.[0-9]{1,6})?$/D', $value) !== 1)
        return false;
    $canonical = strpos($value, '.') === false ? $value : rtrim(rtrim($value, '0'), '.');
    if ($canonical === '' || $canonical === '-0')
        $canonical = '0';
    if ($nonnegative && $canonical[0] === '-')
        return false;
    return $canonical;
}

/** @param mixed $value @param bool $allow_empty @return string|false */
function hrm_lifecycle_training_assignment_date($value, $allow_empty)
{
    $value = trim((string)$value);
    if ($allow_empty && $value === '')
        return '';
    if (preg_match('/^[0-9]{4}-[0-9]{2}-[0-9]{2}$/D', $value) !== 1)
        return false;
    $date = DateTime::createFromFormat('!Y-m-d', $value);
    $errors = DateTime::getLastErrors();
    if (!$date || (is_array($errors) && ((int)$errors['warning_count'] !== 0 || (int)$errors['error_count'] !== 0))
        || $date->format('Y-m-d') !== $value)
        return false;
    return $value;
}

/** @param mixed $value @return string|false */
function hrm_lifecycle_training_assignment_remarks($value)
{
    $value = trim(strip_tags((string)$value));
    return strlen($value) <= 1000 ? $value : false;
}

/** @param mixed $key @return string|false */
function hrm_lifecycle_training_assignment_idempotency_hash($key)
{
    if (!is_string($key) || strlen($key) < 16 || strlen($key) > 128
        || preg_match('/^[A-Za-z0-9._:-]+$/D', $key) !== 1)
        return false;
    return hash('sha256', $key);
}

/** @return bool */
function hrm_lifecycle_training_assignment_workflow_is_maker_checker()
{
    if (!defined('ST_HRM_LIFECYCLE_TRAINING_ASSIGNMENT') || !class_exists('ApprovalWorkflow')
        || !ApprovalWorkflow::isRegistered(ST_HRM_LIFECYCLE_TRAINING_ASSIGNMENT)
        || !function_exists('get_approval_workflow_by_type') || !function_exists('get_approval_levels'))
        return false;
    $workflow = get_approval_workflow_by_type(ST_HRM_LIFECYCLE_TRAINING_ASSIGNMENT);
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
function hrm_lifecycle_training_assignment_employee_for_update($employee_number, $employee_id)
{
    $result = db_query("SELECT employee_number,employee_id,department_id,position_id,grade_id,inactive,released_date FROM "
        .TB_PREF."employees WHERE employee_number=".(int)$employee_number
        ." AND employee_id=".db_escape((string)$employee_id)." LIMIT 1 FOR UPDATE",
        'could not lock Employee Training Assignment employee');
    return $result ? db_fetch_assoc($result) : false;
}

/** @param int $course_id @return array|false */
function hrm_lifecycle_training_assignment_course_for_update($course_id)
{
    $result = db_query("SELECT course_id,course_code,course_name,provider,default_hours,default_cost,inactive FROM "
        .TB_PREF."training_courses WHERE course_id=".(int)$course_id." LIMIT 1 FOR UPDATE",
        'could not lock Employee Training Assignment course');
    return $result ? db_fetch_assoc($result) : false;
}

/** @param array $course @return string|false */
function hrm_lifecycle_training_assignment_course_fingerprint($course)
{
    if (!is_array($course) || (int)$course['course_id'] <= 0 || (int)$course['inactive'] !== 0)
        return false;
    return hash('sha256', implode('|', array(
        'course-v1', (int)$course['course_id'], (string)$course['course_code'],
        (string)$course['course_name'], (string)$course['provider'],
        (string)$course['default_hours'], (string)$course['default_cost'], (int)$course['inactive'],
    )));
}

/** @return string|false */
function hrm_lifecycle_training_assignment_request_hash($company, $worker_id, $employment_id, $assignment_id,
    $employee_number, $course_id, $course_fingerprint, $training_date, $completion_date, $status,
    $score, $cost_amount, $remarks_hash)
{
    if ((int)$company < 0 || (int)$worker_id <= 0 || (int)$employment_id <= 0 || (int)$assignment_id <= 0
        || (int)$employee_number <= 0 || (int)$course_id <= 0
        || !is_string($course_fingerprint) || preg_match('/^[a-f0-9]{64}$/D', $course_fingerprint) !== 1
        || hrm_lifecycle_training_assignment_date($training_date, false) === false
        || hrm_lifecycle_training_assignment_date($completion_date, true) === false
        || hrm_lifecycle_training_assignment_status($status) === false
        || hrm_lifecycle_training_assignment_decimal($score, true, false) === false
        || hrm_lifecycle_training_assignment_decimal($cost_amount, false, true) === false
        || !is_string($remarks_hash) || preg_match('/^[a-f0-9]{64}$/D', $remarks_hash) !== 1)
        return false;
    return hash('sha256', HRM_LIFECYCLE_TRAINING_ASSIGNMENT_HASH_CONTRACT
        .'|company='.(int)$company.'|worker_id='.(int)$worker_id.'|employment_id='.(int)$employment_id
        .'|assignment_id='.(int)$assignment_id.'|employee_number='.(int)$employee_number
        .'|course_id='.(int)$course_id.'|course_fingerprint='.$course_fingerprint
        .'|training_date='.$training_date.'|completion_date='.$completion_date.'|status='.(int)$status
        .'|score='.$score.'|cost_amount='.$cost_amount.'|remarks_hash='.$remarks_hash);
}

/** @param int $command_id @return array|false */
function hrm_lifecycle_training_assignment_command_for_update($command_id)
{
    $command_id = hrm_lifecycle_training_assignment_positive_int($command_id);
    if ($command_id === false)
        return false;
    $result = db_query("SELECT lifecycle_command_id,worker_id,employment_id,assignment_id,command_type,command_status,"
        ."effective_at,idempotency_key_hash,request_hash,approval_draft_id,row_version,requested_by,completed_by "
        ."FROM ".TB_PREF."hrm_lifecycle_commands WHERE lifecycle_command_id=".$command_id." LIMIT 1 FOR UPDATE",
        'could not lock Employee Training Assignment command');
    return $result ? db_fetch_assoc($result) : false;
}

/** @param int $worker_id @param string $idempotency_hash @return array|false */
function hrm_lifecycle_training_assignment_by_idempotency_for_update($worker_id, $idempotency_hash)
{
    $result = db_query("SELECT lifecycle_command_id,worker_id,employment_id,assignment_id,command_type,command_status,"
        ."effective_at,idempotency_key_hash,request_hash,approval_draft_id,row_version,requested_by,completed_by "
        ."FROM ".TB_PREF."hrm_lifecycle_commands WHERE worker_id=".(int)$worker_id
        ." AND command_type=".db_escape(HRM_LIFECYCLE_TRAINING_ASSIGNMENT_COMMAND_TYPE)
        ." AND idempotency_key_hash=".db_escape($idempotency_hash)." LIMIT 1 FOR UPDATE",
        'could not lock idempotent Employee Training Assignment command');
    return $result ? db_fetch_assoc($result) : false;
}

/** @param int $worker_id @return bool */
function hrm_lifecycle_training_assignment_other_pending_exists($worker_id)
{
    $result = db_query("SELECT lifecycle_command_id FROM ".TB_PREF."hrm_lifecycle_commands WHERE worker_id=".(int)$worker_id
        ." AND command_status='pending' ORDER BY lifecycle_command_id LIMIT 1 FOR UPDATE",
        'could not inspect pending Employee lifecycle command');
    return $result && db_fetch_assoc($result) ? true : false;
}

/** @return bool */
function hrm_lifecycle_training_assignment_audit($action, $command, $legal_entity_id, $restricted_diff)
{
    if (!function_exists('append_domain_audit_event') || !is_array($command))
        return false;
    $safe = array_merge(array(
        'fixed_command_type'=>HRM_LIFECYCLE_TRAINING_ASSIGNMENT_COMMAND_TYPE,
        'central_maker_checker'=>true, 'remarks_plaintext_audited'=>false,
        'recruitment_rows_changed'=>0, 'employee_status_fields_changed'=>0,
        'compensation_fields_changed'=>0, 'payroll_rows_changed'=>0,
        'payment_rows_changed'=>0, 'bank_rows_changed'=>0, 'gl_accounting_rows_changed'=>0,
    ), is_array($restricted_diff) ? $restricted_diff : array());
    return append_domain_audit_event(array(
        'company_id'=>function_exists('user_company') ? (int)user_company() : 0,
        'legal_entity_id'=>(int)$legal_entity_id,
        'entity_type'=>'hrm_lifecycle_command',
        'entity_key'=>array('lifecycle_command_id'=>(int)$command['lifecycle_command_id'],
            'worker_id'=>(int)$command['worker_id'], 'employment_id'=>(int)$command['employment_id'],
            'assignment_id'=>(int)$command['assignment_id']),
        'action'=>$action, 'reason_code'=>HRM_LIFECYCLE_TRAINING_ASSIGNMENT_REASON,
        'before'=>array(), 'after'=>array('command_status'=>(string)$command['command_status'],
            'row_version'=>(int)$command['row_version']), 'restricted_diff'=>$safe,
    )) !== false;
}

/** @return bool */
function hrm_lifecycle_training_assignment_draft_matches($draft_data, $command, $allow_terminal_version)
{
    if (!is_array($draft_data) || !is_array($command))
        return false;
    $expected = array('assignment_id','command_row_version','completion_date','cost_amount','course_fingerprint',
        'course_id','employee_number','employment_id','lifecycle_command_id','remarks','remarks_hash','request_hash',
        'score','status','training_date','worker_id');
    $keys = array_keys($draft_data); sort($keys, SORT_STRING);
    if ($keys !== $expected)
        return false;
    $draft_version = hrm_lifecycle_training_assignment_positive_int($draft_data['command_row_version']);
    $terminal = $allow_terminal_version && (int)$command['row_version'] === 2 && $draft_version === 1;
    $remarks = hrm_lifecycle_training_assignment_remarks($draft_data['remarks']);
    if ($draft_version === false || ($draft_version !== (int)$command['row_version'] && !$terminal)
        || $remarks === false || hash('sha256', $remarks) !== (string)$draft_data['remarks_hash']
        || (int)$draft_data['lifecycle_command_id'] !== (int)$command['lifecycle_command_id']
        || (int)$draft_data['worker_id'] !== (int)$command['worker_id']
        || (int)$draft_data['employment_id'] !== (int)$command['employment_id']
        || (int)$draft_data['assignment_id'] !== (int)$command['assignment_id']
        || (string)$draft_data['training_date'].' 00:00:00' !== (string)$command['effective_at'])
        return false;
    $hash = hrm_lifecycle_training_assignment_request_hash(function_exists('user_company') ? user_company() : 0,
        $draft_data['worker_id'], $draft_data['employment_id'], $draft_data['assignment_id'],
        $draft_data['employee_number'], $draft_data['course_id'], $draft_data['course_fingerprint'],
        $draft_data['training_date'], $draft_data['completion_date'], $draft_data['status'],
        $draft_data['score'], $draft_data['cost_amount'], $draft_data['remarks_hash']);
    return $hash !== false && $hash === (string)$command['request_hash']
        && $hash === (string)$draft_data['request_hash'];
}

/**
 * Submit one Employee Training Assignment for central maker/checker approval.
 *
 * @return array|false
 */
function submit_hrm_lifecycle_employee_training_assignment($employee_id, $course_id, $training_date,
    $completion_date, $status, $score, $cost_amount, $remarks, $idempotency_key)
{
    $employee_id = trim((string)$employee_id);
    $course_id = hrm_lifecycle_training_assignment_positive_int($course_id);
    $training_date = hrm_lifecycle_training_assignment_date($training_date, false);
    $completion_date = hrm_lifecycle_training_assignment_date($completion_date, true);
    $status = hrm_lifecycle_training_assignment_status($status);
    $score = hrm_lifecycle_training_assignment_decimal($score, true, false);
    $cost_amount = hrm_lifecycle_training_assignment_decimal($cost_amount, false, true);
    $remarks = hrm_lifecycle_training_assignment_remarks($remarks);
    $idempotency_hash = hrm_lifecycle_training_assignment_idempotency_hash($idempotency_key);
    $actor = hrm_lifecycle_training_assignment_actor_id();
    if ($employee_id === '' || strlen($employee_id) > 20 || preg_match('/^[A-Za-z0-9_-]+$/D', $employee_id) !== 1
        || $course_id === false || $training_date === false || $completion_date === false || $status === false
        || $score === false || $cost_amount === false || $remarks === false || $idempotency_hash === false
        || ($completion_date !== '' && strcmp($completion_date, $training_date) < 0) || $actor <= 0
        || !hrm_lifecycle_training_assignment_permission() || !hrm_lifecycle_training_assignment_workflow_is_maker_checker()
        || !function_exists('get_hrm_person_worker_binding') || !function_exists('get_hrm_legacy_current_employment')
        || !function_exists('get_hrm_legacy_current_assignment') || !function_exists('get_approval_workflow_service'))
        return false;

    $ok = false; $return_value = false; begin_transaction();
    do {
        $binding = get_hrm_person_worker_binding($employee_id, true);
        if (!$binding || !empty($binding['retired_at']) || (int)$binding['employee_number'] <= 0)
            break;
        $employee_number = (int)$binding['employee_number'];
        $employment = get_hrm_legacy_current_employment($employee_number, true);
        $assignment = get_hrm_legacy_current_assignment($employee_number, true);
        $employee = hrm_lifecycle_training_assignment_employee_for_update($employee_number, $employee_id);
        $course = hrm_lifecycle_training_assignment_course_for_update($course_id);
        $course_fingerprint = hrm_lifecycle_training_assignment_course_fingerprint($course);
        if (!$employment || !$assignment || !$employee || $course_fingerprint === false
            || (int)$employment['worker_id'] !== (int)$binding['worker_id']
            || (int)$assignment['employment_id'] !== (int)$employment['employment_id']
            || (int)$assignment['legacy_employee_number'] !== $employee_number
            || (int)$assignment['legacy_current'] !== 1 || (int)$employment['legacy_current'] !== 1
            || (int)$assignment['primary_flag'] !== 1 || (string)$assignment['assignment_type_code'] !== 'legacy_primary'
            || (int)$employee['department_id'] !== (int)$assignment['legacy_department_id']
            || (int)$employee['position_id'] !== (int)$assignment['legacy_position_id']
            || (int)$employee['grade_id'] !== (int)$assignment['legacy_grade_id']
            || (int)$employee['inactive'] !== 0 || !empty($employee['released_date']))
            break;
        if (!empty($employment['start_date']) && strcmp($training_date, (string)$employment['start_date']) < 0)
            break;
        if (!empty($employment['end_date']) && strcmp($training_date, (string)$employment['end_date']) > 0)
            break;
        if (!empty($assignment['effective_from']) && strcmp($training_date, (string)$assignment['effective_from']) < 0)
            break;
        if (!empty($assignment['effective_to']) && strcmp($training_date, (string)$assignment['effective_to']) > 0)
            break;

        $remarks_hash = hash('sha256', $remarks);
        $request_hash = hrm_lifecycle_training_assignment_request_hash(function_exists('user_company') ? user_company() : 0,
            $binding['worker_id'], $employment['employment_id'], $assignment['assignment_id'], $employee_number,
            $course_id, $course_fingerprint, $training_date, $completion_date, $status, $score, $cost_amount, $remarks_hash);
        if ($request_hash === false)
            break;
        $existing = hrm_lifecycle_training_assignment_by_idempotency_for_update($binding['worker_id'], $idempotency_hash);
        if (is_array($existing)) {
            if ((int)$existing['requested_by'] !== $actor || (string)$existing['request_hash'] !== $request_hash
                || (int)$existing['approval_draft_id'] <= 0)
                break;
            $ok = true;
            $return_value = array('lifecycle_command_id'=>(int)$existing['lifecycle_command_id'],
                'approval_draft_id'=>(int)$existing['approval_draft_id'], 'status'=>(string)$existing['command_status'],
                'row_version'=>(int)$existing['row_version'], 'exact_retry'=>true);
            break;
        }
        if (hrm_lifecycle_training_assignment_other_pending_exists($binding['worker_id']))
            break;
        if (!db_query("INSERT INTO ".TB_PREF."hrm_lifecycle_commands (worker_id,employment_id,assignment_id,"
            ."command_type,command_status,effective_at,idempotency_key_hash,request_hash,approval_draft_id,row_version,requested_by) VALUES ("
            .(int)$binding['worker_id'].','.(int)$employment['employment_id'].','.(int)$assignment['assignment_id'].','
            .db_escape(HRM_LIFECYCLE_TRAINING_ASSIGNMENT_COMMAND_TYPE).",'pending',".db_escape($training_date.' 00:00:00').','
            .db_escape($idempotency_hash).','.db_escape($request_hash).",NULL,1,".$actor.")",
            'could not create Employee Training Assignment command'))
            break;
        $command_id = (int)db_insert_id();
        if ($command_id <= 0)
            break;
        $draft_data = array('lifecycle_command_id'=>$command_id, 'command_row_version'=>1,
            'worker_id'=>(int)$binding['worker_id'], 'employment_id'=>(int)$employment['employment_id'],
            'assignment_id'=>(int)$assignment['assignment_id'], 'employee_number'=>$employee_number,
            'course_id'=>$course_id, 'course_fingerprint'=>$course_fingerprint, 'training_date'=>$training_date,
            'completion_date'=>$completion_date, 'status'=>$status, 'score'=>$score, 'cost_amount'=>$cost_amount,
            'remarks'=>$remarks, 'remarks_hash'=>$remarks_hash, 'request_hash'=>$request_hash);
        $approval = get_approval_workflow_service();
        $approval_result = $approval->submit(ST_HRM_LIFECYCLE_TRAINING_ASSIGNMENT, $draft_data, 0,
            array('summary'=>sprintf(_('Employee training assignment lifecycle command #%d'), $command_id)));
        if (!is_array($approval_result) || !isset($approval_result['status'],$approval_result['draft_id'])
            || (string)$approval_result['status'] !== 'pending' || (int)$approval_result['draft_id'] <= 0)
            break;
        $draft_id = (int)$approval_result['draft_id'];
        if (!db_query("UPDATE ".TB_PREF."hrm_lifecycle_commands SET approval_draft_id=".$draft_id
            ." WHERE lifecycle_command_id=".$command_id." AND approval_draft_id IS NULL AND row_version=1 AND command_status='pending'",
            'could not bind Employee Training Assignment approval') || (int)db_num_affected_rows() !== 1)
            break;
        $command = array('lifecycle_command_id'=>$command_id, 'worker_id'=>(int)$binding['worker_id'],
            'employment_id'=>(int)$employment['employment_id'], 'assignment_id'=>(int)$assignment['assignment_id'],
            'command_status'=>'pending', 'row_version'=>1);
        if (!hrm_lifecycle_training_assignment_audit('hrm.lifecycle.training_assignment.submitted', $command,
            (int)$employment['legal_entity_id'], array('approval_draft_id'=>$draft_id, 'maker_user_id'=>$actor,
                'course_fingerprint_bound'=>true, 'remarks_hash_bound'=>true, 'same_subject_conflict_guard'=>true,
                'existing_training_route_redirected'=>true)))
            break;
        $ok = true;
        $return_value = array('lifecycle_command_id'=>$command_id, 'approval_draft_id'=>$draft_id,
            'status'=>'pending', 'row_version'=>1, 'exact_retry'=>false);
    } while (false);
    if ($ok) commit_transaction(); else cancel_transaction();
    return $ok ? $return_value : false;
}

/** @return array|false */
function hrm_lifecycle_training_assignment_bound_draft($draft_data, $draft_id, $allow_terminal_version)
{
    $draft_id = hrm_lifecycle_training_assignment_positive_int($draft_id);
    if ($draft_id === false || !is_array($draft_data) || !isset($draft_data['lifecycle_command_id']))
        return false;
    $command = hrm_lifecycle_training_assignment_command_for_update($draft_data['lifecycle_command_id']);
    if (!$command || (string)$command['command_type'] !== HRM_LIFECYCLE_TRAINING_ASSIGNMENT_COMMAND_TYPE
        || (int)$command['approval_draft_id'] !== $draft_id
        || !hrm_lifecycle_training_assignment_draft_matches($draft_data, $command, $allow_terminal_version))
        return false;
    return $command;
}

/** @return int|false */
function execute_approved_hrm_lifecycle_training_assignment($draft_data, $reserved_trans_no, $draft_id)
{
    if (!hrm_lifecycle_training_assignment_permission())
        return false;
    $actor = hrm_lifecycle_training_assignment_actor_id();
    $command = hrm_lifecycle_training_assignment_bound_draft($draft_data, $draft_id, true);
    if (!$command || $actor <= 0)
        return false;
    if ((string)$command['command_status'] === 'completed' && (int)$command['row_version'] === 2)
        return (int)$command['lifecycle_command_id'];
    if ((string)$command['command_status'] !== 'pending' || (int)$command['row_version'] !== 1
        || (int)$command['requested_by'] <= 0 || (int)$command['requested_by'] === $actor)
        return false;

    $employee_number = (int)$draft_data['employee_number'];
    $binding = get_hrm_person_worker_binding($employee_number, true);
    $employment = get_hrm_legacy_current_employment($employee_number, true);
    $assignment = get_hrm_legacy_current_assignment($employee_number, true);
    if (!$binding || !$employment || !$assignment || !empty($binding['retired_at'])
        || (int)$binding['worker_id'] !== (int)$command['worker_id']
        || (int)$employment['employment_id'] !== (int)$command['employment_id']
        || (int)$assignment['assignment_id'] !== (int)$command['assignment_id']
        || (int)$assignment['legacy_current'] !== 1 || (int)$employment['legacy_current'] !== 1
        || (int)$assignment['primary_flag'] !== 1 || (string)$assignment['assignment_type_code'] !== 'legacy_primary')
        return false;
    $employee = hrm_lifecycle_training_assignment_employee_for_update($employee_number, $binding['employee_id']);
    $course = hrm_lifecycle_training_assignment_course_for_update($draft_data['course_id']);
    if (!$employee || (int)$employee['inactive'] !== 0 || !empty($employee['released_date'])
        || (int)$employee['department_id'] !== (int)$assignment['legacy_department_id']
        || (int)$employee['position_id'] !== (int)$assignment['legacy_position_id']
        || (int)$employee['grade_id'] !== (int)$assignment['legacy_grade_id']
        || hrm_lifecycle_training_assignment_course_fingerprint($course) !== (string)$draft_data['course_fingerprint'])
        return false;

    $training_date = (string)$draft_data['training_date'];
    if (!empty($employment['start_date']) && strcmp($training_date, (string)$employment['start_date']) < 0)
        return false;
    if (!empty($employment['end_date']) && strcmp($training_date, (string)$employment['end_date']) > 0)
        return false;
    if (!empty($assignment['effective_from']) && strcmp($training_date, (string)$assignment['effective_from']) < 0)
        return false;
    if (!empty($assignment['effective_to']) && strcmp($training_date, (string)$assignment['effective_to']) > 0)
        return false;

    $training_id = add_employee_training(array('employee_id'=>(string)$binding['employee_id'],
        'course_id'=>(int)$draft_data['course_id'], 'training_date'=>sql2date($training_date),
        'completion_date'=>$draft_data['completion_date'] === '' ? '' : sql2date($draft_data['completion_date']),
        'status'=>(int)$draft_data['status'], 'score'=>(string)$draft_data['score'],
        'cost_amount'=>(string)$draft_data['cost_amount'], 'remarks'=>(string)$draft_data['remarks']));
    if ((int)$training_id <= 0)
        return false;
    if (!db_query("UPDATE ".TB_PREF."hrm_lifecycle_commands SET command_status='completed',row_version=2,"
        ."completed_at=UTC_TIMESTAMP(),completed_by=".$actor." WHERE lifecycle_command_id=".(int)$command['lifecycle_command_id']
        ." AND command_status='pending' AND row_version=1 AND approval_draft_id=".(int)$draft_id,
        'could not complete Employee Training Assignment command') || (int)db_num_affected_rows() !== 1)
        return false;
    $command['command_status'] = 'completed'; $command['row_version'] = 2;
    if (!hrm_lifecycle_training_assignment_audit('hrm.lifecycle.training_assignment.completed', $command,
        (int)$employment['legal_entity_id'], array('approval_draft_id'=>(int)$draft_id,
            'maker_user_id'=>(int)$command['requested_by'], 'checker_user_id'=>$actor,
            'maker_checker_separated'=>true, 'employee_training_rows_added'=>1,
            'training_id'=>(int)$training_id, 'course_id'=>(int)$draft_data['course_id'],
            'course_fingerprint_bound'=>true, 'request_hash'=>(string)$command['request_hash'])))
        return false;
    return (int)$command['lifecycle_command_id'];
}

/** @return int|false */
function hrm_lifecycle_training_assignment_terminalize($draft_data, $draft_id, $status, $maker_only)
{
    if (!hrm_lifecycle_training_assignment_permission() || !in_array($status, array('rejected','cancelled'), true))
        return false;
    $actor = hrm_lifecycle_training_assignment_actor_id();
    $command = hrm_lifecycle_training_assignment_bound_draft($draft_data, $draft_id, true);
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
        'could not terminalize Employee Training Assignment command') || (int)db_num_affected_rows() !== 1)
        return false;
    $command['command_status'] = $status; $command['row_version'] = 2;
    if (!hrm_lifecycle_training_assignment_audit('hrm.lifecycle.training_assignment.'.$status, $command,
        (int)$employment['legal_entity_id'], array('approval_draft_id'=>(int)$draft_id,
            'terminal_actor_user_id'=>$actor, 'employee_training_rows_added'=>0)))
        return false;
    return (int)$command['lifecycle_command_id'];
}

/** @return int|false */
function reject_hrm_lifecycle_training_assignment_draft($draft_data, $draft_id, $comments)
{
    return hrm_lifecycle_training_assignment_terminalize($draft_data, $draft_id, 'rejected', false);
}

/** @return int|false */
function cancel_hrm_lifecycle_training_assignment_draft($draft_data, $draft_id, $comments)
{
    return hrm_lifecycle_training_assignment_terminalize($draft_data, $draft_id, 'cancelled', true);
}
