<?php
/** Fixed advisory clearance acknowledgements under completed Separation custody. */
include_once(dirname(__FILE__).'/lifecycle_separation_command_db.inc');

/** Finite task policy; none of these acknowledgements executes an external action. */
function hrm_lifecycle_offboarding_task_labels()
{
    return array(
        'offboard_asset_review'=>_('Asset return reviewed'),
        'offboard_access_review'=>_('Access removal reviewed'),
        'offboard_payroll_review'=>_('Final payroll review recorded'),
        'offboard_document_review'=>_('Document retention reviewed'),
    );
}

/**
 * Acknowledge one advisory task, scoped to the Separation maker and Employee.
 * Acknowledgement records review only, not clearance completion or settlement.
 * @return int|false
 */
function acknowledge_hrm_lifecycle_offboarding_task($employee_id, $task_code)
{
    $labels = hrm_lifecycle_offboarding_task_labels();
    $actor = hrm_lifecycle_separation_actor_id();
    if (!hrm_lifecycle_separation_permission() || $actor <= 0
        || !is_string($task_code) || !isset($labels[$task_code])) return false;
    begin_transaction();
    $id = false;
    do {
        $binding = get_hrm_person_worker_binding($employee_id, true);
        if (!$binding || !empty($binding['retired_at'])) break;
        $r = db_query("SELECT lifecycle_command_id FROM ".TB_PREF."hrm_lifecycle_commands WHERE worker_id="
            .(int)$binding['worker_id']." AND command_type='employee_separation' AND command_status='completed'"
            ." ORDER BY lifecycle_command_id DESC LIMIT 1 FOR UPDATE", 'could not lock offboarding Separation');
        $row = $r ? db_fetch_assoc($r) : false;
        $command = $row ? hrm_lifecycle_separation_command_for_update($row['lifecycle_command_id']) : false;
        if (!$command || (int)$command['requested_by'] !== $actor || (int)$command['completed_by'] === $actor
            || (int)$command['completed_by'] <= 0 || (int)$command['approval_draft_id'] <= 0) break;
        $draft = get_approval_draft_for_update((int)$command['approval_draft_id']);
        $data = $draft ? json_decode(html_entity_decode($draft['draft_data'], ENT_QUOTES, 'UTF-8'), true) : false;
        if (!$draft || (int)$draft['trans_type'] !== ST_HRM_LIFECYCLE_SEPARATION
            || (int)$draft['status'] !== APPROVAL_STATUS_APPROVED || (int)$draft['submitted_by'] !== $actor
            || !hrm_lifecycle_separation_draft_matches($data, $command, true)
            || (int)$data['employee_number'] !== (int)$binding['employee_number']) break;
        $r = db_query("SELECT lifecycle_task_id,task_status,created_by,completed_by,retention_locked,"
            ."row_version,confidentiality_class FROM ".TB_PREF."hrm_lifecycle_tasks WHERE lifecycle_command_id="
            .(int)$command['lifecycle_command_id']." AND task_code=".db_escape($task_code)." FOR UPDATE",
            'could not verify offboarding acknowledgement');
        if (!$r) break;
        $task = db_fetch_assoc($r);
        if ($task) {
            if ($task['task_status'] === 'completed' && (int)$task['created_by'] === $actor
                && (int)$task['completed_by'] === $actor && (int)$task['row_version'] === 2
                && (int)$task['retention_locked'] === 1 && $task['confidentiality_class'] === 'restricted')
                $id = (int)$task['lifecycle_task_id'];
            break;
        }
        if (!db_query("INSERT INTO ".TB_PREF."hrm_lifecycle_tasks "
            ."(lifecycle_command_id,task_code,task_status,confidentiality_class,retention_locked,row_version,"
            ."created_by,completed_at,completed_by) VALUES (".(int)$command['lifecycle_command_id'].','
            .db_escape($task_code).",'completed','restricted',1,2,".$actor.",UTC_TIMESTAMP(),".$actor.")",
            'could not record offboarding acknowledgement')) break;
        $candidate = (int)db_insert_id();
        if ($candidate <= 0 || append_domain_audit_event(array(
            'company_id'=>(int)user_company(),'legal_entity_id'=>0,
            'entity_type'=>'hrm_lifecycle_task',
            'entity_key'=>array('lifecycle_task_id'=>$candidate,'lifecycle_command_id'=>(int)$command['lifecycle_command_id']),
            'action'=>'hrm.lifecycle.offboarding.acknowledged','reason_code'=>'hrm_fnd_005_advisory_offboarding',
            'before'=>array(),'after'=>array('task_code'=>$task_code,'task_status'=>'completed'),
            'restricted_diff'=>array('advisory_only'=>true,'external_actions_executed'=>0,
                'payroll_payment_access_rows_changed'=>0,'retention_locked'=>true))) === false) break;
        $id = $candidate;
    } while (false);
    if ($id !== false) commit_transaction(); else cancel_transaction();
    return $id;
}
