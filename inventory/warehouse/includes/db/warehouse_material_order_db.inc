<?php
/** Paired requisition ordering belongs to its verified native purchase command. */
require_once __DIR__.'/warehouse_material_core_db.inc';

/**
 * Link the first native PO without accepting an arbitrary document number.
 * The requisition owner retains the outer transaction and final PO/source check.
 * This command preserves every intrinsic child, opaque classification and review
 * byte; it grants neither requisition approval nor downstream fulfillment.
 */
function link_paired_material_purchase_order($request_id, $order_no, $snapshot)
{
    $context = material_core_context();
    $identity = dimension_writer_reference_id($request_id);
    $number = dimension_writer_reference_id($order_no);
    if (!$context || !$identity || !$number || empty($GLOBALS['transaction_level'])
        || !function_exists('requisition_purchase_authorized') || !requisition_purchase_authorized()
        || !function_exists('verify_agreement_purchase_order_effect')
        || !is_array($snapshot) || empty($snapshot['header']['id'])
        || !dimension_writer_reference_id($snapshot['header']['id'])
        || (int)$snapshot['header']['material_request_id'] !== $identity
        || !isset($snapshot['material'][0], $snapshot['material_lines'], $snapshot['cart'])
        || !($snapshot['cart'] instanceof purch_order) || !$snapshot['cart']->is_current_company()
        || (int)$snapshot['cart']->trans_type !== ST_PURCHORDER || (int)$snapshot['cart']->order_no !== $number
        || (int)$snapshot['cart']->requisition_id !== (int)$snapshot['header']['id']) return false;
    try {
        if (!begin_transaction()) return false;
        $requisition = (int)$snapshot['header']['id'];
        $history_scope = '(trans_type IN ('.ST_PURCHREQUISITION.','.ST_WH_MATERIAL_REQUEST.') OR (trans_type='.ST_PURCHORDER.' AND reserved_trans_no='.$number.'))';
        $queries = array(
            'pair'=>'SELECT * FROM '.TB_PREF.'purch_requisitions WHERE material_request_id='.$identity.' ORDER BY id FOR UPDATE',
            'header'=>'SELECT * FROM '.TB_PREF.'wh_material_requests WHERE request_id='.$identity.' FOR UPDATE',
            'lines'=>'SELECT * FROM '.TB_PREF.'wh_material_request_lines WHERE request_id='.$identity.' ORDER BY line_id FOR UPDATE',
            'requisition_lines'=>'SELECT * FROM '.TB_PREF.'purch_requisition_lines WHERE requisition_id='.$requisition.' ORDER BY id FOR UPDATE',
            'orders'=>'SELECT * FROM '.TB_PREF.'purch_orders WHERE requisition_id='.$requisition.' ORDER BY order_no FOR UPDATE',
            'details'=>'SELECT detail.* FROM '.TB_PREF.'purch_order_details detail INNER JOIN '.TB_PREF.'purch_orders po ON po.order_no=detail.order_no WHERE po.requisition_id='.$requisition.' ORDER BY detail.po_detail_item FOR UPDATE',
            'drafts'=>'SELECT * FROM '.TB_PREF.'approval_drafts WHERE '.$history_scope.' ORDER BY id FOR UPDATE',
            'actions'=>'SELECT action.* FROM '.TB_PREF.'approval_actions action INNER JOIN '.TB_PREF.'approval_drafts draft ON draft.id=action.draft_id WHERE '.$history_scope.' ORDER BY action.id FOR UPDATE');
        $rows = array();
        foreach ($queries as $key=>$query) {
            $rows[$key] = material_core_rows($query);
            if ($rows[$key] === false) { cancel_transaction(); return false; }
        }
        if (count($rows['pair']) !== 1 || (int)$rows['pair'][0]['id'] !== $requisition
            || !in_array($rows['pair'][0]['status'], array('approved','partially_ordered','ordered'), true)
            || $rows['header'] !== $snapshot['material'] || $rows['lines'] !== $snapshot['material_lines']
            || $rows['header'][0]['status'] !== 'approved' || $rows['header'][0]['request_type'] !== 'purchase'
            || !empty($rows['header'][0]['linked_doc_type']) || !empty($rows['header'][0]['linked_doc_no'])
            || !$rows['lines'] || !$rows['requisition_lines']) { cancel_transaction(); return false; }
        $matched = false;
        foreach ($rows['orders'] as $order)
            if ((int)$order['order_no'] === $number && $order['into_stock_location'] === $rows['header'][0]['warehouse_loc_code']) $matched = true;
        $progress = false;
        foreach ($rows['requisition_lines'] as $line)
            if ((int)$line['po_number'] === $number && is_numeric($line['qty_ordered'])
                && is_finite((float)$line['qty_ordered']) && (float)$line['qty_ordered'] > 0) $progress = true;
        if (!$matched || !$progress) { cancel_transaction(); return false; }
        // The native verifier checks exact header, quantities, prices, dates and
        // reserved identity, including zero-price and supplier-description rules.
        $intent = json_decode(json_encode($snapshot['cart']), true);
        verify_agreement_purchase_order_effect($intent, $snapshot['cart'], $number);
        $clock = material_core_rows('SELECT NOW() AS now_');
        if (!$clock || !db_query('UPDATE '.TB_PREF."wh_material_requests SET status='ordered', linked_doc_type=".ST_PURCHORDER
            .', linked_doc_no='.$number.', updated_at=NOW() WHERE request_id='.$identity." AND status='approved' AND (linked_doc_no IS NULL OR linked_doc_no=0)")
            || db_num_affected_rows() !== 1) { cancel_transaction(); return false; }
        $current = material_core_rows($queries['header']); $now = material_core_rows('SELECT NOW() AS now_');
        if (!$current || !$now || !$current[0]['updated_at'] || $current[0]['updated_at'] < $clock[0]['now_']
            || $current[0]['updated_at'] > $now[0]['now_']) { cancel_transaction(); return false; }
        $expected = $rows['header'];
        foreach (array('status','linked_doc_type','linked_doc_no','updated_at') as $field) $expected[0][$field] = $current[0][$field];
        if ($current[0]['status'] !== 'ordered' || (int)$current[0]['linked_doc_type'] !== ST_PURCHORDER
            || (int)$current[0]['linked_doc_no'] !== $number || $current !== $expected) { cancel_transaction(); return false; }
        foreach ($queries as $key=>$query)
            if ($key !== 'header' && material_core_rows($query) !== $rows[$key]) { cancel_transaction(); return false; }
        verify_agreement_purchase_order_effect($intent, $snapshot['cart'], $number);
        if (material_core_context() !== $context || !requisition_purchase_authorized() || !commit_transaction()) { cancel_transaction(); return false; }
        return true;
    } catch (Exception $exception) { cancel_transaction(); return false;
    } catch (Throwable $exception) { cancel_transaction(); return false; }
}
