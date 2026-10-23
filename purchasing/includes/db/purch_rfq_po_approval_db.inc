<?php
/** Immutable RFQ source conversion through the existing purchase-order workflow. */

/** Read current locked RFQ rows in deterministic source order. */
function read_rfq_purchase_rows($query) {
	$result = db_query($query, 'could not verify RFQ purchase source');
	if (!$result) throw new RuntimeException('RFQ purchase source is unavailable.');
	$rows = array();
	while ($row = db_fetch_assoc($result)) $rows[] = $row;
	return $rows;
}

/** Build only current authorized awarded lines, preserving retained source and native pricing. */
function prepare_rfq_purchase_order($rfq_id, $vendor_id, $line_ids = array(), $order_date = null) {
	if (empty($GLOBALS['transaction_level']) || !isset($_SESSION['wa_current_user'])
		|| !$_SESSION['wa_current_user']->can_access('SA_PURCHRFQ')
		|| !$_SESSION['wa_current_user']->can_access('SA_PURCHASEORDER')
		|| !is_scalar($rfq_id) || (string)(int)$rfq_id !== (string)$rfq_id || (int)$rfq_id <= 0
		|| !is_scalar($vendor_id) || (string)(int)$vendor_id !== (string)$vendor_id || (int)$vendor_id <= 0
		|| !is_array($line_ids)) throw new RuntimeException('RFQ purchase authority is unavailable.');
	$cart = new purch_order();
	if (!$cart->is_current_company()) throw new RuntimeException('RFQ company is unavailable.');
	$order_date = $order_date === null ? Today() : $order_date;
	if (!is_string($order_date) || !is_date($order_date)) throw new RuntimeException('RFQ purchase date is invalid.');
	$queries = array('header'=>'SELECT * FROM '.TB_PREF.'purch_rfq WHERE id='.(int)$rfq_id.' FOR UPDATE',
		'vendor'=>'SELECT * FROM '.TB_PREF.'purch_rfq_vendors WHERE rfq_id='.(int)$rfq_id.' ORDER BY id FOR UPDATE');
	$headers = read_rfq_purchase_rows($queries['header']);
	if (count($headers) !== 1 || $headers[0]['status'] !== 'awarded') throw new RuntimeException('RFQ is unavailable.');
	$header = $headers[0]; $vendors = read_rfq_purchase_rows($queries['vendor']); $vendor = null;
	// Preserve native vendor-row identity first, legacy supplier identity second.
	foreach ($vendors as $row) if ((int)$row['id'] === (int)$vendor_id) $vendor = $row;
	if (!$vendor) foreach ($vendors as $row) if ((int)$row['supplier_id'] === (int)$vendor_id) $vendor = $row;
	if (!$vendor || (int)$vendor['is_winner'] !== 1) throw new RuntimeException('Awarded RFQ vendor is unavailable.');
	$queries['vendor'] = 'SELECT * FROM '.TB_PREF.'purch_rfq_vendors WHERE id='.(int)$vendor['id'].' FOR UPDATE';
	$queries['quotes'] = 'SELECT * FROM '.TB_PREF.'purch_rfq_vendor_lines WHERE rfq_vendor_id='.(int)$vendor['id'].' ORDER BY rfq_item_id,id FOR UPDATE';
	$quotes = read_rfq_purchase_rows($queries['quotes']); $quote_map = array();
	foreach ($quotes as $row) $quote_map[(int)$row['rfq_item_id']] = $row;
	$queries['lines'] = 'SELECT line.*, item.description AS stock_description, item.units AS stock_units, item.editable AS stock_editable, item.tax_type_id AS stock_tax, tax.name AS stock_tax_name, purchase.supplier_description FROM '.TB_PREF.'purch_rfq_items line LEFT JOIN '.TB_PREF.'stock_master item ON item.stock_id=line.stock_id LEFT JOIN '.TB_PREF.'item_tax_types tax ON tax.id=item.tax_type_id LEFT JOIN '.TB_PREF.'purch_data purchase ON purchase.stock_id=line.stock_id AND purchase.supplier_id='.(int)$vendor['supplier_id'].' WHERE line.rfq_id='.(int)$rfq_id.' ORDER BY line.sort_order,line.id FOR UPDATE';
	$lines = read_rfq_purchase_rows($queries['lines']);
	$queries['effects'] = 'SELECT po.*, detail.po_detail_item, detail.item_code, detail.description AS line_description, detail.quantity_ordered, detail.quantity_received, detail.qty_invoiced, detail.unit_price, detail.delivery_date FROM '.TB_PREF.'purch_orders po LEFT JOIN '.TB_PREF.'purch_order_details detail ON detail.order_no=po.order_no WHERE po.rfq_id='.(int)$rfq_id.' ORDER BY po.order_no,detail.po_detail_item FOR UPDATE';
	$effects = read_rfq_purchase_rows($queries['effects']);
	$custom = decode_purch_rfq_custom_data($vendor['custom_data']); $consumed = array();
	foreach ($effects as $row) {
		if ((int)$row['supplier_id'] !== (int)$vendor['supplier_id']) continue;
		// Historical PO numbers alone cannot establish which RFQ line remains unconsumed.
		if (!isset($custom['po_line_ids'][$row['order_no']]) || !is_array($custom['po_line_ids'][$row['order_no']]))
			throw new RuntimeException('Historical RFQ line consumption requires reconciliation.');
		foreach ($custom['po_line_ids'][$row['order_no']] as $id) $consumed[(int)$id] = true;
	}
	$selected = array();
	foreach ($line_ids as $id) {
		if (!is_scalar($id) || (string)(int)$id !== (string)$id || (int)$id <= 0) throw new RuntimeException('Invalid RFQ line selection.');
		$selected[(int)$id] = true;
	}
	$queries['location'] = 'SELECT * FROM '.TB_PREF.'locations WHERE loc_code='.db_escape($header['delivery_location']).' AND inactive=0 FOR UPDATE';
	$locations = read_rfq_purchase_rows($queries['location']); $location = $locations ? $locations[0] : null;
	$queries['supplier'] = 'SELECT * FROM '.TB_PREF.'suppliers WHERE supplier_id='.(int)$vendor['supplier_id'].' AND inactive=0 FOR UPDATE';
	$suppliers = read_rfq_purchase_rows($queries['supplier']); $supplier = $suppliers ? $suppliers[0] : null;
	if (!$location || !$supplier) throw new RuntimeException('RFQ supplier or location is unavailable.');
	$cart->trans_type = ST_PURCHORDER; $cart->orig_order_date = $order_date;
	$cart->due_date = $header['required_delivery_date'] ? sql2date($header['required_delivery_date']) : $order_date;
	if (!is_date($cart->due_date)) throw new RuntimeException('RFQ delivery date is invalid.');
	$cart->Location = $location['loc_code'];
	$cart->delivery_address = $location['location_name'] !== '' ? $location['location_name'] : $location['loc_code'];
	$cart->Comments = sprintf(_('Created from RFQ %s'), $header['reference']);
	get_supplier_details_to_order($cart, (int)$vendor['supplier_id'], true);
	$cart->reference = 'auto'; $cart->rfq_id = (int)$rfq_id;
	$cart->dimension = (int)$header['dimension_id']; $cart->dimension2 = (int)$header['dimension2_id'];
	$awards = get_purch_rfq_award_map($header['custom_data']); $used = array();
	foreach ($lines as $line) {
		$id = (int)$line['id'];
		if ($selected && !isset($selected[$id])) continue;
		if (isset($consumed[$id])) { if ($selected) throw new RuntimeException('RFQ line already consumed.'); continue; }
		if (($awards && (!isset($awards[$id]) || (int)$awards[$id] !== (int)$vendor['id'])) || !isset($quote_map[$id])) continue;
		$quote = $quote_map[$id]; $qty = (float)$quote['quoted_quantity'] > 0 ? (float)$quote['quoted_quantity'] : (float)$line['quantity'];
		$price = (float)$quote['quoted_price'];
		if ($line['stock_description'] === null || !is_finite($qty) || !is_finite($price) || $qty <= 0 || $price <= 0) throw new RuntimeException('RFQ line is unavailable.');
		$description = $line['description'] !== null && $line['description'] !== '' ? $line['description'] : $line['stock_description'];
		$units = $line['unit_of_measure'] !== '' ? $line['unit_of_measure'] : $line['stock_units'];
		$cart->add_to_order(count($cart->line_items), $line['stock_id'], $qty, $description, $price, $units, $cart->due_date, 0, 0);
		$item = $cart->line_items[count($cart->line_items)-1];
		$item->item_description = $description; $item->descr_editable = $line['stock_editable']; $item->units = $units;
		$item->tax_type = $line['stock_tax']; $item->tax_type_name = $line['stock_tax_name'];
		if (!$item->descr_editable && $line['supplier_description'] !== null && $line['supplier_description'] !== '') $item->item_description = $line['supplier_description'];
		$used[] = $id;
	}
	if (!$cart->order_has_items() || ($selected && count($selected) !== count($used))) throw new RuntimeException('RFQ selected lines are unavailable.');
	$tax = array();
	if (!$cart->tax_included) {
		$tax = read_agreement_purchase_tax_configuration(false);
		if ($lines !== read_rfq_purchase_rows(substr($queries['lines'],0,-11))
			|| (string)get_company_pref('tax_algorithm') !== (string)(empty($tax['algorithm']) ? null : $tax['algorithm'][0]['value'])
			|| $tax !== read_agreement_purchase_tax_configuration(true)) throw new RuntimeException('RFQ tax configuration changed; retry.');
	}
	return array('cart'=>$cart,'header'=>$header,'vendor'=>$vendor,'quotes'=>$quotes,'lines'=>$lines,'effects'=>$effects,'queries'=>$queries,'used'=>$used,'tax_configuration'=>$tax,'location'=>$locations,'supplier'=>$suppliers);
}
/** Read only matching PO draft identities, using a current locking read under the source lock. */
function rfq_purchase_order_has_pending($rfq_id) {
	if (empty($GLOBALS['transaction_level']) || !function_exists('approval_draft_current_json_sql'))
		throw new RuntimeException('Pending conversion custody requires an owning transaction.');
	$json = approval_draft_current_json_sql();
	$sql = 'SELECT id FROM '.TB_PREF.'approval_drafts WHERE trans_type='.ST_PURCHORDER
		.' AND status=0 AND CASE WHEN JSON_VALID('.$json.') THEN '
		."JSON_UNQUOTE(JSON_EXTRACT(".$json.", '$.rfq_conversion.rfq_id'))="
		.db_escape((string)$rfq_id).' ELSE 1 END ORDER BY id FOR UPDATE';
	$result = db_query($sql, 'could not verify pending rfq purchase conversion');
	if (!$result)
		throw new RuntimeException('Pending conversion custody is unavailable.');
	return db_num_rows($result) > 0;
}

/** Collect the native PO intent plus an immutable digest of the complete locked rfq source. */
function collect_rfq_purchase_order_data($snapshot, $line_ids = array()) {
	$data = collect_purchase_order_data($snapshot['cart']);
	$source = json_encode(array($snapshot['header'], $snapshot['lines'], $snapshot['effects'], $snapshot['vendor'], $snapshot['quotes'], $snapshot['location'], $snapshot['supplier'], $snapshot['tax_configuration']));
	if ($data === false || !is_string($source) || json_last_error() !== JSON_ERROR_NONE)
		throw new RuntimeException('RFQ purchase intent could not be encoded.');
	$data['rfq_conversion'] = array('version'=>1,
		'rfq_id'=>(int)$snapshot['header']['id'], 'vendor_id'=>(int)$snapshot['vendor']['id'],
		'line_ids'=>$line_ids,
		'approval_amount'=>$snapshot['cart']->get_trans_total(),
		'order_date'=>date2sql($snapshot['cart']->orig_order_date),
		'source_sha256'=>hash('sha256', $source));
	return $data;
}

/** Admit the exact current company and source/destination authority before any source identity lookup. */
function rfq_purchase_order_envelope_authorized($data) {
	$cart = new purch_order();
	return is_array($data) && $cart->is_current_company()
		&& isset($_SESSION['wa_current_user'])
		&& $_SESSION['wa_current_user']->can_access('SA_PURCHRFQ')
		&& $_SESSION['wa_current_user']->can_access('SA_PURCHASEORDER')
		&& isset($data['company'], $data['company_prefix'], $data['trans_type'], $data['supplier_id'],
			$data['rfq_conversion'])
		&& is_scalar($data['company']) && (string)$data['company'] === (string)$cart->company
		&& $data['company_prefix'] === $cart->company_prefix
		&& is_scalar($data['trans_type']) && (string)$data['trans_type'] === (string)ST_PURCHORDER
		&& is_array($data['rfq_conversion'])
		&& isset($data['rfq_conversion']['line_ids'], $data['rfq_conversion']['version'], $data['rfq_conversion']['rfq_id'],
			$data['rfq_conversion']['vendor_id'], $data['rfq_conversion']['order_date'], $data['rfq_conversion']['source_sha256'])
		&& is_array($data['rfq_conversion']['line_ids'])
		&& $data['rfq_conversion']['version'] === 1
		&& is_int($data['rfq_conversion']['vendor_id']) && $data['rfq_conversion']['vendor_id'] > 0
		&& is_int($data['rfq_conversion']['rfq_id']) && $data['rfq_conversion']['rfq_id'] > 0
		&& is_string($data['rfq_conversion']['order_date'])
		&& preg_match('/^[0-9]{4}-[0-9]{2}-[0-9]{2}$/D', $data['rfq_conversion']['order_date'])
		&& is_string($data['rfq_conversion']['source_sha256'])
		&& preg_match('/^[a-f0-9]{64}$/D', $data['rfq_conversion']['source_sha256']);
}

/** Rebuild only the locked original intent; drift requires cancellation and a fresh submission. */
function restore_rfq_purchase_order_snapshot($data) {
	if (!rfq_purchase_order_envelope_authorized($data))
		throw new RuntimeException('RFQ purchase envelope is unavailable.');
	$source = $data['rfq_conversion'];
	$date = sql2date($source['order_date']);
	if (!is_date($date) || date2sql($date) !== $source['order_date'])
		throw new RuntimeException('RFQ purchase date is invalid.');
	$snapshot = prepare_rfq_purchase_order($source['rfq_id'], $source['vendor_id'], $source['line_ids'], $date);
	$expected = collect_rfq_purchase_order_data($snapshot, $source['line_ids']);
	// Normalize only JSON numeric representation, as the central workflow does.
	if (json_decode(json_encode($expected), true) !== json_decode(json_encode($data), true))
		throw new RuntimeException('RFQ purchase source or approved intent changed.');
	return $snapshot;
}

/** Validate only rfq conversion envelopes; legacy ordinary PO drafts retain their own contract. */
function validate_rfq_purchase_order_action($action, $data, $draft, $actor, $comments = '') {
	global $path_to_root;
	if (!is_array($data) || !array_key_exists('rfq_conversion', $data))
		return true;
	include_once($path_to_root.'/purchasing/includes/po_class.inc');
	if (!function_exists('create_po_from_rfq'))
		include_once($path_to_root.'/purchasing/includes/db/purch_rfq_db.inc');
	if (!rfq_purchase_order_envelope_authorized($data)
		|| (int)$actor['user_id'] !== (int)$_SESSION['wa_current_user']->user
		|| (int)$actor['role_id'] !== (int)$_SESSION['wa_current_user']->access)
		return false;
	// RFQ conversion always requires an independent human checker, even at zero price.
	if ($action === 'auto_approve' || ($draft && in_array($action, array('approve','reject'), true)
		&& (int)$draft['submitted_by'] === (int)$actor['user_id']))
		return false;
	// Terminal decisions release central reservation custody without changing source/history.
	if ($action === 'reject' || $action === 'cancel')
		return true;
	try {
		$snapshot = restore_rfq_purchase_order_snapshot($data);
		return $action !== 'submit' || !rfq_purchase_order_has_pending($snapshot['header']['id']);
	} catch (Exception $exception) {
		return false;
	}
}

/** Submit immutable source intent or execute the existing direct-policy command, never both. */
function submit_rfq_purchase_order($rfq_id, $vendor_id, $line_ids = array()) {
	global $path_to_root;
	include_once($path_to_root.'/purchasing/includes/db/po_db.inc');
	include_once($path_to_root.'/purchasing/includes/po_class.inc');
	if (!function_exists('create_po_from_rfq'))
		include_once($path_to_root.'/purchasing/includes/db/purch_rfq_db.inc');
	if (!begin_transaction())
		return false;
	try {
		$snapshot = prepare_rfq_purchase_order($rfq_id, $vendor_id, $line_ids);
		if (!class_exists('ApprovalWorkflow') || !ApprovalWorkflow::isRegistered(ST_PURCHORDER)
			|| rfq_purchase_order_has_pending($rfq_id))
			throw new RuntimeException('RFQ purchase approval custody is unavailable or pending.');
		// A new service prevents a prior request-local policy cache from choosing a stale workflow.
		$actor = $_SESSION['wa_current_user'];
		$approval = new ApprovalWorkflow($actor->access, $actor->user);
		if ($approval->isApprovalRequired(ST_PURCHORDER, $snapshot['cart']->get_trans_total(), true)) {
			$data = collect_rfq_purchase_order_data($snapshot, $line_ids);
			$result = $approval->submit(ST_PURCHORDER, $data, $snapshot['cart']->get_trans_total(),
				array('currency'=>$snapshot['cart']->curr_code, 'person_id'=>$snapshot['cart']->supplier_id,
					'loc_code'=>$snapshot['cart']->Location, 'summary'=>$snapshot['cart']->Comments), true);
			if ($result['status'] !== 'pending')
				throw new RuntimeException('RFQ purchase approval submission failed.');
			if (!$snapshot['cart']->is_current_company() || !rfq_purchase_order_envelope_authorized($data))
				throw new RuntimeException('RFQ purchase submission authority changed.');
			restore_rfq_purchase_order_snapshot($data);
		} else {
			$number = create_po_from_rfq($rfq_id, $vendor_id, $line_ids);
			if (!$number)
				throw new RuntimeException('RFQ purchase direct conversion failed.');
			$result = array('status'=>'posted', 'trans_no'=>(int)$number);
		}
		if (!commit_transaction() && !empty($GLOBALS['transaction_level']))
			throw new RuntimeException('RFQ purchase submission commit failed.');
		return $result;
	} catch (Exception $exception) {
		cancel_transaction();
		return false;
	} catch (Throwable $exception) {
		cancel_transaction();
		throw $exception;
	}
}

/** Execute only the actual locked, independently approved central draft, preserving its stored bytes. */
function execute_rfq_purchase_order($data, $reserved_trans_no, $draft_id) {
	if (empty($GLOBALS['transaction_level']) || !rfq_purchase_order_envelope_authorized($data))
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
		$snapshot = restore_rfq_purchase_order_snapshot($data);
		$actor_id = $_SESSION['wa_current_user']->user;
		$role_id = $_SESSION['wa_current_user']->access;
		if (!has_user_approved_draft($draft_id, $actor_id, (int)$draft['current_level']))
			return false;
		$number = write_rfq_purchase_order($snapshot, (int)$draft_id);
		if ($actor_id !== $_SESSION['wa_current_user']->user || $role_id !== $_SESSION['wa_current_user']->access)
			return false;
		return $number;
	} catch (Exception $exception) {
		return false;
	}
}

/** Write and reconcile the native order and the exact source linkage within its owning transaction. */
function write_rfq_purchase_order($snapshot, $approval_draft_id = 0) {
	global $Refs;
	if (empty($GLOBALS['transaction_level'])) throw new RuntimeException('RFQ write requires owning transaction.');
	$cart = $snapshot['cart']; $actor = $_SESSION['wa_current_user']; $user = $actor->user; $role = $actor->access;
	$intent = json_decode(json_encode($cart), true);
	$cart->reference = $Refs->get_next(ST_PURCHORDER, null, array('supplier_id'=>$cart->supplier_id,'date'=>$cart->orig_order_date,'location'=>$cart->Location));
	$number = add_po($cart, $approval_draft_id);
	if (!$number) throw new RuntimeException('RFQ purchase order was not completed.');
	verify_agreement_purchase_order_effect($intent, $cart, $number);
	$expected = $snapshot;
	foreach (array('header'=>'purch_rfq','vendor'=>'purch_rfq_vendors') as $key=>$table) {
		$custom = decode_purch_rfq_custom_data($snapshot[$key]['custom_data']);
		if (!isset($custom['created_po_numbers'])) $custom['created_po_numbers'] = array();
		if (!is_array($custom['created_po_numbers'])) throw new RuntimeException('RFQ prior linkage is unavailable.');
		$custom['created_po_numbers'][] = (int)$number;
		if ($key === 'vendor') {
			if (isset($custom['po_line_ids']) && !is_array($custom['po_line_ids']))
				throw new RuntimeException('RFQ line linkage is unavailable.');
			$custom['po_line_ids'][$number] = $snapshot['used'];
		}
		$sql = sql_purch_rfq_custom_data($custom);
		if (!db_query('UPDATE '.TB_PREF.$table.' SET custom_data='.$sql.' WHERE id='.(int)$snapshot[$key]['id'], 'could not link RFQ purchase order')) throw new RuntimeException('RFQ source linkage failed.');
		$expected[$key]['custom_data'] = encode_purch_rfq_custom_data($custom);
	}
	$payload = array('rfq_id'=>(int)$snapshot['header']['id'],'rfq_vendor_id'=>(int)$snapshot['vendor']['id'],'po_no'=>(int)$number);
	hook_invoke_all('purch_rfq_po_created', $payload);
	if (!$cart->is_current_company() || !$_SESSION['wa_current_user']->can_access('SA_PURCHASEORDER')
		|| !$_SESSION['wa_current_user']->can_access('SA_PURCHRFQ') || $user !== $_SESSION['wa_current_user']->user || $role !== $_SESSION['wa_current_user']->access) throw new RuntimeException('RFQ command authority changed.');
	verify_agreement_purchase_order_effect($intent, $cart, $number);
	foreach (array('header','vendor','quotes','lines','effects','location','supplier') as $key) {
		$rows = read_rfq_purchase_rows($snapshot['queries'][$key]);
		if ($key === 'header' || $key === 'vendor') $rows = $rows[0];
		if ($key === 'effects') { $old = array(); foreach ($rows as $row) if ((int)$row['order_no'] !== (int)$number) $old[] = $row; $rows = $old; }
		if ($rows !== $expected[$key]) throw new RuntimeException('RFQ retained source or prior effects changed.');
	}
	if (!$cart->tax_included && $snapshot['tax_configuration'] !== read_agreement_purchase_tax_configuration(true)) throw new RuntimeException('RFQ tax configuration changed.');
	return (int)$number;
}
