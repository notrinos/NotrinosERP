<?php
/**
 * HRM-FND-005 Employee Transfer checklist-task browser recovery/visibility.
 *
 * This helper is read-only unless an explicit route POST calls one of the two
 * bounded action wrappers below. The browser never accepts a lifecycle command
 * or task id from request data: it resolves only the current actor's completed
 * Employee Transfer command from accepted browser recovery state, then invokes
 * the fixed task writer with that server-resolved command id.
 */
$hrm_lifecycle_transfer_task_browser_root = isset($path_to_root)
    ? $path_to_root : dirname(dirname(dirname(__DIR__)));
$hrm_lifecycle_transfer_task_browser_dependencies = array(
    '/hrm/includes/db/lifecycle_transfer_browser_db.inc',
    '/hrm/includes/db/lifecycle_transfer_task_db.inc',
);
foreach ($hrm_lifecycle_transfer_task_browser_dependencies as $dependency) {
    $candidate = $hrm_lifecycle_transfer_task_browser_root.$dependency;
    if (is_file($candidate))
        include_once($candidate);
}
unset($hrm_lifecycle_transfer_task_browser_root, $hrm_lifecycle_transfer_task_browser_dependencies, $dependency, $candidate);

/**
 * Resolve restricted task visibility from the current actor's accepted
 * Employee Transfer browser state. No task/document/approval mutation occurs.
 *
 * status values:
 *   unavailable   no own completed Transfer command is actionable
 *   not_created   own completed command exists and no task exists yet
 *   pending       fixed task exists and is pending
 *   completed     fixed task exists and is completed
 *   inconsistent  custody or actor binding failed closed
 */
function get_hrm_lifecycle_employee_transfer_task_browser_status($employee_id)
{
    $base = array('status'=>'unavailable','own'=>false,'lifecycle_command_id'=>0,'task_status'=>'');
    if (!function_exists('get_hrm_lifecycle_employee_transfer_browser_status')
        || !function_exists('hrm_lifecycle_transfer_task_permission')
        || !hrm_lifecycle_transfer_task_permission())
        return array_merge($base, array('status'=>'inconsistent'));

    $command = get_hrm_lifecycle_employee_transfer_browser_status($employee_id);
    if (!is_array($command) || !isset($command['status']))
        return array_merge($base, array('status'=>'inconsistent'));
    if ((string)$command['status'] === 'inconsistent')
        return array_merge($base, array('status'=>'inconsistent'));
    if ((string)$command['status'] !== 'completed' || empty($command['own'])
        || (int)$command['lifecycle_command_id'] <= 0)
        return $base;

    $actor = function_exists('hrm_lifecycle_transfer_task_actor_id')
        ? hrm_lifecycle_transfer_task_actor_id() : 0;
    if ($actor <= 0)
        return array_merge($base, array('status'=>'inconsistent'));

    $command_id = (int)$command['lifecycle_command_id'];
    $result = db_query("SELECT lifecycle_task_id,lifecycle_command_id,task_code,task_status,"
        ."confidentiality_class,retention_locked,row_version,created_at,created_by,completed_at,completed_by "
        ."FROM ".TB_PREF."hrm_lifecycle_tasks WHERE lifecycle_command_id=".$command_id
        ." AND task_code=".db_escape(HRM_LIFECYCLE_TRANSFER_TASK_CODE)." LIMIT 1",
        'could not read Employee Transfer lifecycle task browser custody');
    if (!$result)
        return array_merge($base, array('status'=>'inconsistent'));
    $task = db_fetch_assoc($result);
    if (!$task)
        return array('status'=>'not_created','own'=>true,'lifecycle_command_id'=>$command_id,'task_status'=>'');
    if (!hrm_lifecycle_transfer_task_contract_matches($task, $command_id)
        || (int)$task['created_by'] !== $actor)
        return array_merge($base, array('status'=>'inconsistent'));

    return array(
        'status'=>(string)$task['task_status'],
        'own'=>true,
        'lifecycle_command_id'=>$command_id,
        'task_status'=>(string)$task['task_status'],
    );
}

/** Explicit POST wrapper: server resolves command id; client supplies employee only. */
function create_hrm_lifecycle_employee_transfer_task_from_browser($employee_id)
{
    $status = get_hrm_lifecycle_employee_transfer_task_browser_status($employee_id);
    if (!is_array($status) || (string)$status['status'] !== 'not_created'
        || empty($status['own']) || (int)$status['lifecycle_command_id'] <= 0)
        return false;
    return create_hrm_lifecycle_employee_transfer_task((int)$status['lifecycle_command_id']);
}

/** Explicit POST wrapper: only pending -> completed is exposed to the browser. */
function complete_hrm_lifecycle_employee_transfer_task_from_browser($employee_id)
{
    $status = get_hrm_lifecycle_employee_transfer_task_browser_status($employee_id);
    if (!is_array($status) || (string)$status['status'] !== 'pending'
        || empty($status['own']) || (int)$status['lifecycle_command_id'] <= 0)
        return false;
    return complete_hrm_lifecycle_employee_transfer_task((int)$status['lifecycle_command_id']);
}
