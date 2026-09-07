<?php
/**
 * HRM-FND-005 Employee Transfer task-document link writer.
 *
 * The writer binds exactly one already-completed, actor-owned
 * transfer_command_ack task to exactly one reconciled available employee
 * document. It permanently activates document retention (0 -> 1) and creates
 * one immutable version-1 restricted transfer_supporting_document link.
 *
 * It is not a generic document dispatcher and exposes no unlink, replace,
 * retention-unlock, task-level approval, e-signature, payroll or accounting
 * authority.
 */
$hrm_lifecycle_transfer_task_document_root = isset($path_to_root)
    ? $path_to_root : dirname(dirname(dirname(__DIR__)));
$hrm_lifecycle_transfer_task_document_dependencies = array(
    '/includes/types.inc',
    '/includes/db/domain_audit_db.inc',
    '/hrm/includes/db/lifecycle_transfer_task_db.inc',
);
foreach ($hrm_lifecycle_transfer_task_document_dependencies as $dependency) {
    $candidate = $hrm_lifecycle_transfer_task_document_root.$dependency;
    if (is_file($candidate))
        include_once($candidate);
}
unset($hrm_lifecycle_transfer_task_document_root,
    $hrm_lifecycle_transfer_task_document_dependencies, $dependency, $candidate);

if (!defined('HRM_LIFECYCLE_TRANSFER_TASK_DOCUMENT_RELATIONSHIP'))
    define('HRM_LIFECYCLE_TRANSFER_TASK_DOCUMENT_RELATIONSHIP', 'transfer_supporting_document');
if (!defined('HRM_LIFECYCLE_TRANSFER_TASK_DOCUMENT_REASON'))
    define('HRM_LIFECYCLE_TRANSFER_TASK_DOCUMENT_REASON', 'hrm_fnd_005_employee_transfer_task_document_writer');
if (!defined('HRM_LIFECYCLE_TRANSFER_TASK_DOCUMENT_SNAPSHOT_PURPOSE'))
    define('HRM_LIFECYCLE_TRANSFER_TASK_DOCUMENT_SNAPSHOT_PURPOSE', 'hrm-lifecycle-transfer-task-document-v1');

function hrm_lifecycle_transfer_task_document_actor_id()
{
    return function_exists('hrm_lifecycle_transfer_task_actor_id')
        ? hrm_lifecycle_transfer_task_actor_id()
        : (isset($_SESSION['wa_current_user']) && is_object($_SESSION['wa_current_user'])
            && isset($_SESSION['wa_current_user']->user) ? (int)$_SESSION['wa_current_user']->user : 0);
}

/** Linking needs both Transfer authority and restricted employee-document authority. */
function hrm_lifecycle_transfer_task_document_permission()
{
    return function_exists('user_check_access')
        && user_check_access('SA_EMPTRANSFER')
        && user_check_access('SA_EMPLOYEE');
}

function hrm_lifecycle_transfer_task_document_positive_int($value)
{
    if (is_int($value))
        return $value > 0 ? $value : false;
    $value = trim((string)$value);
    if (preg_match('/^[1-9][0-9]*$/D', $value) !== 1)
        return false;
    $number = (int)$value;
    return $number > 0 && (string)$number === $value ? $number : false;
}

function hrm_lifecycle_transfer_task_document_access_key($value)
{
    $value = trim((string)$value);
    return preg_match('/^[a-f0-9]{64}$/D', $value) === 1 ? $value : false;
}

/** Lock the active legacy employee/Worker compatibility row for the command Worker. */
function hrm_lifecycle_transfer_task_document_mapping_for_update($worker_id)
{
    $worker_id = hrm_lifecycle_transfer_task_document_positive_int($worker_id);
    if ($worker_id === false)
        return false;
    $result = db_query("SELECT mapping_id,employee_number,employee_id,worker_id,retired_at "
        ."FROM ".TB_PREF."hrm_employee_worker_map WHERE worker_id=".$worker_id
        ." AND retired_at IS NULL LIMIT 1 FOR UPDATE",
        'could not lock Employee Transfer task-document Worker mapping');
    $row = $result ? db_fetch_assoc($result) : false;
    return $row && (int)$row['mapping_id'] > 0 && (int)$row['employee_number'] > 0
        && trim((string)$row['employee_id']) !== '' && (int)$row['worker_id'] === $worker_id
        && $row['retired_at'] === null ? $row : false;
}

/**
 * Lock one document by opaque key and verify its accepted DB-level available
 * projection plus exact core-attachment employee ownership.
 */
function hrm_lifecycle_transfer_task_document_document_for_update($access_key)
{
    $access_key = hrm_lifecycle_transfer_task_document_access_key($access_key);
    if ($access_key === false || !defined('ST_EMPLOYEE'))
        return false;
    $sql = "SELECT d.doc_id,d.employee_id,d.attachment_id,d.access_key,d.content_state,"
        ."d.content_sha256,d.content_size,d.content_mime,d.retention_locked,"
        ."e.employee_number,a.id attachment_row_id,a.type_no attachment_type_no,"
        ."a.trans_no attachment_trans_no FROM ".TB_PREF."employee_documents d "
        ."INNER JOIN ".TB_PREF."employees e ON e.employee_id=d.employee_id "
        ."INNER JOIN ".TB_PREF."attachments a ON a.id=d.attachment_id "
        ."WHERE d.access_key=".db_escape($access_key)." LIMIT 1 FOR UPDATE";
    $result = db_query($sql, 'could not lock Employee Transfer supporting document');
    $row = $result ? db_fetch_assoc($result) : false;
    if (!$row || (int)$row['doc_id'] <= 0 || (string)$row['access_key'] !== $access_key
        || (string)$row['content_state'] !== 'available'
        || (int)$row['attachment_id'] <= 0
        || (int)$row['attachment_row_id'] !== (int)$row['attachment_id']
        || preg_match('/^[a-f0-9]{64}$/D', (string)$row['content_sha256']) !== 1
        || (int)$row['content_size'] <= 0 || trim((string)$row['content_mime']) === ''
        || (int)$row['employee_number'] <= 0
        || (int)$row['attachment_type_no'] !== (int)ST_EMPLOYEE
        || (int)$row['attachment_trans_no'] !== (int)$row['employee_number']
        || !in_array((int)$row['retention_locked'], array(0, 1), true))
        return false;
    return $row;
}

function hrm_lifecycle_transfer_task_document_link_for_update($task_id)
{
    $task_id = hrm_lifecycle_transfer_task_document_positive_int($task_id);
    if ($task_id === false)
        return false;
    $result = db_query("SELECT task_document_version_id,lifecycle_task_id,employee_document_id,"
        ."employee_worker_mapping_id,document_version,relationship_code,confidentiality_class,"
        ."document_snapshot_hash,retention_locked,row_version,linked_at,linked_by FROM "
        .TB_PREF."hrm_lifecycle_task_document_versions WHERE lifecycle_task_id=".$task_id
        ." ORDER BY task_document_version_id LIMIT 2 FOR UPDATE",
        'could not lock Employee Transfer task-document link');
    if (!$result)
        return false;
    $rows = array();
    while ($row = db_fetch_assoc($result))
        $rows[] = $row;
    return $rows;
}

/** HMAC-only immutable snapshot; protected values never leave the keyed digest. */
function hrm_lifecycle_transfer_task_document_snapshot_hash($task, $command, $mapping, $document)
{
    if (!function_exists('domain_audit_signing_key') || !function_exists('domain_audit_sensitive_hash'))
        return false;
    $company = function_exists('user_company') ? (int)user_company() : 0;
    $key = domain_audit_signing_key($company, false);
    if ($key === false)
        return false;
    return domain_audit_sensitive_hash(array(
        'contract'=>'hrm-fnd-005-transfer-task-document-v1',
        'lifecycle_task_id'=>(int)$task['lifecycle_task_id'],
        'lifecycle_command_id'=>(int)$command['lifecycle_command_id'],
        'worker_id'=>(int)$command['worker_id'],
        'mapping_id'=>(int)$mapping['mapping_id'],
        'employee_number'=>(int)$mapping['employee_number'],
        'employee_id'=>(string)$mapping['employee_id'],
        'employee_document_id'=>(int)$document['doc_id'],
        'attachment_id'=>(int)$document['attachment_id'],
        'content_sha256'=>(string)$document['content_sha256'],
        'content_size'=>(int)$document['content_size'],
        'content_mime'=>(string)$document['content_mime'],
        'document_version'=>1,
        'relationship_code'=>HRM_LIFECYCLE_TRANSFER_TASK_DOCUMENT_RELATIONSHIP,
    ), $key, HRM_LIFECYCLE_TRANSFER_TASK_DOCUMENT_SNAPSHOT_PURPOSE);
}

function hrm_lifecycle_transfer_task_document_link_matches(
    $link, $task, $mapping, $document, $snapshot_hash, $actor
) {
    return is_array($link)
        && (int)$link['task_document_version_id'] > 0
        && (int)$link['lifecycle_task_id'] === (int)$task['lifecycle_task_id']
        && (int)$link['employee_document_id'] === (int)$document['doc_id']
        && (int)$link['employee_worker_mapping_id'] === (int)$mapping['mapping_id']
        && (int)$link['document_version'] === 1
        && (string)$link['relationship_code'] === HRM_LIFECYCLE_TRANSFER_TASK_DOCUMENT_RELATIONSHIP
        && (string)$link['confidentiality_class'] === 'restricted'
        && (string)$link['document_snapshot_hash'] === (string)$snapshot_hash
        && (int)$link['retention_locked'] === 1
        && (int)$link['row_version'] === 1
        && trim((string)$link['linked_at']) !== ''
        && (int)$link['linked_by'] === (int)$actor;
}

function hrm_lifecycle_transfer_task_document_audit($link, $task, $command, $mapping)
{
    if (!function_exists('append_domain_audit_event'))
        return false;
    return append_domain_audit_event(array(
        'company_id'=>function_exists('user_company') ? (int)user_company() : 0,
        'legal_entity_id'=>0,
        'entity_type'=>'hrm_lifecycle_task_document_version',
        'entity_key'=>array(
            'task_document_version_id'=>(int)$link['task_document_version_id'],
            'lifecycle_task_id'=>(int)$task['lifecycle_task_id'],
            'lifecycle_command_id'=>(int)$command['lifecycle_command_id'],
        ),
        'action'=>'hrm.lifecycle.task_document.linked',
        'reason_code'=>HRM_LIFECYCLE_TRANSFER_TASK_DOCUMENT_REASON,
        'before'=>array('document_linked'=>false,'document_retention_locked'=>false),
        'after'=>array(
            'document_linked'=>true,
            'document_version'=>1,
            'relationship_code'=>HRM_LIFECYCLE_TRANSFER_TASK_DOCUMENT_RELATIONSHIP,
            'confidentiality_class'=>'restricted',
            'document_retention_locked'=>true,
            'link_retention_locked'=>true,
            'row_version'=>1,
        ),
        'restricted_diff'=>array(
            'command_type'=>HRM_LIFECYCLE_TRANSFER_COMMAND_TYPE,
            'task_code'=>HRM_LIFECYCLE_TRANSFER_TASK_CODE,
            'same_subject_worker_mapping_proven'=>true,
            'employee_worker_mapping_id'=>(int)$mapping['mapping_id'],
            'document_snapshot_hmac_recorded'=>true,
            'document_payload_stored_in_link'=>false,
            'document_payload_stored_in_audit'=>false,
            'document_retention_transition'=>'0_to_1',
            'document_retention_unlock_performed'=>false,
            'document_replace_performed'=>false,
            'document_delete_performed'=>false,
            'task_level_approval_added'=>false,
            'central_approval_remains_command_owned'=>true,
            'esignature_connector_invoked'=>false,
            'employee_rows_mutated'=>0,
            'employment_rows_mutated'=>0,
            'assignment_rows_mutated'=>0,
            'employee_history_rows_added'=>0,
            'payroll_rows_mutated'=>0,
            'bank_transaction_rows_mutated'=>0,
            'gl_rows_mutated'=>0,
        ),
    )) !== false;
}

/**
 * Link one current actor's completed Transfer acknowledgement task to one
 * available supporting document. Exact retry returns the same immutable link.
 */
function link_hrm_lifecycle_employee_transfer_task_document($command_id, $document_access_key)
{
    if (!hrm_lifecycle_transfer_task_document_permission())
        return false;
    $actor = hrm_lifecycle_transfer_task_document_actor_id();
    $command_id = hrm_lifecycle_transfer_task_document_positive_int($command_id);
    $document_access_key = hrm_lifecycle_transfer_task_document_access_key($document_access_key);
    if ($actor <= 0 || $command_id === false || $document_access_key === false)
        return false;

    begin_transaction();
    $command = function_exists('hrm_lifecycle_transfer_task_command_for_update')
        ? hrm_lifecycle_transfer_task_command_for_update($command_id) : false;
    $task = $command && function_exists('hrm_lifecycle_transfer_task_row_for_update')
        ? hrm_lifecycle_transfer_task_row_for_update($command_id) : false;
    if (!$command || !$task || !hrm_lifecycle_transfer_task_contract_matches($task, $command_id)
        || (string)$task['task_status'] !== 'completed'
        || (int)$task['created_by'] !== $actor || (int)$task['completed_by'] !== $actor) {
        cancel_transaction();
        return false;
    }

    $mapping = hrm_lifecycle_transfer_task_document_mapping_for_update((int)$command['worker_id']);
    $document = $mapping
        ? hrm_lifecycle_transfer_task_document_document_for_update($document_access_key) : false;
    if (!$mapping || !$document
        || (string)$mapping['employee_id'] !== (string)$document['employee_id']
        || (int)$mapping['employee_number'] !== (int)$document['employee_number']) {
        cancel_transaction();
        return false;
    }

    $snapshot_hash = hrm_lifecycle_transfer_task_document_snapshot_hash(
        $task, $command, $mapping, $document
    );
    if ($snapshot_hash === false || preg_match('/^[a-f0-9]{64}$/D', $snapshot_hash) !== 1) {
        cancel_transaction();
        return false;
    }

    $links = hrm_lifecycle_transfer_task_document_link_for_update((int)$task['lifecycle_task_id']);
    if ($links === false || count($links) > 1) {
        cancel_transaction();
        return false;
    }
    if (count($links) === 1) {
        $link = $links[0];
        $ok = (int)$document['retention_locked'] === 1
            && hrm_lifecycle_transfer_task_document_link_matches(
                $link, $task, $mapping, $document, $snapshot_hash, $actor
            );
        if ($ok)
            commit_transaction();
        else
            cancel_transaction();
        return $ok ? (int)$link['task_document_version_id'] : false;
    }

    if ((int)$document['retention_locked'] !== 0) {
        cancel_transaction();
        return false;
    }
    if (!db_query("UPDATE ".TB_PREF."employee_documents SET retention_locked=1 WHERE doc_id="
        .(int)$document['doc_id']." AND retention_locked=0",
        'could not activate Employee Transfer supporting-document retention')
        || (int)db_num_affected_rows() !== 1) {
        cancel_transaction();
        return false;
    }

    $sql = "INSERT INTO ".TB_PREF."hrm_lifecycle_task_document_versions "
        ."(lifecycle_task_id,employee_document_id,employee_worker_mapping_id,document_version,"
        ."relationship_code,confidentiality_class,document_snapshot_hash,retention_locked,row_version,linked_by) VALUES ("
        .(int)$task['lifecycle_task_id'].",".(int)$document['doc_id'].",".(int)$mapping['mapping_id']
        .",1,".db_escape(HRM_LIFECYCLE_TRANSFER_TASK_DOCUMENT_RELATIONSHIP)
        .",'restricted',".db_escape($snapshot_hash).",1,1,".$actor.")";
    if (!db_query($sql, 'could not link Employee Transfer supporting document')) {
        cancel_transaction();
        return false;
    }
    $link_id = (int)db_insert_id();
    $links = hrm_lifecycle_transfer_task_document_link_for_update((int)$task['lifecycle_task_id']);
    $link = is_array($links) && count($links) === 1 ? $links[0] : false;
    if ($link_id <= 0 || !$link || (int)$link['task_document_version_id'] !== $link_id
        || !hrm_lifecycle_transfer_task_document_link_matches(
            $link, $task, $mapping, $document, $snapshot_hash, $actor
        ) || !hrm_lifecycle_transfer_task_document_audit($link, $task, $command, $mapping)) {
        cancel_transaction();
        return false;
    }
    commit_transaction();
    return $link_id;
}
