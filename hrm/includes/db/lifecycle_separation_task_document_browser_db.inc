<?php
/**
 * HRM-FND-005 Employee Separation task-document browser recovery/visibility.
 *
 * GET/status is read-only and exposes only coarse state. The only browser
 * mutation wrapper accepts an opaque 64-hex employee-document access key; the
 * lifecycle command/task identifiers are resolved server-side from the current
 * actor's accepted completed Separation custody.
 */
$hrm_lifecycle_separation_task_document_browser_root = isset($path_to_root)
    ? $path_to_root : dirname(dirname(dirname(__DIR__)));
$hrm_lifecycle_separation_task_document_browser_dependencies = array(
    '/hrm/includes/db/lifecycle_separation_task_browser_db.inc',
    '/hrm/includes/db/lifecycle_separation_task_document_db.inc',
);
foreach ($hrm_lifecycle_separation_task_document_browser_dependencies as $dependency) {
    $candidate = $hrm_lifecycle_separation_task_document_browser_root.$dependency;
    if (is_file($candidate))
        include_once($candidate);
}
unset($hrm_lifecycle_separation_task_document_browser_root,
    $hrm_lifecycle_separation_task_document_browser_dependencies, $dependency, $candidate);

/**
 * Resolve a server-only action context. No protected document payload is
 * returned and no mutation occurs.
 */
function hrm_lifecycle_separation_task_document_browser_context($employee_id)
{
    $base = array('status'=>'unavailable','lifecycle_command_id'=>0,'lifecycle_task_id'=>0);
    if (!function_exists('hrm_lifecycle_separation_task_document_permission')
        || !hrm_lifecycle_separation_task_document_permission())
        return $base;
    if (!function_exists('get_hrm_lifecycle_employee_separation_task_browser_status')
        || !function_exists('hrm_lifecycle_separation_task_document_actor_id'))
        return array_merge($base, array('status'=>'inconsistent'));

    $task_status = get_hrm_lifecycle_employee_separation_task_browser_status($employee_id);
    if (!is_array($task_status) || !isset($task_status['status']))
        return array_merge($base, array('status'=>'inconsistent'));
    if ((string)$task_status['status'] === 'inconsistent')
        return array_merge($base, array('status'=>'inconsistent'));
    if ((string)$task_status['status'] !== 'completed' || empty($task_status['own'])
        || (int)$task_status['lifecycle_command_id'] <= 0)
        return $base;

    $actor = (int)hrm_lifecycle_separation_task_document_actor_id();
    $command_id = (int)$task_status['lifecycle_command_id'];
    if ($actor <= 0 || $command_id <= 0)
        return array_merge($base, array('status'=>'inconsistent'));

    $sql = "SELECT c.lifecycle_command_id,c.worker_id,c.command_type,c.command_status,c.requested_by,c.completed_by,"
        ."t.lifecycle_task_id,t.task_code,t.task_status,t.confidentiality_class,t.retention_locked task_retention_locked,"
        ."t.row_version task_row_version,t.created_by,t.completed_by task_completed_by "
        ."FROM ".TB_PREF."hrm_lifecycle_commands c INNER JOIN ".TB_PREF."hrm_lifecycle_tasks t "
        ."ON t.lifecycle_command_id=c.lifecycle_command_id WHERE c.lifecycle_command_id=".$command_id
        ." AND t.task_code=".db_escape(HRM_LIFECYCLE_SEPARATION_TASK_CODE)." LIMIT 1";
    $result = db_query($sql, 'could not read Employee Separation task-document browser command/task custody');
    $ctx = $result ? db_fetch_assoc($result) : false;
    if (!$ctx || (int)$ctx['lifecycle_command_id'] !== $command_id
        || (int)$ctx['worker_id'] <= 0
        || (string)$ctx['command_type'] !== HRM_LIFECYCLE_SEPARATION_COMMAND_TYPE
        || (string)$ctx['command_status'] !== 'completed'
        || (int)$ctx['requested_by'] !== $actor
        || (int)$ctx['completed_by'] <= 0
        || (int)$ctx['lifecycle_task_id'] <= 0
        || (string)$ctx['task_code'] !== HRM_LIFECYCLE_SEPARATION_TASK_CODE
        || (string)$ctx['task_status'] !== 'completed'
        || (string)$ctx['confidentiality_class'] !== 'restricted'
        || (int)$ctx['task_retention_locked'] !== 1
        || (int)$ctx['task_row_version'] !== 2
        || (int)$ctx['created_by'] !== $actor
        || (int)$ctx['task_completed_by'] !== $actor)
        return array_merge($base, array('status'=>'inconsistent'));

    $task_id = (int)$ctx['lifecycle_task_id'];
    $sql = "SELECT l.task_document_version_id,l.lifecycle_task_id,l.employee_document_id,"
        ."l.employee_worker_mapping_id,l.document_version,l.relationship_code,l.confidentiality_class,"
        ."l.document_snapshot_hash,l.retention_locked link_retention_locked,l.row_version,l.linked_at,l.linked_by,"
        ."m.worker_id mapping_worker_id,m.employee_number mapping_employee_number,m.employee_id mapping_employee_id,m.retired_at,"
        ."d.employee_id document_employee_id,d.retention_locked document_retention_locked,d.content_state,"
        ."e.employee_number document_employee_number FROM ".TB_PREF."hrm_lifecycle_task_document_versions l "
        ."INNER JOIN ".TB_PREF."hrm_employee_worker_map m ON m.mapping_id=l.employee_worker_mapping_id "
        ."INNER JOIN ".TB_PREF."employee_documents d ON d.doc_id=l.employee_document_id "
        ."INNER JOIN ".TB_PREF."employees e ON e.employee_id=d.employee_id "
        ."WHERE l.lifecycle_task_id=".$task_id." ORDER BY l.task_document_version_id LIMIT 2";
    $result = db_query($sql, 'could not read Employee Separation task-document browser link custody');
    if (!$result)
        return array_merge($base, array('status'=>'inconsistent'));
    $links = array();
    while ($row = db_fetch_assoc($result))
        $links[] = $row;
    if (count($links) === 0)
        return array('status'=>'not_linked','lifecycle_command_id'=>$command_id,'lifecycle_task_id'=>$task_id);
    if (count($links) !== 1)
        return array_merge($base, array('status'=>'inconsistent'));

    $link = $links[0];
    if ((int)$link['task_document_version_id'] <= 0
        || (int)$link['lifecycle_task_id'] !== $task_id
        || (int)$link['employee_document_id'] <= 0
        || (int)$link['employee_worker_mapping_id'] <= 0
        || (int)$link['document_version'] !== 1
        || (string)$link['relationship_code'] !== HRM_LIFECYCLE_SEPARATION_TASK_DOCUMENT_RELATIONSHIP
        || (string)$link['confidentiality_class'] !== 'restricted'
        || preg_match('/^[a-f0-9]{64}$/D', (string)$link['document_snapshot_hash']) !== 1
        || (int)$link['link_retention_locked'] !== 1
        || (int)$link['row_version'] !== 1
        || trim((string)$link['linked_at']) === ''
        || (int)$link['linked_by'] !== $actor
        || (int)$link['mapping_worker_id'] !== (int)$ctx['worker_id']
        || (int)$link['mapping_employee_number'] <= 0
        || trim((string)$link['mapping_employee_id']) === ''
        || $link['retired_at'] !== null
        || (string)$link['mapping_employee_id'] !== (string)$link['document_employee_id']
        || (int)$link['mapping_employee_number'] !== (int)$link['document_employee_number']
        || (int)$link['document_retention_locked'] !== 1
        || (string)$link['content_state'] !== 'available')
        return array_merge($base, array('status'=>'inconsistent'));

    return array('status'=>'linked','lifecycle_command_id'=>$command_id,'lifecycle_task_id'=>$task_id);
}

/** Coarse browser visibility only; internal identifiers are never returned. */
function get_hrm_lifecycle_employee_separation_task_document_browser_status($employee_id)
{
    $ctx = hrm_lifecycle_separation_task_document_browser_context($employee_id);
    return array('status'=>is_array($ctx) && isset($ctx['status']) ? (string)$ctx['status'] : 'inconsistent');
}

/**
 * Explicit POST wrapper. The client supplies only employee selector context and
 * an opaque document access key; command/task ids remain server-resolved.
 */
function link_hrm_lifecycle_employee_separation_task_document_from_browser($employee_id, $document_access_key)
{
    if (!function_exists('hrm_lifecycle_separation_task_document_access_key')
        || !function_exists('link_hrm_lifecycle_employee_separation_task_document'))
        return false;
    $document_access_key = hrm_lifecycle_separation_task_document_access_key($document_access_key);
    if ($document_access_key === false)
        return false;
    $ctx = hrm_lifecycle_separation_task_document_browser_context($employee_id);
    if (!is_array($ctx) || !in_array((string)$ctx['status'], array('not_linked','linked'), true)
        || (int)$ctx['lifecycle_command_id'] <= 0 || (int)$ctx['lifecycle_task_id'] <= 0)
        return false;
    return link_hrm_lifecycle_employee_separation_task_document(
        (int)$ctx['lifecycle_command_id'], $document_access_key
    );
}
