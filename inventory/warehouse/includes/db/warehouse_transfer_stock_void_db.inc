<?php
/** Retain native WMS movement receipts at the shared stock-transfer void boundary. */
require_once __DIR__.'/warehouse_transfer_fulfillment_db.inc';

/**
 * Authorize deletion of ordinary unowned location movements only. Exact WMS
 * receipts are immutable physical history: their native counter-transfer is a
 * separately reviewed event. Unknown/orphan/legacy WMS custody is never proof
 * that another movement is unowned. No source/history payload is returned.
 */
function warehouse_transfer_stock_void_allowed($type_no)
{
    if (empty($GLOBALS['transaction_level']) || !isset($_SESSION['wa_current_user'])
        || !$_SESSION['wa_current_user']->can_access('SA_VOIDTRANSACTION')
        || !($type_no = dimension_writer_reference_id($type_no))
        || !lock_warehouse_review_family(ST_WH_MATERIAL_REQUEST)
        || !lock_warehouse_review_family(ST_WH_TRANSFER_ORDER)) return false;
    $headers = transfer_core_rows('SELECT transfer_id FROM '.TB_PREF."wh_transfer_orders WHERE actual_ship_date IS NOT NULL OR status IN ('shipped','in_transit','received') ORDER BY transfer_id FOR UPDATE");
    $events = transfer_core_rows('SELECT DISTINCT d.approved_trans_no AS transfer_id FROM '.TB_PREF.'approval_drafts d INNER JOIN '
        .TB_PREF."approval_actions a ON a.draft_id=d.id WHERE d.trans_type=".ST_WH_TRANSFER_ORDER
        ." AND a.action_type IN ('stock_dispatch','stock_receipt') ORDER BY d.approved_trans_no FOR UPDATE");
    $moves = transfer_core_rows('SELECT trans_id,custom_data FROM '.TB_PREF.'stock_moves WHERE type='.ST_LOCTRANSFER
        .' AND trans_no='.$type_no.' ORDER BY trans_id FOR UPDATE');
    if ($headers === false || $events === false || $moves === false) return false;
    $ids = array(); $targets = array();
    foreach ($moves as $move) {
        // Generic voiding must not erase retained native classification JSON.
        if (dimension_master_payload_transition($move['custom_data'], null, true) === false) return false;
        $targets[(int)$move['trans_id']] = true;
    }
    foreach (array_merge($headers, $events) as $row) {
        $id = dimension_writer_reference_id($row['transfer_id']);
        if (!$id) return false;
        $ids[$id] = $id;
    }
    if (!$ids) return true;
    // The legacy void operator needs the actual owner authority to inspect
    // nonempty WMS custody. A denied/unknown owner cannot certify absence.
    if (!transfer_core_context() || !material_core_context() || !function_exists('get_approval_workflow_service')) return false;
    sort($ids, SORT_NUMERIC);
    foreach ($ids as $id) {
        $custody = transfer_fulfillment_custody($id);
        if (!$custody) return false;
        foreach ($custody['history']['actions'] as $action) {
            if (!in_array($action['action_type'], array('stock_dispatch','stock_receipt'), true)) continue;
            $receipt = get_approval_workflow_service()->decodeStoredDraftData($action['draft_data_snapshot']);
            if (!is_array($receipt) || !isset($receipt['moves']) || !is_array($receipt['moves'])) return false;
            foreach ($receipt['moves'] as $move) if (isset($targets[$move])) return false;
        }
    }
    return true;
}
