<?php
/**
 * HRM-FND-005 fixed Employee Salary Revision lifecycle command.
 *
 * This is one compensation-sensitive command, not a generic dispatcher.
 * Submission captures an exact same-subject Worker/Employment/Assignment and
 * Basic salary-row fingerprint, then creates a central maker/checker approval
 * draft. Final approval revalidates that fingerprint before changing only the
 * accepted employee personal-salary flag, one effective-dated Basic salary row,
 * one salary-history row, lifecycle command state, and restricted PAY-AUD
 * evidence. Historical payroll results, payslips, posting, payment, bank, GL,
 * accounting, formulas, and approval comments are never rewritten here.
 */
$hrm_lifecycle_salary_root = isset($path_to_root)
    ? $path_to_root : dirname(dirname(dirname(__DIR__)));
$hrm_lifecycle_salary_dependencies = array(
    array('/includes/approval/approval_workflow.inc', 'class', 'ApprovalWorkflow'),
    array('/includes/approval/db/approval_rules_db.inc', 'function', 'get_approval_workflow_by_type'),
    array('/includes/db/domain_audit_db.inc', 'function', 'append_domain_audit_event'),
    array('/hrm/includes/db/employee_db.inc', 'function', 'update_employee'),
    array('/hrm/includes/db/employee_salary_db.inc', 'function', 'get_employee_total_salary'),
    array('/hrm/includes/db/employee_history_db.inc', 'function', 'add_employee_history'),
    array('/hrm/includes/db/employee_person_worker_db.inc', 'function', 'get_hrm_person_worker_binding'),
    array('/hrm/includes/db/employee_employment_db.inc', 'function', 'get_hrm_legacy_current_employment'),
    array('/hrm/includes/db/employee_assignment_db.inc', 'function', 'get_hrm_legacy_current_assignment'),
    array('/hrm/includes/hrm_constants.inc', 'constant', 'HRM_HIST_SALARY_CHANGE'),
);
foreach ($hrm_lifecycle_salary_dependencies as $dependency) {
    if ($dependency[1] === 'class')
        $available = class_exists($dependency[2]);
    elseif ($dependency[1] === 'constant')
        $available = defined($dependency[2]);
    else
        $available = function_exists($dependency[2]);
    if ($available)
        continue;
    $candidate = $hrm_lifecycle_salary_root.$dependency[0];
    if (is_file($candidate))
        include_once($candidate);
}
unset($hrm_lifecycle_salary_root, $hrm_lifecycle_salary_dependencies, $dependency, $candidate, $available);

if (!defined('HRM_LIFECYCLE_SALARY_REVISION_COMMAND_TYPE'))
    define('HRM_LIFECYCLE_SALARY_REVISION_COMMAND_TYPE', 'employee_salary_revision');
if (!defined('HRM_LIFECYCLE_SALARY_REVISION_HASH_CONTRACT'))
    define('HRM_LIFECYCLE_SALARY_REVISION_HASH_CONTRACT', 'notrinoserp.hrm-fnd-005.employee-salary-revision');
if (!defined('HRM_LIFECYCLE_SALARY_REVISION_HASH_VERSION'))
    define('HRM_LIFECYCLE_SALARY_REVISION_HASH_VERSION', '1');
if (!defined('HRM_LIFECYCLE_SALARY_REVISION_REASON'))
    define('HRM_LIFECYCLE_SALARY_REVISION_REASON', 'hrm_fnd_005_salary_revision_command');

function hrm_lifecycle_salary_revision_actor_id()
{
    return isset($_SESSION['wa_current_user']) && is_object($_SESSION['wa_current_user'])
        && isset($_SESSION['wa_current_user']->user) ? (int)$_SESSION['wa_current_user']->user : 0;
}

function hrm_lifecycle_salary_revision_permission()
{
    return function_exists('user_check_access') && user_check_access('SA_SALARYREVISION');
}

function hrm_lifecycle_salary_revision_positive_int($value)
{
    if (is_int($value))
        return $value > 0 ? $value : false;
    $value = trim((string)$value);
    if (preg_match('/^[1-9][0-9]*$/D', $value) !== 1)
        return false;
    $number = (int)$value;
    return $number > 0 && (string)$number === $value ? $number : false;
}

function hrm_lifecycle_salary_revision_nonnegative_int($value)
{
    if (is_int($value))
        return $value >= 0 ? $value : false;
    $value = trim((string)$value);
    if (preg_match('/^(?:0|[1-9][0-9]*)$/D', $value) !== 1)
        return false;
    $number = (int)$value;
    return $number >= 0 && (string)$number === $value ? $number : false;
}

function hrm_lifecycle_salary_revision_date($value)
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

function hrm_lifecycle_salary_revision_today_sql()
{
    if (function_exists('Today') && function_exists('date2sql')) {
        $today = date2sql(Today());
        if (hrm_lifecycle_salary_revision_date($today) !== false)
            return $today;
    }
    return date('Y-m-d');
}

/** Canonical positive decimal used in request hashes and draft custody. */
function hrm_lifecycle_salary_revision_amount($value)
{
    if (is_string($value)) {
        $value = trim($value);
        if ($value === '' || preg_match('/^[0-9]+(?:\.[0-9]{1,6})?$/D', $value) !== 1)
            return false;
    } elseif (!is_int($value) && !is_float($value)) {
        return false;
    }
    if (!is_numeric($value))
        return false;
    $number = (float)$value;
    if (!is_finite($number) || $number <= 0 || $number > 999999999999.999999)
        return false;
    $canonical = number_format($number, 6, '.', '');
    $canonical = rtrim(rtrim($canonical, '0'), '.');
    return $canonical === '' || $canonical === '0' ? false : $canonical;
}

/** Bounded legacy-compatible history reason; never copied into PAY-AUD. */
function hrm_lifecycle_salary_revision_reason_value($value)
{
    $value = trim(strip_tags((string)$value));
    if (strlen($value) < 3 || strlen($value) > 240
        || preg_match('/[\x00-\x08\x0B\x0C\x0E-\x1F\x7F]/', $value))
        return false;
    return htmlspecialchars($value, ENT_QUOTES, 'UTF-8');
}

function hrm_lifecycle_salary_revision_idempotency_hash($key)
{
    if (!is_string($key) || strlen($key) < 16 || strlen($key) > 128
        || preg_match('/^[A-Za-z0-9._:-]+$/D', $key) !== 1)
        return false;
    return hash('sha256', $key);
}

function hrm_lifecycle_salary_revision_workflow_is_maker_checker()
{
    if (!defined('ST_HRM_LIFECYCLE_SALARY_REVISION') || !class_exists('ApprovalWorkflow')
        || !ApprovalWorkflow::isRegistered(ST_HRM_LIFECYCLE_SALARY_REVISION)
        || !function_exists('get_approval_workflow_by_type') || !function_exists('get_approval_levels'))
        return false;
    $workflow = get_approval_workflow_by_type(ST_HRM_LIFECYCLE_SALARY_REVISION);
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

function hrm_lifecycle_salary_revision_employee_for_update($employee_number, $employee_id)
{
    if ((int)$employee_number <= 0 || trim((string)$employee_id) === '')
        return false;
    $result = db_query("SELECT employee_number,employee_id,department_id,position_id,grade_id,personal_salary,inactive,released_date "
        ."FROM ".TB_PREF."employees WHERE employee_number=".(int)$employee_number
        ." AND employee_id=".db_escape((string)$employee_id)." LIMIT 1 FOR UPDATE",
        'could not lock Employee Salary Revision employee projection');
    return $result ? db_fetch_assoc($result) : false;
}

/** Require exactly one active Basic element; ambiguity fails closed. */
function hrm_lifecycle_salary_revision_basic_element_for_update()
{
    if (!defined('HRM_ELEM_BASIC'))
        return false;
    $result = db_query("SELECT element_id FROM ".TB_PREF."pay_elements WHERE element_category=".db_escape(HRM_ELEM_BASIC)
        ." AND inactive=0 ORDER BY display_order,element_id FOR UPDATE",
        'could not lock Employee Salary Revision Basic element custody');
    if (!$result)
        return false;
    $first = db_fetch_assoc($result);
    $second = db_fetch_assoc($result);
    if (!$first || $second || (int)$first['element_id'] <= 0)
        return false;
    return (int)$first['element_id'];
}

function hrm_lifecycle_salary_revision_salary_row_for_update($employee_id, $element_id, $effective_date)
{
    $effective_date = hrm_lifecycle_salary_revision_date($effective_date);
    if (trim((string)$employee_id) === '' || (int)$element_id <= 0 || $effective_date === false)
        return false;
    $result = db_query("SELECT salary_id,employee_id,element_id,amount,formula,effective_from,effective_to,is_active,reference "
        ."FROM ".TB_PREF."employee_salary WHERE employee_id=".db_escape((string)$employee_id)
        ." AND element_id=".(int)$element_id." AND effective_from=".db_escape($effective_date)." LIMIT 1 FOR UPDATE",
        'could not lock Employee Salary Revision salary-row custody');
    if (!$result)
        return false;
    $row = db_fetch_assoc($result);
    return $row ? $row : array();
}

function hrm_lifecycle_salary_revision_salary_row_fingerprint($row)
{
    if (!is_array($row))
        return false;
    if (!$row)
        return hash('sha256', 'notrinoserp.hrm-fnd-005.salary-row|none');
    $salary_id = hrm_lifecycle_salary_revision_positive_int(isset($row['salary_id']) ? $row['salary_id'] : 0);
    $element_id = hrm_lifecycle_salary_revision_positive_int(isset($row['element_id']) ? $row['element_id'] : 0);
    $amount = hrm_lifecycle_salary_revision_amount(isset($row['amount']) ? $row['amount'] : '');
    $active = hrm_lifecycle_salary_revision_nonnegative_int(isset($row['is_active']) ? $row['is_active'] : -1);
    $effective_from = hrm_lifecycle_salary_revision_date(isset($row['effective_from']) ? $row['effective_from'] : '');
    if ($salary_id === false || $element_id === false || $amount === false || $active === false
        || !in_array($active, array(0,1), true) || $effective_from === false)
        return false;
    $effective_to = isset($row['effective_to']) && $row['effective_to'] !== null ? trim((string)$row['effective_to']) : '';
    if ($effective_to !== '' && $effective_to !== '0000-00-00' && hrm_lifecycle_salary_revision_date($effective_to) === false)
        return false;
    $formula = isset($row['formula']) && $row['formula'] !== null ? (string)$row['formula'] : '';
    $reference = isset($row['reference']) && $row['reference'] !== null ? (string)$row['reference'] : '';
    $employee_id = isset($row['employee_id']) ? (string)$row['employee_id'] : '';
    return hash('sha256', serialize(array($salary_id,$employee_id,$element_id,$amount,$formula,
        $effective_from,$effective_to,$active,$reference)));
}

function hrm_lifecycle_salary_revision_request_hash($company, $worker_id, $employment_id, $assignment_id,
    $employee_number, $effective_date, $basic_element_id, $new_salary, $reason_hash,
    $salary_row_id, $salary_row_fingerprint, $employee_personal_salary)
{
    foreach (array($worker_id,$employment_id,$assignment_id,$employee_number,$basic_element_id) as $value)
        if (hrm_lifecycle_salary_revision_positive_int((string)$value) === false)
            return false;
    $salary_row_id = hrm_lifecycle_salary_revision_nonnegative_int((string)$salary_row_id);
    $employee_personal_salary = hrm_lifecycle_salary_revision_nonnegative_int((string)$employee_personal_salary);
    $new_salary = hrm_lifecycle_salary_revision_amount($new_salary);
    if ((int)$company < 0 || $salary_row_id === false || $employee_personal_salary === false
        || !in_array($employee_personal_salary, array(0,1), true) || $new_salary === false
        || hrm_lifecycle_salary_revision_date($effective_date) === false
        || !is_string($reason_hash) || preg_match('/^[a-f0-9]{64}$/D', $reason_hash) !== 1
        || !is_string($salary_row_fingerprint) || preg_match('/^[a-f0-9]{64}$/D', $salary_row_fingerprint) !== 1)
        return false;
    return hash('sha256', HRM_LIFECYCLE_SALARY_REVISION_HASH_CONTRACT.'|v='.HRM_LIFECYCLE_SALARY_REVISION_HASH_VERSION
        .'|company='.(int)$company.'|worker_id='.(int)$worker_id.'|employment_id='.(int)$employment_id
        .'|assignment_id='.(int)$assignment_id.'|employee_number='.(int)$employee_number
        .'|effective_date='.$effective_date.'|basic_element_id='.(int)$basic_element_id.'|new_salary='.$new_salary
        .'|reason_hash='.$reason_hash.'|salary_row_id='.(int)$salary_row_id.'|salary_row_fingerprint='.$salary_row_fingerprint
        .'|employee_personal_salary='.(int)$employee_personal_salary);
}

function hrm_lifecycle_salary_revision_command_for_update($command_id)
{
    $command_id = hrm_lifecycle_salary_revision_positive_int($command_id);
    if ($command_id === false)
        return false;
    $result = db_query("SELECT lifecycle_command_id,worker_id,employment_id,assignment_id,command_type,command_status,"
        ."effective_at,idempotency_key_hash,request_hash,approval_draft_id,row_version,requested_at,requested_by,"
        ."completed_at,completed_by FROM ".TB_PREF."hrm_lifecycle_commands WHERE lifecycle_command_id=".$command_id
        ." LIMIT 1 FOR UPDATE", 'could not lock Employee Salary Revision lifecycle command');
    return $result ? db_fetch_assoc($result) : false;
}

function hrm_lifecycle_salary_revision_by_idempotency_for_update($worker_id, $idempotency_hash)
{
    if ((int)$worker_id <= 0 || !is_string($idempotency_hash)
        || preg_match('/^[a-f0-9]{64}$/D', $idempotency_hash) !== 1)
        return false;
    $result = db_query("SELECT lifecycle_command_id,worker_id,employment_id,assignment_id,command_type,command_status,"
        ."effective_at,idempotency_key_hash,request_hash,approval_draft_id,row_version,requested_at,requested_by,"
        ."completed_at,completed_by FROM ".TB_PREF."hrm_lifecycle_commands WHERE worker_id=".(int)$worker_id
        ." AND command_type=".db_escape(HRM_LIFECYCLE_SALARY_REVISION_COMMAND_TYPE)
        ." AND idempotency_key_hash=".db_escape($idempotency_hash)." LIMIT 1 FOR UPDATE",
        'could not lock idempotent Employee Salary Revision lifecycle command');
    return $result ? db_fetch_assoc($result) : false;
}

function hrm_lifecycle_salary_revision_other_pending_exists($worker_id)
{
    if ((int)$worker_id <= 0)
        return true;
    $result = db_query("SELECT lifecycle_command_id FROM ".TB_PREF."hrm_lifecycle_commands WHERE worker_id=".(int)$worker_id
        ." AND command_status='pending' ORDER BY lifecycle_command_id LIMIT 1 FOR UPDATE",
        'could not inspect conflicting Employee lifecycle command');
    return $result && db_fetch_assoc($result) ? true : false;
}

function hrm_lifecycle_salary_revision_audit($action, $reason_code, $command, $legal_entity_id, $restricted_diff)
{
    if (!function_exists('append_domain_audit_event') || !is_array($command)
        || (int)$command['lifecycle_command_id'] <= 0 || (int)$command['worker_id'] <= 0)
        return false;
    $safe = is_array($restricted_diff) ? $restricted_diff : array();
    $safe['command_type'] = HRM_LIFECYCLE_SALARY_REVISION_COMMAND_TYPE;
    $safe['command_status'] = (string)$command['command_status'];
    $safe['row_version'] = (int)$command['row_version'];
    $safe['new_salary_value_stored_in_audit'] = false;
    $safe['reason_value_stored_in_audit'] = false;
    $safe['payroll_rows_mutated'] = 0;
    $safe['payslip_rows_mutated'] = 0;
    $safe['historical_payroll_rows_rewritten'] = 0;
    $safe['historical_payslip_rows_rewritten'] = 0;
    $safe['payroll_post_authority_changed'] = false;
    $safe['payment_execution_authority_changed'] = false;
    $safe['bank_transaction_authority_changed'] = false;
    $safe['gl_accounting_authority_changed'] = false;
    return append_domain_audit_event(array(
        'company_id'=>function_exists('user_company') ? (int)user_company() : 0,
        'legal_entity_id'=>(int)$legal_entity_id,
        'entity_type'=>'hrm_fnd_005_salary_revision_command',
        'entity_key'=>array('lifecycle_command_id'=>(int)$command['lifecycle_command_id'],
            'worker_id'=>(int)$command['worker_id']),
        'action'=>(string)$action,
        'reason_code'=>(string)$reason_code,
        'before'=>array('command_status'=>(int)$command['row_version'] === 1 ? 'new' : 'pending'),
        'after'=>array('command_status'=>(string)$command['command_status'],'row_version'=>(int)$command['row_version']),
        'restricted_diff'=>$safe,
    )) !== false;
}

function hrm_lifecycle_salary_revision_draft_matches($draft_data, $command, $allow_terminal_version)
{
    if (!is_array($draft_data) || !is_array($command))
        return false;
    $expected_keys = array('assignment_id','basic_element_id','command_row_version','effective_date','employee_number',
        'employee_personal_salary','employment_id','lifecycle_command_id','new_salary','reason','reason_hash','request_hash',
        'salary_row_fingerprint','salary_row_id','worker_id');
    $keys = array_keys($draft_data);
    sort($keys, SORT_STRING);
    if ($keys !== $expected_keys)
        return false;
    foreach (array('lifecycle_command_id','worker_id','employment_id','assignment_id','employee_number','basic_element_id') as $key)
        if (hrm_lifecycle_salary_revision_positive_int($draft_data[$key]) === false)
            return false;
    if (hrm_lifecycle_salary_revision_nonnegative_int($draft_data['salary_row_id']) === false
        || hrm_lifecycle_salary_revision_nonnegative_int($draft_data['employee_personal_salary']) === false)
        return false;
    $draft_version = hrm_lifecycle_salary_revision_positive_int($draft_data['command_row_version']);
    $terminal_successor = $allow_terminal_version && (int)$command['row_version'] === 2 && $draft_version === 1;
    $reason = hrm_lifecycle_salary_revision_reason_value($draft_data['reason']);
    $new_salary = hrm_lifecycle_salary_revision_amount($draft_data['new_salary']);
    if ($draft_version === false || ($draft_version !== (int)$command['row_version'] && !$terminal_successor)
        || hrm_lifecycle_salary_revision_date($draft_data['effective_date']) === false || $reason === false || $new_salary === false
        || $reason !== (string)$draft_data['reason'] || !is_string($draft_data['reason_hash'])
        || hash('sha256', $reason) !== (string)$draft_data['reason_hash']
        || !is_string($draft_data['request_hash']) || preg_match('/^[a-f0-9]{64}$/D', $draft_data['request_hash']) !== 1
        || !is_string($draft_data['salary_row_fingerprint']) || preg_match('/^[a-f0-9]{64}$/D', $draft_data['salary_row_fingerprint']) !== 1)
        return false;
    if ((int)$draft_data['lifecycle_command_id'] !== (int)$command['lifecycle_command_id']
        || (int)$draft_data['worker_id'] !== (int)$command['worker_id']
        || (int)$draft_data['employment_id'] !== (int)$command['employment_id']
        || (int)$draft_data['assignment_id'] !== (int)$command['assignment_id']
        || (string)$draft_data['effective_date'].' 00:00:00' !== (string)$command['effective_at']
        || (string)$draft_data['request_hash'] !== (string)$command['request_hash'])
        return false;
    $recomputed = hrm_lifecycle_salary_revision_request_hash(
        function_exists('user_company') ? (int)user_company() : 0,
        (int)$draft_data['worker_id'], (int)$draft_data['employment_id'], (int)$draft_data['assignment_id'],
        (int)$draft_data['employee_number'], (string)$draft_data['effective_date'], (int)$draft_data['basic_element_id'],
        $new_salary, (string)$draft_data['reason_hash'], (int)$draft_data['salary_row_id'],
        (string)$draft_data['salary_row_fingerprint'], (int)$draft_data['employee_personal_salary']
    );
    return is_string($recomputed) && $recomputed === (string)$command['request_hash'];
}

/** Submit one fixed salary revision for central maker/checker approval. */
function submit_hrm_lifecycle_employee_salary_revision($employee_id, $new_salary, $effective_date, $reason, $idempotency_key)
{
    $employee_id = trim((string)$employee_id);
    $new_salary = hrm_lifecycle_salary_revision_amount($new_salary);
    $effective_date = hrm_lifecycle_salary_revision_date($effective_date);
    $reason = hrm_lifecycle_salary_revision_reason_value($reason);
    $idempotency_hash = hrm_lifecycle_salary_revision_idempotency_hash($idempotency_key);
    $actor = hrm_lifecycle_salary_revision_actor_id();
    $company = function_exists('user_company') ? (int)user_company() : 0;
    if ($employee_id === '' || strlen($employee_id) > 20 || $new_salary === false || $effective_date === false
        || $reason === false || $idempotency_hash === false || strcmp($effective_date, hrm_lifecycle_salary_revision_today_sql()) > 0
        || $actor <= 0 || $company < 0 || !hrm_lifecycle_salary_revision_permission()
        || !hrm_lifecycle_salary_revision_workflow_is_maker_checker()
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
        $employee = hrm_lifecycle_salary_revision_employee_for_update($employee_number, $employee_id);
        if (!$employment || !$assignment || !$employee || (int)$employment['worker_id'] !== (int)$binding['worker_id']
            || (int)$assignment['employment_id'] !== (int)$employment['employment_id']
            || (int)$assignment['legacy_employee_number'] !== $employee_number || (int)$assignment['legacy_current'] !== 1
            || (int)$employment['legacy_current'] !== 1 || (int)$assignment['primary_flag'] !== 1
            || (string)$assignment['assignment_type_code'] !== 'legacy_primary'
            || (int)$employee['department_id'] !== (int)$assignment['legacy_department_id']
            || (int)$employee['position_id'] !== (int)$assignment['legacy_position_id']
            || (int)$employee['grade_id'] !== (int)$assignment['legacy_grade_id']
            || (int)$employee['inactive'] !== 0 || ($employee['released_date'] !== null && $employee['released_date'] !== ''))
            break;
        if ($employment['start_date'] !== null && $employment['start_date'] !== ''
            && strcmp($effective_date, (string)$employment['start_date']) < 0)
            break;
        if ($employment['end_date'] !== null && $employment['end_date'] !== ''
            && strcmp($effective_date, (string)$employment['end_date']) > 0)
            break;
        if ($assignment['effective_from'] !== null && $assignment['effective_from'] !== ''
            && strcmp($effective_date, (string)$assignment['effective_from']) < 0)
            break;
        if ($assignment['effective_to'] !== null && $assignment['effective_to'] !== ''
            && strcmp($effective_date, (string)$assignment['effective_to']) > 0)
            break;

        $basic_element_id = hrm_lifecycle_salary_revision_basic_element_for_update();
        if ($basic_element_id === false)
            break;
        $salary_row = hrm_lifecycle_salary_revision_salary_row_for_update($employee_id, $basic_element_id, $effective_date);
        if ($salary_row === false)
            break;
        $salary_row_id = $salary_row ? (int)$salary_row['salary_id'] : 0;
        $salary_row_fingerprint = hrm_lifecycle_salary_revision_salary_row_fingerprint($salary_row);
        if ($salary_row_fingerprint === false)
            break;
        if ($salary_row && hrm_lifecycle_salary_revision_amount($salary_row['amount']) === $new_salary
            && (int)$employee['personal_salary'] === 1 && (int)$salary_row['is_active'] === 1)
            break;
        $reason_hash = hash('sha256', $reason);
        $request_hash = hrm_lifecycle_salary_revision_request_hash($company, (int)$binding['worker_id'],
            (int)$employment['employment_id'], (int)$assignment['assignment_id'], $employee_number, $effective_date,
            $basic_element_id, $new_salary, $reason_hash, $salary_row_id, $salary_row_fingerprint,
            (int)$employee['personal_salary']);
        if ($request_hash === false)
            break;
        $existing = hrm_lifecycle_salary_revision_by_idempotency_for_update((int)$binding['worker_id'], $idempotency_hash);
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
        if (hrm_lifecycle_salary_revision_other_pending_exists((int)$binding['worker_id']))
            break;

        if (!db_query("INSERT INTO ".TB_PREF."hrm_lifecycle_commands "
            ."(worker_id,employment_id,assignment_id,command_type,command_status,effective_at,idempotency_key_hash,"
            ."request_hash,approval_draft_id,row_version,requested_by) VALUES ("
            .(int)$binding['worker_id'].",".(int)$employment['employment_id'].",".(int)$assignment['assignment_id'].','
            .db_escape(HRM_LIFECYCLE_SALARY_REVISION_COMMAND_TYPE).",'pending',".db_escape($effective_date.' 00:00:00').','
            .db_escape($idempotency_hash).','.db_escape($request_hash).",NULL,1,".$actor.")",
            'could not create Employee Salary Revision lifecycle command'))
            break;
        $command_id = (int)db_insert_id();
        if ($command_id <= 0)
            break;

        $draft_data = array(
            'lifecycle_command_id'=>$command_id,'command_row_version'=>1,
            'worker_id'=>(int)$binding['worker_id'],'employment_id'=>(int)$employment['employment_id'],
            'assignment_id'=>(int)$assignment['assignment_id'],'employee_number'=>$employee_number,
            'effective_date'=>$effective_date,'basic_element_id'=>$basic_element_id,'new_salary'=>$new_salary,
            'reason'=>$reason,'reason_hash'=>$reason_hash,'salary_row_id'=>$salary_row_id,
            'salary_row_fingerprint'=>$salary_row_fingerprint,'employee_personal_salary'=>(int)$employee['personal_salary'],
            'request_hash'=>$request_hash,
        );
        $approval = get_approval_workflow_service();
        $approval_result = $approval->submit(ST_HRM_LIFECYCLE_SALARY_REVISION, $draft_data, 0,
            array('summary'=>sprintf(_('Employee salary revision lifecycle command #%d'), $command_id)));
        if (!is_array($approval_result) || !isset($approval_result['status'],$approval_result['draft_id'])
            || (string)$approval_result['status'] !== 'pending' || (int)$approval_result['draft_id'] <= 0)
            break;
        $draft_id = (int)$approval_result['draft_id'];
        if (!db_query("UPDATE ".TB_PREF."hrm_lifecycle_commands SET approval_draft_id=".$draft_id
            ." WHERE lifecycle_command_id=".$command_id." AND approval_draft_id IS NULL AND row_version=1"
            ." AND command_status='pending'", 'could not bind Employee Salary Revision command to approval draft')
            || (int)db_num_affected_rows() !== 1)
            break;
        $command = array('lifecycle_command_id'=>$command_id,'worker_id'=>(int)$binding['worker_id'],
            'employment_id'=>(int)$employment['employment_id'],'assignment_id'=>(int)$assignment['assignment_id'],
            'command_status'=>'pending','row_version'=>1);
        if (!hrm_lifecycle_salary_revision_audit('hrm.lifecycle.salary_revision.submitted', HRM_LIFECYCLE_SALARY_REVISION_REASON,
            $command, (int)$employment['legal_entity_id'], array(
                'approval_draft_id'=>$draft_id,'maker_user_id'=>$actor,'request_hash'=>$request_hash,
                'idempotency_key_plaintext_stored'=>false,'effective_date_bound'=>true,
                'worker_employment_assignment_bound'=>true,'basic_element_bound'=>true,
                'salary_row_fingerprint_bound'=>true,'same_subject_conflict_guard'=>true,
                'existing_salary_revision_route_redirected'=>false)))
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

function hrm_lifecycle_salary_revision_bound_draft($draft_data, $draft_id, $allow_terminal_version)
{
    $draft_id = hrm_lifecycle_salary_revision_positive_int($draft_id);
    if ($draft_id === false || !is_array($draft_data) || !isset($draft_data['lifecycle_command_id']))
        return false;
    $command = hrm_lifecycle_salary_revision_command_for_update($draft_data['lifecycle_command_id']);
    if (!$command || (string)$command['command_type'] !== HRM_LIFECYCLE_SALARY_REVISION_COMMAND_TYPE
        || (int)$command['approval_draft_id'] !== $draft_id
        || !hrm_lifecycle_salary_revision_draft_matches($draft_data, $command, $allow_terminal_version))
        return false;
    return $command;
}

function execute_approved_hrm_lifecycle_salary_revision($draft_data, $reserved_trans_no, $draft_id)
{
    if (!hrm_lifecycle_salary_revision_permission())
        return false;
    $actor = hrm_lifecycle_salary_revision_actor_id();
    $command = hrm_lifecycle_salary_revision_bound_draft($draft_data, $draft_id, true);
    if (!$command || $actor <= 0)
        return false;
    if ((string)$command['command_status'] === 'completed' && (int)$command['row_version'] === 2)
        return (int)$command['lifecycle_command_id'];
    if ((string)$command['command_status'] !== 'pending' || (int)$command['row_version'] !== 1
        || (int)$command['requested_by'] <= 0 || (int)$command['requested_by'] === $actor)
        return false;
    $effective_date = (string)$draft_data['effective_date'];
    if (strcmp($effective_date, hrm_lifecycle_salary_revision_today_sql()) > 0)
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
        || (int)$assignment['primary_flag'] !== 1 || (string)$assignment['assignment_type_code'] !== 'legacy_primary')
        return false;
    $employee_id = (string)$binding['employee_id'];
    $employee = hrm_lifecycle_salary_revision_employee_for_update($employee_number, $employee_id);
    if (!$employee || (int)$employee['department_id'] !== (int)$assignment['legacy_department_id']
        || (int)$employee['position_id'] !== (int)$assignment['legacy_position_id']
        || (int)$employee['grade_id'] !== (int)$assignment['legacy_grade_id']
        || (int)$employee['inactive'] !== 0 || ($employee['released_date'] !== null && $employee['released_date'] !== '')
        || (int)$employee['personal_salary'] !== (int)$draft_data['employee_personal_salary'])
        return false;
    if ($employment['start_date'] !== null && $employment['start_date'] !== ''
        && strcmp($effective_date, (string)$employment['start_date']) < 0)
        return false;
    if ($employment['end_date'] !== null && $employment['end_date'] !== ''
        && strcmp($effective_date, (string)$employment['end_date']) > 0)
        return false;
    if ($assignment['effective_from'] !== null && $assignment['effective_from'] !== ''
        && strcmp($effective_date, (string)$assignment['effective_from']) < 0)
        return false;
    if ($assignment['effective_to'] !== null && $assignment['effective_to'] !== ''
        && strcmp($effective_date, (string)$assignment['effective_to']) > 0)
        return false;

    $basic_element_id = hrm_lifecycle_salary_revision_basic_element_for_update();
    if ($basic_element_id === false || $basic_element_id !== (int)$draft_data['basic_element_id'])
        return false;
    $salary_row = hrm_lifecycle_salary_revision_salary_row_for_update($employee_id, $basic_element_id, $effective_date);
    if ($salary_row === false)
        return false;
    $salary_row_id = $salary_row ? (int)$salary_row['salary_id'] : 0;
    $salary_row_fingerprint = hrm_lifecycle_salary_revision_salary_row_fingerprint($salary_row);
    if ($salary_row_id !== (int)$draft_data['salary_row_id'] || $salary_row_fingerprint === false
        || $salary_row_fingerprint !== (string)$draft_data['salary_row_fingerprint'])
        return false;

    $old_salary = get_employee_total_salary($employee_id, $effective_date);
    if ((int)$employee['personal_salary'] !== 1 && !update_employee($employee_id, array('personal_salary'=>1)))
        return false;
    $new_salary = hrm_lifecycle_salary_revision_amount($draft_data['new_salary']);
    if ($new_salary === false)
        return false;
    if ($salary_row) {
        $salary_saved = update_employee_salary($salary_row_id, $new_salary, $effective_date,
            empty($salary_row['effective_to']) ? '' : (string)$salary_row['effective_to'],
            empty($salary_row['formula']) ? '' : (string)$salary_row['formula'], _('Salary revision'), 1);
        $new_salary_id = $salary_row_id;
    } else {
        $new_salary_id = add_employee_salary($employee_id, $basic_element_id, $new_salary, $effective_date,
            '', '', _('Salary revision'));
        $salary_saved = (int)$new_salary_id > 0;
    }
    if (!$salary_saved || (int)$new_salary_id <= 0)
        return false;
    $written_salary = hrm_lifecycle_salary_revision_salary_row_for_update($employee_id, $basic_element_id, $effective_date);
    if (!$written_salary || (int)$written_salary['salary_id'] !== (int)$new_salary_id
        || (int)$written_salary['is_active'] !== 1
        || hrm_lifecycle_salary_revision_amount($written_salary['amount']) !== $new_salary)
        return false;

    $approved_by = isset($_SESSION['wa_current_user']->loginname) ? (string)$_SESSION['wa_current_user']->loginname : '';
    $history_id = add_employee_history($employee_id, HRM_HIST_SALARY_CHANGE, $effective_date,
        (int)$employee['department_id'], (int)$employee['department_id'],
        (int)$employee['position_id'], (int)$employee['position_id'],
        (int)$employee['grade_id'], (int)$employee['grade_id'], $old_salary, (float)$new_salary,
        (string)$draft_data['reason'], $approved_by);
    if ((int)$history_id <= 0)
        return false;
    if (!db_query("UPDATE ".TB_PREF."hrm_lifecycle_commands SET command_status='completed',row_version=2,"
        ."completed_at=UTC_TIMESTAMP(),completed_by=".$actor." WHERE lifecycle_command_id=".(int)$command['lifecycle_command_id']
        ." AND command_status='pending' AND row_version=1 AND approval_draft_id=".(int)$draft_id,
        'could not complete Employee Salary Revision lifecycle command') || (int)db_num_affected_rows() !== 1)
        return false;
    $command['command_status'] = 'completed';
    $command['row_version'] = 2;
    if (!hrm_lifecycle_salary_revision_audit('hrm.lifecycle.salary_revision.completed', HRM_LIFECYCLE_SALARY_REVISION_REASON,
        $command, (int)$employment['legal_entity_id'], array(
            'approval_draft_id'=>(int)$draft_id,'maker_user_id'=>(int)$command['requested_by'],
            'checker_user_id'=>$actor,'maker_checker_separated'=>true,'employee_history_id'=>(int)$history_id,
            'salary_row_id'=>(int)$new_salary_id,'basic_element_id'=>$basic_element_id,
            'salary_amount_changed'=>true,'employee_personal_salary_enabled'=>true,
            'salary_row_predecessor_fingerprint_bound'=>true,'request_hash'=>(string)$command['request_hash'])))
        return false;
    return (int)$command['lifecycle_command_id'];
}

function hrm_lifecycle_salary_revision_terminalize($draft_data, $draft_id, $status, $maker_only)
{
    if (!hrm_lifecycle_salary_revision_permission() || !in_array($status, array('rejected','cancelled'), true))
        return false;
    $actor = hrm_lifecycle_salary_revision_actor_id();
    $command = hrm_lifecycle_salary_revision_bound_draft($draft_data, $draft_id, true);
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
        'could not terminalize Employee Salary Revision lifecycle command') || (int)db_num_affected_rows() !== 1)
        return false;
    $command['command_status'] = $status;
    $command['row_version'] = 2;
    $action = $status === 'rejected' ? 'hrm.lifecycle.salary_revision.rejected' : 'hrm.lifecycle.salary_revision.cancelled';
    $reason = $status === 'rejected' ? 'hrm_fnd_005_salary_revision_rejection' : 'hrm_fnd_005_salary_revision_cancellation';
    return hrm_lifecycle_salary_revision_audit($action, $reason, $command, (int)$employment['legal_entity_id'], array(
        'approval_draft_id'=>(int)$draft_id,'actor_user_id'=>$actor,'maker_only'=>$maker_only,
        'maker_checker_separated'=>$maker_only ? false : true,'employee_rows_mutated'=>0,
        'employee_salary_rows_mutated'=>0,'employee_history_rows_added'=>0))
        ? (int)$command['lifecycle_command_id'] : false;
}

function reject_hrm_lifecycle_salary_revision_draft($draft_data, $draft_id, $comments)
{
    return hrm_lifecycle_salary_revision_terminalize($draft_data, $draft_id, 'rejected', false);
}

function cancel_hrm_lifecycle_salary_revision_draft($draft_data, $draft_id, $comments)
{
    return hrm_lifecycle_salary_revision_terminalize($draft_data, $draft_id, 'cancelled', true);
}
