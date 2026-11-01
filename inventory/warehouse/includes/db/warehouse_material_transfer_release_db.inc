<?php
/** Attributable release of unconsumed demand after independent Transfer cancellation. */
require_once __DIR__.'/warehouse_material_transfer_link_db.inc';
require_once __DIR__.'/warehouse_transfer_terminal_db.inc';

/** Fingerprint complete immutable review rows using the native canonical representation. */
function material_transfer_release_history_hash($history)
{
    return material_core_source_fingerprint(array('header'=>array(),
        'lines'=>array_merge($history['drafts'], $history['actions'])));
}

/**
 * Read only a current, receipt-bound released source. Restore the former ordered
 * projection from its retained transition, then independently prove both original
 * approvals and cancellation. No supplied snapshots or current-link inference.
 * Replacement, editing, deletion and financial fulfillment remain separate gates.
 */
function material_released_transfer_custody($request_id)
{
    $identity = dimension_writer_reference_id($request_id);
    if (!$identity || !material_core_context() || !transfer_core_context()
        || empty($GLOBALS['transaction_level'])) return false;
    $history = material_core_history($identity);
    if (!$history || !$history['actions']) return false;
    $last = $history['actions'][count($history['actions'])-1];
    $approval = get_approval_workflow_service();
    $receipt = $last['draft_data_snapshot'] === null ? false
        : $approval->decodeStoredDraftData($last['draft_data_snapshot']);
    if (!is_array($receipt) || !isset($receipt['transfer_id'])
        || !($transfer_id = dimension_writer_reference_id($receipt['transfer_id']))) return false;
    $source = material_core_source($identity, 'approved');
    if (!$source || $source['header']['linked_doc_type'] !== null || $source['header']['linked_doc_no'] !== null
        || $source['header']['request_type'] !== 'transfer') return false;
    $ids = array(); foreach ($source['lines'] as $line) $ids[] = (int)$line['line_id'];
    $pairs = material_core_rows('SELECT id FROM '.TB_PREF.'purch_requisition_lines WHERE material_request_line_id IN ('
        .implode(',', $ids).') ORDER BY id FOR UPDATE');
    $to_history = transfer_core_history($transfer_id);
    $headers = transfer_core_rows('SELECT * FROM '.TB_PREF.'wh_transfer_orders WHERE transfer_id='.$transfer_id.' FOR UPDATE');
    $lines = transfer_core_rows('SELECT * FROM '.TB_PREF.'wh_transfer_order_lines WHERE transfer_id='.$transfer_id.' ORDER BY line_id FOR UPDATE');
    $links = material_core_rows('SELECT request_id FROM '.TB_PREF.'wh_material_requests WHERE linked_doc_type='
        .ST_LOCTRANSFER.' AND linked_doc_no='.$transfer_id.' ORDER BY request_id FOR UPDATE');
    if ($pairs !== array() || !$to_history || !$headers || count($headers) !== 1 || !$lines || $links !== array()) return false;
    $transfer = array('header'=>$headers[0], 'lines'=>$lines);
    return material_released_transfer_projection($identity, $source, $history, $transfer, $to_history);
}

/**
 * Compare an exact receipt-bound historical release independently of current links.
 * Callers must load/lock complete persisted rows and prove any reconstruction.
 * This comparison grants no reuse, replacement, approval or financial authority.
 */
function material_released_transfer_projection($request_id, $source, $history, $transfer, $to_history)
{
    $identity = dimension_writer_reference_id($request_id);
    if (!$identity || !material_core_context() || !transfer_core_context()
        || empty($GLOBALS['transaction_level'])) return false;
    foreach (array($source, $transfer) as $rows) {
        if (!is_array($rows) || !isset($rows['header'], $rows['lines'])
            || !is_array($rows['header']) || !is_array($rows['lines']) || !$rows['lines']) return false;
        foreach ($rows['lines'] as $line) if (!is_array($line)) return false;
    }
    foreach (array($history, $to_history) as $rows) {
        if (!is_array($rows) || !isset($rows['drafts'], $rows['actions'])
            || !is_array($rows['drafts']) || !is_array($rows['actions'])) return false;
        foreach (array_merge($rows['drafts'], $rows['actions']) as $row) if (!is_array($row)) return false;
    }
    foreach (array('request_id','status','request_type','linked_doc_type','linked_doc_no','requested_by','updated_at') as $field)
        if (!array_key_exists($field, $source['header'])) return false;
    foreach ($history['actions'] as $row)
        foreach (array('id','draft_id','action_type','user_id','level','draft_data_snapshot','action_date','ip_address') as $field)
            if (!array_key_exists($field, $row)) return false;
    if (dimension_writer_reference_id($source['header']['request_id']) !== $identity
        || $source['header']['status'] !== 'approved' || $source['header']['request_type'] !== 'transfer'
        || $source['header']['linked_doc_type'] !== null || $source['header']['linked_doc_no'] !== null) return false;
    if (!$history['actions']) return false;
    $prior = $history; $action = array_pop($prior['actions']);
    $approval = get_approval_workflow_service();
    $receipt = $action['draft_data_snapshot'] === null ? false
        : $approval->decodeStoredDraftData($action['draft_data_snapshot']);
    if (!is_array($receipt) || array_keys($receipt) !== array('schema','request_id','transfer_id',
        'draft_id','material_company_sha256','transfer_company_sha256','ordered_transition',
        'ordered_source_sha256','source_sha256','history_sha256','transfer_source_sha256',
        'transfer_history_sha256','action_sha256')
        || $receipt['schema'] !== 'material-transfer-release-v1' || $receipt['request_id'] !== $identity
        || !($transfer_id = dimension_writer_reference_id($receipt['transfer_id']))
        || $receipt['material_company_sha256'] !== material_core_context(true)
        || $receipt['transfer_company_sha256'] !== transfer_core_context(true)
        || !is_array($receipt['ordered_transition'])
        || array_keys($receipt['ordered_transition']) !== array('status','linked_doc_type','linked_doc_no','updated_at')
        || $receipt['ordered_transition']['status'] !== 'ordered'
        || (int)$receipt['ordered_transition']['linked_doc_type'] !== ST_LOCTRANSFER
        || dimension_writer_reference_id($receipt['ordered_transition']['linked_doc_no']) !== $transfer_id
        || !is_string($receipt['ordered_transition']['updated_at'])
        || !preg_match('/^[0-9]{4}-[0-9]{2}-[0-9]{2} [0-9]{2}:[0-9]{2}:[0-9]{2}$/D', $receipt['ordered_transition']['updated_at'])
        || $action['action_type'] !== 'source_release') return false;
    $ordered = $source;
    foreach ($receipt['ordered_transition'] as $field=>$value) $ordered['header'][$field] = $value;
    $origin = material_transfer_origin_projection($identity, $ordered, $prior, $transfer, $to_history);
    $binding = $origin ? material_transfer_origin_binding($origin) : false;
    $cancelled = $binding ? transfer_cancelled_projection_custody($transfer_id, $transfer, $to_history, $binding) : false;
    $draft = false;
    if ($origin) foreach ($origin['material_history']['drafts'] as $row)
        if ((int)$row['id'] === (int)$receipt['draft_id']) $draft = $row;
    $unsigned = $action; $unsigned['draft_data_snapshot'] = null;
    $clock = material_core_rows('SELECT NOW() AS now_');
    if (!$cancelled || !$clock || !$draft || $receipt['draft_id'] !== (int)$origin['receipt']['approved_draft_id']
        || (int)$action['draft_id'] !== $receipt['draft_id']
        || (int)$action['user_id'] !== (int)$source['header']['requested_by']
        || (int)$action['level'] !== (int)$draft['current_level']
        || !$source['header']['updated_at'] || !$action['action_date'] || $action['ip_address'] === null
        || $source['header']['updated_at'] < $cancelled['action']['action_date']
        || $action['action_date'] < $source['header']['updated_at'] || $action['action_date'] > $clock[0]['now_']
        || $receipt['ordered_source_sha256'] !== material_core_source_fingerprint($ordered)
        || $receipt['source_sha256'] !== material_core_source_fingerprint($source)
        || $receipt['history_sha256'] !== material_transfer_release_history_hash($prior)
        || $receipt['transfer_source_sha256'] !== transfer_core_source_fingerprint($transfer)
        || $receipt['transfer_history_sha256'] !== material_transfer_release_history_hash($to_history)
        || $receipt['action_sha256'] !== material_core_source_fingerprint(array('header'=>$unsigned, 'lines'=>array()))) return false;
    return array('source'=>$source, 'history'=>$history, 'origin'=>$origin,
        'cancelled'=>$cancelled, 'action'=>$action, 'receipt'=>$receipt);
}

/**
 * Original maker releases an exact v2 linked, independently cancelled Transfer.
 * Preserve all old decisions and financial rows, append one attributable receipt,
 * and reconcile complete current and historical projections before committing.
 * Repeated calls deny; legacy or ambiguous custody requires explicit recovery.
 */
function release_cancelled_material_transfer($request_id, $comments = '')
{
    $identity = dimension_writer_reference_id($request_id);
    $context = material_core_context(); $to_context = transfer_core_context();
    if (!$identity || !$context || !$to_context || !is_string($comments) || strpos($comments, "\0") !== false) return false;
    try {
        if (!begin_transaction()) return false;
        $origin = material_transfer_origin_custody($identity);
        $transfer_id = $origin ? (int)$origin['receipt']['transfer_id'] : 0;
        $cancelled = $transfer_id ? transfer_cancelled_linked_custody($transfer_id) : false;
        if (!$origin || !$cancelled || $origin['receipt']['schema'] !== 'material-transfer-link-v2'
            || (int)$origin['material']['header']['requested_by'] !== (int)$_SESSION['wa_current_user']->user
            || $cancelled['source'] !== $origin['transfer'] || $cancelled['history'] !== $origin['transfer_history']) {
            cancel_transaction(); return false;
        }
        $clock = material_core_rows('SELECT NOW() AS now_');
        if (!$clock || !db_query('UPDATE '.TB_PREF."wh_material_requests SET status='approved',linked_doc_type=NULL,linked_doc_no=NULL,updated_at=NOW() WHERE request_id="
            .$identity." AND status='ordered' AND linked_doc_type=".ST_LOCTRANSFER.' AND linked_doc_no='.$transfer_id)
            || db_num_affected_rows() !== 1) { cancel_transaction(); return false; }
        $source = material_core_source($identity, 'approved'); $expected = $origin['material'];
        $now = material_core_rows('SELECT NOW() AS now_');
        if (!$source || !$now || !$source['header']['updated_at'] || $source['header']['updated_at'] < $clock[0]['now_']
            || $source['header']['updated_at'] > $now[0]['now_']) { cancel_transaction(); return false; }
        $expected['header']['status'] = 'approved'; $expected['header']['linked_doc_type'] = null;
        $expected['header']['linked_doc_no'] = null; $expected['header']['updated_at'] = $source['header']['updated_at'];
        if ($source !== $expected) { cancel_transaction(); return false; }
        $draft_id = (int)$origin['receipt']['approved_draft_id'];
        $draft = false; foreach ($origin['material_history']['drafts'] as $row) if ((int)$row['id'] === $draft_id) $draft = $row;
        if (!$draft) { cancel_transaction(); return false; }
        $action_id = add_approval_action($draft_id, 'source_release', null, $draft['current_level'], $comments, null, null);
        $history = $action_id ? material_core_history($identity) : false;
        if (!$history || count($history['actions']) !== count($origin['material_history']['actions'])+1) { cancel_transaction(); return false; }
        $action = array_pop($history['actions']);
        $literal = material_core_rows('SELECT '.db_escape($comments, true).' AS comments,'.db_escape(isset($_SERVER['REMOTE_ADDR']) ? $_SERVER['REMOTE_ADDR'] : '').' AS ip,NOW() AS now_');
        if (!$literal || $history !== $origin['material_history'] || (int)$action['id'] !== (int)$action_id
            || (int)$action['draft_id'] !== $draft_id || $action['action_type'] !== 'source_release'
            || (int)$action['user_id'] !== (int)$_SESSION['wa_current_user']->user
            || (int)$action['level'] !== (int)$draft['current_level'] || $action['draft_data_snapshot'] !== null
            || $action['comments'] !== $literal[0]['comments'] || $action['ip_address'] !== $literal[0]['ip']
            || $action['action_date'] < $clock[0]['now_'] || $action['action_date'] > $literal[0]['now_']) { cancel_transaction(); return false; }
        $transition = array();
        foreach (array('status','linked_doc_type','linked_doc_no','updated_at') as $field)
            $transition[$field] = $origin['material']['header'][$field];
        $receipt = array('schema'=>'material-transfer-release-v1', 'request_id'=>$identity, 'transfer_id'=>$transfer_id,
            'draft_id'=>$draft_id, 'material_company_sha256'=>material_core_context(true),
            'transfer_company_sha256'=>transfer_core_context(true), 'ordered_transition'=>$transition,
            'ordered_source_sha256'=>material_core_source_fingerprint($origin['material']),
            'source_sha256'=>material_core_source_fingerprint($source),
            'history_sha256'=>material_transfer_release_history_hash($origin['material_history']),
            'transfer_source_sha256'=>transfer_core_source_fingerprint($origin['transfer']),
            'transfer_history_sha256'=>material_transfer_release_history_hash($origin['transfer_history']),
            'action_sha256'=>material_core_source_fingerprint(array('header'=>$action, 'lines'=>array())));
        $snapshot = json_encode($receipt);
        if ($snapshot === false || !db_query('UPDATE '.TB_PREF.'approval_actions SET draft_data_snapshot='.db_escape($snapshot, true)
            .' WHERE id='.(int)$action_id.' AND draft_id='.$draft_id.' AND draft_data_snapshot IS NULL')
            || db_num_affected_rows() !== 1) { cancel_transaction(); return false; }
        $finished = material_released_transfer_custody($identity);
        if (!$finished || $finished['source'] !== $source || $finished['receipt'] !== $receipt
            || material_core_source_fingerprint($finished['origin']['material']) !== material_core_source_fingerprint($origin['material'])
            || $finished['origin']['material_history'] !== $origin['material_history'] || $finished['cancelled'] !== $cancelled
            || material_core_context() !== $context || transfer_core_context() !== $to_context
            || !commit_transaction()) { cancel_transaction(); return false; }
        return true;
    } catch (Exception $exception) { cancel_transaction(); return false;
    } catch (Throwable $exception) { cancel_transaction(); return false; }
}
