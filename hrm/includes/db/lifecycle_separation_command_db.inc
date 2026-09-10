<?php
/**
 * HRM-FND-005 fixed Employee Separation lifecycle command.
 *
 * This is intentionally one bounded internal command, not a generic dispatcher.
 * Submission creates one pending lifecycle-command envelope plus one central
 * ApprovalWorkflow draft. Final independent approval revalidates the exact
 * Worker/Employment/Assignment predecessor and delegates the actual mutation to
 * the accepted effective-dated employee/Assignment writer. No payroll, payment,
 * bank-transaction, GL, checklist, document, or employee-compensation authority
 * is introduced here.
 */
$hrm_lifecycle_separation_root = isset($path_to_root)
    ? $path_to_root : dirname(dirname(dirname(__DIR__)));
$hrm_lifecycle_separation_dependencies = array(
    array('/includes/approval/approval_workflow.inc', 'class', 'ApprovalWorkflow'),
    array('/includes/approval/db/approval_rules_db.inc', 'function', 'get_approval_workflow_by_type'),
    array('/includes/db/domain_audit_db.inc', 'function', 'append_domain_audit_event'),
    array('/hrm/includes/db/employee_db.inc', 'function', 'update_employee'),
    array('/hrm/includes/db/employee_history_db.inc', 'function', 'add_employee_history'),
    array('/hrm/includes/db/employee_salary_db.inc', 'function', 'get_employee_total_salary'),
    array('/hrm/includes/db/eos_db.inc', 'function', 'calculate_eos_amount'),
    array('/hrm/includes/db/employee_person_worker_db.inc', 'function', 'get_hrm_person_worker_binding'),
    array('/hrm/includes/db/employee_employment_db.inc', 'function', 'get_hrm_legacy_current_employment'),
    array('/hrm/includes/db/employee_assignment_db.inc', 'function', 'get_hrm_legacy_current_assignment'),
    array('/hrm/includes/hrm_constants.inc', 'constant', 'HRM_HIST_SEPARATION'),
);
foreach ($hrm_lifecycle_separation_dependencies as $dependency) {
    if ($dependency[1] === 'class')
        $available = class_exists($dependency[2]);
    elseif ($dependency[1] === 'constant')
        $available = defined($dependency[2]);
    else
        $available = function_exists($dependency[2]);
    if ($available)
        continue;
    $candidate = $hrm_lifecycle_separation_root.$dependency[0];
    if (is_file($candidate))
        include_once($candidate);
}
unset($hrm_lifecycle_separation_root, $hrm_lifecycle_separation_dependencies, $dependency, $candidate, $available);

if (!defined('HRM_LIFECYCLE_SEPARATION_COMMAND_TYPE'))
    define('HRM_LIFECYCLE_SEPARATION_COMMAND_TYPE', 'employee_separation');
if (!defined('HRM_LIFECYCLE_SEPARATION_HASH_CONTRACT'))
    define('HRM_LIFECYCLE_SEPARATION_HASH_CONTRACT', 'notrinoserp.hrm-fnd-005.employee-separation');
if (!defined('HRM_LIFECYCLE_SEPARATION_HASH_VERSION'))
    define('HRM_LIFECYCLE_SEPARATION_HASH_VERSION', '1');
if (!defined('HRM_LIFECYCLE_SEPARATION_REASON'))
    define('HRM_LIFECYCLE_SEPARATION_REASON', 'hrm_fnd_005_employee_separation_command');

function hrm_lifecycle_separation_actor_id()
{
    return isset($_SESSION['wa_current_user']) && is_object($_SESSION['wa_current_user'])
        && isset($_SESSION['wa_current_user']->user) ? (int)$_SESSION['wa_current_user']->user : 0;
}

function hrm_lifecycle_separation_permission()
{
    return function_exists('user_check_access') && user_check_access('SA_EMPSEPARATION');
}

function hrm_lifecycle_separation_positive_int($value)
{
    if (is_int($value))
        return $value > 0 ? $value : false;
    $value = trim((string)$value);
    if (preg_match('/^[1-9][0-9]*$/D', $value) !== 1)
        return false;
    $number = (int)$value;
    return $number > 0 && (string)$number === $value ? $number : false;
}

function hrm_lifecycle_separation_nonnegative_int($value)
{
    if (is_int($value))
        return $value >= 0 ? $value : false;
    $value = trim((string)$value);
    if (preg_match('/^(?:0|[1-9][0-9]*)$/D', $value) !== 1)
        return false;
    $number = (int)$value;
    return $number >= 0 && (string)$number === $value ? $number : false;
}

function hrm_lifecycle_separation_date($value)
{
    $value = trim((string)$value);
    if (preg_match('/^[0-9]{4}-[0-9]{2}-[0-9]{2}$/D', $value) !== 1)
        return false;
    $date = DateTime::createFromFormat('!Y-m-d', $value);
    $errors = DateTime::getLastErrors();
    if (!$date || (is_array($errors) && ((int)$errors['warning_count'] !== 0 || (int)$errors['error_count'] !== 0))
        || $date->format('Y-m-d') !== $value)
        return false;
    return $value;
}

function hrm_lifecycle_separation_today_sql()
{
    if (function_exists('Today') && function_exists('date2sql')) {
        $today = date2sql(Today());
        if (hrm_lifecycle_separation_date($today) !== false)
            return $today;
    }
    return date('Y-m-d');
}

function hrm_lifecycle_separation_idempotency_hash($key)
{
    if (!is_string($key) || strlen($key) < 16 || strlen($key) > 128
        || preg_match('/^[A-Za-z0-9._:-]+$/D', $key) !== 1)
        return false;
    return hash('sha256', $key);
}

function hrm_lifecycle_separation_workflow_is_maker_checker()
{
    if (!defined('ST_HRM_LIFECYCLE_SEPARATION') || !class_exists('ApprovalWorkflow')
        || !ApprovalWorkflow::isRegistered(ST_HRM_LIFECYCLE_SEPARATION)
        || !function_exists('get_approval_workflow_by_type') || !function_exists('get_approval_levels'))
        return false;
    $workflow = get_approval_workflow_by_type(ST_HRM_LIFECYCLE_SEPARATION);
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

function hrm_lifecycle_separation_employee_for_update($employee_number, $employee_id)
{
    if ((int)$employee_number <= 0 || trim((string)$employee_id) === '')
        return false;
    $result = db_query("SELECT employee_number,employee_id,department_id,position_id,grade_id,hire_date,inactive,released_date "
        ."FROM ".TB_PREF."employees WHERE employee_number=".(int)$employee_number
        ." AND employee_id=".db_escape((string)$employee_id)." LIMIT 1 FOR UPDATE",
        'could not lock Employee Separation lifecycle employee projection');
    return $result ? db_fetch_assoc($result) : false;
}

function hrm_lifecycle_separation_request_hash($company, $worker_id, $employment_id, $assignment_id,
    $employee_number, $effective_date, $is_resignation)
{
    foreach (array($worker_id,$employment_id,$assignment_id,$employee_number) as $value)
        if (hrm_lifecycle_separation_positive_int((string)$value) === false)
            return false;
    $is_resignation = hrm_lifecycle_separation_nonnegative_int((string)$is_resignation);
    if ((int)$company < 0 || $is_resignation === false || !in_array($is_resignation, array(0,1), true)
        || hrm_lifecycle_separation_date($effective_date) === false)
        return false;
    return hash('sha256', HRM_LIFECYCLE_SEPARATION_HASH_CONTRACT.'|v='.HRM_LIFECYCLE_SEPARATION_HASH_VERSION
        .'|company='.(int)$company.'|worker_id='.(int)$worker_id.'|employment_id='.(int)$employment_id
        .'|assignment_id='.(int)$assignment_id.'|employee_number='.(int)$employee_number
        .'|effective_date='.$effective_date.'|is_resignation='.(int)$is_resignation);
}

function hrm_lifecycle_separation_command_for_update($command_id)
{
    $command_id = hrm_lifecycle_separation_positive_int($command_id);
    if ($command_id === false)
        return false;
    $result = db_query("SELECT lifecycle_command_id,worker_id,employment_id,assignment_id,command_type,command_status,"
        ."effective_at,idempotency_key_hash,request_hash,approval_draft_id,row_version,requested_at,requested_by,"
        ."completed_at,completed_by FROM ".TB_PREF."hrm_lifecycle_commands WHERE lifecycle_command_id=".$command_id
        ." LIMIT 1 FOR UPDATE", 'could not lock Employee Separation lifecycle command');
    return $result ? db_fetch_assoc($result) : false;
}

function hrm_lifecycle_separation_by_idempotency_for_update($worker_id, $idempotency_hash)
{
    if ((int)$worker_id <= 0 || !is_string($idempotency_hash)
        || preg_match('/^[a-f0-9]{64}$/D', $idempotency_hash) !== 1)
        return false;
    $result = db_query("SELECT lifecycle_command_id,worker_id,employment_id,assignment_id,command_type,command_status,"
        ."effective_at,idempotency_key_hash,request_hash,approval_draft_id,row_version,requested_at,requested_by,"
        ."completed_at,completed_by FROM ".TB_PREF."hrm_lifecycle_commands WHERE worker_id=".(int)$worker_id
        ." AND command_type=".db_escape(HRM_LIFECYCLE_SEPARATION_COMMAND_TYPE)
        ." AND idempotency_key_hash=".db_escape($idempotency_hash)." LIMIT 1 FOR UPDATE",
        'could not lock idempotent Employee Separation lifecycle command');
    return $result ? db_fetch_assoc($result) : false;
}

function hrm_lifecycle_separation_other_pending_exists($worker_id)
{
    $result = db_query("SELECT lifecycle_command_id,command_type FROM ".TB_PREF."hrm_lifecycle_commands WHERE worker_id=".(int)$worker_id
        ." AND command_status='pending' ORDER BY lifecycle_command_id LIMIT 1 FOR UPDATE",
        'could not inspect pending Employee lifecycle conflict');
    return $result && db_fetch_assoc($result) ? true : false;
}

function hrm_lifecycle_separation_audit($action, $reason_code, $command, $legal_entity_id, $restricted_diff)
{
    if (!function_exists('append_domain_audit_event') || !is_array($command)
        || (int)$command['lifecycle_command_id'] <= 0 || (int)$command['worker_id'] <= 0
        || (int)$command['employment_id'] <= 0 || (int)$command['assignment_id'] <= 0
        || (int)$legal_entity_id < 0)
        return false;
    $safe = array_merge(array(
        'fixed_command_type'=>'employee_separation',
        'central_maker_checker'=>true,
        'checklist_rows_changed'=>0,
        'document_rows_changed'=>0,
        'compensation_fields_changed'=>0,
        'payroll_calculation_invoked'=>false,
        'payroll_post_authority_changed'=>false,
        'payment_execution_authority_changed'=>false,
        'bank_transaction_authority_changed'=>false,
        'gl_accounting_authority_changed'=>false,
        'historical_payroll_accounting_rows_rewritten'=>0,
        'comment_text_stored'=>false,
        'free_text_separation_reason_stored'=>false,
    ), is_array($restricted_diff) ? $restricted_diff : array());
    return append_domain_audit_event(array(
        'company_id'=>function_exists('user_company') ? (int)user_company() : 0,
        'legal_entity_id'=>(int)$legal_entity_id,
        'entity_type'=>'hrm_lifecycle_command',
        'entity_key'=>array(
            'lifecycle_command_id'=>(int)$command['lifecycle_command_id'],
            'worker_id'=>(int)$command['worker_id'],
            'employment_id'=>(int)$command['employment_id'],
            'assignment_id'=>(int)$command['assignment_id'],
        ),
        'action'=>$action,
        'reason_code'=>$reason_code,
        'before'=>array(),
        'after'=>array('command_status'=>(string)$command['command_status'],'row_version'=>(int)$command['row_version']),
        'restricted_diff'=>$safe,
    )) !== false;
}

function hrm_lifecycle_separation_draft_matches($draft_data, $command, $allow_terminal_version)
{
    if (!is_array($draft_data) || !is_array($command))
        return false;
    $expected_keys = array('assignment_id','command_row_version','effective_date','employee_number','employment_id',
        'is_resignation','lifecycle_command_id','request_hash','worker_id');
    $keys = array_keys($draft_data);
    sort($keys, SORT_STRING);
    if ($keys !== $expected_keys)
        return false;
    foreach (array('lifecycle_command_id','worker_id','employment_id','assignment_id','employee_number') as $key)
        if (hrm_lifecycle_separation_positive_int($draft_data[$key]) === false)
            return false;
    $is_resignation = hrm_lifecycle_separation_nonnegative_int($draft_data['is_resignation']);
    $draft_version = hrm_lifecycle_separation_positive_int($draft_data['command_row_version']);
    $terminal_successor = $allow_terminal_version && (int)$command['row_version'] === 2 && $draft_version === 1;
    if ($is_resignation === false || !in_array($is_resignation, array(0,1), true)
        || $draft_version === false || ($draft_version !== (int)$command['row_version'] && !$terminal_successor)
        || hrm_lifecycle_separation_date($draft_data['effective_date']) === false
        || !is_string($draft_data['request_hash']) || preg_match('/^[a-f0-9]{64}$/D', $draft_data['request_hash']) !== 1)
        return false;
    if ((int)$draft_data['lifecycle_command_id'] !== (int)$command['lifecycle_command_id']
        || (int)$draft_data['worker_id'] !== (int)$command['worker_id']
        || (int)$draft_data['employment_id'] !== (int)$command['employment_id']
        || (int)$draft_data['assignment_id'] !== (int)$command['assignment_id']
        || (string)$draft_data['effective_date'].' 00:00:00' !== (string)$command['effective_at']
        || (string)$draft_data['request_hash'] !== (string)$command['request_hash'])
        return false;
    $recomputed = hrm_lifecycle_separation_request_hash(
        function_exists('user_company') ? (int)user_company() : 0,
        (int)$draft_data['worker_id'], (int)$draft_data['employment_id'], (int)$draft_data['assignment_id'],
        (int)$draft_data['employee_number'], (string)$draft_data['effective_date'], $is_resignation
    );
    return is_string($recomputed) && $recomputed === (string)$command['request_hash'];
}

/**
 * Submit one fixed Employee Separation lifecycle command.
 *
 * Effective date is canonical SQL Y-m-d and must not be future-dated. The
 * existing browser separation route is intentionally not redirected by this slice.
 */
function submit_hrm_lifecycle_employee_separation($employee_id, $effective_date, $is_resignation, $idempotency_key)
{
    $employee_id = trim((string)$employee_id);
    $effective_date = hrm_lifecycle_separation_date($effective_date);
    $is_resignation = hrm_lifecycle_separation_nonnegative_int($is_resignation);
    $idempotency_hash = hrm_lifecycle_separation_idempotency_hash($idempotency_key);
    $actor = hrm_lifecycle_separation_actor_id();
    $company = function_exists('user_company') ? (int)user_company() : 0;
    if ($employee_id === '' || strlen($employee_id) > 20 || $effective_date === false
        || $is_resignation === false || !in_array($is_resignation, array(0,1), true)
        || $idempotency_hash === false || strcmp($effective_date, hrm_lifecycle_separation_today_sql()) > 0
        || $actor <= 0 || $company < 0 || !hrm_lifecycle_separation_permission()
        || !hrm_lifecycle_separation_workflow_is_maker_checker()
        || !function_exists('get_hrm_person_worker_binding') || !function_exists('get_hrm_legacy_current_employment')
        || !function_exists('get_hrm_legacy_current_assignment') || !function_exists('get_approval_workflow_service'))
        return false;

    $ok = false;
    $return_value = false;
    begin_transaction();
    do {
        $binding = get_hrm_person_worker_binding($employee_id, true);
        if (!$binding || !empty($binding['retired_at']) || (int)$binding['employee_number'] <= 0 || (int)$binding['worker_id'] <= 0)
            break;
        $employee_number = (int)$binding['employee_number'];
        $employment = get_hrm_legacy_current_employment($employee_number, true);
        $assignment = get_hrm_legacy_current_assignment($employee_number, true);
        $employee = hrm_lifecycle_separation_employee_for_update($employee_number, $employee_id);
        if (!$employment || !$assignment || !$employee
            || (int)$employment['worker_id'] !== (int)$binding['worker_id']
            || (int)$assignment['employment_id'] !== (int)$employment['employment_id']
            || (int)$assignment['legacy_employee_number'] !== $employee_number
            || (int)$assignment['legacy_current'] !== 1 || (int)$employment['legacy_current'] !== 1
            || (int)$assignment['primary_flag'] !== 1 || (string)$assignment['assignment_type_code'] !== 'legacy_primary'
            || (int)$employee['inactive'] !== 0 || ($employee['released_date'] !== null && $employee['released_date'] !== '')
            || !in_array((string)$employment['employment_status'], array('active','suspended'), true)
            || ($employment['end_date'] !== null && $employment['end_date'] !== '')
            || ($assignment['effective_to'] !== null && $assignment['effective_to'] !== '')
            || (int)$employee['department_id'] !== (int)$assignment['legacy_department_id']
            || (int)$employee['position_id'] !== (int)$assignment['legacy_position_id']
            || (int)$employee['grade_id'] !== (int)$assignment['legacy_grade_id'])
            break;
        if ($employment['start_date'] !== null && $employment['start_date'] !== ''
            && strcmp($effective_date, (string)$employment['start_date']) < 0)
            break;
        if ($assignment['effective_from'] !== null && $assignment['effective_from'] !== ''
            && strcmp($effective_date, (string)$assignment['effective_from']) < 0)
            break;
        if ($employee['hire_date'] !== null && $employee['hire_date'] !== ''
            && strcmp($effective_date, (string)$employee['hire_date']) < 0)
            break;

        $request_hash = hrm_lifecycle_separation_request_hash($company, (int)$binding['worker_id'],
            (int)$employment['employment_id'], (int)$assignment['assignment_id'], $employee_number,
            $effective_date, $is_resignation);
        if ($request_hash === false)
            break;
        $existing = hrm_lifecycle_separation_by_idempotency_for_update((int)$binding['worker_id'], $idempotency_hash);
        if (is_array($existing)) {
            if ((int)$existing['requested_by'] !== $actor || (string)$existing['request_hash'] !== $request_hash
                || (int)$existing['approval_draft_id'] <= 0)
                break;
            $ok = true;
            $return_value = array('lifecycle_command_id'=>(int)$existing['lifecycle_command_id'],
                'approval_draft_id'=>(int)$existing['approval_draft_id'],'status'=>(string)$existing['command_status'],
                'row_version'=>(int)$existing['row_version'],'request_hash'=>$request_hash,'exact_retry'=>true);
            break;
        }
        if (hrm_lifecycle_separation_other_pending_exists((int)$binding['worker_id']))
            break;

        if (!db_query("INSERT INTO ".TB_PREF."hrm_lifecycle_commands "
            ."(worker_id,employment_id,assignment_id,command_type,command_status,effective_at,idempotency_key_hash,"
            ."request_hash,approval_draft_id,row_version,requested_by) VALUES ("
            .(int)$binding['worker_id'].",".(int)$employment['employment_id'].",".(int)$assignment['assignment_id'].','
            .db_escape(HRM_LIFECYCLE_SEPARATION_COMMAND_TYPE).",'pending',".db_escape($effective_date.' 00:00:00').','
            .db_escape($idempotency_hash).','.db_escape($request_hash).",NULL,1,".$actor.")",
            'could not create Employee Separation lifecycle command'))
            break;
        $command_id = (int)db_insert_id();
        if ($command_id <= 0)
            break;

        $draft_data = array(
            'lifecycle_command_id'=>$command_id,'command_row_version'=>1,
            'worker_id'=>(int)$binding['worker_id'],'employment_id'=>(int)$employment['employment_id'],
            'assignment_id'=>(int)$assignment['assignment_id'],'employee_number'=>$employee_number,
            'effective_date'=>$effective_date,'is_resignation'=>$is_resignation,'request_hash'=>$request_hash,
        );
        $approval = get_approval_workflow_service();
        $approval_result = $approval->submit(ST_HRM_LIFECYCLE_SEPARATION, $draft_data, 0,
            array('summary'=>sprintf(_('Employee separation lifecycle command #%d'), $command_id)));
        if (!is_array($approval_result) || !isset($approval_result['status'],$approval_result['draft_id'])
            || (string)$approval_result['status'] !== 'pending' || (int)$approval_result['draft_id'] <= 0)
            break;
        $draft_id = (int)$approval_result['draft_id'];
        if (!db_query("UPDATE ".TB_PREF."hrm_lifecycle_commands SET approval_draft_id=".$draft_id
            ." WHERE lifecycle_command_id=".$command_id." AND approval_draft_id IS NULL AND row_version=1"
            ." AND command_status='pending'", 'could not bind Employee Separation lifecycle command to approval draft')
            || (int)db_num_affected_rows() !== 1)
            break;
        $command = array('lifecycle_command_id'=>$command_id,'worker_id'=>(int)$binding['worker_id'],
            'employment_id'=>(int)$employment['employment_id'],'assignment_id'=>(int)$assignment['assignment_id'],
            'command_status'=>'pending','row_version'=>1);
        if (!hrm_lifecycle_separation_audit('hrm.lifecycle.employee_separation.submitted', HRM_LIFECYCLE_SEPARATION_REASON,
            $command, (int)$employment['legal_entity_id'], array(
                'approval_draft_id'=>$draft_id,'maker_user_id'=>$actor,'request_hash'=>$request_hash,
                'idempotency_key_plaintext_stored'=>false,'effective_date_bound'=>true,'is_resignation_bound'=>true,
                'worker_employment_assignment_bound'=>true,'same_subject_conflict_guard'=>true,
                'existing_separation_route_redirected'=>false)))
            break;
        $ok = true;
        $return_value = array('lifecycle_command_id'=>$command_id,'approval_draft_id'=>$draft_id,
            'status'=>'pending','row_version'=>1,'request_hash'=>$request_hash,'exact_retry'=>false);
    } while (false);
    if ($ok)
        commit_transaction();
    else
        cancel_transaction();
    return $ok ? $return_value : false;
}

function hrm_lifecycle_separation_bound_draft($draft_data, $draft_id, $allow_terminal_version)
{
    $draft_id = hrm_lifecycle_separation_positive_int($draft_id);
    if ($draft_id === false || !is_array($draft_data) || !isset($draft_data['lifecycle_command_id']))
        return false;
    $command = hrm_lifecycle_separation_command_for_update($draft_data['lifecycle_command_id']);
    if (!$command || (string)$command['command_type'] !== HRM_LIFECYCLE_SEPARATION_COMMAND_TYPE
        || (int)$command['approval_draft_id'] !== $draft_id
        || !hrm_lifecycle_separation_draft_matches($draft_data, $command, $allow_terminal_version))
        return false;
    return $command;
}

function hrm_lifecycle_separation_service_years($hire_date, $effective_date)
{
    $hire_date = hrm_lifecycle_separation_date($hire_date);
    $effective_date = hrm_lifecycle_separation_date($effective_date);
    if ($hire_date === false || $effective_date === false || strcmp($effective_date, $hire_date) < 0)
        return false;
    $days = (strtotime($effective_date.' 00:00:00 UTC') - strtotime($hire_date.' 00:00:00 UTC')) / 86400;
    if ($days < 0)
        return false;
    return function_exists('round2') ? round2($days / 365, 4) : round($days / 365, 4);
}

function execute_approved_hrm_lifecycle_separation($draft_data, $reserved_trans_no, $draft_id)
{
    if (!hrm_lifecycle_separation_permission())
        return false;
    $actor = hrm_lifecycle_separation_actor_id();
    $command = hrm_lifecycle_separation_bound_draft($draft_data, $draft_id, true);
    if (!$command || $actor <= 0)
        return false;
    if ((string)$command['command_status'] === 'completed' && (int)$command['row_version'] === 2)
        return (int)$command['lifecycle_command_id'];
    if ((string)$command['command_status'] !== 'pending' || (int)$command['row_version'] !== 1
        || (int)$command['requested_by'] <= 0 || (int)$command['requested_by'] === $actor)
        return false;
    $effective_date = (string)$draft_data['effective_date'];
    if (strcmp($effective_date, hrm_lifecycle_separation_today_sql()) > 0)
        return false;

    $employee_number = (int)$draft_data['employee_number'];
    $binding = get_hrm_person_worker_binding($employee_number, true);
    $employment = get_hrm_legacy_current_employment($employee_number, true);
    $assignment = get_hrm_legacy_current_assignment($employee_number, true);
    if (!$binding || !$employment || !$assignment || !empty($binding['retired_at'])
        || (int)$binding['worker_id'] !== (int)$command['worker_id']
        || (int)$employment['employment_id'] !== (int)$command['employment_id']
        || (int)$employment['worker_id'] !== (int)$command['worker_id']
        || (int)$assignment['assignment_id'] !== (int)$command['assignment_id']
        || (int)$assignment['employment_id'] !== (int)$command['employment_id']
        || (int)$assignment['legacy_current'] !== 1 || (int)$employment['legacy_current'] !== 1
        || !in_array((string)$employment['employment_status'], array('active','suspended'), true)
        || ($employment['end_date'] !== null && $employment['end_date'] !== '')
        || ($assignment['effective_to'] !== null && $assignment['effective_to'] !== ''))
        return false;
    $employee_id = (string)$binding['employee_id'];
    $employee = hrm_lifecycle_separation_employee_for_update($employee_number, $employee_id);
    if (!$employee || (int)$employee['inactive'] !== 0
        || ($employee['released_date'] !== null && $employee['released_date'] !== '')
        || (int)$employee['department_id'] !== (int)$assignment['legacy_department_id']
        || (int)$employee['position_id'] !== (int)$assignment['legacy_position_id']
        || (int)$employee['grade_id'] !== (int)$assignment['legacy_grade_id'])
        return false;
    if (($employment['start_date'] !== null && $employment['start_date'] !== ''
            && strcmp($effective_date, (string)$employment['start_date']) < 0)
        || ($assignment['effective_from'] !== null && $assignment['effective_from'] !== ''
            && strcmp($effective_date, (string)$assignment['effective_from']) < 0)
        || ($employee['hire_date'] !== null && $employee['hire_date'] !== ''
            && strcmp($effective_date, (string)$employee['hire_date']) < 0))
        return false;

    $monthly_salary = function_exists('get_employee_total_salary')
        ? get_employee_total_salary($employee_id, $effective_date) : false;
    $years = hrm_lifecycle_separation_service_years((string)$employee['hire_date'], $effective_date);
    if ($monthly_salary === false || $years === false || !function_exists('calculate_eos_amount'))
        return false;
    $eos_amount = calculate_eos_amount($monthly_salary, $years, (int)$draft_data['is_resignation']);

    if (!update_employee($employee_id, array('inactive'=>1,'released_date'=>$effective_date), array(
        'effective_date'=>$effective_date,'change_type'=>HRM_HIST_SEPARATION)))
        return false;
    $approved_by = isset($_SESSION['wa_current_user']->loginname) ? (string)$_SESSION['wa_current_user']->loginname : '';
    $history_id = add_employee_history($employee_id, HRM_HIST_SEPARATION, $effective_date,
        (int)$employee['department_id'], null, (int)$employee['position_id'], null,
        (int)$employee['grade_id'], null, $monthly_salary, null,
        'lifecycle_command:'.(int)$command['lifecycle_command_id'].'; EOS='.(string)$eos_amount, $approved_by);
    if ((int)$history_id <= 0)
        return false;
    if (!db_query("UPDATE ".TB_PREF."hrm_lifecycle_commands SET command_status='completed',row_version=2,"
        ."completed_at=UTC_TIMESTAMP(),completed_by=".$actor." WHERE lifecycle_command_id=".(int)$command['lifecycle_command_id']
        ." AND command_status='pending' AND row_version=1 AND approval_draft_id=".(int)$draft_id,
        'could not complete Employee Separation lifecycle command') || (int)db_num_affected_rows() !== 1)
        return false;
    $command['command_status'] = 'completed';
    $command['row_version'] = 2;
    if (!hrm_lifecycle_separation_audit('hrm.lifecycle.employee_separation.completed', HRM_LIFECYCLE_SEPARATION_REASON,
        $command, (int)$employment['legal_entity_id'], array(
            'approval_draft_id'=>(int)$draft_id,'maker_user_id'=>(int)$command['requested_by'],
            'checker_user_id'=>$actor,'maker_checker_separated'=>true,'employee_history_id'=>(int)$history_id,
            'predecessor_employment_id'=>(int)$command['employment_id'],
            'predecessor_assignment_id'=>(int)$command['assignment_id'],'employment_assignment_termination_delegated'=>true,
            'eos_amount_stored_in_lifecycle_audit'=>false,'salary_value_stored_in_lifecycle_audit'=>false,
            'request_hash'=>(string)$command['request_hash'])))
        return false;
    return (int)$command['lifecycle_command_id'];
}

function hrm_lifecycle_separation_terminalize($draft_data, $draft_id, $status, $maker_only)
{
    if (!hrm_lifecycle_separation_permission() || !in_array($status, array('rejected','cancelled'), true))
        return false;
    $actor = hrm_lifecycle_separation_actor_id();
    $command = hrm_lifecycle_separation_bound_draft($draft_data, $draft_id, true);
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
        ." WHERE lifecycle_command_id=".(int)$command['lifecycle_command_id']." AND command_status='pending'"
        ." AND row_version=1 AND approval_draft_id=".(int)$draft_id,
        'could not terminalize Employee Separation lifecycle command') || (int)db_num_affected_rows() !== 1)
        return false;
    $command['command_status'] = $status;
    $command['row_version'] = 2;
    $action = $status === 'rejected' ? 'hrm.lifecycle.employee_separation.rejected' : 'hrm.lifecycle.employee_separation.cancelled';
    $reason = $status === 'rejected' ? 'hrm_fnd_005_employee_separation_rejection' : 'hrm_fnd_005_employee_separation_cancellation';
    return hrm_lifecycle_separation_audit($action, $reason, $command, (int)$employment['legal_entity_id'], array(
        'approval_draft_id'=>(int)$draft_id,'actor_user_id'=>$actor,'maker_only'=>$maker_only,
        'maker_checker_separated'=>$maker_only ? false : true,'business_employee_rows_mutated'=>0,
        'business_employment_rows_mutated'=>0,'business_assignment_rows_mutated'=>0,'employee_history_rows_added'=>0))
        ? (int)$command['lifecycle_command_id'] : false;
}

function reject_hrm_lifecycle_separation_draft($draft_data, $draft_id, $comments)
{
    return hrm_lifecycle_separation_terminalize($draft_data, $draft_id, 'rejected', false);
}

function cancel_hrm_lifecycle_separation_draft($draft_data, $draft_id, $comments)
{
    return hrm_lifecycle_separation_terminalize($draft_data, $draft_id, 'cancelled', true);
}
