<?php
/** Immutable requisition source commands through the existing native PO workflow. */

/** Read current source rows under the owning transaction rather than an MVCC snapshot. */
function read_requisition_purchase_rows($query) {
 $rows = db_query($query, 'could not verify requisition purchase source');
 if (!$rows) throw new RuntimeException('Requisition source is unavailable.');
 $result = array(); while ($row = db_fetch_assoc($rows)) $result[] = $row;
 return $result;
}

/** Header progress is derived from current locked lines; retain every other approved field. */
function requisition_purchase_header_intent($header) {
 unset($header['status']);
 return $header;
}

/** Normalize only verified derived Warehouse linkage so disjoint supplier approvals can finish. */
function requisition_purchase_material_intent($material) {
 if ($material) foreach (array('status','linked_doc_type','linked_doc_no','updated_at') as $field) unset($material[0][$field]);
 return $material;
}

/** Freeze all native fallback-pricing inputs before reusing the maintained price resolver. */
function read_requisition_purchase_pricing($supplier, $stock, $lock) {
 $where = 'supplier_id='.(int)$supplier.' AND stock_id='.db_escape($stock);
 $queries = array(
  'defaults'=>'SELECT * FROM '.TB_PREF.'purch_data WHERE '.$where.' ORDER BY supplier_id,stock_id',
  'vendor'=>'SELECT * FROM '.TB_PREF.'vendor_pricelists WHERE '.$where.' ORDER BY id',
  'agreements'=>'SELECT * FROM '.TB_PREF.'purch_agreements WHERE supplier_id='.(int)$supplier.' ORDER BY id',
  'agreement_lines'=>'SELECT line.* FROM '.TB_PREF.'purch_agreement_lines line INNER JOIN '.TB_PREF.'purch_agreements header ON header.id=line.agreement_id WHERE header.supplier_id='.(int)$supplier.' AND line.stock_id='.db_escape($stock).' ORDER BY line.id',
  'preferences'=>"SELECT * FROM ".TB_PREF."sys_prefs WHERE name IN ('use_purchase_agreements','use_vendor_pricelists') ORDER BY name");
 $result = array(); foreach ($queries as $key=>$query) $result[$key] = read_requisition_purchase_rows($query.($lock?' FOR UPDATE':''));
 // The maintained native resolver uses cached switches; bind those to these source rows too.
 $preferences = array(); foreach ($result['preferences'] as $row) $preferences[$row['name']] = $row['value'];
 foreach (array('use_purchase_agreements','use_vendor_pricelists') as $name)
  if ((bool)get_company_pref($name) !== !empty($preferences[$name])) throw new RuntimeException('Purchase pricing preferences changed; reload company settings.');
 return $result;
}

/** Admit source and destination authority before any source identity read. */
function requisition_purchase_authorized() {
 $cart = new purch_order();
 return !empty($GLOBALS['transaction_level']) && $cart->is_current_company()
  && isset($_SESSION['wa_current_user'])
  && $_SESSION['wa_current_user']->can_access('SA_PURCHREQUISITION')
  && $_SESSION['wa_current_user']->can_access('SA_PURCHASEORDER');
}

/** Lock a request and all its lines before partitioning an atomic multi-supplier command. */
function requisition_purchase_groups($id, $supplier_id, $line_ids = array()) {
 if (!requisition_purchase_authorized() || !is_scalar($id) || (string)(int)$id !== (string)$id || (int)$id <= 0
  || !is_scalar($supplier_id) || (string)(int)$supplier_id !== (string)$supplier_id || (int)$supplier_id < 0 || !is_array($line_ids))
  throw new RuntimeException('Requisition purchase authority or selection is unavailable.');
 $headers = read_requisition_purchase_rows('SELECT * FROM '.TB_PREF.'purch_requisitions WHERE id='.(int)$id.' FOR UPDATE');
 if (count($headers) !== 1 || !in_array($headers[0]['status'], array('approved','partially_ordered'), true))
  throw new RuntimeException('Requisition is unavailable.');
 $lines = read_requisition_purchase_rows('SELECT * FROM '.TB_PREF.'purch_requisition_lines WHERE requisition_id='.(int)$id.' ORDER BY id FOR UPDATE');
 $selected = array(); foreach ($line_ids as $line_id) {
  if (!is_scalar($line_id) || (string)(int)$line_id !== (string)$line_id || (int)$line_id <= 0)
   throw new RuntimeException('Requisition selection is invalid.');
  $selected[(int)$line_id] = true;
 }
 $groups = array(); $used = array();
 foreach ($lines as $line) {
  if (!is_finite((float)$line['quantity']) || !is_finite((float)$line['qty_ordered']) || (float)$line['quantity'] <= 0 || (float)$line['qty_ordered'] < 0) throw new RuntimeException('Requisition quantities require reconciliation.');
  if ($selected && !isset($selected[(int)$line['id']])) continue;
  $remaining = round((float)$line['quantity'] - (float)$line['qty_ordered'], get_qty_dec($line['stock_id']));
  if (!is_finite($remaining) || $remaining <= 0) continue;
  $supplier = (int)$supplier_id > 0 ? (int)$supplier_id : (int)$line['preferred_supplier_id'];
  if ($supplier <= 0) continue;
  $groups[$supplier][] = (int)$line['id']; $used[(int)$line['id']] = true;
 }
 if (!$groups || ($selected && count($selected) !== count($used))) throw new RuntimeException('Selected requisition lines are unavailable.');
 ksort($groups, SORT_NUMERIC); return $groups;
}

/** Build one exact supplier order from current locked source, item/default/tax and linked Warehouse rows. */
function prepare_requisition_purchase_order($id, $supplier_id, $line_ids = array(), $order_date = null) {
 $groups = requisition_purchase_groups($id, $supplier_id, $line_ids);
 if (count($groups) !== 1 || (int)$supplier_id <= 0) throw new RuntimeException('One supplier identity is required.');
 $used = reset($groups); $cart = new purch_order(); $pricing = array();
 $queries = array('header'=>'SELECT * FROM '.TB_PREF.'purch_requisitions WHERE id='.(int)$id.' FOR UPDATE');
 $header = read_requisition_purchase_rows($queries['header']); $header = $header[0];
 $queries['all_lines'] = 'SELECT * FROM '.TB_PREF.'purch_requisition_lines WHERE requisition_id='.(int)$id.' ORDER BY id FOR UPDATE';
 $all_lines = read_requisition_purchase_rows($queries['all_lines']);
 $queries['lines'] = 'SELECT line.*, item.description AS stock_description, item.units AS stock_units, item.editable AS stock_editable, item.tax_type_id AS stock_tax, tax.name AS stock_tax_name, purchase.supplier_description, purchase.price AS supplier_price, purchase.conversion_factor AS supplier_conversion FROM '.TB_PREF.'purch_requisition_lines line LEFT JOIN '.TB_PREF.'stock_master item ON item.stock_id=line.stock_id LEFT JOIN '.TB_PREF.'item_tax_types tax ON tax.id=item.tax_type_id LEFT JOIN '.TB_PREF.'purch_data purchase ON purchase.stock_id=line.stock_id AND purchase.supplier_id='.(int)$supplier_id.' WHERE line.requisition_id='.(int)$id.' AND line.id IN ('.implode(',', $used).') ORDER BY line.id FOR UPDATE';
 $lines = read_requisition_purchase_rows($queries['lines']);
 $queries['effects'] = 'SELECT po.*, detail.po_detail_item, detail.item_code, detail.description AS line_description, detail.quantity_ordered, detail.quantity_received, detail.qty_invoiced, detail.unit_price, detail.delivery_date FROM '.TB_PREF.'purch_orders po LEFT JOIN '.TB_PREF.'purch_order_details detail ON detail.order_no=po.order_no WHERE po.requisition_id='.(int)$id.' AND po.supplier_id='.(int)$supplier_id.' ORDER BY po.order_no,detail.po_detail_item FOR UPDATE';
 $effects = read_requisition_purchase_rows($queries['effects']);
 $queries['all_effects'] = str_replace(' AND po.supplier_id='.(int)$supplier_id, '', $queries['effects']);
 $all_effects = read_requisition_purchase_rows($queries['all_effects']);
 $queries['location'] = 'SELECT * FROM '.TB_PREF.'locations WHERE loc_code='.db_escape($header['location']).' AND inactive=0 FOR UPDATE';
 $location = read_requisition_purchase_rows($queries['location']);
 $queries['supplier'] = 'SELECT * FROM '.TB_PREF.'suppliers WHERE supplier_id='.(int)$supplier_id.' AND inactive=0 FOR UPDATE';
 $supplier = read_requisition_purchase_rows($queries['supplier']);
 if (count($supplier) !== 1 || count($location) !== 1) throw new RuntimeException('Supplier or location is unavailable.');
 $material = array();
 if ((int)$header['material_request_id'] > 0) {
  if (!$_SESSION['wa_current_user']->can_access('SA_MATERIALREQUEST')) throw new RuntimeException('Linked Warehouse authority is unavailable.');
  $queries['material'] = 'SELECT * FROM '.TB_PREF.'wh_material_requests WHERE request_id='.(int)$header['material_request_id'].' FOR UPDATE';
  $material = read_requisition_purchase_rows($queries['material']);
  if (count($material) !== 1 || !in_array($material[0]['status'], array('approved','ordered'), true)) throw new RuntimeException('Linked Warehouse source is unavailable.');
  if ($material[0]['status'] === 'ordered') {
   $matched = false; foreach ($all_effects as $effect) if ((int)$effect['order_no'] === (int)$material[0]['linked_doc_no']) $matched = true;
   if ((int)$material[0]['linked_doc_type'] !== ST_PURCHORDER || !$matched) throw new RuntimeException('Linked Warehouse order provenance is unavailable.');
  } elseif (!empty($material[0]['linked_doc_type']) || !empty($material[0]['linked_doc_no'])) throw new RuntimeException('Linked Warehouse source requires reconciliation.');
  $queries['material_lines'] = 'SELECT * FROM '.TB_PREF.'wh_material_request_lines WHERE request_id='.(int)$header['material_request_id'].' ORDER BY line_id FOR UPDATE';
  $material_lines = read_requisition_purchase_rows($queries['material_lines']);
 }
 if (!$material) $material_lines = array();
 $order_date = $order_date === null ? Today() : $order_date;
 if (!is_string($order_date) || !is_date($order_date)) throw new RuntimeException('Requisition order date is invalid.');
 $cart->trans_type = ST_PURCHORDER; $cart->orig_order_date = $order_date;
 $cart->due_date = $header['required_date'] ? sql2date($header['required_date']) : $order_date;
 if (!is_date($cart->due_date)) throw new RuntimeException('Requisition delivery date is invalid.');
 $cart->Location = $location[0]['loc_code']; $cart->delivery_address = $location[0]['location_name'];
 $cart->Comments = sprintf(_('Created from Purchase Requisition %s'), $header['reference']);
 $cart->requisition_id = (int)$id; get_supplier_details_to_order($cart, (int)$supplier_id, true); $cart->reference = 'auto';
 foreach ($lines as $line) {
  $quantity = round((float)$line['quantity'] - (float)$line['qty_ordered'], get_qty_dec($line['stock_id']));
  $price = (float)$line['estimated_unit_price'];
  if ($price <= 0) {
   if (!function_exists('get_purchase_price')) include_once($GLOBALS['path_to_root'].'/purchasing/includes/purchasing_db.inc');
   $inputs = read_requisition_purchase_pricing($supplier_id, $line['stock_id'], true);
   if ($inputs !== read_requisition_purchase_pricing($supplier_id, $line['stock_id'], false)) throw new RuntimeException('Current pricing requires a fresh source snapshot.');
   foreach ($inputs['preferences'] as $preference) if ((string)get_company_pref($preference['name']) !== (string)$preference['value']) throw new RuntimeException('Cached pricing policy changed; reload.');
   $price = (float)get_purchase_price($supplier_id, $line['stock_id']);
   if ($inputs !== read_requisition_purchase_pricing($supplier_id, $line['stock_id'], true)) throw new RuntimeException('Pricing source changed.');
   $pricing[$line['stock_id']] = $inputs;
  }
  if ($line['stock_description'] === null || !is_finite($quantity) || $quantity <= 0 || !is_finite($price) || $price < 0) throw new RuntimeException('Requisition line is unavailable.');
  $description = $line['description'] === null || $line['description'] === '' ? $line['stock_description'] : $line['description'];
  $units = $line['unit_of_measure'] !== '' ? $line['unit_of_measure'] : $line['stock_units'];
  $cart->add_to_order(count($cart->line_items), $line['stock_id'], $quantity, $description, $price, $units, $cart->due_date, 0, 0);
  $item = $cart->line_items[count($cart->line_items)-1];
  $item->item_description = !$line['stock_editable'] && $line['supplier_description'] !== null && $line['supplier_description'] !== '' ? $line['supplier_description'] : $description;
  $item->descr_editable = $line['stock_editable']; $item->units = $units; $item->tax_type = $line['stock_tax']; $item->tax_type_name = $line['stock_tax_name'];
 }
 if (!$cart->order_has_items() || count($lines) !== count($used)) throw new RuntimeException('Requisition selected lines are unavailable.');
 $tax = array(); if (!$cart->tax_included) {
  $tax = read_agreement_purchase_tax_configuration(false);
  if ($lines !== read_requisition_purchase_rows(substr($queries['lines'],0,-11))
   || (string)get_company_pref('tax_algorithm') !== (string)(empty($tax['algorithm']) ? null : $tax['algorithm'][0]['value'])
   || $tax !== read_agreement_purchase_tax_configuration(true)) throw new RuntimeException('Tax configuration changed; retry.');
 }
 return array('cart'=>$cart,'header'=>$header,'lines'=>$lines,'all_lines'=>$all_lines,'effects'=>$effects,'all_effects'=>$all_effects,'queries'=>$queries,'used'=>$used,'tax_configuration'=>$tax,'location'=>$location,'supplier'=>$supplier,'material'=>$material,'material_lines'=>$material_lines,'pricing'=>$pricing);
}

/** Unknown pending custody denies; exact overlapping line selections serialize across supplier overrides. */
function requisition_purchase_order_has_pending($id, $line_ids) {
 if (empty($GLOBALS['transaction_level']) || !function_exists('approval_draft_current_json_sql')) throw new RuntimeException('Pending custody is unavailable.');
 $json = approval_draft_current_json_sql();
 // Unknown custody must deny using identities alone, before transferring any opaque source payload.
 $unknown = read_requisition_purchase_rows('SELECT id FROM '.TB_PREF.'approval_drafts WHERE trans_type='.ST_PURCHORDER
  .' AND status=0 AND NOT JSON_VALID('.$json.') ORDER BY id FOR UPDATE');
 if ($unknown) return true;
 $rows = read_requisition_purchase_rows('SELECT id,draft_data FROM '.TB_PREF.'approval_drafts WHERE trans_type='.ST_PURCHORDER
  .' AND status=0 AND CASE WHEN JSON_VALID('.$json.") THEN JSON_UNQUOTE(JSON_EXTRACT(".$json.", '$.requisition_conversion.requisition_id'))=".db_escape((string)$id).' ELSE 0 END ORDER BY id FOR UPDATE');
 $actor = $_SESSION['wa_current_user']; $approval = new ApprovalWorkflow($actor->access, $actor->user);
 foreach ($rows as $row) {
  $data = $approval->decodeStoredDraftData($row['draft_data']);
  if (!is_array($data) || empty($data['requisition_conversion']['line_ids']) || !is_array($data['requisition_conversion']['line_ids'])) return true;
  foreach ($data['requisition_conversion']['line_ids'] as $line) if (!is_int($line) || $line <= 0) return true;
  if (array_intersect($line_ids, $data['requisition_conversion']['line_ids'])) return true;
 }
 return false;
}

/** Collect the native PO intent plus an immutable digest of the complete locked requisition source. */
function collect_requisition_purchase_order_data($snapshot, $line_ids = array()) {
	$data = collect_purchase_order_data($snapshot['cart']);
	$source = json_encode(array(requisition_purchase_header_intent($snapshot['header']), $snapshot['lines'], $snapshot['effects'], $snapshot['location'], $snapshot['supplier'], $snapshot['tax_configuration'], requisition_purchase_material_intent($snapshot['material']), $snapshot['material_lines'], $snapshot['pricing']));
	if ($data === false || !is_string($source) || json_last_error() !== JSON_ERROR_NONE)
		throw new RuntimeException('Requisition purchase intent could not be encoded.');
	$data['requisition_conversion'] = array('version'=>1,
		'requisition_id'=>(int)$snapshot['header']['id'], 'supplier_id'=>(int)$snapshot['cart']->supplier_id,
		'line_ids'=>$line_ids,
		'approval_amount'=>$snapshot['cart']->get_trans_total(),
		'order_date'=>date2sql($snapshot['cart']->orig_order_date),
		'source_sha256'=>hash('sha256', $source));
	return $data;
}

/** Admit the exact current company and source/destination authority before any source identity lookup. */
function requisition_purchase_order_envelope_authorized($data) {
	$cart = new purch_order();
	return is_array($data) && $cart->is_current_company()
		&& isset($_SESSION['wa_current_user'])
		&& $_SESSION['wa_current_user']->can_access('SA_PURCHREQUISITION')
		&& $_SESSION['wa_current_user']->can_access('SA_PURCHASEORDER')
		&& isset($data['company'], $data['company_prefix'], $data['trans_type'], $data['supplier_id'],
			$data['requisition_conversion'])
		&& is_scalar($data['company']) && (string)$data['company'] === (string)$cart->company
		&& $data['company_prefix'] === $cart->company_prefix
		&& is_scalar($data['trans_type']) && (string)$data['trans_type'] === (string)ST_PURCHORDER
		&& is_array($data['requisition_conversion'])
		&& isset($data['requisition_conversion']['line_ids'], $data['requisition_conversion']['version'], $data['requisition_conversion']['requisition_id'],
			$data['requisition_conversion']['supplier_id'], $data['requisition_conversion']['order_date'], $data['requisition_conversion']['source_sha256'])
		&& is_array($data['requisition_conversion']['line_ids'])
		&& $data['requisition_conversion']['version'] === 1
		&& is_int($data['requisition_conversion']['supplier_id']) && $data['requisition_conversion']['supplier_id'] > 0
		&& is_int($data['requisition_conversion']['requisition_id']) && $data['requisition_conversion']['requisition_id'] > 0
		&& is_string($data['requisition_conversion']['order_date'])
		&& preg_match('/^[0-9]{4}-[0-9]{2}-[0-9]{2}$/D', $data['requisition_conversion']['order_date'])
		&& is_string($data['requisition_conversion']['source_sha256'])
		&& preg_match('/^[a-f0-9]{64}$/D', $data['requisition_conversion']['source_sha256']);
}

/** Rebuild only the locked original intent; drift requires cancellation and a fresh submission. */
function restore_requisition_purchase_order_snapshot($data) {
	if (!requisition_purchase_order_envelope_authorized($data))
		throw new RuntimeException('Requisition purchase envelope is unavailable.');
	$source = $data['requisition_conversion'];
	$date = sql2date($source['order_date']);
	if (!is_date($date) || date2sql($date) !== $source['order_date'])
		throw new RuntimeException('Requisition purchase date is invalid.');
	$snapshot = prepare_requisition_purchase_order($source['requisition_id'], $source['supplier_id'], $source['line_ids'], $date);
	$expected = collect_requisition_purchase_order_data($snapshot, $source['line_ids']);
	// Normalize only JSON numeric representation, as the central workflow does.
	if (json_decode(json_encode($expected), true) !== json_decode(json_encode($data), true))
		throw new RuntimeException('Requisition purchase source or approved intent changed.');
	return $snapshot;
}

/** Validate only requisition conversion envelopes; legacy ordinary PO drafts retain their own contract. */
function validate_requisition_purchase_order_action($action, $data, $draft, $actor, $comments = '') {
	global $path_to_root;
	if (!is_array($data) || !array_key_exists('requisition_conversion', $data))
		return true;
	include_once($path_to_root.'/purchasing/includes/po_class.inc');
	if (!function_exists('create_po_from_requisition'))
		include_once($path_to_root.'/purchasing/includes/db/purch_requisition_db.inc');
	if (!requisition_purchase_order_envelope_authorized($data)
		|| (int)$actor['user_id'] !== (int)$_SESSION['wa_current_user']->user
		|| (int)$actor['role_id'] !== (int)$_SESSION['wa_current_user']->access)
		return false;
	// Requisition conversion always requires an independent human checker, even at zero price.
	if ($action === 'auto_approve' || ($draft && in_array($action, array('approve','reject'), true)
		&& (int)$draft['submitted_by'] === (int)$actor['user_id']))
		return false;
	// Terminal decisions release central reservation custody without changing source/history.
	if ($action === 'reject' || $action === 'cancel')
		return true;
	try {
		$snapshot = restore_requisition_purchase_order_snapshot($data);
		return $action !== 'submit' || !requisition_purchase_order_has_pending($snapshot['header']['id'], $snapshot['used']);
	} catch (Exception $exception) {
		return false;
	}
}

/** Atomically submit every supplier group through current native policy, or execute all direct groups. */
function submit_requisition_purchase_orders($id, $supplier_id = 0, $line_ids = array(), $direct_only = false) {
 global $path_to_root;
 include_once($path_to_root.'/purchasing/includes/db/po_db.inc');
 include_once($path_to_root.'/purchasing/includes/po_class.inc');
 include_once($path_to_root.'/purchasing/includes/db/purch_agreement_po_approval_db.inc');
 if (!begin_transaction()) return false;
 try {
  if (!class_exists('ApprovalWorkflow') || !ApprovalWorkflow::isRegistered(ST_PURCHORDER)) throw new RuntimeException('Requisition PO approval authority is unavailable.');
  $groups = requisition_purchase_groups($id, $supplier_id, $line_ids); $results = array(); $carts = array(); $completed = array();
  $actor = $_SESSION['wa_current_user']; $user = $actor->user; $role = $actor->access; $company_cart = new purch_order();
  foreach ($groups as $supplier=>$ids) {
   $snapshot = prepare_requisition_purchase_order($id, $supplier, $ids); $cart = $snapshot['cart'];
   if (requisition_purchase_order_has_pending($id, $ids)) throw new RuntimeException('Requisition purchase approval is pending.');
   $approval = new ApprovalWorkflow($role, $user);
   $required = $approval->isApprovalRequired(ST_PURCHORDER, $cart->get_trans_total(), true);
   if ($required) {
    if ($direct_only || !ApprovalWorkflow::isRegistered(ST_PURCHORDER)) throw new RuntimeException('Requisition purchase approval is required.');
    $data = collect_requisition_purchase_order_data($snapshot, $ids);
    $result = $approval->submit(ST_PURCHORDER, $data, $cart->get_trans_total(), array('currency'=>$cart->curr_code,'person_id'=>$cart->supplier_id,'loc_code'=>$cart->Location,'summary'=>$cart->Comments), true);
    if ($result['status'] !== 'pending') throw new RuntimeException('Requisition purchase submission failed.');
    restore_requisition_purchase_order_snapshot($data);
   } else {
    $number = write_requisition_purchase_order($snapshot, 0, true);
    if (!purchase_order_conversion_direct_authorized($cart)) throw new RuntimeException('Requisition purchase policy changed.');
    $carts[] = $cart; $completed[] = array('snapshot'=>$snapshot,'number'=>$number,'intent'=>json_decode(json_encode($cart),true)); $result = array('status'=>'posted','trans_no'=>$number);
   }
   if (!$company_cart->is_current_company() || !requisition_purchase_authorized() || $_SESSION['wa_current_user']->user !== $user || $_SESSION['wa_current_user']->access !== $role) throw new RuntimeException('Requisition authority changed.');
   $results[] = $result;
  }
  if ($completed) {
   if (!function_exists('update_purch_requisition_order_status')) include_once($path_to_root.'/purchasing/includes/db/purch_requisition_db.inc');
   if (!update_purch_requisition_order_status($id)) throw new RuntimeException('Requisition batch progress failed.');
   $last = $completed[count($completed)-1]['snapshot'];
   $lines = read_requisition_purchase_rows($last['queries']['all_lines']); $full = 0; $ordered = 0;
   foreach ($lines as $line) { if ((float)$line['qty_ordered'] > 0) $ordered++; if ((float)$line['qty_ordered'] >= (float)$line['quantity']) $full++; }
   $header = $last['header']; $header['status'] = $full === count($lines) ? 'ordered' : ($ordered ? 'partially_ordered' : 'approved');
   if (read_requisition_purchase_rows($last['queries']['header']) !== array($header)) throw new RuntimeException('Requisition batch status differs from native progress.');
   $before_hook = array();
   foreach ($completed as $key=>$receipt) foreach ($receipt['snapshot']['queries'] as $name=>$query) $before_hook[$key][$name] = read_requisition_purchase_rows($query);
   $numbers = array(); foreach ($completed as $receipt) $numbers[] = $receipt['number'];
   $payload = array('requisition_id'=>(int)$id,'po_numbers'=>$numbers); hook_invoke_all('purch_requisition_po_created', $payload);
   if (!$company_cart->is_current_company() || !requisition_purchase_authorized() || $_SESSION['wa_current_user']->user !== $user || $_SESSION['wa_current_user']->access !== $role
    || ($last['material'] && !$_SESSION['wa_current_user']->can_access('SA_MATERIALREQUEST'))) throw new RuntimeException('Requisition batch completion authority changed.');
   foreach ($completed as $key=>$receipt) {
    verify_agreement_purchase_order_effect($receipt['intent'], $receipt['snapshot']['cart'], $receipt['number']);
    foreach ($receipt['snapshot']['queries'] as $name=>$query) if ($before_hook[$key][$name] !== read_requisition_purchase_rows($query)) throw new RuntimeException('Requisition batch completion changed retained source.');
    foreach ($receipt['snapshot']['pricing'] as $stock=>$inputs) if ($inputs !== read_requisition_purchase_pricing($receipt['snapshot']['cart']->supplier_id,$stock,true)) throw new RuntimeException('Requisition batch pricing changed.');
    if (!$receipt['snapshot']['cart']->tax_included && $receipt['snapshot']['tax_configuration'] !== read_agreement_purchase_tax_configuration(true)) throw new RuntimeException('Requisition batch tax configuration changed.');
   }
  }
  foreach ($carts as $cart) if (!purchase_order_conversion_direct_authorized($cart)) throw new RuntimeException('Requisition batch policy changed.');
  if (!commit_transaction()) {
   if (!empty($GLOBALS['transaction_level'])) throw new RuntimeException('Requisition purchase commit failed.');
   display_error(_('Purchase orders were committed, but resource cleanup needs recovery. Reload the source before continuing.'));
  }
  return $results;
 } catch (Exception $exception) { cancel_transaction(); return false;
 } catch (Throwable $exception) { cancel_transaction(); throw $exception; }
}

/** Execute only the actual locked, independently approved central draft, preserving its stored bytes. */
function execute_requisition_purchase_order($data, $reserved_trans_no, $draft_id) {
	if (empty($GLOBALS['transaction_level']) || !requisition_purchase_order_envelope_authorized($data))
		return false;
	$draft = get_approval_draft_for_update($draft_id);
	$actor = $_SESSION['wa_current_user'];
	$approval = new ApprovalWorkflow($actor->access, $actor->user);
	$decoded = $draft ? $approval->decodeStoredDraftData($draft['draft_data']) : null;
	if (!$draft || (int)$draft['trans_type'] !== ST_PURCHORDER
		|| (int)$draft['status'] !== APPROVAL_STATUS_APPROVED || !empty($draft['approved_trans_no'])
		|| (int)$draft['reserved_trans_no'] !== (int)$reserved_trans_no
		|| (int)$draft['submitted_by'] === (int)$_SESSION['wa_current_user']->user
		|| $decoded !== $data)
		return false;
	try {
		$snapshot = restore_requisition_purchase_order_snapshot($data);
		$actor_id = $_SESSION['wa_current_user']->user;
		$role_id = $_SESSION['wa_current_user']->access;
		if (!has_user_approved_draft($draft_id, $actor_id, (int)$draft['current_level']))
			return false;
		$number = write_requisition_purchase_order($snapshot, (int)$draft_id);
		if ($actor_id !== $_SESSION['wa_current_user']->user || $role_id !== $_SESSION['wa_current_user']->access)
			return false;
		return $number;
	} catch (Exception $exception) {
		return false;
	}
}

/** Reconcile exact native effects and authorized source progress in the owning approval transaction. */
function write_requisition_purchase_order($snapshot, $approval_draft_id = 0, $defer_completion = false) {
 global $Refs, $path_to_root;
 if (!requisition_purchase_authorized()) throw new RuntimeException('Requisition write requires authority and an owning transaction.');
 $cart = $snapshot['cart']; $actor = $_SESSION['wa_current_user']; $user = $actor->user; $role = $actor->access;
 if (!$approval_draft_id && !purchase_order_conversion_direct_authorized($cart)) throw new RuntimeException('Requisition PO approval is required.');
 $intent = json_decode(json_encode($cart), true);
 $cart->reference = $Refs->get_next(ST_PURCHORDER, null, array('supplier_id'=>$cart->supplier_id,'date'=>$cart->orig_order_date,'location'=>$cart->Location));
 $number = add_po($cart, $approval_draft_id);
 if (!$number) throw new RuntimeException('Requisition native purchase order failed.');
 verify_agreement_purchase_order_effect($intent, $cart, $number);
 if ($snapshot['material'] && !$_SESSION['wa_current_user']->can_access('SA_MATERIALREQUEST')) throw new RuntimeException('Linked Warehouse authority changed.');
 // Hooks must not change source rows before the legitimate progress update.
 foreach (array('header','lines','all_lines','location','supplier','material','material_lines') as $key) {
  if (!isset($snapshot['queries'][$key])) continue;
  $rows = read_requisition_purchase_rows($snapshot['queries'][$key]);
  if ($key === 'header') $rows = $rows[0];
  if ($rows !== $snapshot[$key]) throw new RuntimeException('Requisition retained source changed.');
 }
 foreach ($snapshot['lines'] as $line) {
  if (!db_query('UPDATE '.TB_PREF.'purch_requisition_lines SET qty_ordered=qty_ordered+'.db_escape($cart->line_items[array_search((int)$line['id'],$snapshot['used'],true)]->quantity).', po_number='.(int)$number.", status='ordered' WHERE id=".(int)$line['id'], 'could not link requisition order')) throw new RuntimeException('Requisition source linkage failed.');
 }
 if (!function_exists('update_purch_requisition_order_status')) include_once($path_to_root.'/purchasing/includes/db/purch_requisition_db.inc');
 if (!$defer_completion && !update_purch_requisition_order_status($snapshot['header']['id'])) throw new RuntimeException('Requisition progress failed.');
 $current_lines = read_requisition_purchase_rows($snapshot['queries']['all_lines']);
 $full = 0; $ordered = 0;
 foreach ($current_lines as $key=>$line) {
  $original = $snapshot['all_lines'][$key];
  if (in_array((int)$line['id'], $snapshot['used'], true)) {
   $index = array_search((int)$line['id'], $snapshot['used'], true);
   if ((float)$line['qty_ordered'] != (float)$original['qty_ordered'] + (float)$cart->line_items[$index]->quantity
    || (int)$line['po_number'] !== (int)$number || $line['status'] !== 'ordered') throw new RuntimeException('Requisition progress differs from native intent.');
   foreach (array('qty_ordered','po_number','status') as $field) $original[$field] = $line[$field];
  }
  if ($line !== $original) throw new RuntimeException('Unselected or retained requisition line changed.');
  if ((float)$line['qty_ordered'] > 0) $ordered++;
  if ((float)$line['qty_ordered'] >= (float)$line['quantity']) $full++;
 }
 $expected_header = $snapshot['header'];
 if (!$defer_completion) $expected_header['status'] = $full === count($current_lines) ? 'ordered' : ($ordered ? 'partially_ordered' : 'approved');
 $current_header = read_requisition_purchase_rows($snapshot['queries']['header']);
 if ($current_header !== array($expected_header)) throw new RuntimeException('Requisition derived status requires a fresh snapshot.');
 if ($snapshot['material'] && $snapshot['material'][0]['status'] === 'approved') {
  if (!function_exists('mark_material_request_ordered')) include_once($path_to_root.'/inventory/warehouse/includes/db/warehouse_material_requests_db.inc');
  if (!mark_material_request_ordered((int)$snapshot['header']['material_request_id'], ST_PURCHORDER, $number, $snapshot)) throw new RuntimeException('Linked Warehouse order failed.');
 }
 if ($snapshot['material']) {
  $current = read_requisition_purchase_rows($snapshot['queries']['material']);
  $want = $snapshot['material'];
  if ($want[0]['status'] === 'approved') {
   if ((int)$current[0]['linked_doc_type'] !== ST_PURCHORDER || (int)$current[0]['linked_doc_no'] !== (int)$number) throw new RuntimeException('Linked Warehouse order identity differs.');
   $want[0]['status'] = 'ordered'; $want[0]['linked_doc_type'] = $current[0]['linked_doc_type']; $want[0]['linked_doc_no'] = $current[0]['linked_doc_no'];
   $want[0]['updated_at'] = $current[0]['updated_at'];
  }
  if ($current !== $want) throw new RuntimeException('Linked Warehouse source differs from native intent.');
 }
 $expected = array(); foreach (array('header','lines','all_lines','material') as $key) if (isset($snapshot['queries'][$key])) $expected[$key] = read_requisition_purchase_rows($snapshot['queries'][$key]);
 $payload = array('requisition_id'=>(int)$snapshot['header']['id'],'po_numbers'=>array((int)$number));
 if (!$defer_completion) hook_invoke_all('purch_requisition_po_created', $payload);
 if (!$cart->is_current_company() || !requisition_purchase_authorized() || $_SESSION['wa_current_user']->user !== $user || $_SESSION['wa_current_user']->access !== $role
  || ($snapshot['material'] && !$_SESSION['wa_current_user']->can_access('SA_MATERIALREQUEST'))) throw new RuntimeException('Requisition completion authority changed.');
 verify_agreement_purchase_order_effect($intent, $cart, $number);
 foreach (array('header','lines','all_lines','location','supplier','material','material_lines','effects','all_effects') as $key) {
  if (!isset($snapshot['queries'][$key])) continue;
  $rows = read_requisition_purchase_rows($snapshot['queries'][$key]);
  if ($key === 'effects' || $key === 'all_effects') { $old = array(); foreach ($rows as $row) if ((int)$row['order_no'] !== (int)$number) $old[]=$row; $rows=$old; }
  $want = isset($expected[$key]) ? $expected[$key] : $snapshot[$key];
  if ($rows !== $want) throw new RuntimeException('Requisition completion source or prior effects changed.');
 }
 if (!$cart->tax_included && $snapshot['tax_configuration'] !== read_agreement_purchase_tax_configuration(true)) throw new RuntimeException('Requisition tax configuration changed.');
 foreach ($snapshot['pricing'] as $stock=>$inputs) if ($inputs !== read_requisition_purchase_pricing($cart->supplier_id, $stock, true)) throw new RuntimeException('Requisition pricing configuration changed.');
 if (!$approval_draft_id && !purchase_order_conversion_direct_authorized($cart)) throw new RuntimeException('Requisition PO completion policy changed.');
 return (int)$number;
}
