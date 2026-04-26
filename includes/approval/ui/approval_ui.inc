<?php
/**********************************************************************
	NotrinosERP-1.0 Approval Workflow System
	Transaction Page UI Helpers

	Provides helper functions for integrating approval workflow into
	transaction entry pages. Used by all module transaction pages to:
	- Check if approval is required before saving
	- Submit transactions for approval instead of direct posting
	- Display approval status messages and banners
	- Collect and serialize cart/form data for draft storage

	PHP 5.6+ compatible.
***********************************************************************/

/**
 * Check if approval is required and submit the transaction for approval.
 *
 * Call this in the transaction page's save/process handler BEFORE the
 * existing write function. If approval is required, this function handles
 * the full submission (draft creation, notification, user message) and
 * returns the result array. The caller should then skip the normal save.
 *
 * If approval is NOT required (no workflow, direct post privilege, or
 * auto-approved and executed), returns false so the caller proceeds normally.
 *
 * @param int    $trans_type Transaction type constant (ST_*)
 * @param array  $draft_data Data to store in the draft (JSON-serializable)
 * @param float  $amount     Transaction amount for threshold checks
 * @param array  $context    Additional context for the approval:
 *   'summary'     => string  Human-readable description
 *   'currency'    => string  Currency code
 *   'person_type' => int     Person type constant
 *   'person_id'   => string  Person identifier
 *   'loc_code'    => string  Location code
 * @return array|false Result array on submission, false if no approval needed
 */
function approval_check_before_save($trans_type, $draft_data, $amount = 0, $context = array())
{
	$approval = get_approval_workflow_service();

	if (!$approval->isApprovalRequired($trans_type, $amount))
		return false;

	$result = $approval->submit($trans_type, $draft_data, $amount, $context);

	if ($result['status'] === 'auto_approved') {
		// Draft was auto-approved and already executed via registered executor.
		// The caller should treat this as a successful post.
		return $result;
	}

	if ($result['status'] === 'pending') {
		// Draft submitted for approval — display message and stop page.
		display_notification_centered(sprintf(
			_('Transaction has been submitted for approval. Draft #%d, Reference: %s'),
			$result['draft_id'],
			$result['reference']
		));

		display_note(_('The transaction is now pending approval. You can track its status on the Approval Dashboard.'), 0, 1);

		hyperlink_no_params('admin/approval_dashboard.php', _('Go to Approval Dashboard'));

		display_footer_exit();
		// Never reaches here
		return $result;
	}

	if ($result['status'] === 'no_workflow') {
		// No active workflow — proceed with normal save
		return false;
	}

	if ($result['status'] === 'execution_failed') {
		return false;
	}

	// Unexpected status — treat as error
	display_error(sprintf(_('Approval submission failed: %s'), $result['status']));
	return $result;
}

/**
 * Collect items_cart data as a plain array for JSON draft storage.
 *
 * Used for GL Journal, Bank Payment/Deposit, Inventory Adjustment,
 * and Inventory Transfer transactions.
 *
 * @param object $cart The items_cart instance
 * @return array Plain array representation of the cart
 */
function collect_items_cart_data($cart)
{
	$data = array(
		'trans_type'    => $cart->trans_type,
		'order_id'      => $cart->order_id,
		'tran_date'     => $cart->tran_date,
		'doc_date'      => isset($cart->doc_date) ? $cart->doc_date : '',
		'event_date'    => isset($cart->event_date) ? $cart->event_date : '',
		'source_ref'    => isset($cart->source_ref) ? $cart->source_ref : '',
		'reference'     => $cart->reference,
		'memo_'         => isset($cart->memo_) ? $cart->memo_ : '',
		'currency'      => isset($cart->currency) ? $cart->currency : '',
		'rate'          => isset($cart->rate) ? $cart->rate : 1,
		'from_loc'      => isset($cart->from_loc) ? $cart->from_loc : '',
		'to_loc'        => isset($cart->to_loc) ? $cart->to_loc : '',
		'transfer_type' => isset($cart->transfer_type) ? $cart->transfer_type : 0,
		'increase'      => isset($cart->increase) ? $cart->increase : 0,
		'branch_id'     => isset($cart->branch_id) ? $cart->branch_id : 0,
		'fixed_asset'   => isset($cart->fixed_asset) ? $cart->fixed_asset : false,
		'tax_info'      => isset($cart->tax_info) ? $cart->tax_info : false,
		'vat_category'  => isset($cart->vat_category) ? $cart->vat_category : null,
	);

	// Collect line items
	$data['line_items'] = array();
	if (!empty($cart->line_items)) {
		foreach ($cart->line_items as $key => $line) {
			$data['line_items'][$key] = array(
				'stock_id'         => $line->stock_id,
				'item_description' => $line->item_description,
				'units'            => isset($line->units) ? $line->units : '',
				'mb_flag'          => isset($line->mb_flag) ? $line->mb_flag : '',
				'quantity'         => $line->quantity,
				'price'            => isset($line->price) ? $line->price : 0,
				'standard_cost'    => isset($line->standard_cost) ? $line->standard_cost : 0,
			);
		}
	}

	// Collect GL items
	$data['gl_items'] = array();
	if (!empty($cart->gl_items)) {
		foreach ($cart->gl_items as $key => $gl) {
			$data['gl_items'][$key] = array(
				'code_id'        => $gl->code_id,
				'dimension_id'   => $gl->dimension_id,
				'dimension2_id'  => $gl->dimension2_id,
				'amount'         => $gl->amount,
				'reference'      => isset($gl->reference) ? $gl->reference : '',
				'description'    => isset($gl->description) ? $gl->description : '',
				'person_id'      => isset($gl->person_id) ? $gl->person_id : null,
				'person_type_id' => isset($gl->person_type_id) ? $gl->person_type_id : null,
				'person_name'    => isset($gl->person_name) ? $gl->person_name : '',
				'branch_id'      => isset($gl->branch_id) ? $gl->branch_id : null,
				'date_'          => isset($gl->date_) ? $gl->date_ : '',
			);
		}
	}

	return $data;
}

/**
 * Restore an items_cart object from draft data array.
 *
 * @param array $data Draft data array (decoded JSON)
 * @return object Restored items_cart instance
 */
function restore_items_cart_from_data($data)
{
	$cart = new items_cart($data['trans_type'], 0);

	$cart->order_id      = isset($data['order_id']) ? $data['order_id'] : 0;
	$cart->tran_date     = $data['tran_date'];
	$cart->doc_date      = isset($data['doc_date']) ? $data['doc_date'] : '';
	$cart->event_date    = isset($data['event_date']) ? $data['event_date'] : '';
	$cart->source_ref    = isset($data['source_ref']) ? $data['source_ref'] : '';
	$cart->reference     = $data['reference'];
	$cart->memo_         = isset($data['memo_']) ? $data['memo_'] : '';
	$cart->currency      = isset($data['currency']) ? $data['currency'] : '';
	$cart->rate          = isset($data['rate']) ? $data['rate'] : 1;
	$cart->from_loc      = isset($data['from_loc']) ? $data['from_loc'] : '';
	$cart->to_loc        = isset($data['to_loc']) ? $data['to_loc'] : '';
	$cart->transfer_type = isset($data['transfer_type']) ? $data['transfer_type'] : 0;
	$cart->increase      = isset($data['increase']) ? $data['increase'] : 0;
	$cart->branch_id     = isset($data['branch_id']) ? $data['branch_id'] : 0;
	$cart->fixed_asset   = isset($data['fixed_asset']) ? $data['fixed_asset'] : false;
	$cart->tax_info      = isset($data['tax_info']) ? $data['tax_info'] : false;
	$cart->vat_category  = isset($data['vat_category']) ? $data['vat_category'] : null;

	// Restore GL items
	if (!empty($data['gl_items'])) {
		foreach ($data['gl_items'] as $gl_data) {
			$cart->add_gl_item(
				$gl_data['code_id'],
				$gl_data['dimension_id'],
				$gl_data['dimension2_id'],
				$gl_data['amount'],
				isset($gl_data['reference']) ? $gl_data['reference'] : '',
				isset($gl_data['description']) ? $gl_data['description'] : '',
				isset($gl_data['person_id']) ? $gl_data['person_id'] : null,
				isset($gl_data['person_type_id']) ? $gl_data['person_type_id'] : null
			);
		}
	}

	// Restore line items
	if (!empty($data['line_items'])) {
		foreach ($data['line_items'] as $line_data) {
			$cart->add_to_cart(
				count($cart->line_items),
				$line_data['stock_id'],
				$line_data['quantity'],
				isset($line_data['standard_cost']) ? $line_data['standard_cost'] : 0
			);
		}
	}

	return $cart;
}

/**
 * Collect Sales Cart data as a plain array for JSON draft storage.
 *
 * Used for Sales Invoice, Sales Order, Customer Delivery, and Credit Note.
 *
 * @param object $cart The Cart instance (sales/includes/cart_class.inc)
 * @return array Plain array representation
 */
function collect_sales_cart_data($cart)
{
	$data = array(
		'trans_type'         => $cart->trans_type,
		'trans_no'           => $cart->trans_no,
		'customer_id'        => $cart->customer_id,
		'customer_name'      => isset($cart->customer_name) ? $cart->customer_name : '',
		'customer_currency'  => isset($cart->customer_currency) ? $cart->customer_currency : '',
		'document_date'      => $cart->document_date,
		'due_date'           => isset($cart->due_date) ? $cart->due_date : '',
		'reference'          => $cart->reference,
		'Comments'           => isset($cart->Comments) ? $cart->Comments : '',
		'Location'           => isset($cart->Location) ? $cart->Location : '',
		'location_name'      => isset($cart->location_name) ? $cart->location_name : '',
		'Branch'             => isset($cart->Branch) ? $cart->Branch : 0,
		'salesman'           => isset($cart->salesman) ? $cart->salesman : 0,
		'deliver_to'         => isset($cart->deliver_to) ? $cart->deliver_to : '',
		'delivery_address'   => isset($cart->delivery_address) ? $cart->delivery_address : '',
		'phone'              => isset($cart->phone) ? $cart->phone : '',
		'email'              => isset($cart->email) ? $cart->email : '',
		'ship_via'           => isset($cart->ship_via) ? $cart->ship_via : 0,
		'freight_cost'       => isset($cart->freight_cost) ? $cart->freight_cost : 0,
		'tax_group_id'       => isset($cart->tax_group_id) ? $cart->tax_group_id : 0,
		'tax_included'       => isset($cart->tax_included) ? $cart->tax_included : 0,
		'sales_type'         => isset($cart->sales_type) ? $cart->sales_type : 0,
		'default_discount'   => isset($cart->default_discount) ? $cart->default_discount : 0,
		'price_factor'       => isset($cart->price_factor) ? $cart->price_factor : 1,
		'dimension_id'       => isset($cart->dimension_id) ? $cart->dimension_id : 0,
		'dimension2_id'      => isset($cart->dimension2_id) ? $cart->dimension2_id : 0,
		'pos'                => isset($cart->pos) ? $cart->pos : null,
		'cash_account'       => isset($cart->cash_account) ? $cart->cash_account : '',
		'account_name'       => isset($cart->account_name) ? $cart->account_name : '',
		'payment'            => isset($cart->payment) ? $cart->payment : 0,
		'payment_terms'      => isset($cart->payment_terms) ? $cart->payment_terms : array(),
		'credit'             => isset($cart->credit) ? $cart->credit : 0,
		'prepaid'            => isset($cart->prepaid) ? $cart->prepaid : false,
		'prep_amount'        => isset($cart->prep_amount) ? $cart->prep_amount : 0,
		'ex_rate'            => isset($cart->ex_rate) ? $cart->ex_rate : 1,
		'fixed_asset'        => isset($cart->fixed_asset) ? $cart->fixed_asset : false,
		'bo_policy'          => isset($cart->bo_policy) ? $cart->bo_policy : null,
		'so_type'            => isset($cart->so_type) ? $cart->so_type : 0,
	);

	// Collect line items
	$data['line_items'] = array();
	if (!empty($cart->line_items)) {
		foreach ($cart->line_items as $key => $line) {
			$data['line_items'][$key] = array(
				'id'                => isset($line->id) ? $line->id : 0,
				'stock_id'          => $line->stock_id,
				'item_description'  => $line->item_description,
				'units'             => isset($line->units) ? $line->units : '',
				'mb_flag'           => isset($line->mb_flag) ? $line->mb_flag : '',
				'tax_type'          => isset($line->tax_type) ? $line->tax_type : 0,
				'tax_type_name'     => isset($line->tax_type_name) ? $line->tax_type_name : '',
				'src_no'            => isset($line->src_no) ? $line->src_no : 0,
				'src_id'            => isset($line->src_id) ? $line->src_id : '',
				'price'             => $line->price,
				'discount_percent'  => isset($line->discount_percent) ? $line->discount_percent : 0,
				'quantity'          => $line->quantity,
				'qty_done'          => isset($line->qty_done) ? $line->qty_done : 0,
				'qty_dispatched'    => isset($line->qty_dispatched) ? $line->qty_dispatched : 0,
				'qty_old'           => isset($line->qty_old) ? $line->qty_old : 0,
				'standard_cost'     => isset($line->standard_cost) ? $line->standard_cost : 0,
				'descr_editable'    => isset($line->descr_editable) ? $line->descr_editable : 0,
			);
		}
	}

	return $data;
}

/**
 * Collect Purchase Order data as a plain array for JSON draft storage.
 *
 * @param object $po The purch_order instance
 * @return array Plain array representation
 */
function collect_purchase_order_data($po)
{
	$data = array(
		'trans_type'        => $po->trans_type,
		'trans_no'          => isset($po->trans_no) ? $po->trans_no : 0,
		'supplier_id'       => $po->supplier_id,
		'supplier_name'     => isset($po->supplier_name) ? $po->supplier_name : '',
		'curr_code'         => isset($po->curr_code) ? $po->curr_code : '',
		'supp_ref'          => isset($po->supp_ref) ? $po->supp_ref : '',
		'delivery_address'  => isset($po->delivery_address) ? $po->delivery_address : '',
		'Comments'          => isset($po->Comments) ? $po->Comments : '',
		'Location'          => isset($po->Location) ? $po->Location : '',
		'agreement_id'      => isset($po->agreement_id) ? $po->agreement_id : 0,
		'requisition_id'    => isset($po->requisition_id) ? $po->requisition_id : 0,
		'rfq_id'            => isset($po->rfq_id) ? $po->rfq_id : 0,
		'orig_order_date'   => isset($po->orig_order_date) ? $po->orig_order_date : '',
		'due_date'          => isset($po->due_date) ? $po->due_date : '',
		'order_no'          => isset($po->order_no) ? $po->order_no : 0,
		'reference'         => isset($po->reference) ? $po->reference : '',
		'tax_group_id'      => isset($po->tax_group_id) ? $po->tax_group_id : 0,
		'tax_included'      => isset($po->tax_included) ? $po->tax_included : 0,
		'terms'             => isset($po->terms) ? $po->terms : array(),
		'ex_rate'           => isset($po->ex_rate) ? $po->ex_rate : 1,
		'cash_account'      => isset($po->cash_account) ? $po->cash_account : '',
		'dimension'         => isset($po->dimension) ? $po->dimension : 0,
		'dimension2'        => isset($po->dimension2) ? $po->dimension2 : 0,
		'tax_overrides'     => isset($po->tax_overrides) ? $po->tax_overrides : array(),
		'prep_amount'       => isset($po->prep_amount) ? $po->prep_amount : 0,
		'fixed_asset'       => isset($po->fixed_asset) ? $po->fixed_asset : false,
	);

	// Collect line items
	$data['line_items'] = array();
	if (!empty($po->line_items)) {
		foreach ($po->line_items as $key => $line) {
			$data['line_items'][$key] = array(
				'line_no'          => isset($line->line_no) ? $line->line_no : $key,
				'po_detail_rec'    => isset($line->po_detail_rec) ? $line->po_detail_rec : 0,
				'grn_item_id'      => isset($line->grn_item_id) ? $line->grn_item_id : 0,
				'stock_id'         => $line->stock_id,
				'item_description' => $line->item_description,
				'price'            => $line->price,
				'units'            => isset($line->units) ? $line->units : '',
				'req_del_date'     => isset($line->req_del_date) ? $line->req_del_date : '',
				'tax_type'         => isset($line->tax_type) ? $line->tax_type : 0,
				'tax_type_name'    => isset($line->tax_type_name) ? $line->tax_type_name : '',
				'quantity'         => $line->quantity,
				'qty_inv'          => isset($line->qty_inv) ? $line->qty_inv : 0,
				'receive_qty'      => isset($line->receive_qty) ? $line->receive_qty : 0,
				'qty_received'     => isset($line->qty_received) ? $line->qty_received : 0,
				'standard_cost'    => isset($line->standard_cost) ? $line->standard_cost : 0,
				'descr_editable'   => isset($line->descr_editable) ? $line->descr_editable : 0,
			);
		}
	}

	return $data;
}

/**
 * Collect Supplier Transaction data as a plain array for JSON draft storage.
 *
 * Used for supplier invoice entry (`supp_trans` object) so approval can resume
 * the same GRN-based invoice flow after approval.
 *
 * @param object $supp_trans The supp_trans instance
 * @return array Plain array representation
 */
function collect_supp_trans_data($supp_trans)
{
	$data = array(
		'trans_type' => isset($supp_trans->trans_type) ? $supp_trans->trans_type : ST_SUPPINVOICE,
		'trans_no' => isset($supp_trans->trans_no) ? $supp_trans->trans_no : 0,
		'supplier_id' => isset($supp_trans->supplier_id) ? $supp_trans->supplier_id : 0,
		'supplier_name' => isset($supp_trans->supplier_name) ? $supp_trans->supplier_name : '',
		'currency' => isset($supp_trans->currency) ? $supp_trans->currency : '',
		'tax_group_id' => isset($supp_trans->tax_group_id) ? $supp_trans->tax_group_id : 0,
		'tax_included' => isset($supp_trans->tax_included) ? $supp_trans->tax_included : 0,
		'tran_date' => isset($supp_trans->tran_date) ? $supp_trans->tran_date : '',
		'due_date' => isset($supp_trans->due_date) ? $supp_trans->due_date : '',
		'reference' => isset($supp_trans->reference) ? $supp_trans->reference : '',
		'supp_reference' => isset($supp_trans->supp_reference) ? $supp_trans->supp_reference : '',
		'Comments' => isset($supp_trans->Comments) ? $supp_trans->Comments : '',
		'ov_amount' => isset($supp_trans->ov_amount) ? $supp_trans->ov_amount : 0,
		'ov_discount' => isset($supp_trans->ov_discount) ? $supp_trans->ov_discount : 0,
		'ov_gst' => isset($supp_trans->ov_gst) ? $supp_trans->ov_gst : 0,
		'dimension' => isset($supp_trans->dimension) ? $supp_trans->dimension : 0,
		'dimension2' => isset($supp_trans->dimension2) ? $supp_trans->dimension2 : 0,
		'ex_rate' => isset($supp_trans->ex_rate) ? $supp_trans->ex_rate : 1,
		'tax_overrides' => isset($supp_trans->tax_overrides) ? $supp_trans->tax_overrides : array(),
	);

	$data['grn_items'] = array();
	if (!empty($supp_trans->grn_items)) {
		foreach ($supp_trans->grn_items as $key => $line) {
			$data['grn_items'][$key] = array(
				'id' => isset($line->id) ? $line->id : 0,
				'po_detail_item' => isset($line->po_detail_item) ? $line->po_detail_item : 0,
				'item_code' => isset($line->item_code) ? $line->item_code : '',
				'item_description' => isset($line->item_description) ? $line->item_description : '',
				'qty_recd' => isset($line->qty_recd) ? $line->qty_recd : 0,
				'prev_quantity_inv' => isset($line->prev_quantity_inv) ? $line->prev_quantity_inv : 0,
				'this_quantity_inv' => isset($line->this_quantity_inv) ? $line->this_quantity_inv : 0,
				'order_price' => isset($line->order_price) ? $line->order_price : 0,
				'chg_price' => isset($line->chg_price) ? $line->chg_price : 0,
				'std_cost_unit' => isset($line->std_cost_unit) ? $line->std_cost_unit : null,
				'gl_code' => isset($line->gl_code) ? $line->gl_code : '',
			);
		}
	}

	$data['gl_codes'] = array();
	if (!empty($supp_trans->gl_codes)) {
		foreach ($supp_trans->gl_codes as $key => $line) {
			$data['gl_codes'][$key] = array(
				'gl_code' => isset($line->gl_code) ? $line->gl_code : '',
				'gl_act_name' => isset($line->gl_act_name) ? $line->gl_act_name : '',
				'gl_dim' => isset($line->gl_dim) ? $line->gl_dim : 0,
				'gl_dim2' => isset($line->gl_dim2) ? $line->gl_dim2 : 0,
				'amount' => isset($line->amount) ? $line->amount : 0,
				'memo_' => isset($line->memo_) ? $line->memo_ : '',
			);
		}
	}

	return $data;
}

/**
 * Calculate the total amount from an items_cart for approval threshold checks.
 *
 * Returns the absolute value of the total debit (or credit) amount.
 *
 * @param object $cart The items_cart instance
 * @return float Total amount
 */
function get_items_cart_total($cart)
{
	if (method_exists($cart, 'gl_items_total_debit'))
		return abs($cart->gl_items_total_debit());

	// Fallback: sum line items
	$total = 0;
	if (!empty($cart->line_items)) {
		foreach ($cart->line_items as $line) {
			$total += abs($line->quantity * (isset($line->standard_cost) ? $line->standard_cost : 0));
		}
	}

	return $total;
}

/**
 * Display an approval pending banner on a transaction page.
 *
 * Shows an info box if the given transaction has a pending approval draft.
 *
 * @param int $trans_type Transaction type constant
 * @param int $trans_no   Transaction number
 * @return void
 */
function display_approval_pending_banner($trans_type, $trans_no)
{
	if (!$trans_no)
		return;

	$draft = get_approval_draft_by_trans($trans_type, $trans_no);

	if ($draft && (int)$draft['status'] === APPROVAL_STATUS_PENDING) {
		display_warning(sprintf(
			_('This transaction is pending approval (Draft #%d, Level %d). It cannot be edited until approved or rejected.'),
			$draft['id'],
			$draft['current_level']
		));
	}
}
