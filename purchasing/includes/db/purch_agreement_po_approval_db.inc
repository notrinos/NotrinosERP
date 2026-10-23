<?php
/** Immutable agreement conversions through the existing purchasing approval registration. */

/** Build a current locked agreement and native cart without consuming source or reservations. */
function prepare_agreement_purchase_order($agreement_id, $line_selections = array(), $order_date = null) {
	if (empty($GLOBALS['transaction_level']) || !isset($_SESSION['wa_current_user'])
		|| !$_SESSION['wa_current_user']->can_access('SA_PURCHAGREEMENT')
		|| !$_SESSION['wa_current_user']->can_access('SA_PURCHASEORDER')
		|| !is_scalar($agreement_id) || (string)(int)$agreement_id !== (string)$agreement_id || (int)$agreement_id <= 0
		|| !is_array($line_selections))
		throw new RuntimeException('Agreement purchase authority is unavailable.');
	$company_order = new purch_order();
	if (!$company_order->is_current_company()) throw new RuntimeException('Agreement company is unavailable.');
	$order_date = $order_date === null ? Today() : $order_date;
	if (!is_date($order_date)) throw new RuntimeException('Agreement date is invalid.');
	$header_query = 'SELECT * FROM '.TB_PREF.'purch_agreements WHERE id='.(int)$agreement_id.' FOR UPDATE';
	$agreement = db_fetch_assoc(db_query($header_query, 'could not lock agreement conversion'));
	if (!$agreement || $agreement['status'] !== 'active' || !empty($agreement['inactive']))
		throw new RuntimeException('Purchase order conversion was not completed.');

	$location = db_fetch_assoc(db_query('SELECT * FROM '.TB_PREF.'locations WHERE loc_code='.db_escape($agreement['delivery_location']).' AND inactive=0 FOR UPDATE', 'could not lock agreement location'));
	if (!$location && trim($agreement['delivery_location']) === '')
		$location = db_fetch_assoc(db_query('SELECT * FROM '.TB_PREF.'locations WHERE inactive=0 AND fixed_asset=0 ORDER BY loc_code LIMIT 1 FOR UPDATE', 'could not lock default agreement location'));
	if (!$location)
		throw new RuntimeException('Purchase order conversion was not completed.');

	// Lock actual linked effects before calculating remaining quantity. No submission writes source totals.
	$effect_query = 'SELECT po.order_no, detail.* FROM '.TB_PREF.'purch_orders po LEFT JOIN '.TB_PREF.'purch_order_details detail ON detail.order_no=po.order_no WHERE po.agreement_id='.(int)$agreement_id.' ORDER BY po.order_no, detail.po_detail_item FOR UPDATE';
	$effects = array(); $ordered = array();
	$result = db_query($effect_query, 'could not lock agreement fulfillment');
	while ($row = db_fetch_assoc($result)) {
		$effects[] = $row;
		if ($row['item_code'] !== null) $ordered[$row['item_code']] = (isset($ordered[$row['item_code']]) ? $ordered[$row['item_code']] : 0) + (float)$row['quantity_ordered'];
	}

	$purchase_order = new purch_order();
	$purchase_order->trans_type = ST_PURCHORDER;
	$purchase_order->orig_order_date = $order_date;
	$purchase_order->due_date = $order_date;
	$purchase_order->Location = $location['loc_code'];
	$purchase_order->Comments = sprintf(_('Created from Purchase Agreement %s'), $agreement['reference']);
	$purchase_order->delivery_address = $location['delivery_address'] !== ''
		? $location['delivery_address']
		: ($location['location_name'] !== '' ? $location['location_name'] : $location['loc_code']);

	$supplier = db_fetch_assoc(db_query('SELECT supplier_id FROM '.TB_PREF.'suppliers WHERE supplier_id='.(int)$agreement['supplier_id'].' AND inactive=0 FOR UPDATE', 'could not lock agreement supplier'));
	if (!$supplier) throw new RuntimeException('Agreement supplier is unavailable.');
	get_supplier_details_to_order($purchase_order, (int)$agreement['supplier_id'], true);
	$purchase_order->reference = 'auto';
	$purchase_order->agreement_id = (int)$agreement_id;
	$purchase_order->rfq_id = (int)$agreement['rfq_id'];
	$purchase_order->dimension = (int)$agreement['dimension_id'];
	$purchase_order->dimension2 = (int)$agreement['dimension2_id'];

	$line_query = 'SELECT line.*, COALESCE(line.description, item.description) AS line_description, item.units AS stock_units, item.description AS stock_description, item.editable AS stock_editable, item.tax_type_id AS stock_tax, tax.name AS stock_tax_name, purchase.supplier_description FROM '.TB_PREF.'purch_agreement_lines line LEFT JOIN '.TB_PREF.'stock_master item ON item.stock_id=line.stock_id LEFT JOIN '.TB_PREF.'item_tax_types tax ON tax.id=item.tax_type_id LEFT JOIN '.TB_PREF.'purch_data purchase ON purchase.stock_id=line.stock_id AND purchase.supplier_id='.(int)$agreement['supplier_id'].' WHERE line.agreement_id='.(int)$agreement_id.' ORDER BY line.id FOR UPDATE';
	$agreement_lines = db_query($line_query, 'could not lock agreement lines');
	$source_lines = array();
	while ($agreement_line = db_fetch_assoc($agreement_lines)) {
		$source_lines[] = $agreement_line;
		if ($agreement_line['stock_description'] === null) throw new RuntimeException('Agreement item is unavailable.');
		$agreement_line['ordered_qty'] = isset($ordered[$agreement_line['stock_id']]) ? $ordered[$agreement_line['stock_id']] : 0;
		if (!is_purch_agreement_line_data_valid(
			$agreement_line['committed_qty'],
			$agreement_line['unit_price'],
			$agreement_line['discount_percent'],
			$agreement_line['min_qty_per_order'],
			$agreement_line['price_valid_until']
		))
			continue;

		if ($agreement_line['price_valid_until']
			&& $agreement_line['price_valid_until'] !== '0000-00-00'
			&& $agreement_line['price_valid_until'] < date2sql($order_date))
			continue;

		$line_id = (int)$agreement_line['id'];
		$selection = isset($line_selections[$line_id]) ? $line_selections[$line_id] : array();
		$selected_quantity = 0;
		$selected_delivery_date = '';

		if (is_array($selection)) {
			if (!empty($selection['skip']))
				continue;
			if (isset($selection['quantity']) && !is_numeric($selection['quantity']))
				throw new RuntimeException('Invalid agreement quantity.');
			$selected_quantity = isset($selection['quantity']) ? (float)$selection['quantity'] : 0;
			$selected_delivery_date = isset($selection['delivery_date']) ? $selection['delivery_date'] : '';
		} elseif ($selection !== null && $selection !== '') {
			if (!is_numeric($selection)) throw new RuntimeException('Invalid agreement selection.');
			$selected_quantity = (float)$selection;
		}

		$remaining_qty = max(0, (float)$agreement_line['committed_qty'] - (float)$agreement_line['ordered_qty']);
		if (!is_finite($selected_quantity)) throw new RuntimeException('Invalid agreement quantity.');
		if ($remaining_qty <= 0) continue;
		if ($selected_quantity <= 0)
			$selected_quantity = $remaining_qty;
		if ($selected_quantity <= 0)
			continue;
		if ($remaining_qty > 0)
			$selected_quantity = min($selected_quantity, $remaining_qty);
		if ($selected_quantity <= 0)
			continue;

		if ((float)$agreement_line['min_qty_per_order'] > 0 && $selected_quantity < (float)$agreement_line['min_qty_per_order'])
			$selected_quantity = (float)$agreement_line['min_qty_per_order'];
		if ($remaining_qty > 0)
			$selected_quantity = min($selected_quantity, $remaining_qty);
		if ($selected_quantity <= 0)
			continue;

		$effective_price = round((float)$agreement_line['unit_price'] * (1 - ((float)$agreement_line['discount_percent'] / 100)), user_price_dec());
		$line_delivery_date = $selected_delivery_date !== '' ? $selected_delivery_date : $order_date;
		if (!is_string($line_delivery_date) || !is_date($line_delivery_date)) throw new RuntimeException('Invalid agreement delivery date.');

		$purchase_order->add_to_order(
			count($purchase_order->line_items),
			$agreement_line['stock_id'],
			$selected_quantity,
			$agreement_line['line_description'] !== '' ? $agreement_line['line_description'] : null,
			$effective_price,
			$agreement_line['stock_units'],
			$line_delivery_date,
			0,
			0
		);
		$item = $purchase_order->line_items[count($purchase_order->line_items)-1];
		$item->item_description = $agreement_line['line_description'] !== '' ? $agreement_line['line_description'] : $agreement_line['stock_description'];
		$item->descr_editable = $agreement_line['stock_editable'];
		$item->units = $agreement_line['stock_units'];
		$item->tax_type = $agreement_line['stock_tax'];
		$item->tax_type_name = $agreement_line['stock_tax_name'];
		if (!$item->descr_editable && $agreement_line['supplier_description'] !== null && $agreement_line['supplier_description'] !== '')
			$item->item_description = $agreement_line['supplier_description'];
	}

	$tax_configuration = array();
	if (!$purchase_order->tax_included) {
		// Native tax readers use a repeatable-read snapshot. Prove it matches
		// current locked configuration before approving any priced tax-exclusive intent.
		$tax_configuration = read_agreement_purchase_tax_configuration(false);
		$consistent_lines=array();
		$rows=db_query(substr($line_query,0,-11), 'could not verify native item tax snapshot');
		while ($row=db_fetch_assoc($rows)) $consistent_lines[]=$row;
		if ($consistent_lines !== $source_lines)
			throw new RuntimeException('Agreement item tax snapshot changed; retry in a fresh transaction.');
		$algorithm = empty($tax_configuration['algorithm']) ? null : $tax_configuration['algorithm'][0]['value'];
		if ((string)get_company_pref('tax_algorithm') !== (string)$algorithm)
			throw new RuntimeException('Agreement tax policy cache changed; retry in a fresh request.');
		if ($tax_configuration !== read_agreement_purchase_tax_configuration(true))
			throw new RuntimeException('Agreement tax configuration changed; retry in a fresh transaction.');
	}
	if (!$purchase_order->order_has_items())
		throw new RuntimeException('Purchase order conversion was not completed.');

	return array('cart'=>$purchase_order, 'header'=>$agreement, 'lines'=>$source_lines, 'effects'=>$effects,
		'header_query'=>$header_query, 'line_query'=>$line_query, 'effect_query'=>$effect_query,
		'tax_configuration'=>$tax_configuration);
}
/** Read only matching PO draft identities, using a current locking read under the source lock. */
function agreement_purchase_order_has_pending($agreement_id) {
	if (empty($GLOBALS['transaction_level']) || !function_exists('approval_draft_current_json_sql'))
		throw new RuntimeException('Pending conversion custody requires an owning transaction.');
	$json = approval_draft_current_json_sql();
	$sql = 'SELECT id FROM '.TB_PREF.'approval_drafts WHERE trans_type='.ST_PURCHORDER
		.' AND status=0 AND CASE WHEN JSON_VALID('.$json.') THEN '
		."JSON_UNQUOTE(JSON_EXTRACT(".$json.", '$.agreement_conversion.agreement_id'))="
		.db_escape((string)$agreement_id).' ELSE 1 END ORDER BY id FOR UPDATE';
	$result = db_query($sql, 'could not verify pending agreement purchase conversion');
	if (!$result)
		throw new RuntimeException('Pending conversion custody is unavailable.');
	return db_num_rows($result) > 0;
}

/** Collect the native PO intent plus an immutable digest of the complete locked agreement source. */
function collect_agreement_purchase_order_data($snapshot, $line_selections = array()) {
	$data = collect_purchase_order_data($snapshot['cart']);
	$source = json_encode(array($snapshot['header'], $snapshot['lines'], $snapshot['effects'], $snapshot['tax_configuration']));
	if ($data === false || !is_string($source) || json_last_error() !== JSON_ERROR_NONE)
		throw new RuntimeException('Agreement purchase intent could not be encoded.');
	$data['agreement_conversion'] = array('version'=>1,
		'agreement_id'=>(int)$snapshot['header']['id'],
		'line_selections'=>$line_selections,
		'approval_amount'=>$snapshot['cart']->get_trans_total(),
		'order_date'=>date2sql($snapshot['cart']->orig_order_date),
		'source_sha256'=>hash('sha256', $source));
	return $data;
}

/** Admit the exact current company and source/destination authority before any source identity lookup. */
function agreement_purchase_order_envelope_authorized($data) {
	$cart = new purch_order();
	return is_array($data) && $cart->is_current_company()
		&& isset($_SESSION['wa_current_user'])
		&& $_SESSION['wa_current_user']->can_access('SA_PURCHAGREEMENT')
		&& $_SESSION['wa_current_user']->can_access('SA_PURCHASEORDER')
		&& isset($data['company'], $data['company_prefix'], $data['trans_type'], $data['supplier_id'],
			$data['agreement_conversion'])
		&& is_scalar($data['company']) && (string)$data['company'] === (string)$cart->company
		&& $data['company_prefix'] === $cart->company_prefix
		&& is_scalar($data['trans_type']) && (string)$data['trans_type'] === (string)ST_PURCHORDER
		&& is_array($data['agreement_conversion'])
		&& isset($data['agreement_conversion']['line_selections'], $data['agreement_conversion']['version'], $data['agreement_conversion']['agreement_id'],
			$data['agreement_conversion']['order_date'], $data['agreement_conversion']['source_sha256'])
		&& is_array($data['agreement_conversion']['line_selections'])
		&& $data['agreement_conversion']['version'] === 1
		&& is_int($data['agreement_conversion']['agreement_id']) && $data['agreement_conversion']['agreement_id'] > 0
		&& is_string($data['agreement_conversion']['order_date'])
		&& preg_match('/^[0-9]{4}-[0-9]{2}-[0-9]{2}$/D', $data['agreement_conversion']['order_date'])
		&& is_string($data['agreement_conversion']['source_sha256'])
		&& preg_match('/^[a-f0-9]{64}$/D', $data['agreement_conversion']['source_sha256']);
}

/** Rebuild only the locked original intent; drift requires cancellation and a fresh submission. */
function restore_agreement_purchase_order_snapshot($data) {
	if (!agreement_purchase_order_envelope_authorized($data))
		throw new RuntimeException('Agreement purchase envelope is unavailable.');
	$source = $data['agreement_conversion'];
	$date = sql2date($source['order_date']);
	if (!is_date($date) || date2sql($date) !== $source['order_date'])
		throw new RuntimeException('Agreement purchase date is invalid.');
	$snapshot = prepare_agreement_purchase_order($source['agreement_id'], $source['line_selections'], $date);
	$expected = collect_agreement_purchase_order_data($snapshot, $source['line_selections']);
	// Normalize only JSON numeric representation, as the central workflow does.
	if (json_decode(json_encode($expected), true) !== json_decode(json_encode($data), true))
		throw new RuntimeException('Agreement purchase source or approved intent changed.');
	return $snapshot;
}

/** Validate only agreement conversion envelopes; legacy ordinary PO drafts retain their own contract. */
function validate_agreement_purchase_order_action($action, $data, $draft, $actor, $comments = '') {
	global $path_to_root;
	if (!is_array($data) || !array_key_exists('agreement_conversion', $data))
		return true;
	include_once($path_to_root.'/purchasing/includes/po_class.inc');
	if (!function_exists('create_po_from_agreement'))
		include_once($path_to_root.'/purchasing/includes/db/purch_agreement_db.inc');
	if (!agreement_purchase_order_envelope_authorized($data)
		|| (int)$actor['user_id'] !== (int)$_SESSION['wa_current_user']->user
		|| (int)$actor['role_id'] !== (int)$_SESSION['wa_current_user']->access)
		return false;
	// Agreement conversion always requires an independent human checker, even at zero price.
	if ($action === 'auto_approve' || ($draft && in_array($action, array('approve','reject'), true)
		&& (int)$draft['submitted_by'] === (int)$actor['user_id']))
		return false;
	// Terminal decisions release central reservation custody without changing source/history.
	if ($action === 'reject' || $action === 'cancel')
		return true;
	try {
		$snapshot = restore_agreement_purchase_order_snapshot($data);
		return $action !== 'submit' || !agreement_purchase_order_has_pending($snapshot['header']['id']);
	} catch (Exception $exception) {
		return false;
	}
}

/** Submit immutable source intent or execute the existing direct-policy command, never both. */
function submit_agreement_purchase_order($agreement_id, $line_selections = array()) {
	global $path_to_root;
	include_once($path_to_root.'/purchasing/includes/db/po_db.inc');
	include_once($path_to_root.'/purchasing/includes/po_class.inc');
	if (!function_exists('create_po_from_agreement'))
		include_once($path_to_root.'/purchasing/includes/db/purch_agreement_db.inc');
	if (!begin_transaction())
		return false;
	try {
		$snapshot = prepare_agreement_purchase_order($agreement_id, $line_selections);
		if (!class_exists('ApprovalWorkflow') || !ApprovalWorkflow::isRegistered(ST_PURCHORDER)
			|| agreement_purchase_order_has_pending($agreement_id))
			throw new RuntimeException('Agreement purchase approval custody is unavailable or pending.');
		// A new service prevents a prior request-local policy cache from choosing a stale workflow.
		$actor = $_SESSION['wa_current_user'];
		$approval = new ApprovalWorkflow($actor->access, $actor->user);
		if ($approval->isApprovalRequired(ST_PURCHORDER, $snapshot['cart']->get_trans_total(), true)) {
			$data = collect_agreement_purchase_order_data($snapshot, $line_selections);
			$result = $approval->submit(ST_PURCHORDER, $data, $snapshot['cart']->get_trans_total(),
				array('currency'=>$snapshot['cart']->curr_code, 'person_id'=>$snapshot['cart']->supplier_id,
					'loc_code'=>$snapshot['cart']->Location, 'summary'=>$snapshot['cart']->Comments), true);
			if ($result['status'] !== 'pending')
				throw new RuntimeException('Agreement purchase approval submission failed.');
			if (!$snapshot['cart']->is_current_company() || !agreement_purchase_order_envelope_authorized($data))
				throw new RuntimeException('Agreement purchase submission authority changed.');
			restore_agreement_purchase_order_snapshot($data);
		} else {
			$number = create_po_from_agreement($agreement_id, $line_selections);
			if (!$number)
				throw new RuntimeException('Agreement purchase direct conversion failed.');
			$result = array('status'=>'posted', 'trans_no'=>(int)$number);
		}
		if (!commit_transaction() && !empty($GLOBALS['transaction_level']))
			throw new RuntimeException('Agreement purchase submission commit failed.');
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
function execute_agreement_purchase_order($data, $reserved_trans_no, $draft_id) {
	if (empty($GLOBALS['transaction_level']) || !agreement_purchase_order_envelope_authorized($data))
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
		$snapshot = restore_agreement_purchase_order_snapshot($data);
		$actor_id = $_SESSION['wa_current_user']->user;
		$role_id = $_SESSION['wa_current_user']->access;
		if (!has_user_approved_draft($draft_id, $actor_id, (int)$draft['current_level']))
			return false;
		$number = write_agreement_purchase_order($snapshot, (int)$draft_id);
		if ($actor_id !== $_SESSION['wa_current_user']->user || $role_id !== $_SESSION['wa_current_user']->access)
			return false;
		return $number;
	} catch (Exception $exception) {
		return false;
	}
}

/** Write only a locked native intent inside the owning direct/approved transaction. */
function write_agreement_purchase_order($snapshot, $approval_draft_id = 0) {
	global $Refs;
	if (empty($GLOBALS['transaction_level'])) throw new RuntimeException('Agreement write requires owning transaction.');
	$agreement=$snapshot['header']; $agreement_id=(int)$agreement['id']; $purchase_order=$snapshot['cart'];
	$actor_id=$_SESSION['wa_current_user']->user; $role_id=$_SESSION['wa_current_user']->access;
	$intent = json_decode(json_encode($purchase_order), true);
	// Preserve the agreement converter's location-aware native reference context.
	$purchase_order->reference=$Refs->get_next(ST_PURCHORDER, null, array('supplier_id'=>$purchase_order->supplier_id,
		'date'=>$purchase_order->orig_order_date, 'location'=>$purchase_order->Location));
	$po_number = add_po($purchase_order, $approval_draft_id);
	if (!$po_number)
		throw new RuntimeException('Purchase order conversion was not completed.');
	if (!($purchase_order->is_current_company() && $_SESSION['wa_current_user']->can_access('SA_PURCHASEORDER'))
		|| !$_SESSION['wa_current_user']->can_access('SA_PURCHAGREEMENT'))
		throw new RuntimeException('Purchase source authority changed.');

	verify_agreement_purchase_order_effect($intent, $purchase_order, $po_number);

	$sql = "UPDATE " . TB_PREF . "purch_orders SET
		agreement_id = " . (int)$agreement_id . ",
		rfq_id = " . (int)$agreement['rfq_id'] . "
		WHERE order_no = " . (int)$po_number;
	if (!db_query($sql, 'could not link purchase order to agreement'))
		throw new RuntimeException('Purchase order source linkage failed.');

	update_agreement_fulfillment($agreement_id);

	$hook_data = array(
		'agreement_id' => (int)$agreement_id,
		'po_no' => (int)$po_number,
	);
	hook_invoke_all('purch_agreement_po_created', $hook_data);
	// Completion hooks must not commit under a changed company or role.
	if (!($purchase_order->is_current_company() && $_SESSION['wa_current_user']->can_access('SA_PURCHASEORDER'))
		|| !$_SESSION['wa_current_user']->can_access('SA_PURCHASEORDER')
		|| !$_SESSION['wa_current_user']->can_access('SA_PURCHAGREEMENT'))
		throw new RuntimeException('Purchase source completion authority changed.');

	verify_agreement_purchase_order_effect($intent, $purchase_order, $po_number);
	verify_agreement_purchase_source($snapshot, $po_number);
	if ($actor_id !== $_SESSION['wa_current_user']->user || $role_id !== $_SESSION['wa_current_user']->access)
		throw new RuntimeException('Agreement command actor changed during execution.');
	if (!$purchase_order->tax_included && $snapshot['tax_configuration'] !== read_agreement_purchase_tax_configuration(true))
		throw new RuntimeException('Agreement tax configuration changed during execution.');
	return (int)$po_number;
}

/** Verify native hook output and stored header/lines against the locked, immutable agreement intent. */
function verify_agreement_purchase_order_effect($intent, $cart, $number) {
	$actual = json_decode(json_encode($cart), true);
	$actual['order_no'] = $intent['order_no'];
	$actual['reference'] = $intent['reference'];
	foreach ($actual['line_items'] as $key=>$line)
		$actual['line_items'][$key]['po_detail_rec'] = $intent['line_items'][$key]['po_detail_rec'];
	if ($actual !== $intent)
		throw new RuntimeException('Native agreement purchase intent changed.');
	$fields = array('supplier_id'=>$intent['supplier_id'], 'Comments'=>$intent['Comments'],
		'agreement_id'=>$intent['agreement_id'], 'requisition_id'=>$intent['requisition_id'], 'rfq_id'=>$intent['rfq_id'],
		'ord_date'=>date2sql($intent['orig_order_date']), 'supp_reference'=>$intent['supp_ref'],
		'into_stock_location'=>$intent['Location'], 'delivery_address'=>$intent['delivery_address'],
		'total'=>$cart->get_trans_total(), 'tax_included'=>$intent['tax_included'], 'prep_amount'=>$intent['prep_amount']);
	$numeric = array('supplier_id','agreement_id','requisition_id','rfq_id','total','tax_included','prep_amount');
	$where = 'order_no='.(int)$number;
	foreach ($fields as $column=>$value)
		$where .= ' AND '.(in_array($column,$numeric,true)?'':'BINARY ').$column.'='.db_escape($value);
	$row = db_fetch_assoc(db_query('SELECT order_no FROM '.TB_PREF.'purch_orders WHERE '.$where.' FOR UPDATE',
		'could not reconcile agreement purchase header'));
	if (!$row)
		throw new RuntimeException('Native agreement purchase header differs from approved intent.');
	$rows = db_query('SELECT * FROM '.TB_PREF.'purch_order_details WHERE order_no='.(int)$number.' ORDER BY po_detail_item FOR UPDATE',
		'could not reconcile agreement purchase lines');
	if (!$rows || db_num_rows($rows) !== count($intent['line_items']))
		throw new RuntimeException('Native agreement purchase line identities changed.');
	foreach ($intent['line_items'] as $line) {
		$row = db_fetch_assoc($rows);
		if (!$row || (string)$row['item_code'] !== (string)$line['stock_id']
			|| (string)$row['description'] !== (string)$line['item_description']
			|| $row['delivery_date'] !== date2sql($line['req_del_date'])
			|| (float)$row['unit_price'] != (float)$line['price'] || (float)$row['quantity_ordered'] != (float)$line['quantity']
			|| (float)$row['quantity_received'] != 0 || (float)$row['qty_invoiced'] != 0)
			throw new RuntimeException('Native agreement purchase line differs from approved intent.');
	}
}


/** Preserve all retained source fields and prior linked effects across native/completion hooks. */
function verify_agreement_purchase_source($snapshot, $number) {
	$current = db_fetch_assoc(db_query($snapshot['header_query'], 'could not recheck agreement source'));
	$header=$current;
	$expected = $snapshot['header'];
	foreach (array('total_committed','total_ordered','total_received','total_invoiced') as $field) {
		unset($current[$field]); unset($expected[$field]);
	}
	if ($current !== $expected) throw new RuntimeException('Agreement header changed during execution.');
	$rows = db_query($snapshot['line_query'], 'could not recheck agreement source lines');
	$lines = array();
	while ($row = db_fetch_assoc($rows)) $lines[]=$row;
	$current_lines=$lines;
	$expected=$snapshot['lines'];
	foreach ($lines as $key=>$row) foreach (array('ordered_qty','received_qty','invoiced_qty') as $field) unset($lines[$key][$field]);
	foreach ($expected as $key=>$row) foreach (array('ordered_qty','received_qty','invoiced_qty') as $field) unset($expected[$key][$field]);
	if ($lines !== $expected) throw new RuntimeException('Agreement lines changed during execution.');
	$rows = db_query($snapshot['effect_query'], 'could not recheck prior agreement effects');
	$effects=array(); $totals=array('total_ordered'=>0,'total_received'=>0,'total_invoiced'=>0); $quantities=array();
	while ($row=db_fetch_assoc($rows)) {
		if ((int)$row['order_no'] !== (int)$number) $effects[]=$row;
		if ($row['item_code'] === null) continue;
		if (!isset($quantities[$row['item_code']])) $quantities[$row['item_code']]=array('ordered_qty'=>0,'received_qty'=>0,'invoiced_qty'=>0);
		foreach (array('quantity_ordered'=>array('total_ordered','ordered_qty'), 'quantity_received'=>array('total_received','received_qty'), 'qty_invoiced'=>array('total_invoiced','invoiced_qty')) as $field=>$targets) {
			$totals[$targets[0]]+=(float)$row[$field]*(float)$row['unit_price'];
			$quantities[$row['item_code']][$targets[1]]+=(float)$row[$field];
		}
	}
	$totals['total_committed']=0;
	foreach ($current_lines as $line) {
		$totals['total_committed']+=(float)$line['committed_qty']*(float)$line['unit_price']*(1-(float)$line['discount_percent']/100);
		foreach (array('ordered_qty','received_qty','invoiced_qty') as $field) {
			$expected=isset($quantities[$line['stock_id']])?$quantities[$line['stock_id']][$field]:0;
			if (abs((float)$line[$field]-$expected)>0.000001) throw new RuntimeException('Agreement fulfillment quantity differs from native effects.');
		}
	}
	foreach ($totals as $field=>$expected)
		if (abs((float)$header[$field]-$expected)>0.000001) throw new RuntimeException('Agreement fulfillment amount differs from native effects.');
	if ($effects !== $snapshot['effects']) throw new RuntimeException('Prior agreement fulfillment changed.');
}

/** Read only native tax configuration, never transaction or protected payload stores. */
function read_agreement_purchase_tax_configuration($lock) {
	if (empty($GLOBALS['transaction_level'])) throw new RuntimeException('Tax custody requires owning transaction.');
	$queries = array(
		'tax_types'=>'SELECT * FROM '.TB_PREF.'tax_types ORDER BY id',
		'tax_groups'=>'SELECT * FROM '.TB_PREF.'tax_groups ORDER BY id',
		'tax_group_items'=>'SELECT * FROM '.TB_PREF.'tax_group_items ORDER BY tax_group_id,tax_type_id',
		'item_tax_types'=>'SELECT * FROM '.TB_PREF.'item_tax_types ORDER BY id',
		'item_tax_type_exemptions'=>'SELECT * FROM '.TB_PREF.'item_tax_type_exemptions ORDER BY item_tax_type_id,tax_type_id',
		'algorithm'=>"SELECT name,value FROM ".TB_PREF."sys_prefs WHERE name='tax_algorithm' ORDER BY name"
	);
	$configuration=array();
	foreach ($queries as $name=>$query) {
		$rows=db_query($query.($lock?' FOR UPDATE':''), 'could not verify agreement tax configuration');
		$configuration[$name]=array();
		while ($row=db_fetch_assoc($rows)) $configuration[$name][]=$row;
	}
	return $configuration;
}
