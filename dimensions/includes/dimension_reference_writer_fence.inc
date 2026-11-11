<?php
/**
 * DIM-LFC-001B / B02: bounded ordinary master-reference writer serialization.
 *
 * A writer must lock its existing owner row and each referenced Dimension master
 * in the SAME transaction before a create/change/remove. Master locks are taken
 * in ascending numeric order, including former references. New references must
 * be on their correct axis and open; unchanged legacy closed/orphan references
 * remain readable and maintainable, never silently remapped.
 *
 * This is NOT a deletion certificate. B02 remains open until every writer,
 * including direct Entity/import/legacy paths, is covered. B10 must reconcile
 * lock ordering with all other modules before permitting Dimension deletion.
 */
require_once __DIR__.'/dimension_master_payload_guard.inc';

/**
 * Admit a generic response write under the same parent-first locks as native
 * activity. Parent hints never establish authority; exact item/vendor links are
 * rechecked with current locking reads and cannot be reassigned by maintenance.
 * This retains Entity's single-row contract, not a complete response submission.
 */
function dimension_rfq_response_write($key, $next, $callback, $remove = false)
{
    if (!is_array($next) || !is_callable($callback)) return false;
    try {
        if (!begin_transaction()) return false;
        $identity = $key === null ? null : dimension_writer_reference_id($key);
        if ($identity === false || $identity === 0) { cancel_transaction(); return false; }
        $links = $next;
        if ($identity !== null) {
            $rows = db_query('SELECT rfq_vendor_id, rfq_item_id FROM '.TB_PREF.'purch_rfq_vendor_lines WHERE id='.$identity);
            $links = $rows ? db_fetch($rows) : false;
        }
        $vendor = is_array($links) && isset($links['rfq_vendor_id']) ? dimension_writer_reference_id($links['rfq_vendor_id']) : false;
        $item = is_array($links) && isset($links['rfq_item_id']) ? dimension_writer_reference_id($links['rfq_item_id']) : false;
        if (!$vendor || !$item) { cancel_transaction(); return false; }
        foreach (array('rfq_vendor_id'=>$vendor, 'rfq_item_id'=>$item) as $field=>$value)
            if (array_key_exists($field, $next) && dimension_writer_reference_id($next[$field]) !== $value) { cancel_transaction(); return false; }
        $rows = db_query('SELECT rfq_id FROM '.TB_PREF.'purch_rfq_vendors WHERE id='.$vendor);
        $hint = $rows ? db_fetch($rows) : false;
        $parent = $hint ? dimension_writer_reference_id($hint['rfq_id']) : false;
        if (!$parent) { cancel_transaction(); return false; }
        $result = dimension_ordinary_master_write('purch_rfq', $parent, array(),
            function($admitted) use ($identity, $vendor, $item, $parent, $next, $callback, $remove) {
                $rows = db_query('SELECT status FROM '.TB_PREF.'purch_rfq WHERE id='.$parent.' FOR UPDATE');
                $header = $rows ? db_fetch($rows) : false;
                if (!$header || !in_array($header['status'], array('draft','sent','received','evaluated'), true)) return false;
                // Match the native activity lock order, including sibling rows.
                foreach (array('purch_rfq_items','purch_rfq_vendors') as $table) {
                    $rows = db_query('SELECT * FROM '.TB_PREF.$table.' WHERE rfq_id='.$parent.' ORDER BY id FOR UPDATE');
                    if (!$rows) return false;
                    $found = false;
                    while ($row = db_fetch($rows))
                        if ((int)$row['id'] === ($table === 'purch_rfq_items' ? $item : $vendor)) $found = $row;
                    if (!$found) return false;
                    if ($table === 'purch_rfq_vendors') $vendor_row = $found;
                }
                if ((int)$vendor_row['is_winner'] || $vendor_row['status'] === 'awarded') return false;
                $rows = db_query('SELECT supplier_id FROM '.TB_PREF.'suppliers WHERE supplier_id='.(int)$vendor_row['supplier_id'].' FOR UPDATE');
                if (!$rows || !db_fetch($rows)) return false;
                if ($identity !== null) {
                    $rows = db_query('SELECT rfq_vendor_id, rfq_item_id FROM '.TB_PREF.'purch_rfq_vendor_lines WHERE id='.$identity.' FOR UPDATE');
                    $current = $rows ? db_fetch($rows) : false;
                    if (!$current || (int)$current['rfq_vendor_id'] !== $vendor || (int)$current['rfq_item_id'] !== $item) return false;
                }
                return dimension_ordinary_master_write('purch_rfq_vendor_lines', $identity, $next, $callback, true, $remove);
            }, true);
        $insert_id = $result && $identity === null ? db_insert_id() : 0;
        if (!$result || !commit_transaction()) { cancel_transaction(); return false; }
        if ($identity === null && !db_preserve_insert_id($insert_id)) return false;
        return $result;
    } catch (Exception $exception) { cancel_transaction(); return false;
    } catch (Throwable $exception) { cancel_transaction(); return false; }
}

/** Canonical native DB coordinate; only zero denotes a blank reference. */
function dimension_writer_reference_id($value)
{
    if ($value === null || $value === '') return 0;
    if (is_int($value)) return $value >= 0 ? $value : false;
    if (!is_string($value) || !preg_match('/^(?:0|[1-9][0-9]*)$/D', $value)) return false;
    $integer = (int)$value;
    return $integer >= 0 && (string)$integer === $value ? $integer : false;
}

/** Validate an exact native stock identity without numeric coercion or truncation. */
function dimension_writer_stock_id($value)
{
    return is_string($value) && trim($value) === $value
        && preg_match('/^[^\x00-\x1f\x7f]{1,20}$/uD', $value) ? $value : false;
}

/** Normalize chart identities exactly as native db_escape, before locking varchar(15). */
function dimension_writer_chart_id($value)
{
    if (is_int($value) && $value >= 0) $value = (string)$value;
    if (!is_string($value) || trim($value) !== $value
        || !preg_match('/^[^\x00-\x1f\x7f]+$/uD', $value)) return false;
    $value = html_specials_encode(html_entity_decode($value, ENT_QUOTES,
        $_SESSION['language']->encoding === 'iso-8859-2' ? 'ISO-8859-1' : $_SESSION['language']->encoding));
    return preg_match('/^[^\x00-\x1f\x7f]{1,15}$/uD', $value) ? $value : false;
}

/** Normalize the native key, including the two string chart-hierarchy identifiers. */
function dimension_writer_master_id($owner,$value)
{
    if($owner==='stock_master')return dimension_writer_stock_id($value);
    if(in_array($owner,array('chart_master','chart_types','chart_class','locations'),true)){
        $id=dimension_writer_chart_id($value);$limit=$owner==='chart_types'?10:($owner==='chart_class'?3:($owner==='locations'?5:15));
        return $id!==false && preg_match('/^[^\x00-\x1f\x7f]{1,'.$limit.'}$/uD',$id)?$id:false;
    }
    return dimension_writer_reference_id($value);
}

/**
 * Fail closed on invalid owner/identity, lock or query failures. The caller's
 * original authorization checks are still required; this does not grant access.
 * The callback receives the normalized payload and executes while both owner
 * and master locks remain held. Its result retains the native Entity contract.
 * Entity callers may opt into partial data: omitted coordinates are validated
 * and locked from current owner state but are never added to the write payload.
 */
function dimension_ordinary_master_write($owner, $key, $next, $callback, $partial = false, $remove = false)
{
    $sources = array('suppliers'=>'supplier_id', 'debtors_master'=>'debtor_no', 'quick_entry_lines'=>'id', 'stock_category'=>'category_id', 'stock_master'=>'stock_id', 'chart_master'=>'account_code', 'bank_accounts'=>'id', 'budget_trans'=>'id', 'purch_requisition_lines'=>'id', 'wh_material_request_lines'=>'line_id', 'wh_transfer_order_lines'=>'line_id', 'wh_material_requests'=>'request_id', 'wh_transfer_orders'=>'transfer_id', 'purch_requisitions'=>'id', 'purch_rfq'=>'id', 'purch_rfq_items'=>'id', 'purch_rfq_vendors'=>'id', 'purch_rfq_vendor_lines'=>'id');
    $sources['warranty_provision_log'] = 'id';
    $sources['gl_trans'] = 'counter';
    $sources['bank_trans'] = 'id';
    $sources['supp_invoice_items'] = 'id';
    $sources['debtor_trans'] = 'trans_no';
    $sources['supp_trans'] = 'trans_no';
    $sources['debtor_trans_details'] = 'id';
    $sources['stock_moves'] = 'trans_id';
    $sources['areas']='area_code';$sources['tax_types']='id';$sources['chart_types']='id';$sources['chart_class']='cid';
    $sources['purch_orders']='order_no';$sources['purch_order_details']='po_detail_item';$sources['grn_batch']='id';$sources['grn_items']='id';$sources['vendor_evaluations']='id';$sources['vendor_performance_log']='id';$sources['purch_matching_config']='id';$sources['purch_matching_exceptions']='id';$sources['purch_bill_control_policy']='id';$sources['purch_order_templates']='id';$sources['purch_order_template_lines']='id';$sources['procurement_plan']='id';$sources['procurement_plan_lines']='id';$sources['sales_quotation_templates']='id';$sources['sales_quotation_template_lines']='id';$sources['sales_discount_programs']='id';$sources['sales_coupons']='id';$sources['sales_commission_plans']='id';$sources['sales_commission_entries']='id';$sources['sales_pricelists']='id';$sources['sales_pricelist_rules']='id';$sources['sales_rma']='id';$sources['sales_rma_lines']='id';$sources['wh_storage_categories']='id';$sources['wh_putaway_rules']='rule_id';$sources['wh_routes']='route_id';$sources['wh_route_rules']='rule_id';$sources['wh_replenishment_rules']='rule_id';$sources['wh_cycle_count_plans']='plan_id';$sources['wh_scrap']='scrap_id';$sources['wh_picking_waves']='wave_id';$sources['wh_packages']='package_id';$sources['wh_return_orders']='return_id';$sources['wh_return_order_lines']='line_id';$sources['wh_consignment_stock']='id';$sources['wh_cycle_counts']='count_id';$sources['workorders']='id';$sources['wo_issue_items']='id';$sources['udi_registrations']='id';$sources['fsma_tracking_events']='id';$sources['dscsa_transactions']='id';$sources['quality_inspections']='id';$sources['recall_campaigns']='id';$sources['warranty_claims']='id';$sources['serial_numbers']='id';$sources['stock_batches']='id';$sources['wh_operations']='op_id';$sources['wh_operation_lines']='line_id';$sources['wh_locations']='loc_id';$sources['wh_bin_stock']='id';$sources['locations']='loc_code';
    $sources['vendor_pricelists']='id';$sources['item_codes']='id';$sources['sales_pos']='id';$sources['sales_types']='id';$sources['salesman']='salesman_code';$sources['prices']='id';
    $sources['sales_agreements'] = 'id';
    $sources['sales_agreement_lines'] = 'id';
    $sources['sales_orders'] = 'order_no';
    $sources['sales_order_details'] = 'id';
    $sources['purch_agreements'] = 'id';
    $sources['purch_agreement_lines'] = 'id';
    $compound = in_array($owner,array('bom','purch_data'),true);
    if ($compound) $sources[$owner] = 'custom_data';
    if (!isset($sources[$owner]) || !is_callable($callback) || !is_array($next)) return false;
    $columns = $owner === 'stock_category' ? array('dflt_dim1'=>1, 'dflt_dim2'=>2) : array('dimension_id'=>1, 'dimension2_id'=>2);
    // Chart and bank accounts retain JSON but have no scalar Dimension slots.
    if ($owner === 'purch_requisition_lines' || $owner === 'wh_material_request_lines' || $owner === 'wh_transfer_order_lines' || $owner === 'wh_material_requests' || $owner === 'wh_transfer_orders' || $owner === 'chart_master' || $owner === 'bank_accounts' || $owner === 'purch_rfq_items' || $owner === 'purch_rfq_vendors' || $owner === 'purch_rfq_vendor_lines') $columns = array();
    if ($owner === 'purch_agreement_lines' || $owner === 'sales_agreement_lines') $columns = array();
    if (in_array($owner,array('sales_orders','sales_order_details','supp_trans','debtor_trans_details','stock_moves','areas','tax_types','chart_types','chart_class','sales_pos','sales_types','salesman','prices','item_codes','vendor_pricelists','purch_orders','purch_order_details','grn_batch','grn_items','vendor_evaluations','vendor_performance_log','purch_matching_config','purch_matching_exceptions','purch_bill_control_policy','purch_order_templates','purch_order_template_lines','procurement_plan','procurement_plan_lines','sales_quotation_templates','sales_quotation_template_lines','sales_discount_programs','sales_coupons','sales_commission_plans','sales_commission_entries','sales_pricelists','sales_pricelist_rules','sales_rma','sales_rma_lines','wh_storage_categories','wh_putaway_rules','wh_routes','wh_route_rules','wh_replenishment_rules','wh_cycle_count_plans','wh_scrap','wh_picking_waves','wh_packages','wh_return_orders','wh_return_order_lines','wh_consignment_stock','wh_cycle_counts','workorders','wo_issue_items','udi_registrations','fsma_tracking_events','dscsa_transactions','quality_inspections','recall_campaigns','warranty_claims','serial_numbers','stock_batches','wh_operations','wh_operation_lines','wh_locations','wh_bin_stock','locations'),true)) $columns = array();
    $composite = in_array($owner,array('debtor_trans','supp_trans','sales_orders'),true);
    if ($compound) $columns = array();
    $string_key = in_array($owner,array('stock_master','chart_master','chart_types','chart_class','locations'),true);
    $names = array_keys($columns);
    if (!$partial) foreach ($names as $name) if (!array_key_exists($name, $next)) return false;
    $new = array();
    foreach ($names as $column) {
        if (!array_key_exists($column, $next)) continue;
        $id = dimension_writer_reference_id($next[$column]);
        if ($id === false) return false;
        $new[$column] = $id;
        $next[$column] = $id;
    }
    $identity = null;
    $type = null;
    if ($key !== null) {
        if ($compound) {
            $identity = dimension_composite_metadata_key($owner,$key);
            if ($identity === false) return false;
        } elseif ($composite) {
            $type_field = $owner === 'sales_orders' ? 'trans_type' : 'type';
            $number_field = $sources[$owner];
            if (!is_array($key) || !isset($key[$type_field],$key[$number_field]) || count($key)!==(in_array($owner,array('supp_trans','debtor_trans'),true)?3:2)
                || ($type=dimension_writer_reference_id($key[$type_field]))===false) return false;
            if($owner==='debtor_trans' && (!isset($key['debtor_no']) || !($debtor=dimension_writer_reference_id($key['debtor_no']))))return false;
            if($owner==='supp_trans' && (!isset($key['supplier_id']) || !($supplier=dimension_writer_reference_id($key['supplier_id']))))return false;
            $key=$key[$number_field];
        }
        if (!$compound) {
            $identity = dimension_writer_master_id($owner,$key);
            if ($identity === false || $identity === 0) return false;
        }
    }
    try {
        if (!function_exists('begin_transaction') || !begin_transaction()) return false;
        if ($compound && !db_query('SELECT custom_data FROM '.TB_PREF.$owner.' LIMIT 0')) { cancel_transaction(); return false; }
        if ($identity === null && in_array($owner, array('purch_agreements','purch_agreement_lines','sales_agreements','sales_agreement_lines','sales_orders','sales_order_details','supp_trans','debtor_trans_details','stock_moves','areas','tax_types','chart_types','chart_class','sales_pos','sales_types','salesman','prices','item_codes','vendor_pricelists','purch_orders','purch_order_details','grn_batch','grn_items','vendor_evaluations','vendor_performance_log','purch_matching_config','purch_matching_exceptions','purch_bill_control_policy','purch_order_templates','purch_order_template_lines','procurement_plan','procurement_plan_lines','sales_quotation_templates','sales_quotation_template_lines','sales_discount_programs','sales_coupons','sales_commission_plans','sales_commission_entries','sales_pricelists','sales_pricelist_rules','sales_rma','sales_rma_lines','wh_storage_categories','wh_putaway_rules','wh_routes','wh_route_rules','wh_replenishment_rules','wh_cycle_count_plans','wh_scrap','wh_picking_waves','wh_packages','wh_return_orders','wh_return_order_lines','wh_consignment_stock','wh_cycle_counts','workorders','wo_issue_items','udi_registrations','fsma_tracking_events','dscsa_transactions','quality_inspections','recall_campaigns','warranty_claims','serial_numbers','stock_batches','wh_operations','wh_operation_lines','wh_locations','wh_bin_stock','locations'), true)
            && !db_query('SELECT custom_data FROM '.TB_PREF.$owner.' LIMIT 0')) {
            cancel_transaction(); return false;
        }
        // An omitted payload must not certify a bank owner whose retained column
        // is unavailable. The transactional read also holds its metadata lock.
        if ($identity === null && in_array($owner, array('purch_requisition_lines', 'wh_material_request_lines', 'wh_transfer_order_lines', 'wh_material_requests', 'wh_transfer_orders', 'bank_accounts', 'budget_trans', 'purch_requisitions', 'purch_rfq', 'purch_rfq_items', 'purch_rfq_vendors', 'purch_rfq_vendor_lines'), true)
            && !db_query('SELECT custom_data FROM '.TB_PREF.$owner.' LIMIT 0')) {
            cancel_transaction(); return false;
        }
        $old = array_fill_keys($names, 0);
        $old_payload = null;
        if ($identity !== null) {
            // These two scalar owners have no retained payload column in
            // either canonical schema. Never infer absence for another owner.
            $payload_column = in_array($owner, array('quick_entry_lines','warranty_provision_log'), true) ? 'NULL AS custom_data' : 'custom_data';
            if ($string_key) $payload_column .= ', '.$sources[$owner].' AS owner_identity';
            $rows = db_query('SELECT '.(count($names) ? implode(', ', $names).', ' : '').$payload_column.' FROM '.TB_PREF.$owner
                .' WHERE '.($compound ? dimension_composite_metadata_predicate($owner,$identity) : $sources[$owner].'='.($string_key ? db_escape($identity) : $identity))
                .($composite ? ' AND '.($owner==='sales_orders'?'trans_type':'type').'='.$type : '').($owner==='supp_trans'?' AND supplier_id='.$supplier:($owner==='debtor_trans'?' AND debtor_no='.$debtor:'')).' FOR UPDATE');
            $found = $rows ? db_fetch($rows) : false;
            if (!is_array($found)) { cancel_transaction(); return false; }
            // A case-insensitive SQL key must not rename a historical string ID.
            if ($string_key && (!isset($found['owner_identity'])
                || $found['owner_identity'] !== $identity)) { cancel_transaction(); return false; }
            if (!array_key_exists('custom_data', $found)) { cancel_transaction(); return false; }
            $old_payload = $found['custom_data'];
            foreach ($old as $column=>$value) {
                $ref = dimension_writer_reference_id($found[$column]);
                if ($ref === false) { cancel_transaction(); return false; }
                $old[$column] = $ref;
            }
        }
        $next_payload = $remove ? null : (array_key_exists('custom_data', $next) ? $next['custom_data'] : $old_payload);
        $payload = dimension_master_payload_transition($old_payload, $next_payload, $remove);
        if ($payload === false) { cancel_transaction(); return false; }
        // Entity partial updates retain omitted coordinates from the current
        // locked row, never from a caller snapshot or an unlocked prior read.
        foreach ($old as $column=>$id) {
            if (!array_key_exists($column, $new)) $new[$column] = $id;
        }
        // No reference is ever omitted simply because it is being cleared.
        $locks = $payload['locks'];
        foreach (array($old, $new) as $coords)
            foreach ($coords as $id) if ($id > 0) $locks[$id] = $id;
        sort($locks, SORT_NUMERIC);
        $masters = array();
        foreach ($locks as $id) {
            $rows = db_query('SELECT id, type_, closed FROM '.TB_PREF.'dimensions'
                .' WHERE id='.$id.' FOR UPDATE');
            if (!$rows) { cancel_transaction(); return false; }
            $masters[$id] = db_fetch($rows); // False is allowed only for an existing orphan.
        }
        foreach ($columns as $column=>$axis) {
            $id = $new[$column];
            if (!$id) continue;
            $unchanged = ($id === $old[$column]);
            $master = $masters[$id];
            if (!$unchanged && (!is_array($master) || (int)$master['type_'] !== $axis
                || (int)$master['closed'] !== 0)) { cancel_transaction(); return false; }
        }
        foreach ($payload['new'] as $reference) {
            $master = $masters[$reference[0]];
            if (!is_array($master) || (int)$master['type_'] !== $reference[1]
                || (int)$master['closed'] !== 0) { cancel_transaction(); return false; }
        }
        $result = call_user_func($callback, $next);
        if (!$result) { cancel_transaction(); return false; }
        $insert_id = $identity === null && !$composite && !$compound ? db_insert_id() : 0;
        if (!commit_transaction()) { cancel_transaction(); return false; }
        if ($identity === null && !$string_key && !$composite && !$compound && !db_preserve_insert_id($insert_id)) return false;
        return $result;
    } catch (Exception $exception) {
        cancel_transaction();
        return false;
    } catch (Throwable $exception) {
        cancel_transaction();
        return false;
    }
}

/**
 * Serialize bounded RFQ item/vendor maintenance with its current draft parent.
 * The initial parent lookup is only a hint; recheck identity under the parent
 * lock before touching the child. Reparenting is never a maintenance operation.
 * All retained cascade targets are checked before either native or Entity delete.
 * This boundary does not grant page authority or certify response/status writers.
 */
function dimension_rfq_child_write($owner, $key, $next, $callback, $remove = false)
{
    if (!in_array($owner, array('purch_rfq_items', 'purch_rfq_vendors'), true)
        || !is_array($next) || !is_callable($callback)) return false;
    try {
        if (!begin_transaction()) return false;
        $identity = $key === null ? null : dimension_writer_reference_id($key);
        if ($identity === false || $identity === 0) { cancel_transaction(); return false; }
        if ($identity === null) {
            $parent = isset($next['rfq_id']) ? dimension_writer_reference_id($next['rfq_id']) : false;
        } else {
            $rows = db_query('SELECT rfq_id FROM '.TB_PREF.$owner.' WHERE id='.$identity);
            $row = $rows ? db_fetch($rows) : false;
            $parent = $row ? dimension_writer_reference_id($row['rfq_id']) : false;
        }
        if (!$parent) { cancel_transaction(); return false; }
        $result = dimension_ordinary_master_write('purch_rfq', $parent, array(),
            function($header) use ($owner, $identity, $parent, $next, $callback, $remove) {
                $rows = db_query('SELECT status FROM '.TB_PREF.'purch_rfq WHERE id='.$parent.' FOR UPDATE');
                $row = $rows ? db_fetch($rows) : false;
                if (!$row || $row['status'] !== 'draft') return false;
                if ($identity !== null) {
                    $rows = db_query('SELECT * FROM '.TB_PREF.$owner.' WHERE id='.$identity.' FOR UPDATE');
                    $row = $rows ? db_fetch($rows) : false;
                    if (!$row || (int)$row['rfq_id'] !== $parent) return false;
                    if ($remove && $owner === 'purch_rfq_vendors'
                        && (in_array($row['status'], array('responded', 'awarded'), true) || (int)$row['is_winner'] === 1)) return false;
                }
                if (array_key_exists('rfq_id', $next) && dimension_writer_reference_id($next['rfq_id']) !== $parent) return false;
                if ($remove) {
                    $foreign = $owner === 'purch_rfq_items' ? 'rfq_item_id' : 'rfq_vendor_id';
                    $rows = db_query('SELECT custom_data FROM '.TB_PREF.'purch_rfq_vendor_lines WHERE '.$foreign.'='.$identity.' ORDER BY id FOR UPDATE');
                    if (!$rows) return false;
                    while ($line = db_fetch($rows))
                        if (!array_key_exists('custom_data', $line)
                            || dimension_master_payload_transition($line['custom_data'], null, true) === false) return false;
                }
                return dimension_ordinary_master_write($owner, $identity, $next, $callback, true, $remove);
            }, true);
        $insert_id = $result && $identity === null ? db_insert_id() : 0;
        if (!$result || !commit_transaction()) { cancel_transaction(); return false; }
        if ($identity === null && !db_preserve_insert_id($insert_id)) return false;
        return $result;
    } catch (Exception $exception) { cancel_transaction(); return false;
    } catch (Throwable $exception) { cancel_transaction(); return false; }
}
