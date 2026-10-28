<?php
/** Approved standalone source custody; historical approval bytes stay immutable. */
require_once __DIR__.'/warehouse_material_core_db.inc';

/**
 * Resolve current committed terminal custody through the real core decoder.
 * Legacy approved sources without a receipt require attributable owner recovery;
 * no fingerprint is inferred from the legitimately changed approval columns.
 */
function material_approved_custody($identity)
{
    if (!material_core_context() || empty($GLOBALS['transaction_level'])
        || !dimension_writer_reference_id($identity)) return false;
    $history = material_core_history((int)$identity);
    $source = material_core_source((int)$identity, 'approved');
    if (!$history || !$source || empty($source['header']['approved_by'])
        || (int)$source['header']['approved_by'] === (int)$source['header']['requested_by']) return false;
    $approved = array(); $approval = get_approval_workflow_service();
    foreach ($history['drafts'] as $draft) {
        $workflow = material_core_rows('SELECT trans_type FROM '.TB_PREF.'approval_workflows WHERE id='.(int)$draft['workflow_id'].' FOR UPDATE');
        $data = $approval->decodeStoredDraftData($draft['draft_data']);
        if (!$workflow || (int)$workflow[0]['trans_type'] !== ST_WH_MATERIAL_REQUEST
            || (int)$draft['submitted_by'] !== (int)$source['header']['requested_by']
            || (isset($data['material_company_sha256']) && $data['material_company_sha256'] !== material_core_context(true))) return false;
        if ((int)$draft['status'] === APPROVAL_STATUS_APPROVED) $approved[] = $draft;
        elseif (!in_array((int)$draft['status'], array(APPROVAL_STATUS_REJECTED, APPROVAL_STATUS_CANCELLED), true)) return false;
    }
    if (count($approved) !== 1) return false;
    $draft = $approved[0]; $data = $approval->decodeStoredDraftData($draft['draft_data']);
    if ((int)$draft['approved_trans_no'] !== (int)$identity || !$draft['completed_date']
        || !isset($data['material_company_sha256'], $data['material_source_sha256'])
        || $data['material_company_sha256'] !== material_core_context(true)) return false;
    $actions = $history['actions']; $last = array_pop($actions);
    if (!$last || (int)$last['draft_id'] !== (int)$draft['id'] || $last['action_type'] !== 'approve'
        || (int)$last['user_id'] !== (int)$source['header']['approved_by']
        || (int)$last['level'] !== (int)$draft['current_level'] || $last['draft_data_snapshot'] === null) return false;
    $receipt = $approval->decodeStoredDraftData($last['draft_data_snapshot']);
    $unsigned = $last; $unsigned['draft_data_snapshot'] = null;
    $expected = array('schema'=>'material-approved-source-v1', 'request_id'=>(int)$identity,
        'draft_id'=>(int)$draft['id'], 'company_sha256'=>material_core_context(true),
        'submitted_source_sha256'=>$data['material_source_sha256'],
        'history_sha256'=>material_core_source_fingerprint(array('header'=>array(), 'lines'=>array_merge($history['drafts'], $actions))),
        'action_sha256'=>material_core_source_fingerprint(array('header'=>$unsigned, 'lines'=>array())),
        'source_sha256'=>material_core_source_fingerprint($source));
    if ($receipt !== $expected) return false;
    $ids = array();
    foreach ($source['lines'] as $line) $ids[] = (int)$line['line_id'];
    $links = material_core_rows('SELECT id FROM '.TB_PREF.'purch_requisition_lines WHERE material_request_line_id IN ('
        .implode(',', $ids).') ORDER BY id FOR UPDATE');
    if ($links !== array()) return false;
    return array('source'=>$source, 'history'=>$history, 'draft'=>$draft);
}

/**
 * Original maker cancels only exact approved, unconsumed standalone demand.
 * Preserve the approved decision and append an attributable core-history event;
 * SQL, source/history drift or authority failure unwinds the whole caller command.
 */
function cancel_standalone_material_approved($request_id, $comments = '')
{
    $context = material_core_context(); $identity = dimension_writer_reference_id($request_id);
    if (!$context || !$identity || !is_string($comments) || strpos($comments, "\0") !== false) return false;
    try {
        if (!begin_transaction()) return false;
        $custody = material_approved_custody($identity);
        if (!$custody || (int)$custody['source']['header']['requested_by'] !== (int)$_SESSION['wa_current_user']->user) {
            cancel_transaction(); return false;
        }
        $clock = material_core_rows('SELECT NOW() AS now_');
        if (!$clock || !db_query('UPDATE '.TB_PREF."wh_material_requests SET status='cancelled',updated_at=NOW() WHERE request_id="
            .$identity." AND status='approved'") || db_num_affected_rows() !== 1) { cancel_transaction(); return false; }
        $source = material_core_source($identity, 'cancelled');
        $now = material_core_rows('SELECT NOW() AS now_'); $expected = $custody['source'];
        if (!$source || !$now || !$source['header']['updated_at'] || $source['header']['updated_at'] < $clock[0]['now_']
            || $source['header']['updated_at'] > $now[0]['now_']) { cancel_transaction(); return false; }
        $expected['header']['status'] = 'cancelled'; $expected['header']['updated_at'] = $source['header']['updated_at'];
        if ($source !== $expected) { cancel_transaction(); return false; }
        $snapshot = json_encode(array('schema'=>'material-source-cancel-v1', 'request_id'=>$identity,
            'draft_id'=>(int)$custody['draft']['id'], 'company_sha256'=>material_core_context(true),
            'approved_source_sha256'=>material_core_source_fingerprint($custody['source']),
            'source_sha256'=>material_core_source_fingerprint($source)));
        if ($snapshot === false) { cancel_transaction(); return false; }
        $action_id = add_approval_action($custody['draft']['id'], 'source_cancel', null, $custody['draft']['current_level'], $comments, $snapshot, null);
        if (!$action_id) { cancel_transaction(); return false; }
        $history = material_core_history($identity);
        if (!$history || count($history['actions']) !== count($custody['history']['actions'])+1) { cancel_transaction(); return false; }
        $new = array_pop($history['actions']);
        $literal = material_core_rows('SELECT '.db_escape($comments, true).' AS comments,'.db_escape($snapshot, true).' AS snapshot_,'
            .db_escape(isset($_SERVER['REMOTE_ADDR']) ? $_SERVER['REMOTE_ADDR'] : '').' AS ip,NOW() AS now_');
        $ids = array(); foreach ($source['lines'] as $line) $ids[] = (int)$line['line_id'];
        $links = material_core_rows('SELECT id FROM '.TB_PREF.'purch_requisition_lines WHERE material_request_line_id IN ('
            .implode(',', $ids).') ORDER BY id FOR UPDATE');
        if (!$literal || $history !== $custody['history'] || (int)$new['id'] !== (int)$action_id
            || (int)$new['draft_id'] !== (int)$custody['draft']['id'] || $new['action_type'] !== 'source_cancel'
            || (int)$new['user_id'] !== (int)$_SESSION['wa_current_user']->user
            || (int)$new['level'] !== (int)$custody['draft']['current_level']
            || $new['comments'] !== $literal[0]['comments'] || $new['draft_data_snapshot'] !== $literal[0]['snapshot_']
            || $new['ip_address'] !== $literal[0]['ip'] || $new['action_date'] < $clock[0]['now_'] || $new['action_date'] > $literal[0]['now_']
            || $links !== array() || material_core_source($identity, 'cancelled') !== $source
            || material_core_context() !== $context || !commit_transaction()) { cancel_transaction(); return false; }
        return true;
    } catch (Exception $exception) { cancel_transaction(); return false;
    } catch (Throwable $exception) { cancel_transaction(); return false; }
}
