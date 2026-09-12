<?php
/**
 * HRM-FND-005 same-code Employee Rehire command custody foundation.
 *
 * This source is deliberately non-mutating. It freezes the exact existing
 * Employee/Person/Worker/terminated-Employment/Contract/Assignment and
 * lifecycle-managed Separation evidence that a future fixed Rehire command
 * must bind. It does not insert a lifecycle command, register approval,
 * reactivate an Employee, create successor normalized rows, or reuse Hire/CSV
 * authority. The execution rollover remains a separately reviewed dependency.
 */

if (!defined('HRM_LIFECYCLE_REHIRE_COMMAND_TYPE'))
    define('HRM_LIFECYCLE_REHIRE_COMMAND_TYPE', 'employee_rehire');
if (!defined('HRM_LIFECYCLE_REHIRE_HASH_CONTRACT'))
    define('HRM_LIFECYCLE_REHIRE_HASH_CONTRACT', 'notrinoserp.hrm-fnd-005.employee-rehire-custody');
if (!defined('HRM_LIFECYCLE_REHIRE_HASH_VERSION'))
    define('HRM_LIFECYCLE_REHIRE_HASH_VERSION', '1');

/** @return bool */
function hrm_lifecycle_rehire_permission()
{
    return function_exists('user_check_access') && user_check_access('SA_EMPLOYEE');
}

/** @param mixed $value @return int|false */
function hrm_lifecycle_rehire_positive_int($value)
{
    if (is_int($value)) return $value > 0 ? $value : false;
    $value = trim((string)$value);
    if (preg_match('/^[1-9][0-9]*$/D', $value) !== 1) return false;
    $number = (int)$value;
    return $number > 0 && (string)$number === $value ? $number : false;
}

/** @param mixed $value @return string|false */
function hrm_lifecycle_rehire_date($value)
{
    $value = trim((string)$value);
    if (preg_match('/^[0-9]{4}-[0-9]{2}-[0-9]{2}$/D', $value) !== 1) return false;
    $date = DateTime::createFromFormat('!Y-m-d', $value);
    $errors = DateTime::getLastErrors();
    if (!$date || (is_array($errors)
        && ((int)$errors['warning_count'] !== 0 || (int)$errors['error_count'] !== 0))
        || $date->format('Y-m-d') !== $value) return false;
    return $value;
}

/** @return array|false */
function hrm_lifecycle_rehire_employee_for_update($employee_number)
{
    $employee_number = hrm_lifecycle_rehire_positive_int($employee_number);
    if ($employee_number === false || !function_exists('db_query') || !defined('TB_PREF')) return false;
    $result = db_query("SELECT employee_number,employee_id,hire_date,released_date,inactive,employment_type,"
        ."department_id,position_id,grade_id,shift_id,cost_center_id,dimension2_id,reporting_to "
        ."FROM ".TB_PREF."employees WHERE employee_number=".$employee_number." LIMIT 1 FOR UPDATE",
        'could not lock Employee Rehire legacy projection');
    return $result ? db_fetch_assoc($result) : false;
}

/** @return array|false */
function hrm_lifecycle_rehire_last_separation_history_for_update($employee_id)
{
    $employee_id = trim((string)$employee_id);
    if ($employee_id === '' || !defined('HRM_HIST_SEPARATION')) return false;
    $result = db_query("SELECT history_id,change_type,effective_date,reason FROM ".TB_PREF."employee_history "
        ."WHERE employee_id=".db_escape($employee_id)." AND change_type=".db_escape(HRM_HIST_SEPARATION)
        ." ORDER BY history_id DESC LIMIT 1 FOR UPDATE",
        'could not lock Employee Rehire Separation predecessor history');
    return $result ? db_fetch_assoc($result) : false;
}

/** @return array|false */
function hrm_lifecycle_rehire_separation_predecessor_for_update($history, $worker_id, $employment_id, $assignment_id)
{
    if (!is_array($history) || (string)$history['change_type'] !== HRM_HIST_SEPARATION
        || preg_match('/^lifecycle_command:([1-9][0-9]*)(?:;.*)?$/D', (string)$history['reason'], $m) !== 1)
        return false;
    $command_id = (int)$m[1];
    $result = db_query("SELECT lifecycle_command_id,worker_id,employment_id,assignment_id,command_type,"
        ."command_status,effective_at,row_version,requested_by,completed_by FROM ".TB_PREF."hrm_lifecycle_commands "
        ."WHERE lifecycle_command_id=".$command_id." LIMIT 1 FOR UPDATE",
        'could not lock Employee Rehire Separation predecessor command');
    $command = $result ? db_fetch_assoc($result) : false;
    if (!$command || (string)$command['command_type'] !== 'employee_separation'
        || (string)$command['command_status'] !== 'completed' || (int)$command['row_version'] !== 2
        || (int)$command['worker_id'] !== (int)$worker_id
        || (int)$command['employment_id'] !== (int)$employment_id
        || (int)$command['assignment_id'] !== (int)$assignment_id
        || (string)$command['effective_at'] !== (string)$history['effective_date'].' 00:00:00') return false;
    return $command;
}

/** @return string|false */
function hrm_lifecycle_rehire_request_hash($company, $worker_id, $predecessor_employment_id,
    $predecessor_contract_id, $predecessor_assignment_id, $employee_number,
    $separation_command_id, $effective_date)
{
    foreach (array($worker_id,$predecessor_employment_id,$predecessor_contract_id,
        $predecessor_assignment_id,$employee_number,$separation_command_id) as $value)
        if (hrm_lifecycle_rehire_positive_int((string)$value) === false) return false;
    $effective_date = hrm_lifecycle_rehire_date($effective_date);
    if ((int)$company < 0 || $effective_date === false) return false;
    return hash('sha256', HRM_LIFECYCLE_REHIRE_HASH_CONTRACT.'|v='.HRM_LIFECYCLE_REHIRE_HASH_VERSION
        .'|company='.(int)$company.'|worker_id='.(int)$worker_id
        .'|predecessor_employment_id='.(int)$predecessor_employment_id
        .'|predecessor_contract_id='.(int)$predecessor_contract_id
        .'|predecessor_assignment_id='.(int)$predecessor_assignment_id
        .'|employee_number='.(int)$employee_number
        .'|separation_command_id='.(int)$separation_command_id
        .'|effective_date='.$effective_date);
}

/**
 * Read/lock the exact same-code Rehire predecessor custody. No business row is
 * written. Historical pre-framework separations are intentionally ineligible.
 *
 * @return array|false
 */
function get_hrm_lifecycle_employee_rehire_custody_for_update($employee_number, $effective_date)
{
    if (!hrm_lifecycle_rehire_permission()
        || !function_exists('get_hrm_person_worker_binding')
        || !function_exists('get_hrm_legacy_current_employment')
        || !function_exists('get_hrm_legacy_current_contract')
        || !function_exists('get_hrm_legacy_current_assignment')) return false;
    $employee_number = hrm_lifecycle_rehire_positive_int($employee_number);
    $effective_date = hrm_lifecycle_rehire_date($effective_date);
    if ($employee_number === false || $effective_date === false) return false;

    $employee = hrm_lifecycle_rehire_employee_for_update($employee_number);
    $binding = get_hrm_person_worker_binding($employee_number, true);
    $employment = get_hrm_legacy_current_employment($employee_number, true);
    $contract = get_hrm_legacy_current_contract($employee_number, true);
    $assignment = get_hrm_legacy_current_assignment($employee_number, true);
    if (!$employee || !$binding || !$employment || !$contract || !$assignment
        || (int)$employee['inactive'] !== 1 || empty($employee['released_date'])
        || !empty($binding['retired_at'])
        || (string)$binding['employee_id'] !== (string)$employee['employee_id']
        || (int)$employment['worker_id'] !== (int)$binding['worker_id']
        || (int)$employment['legacy_current'] !== 1 || (int)$employment['employment_sequence'] < 1
        || ((int)$employment['employment_sequence'] > 1
            && (!function_exists('hrm_lifecycle_rehire_compatibility_allows')
                || !hrm_lifecycle_rehire_compatibility_allows($employee, $employment)))
        || (string)$employment['employment_status'] !== 'terminated'
        || hrm_lifecycle_rehire_date($employment['end_date']) === false
        || (string)$employee['released_date'] !== (string)$employment['end_date']
        || (int)$contract['employment_id'] !== (int)$employment['employment_id']
        || (int)$contract['legacy_current'] !== 1 || (string)$contract['effective_to'] !== (string)$employment['end_date']
        || (int)$assignment['employment_id'] !== (int)$employment['employment_id']
        || (int)$assignment['legacy_current'] !== 1 || (int)$assignment['primary_flag'] !== 1
        || (string)$assignment['effective_to'] !== (string)$employment['end_date']
        || strcmp($effective_date, (string)$employment['end_date']) <= 0) return false;

    $history = hrm_lifecycle_rehire_last_separation_history_for_update($employee['employee_id']);
    $separation = hrm_lifecycle_rehire_separation_predecessor_for_update(
        $history, $binding['worker_id'], $employment['employment_id'], $assignment['assignment_id']);
    if (!$separation || (string)$history['effective_date'] !== (string)$employment['end_date']) return false;

    $company = function_exists('user_company') ? (int)user_company() : 0;
    $request_hash = hrm_lifecycle_rehire_request_hash($company, $binding['worker_id'],
        $employment['employment_id'], $contract['contract_id'], $assignment['assignment_id'],
        $employee_number, $separation['lifecycle_command_id'], $effective_date);
    if ($request_hash === false) return false;
    return array(
        'employee_number'=>$employee_number,'worker_id'=>(int)$binding['worker_id'],
        'person_id'=>(int)$binding['person_id'],'predecessor_employment_id'=>(int)$employment['employment_id'],
        'predecessor_contract_id'=>(int)$contract['contract_id'],'predecessor_assignment_id'=>(int)$assignment['assignment_id'],
        'separation_command_id'=>(int)$separation['lifecycle_command_id'],'separation_date'=>(string)$employment['end_date'],
        'effective_date'=>$effective_date,'request_hash'=>$request_hash,
        'same_code_required'=>true,'same_worker_required'=>true,'same_person_required'=>true,
        'historical_pre_framework_separation_allowed'=>false,'business_writer_released'=>false,
        'approval_transaction_type_registered'=>false,'employment_rollover_released'=>false,
    );
}
