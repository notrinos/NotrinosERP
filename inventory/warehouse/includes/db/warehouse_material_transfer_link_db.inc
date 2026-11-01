<?php
/** Standalone material demand -> native Transfer draft: approval and source custody. */
require_once __DIR__.'/warehouse_material_terminal_db.inc';
require_once __DIR__.'/warehouse_transfer_line_db.inc';

/**
 * Check the exact draft produced from an independently approved material demand.
 * Pure comparison is also used by DB-free negative/tamper regression tests.
 * No quantity, linked identity, source line or opaque financial field is inferred.
 */
function material_transfer_pair_matches($material, $transfer, $actor, $from)
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
    if ($mr['status'] !== 'approved' || $mr['request_type'] !== 'transfer'
        || $mr['linked_doc_type'] !== null || $mr['linked_doc_no'] !== null
        || (int)$mr['requested_by'] !== (int)$actor || (int)$to['requested_by'] !== (int)$actor
        || $to['status'] !== 'draft' || $to['transfer_type'] !== 'standard'
        || (string)$to['from_loc_code'] !== (string)$from
        || $to['from_loc_code'] === $to['to_loc_code']
        || (string)$to['to_loc_code'] !== (string)$mr['warehouse_loc_code']
        || (string)$to['request_date'] !== (string)$mr['request_date']
        || (string)$to['expected_recv_date'] !== (string)$mr['required_date']
        || (string)$to['reference'] !== sprintf(_('From MR %s'), format_material_request_no($mr['request_no']))
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
    return $to_headers && count($to_headers) === 1 && $to_lines !== false
        && array('header'=>$to_headers[0], 'lines'=>$to_lines) === $native;
}
