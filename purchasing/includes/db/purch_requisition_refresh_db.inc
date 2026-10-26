<?php
/** Maker-owned pending recovery through the existing core cancellation/submission. */
require_once __DIR__.'/purch_requisition_submit_db.inc';

/** Find a maker's pending review for presentation only; execution rechecks custody. */
function get_purch_requisition_refresh_draft($requisition_id)
{
    $context = purch_requisition_reopen_context();
    $identity = dimension_writer_reference_id($requisition_id);
    if ($context === false || !$identity || !class_exists('ApprovalWorkflow')
        || !ApprovalWorkflow::isRegistered(ST_PURCHREQUISITION)) return false;
    $result = get_approval_drafts_by_submitter((int)$_SESSION['wa_current_user']->user,
        array('status'=>APPROVAL_STATUS_PENDING, 'trans_type'=>ST_PURCHREQUISITION));
    if (!$result) return false;
    $match = false;
    $approval = get_approval_workflow_service();
    while ($row = db_fetch_assoc($result)) {
        $data = $approval->decodeStoredDraftData($row['draft_data']);
        if (!is_array($data) || !isset($data['requisition_id'])
            || !($source = dimension_writer_reference_id($data['requisition_id']))) return false;
        if ($source !== $identity) continue;
        if ($match !== false) return false;
        $match = (int)$row['id'];
    }
    return purch_requisition_reopen_context() === $context ? $match : false;
}

/**
 * Replace exactly one maker-owned pending review under current policy. Preserve
 * original payload/actions, restart every checker level, and link both histories.
 * The expected old draft ID prevents a stale form/retry from cancelling its
 * successor. Orphan, conflicting and drifted custody remains fail closed.
 */
function refresh_purch_requisition_approval($requisition_id, $expected_draft_id)
{
    $context = purch_requisition_reopen_context();
    $identity = dimension_writer_reference_id($requisition_id);
    $expected = dimension_writer_reference_id($expected_draft_id);
    if ($context === false || !$identity || !$expected || !class_exists('ApprovalWorkflow')
        || !ApprovalWorkflow::isRegistered(ST_PURCHREQUISITION)) return false;
    $role = (int)$_SESSION['wa_current_user']->access;
    try {
        if (!begin_transaction()) return false;
        $approval = get_approval_workflow_service();
        // Same core-before-source lock order as cancellation and submission.
        $drafts = purch_requisition_reopen_drafts($identity, 0, $approval);
        if (!$drafts) { cancel_transaction(); return false; }
        $old = $drafts[count($drafts)-1];
        if ((int)$old['id'] !== $expected || (int)$old['status'] !== APPROVAL_STATUS_PENDING
            || (int)$old['submitted_by'] !== (int)$_SESSION['wa_current_user']->user) {
            cancel_transaction(); return false;
        }
        $history = purch_requisition_reopen_rows('SELECT * FROM '.TB_PREF.'approval_actions WHERE draft_id='.$expected.' ORDER BY id FOR UPDATE');
        $snapshot = purch_requisition_reopen_source($identity, 'submitted', $old['decoded']);
        if (!$history || !$snapshot || !$snapshot['lines']
            || (isset($old['decoded']['requisition_source_sha256'])
                && $old['decoded']['requisition_source_sha256'] !== hash('sha256', serialize($snapshot)))
            || purch_requisition_reopen_drafts($identity, (int)$snapshot['material']['request_id'], $approval) === false
            || !reopen_purch_requisition_from_core($identity)) { cancel_transaction(); return false; }
        $result = submit_purch_requisition_to_core($identity);
        if (!is_array($result) || !isset($result['status'], $result['draft_id'])
            || $result['status'] !== 'pending' || (int)$result['draft_id'] === $expected) { cancel_transaction(); return false; }
        $successor = get_approval_draft_for_update($result['draft_id']);
        $previous_rows = purch_requisition_reopen_rows('SELECT * FROM '.TB_PREF.'approval_drafts WHERE id='.$expected.' FOR UPDATE');
        $previous = $previous_rows ? $previous_rows[0] : false;
        // Cancellation may only change core terminal state/date and notifications.
        unset($old['decoded']);
        $preserved = $previous;
        if (!$previous || !$successor || (int)$previous['status'] !== APPROVAL_STATUS_CANCELLED || empty($previous['completed_date'])
            || (int)$successor['current_level'] !== 1
            || (int)$successor['submitted_by'] !== (int)$old['submitted_by']) { cancel_transaction(); return false; }
        foreach (array('status','completed_date') as $field)
            if (array_key_exists($field, $old)) $preserved[$field] = $old[$field];
        $data = $approval->decodeStoredDraftData($successor['draft_data']);
        $current = purch_requisition_reopen_source($identity, 'submitted', $data);
        if ($current) $current['material']['updated_at'] = $snapshot['material']['updated_at'];
        if ($preserved !== $old || $current !== $snapshot) { cancel_transaction(); return false; }
        $comment = 'Purchase requisition approval refresh: draft #'.$expected.' to #'.(int)$result['draft_id'];
        foreach (array($expected, (int)$result['draft_id']) as $draft_id) {
            $action = add_approval_action($draft_id, 'resubmit', null, 0, $comment);
            $rows = $action ? purch_requisition_reopen_rows('SELECT * FROM '.TB_PREF.'approval_actions WHERE id='.(int)$action.' FOR UPDATE') : false;
            if (!$rows || (int)$rows[0]['draft_id'] !== $draft_id || $rows[0]['action_type'] !== 'resubmit'
                || (int)$rows[0]['user_id'] !== (int)$_SESSION['wa_current_user']->user
                || $rows[0]['comments'] !== $comment) { cancel_transaction(); return false; }
        }
        // Audit triggers cannot change the source, payload or company after review.
        $after = purch_requisition_reopen_source($identity, 'submitted', $data);
        if ($after) $after['material']['updated_at'] = $snapshot['material']['updated_at'];
        foreach ($history as $record) {
            $retained = purch_requisition_reopen_rows('SELECT * FROM '.TB_PREF.'approval_actions WHERE id='.(int)$record['id'].' FOR UPDATE');
            if (!$retained || $retained[0] !== $record) { cancel_transaction(); return false; }
        }
        if ($after !== $snapshot || purch_requisition_reopen_rows('SELECT * FROM '.TB_PREF.'approval_drafts WHERE id='.$expected.' FOR UPDATE') !== array($previous)
            || get_approval_draft_for_update($result['draft_id']) !== $successor
            || purch_requisition_reopen_context() !== $context || (int)$_SESSION['wa_current_user']->access !== $role
            || !commit_transaction()) { cancel_transaction(); return false; }
        return $result;
    } catch (Exception $exception) { cancel_transaction(); return false;
    } catch (Throwable $exception) { cancel_transaction(); return false; }
}
