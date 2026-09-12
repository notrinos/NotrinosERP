<?php
/**
 * Fixed same-code Employee Rehire command. Central ApprovalWorkflow owns final
 * execution, rejection and cancellation transactions; no direct writer route.
 */
include_once(dirname(__FILE__).'/lifecycle_separation_command_db.inc');
include_once(dirname(__FILE__).'/employee_contract_db.inc');
include_once(dirname(__FILE__).'/lifecycle_rehire_rollover_plan_db.inc');
include_once(dirname(__FILE__).'/lifecycle_rehire_evidence_db.inc');

/** Require an active, unconditional, non-self-approving central workflow. */
function hrm_lifecycle_rehire_workflow_is_maker_checker()
{
    if (!defined('ST_HRM_LIFECYCLE_REHIRE')
        || !ApprovalWorkflow::isRegistered(ST_HRM_LIFECYCLE_REHIRE))
        return false;
    $workflow = get_approval_workflow_by_type(ST_HRM_LIFECYCLE_REHIRE);
    if (!$workflow || (int)$workflow['is_active'] !== 1 || (int)$workflow['allow_self_approve'] !== 0)
        return false;
    $levels = get_approval_levels((int)$workflow['id'], true);
    if (!$levels) return false;
    $count = 0;
    while ($level = db_fetch($levels)) {
        if ((int)$level['is_active'] !== 1 || (int)$level['role_id'] <= 0
            || (int)$level['min_approvers'] < 1 || (float)$level['amount_threshold'] != 0.0
            || trim((string)$level['conditions']) !== '')
            return false;
        $count++;
    }
    return $count > 0;
}

/** Bind the exact source plan as well as the accepted identity/date hash. */
function hrm_lifecycle_rehire_execution_request_hash($plan)
{
    return is_array($plan) ? hash('sha256', 'employee_rehire.execution.v1|'.serialize($plan)) : false;
}

/** Lock one actor's retry identity before revalidating mutable eligibility. */
function hrm_lifecycle_rehire_retry_for_update($worker_id, $key_hash)
{
    $result = db_query("SELECT lifecycle_command_id FROM ".TB_PREF."hrm_lifecycle_commands "
        ."WHERE worker_id=".(int)$worker_id." AND command_type='employee_rehire' AND idempotency_key_hash="
        .db_escape($key_hash)." LIMIT 1 FOR UPDATE", 'could not lock Rehire retry');
    $row = $result ? db_fetch_assoc($result) : false;
    return $row ? hrm_lifecycle_separation_command_for_update((int)$row['lifecycle_command_id']) : false;
}

/** Emit restricted, numeric lifecycle evidence without employee/private values. */
function hrm_lifecycle_rehire_audit($command, $legal_entity_id, $details)
{
    return append_domain_audit_event(array(
        'company_id'=>(int)user_company(), 'legal_entity_id'=>(int)$legal_entity_id,
        'entity_type'=>'hrm_lifecycle_command',
        'entity_key'=>array('lifecycle_command_id'=>(int)$command['lifecycle_command_id']),
        'action'=>'hrm.lifecycle.employee_rehire.'.$command['command_status'],
        'reason_code'=>'hrm_fnd_005_employee_rehire_execution',
        'before'=>array(), 'after'=>array('command_status'=>$command['command_status'],
            'row_version'=>(int)$command['row_version']),
        'restricted_diff'=>$details)) !== false;
}

/** Verify stored draft data and envelope identity, including terminal retries. */
function hrm_lifecycle_rehire_draft_matches($data, $command)
{
    if (!is_array($data) || !is_array($command)) return false;
    $keys = array_keys($data); sort($keys);
    if ($keys !== array('command_row_version','effective_date','employee_number',
        'lifecycle_command_id','request_hash','worker_id'))
        return false;
    foreach (array('employee_number','lifecycle_command_id','worker_id') as $key)
        if (hrm_lifecycle_rehire_positive_int($data[$key]) === false) return false;
    return (int)$data['command_row_version'] === 1
        && $command['command_type'] === 'employee_rehire'
        && (int)$command['lifecycle_command_id'] === (int)$data['lifecycle_command_id']
        && (int)$command['worker_id'] === (int)$data['worker_id']
        && (string)$command['effective_at'] === (string)$data['effective_date'].' 00:00:00'
        && hrm_lifecycle_rehire_date($data['effective_date']) !== false
        && is_string($data['request_hash'])
        && preg_match('/^[a-f0-9]{64}$/D', $data['request_hash']) === 1
        && hash_equals((string)$command['request_hash'], $data['request_hash']);
}

/** Return a minimal exact retry result after checking persisted approval custody. */
function hrm_lifecycle_rehire_retry_result($command, $employee_number, $date, $actor)
{
    if (!$command || (int)$command['requested_by'] !== (int)$actor
        || (int)$command['approval_draft_id'] <= 0) return false;
    $draft = get_approval_draft_for_update((int)$command['approval_draft_id']);
    $data = $draft ? json_decode(html_entity_decode($draft['draft_data'], ENT_QUOTES, 'UTF-8'), true) : false;
    $states = array('pending'=>APPROVAL_STATUS_PENDING,'completed'=>APPROVAL_STATUS_APPROVED,
        'rejected'=>APPROVAL_STATUS_REJECTED,'cancelled'=>APPROVAL_STATUS_CANCELLED);
    if (!$draft || !hrm_lifecycle_rehire_draft_matches($data, $command)
        || (int)$draft['trans_type'] !== ST_HRM_LIFECYCLE_REHIRE
        || (int)$draft['submitted_by'] !== (int)$actor
        || (int)$data['employee_number'] !== (int)$employee_number || $data['effective_date'] !== $date
        || !isset($states[$command['command_status']])
        || (int)$draft['status'] !== $states[$command['command_status']]
        || (int)$command['row_version'] !== ($command['command_status'] === 'pending' ? 1 : 2))
        return false;
    if ($command['command_status'] === 'completed') {
        $result = db_query("SELECT h.* FROM ".TB_PREF."hrm_employments h INNER JOIN "
            .TB_PREF."hrm_lifecycle_rehire_executions r ON r.successor_employment_id=h.employment_id "
            ."WHERE r.lifecycle_command_id=".(int)$command['lifecycle_command_id']." FOR UPDATE",
            'could not verify completed Rehire retry');
        $employment = $result ? db_fetch_assoc($result) : false;
        if (!$employment || !get_hrm_lifecycle_rehire_employment_evidence($employment, true)) return false;
    }
    return array('lifecycle_command_id'=>(int)$command['lifecycle_command_id'],
        'approval_draft_id'=>(int)$command['approval_draft_id'],
        'status'=>$command['command_status'],'exact_retry'=>true);
}

/** Recover only the signed-in maker's latest persisted request for this Employee. */
function get_hrm_lifecycle_rehire_maker_status($employee_number)
{
    if (!hrm_lifecycle_rehire_permission() || (int)$employee_number <= 0) return false;
    begin_transaction();
    $actor = hrm_lifecycle_separation_actor_id();
    $binding = get_hrm_person_worker_binding((int)$employee_number, true);
    $result = false;
    if ($binding && empty($binding['retired_at'])) {
        $rows = db_query("SELECT lifecycle_command_id FROM ".TB_PREF."hrm_lifecycle_commands WHERE worker_id="
            .(int)$binding['worker_id']." AND requested_by=".$actor." AND command_type='employee_rehire'"
            ." ORDER BY lifecycle_command_id DESC LIMIT 1 FOR UPDATE");
        $row = $rows ? db_fetch_assoc($rows) : false;
        $command = $row ? hrm_lifecycle_separation_command_for_update($row['lifecycle_command_id']) : false;
        if ($command) {
            $date = substr($command['effective_at'], 0, 10);
            $result = hrm_lifecycle_rehire_retry_result($command, $employee_number, $date, $actor);
            if ($result) $result['effective_date'] = $date;
        }
    }
    commit_transaction();
    return $result;
}

/**
 * Submit Rehire with no Employee/normalized mutation. The same Worker serializes
 * all fixed lifecycle submissions. Response-loss retries precede eligibility.
 * @return array|false
 */
function submit_hrm_lifecycle_employee_rehire($employee_number, $effective_date, $idempotency_key)
{
    $employee_number = hrm_lifecycle_rehire_positive_int($employee_number);
    $date = hrm_lifecycle_rehire_date($effective_date);
    $key_hash = hrm_lifecycle_separation_idempotency_hash($idempotency_key);
    $actor = hrm_lifecycle_separation_actor_id();
    if (!$employee_number || !$date || !$key_hash || $actor <= 0
        || $date > hrm_lifecycle_separation_today_sql() || !hrm_lifecycle_rehire_permission()
        || !hrm_lifecycle_rehire_workflow_is_maker_checker()) return false;
    begin_transaction();
    $return = false;
    do {
        $binding = hrm_employment_lock_binding_for_interval_policy($employee_number);
        if (!$binding || !empty($binding['retired_at'])) break;
        $existing = hrm_lifecycle_rehire_retry_for_update($binding['worker_id'], $key_hash);
        if ($existing) {
            $return = hrm_lifecycle_rehire_retry_result($existing, $employee_number, $date, $actor);
            break;
        }
        $pending = db_query("SELECT lifecycle_command_id FROM ".TB_PREF."hrm_lifecycle_commands "
            ."WHERE worker_id=".(int)$binding['worker_id']." AND command_status='pending' LIMIT 1 FOR UPDATE",
            'could not inspect Rehire pending conflict');
        if (!$pending || db_fetch_assoc($pending)) break;
        $plan = get_hrm_lifecycle_employee_rehire_rollover_plan_for_update($employee_number, $date);
        if (!$plan || !hrm_employment_person_interval_policy_allows($binding, 0, $date, null)) break;
        $hash = hrm_lifecycle_rehire_execution_request_hash($plan);
        if (!db_query("INSERT INTO ".TB_PREF."hrm_lifecycle_commands "
            ."(worker_id,employment_id,assignment_id,command_type,command_status,effective_at,"
            ."idempotency_key_hash,request_hash,row_version,requested_by) VALUES ("
            .(int)$plan['worker_id'].','.(int)$plan['predecessor_employment_id'].','
            .(int)$plan['predecessor_assignment_id'].",'employee_rehire','pending',"
            .db_escape($date.' 00:00:00').','.db_escape($key_hash).','.db_escape($hash).",1,".$actor.")",
            'could not create Rehire command')) break;
        $id = (int)db_insert_id();
        if ($id <= 0) break;
        $data = array('command_row_version'=>1,'effective_date'=>$date,'employee_number'=>$employee_number,
            'lifecycle_command_id'=>$id,'request_hash'=>$hash,'worker_id'=>(int)$plan['worker_id']);
        $workflow = get_approval_workflow_service();
        $submitted = $workflow->submit(ST_HRM_LIFECYCLE_REHIRE, $data, 0,
            array('summary'=>sprintf(_('Employee Rehire lifecycle command #%d'), $id)));
        if (!is_array($submitted) || $submitted['status'] !== 'pending' || (int)$submitted['draft_id'] <= 0) break;
        $draft_id = (int)$submitted['draft_id'];
        if (!db_query("UPDATE ".TB_PREF."hrm_lifecycle_commands SET approval_draft_id=".$draft_id
            ." WHERE lifecycle_command_id=".$id." AND approval_draft_id IS NULL AND row_version=1",
            'could not bind Rehire approval') || db_num_affected_rows() !== 1) break;
        $command = array('lifecycle_command_id'=>$id,'command_status'=>'pending','row_version'=>1);
        if (!hrm_lifecycle_rehire_audit($command, $plan['successor_employment_legal_entity_id'],
            array('approval_draft_id'=>$draft_id,'request_hash'=>$hash,'business_rows_changed'=>0))) break;
        $return = array('lifecycle_command_id'=>$id,'approval_draft_id'=>$draft_id,
            'status'=>'pending','exact_retry'=>false);
    } while (false);
    if ($return !== false) commit_transaction(); else cancel_transaction();
    return $return;
}

/**
 * Require the central workflow's persisted terminal decision and current actor
 * action inside its open transaction. A direct callback cannot self-authorize.
 * @return array|false
 */
function hrm_lifecycle_rehire_terminal_custody($data, $draft_id, $status)
{
    global $transaction_level;
    if (empty($transaction_level) || !hrm_lifecycle_rehire_permission() || !is_array($data)
        || empty($data['lifecycle_command_id']) || (int)$draft_id <= 0) return false;
    $actor = hrm_lifecycle_separation_actor_id();
    $draft = get_approval_draft_for_update((int)$draft_id);
    $command = hrm_lifecycle_separation_command_for_update($data['lifecycle_command_id']);
    $states = array('completed'=>APPROVAL_STATUS_APPROVED,'rejected'=>APPROVAL_STATUS_REJECTED,
        'cancelled'=>APPROVAL_STATUS_CANCELLED);
    if (!$draft || !$command || !isset($states[$status]) || $actor <= 0
        || !hrm_lifecycle_rehire_draft_matches($data, $command)
        || (int)$command['approval_draft_id'] !== (int)$draft_id
        || (int)$draft['trans_type'] !== ST_HRM_LIFECYCLE_REHIRE
        || (int)$draft['submitted_by'] !== (int)$command['requested_by']
        || (int)$draft['status'] !== $states[$status]
        || ($status === 'cancelled' ? $actor !== (int)$command['requested_by'] : $actor === (int)$command['requested_by']))
        return false;
    $stored = json_decode(html_entity_decode($draft['draft_data'], ENT_QUOTES, 'UTF-8'), true);
    if ($stored !== $data) return false;
    $action = $status === 'completed' ? 'approve' : ($status === 'rejected' ? 'reject' : 'cancel');
    $result = db_query("SELECT 1 FROM ".TB_PREF."approval_actions WHERE draft_id=".(int)$draft_id
        ." AND user_id=".$actor." AND action_type=".db_escape($action)." LIMIT 1 FOR UPDATE",
        'could not verify Rehire central decision action');
    if (!$result || !db_fetch_assoc($result)) return false;
    return $command;
}

/** Compare-and-set a pending command's terminal status in the owning transaction. */
function hrm_lifecycle_rehire_complete_command($command, $status)
{
    return db_query("UPDATE ".TB_PREF."hrm_lifecycle_commands SET command_status=".db_escape($status)
        .",row_version=2,completed_at=UTC_TIMESTAMP(),completed_by=".hrm_lifecycle_separation_actor_id()
        ." WHERE lifecycle_command_id=".(int)$command['lifecycle_command_id']
        ." AND command_status='pending' AND row_version=1 AND approval_draft_id=".(int)$command['approval_draft_id'],
        'could not terminalize Rehire command') && db_num_affected_rows() === 1;
}

/** Reject or cancel through central workflow with no business mutation. */
function hrm_lifecycle_rehire_terminalize($data, $draft_id, $status)
{
    $command = hrm_lifecycle_rehire_terminal_custody($data, $draft_id, $status);
    if (!$command || $command['command_status'] !== 'pending' || (int)$command['row_version'] !== 1
        || !hrm_lifecycle_rehire_complete_command($command, $status)) return false;
    $employment = get_hrm_legacy_current_employment($data['employee_number'], true);
    if (!$employment) return false;
    $command['command_status'] = $status; $command['row_version'] = 2;
    return hrm_lifecycle_rehire_audit($command, $employment['legal_entity_id'],
        array('business_rows_changed'=>0)) ? (int)$command['lifecycle_command_id'] : false;
}

/** Central rejection callback. */
function reject_hrm_lifecycle_rehire_draft($data, $draft_id, $comments)
{
    return hrm_lifecycle_rehire_terminalize($data, $draft_id, 'rejected');
}

/** Central original-maker cancellation callback. */
function cancel_hrm_lifecycle_rehire_draft($data, $draft_id, $comments)
{
    return hrm_lifecycle_rehire_terminalize($data, $draft_id, 'cancelled');
}

/**
 * Execute only the centrally approved, distinct-checker Rehire decision.
 * Returning false makes ApprovalWorkflow roll back its decision and every write.
 * @return int|false
 */
function execute_approved_hrm_lifecycle_rehire($data, $reserved_trans_no, $draft_id)
{
    $command = hrm_lifecycle_rehire_terminal_custody($data, $draft_id, 'completed');
    if (!$command) return false;
    if ($command['command_status'] === 'completed') {
        if ((int)$command['completed_by'] !== hrm_lifecycle_separation_actor_id()) return false;
        $retry = hrm_lifecycle_rehire_retry_result($command, $data['employee_number'],
            $data['effective_date'], $command['requested_by']);
        return $retry ? (int)$command['lifecycle_command_id'] : false;
    }
    if ($command['command_status'] !== 'pending' || (int)$command['row_version'] !== 1
        || $data['effective_date'] > hrm_lifecycle_separation_today_sql()) return false;
    $number = (int)$data['employee_number'];
    $binding = hrm_employment_lock_binding_for_interval_policy($number);
    $plan = $binding ? get_hrm_lifecycle_employee_rehire_rollover_plan_for_update($number, $data['effective_date']) : false;
    if (!$plan || !hash_equals($command['request_hash'], hrm_lifecycle_rehire_execution_request_hash($plan))
        || !hrm_employment_person_interval_policy_allows($binding, 0, $data['effective_date'], null))
        return false;
    $employee = hrm_lifecycle_rehire_employee_for_update($number);
    $contract = $plan['successor_contract_projection'];
    $assignment = $plan['successor_assignment_projection'];
    if (!$employee || (int)$employee['employment_type'] !== (int)$contract['legacy_employment_type'])
        return false;
    foreach (array('department_id','position_id','grade_id','shift_id','reporting_to','cost_center_id','dimension2_id') as $field)
        if (!hrm_assignment_nullable_equal($employee[$field], $assignment['legacy_'.$field]))
            return false;
    // Reuse target validators for a new interval. Existing expired authorities
    // are not silently revived merely because they were valid before Separation.
    $target = array('employment_id'=>(int)$plan['predecessor_employment_id'],
        'legal_entity_id'=>(int)$plan['successor_employment_legal_entity_id']);
    $verified = hrm_assignment_bind_organization_unit($assignment, $target, true, false);
    $verified = hrm_assignment_bind_position($verified, $target, true, false);
    $verified = hrm_assignment_bind_grade($verified, $target, true, false);
    $verified = hrm_assignment_bind_job($verified, $target, $assignment['job_id'], true, true);
    $verified = hrm_assignment_bind_work_location($verified, $target, $assignment['work_location_id'], true, true);
    if (!empty($assignment['manager_assignment_id']))
        $verified = hrm_assignment_bind_manager($verified, $target,
            $assignment['legacy_reporting_to'], $data['effective_date'], true);
    if (!$verified) return false;
    foreach (array('organization_unit_id','position_id','grade_id','job_id','work_location_id','manager_assignment_id') as $field)
        if (!hrm_assignment_nullable_int_equal($assignment[$field], $verified[$field])) return false;
    $actor = hrm_lifecycle_separation_actor_id();
    $date = $data['effective_date'];
    foreach (array('hrm_employments'=>array('employment_id','predecessor_employment_id'),
        'hrm_contracts'=>array('contract_id','predecessor_contract_id'),
        'hrm_assignments'=>array('assignment_id','predecessor_assignment_id')) as $table=>$keys) {
        if (!db_query("UPDATE ".TB_PREF.$table." SET legacy_current=NULL WHERE ".$keys[0]."=".(int)$plan[$keys[1]]
            ." AND legacy_employee_number=".$number." AND legacy_current=1",
            'could not retire Rehire predecessor marker') || db_num_affected_rows() !== 1) return false;
    }
    if (!db_query("INSERT INTO ".TB_PREF."hrm_employments (worker_id,employment_sequence,legal_entity_id,"
        ."legacy_employee_number,legacy_current,start_date,end_date,employment_status,interval_quality,source,created_by) VALUES ("
        .(int)$plan['worker_id'].','.(int)$plan['successor_employment_sequence'].','
        .(int)$plan['successor_employment_legal_entity_id'].','.$number.',1,'.db_escape($date)
        .",NULL,'active','exact','runtime_rehire',".$actor.")", 'could not create Rehire Employment')) return false;
    $employment_id = (int)db_insert_id();
    if ($employment_id <= 0) return false;
    if (!db_query("INSERT INTO ".TB_PREF."hrm_contracts (employment_id,contract_sequence,legacy_employee_number,"
        ."legacy_current,legacy_employment_type,contract_type_code,type_quality,effective_from,effective_to,interval_quality,source,created_by)"
        ." VALUES (".$employment_id.",1,".$number.",1,".(int)$contract['legacy_employment_type'].','
        .db_escape($contract['contract_type_code']).','.db_escape($contract['type_quality']).','.db_escape($date)
        .",NULL,'exact','runtime_rehire',".$actor.")", 'could not create Rehire Contract')) return false;
    $contract_id = (int)db_insert_id();
    if ($contract_id <= 0) return false;
    $columns = array('legacy_department_id','organization_unit_id','legacy_position_id','position_id',
        'job_id','work_location_id','manager_assignment_id','legacy_grade_id','grade_id','legacy_shift_id',
        'legacy_reporting_to','legacy_cost_center_id','legacy_dimension2_id','reference_quality');
    $values = array(); foreach ($columns as $column) $values[] = db_escape($assignment[$column]);
    if (!db_query("INSERT INTO ".TB_PREF."hrm_assignments (employment_id,assignment_sequence,legacy_employee_number,"
        ."legacy_current,assignment_type_code,primary_flag,".implode(',', $columns)
        .",effective_from,effective_to,interval_quality,source,created_by) VALUES (".$employment_id.",1,"
        .$number.",1,'legacy_primary',1,".implode(',', $values).','.db_escape($date)
        .",NULL,'exact','runtime_rehire',".$actor.")", 'could not create Rehire Assignment')) return false;
    $assignment_id = (int)db_insert_id();
    if ($assignment_id <= 0 || !hrm_assignment_employment_interval_policy_allows($employment_id)) return false;
    $history_id = add_employee_history($employee['employee_id'], HRM_HIST_REHIRE, $date,
        null, $employee['department_id'], null, $employee['position_id'], null, $employee['grade_id'],
        null, null, 'lifecycle_command:'.(int)$command['lifecycle_command_id']);
    if ($history_id <= 0) return false;
    if (!db_query("INSERT INTO ".TB_PREF."hrm_lifecycle_rehire_executions (lifecycle_command_id,"
        ."predecessor_employment_id,predecessor_contract_id,predecessor_assignment_id,"
        ."successor_employment_id,successor_contract_id,successor_assignment_id,history_id,first_hire_date) VALUES ("
        .(int)$command['lifecycle_command_id'].','.(int)$plan['predecessor_employment_id'].','
        .(int)$plan['predecessor_contract_id'].','.(int)$plan['predecessor_assignment_id'].','
        .$employment_id.','.$contract_id.','.$assignment_id.','.(int)$history_id.','.db_escape($plan['first_hire_date']).")",
        'could not append Rehire successor evidence')
        || !hrm_lifecycle_rehire_complete_command($command, 'completed')) return false;
    $command['command_status'] = 'completed'; $command['row_version'] = 2;
    if (!hrm_lifecycle_rehire_audit($command, $plan['successor_employment_legal_entity_id'],
        array('successor_employment_id'=>$employment_id,'successor_contract_id'=>$contract_id,
            'successor_assignment_id'=>$assignment_id,'history_id'=>$history_id,
            'maker_user_id'=>(int)$command['requested_by'],'checker_user_id'=>$actor,
            'request_hash'=>$command['request_hash'],'original_hire_date_preserved'=>true,
            'historical_payroll_accounting_rows_rewritten'=>0))) return false;
    // These exact compatibility state fields are last; no bank, compensation,
    // profile, external account or historical Worker/Person identity is changed.
    if (!db_query("UPDATE ".TB_PREF."hrm_workers SET worker_status='active',active_to=NULL"
        ." WHERE worker_id=".(int)$plan['worker_id']." AND person_id=".(int)$plan['person_id']
        ." AND worker_status='inactive' AND active_to=".db_escape($employee['released_date']),
        'could not reactivate Rehire Worker projection') || db_num_affected_rows() !== 1) return false;
    if (!db_query("UPDATE ".TB_PREF."employees SET inactive=0,released_date=NULL"
        ." WHERE employee_number=".$number." AND employee_id=".db_escape($employee['employee_id'])
        ." AND inactive=1 AND hire_date=".db_escape($plan['first_hire_date'])
        ." AND released_date=".db_escape($employee['released_date']),
        'could not reactivate Rehire Employee projection') || db_num_affected_rows() !== 1) return false;
    return (int)$command['lifecycle_command_id'];
}
