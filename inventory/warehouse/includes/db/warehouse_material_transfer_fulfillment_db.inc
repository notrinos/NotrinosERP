<?php
/** Receipt-bound Material fulfillment on the existing Transfer core action trail. */
require_once __DIR__.'/warehouse_material_transfer_link_db.inc';

/**
 * Load current linked demand and its immutable history under Material-first
 * review locks. Null is a genuine standalone Transfer; false is unavailable
 * or ambiguous authority. Requisition projections cannot share this demand.
 */
function transfer_fulfillment_material($identity)
{
    if (!transfer_core_context() || empty($GLOBALS['transaction_level'])
        || !($identity = dimension_writer_reference_id($identity))
        || !lock_warehouse_review_family(ST_WH_MATERIAL_REQUEST)
        || !lock_warehouse_review_family(ST_WH_TRANSFER_ORDER)) return false;
    $links = transfer_core_rows('SELECT request_id FROM '.TB_PREF.'wh_material_requests WHERE linked_doc_type='
        .ST_LOCTRANSFER.' AND linked_doc_no='.$identity.' ORDER BY request_id FOR UPDATE');
    if ($links === false) return false;
    if (!$links) return null;
    if (count($links) !== 1 || !material_core_context()) return false;
    $request = dimension_writer_reference_id($links[0]['request_id']);
    if (!$request) return false;
    $history = material_core_history($request);
    $headers = material_core_rows('SELECT * FROM '.TB_PREF.'wh_material_requests WHERE request_id='.$request.' FOR UPDATE');
    $lines = material_core_rows('SELECT * FROM '.TB_PREF.'wh_material_request_lines WHERE request_id='.$request.' ORDER BY line_id FOR UPDATE');
    if (!$history || !$headers || count($headers) !== 1 || !$lines
        || $headers[0]['request_type'] !== 'transfer' || (int)$headers[0]['linked_doc_type'] !== ST_LOCTRANSFER
        || (int)$headers[0]['linked_doc_no'] !== $identity) return false;
    $ids = array();
    foreach ($lines as $line) {
        $id = dimension_writer_reference_id($line['line_id']);
        if (!$id) return false;
        $ids[] = $id;
    }
    $pairs = material_core_rows('SELECT id FROM '.TB_PREF.'purch_requisitions WHERE material_request_id='.$request.' ORDER BY id FOR UPDATE');
    $children = material_core_rows('SELECT id FROM '.TB_PREF.'purch_requisition_lines WHERE material_request_line_id IN ('
        .implode(',', $ids).') ORDER BY id FOR UPDATE');
    return $pairs === array() && $children === array()
        ? array('source'=>array('header'=>$headers[0], 'lines'=>$lines), 'history'=>$history) : false;
}

/** Bind actual independent Material and valued Transfer approvals before dispatch. */
function transfer_fulfillment_dispatch_custody($identity)
{
    $material = transfer_fulfillment_material($identity);
    if ($material === false) return false;
    $custody = transfer_approved_custody($identity);
    if (!$custody) return false;
    if ($material === null) return transfer_fulfillment_reference_locks($custody['source']) ? $custody : false;
    $origin = material_transfer_origin_projection((int)$material['source']['header']['request_id'],
        $material['source'], $material['history'], $custody['source'], $custody['history']);
    // Old zero-cost v1/v2 links remain historical; never silently reprice them.
    if (!$origin || $origin['receipt']['schema'] !== 'material-transfer-link-v3') return false;
    $custody['material'] = $material['source'];
    $custody['material_history'] = $material['history'];
    $custody['material_origin'] = material_transfer_origin_binding($origin);
    return $custody['material_origin'] && transfer_fulfillment_reference_locks($custody['source'], $custody['material']) ? $custody : false;
}

/**
 * Join the ordinary retained-reference writer protocol before stock/source SQL.
 * Owner rows are already locked; acquire the combined native master set in
 * numeric order before invoking each existing owner fence. Historical readers
 * deliberately do not require the current master/dependency tables.
 */
function transfer_fulfillment_reference_locks($transfer, $material = null)
{
    $owners = array();
    if ($material !== null) {
        $owners[] = array('wh_material_requests','request_id',$material['header']);
        foreach ($material['lines'] as $line) $owners[] = array('wh_material_request_lines','line_id',$line);
    }
    $owners[] = array('wh_transfer_orders','transfer_id',$transfer['header']);
    foreach ($transfer['lines'] as $line) $owners[] = array('wh_transfer_order_lines','line_id',$line);
    $ids = array();
    foreach ($owners as $owner) {
        if (!array_key_exists('custom_data', $owner[2])) return false;
        $payload = dimension_master_payload_transition($owner[2]['custom_data'], $owner[2]['custom_data']);
        if ($payload === false) return false;
        foreach ($payload['locks'] as $id) $ids[$id] = $id;
    }
    sort($ids, SORT_NUMERIC);
    foreach ($ids as $id)
        if (transfer_core_rows('SELECT id,type_,closed FROM '.TB_PREF.'dimensions WHERE id='.(int)$id.' FOR UPDATE') === false) return false;
    foreach ($owners as $owner)
        if (!dimension_ordinary_master_write($owner[0], $owner[2][$owner[1]], array(),
            function($admitted) { return true; }, true)) return false;
    return true;
}

/**
 * Permit only receipt-derived cumulative quantities. Dispatch cannot satisfy
 * demand. Every unchanged source field, retained payload and original line ID
 * remains exact; completion changes only the native status/timestamp.
 */
function transfer_fulfillment_material_transition($before, $after, $transfer_before, $transfer_after, $event)
{
    if (!is_array($before) || !is_array($after) || !isset($before['header'], $before['lines'], $after['header'], $after['lines'])
        || !is_array($before['header']) || !is_array($after['header']) || !is_array($before['lines']) || !is_array($after['lines'])
        || !isset($transfer_before['lines'], $transfer_after['lines']) || !$before['lines']
        || count($before['lines']) !== count($after['lines']) || count($before['lines']) !== count($transfer_before['lines'])
        || count($before['lines']) !== count($transfer_after['lines']) || $before['header']['status'] !== 'ordered'
        || !in_array($event, array('dispatch','receipt'), true)) return false;
    $expected = $before; $complete = true;
    foreach ($before['lines'] as $index=>$line) {
        $next = $after['lines'][$index]; $prior = $transfer_before['lines'][$index]; $current = $transfer_after['lines'][$index];
        foreach (array($line, $next) as $row)
            if (!isset($row['qty_fulfilled']) || !is_numeric($row['qty_fulfilled']) || !is_finite((float)$row['qty_fulfilled'])) return false;
        if ($line['stock_id'] !== $prior['stock_id'] || $line['stock_id'] !== $current['stock_id']
            || (float)$line['qty_requested'] !== (float)$prior['qty_requested']
            || (float)$line['qty_fulfilled'] !== (float)$prior['qty_received']
            || (float)$next['qty_fulfilled'] !== (float)$current['qty_received']
            || (float)$next['qty_fulfilled'] < (float)$line['qty_fulfilled']
            || (float)$next['qty_fulfilled'] > (float)$line['qty_requested']) return false;
        if ((float)$next['qty_fulfilled'] < (float)$line['qty_requested']) $complete = false;
        $expected['lines'][$index]['qty_fulfilled'] = $next['qty_fulfilled'];
    }
    if ($event === 'dispatch') return transfer_fulfillment_equal($before, $after);
    if ($complete) {
        if (!isset($after['header']['updated_at']) || !material_transfer_v2_timestamp($after['header']['updated_at'])
            || $after['header']['updated_at'] < $before['header']['updated_at']) return false;
        $expected['header']['status'] = 'fulfilled';
        $expected['header']['updated_at'] = $after['header']['updated_at'];
    }
    return transfer_fulfillment_equal($expected, $after);
}

/**
 * Apply the native negative-stock preference to new linked dispatches using
 * current locking inventory reads at the actual posting date. Stock masters
 * have already been locked by the native dependency guard. Lock the matching
 * movement ranges too, so ordinary inserts cannot race the availability check.
 * Historical custody never acquires a new availability requirement.
 */
function transfer_fulfillment_material_stock_allowed($source, $date, $posted = false)
{
    if (!material_core_context() || !transfer_core_context() || empty($GLOBALS['transaction_level'])) return false;
    $prefs = transfer_core_rows('SELECT value FROM '.TB_PREF."sys_prefs WHERE name='allow_negative_stock' FOR UPDATE");
    if (!$prefs || count($prefs) !== 1 || !in_array((string)$prefs[0]['value'], array('0','1'), true)) return false;
    if ((string)$prefs[0]['value'] === '1') return true;
    $quantities = array();
    foreach ($source['lines'] as $line) {
        $stock = $line['stock_id'];
        $quantities[$stock] = round((isset($quantities[$stock]) ? $quantities[$stock] : 0) + (float)$line['qty_requested'], 4);
    }
    ksort($quantities, SORT_STRING);
    foreach ($quantities as $stock=>$quantity) {
        $rows = transfer_core_rows('SELECT COALESCE(SUM(move.qty),0)>='.db_escape($posted ? 0 : $quantity)
            .' AS sufficient FROM '.TB_PREF.'stock_moves move LEFT JOIN '.TB_PREF.'voided v ON v.type=move.type AND v.id=move.trans_no'
            .' WHERE v.id IS NULL AND move.stock_id='.db_escape($stock)
            .' AND move.loc_code='.db_escape($source['header']['from_loc_code']).' AND move.tran_date<='.db_escape($date).' FOR UPDATE');
        if (!$rows || count($rows) !== 1 || (string)$rows[0]['sufficient'] !== '1') return false;
    }
    return true;
}

/** Apply only the locked, verified stock receipt deltas inside its owning transaction. */
function transfer_fulfillment_material_apply($custody, $transfer_after, $event)
{
    if (!transfer_fulfillment_equal(transfer_fulfillment_source((int)$custody['source']['header']['transfer_id']), $transfer_after)) return false;
    $material = transfer_fulfillment_material((int)$custody['source']['header']['transfer_id']);
    if (!$material || !transfer_fulfillment_equal($material['source'], $custody['material'])
        || $material['history'] !== $custody['material_history']) return false;
    if ($event === 'dispatch') return $material['source'];
    if ($event !== 'receipt' || count($material['source']['lines']) !== count($transfer_after['lines'])) return false;
    $complete = true;
    foreach ($material['source']['lines'] as $index=>$line) {
        $qty = $transfer_after['lines'][$index]['qty_received'];
        if ((float)$qty < (float)$line['qty_requested']) $complete = false;
        if ((float)$qty === (float)$line['qty_fulfilled']) continue;
        if (!db_query('UPDATE '.TB_PREF.'wh_material_request_lines SET qty_fulfilled='.db_escape($qty)
            .' WHERE line_id='.(int)$line['line_id'].' AND request_id='.(int)$line['request_id']
            .' AND qty_fulfilled='.db_escape($line['qty_fulfilled'])) || db_num_affected_rows() !== 1) return false;
    }
    if ($complete && (!db_query('UPDATE '.TB_PREF."wh_material_requests SET status='fulfilled',updated_at=NOW() WHERE request_id="
        .(int)$material['source']['header']['request_id']." AND status='ordered'") || db_num_affected_rows() !== 1)) return false;
    $after = transfer_fulfillment_material((int)$custody['source']['header']['transfer_id']);
    return $after && $after['history'] === $custody['material_history']
        && transfer_fulfillment_material_transition($custody['material'], $after['source'], $custody['source'], $transfer_after, $event)
        ? $after['source'] : false;
}

/** Read original approval/link and actual cumulative fulfillment without granting another command. */
function material_transfer_fulfillment_custody($request_id)
{
    require_once __DIR__.'/warehouse_transfer_fulfillment_db.inc';
    if (!material_core_context() || !transfer_core_context() || empty($GLOBALS['transaction_level'])
        || !($request_id = dimension_writer_reference_id($request_id))
        || !lock_warehouse_review_family(ST_WH_MATERIAL_REQUEST)
        || !lock_warehouse_review_family(ST_WH_TRANSFER_ORDER)) return false;
    $rows = material_core_rows('SELECT linked_doc_type,linked_doc_no FROM '.TB_PREF.'wh_material_requests WHERE request_id='.$request_id.' FOR UPDATE');
    if (!$rows || count($rows) !== 1 || (int)$rows[0]['linked_doc_type'] !== ST_LOCTRANSFER) return false;
    $custody = transfer_fulfillment_custody($rows[0]['linked_doc_no']);
    return $custody && isset($custody['material']) && (int)$custody['material']['header']['request_id'] === $request_id ? $custody : false;
}
