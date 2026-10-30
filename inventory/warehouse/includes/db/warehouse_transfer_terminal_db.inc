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
    $source = transfer_core_source($identity, 'approved');
    if (!$history || !$source || empty($source['header']['approved_by'])
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
    if (!isset($data['transfer_id']) || (int)$data['transfer_id'] !== $identity
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
 * Original maker may cancel only an unshipped, unconsumed approved standalone TO.
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
        $clock = transfer_core_rows('SELECT NOW() AS now_');
        if (!$clock || !db_query('UPDATE '.TB_PREF."wh_transfer_orders SET status='cancelled',updated_at=NOW() WHERE transfer_id="
            .$identity." AND status='approved'") || db_num_affected_rows() !== 1) { cancel_transaction(); return false; }
        $source = transfer_core_source($identity, 'cancelled');
        $now = transfer_core_rows('SELECT NOW() AS now_'); $expected = $custody['source'];
        if (!$source || !$now || !$source['header']['updated_at']
            || $source['header']['updated_at'] < $clock[0]['now_']
            || $source['header']['updated_at'] > $now[0]['now_']) { cancel_transaction(); return false; }
        $expected['header']['status'] = 'cancelled'; $expected['header']['updated_at'] = $source['header']['updated_at'];
        if ($source !== $expected) { cancel_transaction(); return false; }
        $snapshot = json_encode(array('schema'=>'transfer-source-cancel-v1', 'transfer_id'=>$identity,
            'draft_id'=>(int)$custody['draft']['id'], 'company_sha256'=>transfer_core_context(true),
            'approved_source_sha256'=>transfer_core_source_fingerprint($custody['source']),
            'source_sha256'=>transfer_core_source_fingerprint($source)));
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
            || transfer_core_source($identity, 'cancelled') !== $source
            || transfer_core_context() !== $context || !commit_transaction()) { cancel_transaction(); return false; }
        return true;
    } catch (Exception $exception) { cancel_transaction(); return false;
    } catch (Throwable $exception) { cancel_transaction(); return false; }
}
