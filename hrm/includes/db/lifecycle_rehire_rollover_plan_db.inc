<?php
/**
 * HRM-FND-005 same-code Employee Rehire normalized rollover plan foundation.
 *
 * Deliberately non-mutating. It consumes the accepted 1.0.373 Rehire custody
 * and freezes a future atomic rollover plan without changing any business row.
 */

include_once(dirname(__FILE__).'/lifecycle_rehire_command_db.inc');

if (!defined('HRM_HIST_REHIRE'))
    define('HRM_HIST_REHIRE', 'rehire');
if (!defined('HRM_LIFECYCLE_REHIRE_ROLLOVER_PLAN_VERSION'))
    define('HRM_LIFECYCLE_REHIRE_ROLLOVER_PLAN_VERSION', '1');

/** @return array|false */
function hrm_lifecycle_rehire_rollover_sequence_state_for_update($custody)
{
    if (!is_array($custody) || empty($custody['worker_id']) || empty($custody['employee_number'])
        || empty($custody['predecessor_employment_id']) || empty($custody['predecessor_contract_id'])
        || empty($custody['predecessor_assignment_id'])) return false;

    $worker = (int)$custody['worker_id'];
    $employee = (int)$custody['employee_number'];
    $employment_id = (int)$custody['predecessor_employment_id'];
    $contract_id = (int)$custody['predecessor_contract_id'];
    $assignment_id = (int)$custody['predecessor_assignment_id'];

    $er = db_query("SELECT employment_id,employment_sequence,legal_entity_id,legacy_current,start_date,end_date,"
        ."employment_status,interval_quality,source FROM ".TB_PREF."hrm_employments WHERE worker_id=".$worker
        ." ORDER BY employment_sequence,employment_id FOR UPDATE", 'could not lock Employee Rehire Employment sequence state');
    if (!$er) return false;
    $employments = array(); $max_employment_sequence = 0; $current_employments = 0; $predecessor_employment = false;
    while ($row = db_fetch_assoc($er)) {
        $employments[] = $row;
        $max_employment_sequence = max($max_employment_sequence, (int)$row['employment_sequence']);
        if ((string)$row['legacy_current'] === '1') $current_employments++;
        if ((int)$row['employment_id'] === $employment_id) $predecessor_employment = $row;
    }
    if (!$predecessor_employment || $current_employments !== 1
        || (string)$predecessor_employment['legacy_current'] !== '1'
        || (int)$predecessor_employment['employment_sequence'] !== $max_employment_sequence
        || (string)$predecessor_employment['employment_status'] !== 'terminated') return false;

    $cr = db_query("SELECT contract_id,contract_sequence,legacy_current,legacy_employment_type,contract_type_code,"
        ."type_quality,effective_from,effective_to,interval_quality,source FROM ".TB_PREF."hrm_contracts WHERE employment_id=".$employment_id
        ." ORDER BY contract_sequence,contract_id FOR UPDATE", 'could not lock Employee Rehire Contract sequence state');
    if (!$cr) return false;
    $current_contracts = 0; $predecessor_contract = false;
    while ($row = db_fetch_assoc($cr)) {
        if ((string)$row['legacy_current'] === '1') $current_contracts++;
        if ((int)$row['contract_id'] === $contract_id) $predecessor_contract = $row;
    }
    if (!$predecessor_contract || $current_contracts !== 1 || (string)$predecessor_contract['legacy_current'] !== '1') return false;

    $ar = db_query("SELECT assignment_id,assignment_sequence,legacy_current,assignment_type_code,primary_flag,"
        ."legacy_department_id,organization_unit_id,legacy_position_id,position_id,job_id,work_location_id,manager_assignment_id,"
        ."legacy_grade_id,grade_id,legacy_shift_id,legacy_reporting_to,legacy_cost_center_id,legacy_dimension2_id,"
        ."reference_quality,effective_from,effective_to,interval_quality,source FROM ".TB_PREF."hrm_assignments WHERE employment_id=".$employment_id
        ." ORDER BY assignment_sequence,assignment_id FOR UPDATE", 'could not lock Employee Rehire Assignment sequence state');
    if (!$ar) return false;
    $current_assignments = 0; $predecessor_assignment = false;
    while ($row = db_fetch_assoc($ar)) {
        if ((string)$row['legacy_current'] === '1') $current_assignments++;
        if ((int)$row['assignment_id'] === $assignment_id) $predecessor_assignment = $row;
    }
    if (!$predecessor_assignment || $current_assignments !== 1
        || (string)$predecessor_assignment['legacy_current'] !== '1'
        || (int)$predecessor_assignment['primary_flag'] !== 1) return false;

    return array(
        'predecessor_employment'=>$predecessor_employment,
        'predecessor_contract'=>$predecessor_contract,
        'predecessor_assignment'=>$predecessor_assignment,
        'successor_employment_sequence'=>$max_employment_sequence + 1,
        'successor_contract_sequence'=>1,
        'successor_assignment_sequence'=>1,
        'predecessor_retired_legacy_current'=>null,
        'successor_legacy_current'=>1,
    );
}

/**
 * Protected payroll period overlap is denied only when this Employee already
 * has a persisted payslip for a period containing the requested Rehire date.
 * Status 1..5 is calculated through closed financial custody.
 * @return array|false false on query failure, [] when clear, rows when blocked.
 */
function hrm_lifecycle_rehire_protected_payroll_overlap_for_update($employee_id, $effective_date)
{
    $employee_id = trim((string)$employee_id);
    $effective_date = function_exists('hrm_lifecycle_rehire_date') ? hrm_lifecycle_rehire_date($effective_date) : false;
    if ($employee_id === '' || $effective_date === false) return false;
    $sql = "SELECT p.period_id,p.from_date,p.to_date,p.status,ps.payslip_id FROM ".TB_PREF."payroll_periods p "
        ."INNER JOIN ".TB_PREF."payslips ps ON ps.payroll_period_id=p.period_id "
        ."WHERE ps.employee_id=".db_escape($employee_id)." AND p.status IN (1,2,3,4,5) "
        ."AND p.from_date<=".db_escape($effective_date)." AND p.to_date>=".db_escape($effective_date)
        ." ORDER BY p.period_id FOR UPDATE";
    $result = db_query($sql, 'could not verify Employee Rehire protected payroll-period overlap');
    if (!$result) return false;
    $rows = array(); while ($row = db_fetch_assoc($result)) $rows[] = $row;
    return $rows;
}

/**
 * Return a deterministic non-mutating execution plan for future maker/checker
 * Rehire finalization. Any protected payroll overlap denies a plan.
 * @return array|false
 */
function get_hrm_lifecycle_employee_rehire_rollover_plan_for_update($employee_number, $effective_date)
{
    if (!function_exists('get_hrm_lifecycle_employee_rehire_custody_for_update')) return false;
    $custody = get_hrm_lifecycle_employee_rehire_custody_for_update($employee_number, $effective_date);
    if (!$custody) return false;
    $sequence = hrm_lifecycle_rehire_rollover_sequence_state_for_update($custody);
    if (!$sequence) return false;
    $employee = function_exists('hrm_lifecycle_rehire_employee_for_update')
        ? hrm_lifecycle_rehire_employee_for_update($custody['employee_number']) : false;
    if (!$employee || (string)$employee['employee_id'] === '' || empty($employee['hire_date'])) return false;
    $overlap = hrm_lifecycle_rehire_protected_payroll_overlap_for_update($employee['employee_id'], $custody['effective_date']);
    if ($overlap === false || count($overlap) !== 0) return false;

    return array(
        'plan_version'=>HRM_LIFECYCLE_REHIRE_ROLLOVER_PLAN_VERSION,
        'request_hash'=>$custody['request_hash'],
        'employee_number'=>(int)$custody['employee_number'],
        'employee_id'=>(string)$employee['employee_id'],
        'person_id'=>(int)$custody['person_id'],
        'worker_id'=>(int)$custody['worker_id'],
        'rehire_effective_date'=>(string)$custody['effective_date'],
        'first_hire_date'=>(string)$employee['hire_date'],
        'preserve_first_hire_date'=>true,
        'predecessor_employment_id'=>(int)$custody['predecessor_employment_id'],
        'predecessor_contract_id'=>(int)$custody['predecessor_contract_id'],
        'predecessor_assignment_id'=>(int)$custody['predecessor_assignment_id'],
        'predecessor_retired_legacy_current'=>null,
        'successor_employment_sequence'=>(int)$sequence['successor_employment_sequence'],
        'successor_contract_sequence'=>1,
        'successor_assignment_sequence'=>1,
        'successor_legacy_current'=>1,
        'successor_employment_status'=>'active',
        'successor_employment_legal_entity_id'=>(int)$sequence['predecessor_employment']['legal_entity_id'],
        'successor_contract_projection'=>$sequence['predecessor_contract'],
        'successor_assignment_projection'=>$sequence['predecessor_assignment'],
        'history_change_type'=>HRM_HIST_REHIRE,
        'legacy_employee_projection_order'=>'last_after_successor_and_evidence',
        'legacy_employee_clear_released_date'=>true,
        'legacy_employee_clear_inactive'=>true,
        'protected_payroll_statuses'=>array(1,2,3,4,5),
        'maker_security_area'=>'SA_EMPLOYEE',
        'checker_distinct_actor_required'=>true,
        'approval_transaction_type_registered'=>false,
        'business_writer_released'=>false,
        'browser_route_released'=>false,
        'csv_route_released'=>false,
        'atomic_single_transaction_required'=>true,
        'partial_repair_allowed'=>false,
        'exact_retry_requires_successor_identity_binding'=>true,
    );
}
