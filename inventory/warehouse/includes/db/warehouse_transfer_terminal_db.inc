<?php
/** Native unshipped Transfer Order cancellation, retaining original core decisions. */
require_once __DIR__.'/warehouse_transfer_core_db.inc';

/**
 * Verify the actual terminal core approval receipt, never an Approved status alone.
 * Historical/status-only approvals cannot acquire this authority by inference.
 * Must be called under the writer's owning transaction.
 */
function transfer_approved_custody($identity)
{
    if (!transfer_core_context() || empty($GLOBALS['transaction_level'])
        || !($identity = dimension_writer_reference_id($identity))) return false;
    $history = transfer_core_history($identity);
    $source = transfer_core_source($identity, 'approved', false, true);
    return transfer_approved_history_custody($identity, $source, $history);
}

/**
 * Internal receipt comparison for an already locked source/history projection.
 * Callers must load current rows or reconstruct only receipt-bound transitions;
 * this comparison alone grants no command admission or ordinary writer authority.
 */
function transfer_approved_history_custody($identity, $source, $history)
{
    $origin = transfer_review_material_origin($identity);
    if ($origin === false) return false;
    return transfer_approved_projection_custody($identity, $source, $history, $origin);
}

/**
 * Compare a receipt-bound approved projection with an independently proved origin.
 * This internal comparison neither loads current source nor admits any command.
 * Historical callers must prove their reconstructed source and origin separately.
 */
function transfer_approved_projection_custody($identity, $source, $history, $origin)
{
    if (!transfer_core_context() || empty($GLOBALS['transaction_level'])
        || !($identity = dimension_writer_reference_id($identity))) return false;
    if (($origin !== null && (!is_array($origin) || !material_core_context()))
        || !is_array($source) || !isset($source['header'], $source['lines'])
        || !is_array($source['header']) || !is_array($source['lines']) || !$source['lines']
        || !is_array($history) || !isset($history['drafts'], $history['actions'])
        || !is_array($history['drafts']) || !is_array($history['actions'])) return false;
    foreach (array('requested_by','approved_by') as $field)
        if (!array_key_exists($field, $source['header'])) return false;
    foreach ($source['lines'] as $row) if (!is_array($row)) return false;
    foreach ($history['drafts'] as $row) {
        if (!is_array($row)) return false;
        foreach (array('id','status','workflow_id','submitted_by','draft_data','approved_trans_no',
            'completed_date','current_level') as $field) if (!array_key_exists($field, $row)) return false;
    }
    foreach ($history['actions'] as $row) {
        if (!is_array($row)) return false;
        foreach (array('draft_id','action_type','user_id','level','draft_data_snapshot') as $field)
            if (!array_key_exists($field, $row)) return false;
    }
    if (!$history || !$source || !isset($source['header']['transfer_id'], $source['header']['status'])
        || (int)$source['header']['transfer_id'] !== $identity || $source['header']['status'] !== 'approved'
        || empty($source['header']['approved_by'])
        || (int)$source['header']['approved_by'] === (int)$source['header']['requested_by']) return false;
    $approved = array(); $approval = get_approval_workflow_service();
    foreach ($history['drafts'] as $draft) {
        $workflow = transfer_core_rows('SELECT trans_type FROM '.TB_PREF.'approval_workflows WHERE id='.(int)$draft['workflow_id'].' FOR UPDATE');
        $data = $approval->decodeStoredDraftData($draft['draft_data']);
        if (!$workflow || count($workflow) !== 1 || (int)$workflow[0]['trans_type'] !== ST_WH_TRANSFER_ORDER
            || !is_array($data) || !isset($data['transfer_company_sha256'], $data['transfer_source_sha256'])
            || $data['transfer_company_sha256'] !== transfer_core_context(true)
            || (int)$draft['submitted_by'] !== (int)$source['header']['requested_by']) return false;
        if ((int)$draft['status'] === APPROVAL_STATUS_APPROVED) $approved[] = $draft;
        elseif (!in_array((int)$draft['status'], array(APPROVAL_STATUS_REJECTED, APPROVAL_STATUS_CANCELLED), true)) return false;
    }
    if (count($approved) !== 1) return false;
    $draft = $approved[0]; $data = $approval->decodeStoredDraftData($draft['draft_data']);
    if (($origin === null ? array_key_exists('transfer_material_origin', $data)
        : (!is_array($origin) || !isset($data['transfer_material_origin']) || $data['transfer_material_origin'] !== $origin))
        || !isset($data['transfer_id']) || (int)$data['transfer_id'] !== $identity
        || (int)$draft['approved_trans_no'] !== $identity || !$draft['completed_date']) return false;
    $actions = $history['actions']; $last = array_pop($actions);
    if (!$last || (int)$last['draft_id'] !== (int)$draft['id'] || $last['action_type'] !== 'approve'
        || (int)$last['user_id'] !== (int)$source['header']['approved_by']
        || (int)$last['level'] !== (int)$draft['current_level'] || $last['draft_data_snapshot'] === null) return false;
    $receipt = $approval->decodeStoredDraftData($last['draft_data_snapshot']);
    $unsigned = $last; $unsigned['draft_data_snapshot'] = null;
    $expected = array('schema'=>'transfer-approved-source-v1', 'transfer_id'=>$identity,
        'draft_id'=>(int)$draft['id'], 'company_sha256'=>transfer_core_context(true),
        'submitted_source_sha256'=>$data['transfer_source_sha256'],
        'history_sha256'=>transfer_core_source_fingerprint(array('header'=>array(),
            'lines'=>array_merge($history['drafts'], $actions))),
        'action_sha256'=>transfer_core_source_fingerprint(array('header'=>$unsigned, 'lines'=>array())),
        'source_sha256'=>transfer_core_source_fingerprint($source));
    if ($receipt !== $expected) return false;
    return array('source'=>$source, 'history'=>$history, 'draft'=>$draft);
}

/**
 * Verify a current linked cancellation without mutating or normalizing history.
 * v2 preserves the overwritten approved timestamp; v1 cannot be reconstructed
 * safely and remains readable as raw history only. The owning transaction keeps
 * Material-before-Transfer locks until its caller finishes. No release, edit,
 * approval, replacement or financial fulfillment authority follows this read.
 */
function transfer_cancelled_linked_custody($transfer_id)
{
    $identity = dimension_writer_reference_id($transfer_id);
    if (!$identity || !transfer_core_context() || empty($GLOBALS['transaction_level'])) return false;
    $history = transfer_core_history($identity);
    if (!$history || !$history['actions']) return false;
    $origin = transfer_review_material_origin($identity);
    if (!is_array($origin)) return false;
    $headers = transfer_core_rows('SELECT * FROM '.TB_PREF.'wh_transfer_orders WHERE transfer_id='.$identity.' FOR UPDATE');
    $lines = transfer_core_rows('SELECT * FROM '.TB_PREF.'wh_transfer_order_lines WHERE transfer_id='.$identity.' ORDER BY line_id FOR UPDATE');
    if (!$headers || count($headers) !== 1 || !$lines) return false;
    return transfer_cancelled_projection_custody($identity,
        array('header'=>$headers[0], 'lines'=>$lines), $history, $origin);
}

/**
 * Compare exact cancelled history against an independently reconstructed origin.
 * No current-link lookup or state mutation occurs. A future release command must
 * prove its own transition before using these historical comparison results.
 */
function transfer_cancelled_projection_custody($identity, $source, $history, $origin)
{
    if (!transfer_core_context() || !material_core_context() || empty($GLOBALS['transaction_level'])
        || !($identity = dimension_writer_reference_id($identity)) || !is_array($origin)
        || !is_array($source) || !isset($source['header'], $source['lines'])
        || !is_array($source['header']) || !is_array($source['lines']) || !$source['lines']
        || !is_array($history) || !isset($history['drafts'], $history['actions'])
        || !is_array($history['drafts']) || !is_array($history['actions']) || !$history['actions']) return false;
    foreach (array('transfer_id','status','updated_at','requested_by') as $field)
        if (!array_key_exists($field, $source['header'])) return false;
    if (dimension_writer_reference_id($source['header']['transfer_id']) !== $identity) return false;
    foreach ($source['lines'] as $row) if (!is_array($row)) return false;
    $prior = $history; $action = array_pop($prior['actions']);
    if (!is_array($action)) return false;
    foreach (array('draft_id','action_type','user_id','level','draft_data_snapshot','action_date','ip_address') as $field)
        if (!array_key_exists($field, $action)) return false;
    $approval = get_approval_workflow_service();
    $receipt = $action['draft_data_snapshot'] === null ? false
        : $approval->decodeStoredDraftData($action['draft_data_snapshot']);
    if (!is_array($receipt) || array_keys($receipt) !== array('schema','transfer_id','draft_id',
        'company_sha256','approved_source_sha256','source_sha256','approved_updated_at','material_origin')
        || $receipt['schema'] !== 'transfer-source-cancel-v2'
        || $receipt['transfer_id'] !== $identity
        || $receipt['company_sha256'] !== transfer_core_context(true)
        || ($receipt['approved_updated_at'] !== null && !is_string($receipt['approved_updated_at']))
        || $action['action_type'] !== 'source_cancel') return false;
    if ($origin !== $receipt['material_origin']) return false;
    $clock = transfer_core_rows('SELECT NOW() AS now_');
    if (!$clock
        || $source['header']['status'] !== 'cancelled' || !$source['header']['updated_at']
        || !$action['action_date'] || $action['action_date'] < $source['header']['updated_at']
        || $action['action_date'] > $clock[0]['now_'] || $action['ip_address'] === null
        || (int)$action['user_id'] !== (int)$source['header']['requested_by']) return false;
    $approved = $source;
    $approved['header']['status'] = 'approved';
    $approved['header']['updated_at'] = $receipt['approved_updated_at'];
    $custody = transfer_approved_projection_custody($identity, $approved, $prior, $origin);
    if (!$custody || $receipt['draft_id'] !== (int)$custody['draft']['id']
        || (int)$action['draft_id'] !== (int)$custody['draft']['id']
        || (int)$action['level'] !== (int)$custody['draft']['current_level']
        || $action['action_date'] < $custody['draft']['completed_date']
        || $receipt['approved_source_sha256'] !== transfer_core_source_fingerprint($approved)
        || $receipt['source_sha256'] !== transfer_core_source_fingerprint($source)) return false;
    return array('source'=>$source, 'approved'=>$custody, 'history'=>$history,
        'action'=>$action, 'receipt'=>$receipt, 'material_origin'=>$origin);
}

/** Unreviewed, original-maker Draft cancellation with complete source/history custody. */
function cancel_native_transfer_draft($transfer_id)
{
    $identity = dimension_writer_reference_id($transfer_id); $context = transfer_core_context();
    if (!$identity || !$context) return false;
    try {
        if (!begin_transaction()) return false;
        $history = transfer_core_history($identity);
        $source = transfer_core_source($identity, 'draft', true);
        if (!$history || $history['drafts'] || $history['actions'] || !$source
            || (int)$source['header']['requested_by'] !== (int)$_SESSION['wa_current_user']->user
            || $source['header']['approved_by'] !== null || $source['header']['approval_date'] !== null) {
            cancel_transaction(); return false;
        }
        $clock = transfer_core_rows('SELECT NOW() AS now_');
        if (!$clock || !db_query('UPDATE '.TB_PREF."wh_transfer_orders SET status='cancelled',updated_at=NOW() WHERE transfer_id="
            .$identity." AND status='draft'") || db_num_affected_rows() !== 1) { cancel_transaction(); return false; }
        $finished = transfer_core_source($identity, 'cancelled', true);
        $now = transfer_core_rows('SELECT NOW() AS now_'); $expected = $source;
        if (!$finished || !$now || !$finished['header']['updated_at']
            || $finished['header']['updated_at'] < $clock[0]['now_']
            || $finished['header']['updated_at'] > $now[0]['now_']) { cancel_transaction(); return false; }
        $expected['header']['status'] = 'cancelled';
        $expected['header']['updated_at'] = $finished['header']['updated_at'];
        if ($finished !== $expected || transfer_core_history($identity) !== $history
            || transfer_core_context() !== $context || !commit_transaction()) { cancel_transaction(); return false; }
        return true;
    } catch (Exception $exception) { cancel_transaction(); return false;
    } catch (Throwable $exception) { cancel_transaction(); return false; }
}

/**
 * Read a cancellation result against its exact admitted approved source.
 * This grants no ordinary writer or fulfillment authority to a linked source.
 */
function transfer_cancelled_source_matches($identity, $approved, $started)
{
    if (!transfer_core_context() || empty($GLOBALS['transaction_level'])
        || !($identity = dimension_writer_reference_id($identity))
        || !is_array($approved) || !isset($approved['header']['transfer_id'], $approved['header']['status'], $approved['lines'])
        || (int)$approved['header']['transfer_id'] !== $identity || $approved['header']['status'] !== 'approved'
        || !is_string($started)) return false;
    $headers = transfer_core_rows('SELECT * FROM '.TB_PREF.'wh_transfer_orders WHERE transfer_id='.$identity.' FOR UPDATE');
    $lines = transfer_core_rows('SELECT * FROM '.TB_PREF.'wh_transfer_order_lines WHERE transfer_id='.$identity.' ORDER BY line_id FOR UPDATE');
    $clock = transfer_core_rows('SELECT NOW() AS now_');
    if (!$headers || count($headers) !== 1 || $lines === false || !$clock
        || !$headers[0]['updated_at'] || $headers[0]['updated_at'] < $started
        || $headers[0]['updated_at'] > $clock[0]['now_']) return false;
    $expected = $approved;
    $expected['header']['status'] = 'cancelled';
    $expected['header']['updated_at'] = $headers[0]['updated_at'];
    $source = array('header'=>$headers[0], 'lines'=>$lines);
    return $source === $expected ? $source : false;
}

/**
 * Original maker may cancel an unshipped, unconsumed independently approved TO.
 * Linked v2 origin must remain exact; cancellation does not release Material demand.
 * The approved decision is immutable. Record a separately attributable cancel action.
 */
function cancel_native_transfer_approved($transfer_id, $comments = '')
{
    $identity = dimension_writer_reference_id($transfer_id); $context = transfer_core_context();
    if (!$identity || !$context || !is_string($comments) || strpos($comments, "\0") !== false) return false;
    try {
        if (!begin_transaction()) return false;
        $custody = transfer_approved_custody($identity);
        if (!$custody || (int)$custody['source']['header']['requested_by'] !== (int)$_SESSION['wa_current_user']->user) {
            cancel_transaction(); return false;
        }
        $origin = transfer_review_material_origin($identity);
        if ($origin === false) { cancel_transaction(); return false; }
        $clock = transfer_core_rows('SELECT NOW() AS now_');
        if (!$clock || !db_query('UPDATE '.TB_PREF."wh_transfer_orders SET status='cancelled',updated_at=NOW() WHERE transfer_id="
            .$identity." AND status='approved'") || db_num_affected_rows() !== 1) { cancel_transaction(); return false; }
        $source = transfer_cancelled_source_matches($identity, $custody['source'], $clock[0]['now_']);
        if (!$source) { cancel_transaction(); return false; }
        $receipt = array('schema'=>'transfer-source-cancel-v1', 'transfer_id'=>$identity,
            'draft_id'=>(int)$custody['draft']['id'], 'company_sha256'=>transfer_core_context(true),
            'approved_source_sha256'=>transfer_core_source_fingerprint($custody['source']),
            'source_sha256'=>transfer_core_source_fingerprint($source));
        if ($origin !== null) {
            // Preserve the overwritten value exactly, including SQL NULL, so
            // a later recovery reader never guesses its approved timestamp.
            $receipt['schema'] = 'transfer-source-cancel-v2';
            $receipt['approved_updated_at'] = $custody['source']['header']['updated_at'];
            $receipt['material_origin'] = $origin;
        }
        $snapshot = json_encode($receipt);
        if ($snapshot === false) { cancel_transaction(); return false; }
        $action_id = add_approval_action($custody['draft']['id'], 'source_cancel', null,
            $custody['draft']['current_level'], $comments, $snapshot, null);
        if (!$action_id) { cancel_transaction(); return false; }
        $history = transfer_core_history($identity);
        if (!$history || count($history['drafts']) !== count($custody['history']['drafts'])
            || count($history['actions']) !== count($custody['history']['actions'])+1) { cancel_transaction(); return false; }
        $new = array_pop($history['actions']);
        $literal = transfer_core_rows('SELECT '.db_escape($comments, true).' AS comments,'.db_escape($snapshot, true).' AS snapshot_,'
            .db_escape(isset($_SERVER['REMOTE_ADDR']) ? $_SERVER['REMOTE_ADDR'] : '').' AS ip,NOW() AS now_');
        if (!$literal || $history['drafts'] !== $custody['history']['drafts']
            || $history['actions'] !== $custody['history']['actions']
            || (int)$new['id'] !== (int)$action_id || (int)$new['draft_id'] !== (int)$custody['draft']['id']
            || $new['action_type'] !== 'source_cancel'
            || (int)$new['user_id'] !== (int)$_SESSION['wa_current_user']->user
            || (int)$new['level'] !== (int)$custody['draft']['current_level']
            || $new['comments'] !== $literal[0]['comments'] || $new['draft_data_snapshot'] !== $literal[0]['snapshot_']
            || $new['ip_address'] !== $literal[0]['ip'] || $new['action_date'] < $clock[0]['now_']
            || $new['action_date'] > $literal[0]['now_']
            || transfer_cancelled_source_matches($identity, $custody['source'], $clock[0]['now_']) !== $source
            || transfer_review_material_origin($identity) !== $origin
            || ($origin !== null && !transfer_cancelled_linked_custody($identity))
            || transfer_core_context() !== $context || !commit_transaction()) { cancel_transaction(); return false; }
        return true;
    } catch (Exception $exception) { cancel_transaction(); return false;
    } catch (Throwable $exception) { cancel_transaction(); return false; }
}
