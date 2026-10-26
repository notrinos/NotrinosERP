<?php
/** Guard the existing core rejection; never infer approval from a source ID. */
require_once __DIR__.'/purch_requisition_reopen_db.inc';

/** Keep validation local to one core action and the owning SQL transaction. */
function purch_requisition_reject_permit($operation, $value = null)
{
    static $permit = null;
    if ($operation === 'set') { $permit = $value; return true; }
    $saved = $permit;
    $permit = null;
    return $operation === 'take' ? $saved : true;
}

/** Discard unused validation at rollback, outer commit and retry boundaries. */
function release_purch_requisition_reject_permit()
{
    return purch_requisition_reject_permit('clear');
}

/** Validate the current native pair and independent core checker before rejection. */
function validate_purchase_requisition_reject_action($data, $draft, $actor, $comments)
{
    $context = purch_requisition_reopen_context();
    if ($context === false || empty($GLOBALS['transaction_level']) || !is_array($draft)
        || !is_array($actor) || !is_string($comments) || strpos($comments, "\0") !== false
        || !isset($actor['user_id'], $actor['role_id'], $draft['id'], $draft['submitted_by'], $draft['status'])
        || !dimension_writer_reference_id($actor['user_id']) || !dimension_writer_reference_id($actor['role_id'])
        || (int)$actor['user_id'] !== (int)$_SESSION['wa_current_user']->user
        || (int)$actor['role_id'] !== (int)$_SESSION['wa_current_user']->access
        || (int)$draft['submitted_by'] === (int)$actor['user_id']
        || (int)$draft['status'] !== APPROVAL_STATUS_PENDING
        || !isset($data['requisition_id']) || !($identity = dimension_writer_reference_id($data['requisition_id']))) return false;
    $approval = get_approval_workflow_service();
    if (!$approval->canActOnDraft((int)$draft['id'])) return false;
    $snapshot = purch_requisition_reopen_source($identity, 'submitted', $data);
    if (!$snapshot || !$snapshot['lines']) return false;
    $drafts = purch_requisition_reopen_drafts($identity, (int)$snapshot['header']['material_request_id'], $approval);
    if (!$drafts || (int)$drafts[count($drafts)-1]['id'] !== (int)$draft['id']
        || $drafts[count($drafts)-1]['decoded'] !== $data) return false;
    foreach ($drafts as $row)
        if ((int)$row['status'] === APPROVAL_STATUS_PENDING && (int)$row['id'] !== (int)$draft['id']) return false;
    if (!register_transaction_resource_release('purchase-requisition-reject', 'release_purch_requisition_reject_permit')) return false;
    return purch_requisition_reject_permit('set', array('context'=>$context, 'draft_id'=>(int)$draft['id'],
        'data'=>$data, 'actor'=>$actor, 'comments'=>$comments, 'snapshot'=>$snapshot));
}

/** Compare all retained bytes after native SQL and completion hooks. */
function verify_purch_requisition_rejection($snapshot, $context, $actor, $reason)
{
    $identity = (int)$snapshot['header']['id'];
    $source = (int)$snapshot['material']['request_id'];
    $headers = purch_requisition_reopen_rows('SELECT * FROM '.TB_PREF.'purch_requisitions WHERE id='.$identity.' FOR UPDATE');
    $materials = purch_requisition_reopen_rows('SELECT * FROM '.TB_PREF.'wh_material_requests WHERE request_id='.$source.' FOR UPDATE');
    $lines = purch_requisition_reopen_rows('SELECT * FROM '.TB_PREF.'purch_requisition_lines WHERE requisition_id='.$identity.' ORDER BY id FOR UPDATE');
    if (!$headers || !$materials || $lines === false || count($lines) !== count($snapshot['lines'])
        || $headers[0]['status'] !== 'rejected' || $materials[0]['status'] !== 'draft'
        || (int)$headers[0]['rejected_by'] !== $actor || empty($headers[0]['rejected_date'])
        || $headers[0]['rejection_reason'] !== ($reason === '' ? null : $reason)) return false;
    foreach (array('status','rejected_by','rejected_date','rejection_reason') as $field)
        $headers[0][$field] = $snapshot['header'][$field];
    foreach (array('status','updated_at') as $field) $materials[0][$field] = $snapshot['material'][$field];
    foreach ($lines as $index=>$line) {
        if ($line['status'] !== 'rejected') return false;
        $lines[$index]['status'] = $snapshot['lines'][$index]['status'];
    }
    return $headers[0] === $snapshot['header'] && $materials[0] === $snapshot['material'] && $lines === $snapshot['lines']
        && purch_requisition_reopen_rows('SELECT * FROM '.TB_PREF.'wh_material_request_lines WHERE request_id='.$source.' ORDER BY line_id FOR UPDATE') === $snapshot['material_lines']
        && purch_requisition_reopen_context() === $context;
}

/** Execute only the core's one validated terminal rejection inside its transaction. */
function reject_purch_requisition_from_core($requisition_id, $rejected_by, $reason, $draft_id = 0, $data = null)
{
    $context = purch_requisition_reopen_context();
    $identity = dimension_writer_reference_id($requisition_id);
    if ($context === false || !$identity || empty($GLOBALS['transaction_level']) || !is_string($reason)) return false;
    $permit = purch_requisition_reject_permit('take');
    if (!$permit || $permit['draft_id'] !== $draft_id || $permit['data'] !== $data || $permit['context'] !== $context || (int)$permit['data']['requisition_id'] !== $identity
        || $permit['comments'] !== $reason || ($rejected_by !== 0 && $rejected_by !== $permit['actor']['user_id'])
        || (int)$permit['actor']['user_id'] !== (int)$_SESSION['wa_current_user']->user
        || (int)$permit['actor']['role_id'] !== (int)$_SESSION['wa_current_user']->access) return false;
    $draft = get_approval_draft_for_update($permit['draft_id']);
    if (!$draft || (int)$draft['status'] !== APPROVAL_STATUS_REJECTED || (int)$draft['trans_type'] !== ST_PURCHREQUISITION
        || (int)$draft['submitted_by'] === (int)$permit['actor']['user_id']
        || get_approval_workflow_service()->decodeStoredDraftData($draft['draft_data']) !== $permit['data']) return false;
    $snapshot = purch_requisition_reopen_source($identity, 'submitted', $permit['data']);
    if ($snapshot !== $permit['snapshot']) return false;
    $actor = (int)$permit['actor']['user_id'];
    $source = (int)$snapshot['material']['request_id'];
    if (!db_query("UPDATE ".TB_PREF."wh_material_requests SET status='draft',updated_at=NOW() WHERE request_id=".$source." AND status='submitted'")
        || db_num_affected_rows() !== 1
        || !db_query("UPDATE ".TB_PREF."purch_requisitions SET status='rejected',rejected_by=".$actor.",rejected_date=NOW(),rejection_reason="
            .sql_purch_requisition_header_payload($reason === '' ? null : $reason)." WHERE id=".$identity." AND status='submitted'")
        || db_num_affected_rows() !== 1
        || !db_query("UPDATE ".TB_PREF."purch_requisition_lines SET status='rejected' WHERE requisition_id=".$identity)
        || !verify_purch_requisition_rejection($snapshot, $context, $actor, $reason)) return false;
    $hook_data = array('requisition_id'=>$identity, 'reason'=>$reason);
    hook_invoke_all('purch_requisition_rejected', $hook_data);
    return verify_purch_requisition_rejection($snapshot, $context, $actor, $reason)
        && (int)$_SESSION['wa_current_user']->access === (int)$permit['actor']['role_id'];
}
