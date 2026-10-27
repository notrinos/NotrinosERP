<?php
/** Maker-owned pending recovery through the existing core cancellation/submission. */
require_once __DIR__.'/purch_requisition_submit_db.inc';

/** Present only this maker's identifiable duplicate reviews; execution locks all custody. */
function get_purch_requisition_duplicate_drafts($requisition_id)
{
    $context = purch_requisition_reopen_context();
    $identity = dimension_writer_reference_id($requisition_id);
    if ($context === false || !$identity || !class_exists('ApprovalWorkflow')
        || !ApprovalWorkflow::isRegistered(ST_PURCHREQUISITION)) return false;
    $result = get_approval_drafts_by_submitter((int)$_SESSION['wa_current_user']->user,
        array('status'=>APPROVAL_STATUS_PENDING, 'trans_type'=>ST_PURCHREQUISITION));
    if (!$result) return false;
    $ids = array();
    $approval = get_approval_workflow_service();
    while ($row = db_fetch_assoc($result)) {
        $data = $approval->decodeStoredDraftData($row['draft_data']);
        if (!is_array($data) || !isset($data['requisition_id'])
            || !($source = dimension_writer_reference_id($data['requisition_id']))) return false;
        if ($source === $identity) $ids[] = (int)$row['id'];
    }
    sort($ids, SORT_NUMERIC);
    return count($ids) > 1 && purch_requisition_reopen_context() === $context ? $ids : false;
}

/** Read only the locked reservations and notices owned by the explicit old reviews. */
function purch_requisition_duplicate_effect_rows($ids, $reserved)
{
    $numbers = purch_requisition_reopen_rows('SELECT * FROM '.TB_PREF.'voided WHERE type='.ST_PURCHREQUISITION
        .' AND id IN ('.implode(',', $reserved).') ORDER BY id FOR UPDATE');
    $notices = purch_requisition_reopen_rows('SELECT * FROM '.TB_PREF.'approval_notifications WHERE draft_id IN ('
        .implode(',', $ids).') ORDER BY id FOR UPDATE');
    return $numbers === false || $notices === false ? false : array('numbers'=>$numbers, 'notices'=>$notices);
}

/**
 * Recover an explicitly identified duplicate set owned by the original maker.
 * Every old review is cancelled by the shared core while the unchanged source
 * remains submitted. Only after all cancellations succeed may the source reopen
 * and submit under current policy. No old payload or checker decision is erased.
 */
function recover_purch_requisition_duplicate_approvals($requisition_id, $expected_draft_ids)
{
    $context = purch_requisition_reopen_context();
    $identity = dimension_writer_reference_id($requisition_id);
    if ($context === false || !$identity || !is_array($expected_draft_ids) || count($expected_draft_ids) < 2
        || !class_exists('ApprovalWorkflow') || !ApprovalWorkflow::isRegistered(ST_PURCHREQUISITION)) return false;
    $expected = array();
    foreach ($expected_draft_ids as $value) {
        $id = dimension_writer_reference_id($value);
        if (!$id || in_array($id, $expected, true)) return false;
        $expected[] = $id;
    }
    sort($expected, SORT_NUMERIC);
    $role = (int)$_SESSION['wa_current_user']->access;
    try {
        if (!begin_transaction()) return false;
        $approval = get_approval_workflow_service();
        $drafts = purch_requisition_reopen_drafts($identity, 0, $approval);
        if (!$drafts) { cancel_transaction(); return false; }
        $pending = array();
        $reserved = array();
        $snapshot = null;
        foreach ($drafts as $draft) {
            if ((int)$draft['status'] !== APPROVAL_STATUS_PENDING) {
                if (!in_array((int)$draft['status'], array(APPROVAL_STATUS_REJECTED, APPROVAL_STATUS_CANCELLED), true)) {
                    cancel_transaction(); return false;
                }
                continue;
            }
            $current = purch_requisition_reopen_source($identity, 'submitted', $draft['decoded']);
            if ((int)$draft['submitted_by'] !== (int)$_SESSION['wa_current_user']->user || !$current || !$current['lines']
                || ($snapshot !== null && $current !== $snapshot)
                || (array_key_exists('requisition_source_sha256', $draft['decoded'])
                    && $draft['decoded']['requisition_source_sha256'] !== hash('sha256', serialize($current)))) {
                cancel_transaction(); return false;
            }
            $snapshot = $current;
            $pending[] = (int)$draft['id'];
            $number = dimension_writer_reference_id($draft['reserved_trans_no']);
            $workflow = purch_requisition_reopen_rows('SELECT id,trans_type FROM '.TB_PREF.'approval_workflows WHERE id='.(int)$draft['workflow_id'].' FOR UPDATE');
            $owners = $number ? purch_requisition_reopen_rows('SELECT id FROM '.TB_PREF.'approval_drafts WHERE trans_type='.ST_PURCHREQUISITION
                .' AND reserved_trans_no='.$number.' ORDER BY id FOR UPDATE') : false;
            if (!$number || in_array($number, $reserved, true) || !$workflow || (int)$workflow[0]['trans_type'] !== ST_PURCHREQUISITION
                || !$owners || count($owners) !== 1 || (int)$owners[0]['id'] !== (int)$draft['id']) { cancel_transaction(); return false; }
            $reserved[] = $number;
        }
        if ($pending !== $expected || !$snapshot
            || ($drafts = purch_requisition_reopen_drafts($identity, (int)$snapshot['material']['request_id'], $approval)) === false
            || !register_transaction_resource_release('purchase-requisition-duplicate-cancel', 'release_purch_requisition_duplicate_cancel_permit')) {
            cancel_transaction(); return false;
        }
        $history = purch_requisition_reopen_history($drafts);
        $effects = purch_requisition_duplicate_effect_rows($expected, $reserved);
        $started = db_query('SELECT NOW()');
        $started = $started ? db_fetch_row($started) : false;
        if (!$history || !$effects || !$started || count($effects['numbers']) !== count($expected)) { cancel_transaction(); return false; }
        foreach ($effects['numbers'] as $number)
            if ($number['memo_'] !== 'APPROVAL_RESERVED') { cancel_transaction(); return false; }
        foreach ($expected as $draft_id) {
            $submissions = 0;
            foreach ($history['actions'] as $action) {
                if ((int)$action['draft_id'] !== $draft_id || $action['action_type'] !== 'submit') continue;
                if ((int)$action['user_id'] !== (int)$_SESSION['wa_current_user']->user || empty($action['action_date'])) {
                    cancel_transaction(); return false;
                }
                $submissions++;
            }
            if ($submissions !== 1) { cancel_transaction(); return false; }
        }
        $comments = 'Recover duplicate purchase requisition reviews: '.implode(',', $expected);
        foreach ($expected as $draft_id) {
            $draft = get_approval_draft_for_update($draft_id);
            $data = $draft ? $approval->decodeStoredDraftData($draft['draft_data']) : false;
            if (!$draft || !$data || purch_requisition_reopen_context() !== $context
                || (int)$_SESSION['wa_current_user']->access !== $role) { cancel_transaction(); return false; }
            purch_requisition_duplicate_cancel_permit('set', array('context'=>$context, 'role'=>$role,
                'draft'=>$draft, 'data'=>$data, 'comments'=>$comments, 'snapshot'=>$snapshot, 'history'=>$history));
            $result = $approval->cancel($draft_id, $comments);
            if (!is_array($result) || !isset($result['status']) || $result['status'] !== 'cancelled'
                || purch_requisition_duplicate_cancel_permit('take') !== null) { cancel_transaction(); return false; }
            // Core completion has independently reconciled exactly one terminal
            // transition/action against this history after notifications/reservations.
            $drafts = purch_requisition_reopen_drafts($identity, (int)$snapshot['material']['request_id'], $approval);
            $history = $drafts ? purch_requisition_reopen_history($drafts) : false;
            if (!$history || purch_requisition_reopen_source($identity, 'submitted', $data) !== $snapshot) { cancel_transaction(); return false; }
        }
        $cancelled_effects = purch_requisition_duplicate_effect_rows($expected, $reserved);
        $finished = db_query('SELECT NOW()');
        $finished = $finished ? db_fetch_row($finished) : false;
        if (!$cancelled_effects || !$finished || count($effects['notices']) !== count($cancelled_effects['notices'])
            || count($effects['numbers']) !== count($cancelled_effects['numbers'])) { cancel_transaction(); return false; }
        $normalized = $cancelled_effects;
        foreach ($normalized['numbers'] as $index=>$number) {
            if ($number['memo_'] !== 'APPROVAL_CANCELLED') { cancel_transaction(); return false; }
            $normalized['numbers'][$index]['memo_'] = $effects['numbers'][$index]['memo_'];
        }
        foreach ($normalized['notices'] as $index=>$notice) {
            if (empty($effects['notices'][$index]['is_read'])) {
                if ((int)$notice['is_read'] !== 1 || !is_string($notice['read_date'])
                    || strcmp($notice['read_date'], $started[0]) < 0 || strcmp($notice['read_date'], $finished[0]) > 0) { cancel_transaction(); return false; }
                foreach (array('is_read','read_date') as $field) $normalized['notices'][$index][$field] = $effects['notices'][$index][$field];
            }
        }
        if ($normalized !== $effects) { cancel_transaction(); return false; }
        if (!write_purch_requisition_reopen($snapshot, $context)) { cancel_transaction(); return false; }
        $result = submit_purch_requisition_to_core($identity);
        if (!is_array($result) || !isset($result['status'], $result['draft_id']) || $result['status'] !== 'pending'
            || in_array((int)$result['draft_id'], $expected, true)) { cancel_transaction(); return false; }
        $drafts = purch_requisition_reopen_drafts($identity, (int)$snapshot['material']['request_id'], $approval);
        $current = $drafts ? purch_requisition_reopen_history($drafts) : false;
        if (!$current || count($current['drafts']) !== count($history['drafts']) + 1
            || count($current['actions']) !== count($history['actions']) + 1) { cancel_transaction(); return false; }
        $successor = array_pop($current['drafts']);
        $action = array_pop($current['actions']);
        $after = purch_requisition_reopen_source($identity, 'submitted', $approval->decodeStoredDraftData($successor['draft_data']));
        if ($after) $after['material']['updated_at'] = $snapshot['material']['updated_at'];
        if ($current !== $history || $after !== $snapshot || (int)$successor['id'] !== (int)$result['draft_id']
            || (int)$action['draft_id'] !== (int)$result['draft_id'] || $action['action_type'] !== 'submit'
            || purch_requisition_reopen_context() !== $context || (int)$_SESSION['wa_current_user']->access !== $role) { cancel_transaction(); return false; }
        $history = purch_requisition_reopen_history($drafts);
        $comments .= ' to #'.(int)$result['draft_id'];
        $audit = db_query('SELECT '.db_escape($comments, true).', '.db_escape(isset($_SERVER['REMOTE_ADDR']) ? $_SERVER['REMOTE_ADDR'] : '').', NOW()');
        $audit = $audit ? db_fetch_row($audit) : false;
        if (!$history || !$audit) { cancel_transaction(); return false; }
        foreach (array_merge($expected, array((int)$result['draft_id'])) as $draft_id) {
            $id = add_approval_action($draft_id, 'resubmit', null, 0, $comments);
            $rows = $id ? purch_requisition_reopen_rows('SELECT * FROM '.TB_PREF.'approval_actions WHERE id='.(int)$id.' FOR UPDATE') : false;
            $finished = db_query('SELECT NOW()');
            $finished = $finished ? db_fetch_row($finished) : false;
            if (!$rows || !$finished || !purch_requisition_new_action_matches($rows[0], $draft_id, 'resubmit',
                $_SESSION['wa_current_user']->user, $audit[0], $audit[1], $audit[2], $finished[0])) { cancel_transaction(); return false; }
            $history['actions'][] = $rows[0];
        }
        $current = purch_requisition_reopen_drafts($identity, (int)$snapshot['material']['request_id'], $approval);
        $after = purch_requisition_reopen_source($identity, 'submitted', $approval->decodeStoredDraftData($successor['draft_data']));
        if ($after) $after['material']['updated_at'] = $snapshot['material']['updated_at'];
        if (!$current || purch_requisition_reopen_history($current) !== $history || $after !== $snapshot
            || purch_requisition_duplicate_effect_rows($expected, $reserved) !== $cancelled_effects
            || purch_requisition_reopen_context() !== $context || (int)$_SESSION['wa_current_user']->access !== $role
            || !commit_transaction()) { cancel_transaction(); return false; }
        return $result;
    } catch (Exception $exception) { cancel_transaction(); return false;
    } catch (Throwable $exception) { cancel_transaction(); return false; }
}

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
        $drafts = purch_requisition_reopen_drafts($identity, (int)$snapshot['material']['request_id'], $approval);
        $chain = $drafts ? purch_requisition_reopen_history($drafts) : false;
        $audit = db_query('SELECT '.db_escape($comment, true).', '.db_escape(isset($_SERVER['REMOTE_ADDR']) ? $_SERVER['REMOTE_ADDR'] : '').', NOW()');
        $audit = $audit ? db_fetch_row($audit) : false;
        if (!$chain || !$audit) { cancel_transaction(); return false; }
        foreach (array($expected, (int)$result['draft_id']) as $draft_id) {
            $action = add_approval_action($draft_id, 'resubmit', null, 0, $comment);
            $rows = $action ? purch_requisition_reopen_rows('SELECT * FROM '.TB_PREF.'approval_actions WHERE id='.(int)$action.' FOR UPDATE') : false;
            $finished = db_query('SELECT NOW()');
            $finished = $finished ? db_fetch_row($finished) : false;
            if (!$rows || !$finished || !purch_requisition_new_action_matches($rows[0], $draft_id, 'resubmit',
                $_SESSION['wa_current_user']->user, $audit[0], $audit[1], $audit[2], $finished[0])) { cancel_transaction(); return false; }
            $chain['actions'][] = $rows[0];
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
            || ($drafts = purch_requisition_reopen_drafts($identity, (int)$snapshot['material']['request_id'], $approval)) === false
            || purch_requisition_reopen_history($drafts) !== $chain
            || !commit_transaction()) { cancel_transaction(); return false; }
        return $result;
    } catch (Exception $exception) { cancel_transaction(); return false;
    } catch (Throwable $exception) { cancel_transaction(); return false; }
}
