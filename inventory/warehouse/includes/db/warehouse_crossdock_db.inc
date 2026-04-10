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

/**
 * Cross-Docking & Drop-Shipping Database Functions.
 *
 * Cross-dock: receipt items matched against pending outbound demand are
 * routed directly from receiving dock to shipping dock, bypassing putaway.
 *
 * Drop-ship: PO is created with the customer's delivery address; goods
 * go from supplier to customer with no warehouse stock movement.
 *
 * @package NotrinosERP
 * @subpackage Inventory/Warehouse
 */

// =====================================================================
// CROSS-DOCK: Candidate Detection
// =====================================================================

/**
 * Find pending sales order lines that match a received item and location.
 *
 * A "cross-dock candidate" is a pending SO line where:
 *   - item is cross_dock_eligible
 *   - warehouse location has cross_dock_enabled
 *   - SO line has outstanding qty (quantity - qty_sent > 0)
 *   - SO is not on hold / completed
 *
 * @param string $stock_id  Item code
 * @param string $loc_code  Warehouse location code
 * @param float  $available_qty  Received qty available for cross-dock
 * @return array Array of candidate rows (order_no, stk_code, description,
 *               outstanding_qty, delivery_date, debtor_no, name, deliver_to)
 */
function get_cross_dock_candidates($stock_id, $loc_code, $available_qty = 0)
{
	$sql = "SELECT sod.id AS so_line_id, sod.order_no, sod.stk_code,
			sod.description, (sod.quantity - sod.qty_sent) AS outstanding_qty,
			so.delivery_date, so.debtor_no, d.name AS customer_name,
			so.deliver_to, so.delivery_address, so.from_stk_loc,
			sod.drop_ship, sod.drop_ship_supplier_id
		FROM " . TB_PREF . "sales_order_details sod
		INNER JOIN " . TB_PREF . "sales_orders so ON sod.order_no = so.order_no AND so.trans_type = " . ST_SALESORDER . "
		INNER JOIN " . TB_PREF . "debtors_master d ON so.debtor_no = d.debtor_no
		INNER JOIN " . TB_PREF . "stock_master sm ON sod.stk_code = sm.stock_id
		WHERE sod.stk_code = " . db_escape($stock_id) . "
			AND so.from_stk_loc = " . db_escape($loc_code) . "
			AND (sod.quantity - sod.qty_sent) > 0
			AND sm.cross_dock_eligible = 1
			AND sod.drop_ship = 0
		ORDER BY so.delivery_date ASC, sod.order_no ASC";

	return db_query($sql, 'could not get cross-dock candidates');
}

/**
 * Check if a location has cross-docking enabled.
 *
 * @param string $loc_code Location code
 * @return bool
 */
function is_cross_dock_enabled($loc_code)
{
	$sql = "SELECT cross_dock_enabled FROM " . TB_PREF . "locations
		WHERE loc_code = " . db_escape($loc_code);
	$result = db_query($sql, 'could not check cross-dock status');
	$row = db_fetch($result);
	return $row && $row['cross_dock_enabled'];
}

/**
 * Check if an item is eligible for cross-docking.
 *
 * @param string $stock_id Item code
 * @return bool
 */
function is_item_cross_dock_eligible($stock_id)
{
	$sql = "SELECT cross_dock_eligible FROM " . TB_PREF . "stock_master
		WHERE stock_id = " . db_escape($stock_id);
	$result = db_query($sql, 'could not check item cross-dock eligibility');
	$row = db_fetch($result);
	return $row && $row['cross_dock_eligible'];
}

/**
 * Allocate received qty to cross-dock candidates (FIFO by delivery date).
 *
 * Returns an array of allocations, each containing the SO line info
 * and the allocated quantity.
 *
 * @param string $stock_id      Item code
 * @param string $loc_code      Location code
 * @param float  $received_qty  Total received quantity available
 * @return array Allocations [{so_line_id, order_no, qty_allocated, customer_name, deliver_to, delivery_address}]
 */
function allocate_cross_dock_qty($stock_id, $loc_code, $received_qty)
{
	$allocations = array();
	$remaining = $received_qty;

	$candidates = get_cross_dock_candidates($stock_id, $loc_code, $received_qty);
	while ($row = db_fetch($candidates)) {
		if ($remaining <= 0) break;
		$alloc_qty = min($remaining, (float)$row['outstanding_qty']);
		$allocations[] = array(
			'so_line_id'       => (int)$row['so_line_id'],
			'order_no'         => (int)$row['order_no'],
			'qty_allocated'    => $alloc_qty,
			'outstanding_qty'  => (float)$row['outstanding_qty'],
			'customer_name'    => $row['customer_name'],
			'deliver_to'       => $row['deliver_to'],
			'delivery_address' => $row['delivery_address'],
			'delivery_date'    => $row['delivery_date'],
			'debtor_no'        => (int)$row['debtor_no'],
		);
		$remaining -= $alloc_qty;
	}

	return $allocations;
}

// =====================================================================
// CROSS-DOCK: Operation Creation
// =====================================================================

/**
 * Create a cross-dock WMS operation chain (receipt → ship) for allocated items.
 *
 * Uses the 'crossdock' route template if available, or creates receipt+ship
 * operations directly.
 *
 * @param int    $grn_no       GRN number
 * @param string $loc_code     Warehouse location code
 * @param array  $allocations  From allocate_cross_dock_qty()
 * @param array  $item_info    Item detail: [stock_id, std_cost, serial_ids, batch_id]
 * @return array Result with 'success', 'operations', 'message'
 */
function create_cross_dock_operations($grn_no, $loc_code, $allocations, $item_info)
{
	$result = array('success' => false, 'operations' => array(), 'message' => '');
	if (empty($allocations)) {
		$result['message'] = _('No allocations for cross-dock.');
		return $result;
	}

	$stock_id = $item_info['stock_id'];
	$std_cost = isset($item_info['std_cost']) ? $item_info['std_cost'] : 0;
	$serial_ids = isset($item_info['serial_ids']) ? $item_info['serial_ids'] : array();
	$batch_id = isset($item_info['batch_id']) ? $item_info['batch_id'] : null;

	// Try route-based creation
	if (function_exists('resolve_route')) {
		$route = resolve_route($loc_code, 'crossdock');
		if ($route) {
			$total_qty = 0;
			foreach ($allocations as $a) {
				$total_qty += $a['qty_allocated'];
			}
			$items = array(array(
				'stock_id'   => $stock_id,
				'qty'        => $total_qty,
				'std_cost'   => $std_cost,
				'serial_ids' => $serial_ids,
				'batch_id'   => $batch_id,
			));
			return create_operations_from_route($route, ST_SUPPRECEIVE, $grn_no, $items, $loc_code);
		}
	}

	// Fallback: manual receipt → ship operations
	$now = date('Y-m-d H:i:s');
	$dock_zone_id = find_zone_by_type($loc_code, 'dock');
	$staging_zone_id = find_zone_by_type($loc_code, 'staging');
	$ship_to = $dock_zone_id ? $dock_zone_id : $staging_zone_id;

	// Receipt operation
	$receipt_op = add_wh_operation(
		'receipt',
		'ready',
		ST_SUPPRECEIVE,
		$grn_no,
		null,
		null,       // from external
		$ship_to,   // direct to dock/staging for cross-dock
		$now,
		null,
		8,          // higher priority for cross-dock
		null,
		sprintf(_('Cross-dock receipt for GRN #%d'), $grn_no)
	);

	// Ship operation — linked to receipt
	$ship_op = add_wh_operation(
		'ship',
		'draft',
		ST_SUPPRECEIVE,
		$grn_no,
		$receipt_op,
		$ship_to,
		null, // out
		$now,
		null,
		8,
		null,
		sprintf(_('Cross-dock ship for GRN #%d'), $grn_no)
	);

	if ($receipt_op && $ship_op)
		link_wh_operations($receipt_op, $ship_op);

	// Add operation lines for each allocation
	foreach ($allocations as $alloc) {
		$qty = $alloc['qty_allocated'];

		// Receipt line
		add_wh_operation_line($receipt_op, $stock_id, $qty, 0,
			null, $ship_to, $batch_id, null, null, $std_cost);

		// Ship line
		add_wh_operation_line($ship_op, $stock_id, $qty, 0,
			$ship_to, null, $batch_id, null, null, $std_cost);
	}

	// Auto-complete receipt (goods arrived)
	complete_wh_operation($receipt_op);

	$result['success'] = true;
	$result['operations'] = array($receipt_op, $ship_op);
	$result['message'] = sprintf(_('Cross-dock operations created for GRN #%d (%d allocation(s)).'),
		$grn_no, count($allocations));
	return $result;
}

/**
 * Link a PO line to a SO line for cross-dock tracking.
 *
 * @param int $po_detail_id  PO detail row id
 * @param int $so_no         Sales order number
 * @param int $so_line_id    Sales order detail line id
 */
function link_po_line_to_crossdock_so($po_detail_id, $so_no, $so_line_id)
{
	$sql = "UPDATE " . TB_PREF . "purch_order_details
		SET cross_dock_so_no = " . (int)$so_no . ",
			cross_dock_so_line = " . (int)$so_line_id . "
		WHERE po_detail_item = " . (int)$po_detail_id;
	db_query($sql, 'could not link PO line to cross-dock SO');
}

// =====================================================================
// DROP-SHIP: Functions
// =====================================================================

/**
 * Check if an item is eligible for drop-shipping.
 *
 * @param string $stock_id Item code
 * @return bool
 */
function is_item_drop_ship_eligible($stock_id)
{
	$sql = "SELECT drop_ship_eligible FROM " . TB_PREF . "stock_master
		WHERE stock_id = " . db_escape($stock_id);
	$result = db_query($sql, 'could not check drop-ship eligibility');
	$row = db_fetch($result);
	return $row && $row['drop_ship_eligible'];
}

/**
 * Get the preferred supplier for drop-shipping an item.
 *
 * Returns the default supplier from purchasing_data (ordering priority).
 *
 * @param string $stock_id Item code
 * @return array|false Supplier row (supplier_id, supp_name, price, etc.) or false
 */
function get_drop_ship_supplier($stock_id)
{
	$sql = "SELECT pd.supplier_id, s.supp_name, pd.price, pd.suppliers_uom,
			pd.conversion_factor, pd.supplier_description
		FROM " . TB_PREF . "purch_data pd
		INNER JOIN " . TB_PREF . "suppliers s ON pd.supplier_id = s.supplier_id
		WHERE pd.stock_id = " . db_escape($stock_id) . "
		ORDER BY pd.supplier_id ASC LIMIT 1";
	$result = db_query($sql, 'could not get drop-ship supplier');
	$row = db_fetch($result);
	return $row ? $row : false;
}

/**
 * Mark a SO line as drop-ship and record the supplier/PO reference.
 *
 * @param int $so_line_id     Sales order detail id
 * @param int $supplier_id    Supplier used for drop-ship
 * @param int $po_no          Purchase order number created
 */
function mark_so_line_drop_ship($so_line_id, $supplier_id, $po_no)
{
	$sql = "UPDATE " . TB_PREF . "sales_order_details
		SET drop_ship = 1,
			drop_ship_supplier_id = " . (int)$supplier_id . ",
			drop_ship_po_no = " . (int)$po_no . "
		WHERE id = " . (int)$so_line_id;
	db_query($sql, 'could not mark SO line as drop-ship');
}

/**
 * Mark a PO as a drop-ship purchase order tied to a sales order.
 *
 * @param int $po_no   Purchase order number
 * @param int $so_no   Sales order number
 */
function mark_po_as_drop_ship($po_no, $so_no)
{
	$sql = "UPDATE " . TB_PREF . "purch_orders
		SET drop_ship = 1,
			drop_ship_so_no = " . (int)$so_no . "
		WHERE order_no = " . (int)$po_no;
	db_query($sql, 'could not mark PO as drop-ship');
}

/**
 * Create a drop-ship purchase order for a sales order line.
 *
 * Creates a PO with the customer's delivery address. The item goes
 * directly from the supplier to the customer — no warehouse stock moves.
 *
 * @param int    $so_no        Sales order number
 * @param int    $so_line_id   Sales order detail row id
 * @param string $stock_id     Item code
 * @param float  $qty          Quantity
 * @param int    $supplier_id  Supplier id (if null, uses default supplier)
 * @param string $delivery_address Customer delivery address
 * @param string $deliver_to       Customer company name
 * @param string $loc_code         Warehouse location code (for reference)
 * @return array Result with 'success', 'po_no', 'message'
 */
function create_drop_ship_po($so_no, $so_line_id, $stock_id, $qty, $supplier_id = null,
	$delivery_address = '', $deliver_to = '', $loc_code = '')
{
	$result = array('success' => false, 'po_no' => 0, 'message' => '');

	// Get supplier
	if (!$supplier_id) {
		$supplier_info = get_drop_ship_supplier($stock_id);
		if (!$supplier_info) {
			$result['message'] = sprintf(_('No supplier found for item %s.'), $stock_id);
			return $result;
		}
		$supplier_id = (int)$supplier_info['supplier_id'];
	}

	// Get supplier details
	$supplier = get_supplier($supplier_id);
	if (!$supplier) {
		$result['message'] = _('Invalid supplier.');
		return $result;
	}

	// Get item purchasing data
	$purch_data = get_items_purchasing_data($supplier_id, $stock_id);
	$purchase_price = $purch_data ? (float)$purch_data['price'] : 0;
	$supplier_uom = $purch_data ? $purch_data['suppliers_uom'] : '';
	$conversion = $purch_data ? (float)$purch_data['conversion_factor'] : 1;
	$supplier_desc = $purch_data ? $purch_data['supplier_description'] : '';

	// Reference with drop-ship identifier
	$reference = sprintf('DS-%d-%s', $so_no, $stock_id);

	// Build minimal PO object for add_po()
	$po = new purch_order;
	$po->supplier_id = $supplier_id;
	$po->orig_order_date = Today();
	$po->delivery_date = Today();
	$po->reference = $reference;
	$po->requisition_no = '';
	$po->Comments = sprintf(_('Drop-ship for SO#%d — ship to: %s'), $so_no, $deliver_to);
	$po->Location = $loc_code;
	$po->delivery_address = $delivery_address;
	$po->curr_code = $supplier['curr_code'];
	$po->supplier_name = $supplier['supp_name'];
	$po->tax_group_id = $supplier['tax_group_id'];

	// Add PO line
	$po->add_to_order(
		0,                 // line_no
		$stock_id,
		$qty,
		'',                // item description (will use default)
		$purchase_price,
		$supplier_uom,
		Today(),           // req_del_date
		0,                 // qty_received
		0                  // qty_invoiced
	);

	begin_transaction();

	$po_no = add_po($po);
	if ($po_no) {
		mark_po_as_drop_ship($po_no, $so_no);
		mark_so_line_drop_ship($so_line_id, $supplier_id, $po_no);

		$result['success'] = true;
		$result['po_no'] = $po_no;
		$result['message'] = sprintf(_('Drop-ship PO #%d created for SO #%d, item %s.'),
			$po_no, $so_no, $stock_id);
	} else {
		$result['message'] = _('Failed to create purchase order.');
	}

	commit_transaction();
	return $result;
}

// =====================================================================
// INQUIRY: Cross-dock & Drop-ship History
// =====================================================================

/**
 * Get all cross-dock operations history.
 *
 * @param string|null $from_date  SQL date range start (null = all)
 * @param string|null $to_date    SQL date range end
 * @param string|null $loc_code   Filter by location
 * @param string|null $stock_id   Filter by item
 * @return resource DB result
 */
function get_cross_dock_history($from_date = null, $to_date = null, $loc_code = null, $stock_id = null)
{
	$sql = "SELECT op.op_id, op.op_type, op.op_status, op.source_doc_no,
			op.created_at, op.completed_at, op.memo,
			ol.stock_id, ol.qty_planned, ol.qty_done,
			sm.description AS item_description
		FROM " . TB_PREF . "wh_operations op
		INNER JOIN " . TB_PREF . "wh_operation_lines ol ON op.op_id = ol.op_id
		INNER JOIN " . TB_PREF . "stock_master sm ON ol.stock_id = sm.stock_id
		WHERE op.memo LIKE '%Cross-dock%'";

	if ($from_date)
		$sql .= " AND op.created_at >= " . db_escape($from_date);
	if ($to_date)
		$sql .= " AND op.created_at <= " . db_escape($to_date . ' 23:59:59');
	if ($loc_code)
		$sql .= " AND (ol.from_loc_id IN (SELECT loc_id FROM " . TB_PREF
			. "wh_locations WHERE warehouse_loc_code = " . db_escape($loc_code) . ")
			OR ol.to_loc_id IN (SELECT loc_id FROM " . TB_PREF
			. "wh_locations WHERE warehouse_loc_code = " . db_escape($loc_code) . "))";
	if ($stock_id)
		$sql .= " AND ol.stock_id = " . db_escape($stock_id);

	$sql .= " ORDER BY op.created_at DESC";
	return db_query($sql, 'could not get cross-dock history');
}

/**
 * Get all drop-ship purchase orders.
 *
 * @param string|null $from_date  SQL date range start
 * @param string|null $to_date    SQL date range end
 * @param int|null    $supplier_id  Filter by supplier
 * @return resource DB result
 */
function get_drop_ship_orders($from_date = null, $to_date = null, $supplier_id = null)
{
	$sql = "SELECT po.order_no, po.supplier_id, s.supp_name, po.ord_date,
			po.delivery_address, po.drop_ship_so_no,
			po.comments, po.total,
			GROUP_CONCAT(DISTINCT pod.item_code SEPARATOR ', ') AS items,
			SUM(pod.quantity_ordered) AS total_qty,
			SUM(pod.quantity_received) AS total_received
		FROM " . TB_PREF . "purch_orders po
		INNER JOIN " . TB_PREF . "suppliers s ON po.supplier_id = s.supplier_id
		INNER JOIN " . TB_PREF . "purch_order_details pod ON po.order_no = pod.order_no
		WHERE po.drop_ship = 1";

	if ($from_date)
		$sql .= " AND po.ord_date >= " . db_escape($from_date);
	if ($to_date)
		$sql .= " AND po.ord_date <= " . db_escape($to_date);
	if ($supplier_id)
		$sql .= " AND po.supplier_id = " . (int)$supplier_id;

	$sql .= " GROUP BY po.order_no ORDER BY po.ord_date DESC";
	return db_query($sql, 'could not get drop-ship orders');
}

/**
 * Get drop-ship SO lines (lines on sales orders that are drop-shipped).
 *
 * @param int|null $so_no Filter by specific SO
 * @return resource DB result
 */
function get_drop_ship_so_lines($so_no = null)
{
	$sql = "SELECT sod.id, sod.order_no, sod.stk_code, sod.description,
			sod.quantity, sod.qty_sent, sod.drop_ship_supplier_id,
			sod.drop_ship_po_no, s.supp_name,
			so.debtor_no, d.name AS customer_name, so.deliver_to
		FROM " . TB_PREF . "sales_order_details sod
		INNER JOIN " . TB_PREF . "sales_orders so ON sod.order_no = so.order_no
			AND so.trans_type = " . ST_SALESORDER . "
		INNER JOIN " . TB_PREF . "debtors_master d ON so.debtor_no = d.debtor_no
		LEFT JOIN " . TB_PREF . "suppliers s ON sod.drop_ship_supplier_id = s.supplier_id
		WHERE sod.drop_ship = 1";

	if ($so_no)
		$sql .= " AND sod.order_no = " . (int)$so_no;

	$sql .= " ORDER BY sod.order_no DESC, sod.id ASC";
	return db_query($sql, 'could not get drop-ship SO lines');
}

// =====================================================================
// CONFIGURATION: Item & Location Flags
// =====================================================================

/**
 * Update item cross-dock and drop-ship eligibility flags.
 *
 * @param string $stock_id            Item code
 * @param bool   $cross_dock_eligible Cross-dock eligible flag
 * @param bool   $drop_ship_eligible  Drop-ship eligible flag
 */
function update_item_crossdock_dropship_flags($stock_id, $cross_dock_eligible, $drop_ship_eligible)
{
	$sql = "UPDATE " . TB_PREF . "stock_master
		SET cross_dock_eligible = " . ($cross_dock_eligible ? 1 : 0)
		. ", drop_ship_eligible = " . ($drop_ship_eligible ? 1 : 0)
		. " WHERE stock_id = " . db_escape($stock_id);
	db_query($sql, 'could not update item cross-dock/drop-ship flags');
}

/**
 * Update location cross-dock enabled flag.
 *
 * @param string $loc_code         Location code
 * @param bool   $cross_dock_enabled  Enable/disable cross-dock
 */
function update_location_cross_dock_flag($loc_code, $cross_dock_enabled)
{
	$sql = "UPDATE " . TB_PREF . "locations
		SET cross_dock_enabled = " . ($cross_dock_enabled ? 1 : 0)
		. " WHERE loc_code = " . db_escape($loc_code);
	db_query($sql, 'could not update location cross-dock flag');
}

/**
 * Get items eligible for cross-dock or drop-ship.
 *
 * @param string $flag  'cross_dock' or 'drop_ship'
 * @return resource DB result
 */
function get_eligible_items($flag = 'cross_dock')
{
	$col = ($flag === 'drop_ship') ? 'drop_ship_eligible' : 'cross_dock_eligible';
	$sql = "SELECT sm.stock_id, sm.description, sm.category_id, sc.description AS category,
			sm.drop_ship_eligible, sm.cross_dock_eligible
		FROM " . TB_PREF . "stock_master sm
		LEFT JOIN " . TB_PREF . "stock_category sc ON sm.category_id = sc.category_id
		WHERE sm." . $col . " = 1
		ORDER BY sm.stock_id";
	return db_query($sql, 'could not get eligible items');
}

/**
 * Get locations with cross-dock enabled.
 *
 * @return resource DB result
 */
function get_cross_dock_locations()
{
	$sql = "SELECT loc_code, location_name, cross_dock_enabled
		FROM " . TB_PREF . "locations
		WHERE cross_dock_enabled = 1 AND inactive = 0
		ORDER BY location_name";
	return db_query($sql, 'could not get cross-dock locations');
}

// =====================================================================
// BADGE: Pending counts for dashboard
// =====================================================================

/**
 * Get count of pending cross-dock candidates across all locations.
 *
 * @return int
 */
function get_pending_cross_dock_count()
{
	$sql = "SELECT COUNT(DISTINCT sod.id) AS cnt
		FROM " . TB_PREF . "sales_order_details sod
		INNER JOIN " . TB_PREF . "sales_orders so ON sod.order_no = so.order_no
			AND so.trans_type = " . ST_SALESORDER . "
		INNER JOIN " . TB_PREF . "stock_master sm ON sod.stk_code = sm.stock_id
		INNER JOIN " . TB_PREF . "locations loc ON so.from_stk_loc = loc.loc_code
		WHERE (sod.quantity - sod.qty_sent) > 0
			AND sm.cross_dock_eligible = 1
			AND loc.cross_dock_enabled = 1
			AND sod.drop_ship = 0";
	$result = db_query($sql, 'could not count cross-dock candidates');
	$row = db_fetch($result);
	return $row ? (int)$row['cnt'] : 0;
}

/**
 * Get count of active drop-ship POs (not fully received).
 *
 * @return int
 */
function get_active_drop_ship_count()
{
	$sql = "SELECT COUNT(*) AS cnt
		FROM " . TB_PREF . "purch_orders po
		WHERE po.drop_ship = 1
			AND EXISTS (
				SELECT 1 FROM " . TB_PREF . "purch_order_details pod
				WHERE pod.order_no = po.order_no
				AND pod.quantity_received < pod.quantity_ordered
			)";
	$result = db_query($sql, 'could not count active drop-ship POs');
	$row = db_fetch($result);
	return $row ? (int)$row['cnt'] : 0;
}
