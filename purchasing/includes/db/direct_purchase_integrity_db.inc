<?php
/** Native readback for an approved direct invoice, inside its existing transaction. */

/** Read the native financial dependencies without including mutable stock valuation or supplier balances. */
function direct_purchase_financial_inputs($cart, $lock) {
	direct_purchase_effect_context();
	if (!($cart instanceof purch_order) || !$cart->is_current_company() || $cart->trans_type != ST_SUPPINVOICE
		|| !is_date($cart->orig_order_date) || !$cart->line_items)
		throw new RuntimeException('Direct invoice financial identity is unavailable.');
	$items = array();
	foreach ($cart->line_items as $line) $items[$line->stock_id] = db_escape($line->stock_id);
	ksort($items);
	$queries = array(
		'supplier'=>'SELECT supplier_id,curr_code,tax_group_id,tax_included,payable_account,purchase_account,'
			.'payment_discount_account,dimension_id,dimension2_id,inactive FROM '.TB_PREF.'suppliers WHERE supplier_id='.db_escape($cart->supplier_id),
		'items'=>'SELECT stock_id,mb_flag,tax_type_id,inventory_account,cogs_account,adjustment_account,sales_account,'
			.'wip_account,dimension_id,dimension2_id,inactive FROM '.TB_PREF.'stock_master WHERE stock_id IN ('.implode(',', $items).') ORDER BY stock_id',
		'preferences'=>"SELECT name,value FROM ".TB_PREF."sys_prefs WHERE name IN ('curr_default','grn_clearing_act','tax_algorithm','exchange_diff_act') ORDER BY name"
	);
	if ($cart->cash_account)
		$queries['bank'] = 'SELECT id,account_code,bank_curr_code,inactive FROM '.TB_PREF.'bank_accounts WHERE id='.db_escape($cart->cash_account);
	$state = array();
	foreach ($queries as $name=>$query) {
		$result = db_query($query.($lock ? ' FOR UPDATE' : ''), 'Could not verify direct invoice financial inputs');
		if (!$result) throw new RuntimeException('Direct invoice financial inputs are unavailable.');
		$state[$name] = array();
		while ($row = db_fetch_assoc($result)) $state[$name][] = $row;
	}
	if (count($state['supplier']) !== 1 || count($state['items']) !== count($items)
		|| ($cart->cash_account && count($state['bank']) !== 1))
		throw new RuntimeException('Direct invoice financial owner is unavailable.');
	$supplier = $state['supplier'][0];
	if ($supplier['inactive'] || $supplier['curr_code'] !== $cart->curr_code
		|| (string)$supplier['tax_group_id'] !== (string)$cart->tax_group_id)
		throw new RuntimeException('Direct invoice supplier currency or tax group changed; reload.');
	foreach ($state['items'] as $item) if ($item['inactive'])
		throw new RuntimeException('Direct invoice item is inactive.');
	if ($cart->cash_account && $state['bank'][0]['inactive'])
		throw new RuntimeException('Direct invoice cash account is inactive.');
	$preferences = array();
	foreach ($state['preferences'] as $row) $preferences[$row['name']] = $row['value'];
	if (empty($preferences['curr_default'])) throw new RuntimeException('Direct invoice home currency is unavailable.');
	foreach (array('curr_default','grn_clearing_act','tax_algorithm','exchange_diff_act') as $name)
		if ((string)get_company_pref($name) !== (string)(isset($preferences[$name]) ? $preferences[$name] : null))
			throw new RuntimeException('Direct invoice cached financial policy changed; reload.');
	global $path_to_root;
	include_once($path_to_root.'/purchasing/includes/db/purch_agreement_po_approval_db.inc');
	$state['tax'] = read_agreement_purchase_tax_configuration($lock);
	$currencies = array($cart->curr_code);
	if ($cart->cash_account) $currencies[] = $state['bank'][0]['bank_curr_code'];
	$currencies = array_unique($currencies); sort($currencies);
	foreach ($currencies as $currency) {
		if ($currency === $preferences['curr_default']) { $state['rates'][$currency] = '1'; continue; }
		// Lock the complete as-of range, including its gaps. A native same-date
		// insertion is compatible only when it preserves the already approved rate.
		$result = db_query('SELECT id,date_,rate_buy,rate_sell FROM '.TB_PREF.'exchange_rates WHERE curr_code='
			.db_escape($currency).' AND date_<='.db_escape(date2sql($cart->orig_order_date))
			.' ORDER BY date_ DESC,id DESC'.($lock ? ' FOR UPDATE' : ''), 'Could not verify direct invoice exchange rate');
		if (!$result) throw new RuntimeException('Direct invoice exchange rate is unavailable.');
		$rate = db_fetch_assoc($result);
		$value = $rate ? $rate['rate_buy'] : ($currency === $cart->curr_code ? $cart->ex_rate : 0);
		if ($currency === $cart->curr_code && (!$rate || $rate['date_'] !== date2sql($cart->orig_order_date))
			&& is_numeric($cart->ex_rate) && $cart->ex_rate > 0)
			$value = $cart->ex_rate;
		if (!is_numeric($value) || !is_finite((float)$value) || $value <= 0)
			throw new RuntimeException('Direct invoice requires a positive native exchange rate.');
		$state['rates'][$currency] = (string)(float)$value;
		if ($rate && $rate['date_'] === date2sql($cart->orig_order_date) && $currency === $cart->curr_code
			&& $cart->ex_rate && (float)$cart->ex_rate !== (float)$value)
			throw new RuntimeException('Direct invoice explicit exchange rate conflicts with its native date rate.');
	}
	return $state;
}

/** Calculate the native gross approval amount, including explicit tax overrides and line rounding. */
function direct_purchase_financial_amount($cart) {
	$total = 0;
	foreach ($cart->line_items as $line) $total += round2($line->quantity * $line->price, user_price_dec());
	if (!$cart->tax_included) foreach ($cart->get_taxes() as $tax)
		$total += isset($tax['Override']) ? $tax['Override'] : $tax['Value'];
	if (!is_numeric($total) || !is_finite((float)$total))
		throw new RuntimeException('Direct invoice approval amount is invalid.');
	return round2($total, user_price_dec());
}

/** Bind a new direct command, or validate an immutable command against current native inputs. */
function direct_purchase_financial_binding($cart, $expected = null) {
	$consistent = direct_purchase_financial_inputs($cart, false);
	$current = direct_purchase_financial_inputs($cart, true);
	if ($consistent !== $current)
		throw new RuntimeException('Direct invoice financial snapshot is stale; retry in a fresh transaction.');
	$taxes = get_tax_group_items_as_array($cart->tax_group_id);
	if ($cart->tax_group_array !== null && json_encode($cart->tax_group_array) !== json_encode($taxes))
		throw new RuntimeException('Direct invoice cached tax rates changed; reload.');
	$encoded = json_encode($current);
	if ($encoded === false) throw new RuntimeException('Direct invoice financial inputs cannot be encoded.');
	$binding = array('version'=>1, 'inputs_sha256'=>hash('sha256', $encoded),
		'amount'=>direct_purchase_financial_amount($cart), 'precision'=>user_price_dec());
	if ($expected !== null && json_encode($expected) !== json_encode($binding))
		throw new RuntimeException('Approved direct invoice financial inputs or amount changed; resubmit.');
	return $binding;
}

/** Bind every effect read to the originally admitted company, actor and native authority. */
function direct_purchase_effect_context() {
	global $db_connections;
	$actor = isset($_SESSION['wa_current_user']) ? $_SESSION['wa_current_user'] : null;
	if (empty($GLOBALS['transaction_level']) || !$actor || !isset($actor->cur_con, $actor->user, $actor->access)
		|| !$actor->can_access('SA_SUPPLIERINVOICE') || !isset($db_connections[$actor->cur_con]['tbpref']))
		throw new RuntimeException('Approved direct purchase effect authority is unavailable.');
	return array((string)$actor->cur_con, $db_connections[$actor->cur_con]['tbpref'],
		(string)$actor->user, (string)$actor->access);
}

/** Read only this native event and its exact PO/GRN lineage, using current locking reads. */
function direct_purchase_native_effect_state($type, $number, $context, $include_progress = true) {
	if (direct_purchase_effect_context() !== $context || !is_scalar($number)
		|| (string)(int)$number !== (string)$number || (int)$number <= 0
		|| !in_array($type, array(ST_SUPPRECEIVE, ST_SUPPINVOICE, ST_SUPPAYMENT), true))
		throw new RuntimeException('Approved direct purchase effect identity changed.');
	$number = (int)$number;
	$queries = array(
		'ledger' => 'SELECT counter,type,type_no,tran_date,account,memo_,amount,dimension_id,dimension2_id,person_type_id,person_id'
			.' FROM '.TB_PREF.'gl_trans WHERE type='.$type.' AND type_no='.$number.' ORDER BY counter',
		'reference' => 'SELECT id,type,reference FROM '.TB_PREF.'refs WHERE type='.$type.' AND id='.$number,
		'audit' => 'SELECT id,type,trans_no,user,gl_date,description,fiscal_year FROM '.TB_PREF
			.'audit_trail WHERE type='.$type.' AND trans_no='.$number.' ORDER BY id',
		'comments' => 'SELECT type,id,date_,memo_ FROM '.TB_PREF.'comments WHERE type='.$type.' AND id='.$number.' ORDER BY date_',
		'number_history' => 'SELECT type,id,date_,memo_ FROM '.TB_PREF.'voided WHERE type='.$type.' AND id='.$number
	);
	if ($type === ST_SUPPRECEIVE) {
		$queries['header'] = 'SELECT id,supplier_id,purch_order_no,reference,delivery_date,loc_code,rate FROM '
			.TB_PREF.'grn_batch WHERE id='.$number;
		$receipt_ids = 'SELECT id FROM '.TB_PREF.'grn_items WHERE grn_batch_id='.$number;
		$po_line_ids = 'SELECT po_detail_item FROM '.TB_PREF.'grn_items WHERE grn_batch_id='.$number;
		$queries['stock'] = 'SELECT trans_id,trans_no,stock_id,type,loc_code,tran_date,price,reference,qty,standard_cost'
			.' FROM '.TB_PREF.'stock_moves WHERE type='.$type.' AND trans_no='.$number.' ORDER BY trans_id';
		if ($include_progress)
			$queries['costs'] = 'SELECT stock_id,material_cost FROM '.TB_PREF.'stock_master WHERE stock_id IN'
				.' (SELECT item_code FROM '.TB_PREF.'grn_items WHERE grn_batch_id='.$number.') ORDER BY stock_id';
	} else {
		$queries['header'] = 'SELECT trans_no,type,supplier_id,reference,supp_reference,tran_date,due_date,ov_amount,'
			.'ov_discount,ov_gst,rate,tax_included'.($include_progress ? ',alloc' : '').' FROM '.TB_PREF
			.'supp_trans WHERE type='.$type.' AND trans_no='.$number.' ORDER BY supplier_id';
		$queries['tax'] = 'SELECT id,trans_type,trans_no,tran_date,tax_type_id,rate,ex_rate,included_in_price,'
			.'net_amount,amount,memo,reg_type FROM '.TB_PREF.'trans_tax_details WHERE trans_type='.$type
			.' AND trans_no='.$number.' ORDER BY id';
		if ($include_progress)
			$queries['allocations'] = 'SELECT id,amt,date_alloc,trans_type_from,trans_no_from,trans_type_to,trans_no_to,person_id'
				.' FROM '.TB_PREF.'supp_allocations WHERE (trans_type_from='.$type.' AND trans_no_from='.$number
				.') OR (trans_type_to='.$type.' AND trans_no_to='.$number.') ORDER BY id';
		if ($type === ST_SUPPINVOICE) {
			$queries['items'] = 'SELECT id,supp_trans_no,supp_trans_type,gl_code,grn_item_id,po_detail_item_id,stock_id,'
				.'description,quantity,unit_price,unit_tax,memo_,dimension_id,dimension2_id FROM '.TB_PREF
				.'supp_invoice_items WHERE supp_trans_type='.$type.' AND supp_trans_no='.$number.' ORDER BY id';
			$receipt_ids = 'SELECT grn_item_id FROM '.TB_PREF.'supp_invoice_items WHERE supp_trans_type='.$type
				.' AND supp_trans_no='.$number;
			$po_line_ids = 'SELECT po_detail_item_id FROM '.TB_PREF.'supp_invoice_items WHERE supp_trans_type='.$type
				.' AND supp_trans_no='.$number;
			$queries['costs'] = 'SELECT stock_id,material_cost FROM '.TB_PREF.'stock_master WHERE stock_id IN'
				.' (SELECT stock_id FROM '.TB_PREF.'supp_invoice_items WHERE supp_trans_type='.$type
				.' AND supp_trans_no='.$number.') ORDER BY stock_id';
		} else
			$queries['bank'] = 'SELECT id,type,trans_no,bank_act,ref,trans_date,amount,dimension_id,dimension2_id,'
				.'person_type_id,person_id,reconciled FROM '.TB_PREF.'bank_trans WHERE type='.$type
				.' AND trans_no='.$number.' ORDER BY id';
	}
	if (isset($receipt_ids)) {
		$queries['receipts'] = 'SELECT id,grn_batch_id,po_detail_item,item_code,description,qty_recd'
			.(($include_progress || $type === ST_SUPPINVOICE) ? ',quantity_inv' : '').' FROM '.TB_PREF.'grn_items WHERE id IN ('.$receipt_ids.') ORDER BY id';
		$queries['source_lines'] = 'SELECT po_detail_item,order_no,item_code,description,delivery_date,unit_price,'
			.'quantity_ordered,quantity_received,std_cost_unit'.(($include_progress || $type === ST_SUPPINVOICE) ? ',qty_invoiced,act_price' : '')
			.' FROM '.TB_PREF.'purch_order_details WHERE po_detail_item IN ('.$po_line_ids.') ORDER BY po_detail_item';
		$queries['source_headers'] = 'SELECT order_no,supplier_id,comments,ord_date,reference,supp_reference,'
			.'into_stock_location,delivery_address,total,tax_included,prep_amount,agreement_id,requisition_id,rfq_id FROM '
			.TB_PREF.'purch_orders WHERE order_no IN (SELECT order_no FROM '.TB_PREF.'purch_order_details'
			.' WHERE po_detail_item IN ('.$po_line_ids.')) ORDER BY order_no';
	}
	$state = array();
	foreach ($queries as $name => $sql) {
		$result = db_query($sql.' FOR UPDATE', 'Could not verify approved direct purchase effects');
		if (!$result) throw new RuntimeException('Approved direct purchase effects are unavailable.');
		$state[$name] = array();
		while ($row = db_fetch($result)) $state[$name][] = $row;
	}
	if (count($state['header']) !== 1 || count($state['reference']) !== 1 || !$state['audit'])
		throw new RuntimeException('Approved direct purchase native effect identity is unavailable.');
	return $state;
}

/** Preserve native rows exactly, allowing only appended shared audit events and their gl_seq handoff. */
function direct_purchase_assert_native_effects($expected, $actual) {
	foreach ($expected['audit'] as $event)
		if (!in_array($event, $actual['audit'], true))
			throw new RuntimeException('Approved direct purchase audit changed.');
	unset($expected['audit'], $actual['audit']);
	if ($expected !== $actual)
		throw new RuntimeException('Approved direct purchase native effects changed.');
}

/** Reconcile the single new cash allocation and both native AP balances before outer commit. */
function direct_purchase_assert_cash_allocation($invoice_no, $payment_no, $supplier_id, $amount, $date, $context) {
	if (direct_purchase_effect_context() !== $context)
		throw new RuntimeException('Approved direct cash allocation authority changed.');
	$result = db_query('SELECT id,amt,date_alloc,trans_type_from,trans_no_from,trans_type_to,trans_no_to,person_id FROM '
		.TB_PREF.'supp_allocations WHERE (trans_type_from='.ST_SUPPAYMENT.' AND trans_no_from='.(int)$payment_no
		.') OR (trans_type_to='.ST_SUPPINVOICE.' AND trans_no_to='.(int)$invoice_no.') ORDER BY id FOR UPDATE',
		'Could not verify approved direct cash allocation');
	$row = $result ? db_fetch($result) : false;
	if (!$row || db_num_rows($result) !== 1 || (int)$row['trans_type_from'] !== ST_SUPPAYMENT
		|| (int)$row['trans_no_from'] !== (int)$payment_no || (int)$row['trans_type_to'] !== ST_SUPPINVOICE
		|| (int)$row['trans_no_to'] !== (int)$invoice_no || (int)$row['person_id'] !== (int)$supplier_id
		|| $row['date_alloc'] !== date2sql($date) || round2($row['amt'] - $amount, user_price_dec()) != 0)
		throw new RuntimeException('Approved direct cash allocation changed.');
	foreach (array(ST_SUPPINVOICE => $invoice_no, ST_SUPPAYMENT => $payment_no) as $type => $number) {
		$result = db_query('SELECT supplier_id,ov_amount,ov_gst,ov_discount,alloc FROM '.TB_PREF.'supp_trans WHERE type='
			.$type.' AND trans_no='.(int)$number.' ORDER BY supplier_id FOR UPDATE',
			'Could not verify approved direct cash balance');
		$row = $result ? db_fetch($result) : false;
		$expected = $type === ST_SUPPAYMENT ? -$amount : $amount;
		if (!$row || db_num_rows($result) !== 1 || (int)$row['supplier_id'] !== (int)$supplier_id
			|| round2($row['alloc'] - $amount, user_price_dec()) != 0
			|| round2($row['ov_amount'] + $row['ov_gst'] + $row['ov_discount'] - $expected, user_price_dec()) != 0)
			throw new RuntimeException('Approved direct cash balance changed.');
	}
}
