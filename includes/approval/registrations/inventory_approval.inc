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
include_once($path_to_root . '/inventory/warehouse/includes/db/warehouse_transfers_db.inc');
include_once($path_to_root . '/inventory/warehouse/includes/db/warehouse_scrap_db.inc');
include_once($path_to_root . '/inventory/warehouse/includes/db/warehouse_counting_db.inc');

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

ApprovalWorkflow::registerTransactionType(ST_WH_TRANSFER_ORDER, array(
	'label'          => _('Transfer Order'),
	'application'    => 'stock',
	'executor'       => 'execute_approved_warehouse_transfer_order',
	'rejector'       => 'reject_warehouse_transfer_order_draft',
	'canceler'       => 'cancel_warehouse_transfer_order_draft',
	'amount_getter'  => 'get_warehouse_transfer_order_draft_amount',
	'draft_renderer' => 'render_warehouse_transfer_order_draft',
	'edit_url'       => 'inventory/warehouse/transfer_orders.php',
));

ApprovalWorkflow::registerTransactionType(ST_WH_SCRAP, array(
	'label'          => _('Scrap Entry'),
	'application'    => 'stock',
	'executor'       => 'execute_approved_warehouse_scrap_entry',
	'rejector'       => 'reject_warehouse_scrap_entry_draft',
	'canceler'       => 'cancel_warehouse_scrap_entry_draft',
	'amount_getter'  => 'get_warehouse_scrap_draft_amount',
	'draft_renderer' => 'render_warehouse_scrap_draft',
	'edit_url'       => 'inventory/warehouse/scrap.php',
));

ApprovalWorkflow::registerTransactionType(ST_WH_CYCLE_COUNT, array(
	'label'          => _('Cycle Count Adjustment'),
	'application'    => 'stock',
	'executor'       => 'execute_approved_warehouse_cycle_count',
	'rejector'       => 'reject_warehouse_cycle_count_draft',
	'canceler'       => 'cancel_warehouse_cycle_count_draft',
	'amount_getter'  => 'get_warehouse_cycle_count_draft_amount',
	'draft_renderer' => 'render_warehouse_cycle_count_draft',
	'edit_url'       => 'inventory/warehouse/cycle_counts.php',
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

/**
 * Execute an approved Transfer Order draft.
 *
 * @param array $draft_data
 * @param int   $reserved_trans_no
 * @param int   $draft_id
 * @return int|false
 */
function execute_approved_warehouse_transfer_order($draft_data, $reserved_trans_no, $draft_id)
{
	$transfer_id = isset($draft_data['transfer_id']) ? (int)$draft_data['transfer_id'] : 0;
	if ($transfer_id <= 0)
		return false;

	if (!approve_transfer_order($transfer_id))
		return false;

	return $transfer_id;
}

/**
 * Sync rejected Transfer Order draft to business state.
 *
 * @param array  $draft_data
 * @param int    $draft_id
 * @param string $comments
 * @return bool
 */
function reject_warehouse_transfer_order_draft($draft_data, $draft_id, $comments)
{
	$transfer_id = isset($draft_data['transfer_id']) ? (int)$draft_data['transfer_id'] : 0;
	if ($transfer_id <= 0)
		return false;

	return reject_transfer_order($transfer_id);
}

/**
 * Sync cancelled Transfer Order draft to business state.
 *
 * @param array  $draft_data
 * @param int    $draft_id
 * @param string $comments
 * @return bool
 */
function cancel_warehouse_transfer_order_draft($draft_data, $draft_id, $comments)
{
	$transfer_id = isset($draft_data['transfer_id']) ? (int)$draft_data['transfer_id'] : 0;
	if ($transfer_id <= 0)
		return false;

	return cancel_transfer_order($transfer_id);
}

/**
 * Execute an approved Scrap Entry draft.
 *
 * @param array $draft_data
 * @param int   $reserved_trans_no
 * @param int   $draft_id
 * @return int|false
 */
function execute_approved_warehouse_scrap_entry($draft_data, $reserved_trans_no, $draft_id)
{
	$stock_id = isset($draft_data['stock_id']) ? $draft_data['stock_id'] : '';
	$qty = isset($draft_data['qty']) ? (float)$draft_data['qty'] : 0;
	$loc_code = isset($draft_data['loc_code']) ? $draft_data['loc_code'] : '';
	$date_ = isset($draft_data['scrap_date']) ? $draft_data['scrap_date'] : '';
	$reason_code = isset($draft_data['reason_code']) ? $draft_data['reason_code'] : '';

	if ($stock_id === '' || $qty <= 0 || $loc_code === '' || $date_ === '' || $reason_code === '')
		return false;

	return add_scrap_entry(
		$stock_id,
		$qty,
		$loc_code,
		$date_,
		$reason_code,
		isset($draft_data['reason_detail']) ? $draft_data['reason_detail'] : null,
		!empty($draft_data['from_bin_id']) ? (int)$draft_data['from_bin_id'] : null,
		!empty($draft_data['scrap_loc_id']) ? (int)$draft_data['scrap_loc_id'] : null,
		!empty($draft_data['serial_id']) ? (int)$draft_data['serial_id'] : null,
		!empty($draft_data['batch_id']) ? (int)$draft_data['batch_id'] : null,
		isset($draft_data['memo']) ? $draft_data['memo'] : null
	);
}

/**
 * Rejecting a scrap draft does not require state rollback.
 *
 * @param array  $draft_data
 * @param int    $draft_id
 * @param string $comments
 * @return bool
 */
function reject_warehouse_scrap_entry_draft($draft_data, $draft_id, $comments)
{
	return true;
}

/**
 * Cancelling a scrap draft does not require state rollback.
 *
 * @param array  $draft_data
 * @param int    $draft_id
 * @param string $comments
 * @return bool
 */
function cancel_warehouse_scrap_entry_draft($draft_data, $draft_id, $comments)
{
	return true;
}

/**
 * Execute an approved Cycle Count draft.
 *
 * @param array $draft_data
 * @param int   $reserved_trans_no
 * @param int   $draft_id
 * @return int|false
 */
function execute_approved_warehouse_cycle_count($draft_data, $reserved_trans_no, $draft_id)
{
	$count_id = isset($draft_data['count_id']) ? (int)$draft_data['count_id'] : 0;
	if ($count_id <= 0)
		return false;

	if (!approve_cycle_count($count_id))
		return false;

	return $count_id;
}

/**
 * Sync rejected Cycle Count draft to business state.
 *
 * @param array  $draft_data
 * @param int    $draft_id
 * @param string $comments
 * @return bool
 */
function reject_warehouse_cycle_count_draft($draft_data, $draft_id, $comments)
{
	$count_id = isset($draft_data['count_id']) ? (int)$draft_data['count_id'] : 0;
	if ($count_id <= 0)
		return false;

	return reject_cycle_count($count_id);
}

/**
 * Sync cancelled Cycle Count draft to business state.
 *
 * @param array  $draft_data
 * @param int    $draft_id
 * @param string $comments
 * @return bool
 */
function cancel_warehouse_cycle_count_draft($draft_data, $draft_id, $comments)
{
	$count_id = isset($draft_data['count_id']) ? (int)$draft_data['count_id'] : 0;
	if ($count_id <= 0)
		return false;

	return cancel_cycle_count_submission($count_id);
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

/**
 * Calculate total amount of a transfer order draft.
 *
 * @param array $draft_data
 * @return float
 */
function get_warehouse_transfer_order_draft_amount($draft_data)
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

/**
 * Calculate total amount of a scrap draft.
 *
 * @param array $draft_data
 * @return float
 */
function get_warehouse_scrap_draft_amount($draft_data)
{
	if (isset($draft_data['total_cost']))
		return (float)$draft_data['total_cost'];

	$qty = isset($draft_data['qty']) ? (float)$draft_data['qty'] : 0;
	$cost = isset($draft_data['unit_cost']) ? (float)$draft_data['unit_cost'] : 0;
	return $qty * $cost;
}

/**
 * Calculate total amount of a cycle count draft.
 *
 * @param array $draft_data
 * @return float
 */
function get_warehouse_cycle_count_draft_amount($draft_data)
{
	if (isset($draft_data['total_variance_value']))
		return (float)$draft_data['total_variance_value'];

	$total = 0;
	if (!empty($draft_data['lines']) && is_array($draft_data['lines'])) {
		foreach ($draft_data['lines'] as $line) {
			if (isset($line['variance_value']))
				$total += abs((float)$line['variance_value']);
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

/**
 * Render transfer order draft details.
 *
 * @param array $draft_data
 * @return string
 */
function render_warehouse_transfer_order_draft($draft_data)
{
	$html = '<table class="tablestyle" width="100%">';
	$html .= '<tr><th>' . _('Transfer') . '</th><td>TO-' . str_pad((string)(isset($draft_data['transfer_no']) ? $draft_data['transfer_no'] : ''), 4, '0', STR_PAD_LEFT) . '</td>';
	$html .= '<th>' . _('Status') . '</th><td>' . htmlspecialchars(isset($draft_data['status_label']) ? $draft_data['status_label'] : '') . '</td></tr>';
	$html .= '<tr><th>' . _('From') . '</th><td>' . htmlspecialchars(isset($draft_data['from_loc_name']) ? $draft_data['from_loc_name'] : '') . '</td>';
	$html .= '<th>' . _('To') . '</th><td>' . htmlspecialchars(isset($draft_data['to_loc_name']) ? $draft_data['to_loc_name'] : '') . '</td></tr>';
	$html .= '<tr><th>' . _('Request Date') . '</th><td>' . htmlspecialchars(isset($draft_data['request_date']) ? $draft_data['request_date'] : '') . '</td>';
	$html .= '<th>' . _('Reference') . '</th><td>' . htmlspecialchars(isset($draft_data['reference']) ? $draft_data['reference'] : '') . '</td></tr>';
	if (!empty($draft_data['memo']))
		$html .= '<tr><th>' . _('Memo') . '</th><td colspan="3">' . htmlspecialchars($draft_data['memo']) . '</td></tr>';
	$html .= '</table>';

	if (!empty($draft_data['lines']) && is_array($draft_data['lines'])) {
		$html .= '<br><table class="tablestyle" width="100%">';
		$html .= '<tr><th>' . _('Item') . '</th><th>' . _('Description') . '</th><th>' . _('Qty') . '</th><th>' . _('Unit Cost') . '</th><th>' . _('Line Total') . '</th></tr>';
		$total = 0;
		foreach ($draft_data['lines'] as $line) {
			$line_total = isset($line['line_total']) ? (float)$line['line_total'] : 0;
			$total += $line_total;
			$html .= '<tr><td>' . htmlspecialchars(isset($line['stock_id']) ? $line['stock_id'] : '') . '</td>';
			$html .= '<td>' . htmlspecialchars(isset($line['description']) ? $line['description'] : '') . '</td>';
			$html .= '<td align="right">' . number_format((float)(isset($line['qty_requested']) ? $line['qty_requested'] : 0), 2) . '</td>';
			$html .= '<td align="right">' . number_format((float)(isset($line['unit_cost']) ? $line['unit_cost'] : 0), 2) . '</td>';
			$html .= '<td align="right">' . number_format($line_total, 2) . '</td></tr>';
		}
		$html .= '<tr><td colspan="4"><b>' . _('Total') . '</b></td><td align="right"><b>' . number_format($total, 2) . '</b></td></tr>';
		$html .= '</table>';
	}

	return $html;
}

/**
 * Render scrap draft details.
 *
 * @param array $draft_data
 * @return string
 */
function render_warehouse_scrap_draft($draft_data)
{
	$html = '<table class="tablestyle" width="100%">';
	$html .= '<tr><th>' . _('Item') . '</th><td>' . htmlspecialchars(isset($draft_data['stock_id']) ? $draft_data['stock_id'] : '') . '</td>';
	$html .= '<th>' . _('Description') . '</th><td>' . htmlspecialchars(isset($draft_data['item_description']) ? $draft_data['item_description'] : '') . '</td></tr>';
	$html .= '<tr><th>' . _('Date') . '</th><td>' . htmlspecialchars(isset($draft_data['scrap_date']) ? $draft_data['scrap_date'] : '') . '</td>';
	$html .= '<th>' . _('Warehouse') . '</th><td>' . htmlspecialchars(isset($draft_data['loc_code']) ? $draft_data['loc_code'] : '') . '</td></tr>';
	$html .= '<tr><th>' . _('Qty') . '</th><td>' . number_format((float)(isset($draft_data['qty']) ? $draft_data['qty'] : 0), 2) . '</td>';
	$html .= '<th>' . _('Reason') . '</th><td>' . htmlspecialchars(isset($draft_data['reason_label']) ? $draft_data['reason_label'] : '') . '</td></tr>';
	$html .= '<tr><th>' . _('Unit Cost') . '</th><td>' . number_format((float)(isset($draft_data['unit_cost']) ? $draft_data['unit_cost'] : 0), 2) . '</td>';
	$html .= '<th>' . _('Total Cost') . '</th><td><b>' . number_format((float)(isset($draft_data['total_cost']) ? $draft_data['total_cost'] : 0), 2) . '</b></td></tr>';
	if (!empty($draft_data['memo']))
		$html .= '<tr><th>' . _('Memo') . '</th><td colspan="3">' . htmlspecialchars($draft_data['memo']) . '</td></tr>';
	$html .= '</table>';

	return $html;
}

/**
 * Render cycle count draft details.
 *
 * @param array $draft_data
 * @return string
 */
function render_warehouse_cycle_count_draft($draft_data)
{
	$html = '<table class="tablestyle" width="100%">';
	$html .= '<tr><th>' . _('Count ID') . '</th><td>#' . (int)(isset($draft_data['count_id']) ? $draft_data['count_id'] : 0) . '</td>';
	$html .= '<th>' . _('Status') . '</th><td>' . htmlspecialchars(isset($draft_data['status_label']) ? $draft_data['status_label'] : '') . '</td></tr>';
	$html .= '<tr><th>' . _('Warehouse') . '</th><td>' . htmlspecialchars(isset($draft_data['warehouse_name']) ? $draft_data['warehouse_name'] : '') . '</td>';
	$html .= '<th>' . _('Count Date') . '</th><td>' . htmlspecialchars(isset($draft_data['count_date']) ? $draft_data['count_date'] : '') . '</td></tr>';
	$html .= '<tr><th>' . _('Variance Lines') . '</th><td>' . (int)(isset($draft_data['variance_line_count']) ? $draft_data['variance_line_count'] : 0) . '</td>';
	$html .= '<th>' . _('Variance Value') . '</th><td><b>' . number_format((float)(isset($draft_data['total_variance_value']) ? $draft_data['total_variance_value'] : 0), 2) . '</b></td></tr>';
	if (!empty($draft_data['memo']))
		$html .= '<tr><th>' . _('Memo') . '</th><td colspan="3">' . htmlspecialchars($draft_data['memo']) . '</td></tr>';
	$html .= '</table>';

	if (!empty($draft_data['lines']) && is_array($draft_data['lines'])) {
		$html .= '<br><table class="tablestyle" width="100%">';
		$html .= '<tr><th>' . _('Bin') . '</th><th>' . _('Item') . '</th><th>' . _('System') . '</th><th>' . _('Counted') . '</th><th>' . _('Variance') . '</th><th>' . _('Value') . '</th></tr>';
		foreach ($draft_data['lines'] as $line) {
			$html .= '<tr><td>' . htmlspecialchars(isset($line['bin_code']) ? $line['bin_code'] : '') . '</td>';
			$html .= '<td>' . htmlspecialchars(isset($line['stock_id']) ? $line['stock_id'] : '') . ' - ' . htmlspecialchars(isset($line['item_description']) ? $line['item_description'] : '') . '</td>';
			$html .= '<td align="right">' . number_format((float)(isset($line['system_qty']) ? $line['system_qty'] : 0), 2) . '</td>';
			$html .= '<td align="right">' . number_format((float)(isset($line['counted_qty']) ? $line['counted_qty'] : 0), 2) . '</td>';
			$html .= '<td align="right">' . number_format((float)(isset($line['variance_qty']) ? $line['variance_qty'] : 0), 2) . '</td>';
			$html .= '<td align="right">' . number_format((float)(isset($line['variance_value']) ? $line['variance_value'] : 0), 2) . '</td></tr>';
		}
		$html .= '</table>';
	}

	return $html;
}
