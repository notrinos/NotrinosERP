<?php
/** Atomic native submission through the existing shared-core registration. */
require_once __DIR__.'/purch_requisition_approve_db.inc';

/** Keep submission ownership private to the current native transaction. */
function purch_requisition_submit_permit($operation, $value = null)
{
    static $permit = null;
    if ($operation === 'set') { $permit = $value; return true; }
    $saved = $permit;
    $permit = null;
    return $operation === 'take' ? $saved : true;
}

/** Discard submission admission on outer completion, failure or retry. */
function release_purch_requisition_submit_permit()
{
    return purch_requisition_submit_permit('clear');
}

/** Build review data from current locked intrinsic rows, without stale joins. */
function purch_requisition_locked_submission_data($snapshot)
{
    $data = $snapshot['header'];
    $data['requisition_id'] = (int)$data['id'];
    $data['requester_name'] = $data['department_name'] = $data['location_name'] = '';
    $data['custom_data'] = decode_purch_requisition_custom_data($data['custom_data']);
    $data['line_items'] = array();
    foreach ($snapshot['lines'] as $line) {
        $line['preferred_supplier_name'] = '';
        $data['line_items'][] = $line;
    }
    $data['requisition_source_sha256'] = hash('sha256', serialize($snapshot));
    return $data;
}

/** Accept only the native owner's exact current submission, once. */
function validate_purchase_requisition_submit_action($data, $actor)
{
    $context = purch_requisition_reopen_context();
    if ($context === false || empty($GLOBALS['transaction_level']) || !is_array($actor)
        || (int)$actor['user_id'] !== (int)$_SESSION['wa_current_user']->user
        || (int)$actor['role_id'] !== (int)$_SESSION['wa_current_user']->access) return false;
    $permit = purch_requisition_submit_permit('take');
    return $permit && $permit['context'] === $context && $permit['data'] === $data
        && $permit['role'] === (int)$actor['role_id']
        && purch_requisition_reopen_source((int)$data['requisition_id'], 'submitted', $data) === $permit['snapshot'];
}

/** Require a newly appended maker audit to match its command and DB time window. */
function purch_requisition_new_action_matches($row, $draft_id, $type, $actor, $comments, $ip, $started, $finished)
{
    return is_array($row) && (int)$row['id'] > 0 && (int)$row['draft_id'] === (int)$draft_id
        && $row['action_type'] === $type && (int)$row['user_id'] === (int)$actor
        && (int)$row['level'] === 0 && $row['comments'] === $comments
        && $row['ip_address'] === $ip && $row['draft_data_snapshot'] === null
        && is_string($row['action_date']) && strcmp($row['action_date'], $started) >= 0
        && strcmp($row['action_date'], $finished) <= 0;
}

/** Commit source, core intent, history, reservation and notifications together. */
function submit_purch_requisition_to_core($requisition_id)
{
    $context = purch_requisition_reopen_context();
    $identity = dimension_writer_reference_id($requisition_id);
    if ($context === false || !$identity || !class_exists('ApprovalWorkflow')
        || !ApprovalWorkflow::isRegistered(ST_PURCHREQUISITION)) return false;
    $role = (int)$_SESSION['wa_current_user']->access;
    try {
        if (!begin_transaction()) return false;
        $approval = get_approval_workflow_service();
        // Lock core custody before source rows, matching approval/cancel/reject.
        $drafts = purch_requisition_reopen_drafts($identity, 0, $approval);
        if ($drafts === false) { cancel_transaction(); return false; }
        foreach ($drafts as $draft)
            if (!in_array((int)$draft['status'], array(APPROVAL_STATUS_REJECTED, APPROVAL_STATUS_CANCELLED), true)) { cancel_transaction(); return false; }
        $history = $drafts ? purch_requisition_reopen_history($drafts) : array('drafts'=>array(), 'actions'=>array());
        if (!$history) { cancel_transaction(); return false; }
        $headers = purch_requisition_reopen_rows('SELECT * FROM '.TB_PREF.'purch_requisitions WHERE id='.$identity.' FOR UPDATE');
        // Rejected sources must first use the existing attributable core reopen.
        if (!$headers || $headers[0]['status'] !== 'draft') { cancel_transaction(); return false; }
        $source = dimension_writer_reference_id($headers[0]['material_request_id']);
        if (!$source || purch_requisition_reopen_drafts($identity, $source, $approval) === false) { cancel_transaction(); return false; }
        $lines = purch_requisition_reopen_rows('SELECT * FROM '.TB_PREF.'purch_requisition_lines WHERE requisition_id='.$identity.' ORDER BY id FOR UPDATE');
        if (!$lines) { cancel_transaction(); return false; }
        $data = purch_requisition_locked_submission_data(array('header'=>$headers[0], 'lines'=>$lines));
        $snapshot = purch_requisition_reopen_source($identity, 'draft', $data);
        $total = 0.0;
        foreach ($lines as $line) {
            if (!is_numeric($line['estimated_unit_price']) || !is_finite((float)$line['estimated_unit_price'])
                || (float)$line['estimated_unit_price'] < 0) { cancel_transaction(); return false; }
            $total += (float)$line['quantity'] * (float)$line['estimated_unit_price'];
        }
        $workflow = get_approval_workflow_by_type(ST_PURCHREQUISITION, true);
        if (!$snapshot || !is_finite($total) || !is_finite((float)$headers[0]['total_estimated'])
            || abs($total - (float)$headers[0]['total_estimated']) > 0.00000001
            || !$workflow || empty($workflow['is_active']) || !workflow_has_active_levels($workflow['id'], true)) {
            cancel_transaction(); return false;
        }
        // This paired family requires an independent core checker. Automatic
        // and direct approval remain unavailable until their full native gates.
        if (!db_query("UPDATE ".TB_PREF."wh_material_requests SET status='submitted',updated_at=NOW() WHERE request_id=".$source." AND status='draft'")
            || db_num_affected_rows() !== 1
            || !db_query("UPDATE ".TB_PREF."purch_requisitions SET status='submitted' WHERE id=".$identity." AND status='draft'")
            || db_num_affected_rows() !== 1) { cancel_transaction(); return false; }
        $data['status'] = 'submitted';
        $submitted = purch_requisition_reopen_source($identity, 'submitted', $data);
        $expected = $snapshot;
        $expected['header']['status'] = 'submitted';
        $expected['material']['status'] = 'submitted';
        if (!$submitted) { cancel_transaction(); return false; }
        $expected['material']['updated_at'] = $submitted['material']['updated_at'];
        if ($submitted !== $expected) { cancel_transaction(); return false; }
        $data = purch_requisition_locked_submission_data($submitted);
        // Match the core's existing SQL numeric-string boundary on both runtimes;
        // source DOUBLE totals may contain more digits than PHP's SQL conversion.
        $audit = db_query('SELECT '.db_escape('', true).', '.db_escape(isset($_SERVER['REMOTE_ADDR']) ? $_SERVER['REMOTE_ADDR'] : '')
            .', NOW(), '.db_escape((float)$submitted['header']['total_estimated']));
        $audit = $audit ? db_fetch_row($audit) : false;
        if (!$audit) { cancel_transaction(); return false; }
        if (!register_transaction_resource_release('purchase-requisition-submit', 'release_purch_requisition_submit_permit')) { cancel_transaction(); return false; }
        purch_requisition_submit_permit('set', array('context'=>$context,'role'=>$role,'data'=>$data,'snapshot'=>$submitted));
        $result = $approval->submit(ST_PURCHREQUISITION, $data, (float)$submitted['header']['total_estimated'], array(
            'summary'=>sprintf(_('Purchase Requisition %s'), $submitted['header']['reference']),
            'currency'=>get_company_pref('curr_default'), 'person_type'=>PT_MISC,
            'person_id'=>$submitted['header']['requester_id'], 'loc_code'=>$submitted['header']['location'],
            'requisition_id'=>$identity), true);
        if (!is_array($result) || !isset($result['status'], $result['draft_id']) || $result['status'] !== 'pending') { cancel_transaction(); return false; }
        $draft = get_approval_draft_for_update($result['draft_id']);
        if (!$draft || (int)$draft['status'] !== APPROVAL_STATUS_PENDING
            || (int)$draft['submitted_by'] !== (int)$_SESSION['wa_current_user']->user
            || (int)$draft['workflow_id'] !== (int)$workflow['id']
            || (int)$draft['workflow_version'] !== (int)$workflow['version']
            || (int)$draft['current_level'] !== 1 || $draft['completed_date'] !== null
            || $draft['approved_trans_no'] !== null
            || (float)$draft['amount'] !== (float)$audit[3]
            || $approval->decodeStoredDraftData($draft['draft_data']) !== json_decode(json_encode($data), true)) { cancel_transaction(); return false; }
        $current_drafts = purch_requisition_reopen_drafts($identity, $source, $approval);
        $current = $current_drafts ? purch_requisition_reopen_history($current_drafts) : false;
        $finished = db_query('SELECT NOW()');
        $finished = $finished ? db_fetch_row($finished) : false;
        if (!$current || !$finished || count($current['drafts']) !== count($history['drafts']) + 1
            || count($current['actions']) !== count($history['actions']) + 1) { cancel_transaction(); return false; }
        $retained = $current;
        $new_draft = array_pop($retained['drafts']);
        $new_action = array_pop($retained['actions']);
        if ($retained !== $history || $new_draft !== array_intersect_key($draft, $new_draft)
            || !purch_requisition_new_action_matches($new_action, $result['draft_id'], 'submit',
                $_SESSION['wa_current_user']->user, $audit[0], $audit[1], $audit[2], $finished[0])) { cancel_transaction(); return false; }
        $hook_identity = $identity;
        hook_invoke_all('purch_requisition_submitted', $hook_identity);
        if (purch_requisition_reopen_context() !== $context || (int)$_SESSION['wa_current_user']->access !== $role
            || purch_requisition_reopen_source($identity, 'submitted', $data) !== $submitted
            || get_approval_draft_for_update($result['draft_id']) !== $draft
            || ($current_drafts = purch_requisition_reopen_drafts($identity, $source, $approval)) === false
            || purch_requisition_reopen_history($current_drafts) !== $current
            || !commit_transaction()) { cancel_transaction(); return false; }
        return $result;
    } catch (Exception $exception) { cancel_transaction(); return false;
    } catch (Throwable $exception) { cancel_transaction(); return false; }
}
