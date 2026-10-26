<?php
/** Exact source custody for the existing core requisition approval executor. */
require_once __DIR__.'/purch_requisition_reject_db.inc';

/** Keep checker validation inside the owning core transaction. */
function purch_requisition_approve_permit($operation, $value = null)
{
    static $permit = null;
    if ($operation === 'set') { $permit = $value; return true; }
    $saved = $permit;
    $permit = null;
    return $operation === 'take' ? $saved : true;
}

/** Release incomplete multilevel validation on outer completion or rollback. */
function release_purch_requisition_approve_permit()
{
    return purch_requisition_approve_permit('clear');
}

/** Require current policy, independent checker and exact retained native source. */
function validate_purchase_requisition_approve_action($data, $draft, $actor, $comments)
{
    // Reuse the same current actor, unique history and paired-source admission
    // as rejection. Consume its permit immediately; only approval owns execution.
    if (!validate_purchase_requisition_reject_action($data, $draft, $actor, $comments)) return false;
    $permit = purch_requisition_reject_permit('take');
    $workflow = get_approval_workflow_by_type(ST_PURCHREQUISITION, true);
    if (!$permit || !$workflow || empty($workflow['is_active'])
        || (int)$workflow['id'] !== (int)$draft['workflow_id']
        || (int)$workflow['version'] !== (int)$draft['workflow_version']
        || !isset($draft['amount']) || !is_numeric($draft['amount']) || !is_finite((float)$draft['amount'])
        || (float)$draft['amount'] !== (float)$permit['snapshot']['header']['total_estimated']
        || !workflow_has_active_levels($workflow['id'], true)
        || !get_approval_workflow_service()->canActOnDraft((int)$draft['id'], true)) return false;
    // New submissions bind all four native rows, including opaque retained data.
    // Legacy decoded envelopes keep their historical intrinsic-field contract.
    if (isset($data['requisition_source_sha256'])
        && (!is_string($data['requisition_source_sha256'])
            || $data['requisition_source_sha256'] !== hash('sha256', serialize($permit['snapshot'])))) return false;
    if (!register_transaction_resource_release('purchase-requisition-approve', 'release_purch_requisition_approve_permit')) return false;
    return purch_requisition_approve_permit('set', $permit);
}

/** Verify explicit approval effects and every other retained native byte. */
function verify_purch_requisition_approval($snapshot, $context, $actor)
{
    if (purch_requisition_reopen_context() !== $context) return false;
    $identity = (int)$snapshot['header']['id'];
    $source = (int)$snapshot['material']['request_id'];
    $headers = purch_requisition_reopen_rows('SELECT * FROM '.TB_PREF.'purch_requisitions WHERE id='.$identity.' FOR UPDATE');
    $materials = purch_requisition_reopen_rows('SELECT * FROM '.TB_PREF.'wh_material_requests WHERE request_id='.$source.' FOR UPDATE');
    $lines = purch_requisition_reopen_rows('SELECT * FROM '.TB_PREF.'purch_requisition_lines WHERE requisition_id='.$identity.' ORDER BY id FOR UPDATE');
    if (!$headers || !$materials || $lines === false || count($lines) !== count($snapshot['lines'])
        || $headers[0]['status'] !== 'approved' || $materials[0]['status'] !== 'approved'
        || (int)$headers[0]['approved_by'] !== $actor || empty($headers[0]['approved_date'])
        || (int)$materials[0]['approved_by'] !== $actor) return false;
    foreach (array('status','approved_by','approved_date') as $field) $headers[0][$field] = $snapshot['header'][$field];
    foreach (array('status','approved_by','updated_at') as $field) $materials[0][$field] = $snapshot['material'][$field];
    foreach ($lines as $index=>$line) {
        if ($line['status'] !== 'approved') return false;
        $lines[$index]['status'] = $snapshot['lines'][$index]['status'];
    }
    return $headers[0] === $snapshot['header'] && $materials[0] === $snapshot['material'] && $lines === $snapshot['lines']
        && purch_requisition_reopen_rows('SELECT * FROM '.TB_PREF.'wh_material_request_lines WHERE request_id='.$source.' ORDER BY line_id FOR UPDATE') === $snapshot['material_lines']
        && purch_requisition_reopen_context() === $context;
}

/** Execute only a validated core terminal transition; direct legacy calls deny. */
function approve_purch_requisition_from_core($requisition_id, $approved_by = 0, $draft_id = 0, $data = null)
{
    $context = purch_requisition_reopen_context();
    $identity = dimension_writer_reference_id($requisition_id);
    $draft_id = dimension_writer_reference_id($draft_id);
    if ($context === false || !$identity || !$draft_id || empty($GLOBALS['transaction_level'])) return false;
    $permit = purch_requisition_approve_permit('take');
    if (!$permit || $permit['draft_id'] !== $draft_id || $permit['data'] !== $data || $permit['context'] !== $context
        || (int)$permit['data']['requisition_id'] !== $identity
        || ($approved_by !== 0 && $approved_by !== $permit['actor']['user_id'])
        || (int)$permit['actor']['user_id'] !== (int)$_SESSION['wa_current_user']->user
        || (int)$permit['actor']['role_id'] !== (int)$_SESSION['wa_current_user']->access) return false;
    $draft = get_approval_draft_for_update($draft_id);
    if (!$draft || (int)$draft['status'] !== APPROVAL_STATUS_APPROVED || (int)$draft['trans_type'] !== ST_PURCHREQUISITION
        || (int)$draft['submitted_by'] === (int)$permit['actor']['user_id']
        || get_approval_workflow_service()->decodeStoredDraftData($draft['draft_data']) !== $data) return false;
    $snapshot = purch_requisition_reopen_source($identity, 'submitted', $data);
    if ($snapshot !== $permit['snapshot']) return false;
    $actor = (int)$permit['actor']['user_id'];
    $source = (int)$snapshot['material']['request_id'];
    if (!db_query("UPDATE ".TB_PREF."wh_material_requests SET status='approved',approved_by=".$actor.",updated_at=NOW() WHERE request_id=".$source." AND status='submitted'")
        || db_num_affected_rows() !== 1
        || !db_query("UPDATE ".TB_PREF."purch_requisitions SET status='approved',approved_by=".$actor.",approved_date=NOW() WHERE id=".$identity." AND status='submitted'")
        || db_num_affected_rows() !== 1
        || !db_query("UPDATE ".TB_PREF."purch_requisition_lines SET status='approved' WHERE requisition_id=".$identity)
        || !verify_purch_requisition_approval($snapshot, $context, $actor)) return false;
    $hook_identity = $identity;
    hook_invoke_all('purch_requisition_approved', $hook_identity);
    return (int)$_SESSION['wa_current_user']->access === (int)$permit['actor']['role_id']
        && verify_purch_requisition_approval($snapshot, $context, $actor);
}
