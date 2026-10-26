<?php
/** Native requisition lines and their exact Warehouse partners commit together. */
require_once __DIR__.'/purch_requisition_header_db.inc';

/**
 * Admit a native line under current parent/source locks. A line hint only locates
 * its parent; it never authorizes a write. Omitted metadata retains locked bytes.
 * This bounded native command grants no generic Entity or Dimension deletion authority.
 */
function write_purch_requisition_line($operation, $key, $data)
{
    $context = purch_requisition_header_context();
    $identity = dimension_writer_reference_id($key);
    if ($context === false || !$identity || !in_array($operation, array('add','update','delete'), true)) return false;
    try {
        if (!begin_transaction()) return false;
        $parent = $identity;
        if ($operation !== 'add') {
            $hint = purch_requisition_header_rows('SELECT requisition_id FROM '.TB_PREF.'purch_requisition_lines WHERE id='.$identity);
            $parent = $hint ? dimension_writer_reference_id($hint[0]['requisition_id']) : false;
        }
        if (!$parent) { cancel_transaction(); return false; }
        $result = dimension_ordinary_master_write('purch_requisitions', $parent, array(),
            function($ignored) use ($operation, $identity, $parent, $data, $context) {
                $headers = purch_requisition_header_rows('SELECT * FROM '.TB_PREF.'purch_requisitions WHERE id='.$parent.' FOR UPDATE');
                if (!$headers || !in_array($headers[0]['status'], array('draft','rejected'), true)) return false;
                $header = $headers[0];
                $source = dimension_writer_reference_id($header['material_request_id']);
                if (!$source) return false;
                $sources = purch_requisition_header_rows('SELECT * FROM '.TB_PREF.'wh_material_requests WHERE request_id='.$source.' FOR UPDATE');
                if (!$sources || $sources[0]['request_type'] !== 'purchase'
                    || !in_array($sources[0]['status'], array('draft','rejected'), true)
                    || !empty($sources[0]['linked_doc_type']) || !empty($sources[0]['linked_doc_no'])) return false;
                $links = purch_requisition_header_rows('SELECT id FROM '.TB_PREF.'purch_requisitions WHERE material_request_id='.$source.' ORDER BY id FOR UPDATE');
                if (!$links || count($links) !== 1 || (int)$links[0]['id'] !== $parent) return false;
                // Sourced RFQs retain this requisition's original quantities and identities.
                $rfqs = purch_requisition_header_rows('SELECT id FROM '.TB_PREF.'purch_rfq WHERE requisition_id='.$parent.' ORDER BY id FOR UPDATE');
                if ($rfqs === false || $rfqs) return false;
                $old = null; $material = null; $material_id = null;
                if ($operation !== 'add') {
                    $rows = purch_requisition_header_rows('SELECT * FROM '.TB_PREF.'purch_requisition_lines WHERE id='.$identity.' FOR UPDATE');
                    if (!$rows || (int)$rows[0]['requisition_id'] !== $parent) return false;
                    $old = $rows[0];
                    if ((float)$old['qty_ordered'] != 0 || (int)$old['po_number'] != 0
                        || !in_array($old['status'], array('pending','rejected'), true)) return false;
                    $material_id = dimension_writer_reference_id($old['material_request_line_id']);
                    if (!$material_id) return false;
                    $rows = purch_requisition_header_rows('SELECT * FROM '.TB_PREF.'wh_material_request_lines WHERE line_id='.$material_id.' FOR UPDATE');
                    if (!$rows || (int)$rows[0]['request_id'] !== $source || (float)$rows[0]['qty_fulfilled'] != 0
                        || $rows[0]['stock_id'] !== $old['stock_id'] || (float)$rows[0]['qty_requested'] != (float)$old['quantity']) return false;
                    $material = $rows[0];
                    $links = purch_requisition_header_rows('SELECT id FROM '.TB_PREF.'purch_requisition_lines WHERE material_request_line_id='.$material_id.' ORDER BY id FOR UPDATE');
                    if (!$links || count($links) !== 1 || (int)$links[0]['id'] !== $identity) return false;
                }
                if ($operation !== 'delete') {
                    if (!can_save_requisition_line_values($data['quantity'], $data['estimated_unit_price'])
                        || !is_finite((float)$data['quantity']) || !is_finite((float)$data['estimated_unit_price'])
                        || dimension_writer_stock_id($data['stock_id']) === false
                        || dimension_writer_reference_id($data['preferred_supplier_id']) === false) return false;
                    // The Warehouse partner stores DECIMAL(15,4), unlike native DOUBLE.
                    // Admit machine epsilon only; never silently round a business quantity.
                    $quantity = round((float)$data['quantity'], 4);
                    if ($quantity <= 0 || $quantity > 99999999999.9999
                        || abs($quantity - (float)$data['quantity']) > 0.000000001) return false;
                    // Preserve all DECIMAL digits when db_escape stringifies values.
                    $data['quantity'] = number_format($quantity, 4, '.', '');
                    $items = purch_requisition_header_rows('SELECT stock_id,description,units,inactive FROM '.TB_PREF.'stock_master WHERE stock_id='.db_escape($data['stock_id']).' FOR UPDATE');
                    if (!$items || $items[0]['stock_id'] !== $data['stock_id']
                        || ($items[0]['inactive'] && (!$old || $old['stock_id'] !== $data['stock_id']))) return false;
                    if ($data['preferred_supplier_id'] && !purch_requisition_header_rows('SELECT supplier_id FROM '.TB_PREF.'suppliers WHERE supplier_id='.(int)$data['preferred_supplier_id'].' FOR UPDATE')) return false;
                    if ($data['description'] === '') $data['description'] = $items[0]['description'];
                    if ($data['unit_of_measure'] === '') $data['unit_of_measure'] = $items[0]['units'];
                    $data['required_date'] = $header['required_date'];
                    // Warehouse quantities are stock units; no conversion contract exists here.
                    if ($data['unit_of_measure'] !== $items[0]['units']
                        && (!$old || $data['stock_id'] !== $old['stock_id'] || $data['unit_of_measure'] !== $old['unit_of_measure'])) return false;
                    if ($data['custom_data'] === null && $old) $data['custom_data'] = $old['custom_data'];
                    $encoded = encode_purch_requisition_custom_data($data['custom_data']);
                    if ($encoded === null && !in_array($data['custom_data'], array(null,'',array()), true)) return false;
                    $data['custom_data'] = $encoded;
                }
                $next = $operation === 'delete' ? array() : array('custom_data'=>$data['custom_data']);
                return dimension_ordinary_master_write('purch_requisition_lines', $old ? $identity : null, $next,
                    function($admitted) use ($operation, $identity, $parent, $source, $material_id, $data, $context) {
                        // The paired Warehouse retained payload is independently fenced.
                        return dimension_ordinary_master_write('wh_material_request_lines', $material_id, array(),
                            function($ignored) use ($operation, $identity, $parent, $source, $material_id, $data, $context) {
                                if (purch_requisition_header_context() !== $context) return false;
                                $line_id = $identity;
                                if ($operation === 'delete') {
                                    if (!db_query('DELETE FROM '.TB_PREF.'purch_requisition_lines WHERE id='.$identity)
                                        || !db_query('DELETE FROM '.TB_PREF.'wh_material_request_lines WHERE line_id='.$material_id)) return false;
                                } else {
                                    $sets = array();
                                    foreach (array('stock_id','description','quantity','unit_of_measure','estimated_unit_price','preferred_supplier_id','notes','custom_data') as $field)
                                        $sets[] = $field.'='.($field === 'custom_data' ? sql_purch_requisition_header_payload($data[$field]) : db_escape($data[$field]));
                                    $warehouse_sets = 'stock_id='.db_escape($data['stock_id']).',qty_requested='.db_escape($data['quantity']).',memo='.db_escape($data['notes'])
                                        .',required_date='.($data['required_date'] ? db_escape($data['required_date']) : 'NULL');
                                    if ($operation === 'add') {
                                        if (!db_query('INSERT INTO '.TB_PREF.'wh_material_request_lines SET request_id='.$source.',qty_fulfilled=0,'.$warehouse_sets)) return false;
                                        $material_id = db_insert_id();
                                        if (!$material_id || !db_query('INSERT INTO '.TB_PREF.'purch_requisition_lines SET requisition_id='.$parent.',material_request_line_id='.(int)$material_id.",status='pending',qty_ordered=0,po_number=0,".implode(',', $sets))) return false;
                                        $line_id = db_insert_id();
                                    } else {
                                        if (!db_query('UPDATE '.TB_PREF.'purch_requisition_lines SET '.implode(',', $sets).' WHERE id='.$identity)
                                            || !db_query('UPDATE '.TB_PREF.'wh_material_request_lines SET '.$warehouse_sets.' WHERE line_id='.$material_id)) return false;
                                    }
                                }
                                if (!db_query('UPDATE '.TB_PREF.'purch_requisitions SET total_estimated=(SELECT COALESCE(SUM(quantity*estimated_unit_price),0) FROM '.TB_PREF.'purch_requisition_lines WHERE requisition_id='.$parent.') WHERE id='.$parent)) return false;
                                if ($operation === 'add' && !db_preserve_insert_id($line_id)) return false;
                                return $operation === 'add' ? $line_id : true;
                            }, true, $operation === 'delete');
                    }, true, $operation === 'delete');
            }, true);
        if (!$result || purch_requisition_header_context() !== $context || !commit_transaction()) { cancel_transaction(); return false; }
        return $result;
    } catch (Exception $exception) { cancel_transaction(); return false;
    } catch (Throwable $exception) { cancel_transaction(); return false; }
}
