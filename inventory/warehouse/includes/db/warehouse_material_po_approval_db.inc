<?php
/** Material-request PO custody through the existing registered purchasing workflow. */

/** Verify native hook output and stored header/lines against the locked, immutable zero-price intent. */
function verify_material_request_purchase_order_effect($intent, $cart, $number) {
	$actual = json_decode(json_encode($cart), true);
	$actual['order_no'] = $intent['order_no'];
	$actual['reference'] = $intent['reference'];
	foreach ($actual['line_items'] as $key=>$line)
		$actual['line_items'][$key]['po_detail_rec'] = $intent['line_items'][$key]['po_detail_rec'];
	if ($actual !== $intent)
		throw new RuntimeException('Native material purchase intent changed.');
	$fields = array('supplier_id'=>$intent['supplier_id'], 'Comments'=>$intent['Comments'],
		'agreement_id'=>$intent['agreement_id'], 'requisition_id'=>$intent['requisition_id'], 'rfq_id'=>$intent['rfq_id'],
		'ord_date'=>date2sql($intent['orig_order_date']), 'supp_reference'=>$intent['supp_ref'],
		'into_stock_location'=>$intent['Location'], 'delivery_address'=>$intent['delivery_address'],
		'total'=>0, 'tax_included'=>$intent['tax_included'], 'prep_amount'=>$intent['prep_amount']);
	$numeric = array('supplier_id','agreement_id','requisition_id','rfq_id','total','tax_included','prep_amount');
	$where = 'order_no='.(int)$number;
	foreach ($fields as $column=>$value)
		$where .= ' AND '.(in_array($column,$numeric,true)?'':'BINARY ').$column.'='.db_escape($value);
	$row = db_fetch(db_query('SELECT order_no FROM '.TB_PREF.'purch_orders WHERE '.$where.' FOR UPDATE',
		'could not reconcile material purchase header'));
	if (!$row)
		throw new RuntimeException('Native material purchase header differs from approved intent.');
	$rows = db_query('SELECT * FROM '.TB_PREF.'purch_order_details WHERE order_no='.(int)$number.' ORDER BY po_detail_item FOR UPDATE',
		'could not reconcile material purchase lines');
	if (!$rows || db_num_rows($rows) !== count($intent['line_items']))
		throw new RuntimeException('Native material purchase line identities changed.');
	foreach ($intent['line_items'] as $line) {
		$row = db_fetch($rows);
		if (!$row || (string)$row['item_code'] !== (string)$line['stock_id']
			|| (string)$row['description'] !== (string)$line['item_description']
			|| $row['delivery_date'] !== date2sql($line['req_del_date'])
			|| (float)$row['unit_price'] != 0 || (float)$row['quantity_ordered'] != (float)$line['quantity']
			|| (float)$row['quantity_received'] != 0 || (float)$row['qty_invoiced'] != 0)
			throw new RuntimeException('Native material purchase line differs from approved intent.');
	}
}

/** Read only matching PO draft identities, using a current locking read under the source lock. */
function material_request_purchase_order_has_pending($request_id) {
	if (empty($GLOBALS['transaction_level']) || !function_exists('approval_draft_current_json_sql'))
		throw new RuntimeException('Pending conversion custody requires an owning transaction.');
	$json = approval_draft_current_json_sql();
	$sql = 'SELECT id FROM '.TB_PREF.'approval_drafts WHERE trans_type='.ST_PURCHORDER
		.' AND status=0 AND CASE WHEN JSON_VALID('.$json.') THEN '
		."JSON_UNQUOTE(JSON_EXTRACT(".$json.", '$.material_request_conversion.request_id'))="
		.db_escape((string)$request_id).' ELSE 1 END ORDER BY id FOR UPDATE';
	$result = db_query($sql, 'could not verify pending material purchase conversion');
	if (!$result)
		throw new RuntimeException('Pending conversion custody is unavailable.');
	return db_num_rows($result) > 0;
}

/** Collect the native PO intent plus an immutable digest of the complete locked material source. */
function collect_material_request_purchase_order_data($snapshot) {
	$data = collect_purchase_order_data($snapshot['cart']);
	$source = json_encode(array($snapshot['header'], $snapshot['lines']));
	if ($data === false || !is_string($source) || json_last_error() !== JSON_ERROR_NONE)
		throw new RuntimeException('Material purchase intent could not be encoded.');
	$data['material_request_conversion'] = array('version'=>1,
		'request_id'=>(int)$snapshot['header']['request_id'],
		'order_date'=>date2sql($snapshot['cart']->orig_order_date),
		'source_sha256'=>hash('sha256', $source));
	return $data;
}

/** Admit the exact current company and source/destination authority before any source identity lookup. */
function material_request_purchase_order_envelope_authorized($data) {
	$cart = new purch_order();
	return is_array($data) && $cart->is_current_company()
		&& isset($_SESSION['wa_current_user'])
		&& $_SESSION['wa_current_user']->can_access('SA_MATERIALREQUEST')
		&& $_SESSION['wa_current_user']->can_access('SA_PURCHASEORDER')
		&& isset($data['company'], $data['company_prefix'], $data['trans_type'], $data['supplier_id'],
			$data['material_request_conversion'])
		&& is_scalar($data['company']) && (string)$data['company'] === (string)$cart->company
		&& $data['company_prefix'] === $cart->company_prefix
		&& is_scalar($data['trans_type']) && (string)$data['trans_type'] === (string)ST_PURCHORDER
		&& is_array($data['material_request_conversion'])
		&& isset($data['material_request_conversion']['version'], $data['material_request_conversion']['request_id'],
			$data['material_request_conversion']['order_date'], $data['material_request_conversion']['source_sha256'])
		&& $data['material_request_conversion']['version'] === 1
		&& is_int($data['material_request_conversion']['request_id']) && $data['material_request_conversion']['request_id'] > 0
		&& is_string($data['material_request_conversion']['order_date'])
		&& preg_match('/^[0-9]{4}-[0-9]{2}-[0-9]{2}$/D', $data['material_request_conversion']['order_date'])
		&& is_string($data['material_request_conversion']['source_sha256'])
		&& preg_match('/^[a-f0-9]{64}$/D', $data['material_request_conversion']['source_sha256']);
}

/** Rebuild only the locked original intent; drift requires cancellation and a fresh submission. */
function restore_material_request_purchase_order_snapshot($data) {
	if (!material_request_purchase_order_envelope_authorized($data))
		throw new RuntimeException('Material purchase envelope is unavailable.');
	$source = $data['material_request_conversion'];
	$date = sql2date($source['order_date']);
	if (!is_date($date) || date2sql($date) !== $source['order_date'])
		throw new RuntimeException('Material purchase date is invalid.');
	$snapshot = prepare_material_request_purchase_order($source['request_id'], $data['supplier_id'], $date);
	$expected = collect_material_request_purchase_order_data($snapshot);
	// Normalize only JSON numeric representation, as the central workflow does.
	if (json_decode(json_encode($expected), true) !== json_decode(json_encode($data), true))
		throw new RuntimeException('Material purchase source or approved intent changed.');
	return $snapshot;
}

/** Validate only material conversion envelopes; legacy ordinary PO drafts retain their own contract. */
function validate_purchase_order_source_action($action, $data, $draft, $actor, $comments = '') {
	global $path_to_root;
	if (is_array($data) && array_key_exists('rfq_conversion', $data)) {
		include_once($path_to_root.'/purchasing/includes/db/purch_rfq_po_approval_db.inc');
		return !array_key_exists('agreement_conversion', $data) && !array_key_exists('material_request_conversion', $data)
			&& validate_rfq_purchase_order_action($action, $data, $draft, $actor, $comments);
	}
	if (is_array($data) && array_key_exists('agreement_conversion', $data)) {
		include_once($path_to_root.'/purchasing/includes/db/purch_agreement_po_approval_db.inc');
		return !array_key_exists('material_request_conversion', $data)
			&& validate_agreement_purchase_order_action($action, $data, $draft, $actor, $comments);
	}
	if (!is_array($data) || !array_key_exists('material_request_conversion', $data))
		return true;
	include_once($path_to_root.'/purchasing/includes/po_class.inc');
	include_once($path_to_root.'/inventory/warehouse/includes/db/warehouse_material_requests_db.inc');
	if (!material_request_purchase_order_envelope_authorized($data)
		|| (int)$actor['user_id'] !== (int)$_SESSION['wa_current_user']->user
		|| (int)$actor['role_id'] !== (int)$_SESSION['wa_current_user']->access)
		return false;
	// Material conversion always requires an independent human checker, even at zero price.
	if ($action === 'auto_approve' || ($draft && in_array($action, array('approve','reject'), true)
		&& (int)$draft['submitted_by'] === (int)$actor['user_id']))
		return false;
	// Terminal decisions release central reservation custody without changing source/history.
	if ($action === 'reject' || $action === 'cancel')
		return true;
	try {
		$snapshot = restore_material_request_purchase_order_snapshot($data);
		return $action !== 'submit' || !material_request_purchase_order_has_pending($snapshot['header']['request_id']);
	} catch (Exception $exception) {
		return false;
	}
}

/** Submit immutable source intent or execute the existing direct-policy command, never both. */
function submit_material_request_purchase_order($request_id, $supplier_id) {
	global $path_to_root;
	include_once($path_to_root.'/purchasing/includes/db/po_db.inc');
	include_once($path_to_root.'/purchasing/includes/po_class.inc');
	if (!begin_transaction())
		return false;
	try {
		$snapshot = prepare_material_request_purchase_order($request_id, $supplier_id);
		if (!class_exists('ApprovalWorkflow') || !ApprovalWorkflow::isRegistered(ST_PURCHORDER)
			|| material_request_purchase_order_has_pending($request_id))
			throw new RuntimeException('Material purchase approval custody is unavailable or pending.');
		// A new service prevents a prior request-local policy cache from choosing a stale workflow.
		$actor = $_SESSION['wa_current_user'];
		$approval = new ApprovalWorkflow($actor->access, $actor->user);
		if ($approval->isApprovalRequired(ST_PURCHORDER, $snapshot['cart']->get_trans_total(), true)) {
			$data = collect_material_request_purchase_order_data($snapshot);
			$result = $approval->submit(ST_PURCHORDER, $data, $snapshot['cart']->get_trans_total(),
				array('currency'=>$snapshot['cart']->curr_code, 'person_id'=>$supplier_id,
					'loc_code'=>$snapshot['cart']->Location, 'summary'=>$snapshot['cart']->Comments), true);
			if ($result['status'] !== 'pending')
				throw new RuntimeException('Material purchase approval submission failed.');
			if (!$snapshot['cart']->is_current_company() || !material_request_purchase_order_envelope_authorized($data))
				throw new RuntimeException('Material purchase submission authority changed.');
			restore_material_request_purchase_order_snapshot($data);
		} else {
			$number = convert_material_request_to_po($request_id, $supplier_id, $snapshot['cart']->Location);
			if (!$number)
				throw new RuntimeException('Material purchase direct conversion failed.');
			$result = array('status'=>'posted', 'trans_no'=>(int)$number);
		}
		if (!commit_transaction() && !empty($GLOBALS['transaction_level']))
			throw new RuntimeException('Material purchase submission commit failed.');
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
function execute_material_request_purchase_order($data, $reserved_trans_no, $draft_id) {
	if (empty($GLOBALS['transaction_level']) || !material_request_purchase_order_envelope_authorized($data))
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
		$snapshot = restore_material_request_purchase_order_snapshot($data);
		$actor_id = $_SESSION['wa_current_user']->user;
		$role_id = $_SESSION['wa_current_user']->access;
		if (!has_user_approved_draft($draft_id, $actor_id, (int)$draft['current_level']))
			return false;
		$number = write_material_request_purchase_order($snapshot, (int)$draft_id);
		if ($actor_id !== $_SESSION['wa_current_user']->user || $role_id !== $_SESSION['wa_current_user']->access)
			return false;
		return $number;
	} catch (Exception $exception) {
		return false;
	}
}
