<?php
/**
 * HRM-FND-005 Employee Training Assignment checklist-task browser recovery.
 *
 * GET/status is read-only. The browser never accepts lifecycle command/task ids;
 * the current actor's latest accepted completed Training Assignment command is
 * resolved server-side and only coarse task state is returned.
 */
$hrm_lifecycle_training_assignment_task_browser_root = isset($path_to_root)
    ? $path_to_root : dirname(dirname(dirname(__DIR__)));
$hrm_lifecycle_training_assignment_task_browser_dependencies = array(
    '/hrm/includes/db/lifecycle_training_assignment_browser_db.inc',
    '/hrm/includes/db/lifecycle_training_assignment_task_db.inc',
);
foreach ($hrm_lifecycle_training_assignment_task_browser_dependencies as $dependency) {
    $candidate = $hrm_lifecycle_training_assignment_task_browser_root.$dependency;
    if (is_file($candidate))
        include_once($candidate);
}
unset($hrm_lifecycle_training_assignment_task_browser_root, $hrm_lifecycle_training_assignment_task_browser_dependencies, $dependency, $candidate);

/** Internal server-only context; never render its lifecycle_command_id. */
function hrm_lifecycle_training_assignment_task_browser_context($employee_id)
{
    $base = array('status'=>'unavailable','own'=>false,'lifecycle_command_id'=>0);
    if (!function_exists('hrm_lifecycle_training_assignment_browser_employee_id')
        || !function_exists('hrm_lifecycle_training_assignment_task_permission')
        || !hrm_lifecycle_training_assignment_task_permission()
        || !function_exists('get_hrm_person_worker_binding'))
        return array_merge($base, array('status'=>'inconsistent'));

    $employee_id = hrm_lifecycle_training_assignment_browser_employee_id($employee_id);
    $actor = function_exists('hrm_lifecycle_training_assignment_task_actor_id')
        ? hrm_lifecycle_training_assignment_task_actor_id() : 0;
    if ($employee_id === false || $actor <= 0)
        return array_merge($base, array('status'=>'inconsistent'));

    $binding = get_hrm_person_worker_binding($employee_id, false);
    if (!$binding || !empty($binding['retired_at']) || (int)$binding['worker_id'] <= 0)
        return $base;

    $pending = get_hrm_lifecycle_employee_training_assignment_browser_status($employee_id);
    if (!is_array($pending) || !isset($pending['status']) || (string)$pending['status'] === 'inconsistent')
        return array_merge($base, array('status'=>'inconsistent'));
    if ((string)$pending['status'] === 'pending' || (string)$pending['status'] === 'blocked')
        return $base;

    $result = db_query("SELECT c.lifecycle_command_id,c.command_status,c.row_version,c.requested_by,c.completed_by,"
        ."c.completed_at,c.approval_draft_id,d.id approval_row_id,d.trans_type,d.status approval_status,d.submitted_by "
        ."FROM ".TB_PREF."hrm_lifecycle_commands c LEFT JOIN ".TB_PREF."approval_drafts d ON d.id=c.approval_draft_id "
        ."WHERE c.worker_id=".(int)$binding['worker_id']." AND c.command_type="
        .db_escape(HRM_LIFECYCLE_TRAINING_ASSIGNMENT_COMMAND_TYPE)." AND c.requested_by=".$actor
        ." AND c.command_status='completed' ORDER BY c.lifecycle_command_id DESC LIMIT 1",
        'could not resolve Employee Training Assignment task browser command custody');
    if (!$result)
        return array_merge($base, array('status'=>'inconsistent'));
    $command = db_fetch_assoc($result);
    if (!$command)
        return $base;
    if ((int)$command['lifecycle_command_id'] <= 0 || (int)$command['row_version'] !== 2
        || (int)$command['requested_by'] !== $actor || (int)$command['completed_by'] <= 0
        || (int)$command['completed_by'] === (int)$command['requested_by']
        || trim((string)$command['completed_at']) === '' || (int)$command['approval_draft_id'] <= 0
        || (int)$command['approval_row_id'] !== (int)$command['approval_draft_id']
        || !defined('ST_HRM_LIFECYCLE_TRAINING_ASSIGNMENT')
        || !defined('APPROVAL_STATUS_APPROVED')
        || (int)$command['trans_type'] !== (int)ST_HRM_LIFECYCLE_TRAINING_ASSIGNMENT
        || (int)$command['approval_status'] !== (int)APPROVAL_STATUS_APPROVED
        || (int)$command['submitted_by'] !== $actor)
        return array_merge($base, array('status'=>'inconsistent'));

    $command_id = (int)$command['lifecycle_command_id'];
    $task_result = db_query("SELECT lifecycle_task_id,lifecycle_command_id,task_code,task_status,"
        ."confidentiality_class,retention_locked,row_version,created_at,created_by,completed_at,completed_by "
        ."FROM ".TB_PREF."hrm_lifecycle_tasks WHERE lifecycle_command_id=".$command_id
        ." AND task_code=".db_escape(HRM_LIFECYCLE_TRAINING_ASSIGNMENT_TASK_CODE)." LIMIT 1",
        'could not read Employee Training Assignment task browser custody');
    if (!$task_result)
        return array_merge($base, array('status'=>'inconsistent'));
    $task = db_fetch_assoc($task_result);
    if (!$task)
        return array('status'=>'not_created','own'=>true,'lifecycle_command_id'=>$command_id);
    if (!hrm_lifecycle_training_assignment_task_contract_matches($task, $command_id)
        || (int)$task['created_by'] !== $actor
        || ((string)$task['task_status'] === 'completed' && (int)$task['completed_by'] !== $actor))
        return array_merge($base, array('status'=>'inconsistent'));
    return array('status'=>(string)$task['task_status'],'own'=>true,'lifecycle_command_id'=>$command_id);
}

/** Public read-only coarse status; no command/task identifier is returned. */
function get_hrm_lifecycle_employee_training_assignment_task_browser_status($employee_id)
{
    $context = hrm_lifecycle_training_assignment_task_browser_context($employee_id);
    return array(
        'status'=>is_array($context) && isset($context['status']) ? (string)$context['status'] : 'inconsistent',
        'own'=>is_array($context) && !empty($context['own']),
    );
}

/** Reconcile repeated create POSTs through the actor-bound idempotent writer. */
function create_hrm_lifecycle_employee_training_assignment_task_from_browser($employee_id)
{
    $context = hrm_lifecycle_training_assignment_task_browser_context($employee_id);
    if (!is_array($context)
        || !in_array((string)$context['status'], array('not_created','pending','completed'), true)
        || empty($context['own']) || (int)$context['lifecycle_command_id'] <= 0)
        return false;
    return create_hrm_lifecycle_employee_training_assignment_task((int)$context['lifecycle_command_id']);
}

/** Reconcile a lost completion response without repeating its task/audit mutation. */
function complete_hrm_lifecycle_employee_training_assignment_task_from_browser($employee_id)
{
    $context = hrm_lifecycle_training_assignment_task_browser_context($employee_id);
    if (!is_array($context)
        || !in_array((string)$context['status'], array('pending','completed'), true)
        || empty($context['own']) || (int)$context['lifecycle_command_id'] <= 0)
        return false;
    return complete_hrm_lifecycle_employee_training_assignment_task((int)$context['lifecycle_command_id']);
}
