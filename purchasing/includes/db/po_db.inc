<?php
/**********************************************************************
	Copyright (C) NotrinosERP.
	Released under the terms of the GNU General Public License, GPL, 
	as published by the Free Software Foundation, either version 3 
	of the License, or (at your option) any later version.
	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  
	See the License here <http://www.gnu.org/licenses/gpl-3.0.html>.
***********************************************************************/
include_once($path_to_root . '/purchasing/includes/db/purch_orders_entity.inc');

/** Load native supplier defaults; an owning source conversion can request a current locking read. */
function get_supplier_details_to_order(&$order, $supplier_id, $lock_supplier = false) {
	$sql = "SELECT curr_code, supp_name, tax_group_id, supp.tax_included, dimension_id, dimension2_id, supp.credit_limit - Sum((ov_amount + ov_gst + ov_discount)) as cur_credit, terms.terms, terms.days_before_due, terms.day_in_following_month
		FROM ".TB_PREF."suppliers supp
			LEFT JOIN ".TB_PREF."supp_trans trans ON supp.supplier_id = trans.supplier_id
			LEFT JOIN ".TB_PREF."payment_terms terms ON supp.payment_terms=terms.terms_indicator
		WHERE supp.supplier_id = ".db_escape($supplier_id)."
		GROUP BY supp.supp_name";

	if ($lock_supplier) {
		if (empty($GLOBALS['transaction_level']))
			throw new RuntimeException('Current supplier defaults require an owning transaction.');
		$sql .= ' FOR UPDATE';
	}
	$result = db_query($sql, 'The supplier details could not be retreived');
	$myrow = db_fetch($result);

	$order->credit = $myrow['cur_credit'];
	$order->terms = array( 
		'description' => $myrow['terms'],
		'days_before_due' => $myrow['days_before_due'], 
		'day_in_following_month' => $myrow['day_in_following_month']);

	$_POST['supplier_id'] = $supplier_id;
	$_POST['supplier_name'] = $myrow['supp_name'];
	$_POST['curr_code'] = $myrow['curr_code'];
	$_POST['dimension'] = $myrow['dimension_id'];
	$_POST['dimension2'] = $myrow['dimension2_id'];

	$order->set_supplier($supplier_id, $myrow['supp_name'], $myrow['curr_code'], $myrow['tax_group_id'], $myrow['tax_included']);
}

/**
 * Admit only direct-policy source conversions through the existing PO workflow.
 * Conversion commands have no delayed source envelope yet. Their callers must
 * recheck this boundary after native and completion hooks, before committing.
 * Ordinary PO entry and approved executors keep their existing workflow path.
 *
 * @param purch_order $order Current-company conversion cart
 * @return bool Whether this source conversion can write a PO directly
 */
function purchase_order_conversion_direct_authorized($order) {
	if (!($order instanceof purch_order) || !$order->is_current_company()
		|| $order->trans_type != ST_PURCHORDER
		|| !isset($_SESSION['wa_current_user'])
		|| !$_SESSION['wa_current_user']->can_access('SA_PURCHASEORDER'))
		return false;
	if (!class_exists('ApprovalWorkflow') || !ApprovalWorkflow::isRegistered(ST_PURCHORDER)
		|| !function_exists('get_approval_workflow_service')) {
		display_error(_('This conversion is unavailable because purchase order approval could not be verified.'));
		return false;
	}
	$approval = get_approval_workflow_service();
	if (!$approval || $approval->isApprovalRequired(ST_PURCHORDER, $order->get_trans_total(), true)) {
		display_error(_('This conversion is unavailable because purchase order approval is required.'));
		return false;
	}
	return true;
}

/** Retain PO history unless the locked aggregate has no known dependent owner. */
function purchase_order_cancellation_is_unused($po, $header) {
	if (($header['custom_data'] !== null && $header['custom_data'] !== '' && $header['custom_data'] !== '{}')
		|| $header['alloc'] != 0 || $header['prep_amount'] != 0
		|| !empty($header['drop_ship']) || !empty($header['drop_ship_so_no'])
		|| !empty($header['agreement_id']) || !empty($header['requisition_id']) || !empty($header['rfq_id']))
		return false;
	$lines = db_query("SELECT * FROM ".TB_PREF."purch_order_details WHERE order_no=".db_escape($po)
		." ORDER BY po_detail_item FOR UPDATE", 'Could not lock purchase order cancellation lines');
	if (!$lines)
		return false;
	while ($line = db_fetch($lines)) {
		if (($line['custom_data'] !== null && $line['custom_data'] !== '' && $line['custom_data'] !== '{}')
			|| $line['quantity_received'] != 0 || $line['qty_invoiced'] != 0
			|| $line['act_price'] != 0 || $line['std_cost_unit'] != 0
			|| !empty($line['cross_dock_so_no']) || !empty($line['cross_dock_so_line']))
			return false;
		foreach (array('grn_items' => 'po_detail_item', 'supp_invoice_items' => 'po_detail_item_id') as $table => $column) {
			$result = db_query("SELECT ".$column." FROM ".TB_PREF.$table." WHERE ".$column."="
				.db_escape($line['po_detail_item'])." LIMIT 1 FOR UPDATE", 'Could not verify purchase line history');
			if (!$result || db_num_rows($result))
				return false;
		}
	}
	$conditions = array(
		'grn_batch' => 'purch_order_no='.db_escape($po),
		'supp_allocations' => '(trans_type_to='.ST_PURCHORDER.' AND trans_no_to='.db_escape($po)
			.') OR (trans_type_from='.ST_PURCHORDER.' AND trans_no_from='.db_escape($po).')',
		'sales_order_details' => 'drop_ship_po_no='.db_escape($po),
		'wh_material_requests' => 'linked_doc_type='.ST_PURCHORDER.' AND linked_doc_no='.db_escape($po),
		'purch_requisition_lines' => 'po_number='.db_escape($po),
		'procurement_plan_lines' => 'po_number='.db_escape($po),
		'purch_matching_exceptions' => 'po_number='.db_escape($po),
		'attachments' => 'type_no='.ST_PURCHORDER.' AND trans_no='.db_escape($po),
		'comments' => 'type='.ST_PURCHORDER.' AND id='.db_escape($po),
		'voided' => 'type='.ST_PURCHORDER.' AND id='.db_escape($po),
		'gl_trans' => 'type='.ST_PURCHORDER.' AND type_no='.db_escape($po),
		'approval_drafts' => 'trans_type='.ST_PURCHORDER.' AND (reserved_trans_no='.db_escape($po)
			.' OR approved_trans_no='.db_escape($po).')'
	);
	foreach ($conditions as $table => $condition) {
		$result = db_query("SELECT 1 FROM ".TB_PREF.$table." WHERE ".$condition." LIMIT 1 FOR UPDATE",
			'Could not verify purchase order cancellation history');
		if (!$result || db_num_rows($result))
			return false;
	}
	return true;
}

/** Cancel only an authorized, locked and unfulfilled PO; failure retains the cart. */
function delete_po($po) {
	global $Refs;

	if (!isset($_SESSION['wa_current_user'])
		|| !$_SESSION['wa_current_user']->can_access('SA_PURCHASEORDER')
		|| !is_scalar($po) || (string)(int)$po !== (string)$po || (int)$po <= 0)
		return false;
	if (!begin_transaction())
		return false;
	try {
		$header = db_fetch(db_query("SELECT * FROM ".TB_PREF."purch_orders WHERE order_no="
			.db_escape($po)." FOR UPDATE", 'Could not lock purchase order cancellation'));
		if (!$header)
			return reject_purchase_order_update();
		if (!$_SESSION['wa_current_user']->can_access('SA_EDITOTHERSTRANS')) {
			$audit = db_fetch(db_query("SELECT user FROM ".TB_PREF."audit_trail WHERE type=".ST_PURCHORDER
				." AND trans_no=".db_escape($po)." AND gl_seq IS NOT NULL LIMIT 1 FOR UPDATE",
				'Could not verify purchase order cancellation authority'));
			if (!$audit || $audit['user'] != $_SESSION['wa_current_user']->user)
				return reject_purchase_order_update();
		}
		if (!purchase_order_cancellation_is_unused($po, $header))
			return reject_purchase_order_update();
		hook_db_prevoid(ST_PURCHORDER, $po);
		$current = db_fetch(db_query("SELECT * FROM ".TB_PREF."purch_orders WHERE order_no="
			.db_escape($po)." FOR UPDATE", 'Could not recheck purchase order cancellation'));
		if ($current !== $header || !purchase_order_cancellation_is_unused($po, $current))
			return reject_purchase_order_update();
		$Refs->restore_last(ST_PURCHORDER, $po);
		if (!db_query("DELETE FROM ".TB_PREF."purch_order_details WHERE order_no=".db_escape($po),
			'The order detail lines could not be deleted')
			|| !db_query("DELETE FROM ".TB_PREF."purch_orders WHERE order_no=".db_escape($po),
			'The order header could not be deleted'))
			return reject_purchase_order_update();
		add_audit_trail(ST_PURCHORDER, $po, Today(), _('Cancelled.'));
		if (!commit_transaction()) {
			cancel_transaction();
			return false;
		}
		return true;
	} catch (Exception $exception) {
		cancel_transaction();
		throw $exception;
	}
}

/**
 * Create a native PO atomically, including its automatic direct-document parent.
 * The existing approval service owns command admission; this boundary rechecks
 * current module/company authority and preserves failed cart identities for retry.
 * An optional approved source draft binds its validated reservation to the native key.
 * Approved ordinary POs can additionally preserve business intent across prewrite hooks.
 */
function add_po(&$po_obj, $approval_draft_id = 0, $preserve_approved_intent = false) {
	global $Refs;
	if (!purchase_order_creation_authorized($po_obj))
		return false;
	$creation_context = purchase_order_creation_context($po_obj);
	// Native SQL errors can exit before catch blocks and persist the session.
	// Keep generated identities private until this writer has completed.
	$working_order = clone $po_obj;
	foreach ($working_order->line_items as $key => $line)
		$working_order->line_items[$key] = clone $line;
	$original_trans_type = $working_order->trans_type;
	$creation_areas = array(ST_PURCHORDER => 'SA_PURCHASEORDER', ST_SUPPRECEIVE => 'SA_GRN', ST_SUPPINVOICE => 'SA_SUPPLIERINVOICE');
	if (!begin_transaction())
		return false;
	try {
		$approved_order_no = 0;
		if ($approval_draft_id !== 0) {
			if (!function_exists('get_source_approved_purchase_order_number'))
				throw new RuntimeException('Approved purchase order identity is unavailable.');
			$approved_order_no = get_source_approved_purchase_order_number($po_obj, $approval_draft_id);
			if (!$approved_order_no)
				throw new RuntimeException('Approved purchase order identity is unavailable.');
		}
		if ($working_order->reference == 'auto')
			$working_order->reference = $Refs->get_next(ST_PURCHORDER, null,
				array('supplier_id' => $working_order->supplier_id, 'date' => $working_order->orig_order_date));
		$approved_intent = $preserve_approved_intent ? purchase_order_creation_cart_state($working_order) : null;
		hook_db_prewrite($working_order, ST_PURCHORDER);
		if (!purchase_order_creation_authorized($working_order)
			|| purchase_order_creation_context($working_order) !== $creation_context
			|| ($preserve_approved_intent && purchase_order_creation_cart_state($working_order) !== $approved_intent))
			throw new RuntimeException('Purchase order admission changed.');
		/*Insert to purchase order header record */
		$sql = "INSERT INTO ".TB_PREF."purch_orders (".($approved_order_no ? 'order_no, ' : '')."supplier_id, Comments, agreement_id, requisition_id, rfq_id, ord_date, reference, supp_reference, into_stock_location, delivery_address, total, tax_included, prep_amount) VALUES(";
		if ($approved_order_no)
			$sql .= (int)$approved_order_no . ', ';
		$sql .= db_escape($working_order->supplier_id) . "," .
			db_escape($working_order->Comments) . ", " .
			(int)(isset($working_order->agreement_id) ? $working_order->agreement_id : 0) . ", " .
			(int)(isset($working_order->requisition_id) ? $working_order->requisition_id : 0) . ", " .
			(int)(isset($working_order->rfq_id) ? $working_order->rfq_id : 0) . ", '" .
			date2sql($working_order->orig_order_date) . "', " .
			db_escape($working_order->reference) . ", " .
			db_escape($working_order->supp_ref) . ", " .
			db_escape($working_order->Location) . ", " .
			db_escape($working_order->delivery_address) . ", " .
			db_escape($working_order->get_trans_total()). ", " .
			db_escape($working_order->tax_included). ", " .
			db_escape($working_order->prep_amount). ")";

		if (!db_query($sql, 'The purchase order header record could not be inserted'))
			throw new RuntimeException('Purchase order header failed.');

		/*Get the auto increment value of the order number created from the sql above */
		$working_order->order_no = db_insert_id();
		// Auto-increment does not consider the workflow's retained number history.
		// Never publish a PO under a pending, cancelled or rejected identity.
		$number_history = db_query('SELECT id FROM '.TB_PREF.'voided WHERE type='.ST_PURCHORDER
			.' AND id='.(int)$working_order->order_no.' FOR UPDATE', 'Could not verify purchase order number history');
		if (!$number_history)
			throw new RuntimeException('Purchase order identity conflicts with retained approval history; retry.');
		if (db_num_rows($number_history)) {
			// Only this unpublished, newly inserted header may move beyond retained
			// numbers. No lines, references, audit or historical native rows exist
			// under its provisional key. Approved source identities remain exact.
			if ($approved_order_no || !function_exists('get_next_purchase_order_approval_number'))
				throw new RuntimeException('Purchase order identity conflicts with retained approval history; retry.');
			$next_order_no = get_next_purchase_order_approval_number();
			if (!$next_order_no || !db_query('UPDATE '.TB_PREF.'purch_orders SET order_no='.(int)$next_order_no
				.' WHERE order_no='.(int)$working_order->order_no, 'Could not advance unpublished purchase order identity')
				|| db_num_affected_rows() !== 1)
				throw new RuntimeException('Purchase order identity could not advance beyond retained history.');
			$working_order->order_no = $next_order_no;
		}

		/*Insert the purchase order detail records */
		foreach ($working_order->line_items as $line_no => $po_line) {

			if (!$po_line->descr_editable) {
				$data = get_purchase_data($working_order->supplier_id, $po_line->stock_id);
				if ($data !== false && $data['supplier_description'] != '')
					$po_line->item_description = $data['supplier_description'];
			}

			$sql = "INSERT INTO ".TB_PREF."purch_order_details (order_no, item_code, description, delivery_date,	unit_price,	quantity_ordered)
			VALUES (".
				db_escape($working_order->order_no).", ".
				db_escape($po_line->stock_id).",".
				db_escape($po_line->item_description).",'".
				date2sql($po_line->req_del_date) . "'," .
				db_escape($po_line->price) . ", " .
				db_escape($po_line->quantity). ")";

			if (!db_query($sql, 'One of the purchase order detail records could not be inserted'))
				throw new RuntimeException('Purchase order line failed.');
			$working_order->line_items[$line_no]->po_detail_rec = db_insert_id();
		}

		$Refs->save(ST_PURCHORDER, $working_order->order_no, $working_order->reference);

		add_audit_trail(ST_PURCHORDER, $working_order->order_no, $working_order->orig_order_date);
		$created_order_no = $working_order->order_no;
		$created_cart = purchase_order_creation_cart_state($working_order);
		$created_state = purchase_order_creation_native_state($created_order_no);
		hook_db_postwrite($working_order, ST_PURCHORDER);
		// Recheck admission before any completion read can use a switched company.
		if (!$working_order->is_current_company() || $working_order->trans_type != $original_trans_type
			|| purchase_order_creation_context($working_order) !== $creation_context
			|| !$_SESSION['wa_current_user']->can_access($creation_areas[$original_trans_type])
			|| purchase_order_creation_cart_state($working_order) !== $created_cart)
			throw new RuntimeException('Purchase order completion authority changed.');
		$completion_state = purchase_order_creation_native_state($created_order_no);
		// Domain hooks may append audit events through the shared audit writer.
		// Preserve the native event while allowing its standard gl_seq handoff.
		foreach ($created_state['audit'] as $event)
			if (!in_array($event, $completion_state['audit'], true))
				throw new RuntimeException('Purchase order creation audit changed.');
		unset($created_state['audit'], $completion_state['audit']);
		if ($completion_state !== $created_state)
			throw new RuntimeException('Purchase order native completion changed.');
		if (!empty($working_order->agreement_id) && function_exists('update_agreement_fulfillment'))
			update_agreement_fulfillment((int)$working_order->agreement_id);

		if (!commit_transaction()) {
			// Resource release can fail after SQL COMMIT. Keep the published
			// identities in that case so retry cannot create a duplicate order.
			if (empty($GLOBALS['transaction_level'])) {
				foreach (get_object_vars($working_order) as $key => $value)
					$po_obj->$key = $value;
				display_error(_('The purchase order was committed, but resource cleanup needs recovery. Reload the order before continuing.'));
				return false;
			}
			throw new RuntimeException('Purchase order commit failed.');
		}
		foreach (get_object_vars($working_order) as $key => $value)
			$po_obj->$key = $value;
		return $working_order->order_no;
	} catch (Exception $exception) {
		cancel_transaction();
		display_error(_('The purchase order could not be saved. Reload the source before retrying.'));
		return false;
	} catch (Throwable $exception) {
		cancel_transaction();
		throw $exception;
	}
}

/** Bind an admitted native command to its company, supplier, transaction family and current actor. */
function purchase_order_creation_context($cart) {
	$actor = isset($_SESSION['wa_current_user']) ? $_SESSION['wa_current_user'] : null;
	return array((string)$cart->company, $cart->company_prefix, (string)$cart->supplier_id,
		(string)$cart->trans_type, $actor && isset($actor->user) ? (string)$actor->user : null,
		$actor && isset($actor->access) ? (string)$actor->access : null);
}

/** Retain only native command fields; extension-owned metadata remains outside this comparison. */
function purchase_order_creation_cart_state($cart) {
	$state = array();
	foreach (array('order_no','supplier_id','trans_type','orig_order_date','Location','tax_included',
		'prep_amount','reference','Comments','supp_ref','delivery_address','dimension','dimension2',
		'curr_code','tax_group_id','ex_rate','agreement_id','requisition_id','rfq_id') as $field)
		$state[$field] = isset($cart->$field) ? $cart->$field : null;
	$state['lines'] = array();
	foreach ($cart->line_items as $key => $line) {
		$values = array();
		foreach (array('po_detail_rec','stock_id','item_description','req_del_date','price','quantity') as $field)
			$values[$field] = isset($line->$field) ? $line->$field : null;
		$state['lines'][$key] = $values;
	}
	return $state;
}

/** Recheck the actual inserted native aggregate with current locking reads before publication. */
function purchase_order_creation_native_state($order_no) {
	if (empty($GLOBALS['transaction_level']))
		throw new RuntimeException('Purchase order completion requires an owning transaction.');
	$state = array();
	$queries = array(
		'header' => 'SELECT order_no,supplier_id,Comments,agreement_id,requisition_id,rfq_id,ord_date,reference,'
			.'supp_reference,into_stock_location,delivery_address,total,tax_included,prep_amount FROM '.TB_PREF
			.'purch_orders WHERE order_no='.db_escape($order_no),
		'lines' => 'SELECT po_detail_item,order_no,item_code,description,delivery_date,unit_price,quantity_ordered FROM '
			.TB_PREF.'purch_order_details WHERE order_no='.db_escape($order_no).' ORDER BY po_detail_item',
		'reference' => 'SELECT id,type,reference FROM '.TB_PREF.'refs WHERE type='.ST_PURCHORDER
			.' AND id='.db_escape($order_no),
		'audit' => 'SELECT id,type,trans_no,user,gl_date,description,fiscal_year FROM '.TB_PREF
			.'audit_trail WHERE type='.ST_PURCHORDER.' AND trans_no='.db_escape($order_no).' ORDER BY id',
		'number_history' => 'SELECT type,id,memo_ FROM '.TB_PREF.'voided WHERE type='.ST_PURCHORDER
			.' AND id='.db_escape($order_no)
	);
	foreach ($queries as $name => $sql) {
		$result = db_query($sql.' FOR UPDATE', 'Could not verify purchase order completion');
		if (!$result)
			throw new RuntimeException('Purchase order completion is unavailable.');
		$state[$name] = array();
		while ($row = db_fetch($result))
			$state[$name][] = $row;
	}
	if (count($state['header']) !== 1 || !$state['lines'] || count($state['reference']) !== 1
		|| !$state['audit'] || $state['number_history'])
		throw new RuntimeException('Purchase order native identity is unavailable.');
	return $state;
}

/** Check current company and native create permission before hooks, numbers or SQL. */
function purchase_order_creation_authorized($po_obj) {
	if (!is_object($po_obj) || !method_exists($po_obj, 'is_current_company') || !$po_obj->is_current_company()
		|| !isset($_SESSION['wa_current_user']) || !method_exists($_SESSION['wa_current_user'], 'can_access')
		|| !isset($po_obj->order_no) || !is_scalar($po_obj->order_no) || (string)$po_obj->order_no !== '0'
		|| !isset($po_obj->supplier_id) || !is_scalar($po_obj->supplier_id)
		|| (string)(int)$po_obj->supplier_id !== (string)$po_obj->supplier_id || (int)$po_obj->supplier_id <= 0
		|| !isset($po_obj->line_items) || !is_array($po_obj->line_items) || !count($po_obj->line_items))
		return false;
	$areas = array(ST_PURCHORDER => 'SA_PURCHASEORDER', ST_SUPPRECEIVE => 'SA_GRN', ST_SUPPINVOICE => 'SA_SUPPLIERINVOICE');
	if (!isset($po_obj->trans_type) || !is_scalar($po_obj->trans_type) || !isset($areas[$po_obj->trans_type])
		|| !$_SESSION['wa_current_user']->can_access($areas[$po_obj->trans_type]))
		return false;
	foreach ($po_obj->line_items as $line)
		if (!is_object($line) || !isset($line->quantity, $line->price)
			|| !is_numeric($line->quantity) || !is_finite((float)$line->quantity) || $line->quantity <= 0
			|| !is_numeric($line->price) || !is_finite((float)$line->price))
			return false;
	return true;
}

/** Reject an unsafe PO edit without exposing another order's retained payload. */
function reject_purchase_order_update() {
	cancel_transaction();
	display_error(_('The purchase order changed or contains protected line history. Reload it before editing.'));
	return false;
}

/**
 * Maintain existing line identities and owner-retained state in one transaction.
 * Receipt/invoice quantities are authoritative database state, never cart input.
 */
function update_po(&$po_obj) {
	if (!is_object($po_obj) || !method_exists($po_obj, 'is_current_company') || !$po_obj->is_current_company()
		|| !isset($_SESSION['wa_current_user'])
		|| !$_SESSION['wa_current_user']->can_access('SA_PURCHASEORDER')
		|| $po_obj->trans_type != ST_PURCHORDER
		|| !is_scalar($po_obj->order_no)
		|| (string)(int)$po_obj->order_no !== (string)$po_obj->order_no
		|| (int)$po_obj->order_no <= 0)
		return false;
	if (!begin_transaction())
		return false;
	try {
		$order_no = (int)$po_obj->order_no;
		$supplier_id = (int)$po_obj->supplier_id;
		$header = db_fetch(db_query("SELECT supplier_id FROM ".TB_PREF."purch_orders WHERE order_no="
			.db_escape($po_obj->order_no)." FOR UPDATE", 'Could not lock purchase order'));
		if (!$header || $header['supplier_id'] != $po_obj->supplier_id)
			return reject_purchase_order_update();
		if (!$_SESSION['wa_current_user']->can_access('SA_EDITOTHERSTRANS')) {
			// Match check_is_editable(), rechecking current ownership at the writer.
			$audit = db_fetch(db_query("SELECT user FROM ".TB_PREF."audit_trail WHERE type=".ST_PURCHORDER
				." AND trans_no=".db_escape($po_obj->order_no)." AND gl_seq IS NOT NULL LIMIT 1 FOR UPDATE",
				'Could not verify purchase order edit authority'));
			if (!$audit || $audit['user'] != $_SESSION['wa_current_user']->user)
				return reject_purchase_order_update();
		}
		hook_db_prewrite($po_obj, ST_PURCHORDER);
		if (!$po_obj->is_current_company() || !$_SESSION['wa_current_user']->can_access('SA_PURCHASEORDER')
			|| (string)$po_obj->order_no !== (string)$order_no || $po_obj->supplier_id != $supplier_id
			|| $po_obj->trans_type != ST_PURCHORDER)
			return reject_purchase_order_update();
		$result = db_query("SELECT * FROM ".TB_PREF."purch_order_details WHERE order_no="
			.db_escape($po_obj->order_no)." ORDER BY po_detail_item FOR UPDATE", 'Could not lock purchase order lines');
		$existing = array();
		while ($row = db_fetch($result))
			$existing[$row['po_detail_item']] = $row;
		$kept = array();
		foreach ($po_obj->line_items as $po_line) {
			$id = isset($po_line->po_detail_rec) ? $po_line->po_detail_rec : 0;
			if (!is_scalar($id) || (string)(int)$id !== (string)$id || (int)$id < 0
				|| !is_numeric($po_line->quantity) || !is_finite((float)$po_line->quantity)
				|| $po_line->quantity <= 0)
				return reject_purchase_order_update();
			if ($id) {
				if (isset($kept[$id]) || !isset($existing[$id])
					|| $existing[$id]['item_code'] !== $po_line->stock_id
					|| $po_line->quantity < $existing[$id]['quantity_received']
					|| $po_line->quantity < $existing[$id]['qty_invoiced'])
					return reject_purchase_order_update();
				$kept[$id] = true;
			}
		}
		$removed = array_diff_key($existing, $kept);
		foreach ($removed as $id => $row) {
			// Opaque metadata is retained history even when no native axis is recognized.
			if (($row['custom_data'] !== null && $row['custom_data'] !== '' && $row['custom_data'] !== '{}')
				|| $row['quantity_received'] != 0 || $row['qty_invoiced'] != 0
				|| $row['act_price'] != 0 || $row['std_cost_unit'] != 0
				|| !empty($row['cross_dock_so_no']) || !empty($row['cross_dock_so_line']))
				return reject_purchase_order_update();
			$receipts = db_query("SELECT po_detail_item FROM ".TB_PREF."grn_items WHERE po_detail_item="
				.db_escape($id)." LIMIT 1 FOR UPDATE", 'Could not verify purchase receipt history');
			$invoices = db_query("SELECT po_detail_item_id FROM ".TB_PREF."supp_invoice_items WHERE po_detail_item_id="
				.db_escape($id)." LIMIT 1 FOR UPDATE", 'Could not verify purchase invoice history');
			if (db_num_rows($receipts) || db_num_rows($invoices))
				return reject_purchase_order_update();
		}

		/*Update the purchase order header with any changes */
		$sql = "UPDATE ".TB_PREF."purch_orders SET Comments=".db_escape($po_obj->Comments).",
			agreement_id=".(int)(isset($po_obj->agreement_id) ? $po_obj->agreement_id : 0).",
			requisition_id=".(int)(isset($po_obj->requisition_id) ? $po_obj->requisition_id : 0).",
			rfq_id=".(int)(isset($po_obj->rfq_id) ? $po_obj->rfq_id : 0).",
			supp_reference= ".db_escape( $po_obj->supp_ref).",
			into_stock_location=".db_escape($po_obj->Location).",
			ord_date='".date2sql($po_obj->orig_order_date)."',
			delivery_address=".db_escape($po_obj->delivery_address).",
			total=". db_escape($po_obj->get_trans_total()).",
			prep_amount=".db_escape($po_obj->prep_amount).",
			tax_included=".db_escape($po_obj->tax_included)."
		WHERE order_no = ".db_escape($po_obj->order_no);

		db_query($sql, 'The purchase order could not be updated');

		foreach ($removed as $id => $row) {
			$sql = "DELETE FROM ".TB_PREF."purch_order_details WHERE order_no=".db_escape($po_obj->order_no)
				." AND po_detail_item=".db_escape($id);
			db_query($sql, 'Could not remove unused purchase order line');
		}
		$saved_lines = array();

		/*Now Update the purchase order detail records */
		foreach ($po_obj->line_items as $po_line) {

			if (!$po_line->descr_editable) {
				$data = get_purchase_data($po_obj->supplier_id, $po_line->stock_id);
				if ($data !== false && $data['supplier_description'] != '')
					$po_line->item_description = $data['supplier_description'];
			}

			$id = isset($po_line->po_detail_rec) ? (int)$po_line->po_detail_rec : 0;
			$values = "description=".db_escape($po_line->item_description)
				.", delivery_date='".date2sql($po_line->req_del_date)."'"
				.", unit_price=".db_escape($po_line->price)
				.", quantity_ordered=".db_escape($po_line->quantity);
			if ($id) {
				// Leave custom_data, act_price, std_cost_unit and fulfillment state untouched.
				$sql = "UPDATE ".TB_PREF."purch_order_details SET ".$values
					." WHERE order_no=".db_escape($po_obj->order_no)." AND po_detail_item=".db_escape($id);
			} else {
				$sql = "INSERT INTO ".TB_PREF."purch_order_details SET ".$values
					.", order_no=".db_escape($po_obj->order_no).", item_code=".db_escape($po_line->stock_id);
			}
			db_query($sql, 'One of the purchase order detail records could not be updated');
			$saved_lines[] = array($po_line, $id ? $id : (int)db_insert_id(),
				$id ? $existing[$id]['quantity_received'] : 0, $id ? $existing[$id]['qty_invoiced'] : 0);
		}

		reallocate_payments($po_obj->order_no, ST_PURCHORDER, $po_obj->orig_order_date, $po_obj->get_trans_total(), $po_obj->prepayments, $po_obj->supplier_id);

		add_audit_trail($po_obj->trans_type, $po_obj->order_no, Today(), _('Updated.'));
		hook_db_postwrite($po_obj, ST_PURCHORDER);
		if (!commit_transaction())
			return false;
		foreach ($saved_lines as $saved) {
			$saved[0]->po_detail_rec = $saved[1];
			$saved[0]->qty_received = $saved[2];
			$saved[0]->qty_inv = $saved[3];
		}
		if (!empty($po_obj->agreement_id) && function_exists('update_agreement_fulfillment'))
			update_agreement_fulfillment((int)$po_obj->agreement_id);

		return $po_obj->order_no;
	} catch (Exception $exception) {
		cancel_transaction();
		throw $exception;
	}
}

/**
 * Get a purchase order header by order_no.
 * Thin wrapper delegating to purch_orders_entity::find().
 *
 * @param int $order_no Purchase order number
 * @return array|null
 */
function get_po($order_no) {
	return purch_orders_entity::find((int)$order_no);
}

function read_po_header($order_no, &$order) {
		$sql = "SELECT po.*, supplier.*, loc.location_name 
		FROM ".TB_PREF."purch_orders po
		INNER JOIN ".TB_PREF."suppliers supplier ON po.supplier_id = supplier.supplier_id
		LEFT JOIN ".TB_PREF."locations loc ON loc.loc_code = po.into_stock_location
		WHERE po.order_no = ".db_escape($order_no);

	$result = db_query($sql, 'The order cannot be retrieved');

	if (db_num_rows($result) == 1) {

		$myrow = db_fetch($result);

		$order->trans_type = ST_PURCHORDER;
		$order->order_no = $order_no;

		$order->set_supplier($myrow['supplier_id'], $myrow['supp_name'], $myrow['curr_code'], $myrow['tax_group_id'], $myrow['tax_included']);

		$order->credit = get_current_supp_credit($order->supplier_id);

		$order->orig_order_date = sql2date($myrow['ord_date']);
		$order->Comments = $myrow['comments'];
		$order->Location = $myrow['into_stock_location'];
		$order->supp_ref = $myrow['supp_reference'];
		$order->reference = $myrow['reference'];
		$order->delivery_address = $myrow['delivery_address'];
		$order->agreement_id = isset($myrow['agreement_id']) ? (int)$myrow['agreement_id'] : 0;
		$order->requisition_id = isset($myrow['requisition_id']) ? (int)$myrow['requisition_id'] : 0;
		$order->rfq_id = isset($myrow['rfq_id']) ? (int)$myrow['rfq_id'] : 0;
		$order->alloc = $myrow['alloc'];
		$order->prep_amount = $myrow['prep_amount'];
		$order->prepayments = get_payments_for($order_no, ST_PURCHORDER, $myrow['supplier_id']);

		return true;
	}

	display_db_error('FATAL : duplicate purchase order found', '', true);
	return false;
}

function read_po_items($order_no, &$order, $open_items_only=false) {
	/*now populate the line po array with the purchase order details records */

	$sql = "SELECT poline.*, units, editable 
		FROM ".TB_PREF."purch_order_details poline
			LEFT JOIN ".TB_PREF."stock_master item	ON poline.item_code=item.stock_id
		WHERE order_no =".db_escape($order_no);

	if ($open_items_only)
		$sql .= " AND (poline.quantity_ordered > poline.quantity_received) ";

	$sql .= " ORDER BY po_detail_item";

	$result = db_query($sql, 'The lines on the purchase order cannot be retrieved');

	if (db_num_rows($result) > 0) {
		while ($myrow = db_fetch($result)) {
			$data = get_purchase_data($order->supplier_id, $myrow['item_code']);
			if ($data !== false && !$myrow['editable'] && $data['supplier_description'] != '' && 
				$myrow['description'] != $data['supplier_description']) // backward compatibility
				$myrow['description'] = $data['supplier_description'];
			$units = is_null($myrow['units']) ? '' : $myrow['units'];

			if ($order->add_to_order($order->lines_on_order, $myrow['item_code'], $myrow['quantity_ordered'], $myrow['description'], $myrow['unit_price'], $units, sql2date($myrow['delivery_date']), $myrow['qty_invoiced'], $myrow['quantity_received']))
				$order->line_items[$order->lines_on_order-1]->po_detail_rec = $myrow['po_detail_item'];
		} /* line po from purchase order details */
	} //end of checks on returned data set
}

function read_po($order_no, &$order, $open_items_only=false) {

	if (read_po_header($order_no, $order))
		read_po_items($order_no, $order, $open_items_only);
}

/**
 * Return one purchase order detail row.
 *
 * @param int $purchase_order_detail_id
 * @return array|false
 */
function get_po_details($purchase_order_detail_id) {
	$sql = "SELECT *
		FROM " . TB_PREF . "purch_order_details
		WHERE po_detail_item = " . db_escape($purchase_order_detail_id);

	$result = db_query($sql, 'The purchase order detail could not be retrieved');

	return db_fetch($result);
}

function get_po_items($order_no) {
	$sql = "SELECT item_code, quantity_ordered, quantity_received, qty_invoiced FROM ".TB_PREF."purch_order_details WHERE order_no=".db_escape($order_no)." ORDER BY po_detail_item";

	$result = db_query($sql, 'could not query purch order details');
	check_db_error('Could not check that the details of the purchase order had not been changed by another user ', $sql);
	return $result;
}

function get_short_info($stock_id) {
	$sql = "SELECT description, units, mb_flag FROM ".TB_PREF."stock_master WHERE stock_id = ".db_escape($stock_id);

	return db_query($sql,'The stock details for '.$stock_id.' could not be retrieved');
}

function get_sql_for_po_search_completed($from, $to, $supplier_id=ALL_TEXT, $location=ALL_TEXT, $order_number='', $stock_id='', $also_closed=false) {
	$sql = "SELECT 
		porder.order_no, 
		porder.reference, 
		supplier.supp_name, 
		location.location_name,
		porder.supp_reference, 
		porder.ord_date, 
		supplier.curr_code, 
		Sum(line.unit_price*line.quantity_ordered) AS OrderValue,
		Sum(line.delivery_date < '". date2sql(Today()) ."'
		AND (line.quantity_ordered > line.quantity_received)) As OverDue,
		porder.into_stock_location,
		chk.isopen
	FROM ".TB_PREF."purch_orders as porder
		LEFT JOIN (
			SELECT order_no, SUM(quantity_ordered-quantity_received + quantity_ordered-qty_invoiced) isopen
			FROM ".TB_PREF."purch_order_details
			GROUP BY order_no) chk ON chk.order_no=porder.order_no,"
		.TB_PREF."purch_order_details as line, "
		.TB_PREF."suppliers as supplier, "
		.TB_PREF."locations as location
	WHERE porder.order_no = line.order_no
		AND porder.supplier_id = supplier.supplier_id
		AND location.loc_code = porder.into_stock_location ";

	if ($supplier_id != ALL_TEXT)
		$sql .= "AND supplier.supplier_id=".$supplier_id." ";
	if ($order_number != "")
		$sql .= "AND porder.reference LIKE ".db_escape('%'. $order_number . '%');
	else {
		$sql .= " AND porder.ord_date >= '".date2sql($from)."' AND porder.ord_date <= '".date2sql($to)."'";

		if ($location != ALL_TEXT)
			$sql .= " AND porder.into_stock_location = ".db_escape($location);
		if ($stock_id !== '')
			$sql .= " AND line.item_code=".db_escape($stock_id);
		if ($supplier_id != ALL_TEXT)
			$sql .= " AND supplier.supplier_id=".db_escape($supplier_id);
	}

	if (!$also_closed)
		$sql .= " AND isopen";
	$sql .= " GROUP BY porder.order_no";

	return $sql;
}

function get_sql_for_po_search($from, $to, $supplier_id=ALL_TEXT, $location=ALL_TEXT, $order_number='', $stock_id='') {
	$sql = "SELECT 
		porder.order_no, 
		porder.reference,
		supplier.supp_name, 
		location.location_name,
		porder.supp_reference, 
		porder.ord_date,
		supplier.curr_code,
		Sum(line.unit_price*line.quantity_ordered) AS OrderValue,
		Sum(line.delivery_date < '". date2sql(Today()) ."'
		AND (line.quantity_ordered > line.quantity_received)) As OverDue
		FROM ".TB_PREF."purch_orders as porder,"
			.TB_PREF."purch_order_details as line, "
			.TB_PREF."suppliers as supplier, "
			.TB_PREF."locations as location
		WHERE porder.order_no = line.order_no
		AND porder.supplier_id = supplier.supplier_id
		AND location.loc_code = porder.into_stock_location
		AND (line.quantity_ordered > line.quantity_received) ";

	if ($order_number != '')
		$sql .= "AND porder.reference LIKE ".db_escape('%'.$order_number.'%');
	else {
		$sql .= "  AND porder.ord_date >= '".date2sql($from)."' AND porder.ord_date <= '".date2sql($to)."'";

		if ($location != ALL_TEXT)
			$sql .= " AND porder.into_stock_location = ".db_escape($location);
		if ($stock_id != '')
			$sql .= " AND line.item_code=".db_escape($stock_id);
		if ($supplier_id != ALL_TEXT)
			$sql .= " AND supplier.supplier_id=".db_escape($supplier_id);
	} //end not order number selected

	$sql .= " GROUP BY porder.order_no";

	return $sql;
}

/** Bind the native primary key only to the actual independently approved source draft and exact cart. */
function get_source_approved_purchase_order_number($cart, $draft_id) {
	if (empty($GLOBALS['transaction_level']) || !is_int($draft_id) || $draft_id <= 0
		|| !class_exists('ApprovalWorkflow') || !function_exists('get_approval_draft_for_update')
		|| !function_exists('validate_purchase_order_source_action') || !function_exists('collect_purchase_order_data') || !isset($_SESSION['wa_current_user']))
		return false;
	$draft = get_approval_draft_for_update($draft_id);
	$actor = $_SESSION['wa_current_user'];
	if (!$draft || (int)$draft['trans_type'] !== ST_PURCHORDER
		|| (int)$draft['status'] !== APPROVAL_STATUS_APPROVED || !empty($draft['approved_trans_no'])
		|| (int)$draft['reserved_trans_no'] <= 0 || (int)$draft['submitted_by'] === (int)$actor->user
		|| !has_user_approved_draft($draft_id, $actor->user, (int)$draft['current_level']))
		return false;
	$approval = new ApprovalWorkflow($actor->access, $actor->user);
	$data = $approval->decodeStoredDraftData($draft['draft_data']);
	if (!is_array($data)) return false;
	$markers = array_intersect(array('plan_conversion','requisition_conversion','rfq_conversion','agreement_conversion','material_request_conversion'), array_keys($data));
	if (count($markers) !== 1 || !validate_purchase_order_source_action('approve', $data, $draft,
		array('user_id'=>$actor->user, 'role_id'=>$actor->access)))
		return false;
	$expected = $data;
	foreach ($markers as $marker) unset($expected[$marker]);
	$actual = collect_purchase_order_data($cart);
	if (!is_array($actual)) return false;
	// The source writer resolves its native reference immediately before add_po.
	$actual['reference'] = $expected['reference'];
	if (json_decode(json_encode($actual), true) !== json_decode(json_encode($expected), true))
		return false;
	return (int)$draft['reserved_trans_no'];
}
