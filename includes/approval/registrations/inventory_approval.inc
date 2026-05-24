<?php
/**********************************************************************
	NotrinosERP-1.0 Approval Workflow System
	Inventory Module Registration & Executors

	Registers Inventory Adjustment and Location Transfer transaction
	types for the approval workflow. Provides executor callbacks that
	reconstruct and post approved drafts.

	PHP 5.6+ compatible.
***********************************************************************/

include_once($path_to_root . '/includes/approval/ui/approval_ui.inc');
include_once($path_to_root . '/inventory/warehouse/includes/db/warehouse_material_requests_db.inc');

// =====================================================
// TRANSACTION TYPE REGISTRATIONS
// =====================================================

ApprovalWorkflow::registerTransactionType(ST_INVADJUST, array(
	'label'          => _('Inventory Adjustment'),
	'application'    => 'stock',
	'executor'       => 'execute_approved_inventory_adjustment',
	'amount_getter'  => 'get_inventory_adjustment_draft_amount',
	'draft_renderer' => 'render_inventory_adjustment_draft',
	'edit_url'       => 'inventory/adjustments.php',
));

ApprovalWorkflow::registerTransactionType(ST_LOCTRANSFER, array(
	'label'          => _('Inventory Transfer'),
	'application'    => 'stock',
	'executor'       => 'execute_approved_inventory_transfer',
	'amount_getter'  => 'get_inventory_transfer_draft_amount',
	'draft_renderer' => 'render_inventory_transfer_draft',
	'edit_url'       => 'inventory/transfers.php',
));

ApprovalWorkflow::registerTransactionType(ST_WH_MATERIAL_REQUEST, array(
	'label'          => _('Material Request'),
	'application'    => 'stock',
	'executor'       => 'execute_approved_material_request',
	'rejector'       => 'reject_material_request_draft',
	'canceler'       => 'cancel_material_request_draft',
	'amount_getter'  => 'get_material_request_draft_amount',
	'draft_renderer' => 'render_material_request_draft',
	'edit_url'       => 'inventory/warehouse/material_requests.php',
));

// =====================================================
// EXECUTOR CALLBACKS
// =====================================================

/**
 * Execute an approved Inventory Adjustment draft.
 *
 * Reconstructs line items from saved draft data and calls
 * add_stock_adjustment() to post the actual transaction.
 *
 * @param array $draft_data  Decoded JSON draft data
 * @param int   $reserved_trans_no Reserved transaction number
 * @param int   $draft_id    Approval draft ID
 * @return int|false Posted transaction number or false on failure
 */
function execute_approved_inventory_adjustment($draft_data, $reserved_trans_no, $draft_id)
{
	global $path_to_root;

	include_once($path_to_root . '/includes/ui/items_cart.inc');
	include_once($path_to_root . '/inventory/includes/inventory_db.inc');

	// Reconstruct line items as an items_cart
	$cart = restore_items_cart_from_data($draft_data);

	$location  = $draft_data['location'];
	$date_     = $draft_data['date'];
	$reference = $draft_data['reference'];
	$memo_     = isset($draft_data['memo_']) ? $draft_data['memo_'] : '';

	$trans_no = add_stock_adjustment(
		$cart->line_items,
		$location,
		$date_,
		$reference,
		$memo_
	);

	return $trans_no;
}

/**
 * Execute an approved Inventory Transfer draft.
 *
 * @param array $draft_data  Decoded JSON draft data
 * @param int   $reserved_trans_no Reserved transaction number
 * @param int   $draft_id    Approval draft ID
 * @return int|false Posted transaction number or false on failure
 */
function execute_approved_inventory_transfer($draft_data, $reserved_trans_no, $draft_id)
{
	global $path_to_root;

	include_once($path_to_root . '/includes/ui/items_cart.inc');
	include_once($path_to_root . '/inventory/includes/inventory_db.inc');

	// Reconstruct line items
	$cart = restore_items_cart_from_data($draft_data);

	$from_location = $draft_data['from_location'];
	$to_location   = $draft_data['to_location'];
	$date_         = $draft_data['date'];
	$reference     = $draft_data['reference'];
	$memo_         = isset($draft_data['memo_']) ? $draft_data['memo_'] : '';

	$trans_no = add_stock_transfer(
		$cart->line_items,
		$from_location,
		$to_location,
		$date_,
		$reference,
		$memo_
	);

	return $trans_no;
}

/**
 * Execute an approved Material Request draft.
 *
 * @param array $draft_data Decoded JSON draft data
 * @param int   $reserved_trans_no Reserved transaction number
 * @param int   $draft_id Approval draft ID
 * @return int|false
 */
function execute_approved_material_request($draft_data, $reserved_trans_no, $draft_id)
{
	$request_id = isset($draft_data['request_id']) ? (int)$draft_data['request_id'] : 0;
	if ($request_id <= 0)
		return false;

	if (!approve_material_request($request_id))
		return false;

	return $request_id;
}

/**
 * Sync a rejected Material Request draft back to business state.
 *
 * @param array  $draft_data Decoded JSON draft data
 * @param int    $draft_id Approval draft ID
 * @param string $comments Rejection comments
 * @return bool
 */
function reject_material_request_draft($draft_data, $draft_id, $comments)
{
	$request_id = isset($draft_data['request_id']) ? (int)$draft_data['request_id'] : 0;
	if ($request_id <= 0)
		return false;

	return reject_material_request($request_id);
}

/**
 * Sync a cancelled Material Request draft back to business state.
 *
 * @param array  $draft_data Decoded JSON draft data
 * @param int    $draft_id Approval draft ID
 * @param string $comments Cancellation comments
 * @return bool
 */
function cancel_material_request_draft($draft_data, $draft_id, $comments)
{
	$request_id = isset($draft_data['request_id']) ? (int)$draft_data['request_id'] : 0;
	if ($request_id <= 0)
		return false;

	return cancel_material_request($request_id);
}

// =====================================================
// AMOUNT GETTERS
// =====================================================

/**
 * Calculate total amount of an inventory adjustment draft.
 *
 * @param array $draft_data Decoded JSON draft data
 * @return float Total absolute value of qty * standard_cost
 */
function get_inventory_adjustment_draft_amount($draft_data)
{
	$total = 0;
	if (!empty($draft_data['line_items'])) {
		foreach ($draft_data['line_items'] as $line) {
			$cost = isset($line['standard_cost']) ? $line['standard_cost'] : 0;
			$total += abs($line['quantity'] * $cost);
		}
	}
	return $total;
}

/**
 * Calculate total amount of an inventory transfer draft.
 *
 * @param array $draft_data Decoded JSON draft data
 * @return float Total absolute value of qty * standard_cost
 */
function get_inventory_transfer_draft_amount($draft_data)
{
	return get_inventory_adjustment_draft_amount($draft_data);
}

/**
 * Calculate total amount of a material request draft.
 *
 * @param array $draft_data Decoded JSON draft data
 * @return float
 */
function get_material_request_draft_amount($draft_data)
{
	if (isset($draft_data['total_estimated']))
		return (float)$draft_data['total_estimated'];

	$total = 0;
	if (!empty($draft_data['lines']) && is_array($draft_data['lines'])) {
		foreach ($draft_data['lines'] as $line) {
			$qty = isset($line['qty_requested']) ? (float)$line['qty_requested'] : 0;
			$cost = isset($line['unit_cost']) ? (float)$line['unit_cost'] : 0;
			$total += $qty * $cost;
		}
	}

	return $total;
}

// =====================================================
// DRAFT RENDERERS
// =====================================================

/**
 * Render an inventory adjustment draft for the approval review UI.
 *
 * @param array $draft_data Decoded JSON draft data
 * @return string HTML
 */
function render_inventory_adjustment_draft($draft_data)
{
	$html = '<table class="tablestyle" width="100%">';
	$html .= '<tr><th>' . _('Date') . '</th><td>' . htmlspecialchars($draft_data['date']) . '</td>';
	$html .= '<th>' . _('Reference') . '</th><td>' . htmlspecialchars($draft_data['reference']) . '</td></tr>';
	$html .= '<tr><th>' . _('Location') . '</th><td>' . htmlspecialchars($draft_data['location']) . '</td>';
	$html .= '<td colspan="2"></td></tr>';

	if (!empty($draft_data['memo_']))
		$html .= '<tr><th>' . _('Memo') . '</th><td colspan="3">' . htmlspecialchars($draft_data['memo_']) . '</td></tr>';

	$html .= '</table>';

	// Line items
	if (!empty($draft_data['line_items'])) {
		$html .= '<br><table class="tablestyle" width="100%">';
		$html .= '<tr><th>' . _('Item Code') . '</th><th>' . _('Description') . '</th>';
		$html .= '<th>' . _('Qty') . '</th><th>' . _('Std Cost') . '</th><th>' . _('Total') . '</th></tr>';

		$grand_total = 0;
		foreach ($draft_data['line_items'] as $line) {
			$cost = isset($line['standard_cost']) ? $line['standard_cost'] : 0;
			$line_total = $line['quantity'] * $cost;
			$grand_total += $line_total;

			$html .= '<tr><td>' . htmlspecialchars($line['stock_id']) . '</td>';
			$html .= '<td>' . htmlspecialchars(isset($line['item_description']) ? $line['item_description'] : '') . '</td>';
			$html .= '<td align="right">' . number_format($line['quantity'], 2) . '</td>';
			$html .= '<td align="right">' . number_format($cost, 2) . '</td>';
			$html .= '<td align="right">' . number_format($line_total, 2) . '</td></tr>';
		}

		$html .= '<tr><td colspan="4"><b>' . _('Total') . '</b></td>';
		$html .= '<td align="right"><b>' . number_format($grand_total, 2) . '</b></td></tr>';
		$html .= '</table>';
	}

	return $html;
}

/**
 * Render an inventory transfer draft for the approval review UI.
 *
 * @param array $draft_data Decoded JSON draft data
 * @return string HTML
 */
function render_inventory_transfer_draft($draft_data)
{
	$html = '<table class="tablestyle" width="100%">';
	$html .= '<tr><th>' . _('Date') . '</th><td>' . htmlspecialchars($draft_data['date']) . '</td>';
	$html .= '<th>' . _('Reference') . '</th><td>' . htmlspecialchars($draft_data['reference']) . '</td></tr>';
	$html .= '<tr><th>' . _('From Location') . '</th><td>' . htmlspecialchars($draft_data['from_location']) . '</td>';
	$html .= '<th>' . _('To Location') . '</th><td>' . htmlspecialchars($draft_data['to_location']) . '</td></tr>';

	if (!empty($draft_data['memo_']))
		$html .= '<tr><th>' . _('Memo') . '</th><td colspan="3">' . htmlspecialchars($draft_data['memo_']) . '</td></tr>';

	$html .= '</table>';

	// Line items
	if (!empty($draft_data['line_items'])) {
		$html .= '<br><table class="tablestyle" width="100%">';
		$html .= '<tr><th>' . _('Item Code') . '</th><th>' . _('Description') . '</th>';
		$html .= '<th>' . _('Qty') . '</th><th>' . _('Std Cost') . '</th><th>' . _('Total') . '</th></tr>';

		$grand_total = 0;
		foreach ($draft_data['line_items'] as $line) {
			$cost = isset($line['standard_cost']) ? $line['standard_cost'] : 0;
			$line_total = $line['quantity'] * $cost;
			$grand_total += $line_total;

			$html .= '<tr><td>' . htmlspecialchars($line['stock_id']) . '</td>';
			$html .= '<td>' . htmlspecialchars(isset($line['item_description']) ? $line['item_description'] : '') . '</td>';
			$html .= '<td align="right">' . number_format($line['quantity'], 2) . '</td>';
			$html .= '<td align="right">' . number_format($cost, 2) . '</td>';
			$html .= '<td align="right">' . number_format($line_total, 2) . '</td></tr>';
		}

		$html .= '<tr><td colspan="4"><b>' . _('Total') . '</b></td>';
		$html .= '<td align="right"><b>' . number_format($grand_total, 2) . '</b></td></tr>';
		$html .= '</table>';
	}

	return $html;
}

/**
 * Render a material request draft for the approval review UI.
 *
 * @param array $draft_data Decoded JSON draft data
 * @return string HTML
 */
function render_material_request_draft($draft_data)
{
	$type = isset($draft_data['request_type_label']) ? $draft_data['request_type_label'] : '';
	$status = isset($draft_data['status_label']) ? $draft_data['status_label'] : '';
	$request_no = isset($draft_data['request_no_formatted']) ? $draft_data['request_no_formatted'] : '';

	$html = '<table class="tablestyle" width="100%">';
	$html .= '<tr><th>' . _('Request') . '</th><td>' . htmlspecialchars($request_no) . '</td>';
	$html .= '<th>' . _('Type') . '</th><td>' . htmlspecialchars($type) . '</td></tr>';
	$html .= '<tr><th>' . _('Warehouse') . '</th><td>' . htmlspecialchars(isset($draft_data['warehouse_name']) ? $draft_data['warehouse_name'] : '') . '</td>';
	$html .= '<th>' . _('Request Date') . '</th><td>' . htmlspecialchars(isset($draft_data['request_date']) ? $draft_data['request_date'] : '') . '</td></tr>';
	$html .= '<tr><th>' . _('Required Date') . '</th><td>' . htmlspecialchars(isset($draft_data['required_date']) ? $draft_data['required_date'] : '') . '</td>';
	$html .= '<th>' . _('Status') . '</th><td>' . htmlspecialchars($status) . '</td></tr>';

	if (!empty($draft_data['reference']))
		$html .= '<tr><th>' . _('Reference') . '</th><td colspan="3">' . htmlspecialchars($draft_data['reference']) . '</td></tr>';

	if (!empty($draft_data['memo']))
		$html .= '<tr><th>' . _('Memo') . '</th><td colspan="3">' . htmlspecialchars($draft_data['memo']) . '</td></tr>';

	$html .= '</table>';

	if (!empty($draft_data['lines']) && is_array($draft_data['lines'])) {
		$html .= '<br><table class="tablestyle" width="100%">';
		$html .= '<tr><th>' . _('Item Code') . '</th><th>' . _('Description') . '</th><th>' . _('Qty Requested') . '</th><th>' . _('Unit') . '</th><th>' . _('Unit Cost') . '</th><th>' . _('Line Total') . '</th></tr>';

		$grand_total = 0;
		foreach ($draft_data['lines'] as $line) {
			$qty = isset($line['qty_requested']) ? (float)$line['qty_requested'] : 0;
			$cost = isset($line['unit_cost']) ? (float)$line['unit_cost'] : 0;
			$line_total = $qty * $cost;
			$grand_total += $line_total;

			$html .= '<tr><td>' . htmlspecialchars(isset($line['stock_id']) ? $line['stock_id'] : '') . '</td>';
			$html .= '<td>' . htmlspecialchars(isset($line['description']) ? $line['description'] : '') . '</td>';
			$html .= '<td align="right">' . number_format($qty, 2) . '</td>';
			$html .= '<td>' . htmlspecialchars(isset($line['units']) ? $line['units'] : '') . '</td>';
			$html .= '<td align="right">' . number_format($cost, 2) . '</td>';
			$html .= '<td align="right">' . number_format($line_total, 2) . '</td></tr>';
		}

		$html .= '<tr><td colspan="5"><b>' . _('Total') . '</b></td>';
		$html .= '<td align="right"><b>' . number_format($grand_total, 2) . '</b></td></tr>';
		$html .= '</table>';
	}

	return $html;
}
