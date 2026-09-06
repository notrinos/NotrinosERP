<?php
/**
 * HRM-FND-005 first bounded lifecycle checklist-task writer.
 *
 * The task is intentionally a post-approval Employee Transfer command
 * acknowledgement. It is not a generic checklist dispatcher, does not gate or
 * reopen central approval, and does not mutate Employee/Assignment/document,
 * payroll, payment, bank, GL, or accounting state.
 */
$hrm_lifecycle_transfer_task_root = isset($path_to_root)
    ? $path_to_root : dirname(dirname(dirname(__DIR__)));
$hrm_lifecycle_transfer_task_dependencies = array(
    '/includes/types.inc',
    '/includes/approval/db/approval_db.inc',
    '/includes/db/domain_audit_db.inc',
    '/hrm/includes/db/lifecycle_transfer_command_db.inc',
);
foreach ($hrm_lifecycle_transfer_task_dependencies as $dependency) {
    $candidate = $hrm_lifecycle_transfer_task_root.$dependency;
    if (is_file($candidate))
        include_once($candidate);
}
unset($hrm_lifecycle_transfer_task_root, $hrm_lifecycle_transfer_task_dependencies, $dependency, $candidate);

if (!defined('HRM_LIFECYCLE_TRANSFER_TASK_CODE'))
    define('HRM_LIFECYCLE_TRANSFER_TASK_CODE', 'transfer_command_ack');
if (!defined('HRM_LIFECYCLE_TRANSFER_TASK_REASON'))
    define('HRM_LIFECYCLE_TRANSFER_TASK_REASON', 'hrm_fnd_005_employee_transfer_task_writer');

function hrm_lifecycle_transfer_task_actor_id()
{
    return function_exists('hrm_lifecycle_transfer_actor_id')
        ? hrm_lifecycle_transfer_actor_id()
        : (isset($_SESSION['wa_current_user']) && is_object($_SESSION['wa_current_user'])
            && isset($_SESSION['wa_current_user']->user) ? (int)$_SESSION['wa_current_user']->user : 0);
}

function hrm_lifecycle_transfer_task_permission()
{
    return function_exists('user_check_access') && user_check_access('SA_EMPTRANSFER');
}

function hrm_lifecycle_transfer_task_positive_int($value)
{
    if (is_int($value))
        return $value > 0 ? $value : false;
    $value = trim((string)$value);
    if (preg_match('/^[1-9][0-9]*$/D', $value) !== 1)
        return false;
    $number = (int)$value;
    return $number > 0 && (string)$number === $value ? $number : false;
}

/**
 * Lock and validate one accepted Employee Transfer lifecycle command plus its
 * command-owned central approval draft. Only a completed row-version 2 command
 * with an approved matching draft can own this task.
 */
function hrm_lifecycle_transfer_task_command_for_update($command_id)
{
    $command_id = hrm_lifecycle_transfer_task_positive_int($command_id);
    if ($command_id === false || !defined('ST_HRM_LIFECYCLE_TRANSFER')
        || !defined('APPROVAL_STATUS_APPROVED'))
        return false;
    $sql = "SELECT c.lifecycle_command_id,c.worker_id,c.employment_id,c.assignment_id,c.command_type,"
        ."c.command_status,c.approval_draft_id,c.row_version,c.requested_by,c.completed_at,c.completed_by,"
        ."d.id approval_row_id,d.trans_type,d.status approval_status,d.submitted_by "
        ."FROM ".TB_PREF."hrm_lifecycle_commands c LEFT JOIN ".TB_PREF."approval_drafts d "
        ."ON d.id=c.approval_draft_id WHERE c.lifecycle_command_id=".$command_id." LIMIT 1 FOR UPDATE";
    $result = db_query($sql, 'could not lock Employee Transfer lifecycle task command');
    $row = $result ? db_fetch_assoc($result) : false;
    if (!$row || (string)$row['command_type'] !== HRM_LIFECYCLE_TRANSFER_COMMAND_TYPE
        || (string)$row['command_status'] !== 'completed' || (int)$row['row_version'] !== 2
        || (int)$row['worker_id'] <= 0 || (int)$row['requested_by'] <= 0
        || (int)$row['completed_by'] <= 0 || trim((string)$row['completed_at']) === ''
        || (int)$row['approval_draft_id'] <= 0
        || (int)$row['approval_row_id'] !== (int)$row['approval_draft_id']
        || (int)$row['trans_type'] !== (int)ST_HRM_LIFECYCLE_TRANSFER
        || (int)$row['approval_status'] !== (int)APPROVAL_STATUS_APPROVED
        || (int)$row['submitted_by'] !== (int)$row['requested_by'])
        return false;
    return $row;
}

function hrm_lifecycle_transfer_task_row_for_update($command_id)
{
    $command_id = hrm_lifecycle_transfer_task_positive_int($command_id);
    if ($command_id === false)
        return false;
    $result = db_query("SELECT lifecycle_task_id,lifecycle_command_id,task_code,task_status,"
        ."confidentiality_class,retention_locked,row_version,created_at,created_by,completed_at,completed_by "
        ."FROM ".TB_PREF."hrm_lifecycle_tasks WHERE lifecycle_command_id=".$command_id
        ." AND task_code=".db_escape(HRM_LIFECYCLE_TRANSFER_TASK_CODE)." LIMIT 1 FOR UPDATE",
        'could not lock Employee Transfer lifecycle task');
    return $result ? db_fetch_assoc($result) : false;
}

function get_hrm_lifecycle_employee_transfer_task($command_id)
{
    $command_id = hrm_lifecycle_transfer_task_positive_int($command_id);
    if ($command_id === false || !hrm_lifecycle_transfer_task_permission())
        return false;
    $result = db_query("SELECT lifecycle_task_id,lifecycle_command_id,task_code,task_status,"
        ."confidentiality_class,retention_locked,row_version,created_at,created_by,completed_at,completed_by "
        ."FROM ".TB_PREF."hrm_lifecycle_tasks WHERE lifecycle_command_id=".$command_id
        ." AND task_code=".db_escape(HRM_LIFECYCLE_TRANSFER_TASK_CODE)." LIMIT 1",
        'could not read Employee Transfer lifecycle task');
    $row = $result ? db_fetch_assoc($result) : false;
    return $row && hrm_lifecycle_transfer_task_contract_matches($row, $command_id) ? $row : false;
}

function hrm_lifecycle_transfer_task_contract_matches($task, $command_id)
{
    return is_array($task)
        && (int)$task['lifecycle_task_id'] > 0
        && (int)$task['lifecycle_command_id'] === (int)$command_id
        && (string)$task['task_code'] === HRM_LIFECYCLE_TRANSFER_TASK_CODE
        && in_array((string)$task['task_status'], array('pending','completed'), true)
        && (string)$task['confidentiality_class'] === 'restricted'
        && (int)$task['retention_locked'] === 1
        && (((string)$task['task_status'] === 'pending'
                && (int)$task['row_version'] === 1
                && (int)$task['created_by'] > 0
                && empty($task['completed_at'])
                && empty($task['completed_by']))
            || ((string)$task['task_status'] === 'completed'
                && (int)$task['row_version'] === 2
                && (int)$task['created_by'] > 0
                && trim((string)$task['completed_at']) !== ''
                && (int)$task['completed_by'] > 0));
}

function hrm_lifecycle_transfer_task_audit($action, $task, $command)
{
    if (!function_exists('append_domain_audit_event') || !is_array($task) || !is_array($command))
        return false;
    $event = append_domain_audit_event(array(
        'company_id'=>function_exists('user_company') ? (int)user_company() : 0,
        'legal_entity_id'=>0,
        'entity_type'=>'hrm_lifecycle_task',
        'entity_key'=>array(
            'lifecycle_task_id'=>(int)$task['lifecycle_task_id'],
            'lifecycle_command_id'=>(int)$command['lifecycle_command_id'],
            'task_code'=>HRM_LIFECYCLE_TRANSFER_TASK_CODE,
        ),
        'action'=>$action,
        'reason_code'=>HRM_LIFECYCLE_TRANSFER_TASK_REASON,
        'before'=>$action === 'hrm.lifecycle.task.created' ? null : array(
            'task_status'=>'pending','row_version'=>1,'retention_locked'=>1,
        ),
        'after'=>array(
            'task_status'=>(string)$task['task_status'],
            'row_version'=>(int)$task['row_version'],
            'retention_locked'=>1,
        ),
        'restricted_diff'=>array(
            'command_type'=>HRM_LIFECYCLE_TRANSFER_COMMAND_TYPE,
            'task_code'=>HRM_LIFECYCLE_TRANSFER_TASK_CODE,
            'confidentiality_class'=>'restricted',
            'central_approval_remains_command_owned'=>true,
            'document_link_id'=>0,
            'employee_document_rows_mutated'=>0,
            'esignature_connector_invoked'=>false,
            'task_retention_unlock_performed'=>false,
            'employee_rows_mutated'=>0,
            'employment_rows_mutated'=>0,
            'assignment_rows_mutated'=>0,
            'employee_history_rows_added'=>0,
            'payroll_rows_mutated'=>0,
            'bank_transaction_rows_mutated'=>0,
            'gl_rows_mutated'=>0,
        ),
    ));
    return $event !== false;
}

/**
 * Idempotently create the one fixed post-approval Employee Transfer task.
 * Exact retry returns the existing valid task and emits no duplicate audit.
 */
function create_hrm_lifecycle_employee_transfer_task($command_id)
{
    if (!hrm_lifecycle_transfer_task_permission())
        return false;
    $actor = hrm_lifecycle_transfer_task_actor_id();
    $command_id = hrm_lifecycle_transfer_task_positive_int($command_id);
    if ($actor <= 0 || $command_id === false)
        return false;

    begin_transaction();
    $command = hrm_lifecycle_transfer_task_command_for_update($command_id);
    if (!$command) {
        cancel_transaction();
        return false;
    }
    $existing = hrm_lifecycle_transfer_task_row_for_update($command_id);
    if ($existing) {
        $ok = hrm_lifecycle_transfer_task_contract_matches($existing, $command_id);
        if ($ok)
            commit_transaction();
        else
            cancel_transaction();
        return $ok ? (int)$existing['lifecycle_task_id'] : false;
    }

    $sql = "INSERT INTO ".TB_PREF."hrm_lifecycle_tasks "
        ."(lifecycle_command_id,task_code,task_status,confidentiality_class,retention_locked,row_version,created_by) VALUES ("
        .$command_id.",".db_escape(HRM_LIFECYCLE_TRANSFER_TASK_CODE).",'pending','restricted',1,1,".$actor.")";
    if (!db_query($sql, 'could not create Employee Transfer lifecycle task')) {
        cancel_transaction();
        return false;
    }
    $task_id = (int)db_insert_id();
    $task = hrm_lifecycle_transfer_task_row_for_update($command_id);
    if ($task_id <= 0 || !$task || (int)$task['lifecycle_task_id'] !== $task_id
        || !hrm_lifecycle_transfer_task_contract_matches($task, $command_id)
        || !hrm_lifecycle_transfer_task_audit('hrm.lifecycle.task.created', $task, $command)) {
        cancel_transaction();
        return false;
    }
    commit_transaction();
    return $task_id;
}

/**
 * Idempotently complete the fixed Employee Transfer task. Pending -> completed
 * is the only runtime transition; reopen/cancel/delete/unlock do not exist.
 */
function complete_hrm_lifecycle_employee_transfer_task($command_id)
{
    if (!hrm_lifecycle_transfer_task_permission())
        return false;
    $actor = hrm_lifecycle_transfer_task_actor_id();
    $command_id = hrm_lifecycle_transfer_task_positive_int($command_id);
    if ($actor <= 0 || $command_id === false)
        return false;

    begin_transaction();
    $command = hrm_lifecycle_transfer_task_command_for_update($command_id);
    $task = $command ? hrm_lifecycle_transfer_task_row_for_update($command_id) : false;
    if (!$command || !$task || !hrm_lifecycle_transfer_task_contract_matches($task, $command_id)) {
        cancel_transaction();
        return false;
    }
    if ((string)$task['task_status'] === 'completed') {
        commit_transaction();
        return (int)$task['lifecycle_task_id'];
    }
    if ((string)$task['task_status'] !== 'pending' || (int)$task['row_version'] !== 1)
    {
        cancel_transaction();
        return false;
    }
    if (!db_query("UPDATE ".TB_PREF."hrm_lifecycle_tasks SET task_status='completed',row_version=2,"
        ."completed_at=UTC_TIMESTAMP(),completed_by=".$actor." WHERE lifecycle_task_id=".(int)$task['lifecycle_task_id']
        ." AND task_status='pending' AND row_version=1 AND retention_locked=1",
        'could not complete Employee Transfer lifecycle task') || (int)db_num_affected_rows() !== 1) {
        cancel_transaction();
        return false;
    }
    $task = hrm_lifecycle_transfer_task_row_for_update($command_id);
    if (!$task || (string)$task['task_status'] !== 'completed'
        || !hrm_lifecycle_transfer_task_contract_matches($task, $command_id)
        || !hrm_lifecycle_transfer_task_audit('hrm.lifecycle.task.completed', $task, $command)) {
        cancel_transaction();
        return false;
    }
    commit_transaction();
    return (int)$task['lifecycle_task_id'];
}
