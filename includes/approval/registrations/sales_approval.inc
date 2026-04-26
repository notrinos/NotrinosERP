<?php
/**********************************************************************
	NotrinosERP-1.0 Approval Workflow System
	Sales Module Registration & Executors

	Registers Sales Invoice, Sales Order, and Customer Delivery transaction
	types for the approval workflow. Provides executor callbacks that
	reconstruct Cart objects and post approved drafts.

	PHP 5.6+ compatible.
***********************************************************************/

include_once($path_to_root . '/includes/approval/ui/approval_ui.inc');

// =====================================================
// TRANSACTION TYPE REGISTRATIONS
// =====================================================

ApprovalWorkflow::registerTransactionType(ST_SALESINVOICE, array(
	'label'          => _('Sales Invoice'),
	'application'    => 'orders',
	'executor'       => 'execute_approved_sales_invoice',
	'amount_getter'  => 'get_sales_draft_amount',
	'draft_renderer' => 'render_sales_draft',
	'edit_url'       => 'sales/customer_invoice.php',
));

ApprovalWorkflow::registerTransactionType(ST_SALESORDER, array(
	'label'          => _('Sales Order'),
	'application'    => 'orders',
	'executor'       => 'execute_approved_sales_order',
	'amount_getter'  => 'get_sales_draft_amount',
	'draft_renderer' => 'render_sales_draft',
	'edit_url'       => 'sales/sales_order_entry.php',
));

ApprovalWorkflow::registerTransactionType(ST_CUSTDELIVERY, array(
	'label'          => _('Customer Delivery'),
	'application'    => 'orders',
	'executor'       => 'execute_approved_customer_delivery',
	'amount_getter'  => 'get_sales_draft_amount',
	'draft_renderer' => 'render_sales_draft',
	'edit_url'       => 'sales/customer_delivery.php',
));

// Phase 5: Sales RMA
ApprovalWorkflow::registerTransactionType(ST_SALESRETURN, array(
	'label'          => _('Sales RMA'),
	'application'    => 'orders',
	'executor'       => 'execute_approved_sales_rma',
	'amount_getter'  => 'get_rma_draft_amount',
	'draft_renderer' => 'render_rma_draft',
	'edit_url'       => 'sales/sales_rma_entry.php',
));

// =====================================================
// EXECUTOR CALLBACKS
// =====================================================

/**
 * Restore a Sales Cart object from draft data array.
 *
 * Creates a new Cart instance and populates it with saved properties
 * and line items from the JSON draft.
 *
 * @param array $data Draft data array (decoded JSON)
 * @return object Restored Cart instance
 */
function restore_sales_cart_from_data($data)
{
	global $path_to_root;
	include_once($path_to_root . '/sales/includes/cart_class.inc');
	include_once($path_to_root . '/sales/includes/db/sales_discount_db.inc');

	$cart = new Cart($data['trans_type']);

	$customer_id = isset($data['customer_id']) ? $data['customer_id'] : '';
	$customer_name = isset($data['customer_name']) ? $data['customer_name'] : '';
	$customer_currency = isset($data['customer_currency']) ? $data['customer_currency'] : '';
	$default_discount = isset($data['default_discount']) ? $data['default_discount'] : 0;
	$payment = isset($data['payment']) ? $data['payment'] : 0;
	$branch_id = isset($data['Branch']) ? $data['Branch'] : 0;
	$tax_group_id = isset($data['tax_group_id']) ? $data['tax_group_id'] : 0;
	$sales_type = isset($data['sales_type']) ? $data['sales_type'] : 0;
	$tax_included = isset($data['tax_included']) ? $data['tax_included'] : 0;
	$price_factor = isset($data['price_factor']) ? $data['price_factor'] : 1;

	$cart->trans_no = isset($data['trans_no']) ? $data['trans_no'] : array(0 => 0);
	$cart->set_customer($customer_id, $customer_name, $customer_currency, $default_discount, $payment);
	$cart->set_branch($branch_id, $tax_group_id, '', isset($data['phone']) ? $data['phone'] : '', isset($data['email']) ? $data['email'] : '');
	$cart->set_sales_type($sales_type, '', $tax_included, $price_factor);
	$cart->set_location(isset($data['Location']) ? $data['Location'] : '', isset($data['location_name']) ? $data['location_name'] : '');
	$cart->set_delivery(
		isset($data['ship_via']) ? $data['ship_via'] : 0,
		isset($data['deliver_to']) ? $data['deliver_to'] : '',
		isset($data['delivery_address']) ? $data['delivery_address'] : '',
		isset($data['freight_cost']) ? $data['freight_cost'] : 0
	);

	// Restore core properties
	$cart->document_date     = $data['document_date'];
	$cart->due_date          = isset($data['due_date']) ? $data['due_date'] : '';
	$cart->reference         = $data['reference'];
	$cart->Comments          = isset($data['Comments']) ? $data['Comments'] : '';
	$cart->salesman          = isset($data['salesman']) ? $data['salesman'] : 0;
	$cart->phone             = isset($data['phone']) ? $data['phone'] : '';
	$cart->email             = isset($data['email']) ? $data['email'] : '';
	$cart->dimension_id      = isset($data['dimension_id']) ? $data['dimension_id'] : 0;
	$cart->dimension2_id     = isset($data['dimension2_id']) ? $data['dimension2_id'] : 0;
	$cart->pos               = isset($data['pos']) ? $data['pos'] : null;
	$cart->cash_account      = isset($data['cash_account']) ? $data['cash_account'] : '';
	$cart->account_name      = isset($data['account_name']) ? $data['account_name'] : '';
	$cart->payment_terms     = isset($data['payment_terms']) ? $data['payment_terms'] : array();
	$cart->credit            = isset($data['credit']) ? $data['credit'] : 0;
	$cart->prepaid           = isset($data['prepaid']) ? $data['prepaid'] : false;
	$cart->prep_amount       = isset($data['prep_amount']) ? $data['prep_amount'] : 0;
	$cart->ex_rate           = isset($data['ex_rate']) ? $data['ex_rate'] : 1;
	$cart->fixed_asset       = isset($data['fixed_asset']) ? $data['fixed_asset'] : false;
	$cart->bo_policy         = isset($data['bo_policy']) ? $data['bo_policy'] : null;

	// Phase 2: Quotation Templates & CRM Pipeline Integration
	$cart->template_id          = isset($data['template_id']) ? (int)$data['template_id'] : 0;
	$cart->validity_date        = isset($data['validity_date']) ? $data['validity_date'] : null;
	$cart->opportunity_id       = isset($data['opportunity_id']) ? (int)$data['opportunity_id'] : 0;
	$cart->terms_and_conditions = isset($data['terms_and_conditions']) ? $data['terms_and_conditions'] : '';
	$cart->margin_total         = isset($data['margin_total']) ? (float)$data['margin_total'] : 0;
	$cart->cost_total           = isset($data['cost_total']) ? (float)$data['cost_total'] : 0;
	$cart->optional_items       = isset($data['optional_items']) ? $data['optional_items'] : array();

	// Phase 3: Sales Agreements & Contracts
	$cart->agreement_id         = isset($data['agreement_id']) ? (int)$data['agreement_id'] : 0;

	if (isset($data['sales_discount_state']))
		restore_cart_discount_state($cart, $data['sales_discount_state']);

	// Restore line items
	if (!empty($data['line_items'])) {
		foreach ($data['line_items'] as $line_data) {
			$cart->add_to_cart(
				count($cart->line_items),
				$line_data['stock_id'],
				$line_data['quantity'],
				$line_data['price'],
				isset($line_data['discount_percent']) ? $line_data['discount_percent'] : 0,
				isset($line_data['qty_done']) ? $line_data['qty_done'] : 0,
				isset($line_data['standard_cost']) ? $line_data['standard_cost'] : 0,
				isset($line_data['item_description']) ? $line_data['item_description'] : '',
				isset($line_data['id']) ? $line_data['id'] : 0,
				isset($line_data['src_no']) ? $line_data['src_no'] : 0
			);
		}
	}

	return $cart;
}

/**
 * Execute an approved Sales Invoice draft.
 *
 * Reconstructs the Cart from saved data and calls Cart::write()
 * to post the invoice.
 *
 * @param array $draft_data  Decoded JSON draft data
 * @param int   $reserved_trans_no Reserved transaction number
 * @param int   $draft_id    Approval draft ID
 * @return int|false Posted transaction number or false on failure
 */
function execute_approved_sales_invoice($draft_data, $reserved_trans_no, $draft_id)
{
	global $path_to_root;
	include_once($path_to_root . '/sales/includes/sales_db.inc');

	$cart = restore_sales_cart_from_data($draft_data);
	$cart->trans_type = ST_SALESINVOICE;

	$invoice_no = $cart->write();

	if ($invoice_no == -1)
		return false;

	return $invoice_no;
}

/**
 * Execute an approved Sales Order draft.
 *
 * @param array $draft_data  Decoded JSON draft data
 * @param int   $reserved_trans_no Reserved transaction number
 * @param int   $draft_id    Approval draft ID
 * @return int|false Posted transaction number or false on failure
 */
function execute_approved_sales_order($draft_data, $reserved_trans_no, $draft_id)
{
	global $path_to_root;
	include_once($path_to_root . '/sales/includes/sales_db.inc');

	$cart = restore_sales_cart_from_data($draft_data);
	$cart->trans_type = ST_SALESORDER;

	$ret = $cart->write(1);

	if ($ret == -1)
		return false;

	return key($cart->trans_no);
}

/**
 * Execute an approved Customer Delivery draft.
 *
 * @param array $draft_data  Decoded JSON draft data
 * @param int   $reserved_trans_no Reserved transaction number
 * @param int   $draft_id    Approval draft ID
 * @return int|false Posted transaction number or false on failure
 */
function execute_approved_customer_delivery($draft_data, $reserved_trans_no, $draft_id)
{
	global $path_to_root;
	include_once($path_to_root . '/sales/includes/sales_db.inc');

	$cart = restore_sales_cart_from_data($draft_data);
	$cart->trans_type = ST_CUSTDELIVERY;

	$policy = isset($draft_data['bo_policy']) ? (int)$draft_data['bo_policy'] : 0;
	$ret = $cart->write($policy);

	if ($ret == -1)
		return false;

	return key($cart->trans_no);
}

// =====================================================
// AMOUNT GETTERS
// =====================================================

/**
 * Calculate the total amount of a sales draft.
 *
 * @param array $draft_data Decoded JSON draft data
 * @return float Total amount (sum of qty * price * (1 - discount))
 */
function get_sales_draft_amount($draft_data)
{
	$total = 0;
	if (!empty($draft_data['line_items'])) {
		foreach ($draft_data['line_items'] as $line) {
			$price    = isset($line['price']) ? (float)$line['price'] : 0;
			$qty      = isset($line['quantity']) ? (float)$line['quantity'] : 0;
			$discount = isset($line['discount_percent']) ? (float)$line['discount_percent'] : 0;
			$total += round($qty * $price * (1 - $discount), 2);
		}
	}
	$total += isset($draft_data['freight_cost']) ? (float)$draft_data['freight_cost'] : 0;

	return $total;
}

// =====================================================
// DRAFT RENDERERS
// =====================================================

/**
 * Render a sales transaction draft for the approval review UI.
 *
 * @param array $draft_data Decoded JSON draft data
 * @return string HTML
 */
function render_sales_draft($draft_data)
{
	$html = '<table class="tablestyle" width="100%">';
	$html .= '<tr><th>' . _('Customer') . '</th><td>' . htmlspecialchars(isset($draft_data['customer_name']) ? $draft_data['customer_name'] : $draft_data['customer_id']) . '</td>';
	$html .= '<th>' . _('Reference') . '</th><td>' . htmlspecialchars($draft_data['reference']) . '</td></tr>';
	$html .= '<tr><th>' . _('Date') . '</th><td>' . htmlspecialchars($draft_data['document_date']) . '</td>';
	$html .= '<th>' . _('Due Date') . '</th><td>' . htmlspecialchars(isset($draft_data['due_date']) ? $draft_data['due_date'] : '') . '</td></tr>';

	if (!empty($draft_data['deliver_to']))
		$html .= '<tr><th>' . _('Deliver To') . '</th><td colspan="3">' . htmlspecialchars($draft_data['deliver_to']) . '</td></tr>';

	if (!empty($draft_data['Comments']))
		$html .= '<tr><th>' . _('Comments') . '</th><td colspan="3">' . htmlspecialchars($draft_data['Comments']) . '</td></tr>';

	$html .= '</table>';

	// Line items
	if (!empty($draft_data['line_items'])) {
		$html .= '<br><table class="tablestyle" width="100%">';
		$html .= '<tr><th>' . _('Item Code') . '</th><th>' . _('Description') . '</th>';
		$html .= '<th>' . _('Qty') . '</th><th>' . _('Price') . '</th><th>' . _('Disc %') . '</th><th>' . _('Total') . '</th></tr>';

		$grand_total = 0;
		foreach ($draft_data['line_items'] as $line) {
			$price    = isset($line['price']) ? (float)$line['price'] : 0;
			$qty      = isset($line['quantity']) ? (float)$line['quantity'] : 0;
			$discount = isset($line['discount_percent']) ? (float)$line['discount_percent'] : 0;
			$line_total = round($qty * $price * (1 - $discount), 2);
			$grand_total += $line_total;

			$html .= '<tr><td>' . htmlspecialchars($line['stock_id']) . '</td>';
			$html .= '<td>' . htmlspecialchars(isset($line['item_description']) ? $line['item_description'] : '') . '</td>';
			$html .= '<td align="right">' . number_format($qty, 2) . '</td>';
			$html .= '<td align="right">' . number_format($price, 2) . '</td>';
			$html .= '<td align="right">' . number_format($discount * 100, 1) . '%</td>';
			$html .= '<td align="right">' . number_format($line_total, 2) . '</td></tr>';
		}

		if (!empty($draft_data['freight_cost'])) {
			$grand_total += (float)$draft_data['freight_cost'];
			$html .= '<tr><td colspan="5"><i>' . _('Freight') . '</i></td>';
			$html .= '<td align="right">' . number_format((float)$draft_data['freight_cost'], 2) . '</td></tr>';
		}

		$html .= '<tr><td colspan="5"><b>' . _('Total') . '</b></td>';
		$html .= '<td align="right"><b>' . number_format($grand_total, 2) . '</b></td></tr>';
		$html .= '</table>';
	}

	return $html;
}

// =====================================================
// PHASE 5: RMA APPROVAL HELPERS
// =====================================================

/**
 * Execute an approved Sales RMA draft.
 *
 * Authorizes the RMA (changes status from pending → authorized).
 *
 * @param array $draft_data      Decoded JSON draft data
 * @param int   $reserved_trans_no Reserved transaction number
 * @param int   $draft_id        Approval draft ID
 * @return int|false  RMA ID or false on failure
 */
function execute_approved_sales_rma($draft_data, $reserved_trans_no, $draft_id)
{
	global $path_to_root;
	include_once($path_to_root . '/sales/includes/db/sales_rma_db.inc');

	$rma_id = isset($draft_data['rma_id']) ? (int)$draft_data['rma_id'] : 0;
	if (!$rma_id)
		return false;

	if (authorize_rma($rma_id, _('Approved via workflow')))
		return $rma_id;

	return false;
}

/**
 * Get the refund amount of a Sales RMA draft.
 *
 * @param array $draft_data
 * @return float
 */
function get_rma_draft_amount($draft_data)
{
	return isset($draft_data['refund_amount']) ? (float)$draft_data['refund_amount'] : 0;
}

/**
 * Render a Sales RMA draft for the approval review UI.
 *
 * @param array $draft_data
 * @return string HTML
 */
function render_rma_draft($draft_data)
{
	$html = '<table class="tablestyle" width="100%">';
	$html .= '<tr><th>' . _('Customer') . '</th><td>' . htmlspecialchars(isset($draft_data['customer_name']) ? $draft_data['customer_name'] : '') . '</td>';
	$html .= '<th>' . _('Reference') . '</th><td>' . htmlspecialchars(isset($draft_data['reference']) ? $draft_data['reference'] : '') . '</td></tr>';
	$html .= '<tr><th>' . _('Request Date') . '</th><td>' . htmlspecialchars(isset($draft_data['request_date']) ? $draft_data['request_date'] : '') . '</td>';
	$html .= '<th>' . _('Return Method') . '</th><td>' . htmlspecialchars(isset($draft_data['return_method']) ? $draft_data['return_method'] : '') . '</td></tr>';
	if (!empty($draft_data['customer_notes']))
		$html .= '<tr><th>' . _('Customer Notes') . '</th><td colspan="3">' . htmlspecialchars($draft_data['customer_notes']) . '</td></tr>';
	$html .= '</table>';

	if (!empty($draft_data['lines'])) {
		$html .= '<br><table class="tablestyle" width="100%">';
		$html .= '<tr><th>' . _('Item') . '</th><th>' . _('Description') . '</th>';
		$html .= '<th>' . _('Qty') . '</th><th>' . _('Unit Price') . '</th><th>' . _('Condition') . '</th></tr>';
		$grand_total = 0;
		foreach ($draft_data['lines'] as $line) {
			$qty   = isset($line['quantity_authorized']) ? (float)$line['quantity_authorized'] : 0;
			$price = isset($line['unit_price']) ? (float)$line['unit_price'] : 0;
			$grand_total += $qty * $price;
			$html .= '<tr><td>' . htmlspecialchars($line['stock_id']) . '</td>';
			$html .= '<td>' . htmlspecialchars(isset($line['description']) ? $line['description'] : '') . '</td>';
			$html .= '<td align="right">' . number_format($qty, 2) . '</td>';
			$html .= '<td align="right">' . number_format($price, 2) . '</td>';
			$html .= '<td>' . htmlspecialchars(isset($line['return_condition']) ? $line['return_condition'] : '') . '</td></tr>';
		}
		$html .= '<tr><td colspan="3"><b>' . _('Refund Amount') . '</b></td>';
		$html .= '<td align="right" colspan="2"><b>' . number_format(isset($draft_data['refund_amount']) ? (float)$draft_data['refund_amount'] : $grand_total, 2) . '</b></td></tr>';
		$html .= '</table>';
	}

	return $html;
}
