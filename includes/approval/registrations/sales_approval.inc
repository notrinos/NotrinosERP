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

	$cart = new Cart($data['trans_type']);

	// Restore core properties
	$cart->trans_no          = isset($data['trans_no']) ? $data['trans_no'] : array(0 => 0);
	$cart->customer_id       = $data['customer_id'];
	$cart->customer_name     = isset($data['customer_name']) ? $data['customer_name'] : '';
	$cart->customer_currency = isset($data['customer_currency']) ? $data['customer_currency'] : '';
	$cart->document_date     = $data['document_date'];
	$cart->due_date          = isset($data['due_date']) ? $data['due_date'] : '';
	$cart->reference         = $data['reference'];
	$cart->Comments          = isset($data['Comments']) ? $data['Comments'] : '';
	$cart->Location          = isset($data['Location']) ? $data['Location'] : '';
	$cart->location_name     = isset($data['location_name']) ? $data['location_name'] : '';
	$cart->Branch            = isset($data['Branch']) ? $data['Branch'] : 0;
	$cart->salesman          = isset($data['salesman']) ? $data['salesman'] : 0;
	$cart->deliver_to        = isset($data['deliver_to']) ? $data['deliver_to'] : '';
	$cart->delivery_address  = isset($data['delivery_address']) ? $data['delivery_address'] : '';
	$cart->phone             = isset($data['phone']) ? $data['phone'] : '';
	$cart->email             = isset($data['email']) ? $data['email'] : '';
	$cart->ship_via          = isset($data['ship_via']) ? $data['ship_via'] : 0;
	$cart->freight_cost      = isset($data['freight_cost']) ? $data['freight_cost'] : 0;
	$cart->tax_group_id      = isset($data['tax_group_id']) ? $data['tax_group_id'] : 0;
	$cart->tax_included      = isset($data['tax_included']) ? $data['tax_included'] : 0;
	$cart->sales_type        = isset($data['sales_type']) ? $data['sales_type'] : 0;
	$cart->default_discount  = isset($data['default_discount']) ? $data['default_discount'] : 0;
	$cart->price_factor      = isset($data['price_factor']) ? $data['price_factor'] : 1;
	$cart->dimension_id      = isset($data['dimension_id']) ? $data['dimension_id'] : 0;
	$cart->dimension2_id     = isset($data['dimension2_id']) ? $data['dimension2_id'] : 0;
	$cart->pos               = isset($data['pos']) ? $data['pos'] : null;
	$cart->cash_account      = isset($data['cash_account']) ? $data['cash_account'] : '';
	$cart->account_name      = isset($data['account_name']) ? $data['account_name'] : '';
	$cart->payment           = isset($data['payment']) ? $data['payment'] : 0;
	$cart->payment_terms     = isset($data['payment_terms']) ? $data['payment_terms'] : array();
	$cart->credit            = isset($data['credit']) ? $data['credit'] : 0;
	$cart->prepaid           = isset($data['prepaid']) ? $data['prepaid'] : false;
	$cart->prep_amount       = isset($data['prep_amount']) ? $data['prep_amount'] : 0;
	$cart->ex_rate           = isset($data['ex_rate']) ? $data['ex_rate'] : 1;
	$cart->fixed_asset       = isset($data['fixed_asset']) ? $data['fixed_asset'] : false;
	$cart->bo_policy         = isset($data['bo_policy']) ? $data['bo_policy'] : null;

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
