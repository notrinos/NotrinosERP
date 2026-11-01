<?php
/** Standalone material demand -> native Transfer draft: approval and source custody. */
require_once __DIR__.'/warehouse_material_terminal_db.inc';
require_once __DIR__.'/warehouse_transfer_line_db.inc';

/**
 * Check the exact draft produced from an independently approved material demand.
 * Pure comparison is also used by DB-free negative/tamper regression tests.
 * No quantity, linked identity, source line or opaque financial field is inferred.
 */
function material_transfer_pair_matches($material, $transfer, $actor, $from, $authored_reference = null)
{
    if (!is_array($material) || !isset($material['header'], $material['lines'])
        || !is_array($transfer) || !isset($transfer['header'], $transfer['lines'])
        || !is_array($material['header']) || !is_array($material['lines'])
        || !is_array($transfer['header']) || !is_array($transfer['lines'])) return false;
    $mr = $material['header']; $to = $transfer['header'];
    foreach (array('request_id','request_no','request_type','status','warehouse_loc_code',
        'requested_by','request_date','required_date','memo','linked_doc_type','linked_doc_no') as $field)
        if (!array_key_exists($field, $mr)) return false;
    foreach (array('transfer_id','status','transfer_type','requested_by','from_loc_code',
        'to_loc_code','request_date','reference','memo','expected_recv_date','expected_ship_date',
        'shipped_by','received_by','approved_by','actual_ship_date','actual_recv_date') as $field)
        if (!array_key_exists($field, $to)) return false;
    $expected_reference = $authored_reference === null
        ? sprintf(_('From MR %s'), format_material_request_no($mr['request_no'])) : $authored_reference;
    if (!is_string($expected_reference)) return false;
    if ($mr['status'] !== 'approved' || $mr['request_type'] !== 'transfer'
        || $mr['linked_doc_type'] !== null || $mr['linked_doc_no'] !== null
        || (int)$mr['requested_by'] !== (int)$actor || (int)$to['requested_by'] !== (int)$actor
        || $to['status'] !== 'draft' || $to['transfer_type'] !== 'standard'
        || (string)$to['from_loc_code'] !== (string)$from
        || $to['from_loc_code'] === $to['to_loc_code']
        || (string)$to['to_loc_code'] !== (string)$mr['warehouse_loc_code']
        || (string)$to['request_date'] !== (string)$mr['request_date']
        || (string)$to['expected_recv_date'] !== (string)$mr['required_date']
        || (string)$to['reference'] !== $expected_reference
        || (string)$to['memo'] !== (string)$mr['memo'] || $to['expected_ship_date'] !== null
        || $to['shipped_by'] !== null || $to['received_by'] !== null
        || $to['approved_by'] !== null || $to['actual_ship_date'] !== null
        || $to['actual_recv_date'] !== null
        || count($material['lines']) === 0 || count($material['lines']) !== count($transfer['lines'])) return false;
    foreach ($material['lines'] as $index=>$line) {
        if (!isset($line['stock_id'], $line['qty_requested'], $line['qty_fulfilled'])
            || !isset($transfer['lines'][$index])) return false;
        $actual = $transfer['lines'][$index];
        foreach (array('stock_id','qty_requested','qty_shipped','qty_received','unit_cost',
            'from_bin_id','to_bin_id','batch_id','serial_id','memo','custom_data','transfer_id') as $field)
            if (!array_key_exists($field, $actual)) return false;
        $quantity = transfer_line_decimal($line['qty_requested'], true);
        if ($quantity === false || !is_numeric($line['qty_fulfilled'])
            || (float)$line['qty_fulfilled'] != 0 || $actual['stock_id'] !== $line['stock_id']
            || (string)$actual['qty_requested'] !== $quantity
            || (float)$actual['qty_shipped'] != 0 || (float)$actual['qty_received'] != 0
            || (float)$actual['unit_cost'] != 0
            || (int)$actual['transfer_id'] !== (int)$to['transfer_id']) return false;
        foreach (array('from_bin_id','to_bin_id','batch_id','serial_id','memo','custom_data') as $field)
            if ($actual[$field] !== null) return false;
    }
    return true;
}

/**
 * Immutable link intent for later *independent* material-origin review. Only
 * original approval-custody state is used: a numeric Transfer link alone does
 * not prove that the maker obtained an independent Material approval.
 */
function material_transfer_link_intent($custody, $transfer, $actor, $from, $authored_reference = null, $schema = 'material-transfer-link-v2')
{
    if (!is_array($custody) || !isset($custody['source'], $custody['history'], $custody['draft'])
        || !isset($custody['source']['header']['request_id']) || !isset($transfer['header']['transfer_id'])
        || !isset($custody['source']['header']['updated_at'])
        || !material_transfer_pair_matches($custody['source'], $transfer, $actor, $from, $authored_reference)) return false;
    $material_company = material_core_context(true);
    $transfer_company = transfer_core_context(true);
    if (!$material_company || !$transfer_company) return false;
    if (!in_array($schema, array('material-transfer-link-v1', 'material-transfer-link-v2'), true)) return false;
    $intent = array('schema'=>$schema,
        'request_id'=>(int)$custody['source']['header']['request_id'],
        'transfer_id'=>(int)$transfer['header']['transfer_id'],
        'approved_draft_id'=>(int)$custody['draft']['id'],
        'maker_id'=>(int)$actor, 'from_loc_code'=>$from,
        'authored_reference'=>$transfer['header']['reference'],
        'material_company_sha256'=>$material_company,
        'transfer_company_sha256'=>$transfer_company,
        'approved_updated_at'=>$custody['source']['header']['updated_at'],
        'approved_source_sha256'=>material_core_source_fingerprint($custody['source']),
        'approved_history_sha256'=>material_core_source_fingerprint(array('header'=>array(),
            'lines'=>array_merge($custody['history']['drafts'], $custody['history']['actions']))),
        'transfer_draft_sha256'=>transfer_core_source_fingerprint($transfer));
    if ($schema === 'material-transfer-link-v2') {
        // Capture actual pre-review values, including SQL NULL. A later reader
        // must never guess updated_at from created_at or its current clock.
        $transition = array();
        foreach (array('status','updated_at','approved_by','approval_date') as $field) {
            if (!array_key_exists($field, $transfer['header'])) return false;
            $transition[$field] = $transfer['header'][$field];
        }
        if ($transition['status'] !== 'draft' || $transition['approved_by'] !== null
            || $transition['approval_date'] !== null) return false;
        $intent['transfer_draft_transition'] = $transition;
    }
    return $intent;
}

/**
 * Reconstruct only the original Draft described by a retained link receipt.
 * This pure provenance check grants no workflow/terminal/financial authority.
 * Callers must independently verify the receipt's immutable Material history,
 * company, current Transfer review and the allowed transition before writing.
 * Legacy v1 can prove its unchanged Draft only; no historical timestamp guess.
 */
function material_transfer_original_draft($transfer, $receipt)
{
    if (!is_array($transfer) || !isset($transfer['header'], $transfer['lines'])
        || !is_array($transfer['header']) || !is_array($transfer['lines'])
        || !is_array($receipt) || !isset($receipt['schema'], $receipt['transfer_id'], $receipt['transfer_draft_sha256'])
        || !isset($transfer['header']['transfer_id'], $transfer['header']['status'])
        || !in_array($transfer['header']['status'], array('draft','submitted','approved','cancelled'), true)
        || !($identity = dimension_writer_reference_id($receipt['transfer_id']))
        || dimension_writer_reference_id($transfer['header']['transfer_id']) !== $identity
        || !is_string($receipt['transfer_draft_sha256'])
        || !preg_match('/^[a-f0-9]{64}$/D', $receipt['transfer_draft_sha256'])) return false;
    $original = $transfer;
    if ($receipt['schema'] === 'material-transfer-link-v2') {
        if (!isset($receipt['transfer_draft_transition']) || !is_array($receipt['transfer_draft_transition'])
            || array_keys($receipt['transfer_draft_transition']) !== array('status','updated_at','approved_by','approval_date')) return false;
        $transition = $receipt['transfer_draft_transition'];
        if ($transition['status'] !== 'draft' || $transition['approved_by'] !== null || $transition['approval_date'] !== null
            || ($transition['updated_at'] !== null && (!is_string($transition['updated_at'])
                || !preg_match('/^[0-9]{4}-[0-9]{2}-[0-9]{2} [0-9]{2}:[0-9]{2}:[0-9]{2}$/D', $transition['updated_at'])))) return false;
        foreach ($transition as $field=>$value) {
            if (!array_key_exists($field, $original['header'])) return false;
            $original['header'][$field] = $value;
        }
    } elseif ($receipt['schema'] !== 'material-transfer-link-v1'
        || isset($receipt['transfer_draft_transition'])) return false;
    return isset($original['header']['status']) && $original['header']['status'] === 'draft'
        && transfer_core_source_fingerprint($original) === $receipt['transfer_draft_sha256'] ? $original : false;
}

/**
 * Verify immutable Material origin independently of current Transfer review.
 * Return the exact locked current source/history and its original Draft, so a
 * future core command can bind both explicitly. Transfer histories are read,
 * not endorsed: this reader does not prove approval, authorize a transition,
 * or grant shipping, GL posting, cancellation or fulfillment authority.
 *
 * All reads are locked in the same company and owning transaction. Rebuild
 * the original approved source only from the new immutable link receipt;
 * source history and its *older* signed core decision must both still match.
 */
function material_transfer_origin_custody($request_id)
{
    $identity = dimension_writer_reference_id($request_id);
    $actor = isset($_SESSION['wa_current_user']->user)
        ? dimension_writer_reference_id($_SESSION['wa_current_user']->user) : false;
    if (!$identity || !$actor || !material_core_context() || !transfer_core_context()
        || empty($GLOBALS['transaction_level'])) return false;
    $history = material_core_history($identity);
    if (!$history || !$history['drafts'] || count($history['actions']) < 2) return false;
    $link = $history['actions'][count($history['actions'])-1];
    $receipt = material_transfer_link_receipt($link);
    if (!$receipt) return false;
    $transfer_id = dimension_writer_reference_id($receipt['transfer_id']);
    $headers = material_core_rows('SELECT * FROM '.TB_PREF.'wh_material_requests WHERE request_id='.$identity.' FOR UPDATE');
    $lines = material_core_rows('SELECT * FROM '.TB_PREF.'wh_material_request_lines WHERE request_id='.$identity.' ORDER BY line_id FOR UPDATE');
    $pairs = material_core_rows('SELECT id FROM '.TB_PREF.'purch_requisitions WHERE material_request_id='.$identity.' ORDER BY id FOR UPDATE');
    $to_history = transfer_core_history($transfer_id);
    $to_header = transfer_core_rows('SELECT * FROM '.TB_PREF.'wh_transfer_orders WHERE transfer_id='.$transfer_id.' FOR UPDATE');
    $to_lines = transfer_core_rows('SELECT * FROM '.TB_PREF.'wh_transfer_order_lines WHERE transfer_id='.$transfer_id.' ORDER BY line_id FOR UPDATE');
    $links = material_core_rows('SELECT request_id FROM '.TB_PREF.'wh_material_requests WHERE linked_doc_type='
        .ST_LOCTRANSFER.' AND linked_doc_no='.$transfer_id.' ORDER BY request_id FOR UPDATE');
    $line_ids = array();
    if ($lines) foreach ($lines as $row) {
        $line_id = isset($row['line_id']) ? dimension_writer_reference_id($row['line_id']) : false;
        if (!$line_id) return false;
        $line_ids[] = $line_id;
    }
    $requisition_lines = $line_ids ? material_core_rows('SELECT id FROM '.TB_PREF.
        'purch_requisition_lines WHERE material_request_line_id IN ('.implode(',', $line_ids).
        ') ORDER BY id FOR UPDATE') : false;
    if (!$headers || count($headers) !== 1 || !$lines || $pairs !== array() || $requisition_lines !== array()
        || !$to_history
        || !$to_header || count($to_header) !== 1 || !$to_lines || !$links || count($links) !== 1
        || (int)$links[0]['request_id'] !== $identity
        || $headers[0]['status'] !== 'ordered' || $headers[0]['request_type'] !== 'transfer'
        || (int)$headers[0]['linked_doc_type'] !== ST_LOCTRANSFER
        || (int)$headers[0]['linked_doc_no'] !== $transfer_id
        || (int)$headers[0]['requested_by'] !== (int)$receipt['maker_id']) return false;
    return material_transfer_origin_projection($identity,
        array('header'=>$headers[0], 'lines'=>$lines), $history,
        array('header'=>$to_header[0], 'lines'=>$to_lines), $to_history);
}

/** Decode only a canonical immutable link action; this grants no source authority. */
function material_transfer_link_receipt($link)
{
    if (!is_array($link)) return false;
    foreach (array('draft_data_snapshot','action_type','user_id','draft_id') as $field)
        if (!array_key_exists($field, $link)) return false;
    $approval = get_approval_workflow_service();
    $receipt = $link['draft_data_snapshot'] === null ? false
        : $approval->decodeStoredDraftData($link['draft_data_snapshot']);
    $receipt_keys = array('schema','request_id','transfer_id','approved_draft_id','maker_id',
        'from_loc_code','authored_reference','material_company_sha256','transfer_company_sha256',
        'approved_updated_at','approved_source_sha256','approved_history_sha256','transfer_draft_sha256');
    if (is_array($receipt) && isset($receipt['schema']) && $receipt['schema'] === 'material-transfer-link-v2')
        $receipt_keys[] = 'transfer_draft_transition';
    if (!is_array($receipt) || array_keys($receipt) !== $receipt_keys
        || !is_string($receipt['from_loc_code']) || $receipt['from_loc_code'] === ''
        || !is_string($receipt['authored_reference'])
        || !is_string($receipt['approved_updated_at'])
        || !in_array($receipt['schema'], array('material-transfer-link-v1','material-transfer-link-v2'), true)
        || !($transfer_id = dimension_writer_reference_id($receipt['transfer_id']))
        || $link['action_type'] !== 'source_transfer'
        || (int)$link['user_id'] !== (int)$receipt['maker_id']
        || (int)$link['draft_id'] !== (int)$receipt['approved_draft_id']) return false;
    return $receipt;
}

/**
 * Compare receipt-bound historical projections without consulting the live link.
 * The caller must lock/load the full rows and independently prove any transition
 * used to reconstruct them. Success proves only the supplied origin projection;
 * it never admits current source edits, release, approval or stock/GL execution.
 */
function material_transfer_origin_projection($request_id, $source, $history, $transfer, $transfer_history)
{
    $identity = dimension_writer_reference_id($request_id);
    if (!$identity || !material_core_context() || !transfer_core_context()
        || empty($GLOBALS['transaction_level'])) return false;
    foreach (array($source, $transfer) as $rows) {
        if (!is_array($rows) || !isset($rows['header'], $rows['lines'])
            || !is_array($rows['header']) || !is_array($rows['lines']) || !$rows['lines']) return false;
        foreach ($rows['lines'] as $line) if (!is_array($line)) return false;
    }
    foreach (array($history, $transfer_history) as $rows) {
        if (!is_array($rows) || !isset($rows['drafts'], $rows['actions'])
            || !is_array($rows['drafts']) || !is_array($rows['actions'])) return false;
        foreach (array_merge($rows['drafts'], $rows['actions']) as $row) if (!is_array($row)) return false;
    }
    if (!$history['drafts'] || count($history['actions']) < 2) return false;
    foreach ($history['drafts'] as $row)
        foreach (array('id','status','workflow_id','submitted_by','draft_data','approved_trans_no',
            'completed_date','current_level') as $field) if (!array_key_exists($field, $row)) return false;
    foreach ($history['actions'] as $row)
        foreach (array('id','draft_id','action_type','user_id','level','draft_data_snapshot',
            'comments','action_date','ip_address') as $field) if (!array_key_exists($field, $row)) return false;
    $actions = $history['actions']; $link = array_pop($actions);
    $receipt = material_transfer_link_receipt($link);
    if (!$receipt) return false;
    $transfer_id = dimension_writer_reference_id($receipt['transfer_id']);
    foreach (array('request_id','status','request_type','linked_doc_type','linked_doc_no',
        'requested_by','approved_by','updated_at') as $field)
        if (!array_key_exists($field, $source['header'])) return false;
    if (dimension_writer_reference_id($source['header']['request_id']) !== $identity
        || $source['header']['status'] !== 'ordered' || $source['header']['request_type'] !== 'transfer'
        || (int)$source['header']['linked_doc_type'] !== ST_LOCTRANSFER
        || dimension_writer_reference_id($source['header']['linked_doc_no']) !== $transfer_id
        || (int)$source['header']['requested_by'] !== (int)$receipt['maker_id']) return false;
    $approval = get_approval_workflow_service();
    $approved = $source;
    $approved['header']['status'] = 'approved';
    $approved['header']['linked_doc_type'] = null;
    $approved['header']['linked_doc_no'] = null;
    $approved['header']['updated_at'] = $receipt['approved_updated_at'];
    $prior_history = array('drafts'=>$history['drafts'], 'actions'=>$actions);
    $draft = false;
    foreach ($history['drafts'] as $row) {
        if ((int)$row['status'] === APPROVAL_STATUS_APPROVED) {
            if ($draft) return false;
            $draft = $row;
        } elseif (!in_array((int)$row['status'], array(APPROVAL_STATUS_REJECTED, APPROVAL_STATUS_CANCELLED), true)) return false;
        $workflow = material_core_rows('SELECT trans_type FROM '.TB_PREF.'approval_workflows WHERE id='.(int)$row['workflow_id'].' FOR UPDATE');
        if (!$workflow || count($workflow) !== 1 || (int)$workflow[0]['trans_type'] !== ST_WH_MATERIAL_REQUEST
            || (int)$row['submitted_by'] !== (int)$receipt['maker_id']) return false;
    }
    $signed = $actions ? $actions[count($actions)-1] : false;
    $data = $draft ? $approval->decodeStoredDraftData($draft['draft_data']) : false;
    $terminal = $signed && $signed['draft_data_snapshot'] !== null
        ? $approval->decodeStoredDraftData($signed['draft_data_snapshot']) : false;
    if (!$draft || !is_array($data) || !is_array($terminal)
        || !isset($data['material_company_sha256'], $data['material_source_sha256'])
        || $data['material_company_sha256'] !== material_core_context(true)
        || (int)$draft['approved_trans_no'] !== $identity || !$draft['completed_date']
        || (int)$approved['header']['approved_by'] !== (int)$signed['user_id']
        || (int)$signed['user_id'] === (int)$receipt['maker_id'] || $signed['action_type'] !== 'approve'
        || (int)$signed['draft_id'] !== (int)$draft['id']
        || (int)$signed['level'] !== (int)$draft['current_level']) return false;
    $without_terminal = $actions;
    array_pop($without_terminal);
    $unsigned = $signed; $unsigned['draft_data_snapshot'] = null;
    $terminal_expected = array('schema'=>'material-approved-source-v1', 'request_id'=>$identity,
        'draft_id'=>(int)$draft['id'], 'company_sha256'=>material_core_context(true),
        'submitted_source_sha256'=>$data['material_source_sha256'],
        'history_sha256'=>material_core_source_fingerprint(array('header'=>array(),
            'lines'=>array_merge($history['drafts'], $without_terminal))),
        'action_sha256'=>material_core_source_fingerprint(array('header'=>$unsigned, 'lines'=>array())),
        'source_sha256'=>material_core_source_fingerprint($approved));
    $original = material_transfer_original_draft($transfer, $receipt);
    if (!$original) return false;
    $custody = array('source'=>$approved, 'history'=>$prior_history, 'draft'=>$draft);
    $expected = material_transfer_link_intent($custody, $original, (int)$receipt['maker_id'],
        isset($receipt['from_loc_code']) ? $receipt['from_loc_code'] : '',
        $receipt['authored_reference'], $receipt['schema']);
    if ($terminal !== $terminal_expected || !$expected || $receipt !== $expected
        || (int)$receipt['request_id'] !== $identity || (int)$receipt['approved_draft_id'] !== (int)$draft['id']
        || (int)$link['level'] !== (int)$draft['current_level']
        || $link['comments'] !== 'Material-to-Transfer source link'
        || !$link['action_date'] || $link['ip_address'] === null
        || $source['header']['updated_at'] === null) return false;
    return array('material'=>$source, 'approved'=>$approved, 'transfer'=>$transfer,
        'material_history'=>$history, 'transfer_history'=>$transfer_history, 'receipt'=>$receipt,
        'original_transfer'=>$original);
}

/**
 * Preserve the converter's strict unreviewed Draft completion contract.
 * Origin verification alone must never admit an existing Transfer review or
 * normalize its current transition fields on behalf of a writing command.
 */
function material_transfer_link_custody($request_id)
{
    $custody = material_transfer_origin_custody($request_id);
    if (!$custody || $custody['original_transfer'] !== $custody['transfer']
        || $custody['transfer_history']['drafts'] || $custody['transfer_history']['actions']) return false;
    unset($custody['original_transfer']);
    return $custody;
}

/**
 * Link only a freshly constructed, unreviewed native draft to its original
 * approved standalone source. The caller owns the enclosing transaction.
 * History, source children, destination draft and actor/company stay locked.
 * This DOES NOT grant downstream Transfer approval or stock/GL fulfillment.
 */
function link_approved_material_transfer($request_id, $transfer_id, $snapshot)
{
    $identity = dimension_writer_reference_id($request_id);
    $to_identity = dimension_writer_reference_id($transfer_id);
    $actor = isset($_SESSION['wa_current_user']->user) ? dimension_writer_reference_id($_SESSION['wa_current_user']->user) : false;
    $material_context = material_core_context(); $transfer_context = transfer_core_context();
    if (!$identity || !$to_identity || !$actor || !$material_context || !$transfer_context
        || empty($GLOBALS['transaction_level']) || !is_array($snapshot)
        || !isset($snapshot['custody'], $snapshot['transfer'], $snapshot['from'])
        || !is_string($snapshot['from']) || $snapshot['from'] === '') return false;
    $custody = material_approved_custody($identity);
    $history = transfer_core_history($to_identity);
    $native = transfer_line_source($to_identity);
    if (!$custody || $custody !== $snapshot['custody'] || !$history
        || $history['drafts'] || $history['actions'] || !$native
        || $native !== $snapshot['transfer']
        || !material_transfer_pair_matches($custody['source'], $native, $actor, $snapshot['from'])) return false;
    $existing = material_core_rows('SELECT request_id FROM '.TB_PREF.'wh_material_requests WHERE linked_doc_type='
        .ST_LOCTRANSFER.' AND linked_doc_no='.$to_identity.' ORDER BY request_id FOR UPDATE');
    $started = material_core_rows('SELECT NOW() AS now_');
    if ($existing !== array() || !$started) return false;
    if (!db_query('UPDATE '.TB_PREF."wh_material_requests SET status='ordered',linked_doc_type="
        .ST_LOCTRANSFER.',linked_doc_no='.$to_identity.',updated_at=NOW() WHERE request_id='
        .$identity." AND status='approved' AND linked_doc_type IS NULL AND linked_doc_no IS NULL")
        || db_num_affected_rows() !== 1) return false;
    $headers = material_core_rows('SELECT * FROM '.TB_PREF.'wh_material_requests WHERE request_id='.$identity.' FOR UPDATE');
    $lines = material_core_rows('SELECT * FROM '.TB_PREF.'wh_material_request_lines WHERE request_id='.$identity.' ORDER BY line_id FOR UPDATE');
    $links = material_core_rows('SELECT request_id FROM '.TB_PREF.'wh_material_requests WHERE linked_doc_type='
        .ST_LOCTRANSFER.' AND linked_doc_no='.$to_identity.' ORDER BY request_id FOR UPDATE');
    $finished = material_core_rows('SELECT NOW() AS now_');
    if (!$headers || count($headers) !== 1 || $lines === false || !$finished
        || !$headers[0]['updated_at'] || $headers[0]['updated_at'] < $started[0]['now_']
        || $headers[0]['updated_at'] > $finished[0]['now_']) return false;
    $expected = $custody['source']['header'];
    $expected['status'] = 'ordered';
    $expected['linked_doc_type'] = (string)ST_LOCTRANSFER;
    $expected['linked_doc_no'] = (string)$to_identity;
    $expected['updated_at'] = $headers[0]['updated_at'];
    // PDO numeric modes vary: compare only the newly authored numeric link as
    // integer, every unchanged column (including opaque payload) byte-exactly.
    $actual = $headers[0];
    if ((int)$actual['linked_doc_type'] !== ST_LOCTRANSFER
        || (int)$actual['linked_doc_no'] !== $to_identity) return false;
    $actual['linked_doc_type'] = $expected['linked_doc_type'];
    $actual['linked_doc_no'] = $expected['linked_doc_no'];
    if ($actual !== $expected || $lines !== $custody['source']['lines']
        || count($links) !== 1 || (int)$links[0]['request_id'] !== $identity
        || material_core_history($identity) !== $custody['history']
        || transfer_core_history($to_identity) !== $history
        || material_core_context() !== $material_context
        || transfer_core_context() !== $transfer_context) return false;
    $to_headers = transfer_core_rows('SELECT * FROM '.TB_PREF.'wh_transfer_orders WHERE transfer_id='.$to_identity.' FOR UPDATE');
    $to_lines = transfer_core_rows('SELECT * FROM '.TB_PREF.'wh_transfer_order_lines WHERE transfer_id='
        .$to_identity.' ORDER BY line_id FOR UPDATE');
    if (!$to_headers || count($to_headers) !== 1 || $to_lines === false
        || array('header'=>$to_headers[0], 'lines'=>$to_lines) !== $native) return false;
    $receipt = material_transfer_link_intent($custody, $native, $actor, $snapshot['from']);
    $encoded = $receipt ? json_encode($receipt) : false;
    if ($encoded === false || strlen($encoded) > 16000000) return false;
    $draft_id = (int)$custody['draft']['id'];
    $level = (int)$custody['draft']['current_level'];
    $action_id = add_approval_action($draft_id, 'source_transfer', null, $level,
        'Material-to-Transfer source link', $encoded, null);
    $post_history = $action_id ? material_core_history($identity) : false;
    if (!$post_history || $post_history['drafts'] !== $custody['history']['drafts']
        || count($post_history['actions']) !== count($custody['history']['actions']) + 1) return false;
    $actions = $post_history['actions']; $action = array_pop($actions);
    // The shared audit writer stores JSON through db_escape(), which encodes
    // HTML quotes. Compare the exact SQL-bound bytes, as native terminal
    // receipts do; the custody reader separately decodes and verifies intent.
    $literal = material_core_rows('SELECT '.db_escape($encoded, true).' AS snapshot_');
    if (!$literal || $actions !== $custody['history']['actions'] || (int)$action['id'] !== (int)$action_id
        || (int)$action['draft_id'] !== $draft_id || $action['action_type'] !== 'source_transfer'
        || (int)$action['user_id'] !== $actor || (int)$action['level'] !== $level
        || $action['comments'] !== 'Material-to-Transfer source link'
        || $action['draft_data_snapshot'] !== $literal[0]['snapshot_']
        || transfer_core_history($to_identity) !== $history) return false;
    return material_transfer_link_custody($identity) !== false;
}
