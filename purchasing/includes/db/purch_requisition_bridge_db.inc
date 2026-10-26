<?php
/** Atomic Warehouse draft bridge; approval transitions belong to the shared core. */
require_once __DIR__.'/purch_requisition_header_db.inc';

/** Recheck both native permissions before reading the source or returning its link. */
function purch_requisition_bridge_context()
{
    $context = purch_requisition_header_context();
    return $context !== false && $_SESSION['wa_current_user']->can_access('SA_MATERIALREQUEST') ? $context : false;
}

/**
 * Construct one complete draft under current source locks. Non-draft sources
 * require a separately verified shared-core approval adapter; never fabricate
 * requisition approval from a Warehouse status. Existing links are read only.
 * Warehouse retained metadata stays with its original owner, byte for byte.
 */
function write_purch_requisition_bridge($material_request_id)
{
    $context = purch_requisition_bridge_context();
    $source = dimension_writer_reference_id($material_request_id);
    if ($context === false || !$source) return false;
    try {
        if (!begin_transaction()) return false;
        $rows = purch_requisition_header_rows('SELECT * FROM '.TB_PREF.'wh_material_requests WHERE request_id='.$source.' FOR UPDATE');
        if (!$rows || $rows[0]['request_type'] !== 'purchase') { cancel_transaction(); return false; }
        $header = $rows[0];
        $links = purch_requisition_header_rows('SELECT id FROM '.TB_PREF.'purch_requisitions WHERE material_request_id='.$source.' ORDER BY id FOR UPDATE');
        if ($links === false || count($links) > 1) { cancel_transaction(); return false; }
        if ($links) {
            if (purch_requisition_bridge_context() !== $context || !commit_transaction()) { cancel_transaction(); return false; }
            return (int)$links[0]['id'];
        }
        if ($header['status'] !== 'draft' || !empty($header['linked_doc_type']) || !empty($header['linked_doc_no'])) { cancel_transaction(); return false; }
        $lines = purch_requisition_header_rows('SELECT * FROM '.TB_PREF.'wh_material_request_lines WHERE request_id='.$source.' ORDER BY line_id FOR UPDATE');
        if (!$lines) { cancel_transaction(); return false; }
        $id = add_purch_requisition($header['requested_by'], 0, $header['required_date'],
            $header['memo'] ? $header['memo'] : '', $header['warehouse_loc_code'], 'normal',
            $header['memo'] ? $header['memo'] : '', 0, 0, '', $header['request_date'], $source,
            array('bridged_from_material_request'=>1));
        if (!$id || purch_requisition_bridge_context() !== $context) { cancel_transaction(); return false; }
        $created = purch_requisition_header_rows('SELECT * FROM '.TB_PREF.'purch_requisitions WHERE id='.(int)$id.' FOR UPDATE');
        $children = purch_requisition_header_rows('SELECT id FROM '.TB_PREF.'purch_requisition_lines WHERE requisition_id='.(int)$id.' FOR UPDATE');
        if (!$created || $children === false || $children || $created[0]['status'] !== 'draft'
            || (int)$created[0]['material_request_id'] !== $source || (int)$created[0]['dimension_id'] !== 0
            || (int)$created[0]['dimension2_id'] !== 0 || (int)$created[0]['requester_id'] !== (int)$header['requested_by']
            || $created[0]['location'] !== $header['warehouse_loc_code'] || $created[0]['request_date'] !== $header['request_date']
            || $created[0]['required_date'] !== $header['required_date']) { cancel_transaction(); return false; }
        foreach ($lines as $line) {
            $linked = purch_requisition_header_rows('SELECT id FROM '.TB_PREF.'purch_requisition_lines WHERE material_request_line_id='.(int)$line['line_id'].' ORDER BY id FOR UPDATE');
            $items = purch_requisition_header_rows('SELECT stock_id,description,units,inactive FROM '.TB_PREF.'stock_master WHERE stock_id='.db_escape($line['stock_id']).' FOR UPDATE');
            if ($linked === false || $linked || !$items || $items[0]['inactive'] || $items[0]['stock_id'] !== $line['stock_id']
                || !is_numeric($line['qty_requested']) || !is_finite((float)$line['qty_requested'])
                || (float)$line['qty_requested'] <= 0 || (float)$line['qty_fulfilled'] != 0) { cancel_transaction(); return false; }
            // Current pricing and vendor identity, without a stale consistent-read helper.
            $pricing = purch_requisition_header_rows('SELECT supplier_id,price,conversion_factor FROM '.TB_PREF.'purch_data WHERE stock_id='.db_escape($line['stock_id']).' ORDER BY supplier_id FOR UPDATE');
            if ($pricing === false) { cancel_transaction(); return false; }
            $supplier = 0; $price = 0;
            if ($pricing) {
                $supplier = dimension_writer_reference_id($pricing[0]['supplier_id']);
                $factor = (float)$pricing[0]['conversion_factor'] > 0 ? (float)$pricing[0]['conversion_factor'] : 1;
                $price = round((float)$pricing[0]['price'] / $factor, user_price_dec());
                if (!$supplier || !is_finite($price) || $price < 0
                    || !purch_requisition_header_rows('SELECT supplier_id FROM '.TB_PREF.'suppliers WHERE supplier_id='.$supplier.' AND inactive=0 FOR UPDATE')) { cancel_transaction(); return false; }
            }
            $result = dimension_ordinary_master_write('wh_material_request_lines', (int)$line['line_id'], array(),
                function($ignored) use ($line, $items, $id, $supplier, $price) {
                    $payload = encode_purch_requisition_custom_data(array('bridged_from_material_request_line'=>(int)$line['line_id']));
                    return dimension_ordinary_master_write('purch_requisition_lines', null, array('custom_data'=>$payload),
                        function($admitted) use ($line, $items, $id, $supplier, $price, $payload) {
                            return db_query('INSERT INTO '.TB_PREF.'purch_requisition_lines SET requisition_id='.(int)$id
                                .',material_request_line_id='.(int)$line['line_id'].',stock_id='.db_escape($line['stock_id'])
                                .',description='.db_escape($items[0]['description']).',quantity='.db_escape($line['qty_requested'])
                                .',unit_of_measure='.db_escape($items[0]['units']).',estimated_unit_price='.db_escape($price)
                                .',preferred_supplier_id='.$supplier.',notes='.($line['memo'] === null ? 'NULL' : db_escape($line['memo']))
                                .",qty_ordered=0,po_number=0,status='pending',custom_data=".sql_purch_requisition_header_payload($payload));
                        }, true);
                }, true);
            if (!$result) { cancel_transaction(); return false; }
        }
        if (!db_query('UPDATE '.TB_PREF.'purch_requisitions SET total_estimated=(SELECT COALESCE(SUM(quantity*estimated_unit_price),0) FROM '.TB_PREF.'purch_requisition_lines WHERE requisition_id='.(int)$id.') WHERE id='.(int)$id)) { cancel_transaction(); return false; }
        $expected_header = purch_requisition_header_rows('SELECT * FROM '.TB_PREF.'purch_requisitions WHERE id='.(int)$id.' FOR UPDATE');
        $expected_lines = purch_requisition_header_rows('SELECT * FROM '.TB_PREF.'purch_requisition_lines WHERE requisition_id='.(int)$id.' ORDER BY id FOR UPDATE');
        $expected_links = purch_requisition_header_rows('SELECT id FROM '.TB_PREF.'purch_requisitions WHERE material_request_id='.$source.' ORDER BY id FOR UPDATE');
        if (!$expected_links || count($expected_links) !== 1 || (int)$expected_links[0]['id'] !== (int)$id) { cancel_transaction(); return false; }
        $hook_data = array('requisition_id'=>(int)$id, 'material_request_id'=>$source);
        hook_invoke_all('purch_requisition_from_mr', $hook_data);
        // Hooks cannot change source, result, actor or company under this command.
        if (!$expected_header || !$expected_lines || purch_requisition_bridge_context() !== $context
            || purch_requisition_header_rows('SELECT * FROM '.TB_PREF.'wh_material_requests WHERE request_id='.$source.' FOR UPDATE') !== array($header)
            || purch_requisition_header_rows('SELECT * FROM '.TB_PREF.'wh_material_request_lines WHERE request_id='.$source.' ORDER BY line_id FOR UPDATE') !== $lines
            || purch_requisition_header_rows('SELECT * FROM '.TB_PREF.'purch_requisitions WHERE id='.(int)$id.' FOR UPDATE') !== $expected_header
            || purch_requisition_header_rows('SELECT * FROM '.TB_PREF.'purch_requisition_lines WHERE requisition_id='.(int)$id.' ORDER BY id FOR UPDATE') !== $expected_lines
            || purch_requisition_header_rows('SELECT id FROM '.TB_PREF.'purch_requisitions WHERE material_request_id='.$source.' ORDER BY id FOR UPDATE') !== $expected_links
            || !commit_transaction()) { cancel_transaction(); return false; }
        db_preserve_insert_id($id);
        return (int)$id;
    } catch (Exception $exception) { cancel_transaction(); return false;
    } catch (Throwable $exception) { cancel_transaction(); return false; }
}
