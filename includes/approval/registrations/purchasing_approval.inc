<?php
/**********************************************************************
	NotrinosERP-1.0 Approval Workflow System
	Purchasing Module Registration & Executors

	Registers Purchase Order, Supplier Invoice, and GRN transaction
	types for the approval workflow. Provides executor callbacks that
	reconstruct purch_order objects and post approved drafts.

	PHP 5.6+ compatible.
***********************************************************************/

include_once($path_to_root . '/includes/approval/ui/approval_ui.inc');

// =====================================================
// TRANSACTION TYPE REGISTRATIONS
// =====================================================

ApprovalWorkflow::registerTransactionType(ST_PURCHORDER, array(
	'label'          => _('Purchase Order'),
	'application'    => 'AP',
	'executor'       => 'execute_approved_purchase_order',
	'amount_getter'  => 'get_purchase_draft_amount',
	'draft_renderer' => 'render_purchase_draft',
	'edit_url'       => 'purchasing/po_entry_items.php',
));

ApprovalWorkflow::registerTransactionType(ST_SUPPINVOICE, array(
	'label'          => _('Supplier Invoice'),
	'application'    => 'AP',
	'executor'       => 'execute_approved_supplier_invoice',
	'amount_getter'  => 'get_purchase_draft_amount',
	'draft_renderer' => 'render_purchase_draft',
	'edit_url'       => 'purchasing/supplier_invoice.php',
));

// =====================================================
// EXECUTOR CALLBACKS
// =====================================================

/**
 * Restore a purch_order object from draft data array.
 *
 * Creates a new purch_order instance and populates it with saved
 * properties and line items from the JSON draft.
 *
 * @param array $data Draft data array (decoded JSON)
 * @return object Restored purch_order instance
 */
function restore_purchase_order_from_data($data)
{
	global $path_to_root;
	include_once($path_to_root . '/purchasing/includes/po_class.inc');

	$po = new purch_order();

	$po->trans_type       = (int)$data['trans_type'];
	$po->trans_no         = isset($data['trans_no']) ? $data['trans_no'] : 0;
	$po->supplier_id      = (int)$data['supplier_id'];
	$po->supplier_name    = isset($data['supplier_name']) ? $data['supplier_name'] : '';
	$po->curr_code        = isset($data['curr_code']) ? $data['curr_code'] : '';
	$po->supp_ref         = isset($data['supp_ref']) ? $data['supp_ref'] : '';
	$po->delivery_address = isset($data['delivery_address']) ? $data['delivery_address'] : '';
	$po->Comments         = isset($data['Comments']) ? $data['Comments'] : '';
	$po->Location         = isset($data['Location']) ? $data['Location'] : '';
	$po->orig_order_date  = isset($data['orig_order_date']) ? $data['orig_order_date'] : '';
	$po->due_date         = isset($data['due_date']) ? $data['due_date'] : '';
	$po->order_no         = isset($data['order_no']) ? (int)$data['order_no'] : 0;
	$po->reference        = isset($data['reference']) ? $data['reference'] : '';
	$po->tax_group_id     = isset($data['tax_group_id']) ? (int)$data['tax_group_id'] : 0;
	$po->tax_included     = isset($data['tax_included']) ? (int)$data['tax_included'] : 0;
	$po->terms            = isset($data['terms']) ? $data['terms'] : array();
	$po->ex_rate          = isset($data['ex_rate']) ? $data['ex_rate'] : 1;
	$po->cash_account     = isset($data['cash_account']) ? $data['cash_account'] : '';
	$po->dimension        = isset($data['dimension']) ? (int)$data['dimension'] : 0;
	$po->dimension2       = isset($data['dimension2']) ? (int)$data['dimension2'] : 0;
	$po->tax_overrides    = isset($data['tax_overrides']) ? $data['tax_overrides'] : array();
	$po->prep_amount      = isset($data['prep_amount']) ? (float)$data['prep_amount'] : 0;
	$po->fixed_asset      = isset($data['fixed_asset']) ? $data['fixed_asset'] : false;

	// Restore line items
	if (!empty($data['line_items'])) {
		foreach ($data['line_items'] as $line_data) {
			$po->add_to_order(
				count($po->line_items),
				$line_data['stock_id'],
				$line_data['quantity'],
				isset($line_data['item_description']) ? $line_data['item_description'] : '',
				$line_data['price'],
				isset($line_data['units']) ? $line_data['units'] : '',
				isset($line_data['req_del_date']) ? $line_data['req_del_date'] : '',
				isset($line_data['qty_inv']) ? $line_data['qty_inv'] : 0,
				isset($line_data['qty_received']) ? $line_data['qty_received'] : 0
			);
		}
	}

	return $po;
}

/**
 * Execute an approved Purchase Order draft.
 *
 * Reconstructs the purch_order from saved data and calls
 * add_direct_supp_trans() to post the order.
 *
 * @param array $draft_data  Decoded JSON draft data
 * @param int   $reserved_trans_no Reserved transaction number
 * @param int   $draft_id    Approval draft ID
 * @return int|false Posted transaction number or false on failure
 */
function execute_approved_purchase_order($draft_data, $reserved_trans_no, $draft_id)
{
	global $path_to_root;
	include_once($path_to_root . '/purchasing/includes/purchasing_db.inc');

	$po = restore_purchase_order_from_data($draft_data);
	$po->trans_type = ST_PURCHORDER;

	$order_no = add_direct_supp_trans($po);

	return $order_no ? $order_no : false;
}

/**
 * Execute an approved Supplier Invoice draft.
 *
 * @param array $draft_data  Decoded JSON draft data
 * @param int   $reserved_trans_no Reserved transaction number
 * @param int   $draft_id    Approval draft ID
 * @return int|false Posted transaction number or false on failure
 */
function execute_approved_supplier_invoice($draft_data, $reserved_trans_no, $draft_id)
{
	global $path_to_root;
	include_once($path_to_root . '/purchasing/includes/purchasing_db.inc');

	$po = restore_purchase_order_from_data($draft_data);
	$po->trans_type = ST_SUPPINVOICE;

	$order_no = add_direct_supp_trans($po);

	return $order_no ? $order_no : false;
}

// =====================================================
// AMOUNT GETTERS
// =====================================================

/**
 * Calculate the total amount of a purchase draft.
 *
 * @param array $draft_data Decoded JSON draft data
 * @return float Total amount (sum of qty * price)
 */
function get_purchase_draft_amount($draft_data)
{
	$total = 0;
	if (!empty($draft_data['line_items'])) {
		foreach ($draft_data['line_items'] as $line) {
			$price = isset($line['price']) ? (float)$line['price'] : 0;
			$qty   = isset($line['quantity']) ? (float)$line['quantity'] : 0;
			$total += round($qty * $price, 2);
		}
	}
	return $total;
}

// =====================================================
// DRAFT RENDERERS
// =====================================================

/**
 * Render a purchase transaction draft for the approval review UI.
 *
 * @param array $draft_data Decoded JSON draft data
 * @return string HTML
 */
function render_purchase_draft($draft_data)
{
	$html = '<table class="tablestyle" width="100%">';
	$html .= '<tr><th>' . _('Supplier') . '</th><td>' . htmlspecialchars(isset($draft_data['supplier_name']) ? $draft_data['supplier_name'] : $draft_data['supplier_id']) . '</td>';
	$html .= '<th>' . _('Reference') . '</th><td>' . htmlspecialchars(isset($draft_data['reference']) ? $draft_data['reference'] : '') . '</td></tr>';
	$html .= '<tr><th>' . _('Date') . '</th><td>' . htmlspecialchars(isset($draft_data['orig_order_date']) ? $draft_data['orig_order_date'] : '') . '</td>';
	$html .= '<th>' . _('Due Date') . '</th><td>' . htmlspecialchars(isset($draft_data['due_date']) ? $draft_data['due_date'] : '') . '</td></tr>';

	if (!empty($draft_data['supp_ref']))
		$html .= '<tr><th>' . _('Supplier Ref') . '</th><td colspan="3">' . htmlspecialchars($draft_data['supp_ref']) . '</td></tr>';

	if (!empty($draft_data['Comments']))
		$html .= '<tr><th>' . _('Comments') . '</th><td colspan="3">' . htmlspecialchars($draft_data['Comments']) . '</td></tr>';

	$html .= '</table>';

	// Line items
	if (!empty($draft_data['line_items'])) {
		$html .= '<br><table class="tablestyle" width="100%">';
		$html .= '<tr><th>' . _('Item Code') . '</th><th>' . _('Description') . '</th>';
		$html .= '<th>' . _('Qty') . '</th><th>' . _('Price') . '</th><th>' . _('Total') . '</th></tr>';

		$grand_total = 0;
		foreach ($draft_data['line_items'] as $line) {
			$price     = isset($line['price']) ? (float)$line['price'] : 0;
			$qty       = isset($line['quantity']) ? (float)$line['quantity'] : 0;
			$line_total = round($qty * $price, 2);
			$grand_total += $line_total;

			$html .= '<tr><td>' . htmlspecialchars($line['stock_id']) . '</td>';
			$html .= '<td>' . htmlspecialchars(isset($line['item_description']) ? $line['item_description'] : '') . '</td>';
			$html .= '<td align="right">' . number_format($qty, 2) . '</td>';
			$html .= '<td align="right">' . number_format($price, 2) . '</td>';
			$html .= '<td align="right">' . number_format($line_total, 2) . '</td></tr>';
		}

		$html .= '<tr><td colspan="4"><b>' . _('Total') . '</b></td>';
		$html .= '<td align="right"><b>' . number_format($grand_total, 2) . '</b></td></tr>';
		$html .= '</table>';
	}

	return $html;
}
