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
 * Warehouse Dashboard data aggregation functions.
 *
 * Provides KPI queries for the central WMS dashboard:
 * - Stock value and unit summaries
 * - Operations pipeline counts
 * - Alert detection (low stock, expiry, capacity)
 * - Projected stock forecast from open PO/SO/WO
 * - Bin-level drill-down data
 *
 * @package NotrinosERP
 * @subpackage Inventory/Warehouse
 */

//----------------------------------------------------------------------
// Stock Value & Units KPIs
//----------------------------------------------------------------------

/**
 * Get total stock value and unit count across all or a specific warehouse.
 *
 * @param string $warehouse_loc_code Warehouse code (empty = all WMS warehouses)
 * @return array ['total_value', 'total_units', 'distinct_items', 'total_bins_used']
 */
function get_dashboard_stock_summary($warehouse_loc_code = '') {
	if ($warehouse_loc_code) {
		$sql = "SELECT
				COALESCE(SUM(bs.qty_on_hand * COALESCE(sm.material_cost, 0)), 0) AS total_value,
				COALESCE(SUM(bs.qty_on_hand), 0) AS total_units,
				COUNT(DISTINCT bs.stock_id) AS distinct_items,
				COUNT(DISTINCT bs.wh_loc_id) AS total_bins_used
			FROM " . TB_PREF . "wh_bin_stock bs
			INNER JOIN " . TB_PREF . "wh_locations wl ON bs.wh_loc_id = wl.loc_id
			LEFT JOIN " . TB_PREF . "stock_master sm ON bs.stock_id = sm.stock_id
			WHERE wl.warehouse_loc_code = " . db_escape($warehouse_loc_code)
			. " AND bs.qty_on_hand > 0";
	} else {
		$sql = "SELECT
				COALESCE(SUM(bs.qty_on_hand * COALESCE(sm.material_cost, 0)), 0) AS total_value,
				COALESCE(SUM(bs.qty_on_hand), 0) AS total_units,
				COUNT(DISTINCT bs.stock_id) AS distinct_items,
				COUNT(DISTINCT bs.wh_loc_id) AS total_bins_used
			FROM " . TB_PREF . "wh_bin_stock bs
			LEFT JOIN " . TB_PREF . "stock_master sm ON bs.stock_id = sm.stock_id
			WHERE bs.qty_on_hand > 0";
	}
	$result = db_query($sql, 'could not get dashboard stock summary');
	$row = db_fetch($result);
	if (!$row)
		return array('total_value' => 0, 'total_units' => 0, 'distinct_items' => 0, 'total_bins_used' => 0);

	return array(
		'total_value'    => round((float)$row['total_value'], 2),
		'total_units'    => (float)$row['total_units'],
		'distinct_items' => (int)$row['distinct_items'],
		'total_bins_used'=> (int)$row['total_bins_used'],
	);
}

//----------------------------------------------------------------------
// Operations Pipeline
//----------------------------------------------------------------------

/**
 * Get operations pipeline counts for the dashboard.
 *
 * Pipeline stages: To Receive → To Put Away → To Pick → To Pack → To Ship
 * Each stage counts operations in ready or in_progress status.
 *
 * @param string $warehouse_loc_code Warehouse code (empty = all)
 * @return array ['to_receive', 'to_putaway', 'to_pick', 'to_pack', 'to_ship', 'total_pending']
 */
function get_dashboard_operations_pipeline($warehouse_loc_code = '') {
	$where_parts = array("op.op_status IN ('ready', 'in_progress')");

	if ($warehouse_loc_code) {
		$where_parts[] = "(op.from_loc_id IN (SELECT loc_id FROM " . TB_PREF . "wh_locations WHERE warehouse_loc_code=" . db_escape($warehouse_loc_code) . ")"
			. " OR op.to_loc_id IN (SELECT loc_id FROM " . TB_PREF . "wh_locations WHERE warehouse_loc_code=" . db_escape($warehouse_loc_code) . "))";
	}

	$where = implode(' AND ', $where_parts);

	$sql = "SELECT
			SUM(CASE WHEN op.op_type = 'receipt' THEN 1 ELSE 0 END) AS to_receive,
			SUM(CASE WHEN op.op_type = 'putaway' THEN 1 ELSE 0 END) AS to_putaway,
			SUM(CASE WHEN op.op_type = 'pick' THEN 1 ELSE 0 END) AS to_pick,
			SUM(CASE WHEN op.op_type = 'pack' THEN 1 ELSE 0 END) AS to_pack,
			SUM(CASE WHEN op.op_type = 'ship' THEN 1 ELSE 0 END) AS to_ship,
			SUM(CASE WHEN op.op_type = 'quality_check' THEN 1 ELSE 0 END) AS to_inspect,
			SUM(CASE WHEN op.op_type = 'transfer' THEN 1 ELSE 0 END) AS to_transfer,
			COUNT(*) AS total_pending
		FROM " . TB_PREF . "wh_operations op
		WHERE " . $where;

	$result = db_query($sql, 'could not get operations pipeline');
	$row = db_fetch($result);
	if (!$row)
		return array('to_receive' => 0, 'to_putaway' => 0, 'to_pick' => 0, 'to_pack' => 0,
			'to_ship' => 0, 'to_inspect' => 0, 'to_transfer' => 0, 'total_pending' => 0);

	return array(
		'to_receive'    => (int)$row['to_receive'],
		'to_putaway'    => (int)$row['to_putaway'],
		'to_pick'       => (int)$row['to_pick'],
		'to_pack'       => (int)$row['to_pack'],
		'to_ship'       => (int)$row['to_ship'],
		'to_inspect'    => (int)$row['to_inspect'],
		'to_transfer'   => (int)$row['to_transfer'],
		'total_pending' => (int)$row['total_pending'],
	);
}

/**
 * Get recent completed operations for the activity feed.
 *
 * @param string $warehouse_loc_code Warehouse code (empty = all)
 * @param int    $limit              Max rows
 * @return resource DB result
 */
function get_dashboard_recent_operations($warehouse_loc_code = '', $limit = 10) {
	$where_parts = array("op.op_status = 'done'");

	if ($warehouse_loc_code) {
		$where_parts[] = "(op.from_loc_id IN (SELECT loc_id FROM " . TB_PREF . "wh_locations WHERE warehouse_loc_code=" . db_escape($warehouse_loc_code) . ")"
			. " OR op.to_loc_id IN (SELECT loc_id FROM " . TB_PREF . "wh_locations WHERE warehouse_loc_code=" . db_escape($warehouse_loc_code) . "))";
	}

	$sql = "SELECT op.op_id, op.op_type, op.op_status, op.source_doc_type, op.source_doc_no,
			op.completed_at, op.created_at,
			COUNT(ol.line_id) AS line_count,
			SUM(ol.qty_done) AS total_qty_done
		FROM " . TB_PREF . "wh_operations op
		LEFT JOIN " . TB_PREF . "wh_operation_lines ol ON op.op_id = ol.op_id
		WHERE " . implode(' AND ', $where_parts)
		. " GROUP BY op.op_id"
		. " ORDER BY op.completed_at DESC"
		. " LIMIT " . (int)$limit;
	return db_query($sql, 'could not get recent operations');
}

//----------------------------------------------------------------------
// Alerts
//----------------------------------------------------------------------

/**
 * Get low stock alerts — items below reorder level at WMS locations.
 *
 * @param string $warehouse_loc_code Warehouse code (empty = all WMS locations)
 * @param int    $limit              Max rows
 * @return resource DB result with stock_id, description, loc_code, on_hand, reorder_level, deficit
 */
function get_dashboard_low_stock_alerts($warehouse_loc_code = '', $limit = 20) {
	$where_parts = array(
		"rl.reorder_level > 0",
		"sm.inactive = 0",
		"sm.mb_flag <> 'D'",
	);

	if ($warehouse_loc_code) {
		$where_parts[] = "l.loc_code = " . db_escape($warehouse_loc_code);
	}

	$sql = "SELECT sm.stock_id, sm.description, sm.units, l.loc_code, l.location_name,
			rl.reorder_level,
			COALESCE(
				(SELECT SUM(qty) FROM " . TB_PREF . "stock_moves
				 WHERE stock_id = sm.stock_id AND loc_code = l.loc_code),
			0) AS qty_on_hand
		FROM " . TB_PREF . "loc_stock rl
		INNER JOIN " . TB_PREF . "stock_master sm ON rl.stock_id = sm.stock_id
		INNER JOIN " . TB_PREF . "locations l ON rl.loc_code = l.loc_code
		WHERE " . implode(' AND ', $where_parts)
		. " HAVING qty_on_hand < rl.reorder_level"
		. " ORDER BY (qty_on_hand / rl.reorder_level) ASC"
		. " LIMIT " . (int)$limit;

	return db_query($sql, 'could not get low stock alerts');
}

/**
 * Get count of active alerts by category.
 *
 * @param string $warehouse_loc_code Warehouse code (empty = all)
 * @return array ['low_stock', 'expiring_soon', 'expired', 'capacity_critical', 'pending_inspections', 'overdue_counts']
 */
function get_dashboard_alert_counts($warehouse_loc_code = '') {
	$alerts = array(
		'low_stock'           => 0,
		'expiring_soon'       => 0,
		'expired'             => 0,
		'capacity_critical'   => 0,
		'pending_inspections' => 0,
		'overdue_counts'      => 0,
	);

	// Low stock: items below reorder level
	$sql = "SELECT COUNT(*) FROM " . TB_PREF . "loc_stock rl
		INNER JOIN " . TB_PREF . "stock_master sm ON rl.stock_id = sm.stock_id
		WHERE rl.reorder_level > 0 AND sm.inactive = 0 AND sm.mb_flag <> 'D'
		AND COALESCE((SELECT SUM(qty) FROM " . TB_PREF . "stock_moves WHERE stock_id = rl.stock_id AND loc_code = rl.loc_code), 0) < rl.reorder_level";
	$result = db_query($sql, 'alert count');
	$row = db_fetch_row($result);
	$alerts['low_stock'] = (int)$row[0];

	// Expiring soon (within 30 days)
	$sql = "SELECT COUNT(*) FROM " . TB_PREF . "stock_batches
		WHERE status = 'active' AND inactive = 0
		AND expiry_date IS NOT NULL AND expiry_date >= CURDATE()
		AND expiry_date <= DATE_ADD(CURDATE(), INTERVAL 30 DAY)";
	$result = db_query($sql, 'alert count');
	$row = db_fetch_row($result);
	$alerts['expiring_soon'] = (int)$row[0];

	// Expired but still active
	$sql = "SELECT COUNT(*) FROM " . TB_PREF . "stock_batches
		WHERE status = 'active' AND inactive = 0
		AND expiry_date IS NOT NULL AND expiry_date < CURDATE()";
	$result = db_query($sql, 'alert count');
	$row = db_fetch_row($result);
	$alerts['expired'] = (int)$row[0];

	// Capacity critical: bins at >85% utilization
	if ($warehouse_loc_code) {
		$sql = "SELECT COUNT(*) FROM " . TB_PREF . "wh_locations wl
			INNER JOIN " . TB_PREF . "wh_location_types lt ON wl.location_type_id = lt.id
			WHERE wl.warehouse_loc_code = " . db_escape($warehouse_loc_code)
			. " AND lt.can_store = 1 AND wl.is_active = 1"
			. " AND wl.max_weight > 0 AND (wl.current_weight / wl.max_weight) > 0.85";
	} else {
		$sql = "SELECT COUNT(*) FROM " . TB_PREF . "wh_locations wl
			INNER JOIN " . TB_PREF . "wh_location_types lt ON wl.location_type_id = lt.id
			WHERE lt.can_store = 1 AND wl.is_active = 1"
			. " AND wl.max_weight > 0 AND (wl.current_weight / wl.max_weight) > 0.85";
	}
	$result = db_query($sql, 'alert count');
	$row = db_fetch_row($result);
	$alerts['capacity_critical'] = (int)$row[0];

	// Pending QC inspections
	$sql = "SELECT COUNT(*) FROM " . TB_PREF . "quality_inspections
		WHERE result IN ('pending', 'in_progress')";
	$result = db_query($sql, 'alert count');
	$row = db_fetch_row($result);
	$alerts['pending_inspections'] = (int)$row[0];

	// Overdue cycle counts
	$sql = "SELECT COUNT(*) FROM " . TB_PREF . "wh_cycle_count_plans
		WHERE active = 1 AND next_count_date IS NOT NULL AND next_count_date < CURDATE()";
	$result = db_query($sql, 'alert count');
	$row = db_fetch_row($result);
	$alerts['overdue_counts'] = (int)$row[0];

	return $alerts;
}

//----------------------------------------------------------------------
// Capacity Heatmap Data
//----------------------------------------------------------------------

/**
 * Get zone-level capacity data for heatmap display.
 *
 * Returns zones with utilization percentages and color coding.
 *
 * @param string $warehouse_loc_code Warehouse code
 * @return array Array of zone data with utilization metrics
 */
function get_dashboard_capacity_heatmap($warehouse_loc_code) {
	if (!$warehouse_loc_code)
		return array();

	$sql = "SELECT wl.loc_id, wl.loc_code, wl.loc_name,
			lt.type_code, lt.type_name,
			COUNT(DISTINCT child.loc_id) AS bin_count,
			SUM(CASE WHEN clt.can_store = 1 AND child.max_weight > 0
				AND (child.current_weight / child.max_weight) > 0.85 THEN 1 ELSE 0 END) AS critical_bins,
			SUM(CASE WHEN clt.can_store = 1 AND child.max_weight > 0
				AND (child.current_weight / child.max_weight) <= 0.60 THEN 1 ELSE 0 END) AS ok_bins,
			COALESCE(SUM(child.max_weight), 0) AS total_max_weight,
			COALESCE(SUM(child.current_weight), 0) AS total_current_weight,
			COALESCE(SUM(child.max_volume), 0) AS total_max_volume,
			COALESCE(SUM(child.current_volume), 0) AS total_current_volume
		FROM " . TB_PREF . "wh_locations wl
		INNER JOIN " . TB_PREF . "wh_location_types lt ON wl.location_type_id = lt.id
		LEFT JOIN " . TB_PREF . "wh_locations child ON child.parent_loc_id = wl.loc_id AND child.is_active = 1
		LEFT JOIN " . TB_PREF . "wh_location_types clt ON child.location_type_id = clt.id
		WHERE wl.warehouse_loc_code = " . db_escape($warehouse_loc_code)
		. " AND wl.parent_loc_id = (SELECT loc_id FROM " . TB_PREF . "wh_locations WHERE warehouse_loc_code = " . db_escape($warehouse_loc_code) . " AND parent_loc_id IS NULL LIMIT 1)"
		. " AND wl.is_active = 1"
		. " GROUP BY wl.loc_id"
		. " ORDER BY wl.loc_code";

	$result = db_query($sql, 'could not get capacity heatmap data');

	$zones = array();
	while ($row = db_fetch($result)) {
		$weight_pct = $row['total_max_weight'] > 0
			? round(($row['total_current_weight'] / $row['total_max_weight']) * 100, 1) : 0;
		$volume_pct = $row['total_max_volume'] > 0
			? round(($row['total_current_volume'] / $row['total_max_volume']) * 100, 1) : 0;
		$overall_pct = max($weight_pct, $volume_pct);

		if ($overall_pct > 95) $status = 'full';
		elseif ($overall_pct > 85) $status = 'critical';
		elseif ($overall_pct > 60) $status = 'warning';
		else $status = 'ok';

		$zones[] = array(
			'loc_id'        => (int)$row['loc_id'],
			'loc_code'      => $row['loc_code'],
			'loc_name'      => $row['loc_name'],
			'type_code'     => $row['type_code'],
			'type_name'     => $row['type_name'],
			'bin_count'     => (int)$row['bin_count'],
			'critical_bins' => (int)$row['critical_bins'],
			'ok_bins'       => (int)$row['ok_bins'],
			'weight_pct'    => $weight_pct,
			'volume_pct'    => $volume_pct,
			'overall_pct'   => $overall_pct,
			'status'        => $status,
		);
	}
	return $zones;
}

//----------------------------------------------------------------------
// Drill-Down: Bin Contents
//----------------------------------------------------------------------

/**
 * Get bin-level stock detail for a specific location (drill-down).
 *
 * @param int $loc_id Location ID to drill into
 * @return resource DB result with stock details per bin
 */
function get_dashboard_bin_contents($loc_id) {
	$sql = "SELECT bs.wh_loc_id, wl.loc_code AS bin_code, wl.loc_name AS bin_name,
			bs.stock_id, sm.description AS item_description, sm.units,
			bs.qty_on_hand, bs.qty_reserved,
			(bs.qty_on_hand - bs.qty_reserved) AS qty_available,
			bs.batch_id, sb.batch_no, sb.expiry_date,
			bs.serial_id, sn.serial_no,
			COALESCE(sm.material_cost, 0) AS unit_cost,
			(bs.qty_on_hand * COALESCE(sm.material_cost, 0)) AS line_value
		FROM " . TB_PREF . "wh_bin_stock bs
		INNER JOIN " . TB_PREF . "wh_locations wl ON bs.wh_loc_id = wl.loc_id
		LEFT JOIN " . TB_PREF . "stock_master sm ON bs.stock_id = sm.stock_id
		LEFT JOIN " . TB_PREF . "stock_batches sb ON bs.batch_id = sb.id
		LEFT JOIN " . TB_PREF . "serial_numbers sn ON bs.serial_id = sn.id
		WHERE wl.loc_id = " . (int)$loc_id
		. " OR wl.parent_loc_id = " . (int)$loc_id
		. " ORDER BY wl.loc_code, bs.stock_id";
	return db_query($sql, 'could not get bin contents');
}

/**
 * Get child locations for drill-down navigation.
 *
 * @param int $parent_loc_id Parent location ID
 * @return resource DB result
 */
function get_dashboard_location_children($parent_loc_id) {
	$sql = "SELECT wl.loc_id, wl.loc_code, wl.loc_name,
			lt.type_code, lt.type_name,
			lt.can_store, wl.is_active,
			wl.max_weight, wl.current_weight,
			wl.max_volume, wl.current_volume,
			(SELECT COUNT(*) FROM " . TB_PREF . "wh_locations c WHERE c.parent_loc_id = wl.loc_id) AS child_count,
			(SELECT COALESCE(SUM(bs.qty_on_hand), 0) FROM " . TB_PREF . "wh_bin_stock bs
				INNER JOIN " . TB_PREF . "wh_locations bl ON bs.wh_loc_id = bl.loc_id
				WHERE bl.location_path LIKE CONCAT(wl.location_path, wl.loc_id, '/%')
					OR bl.loc_id = wl.loc_id) AS total_stock_qty
		FROM " . TB_PREF . "wh_locations wl
		INNER JOIN " . TB_PREF . "wh_location_types lt ON wl.location_type_id = lt.id
		WHERE wl.parent_loc_id = " . (int)$parent_loc_id
		. " AND wl.is_active = 1"
		. " ORDER BY wl.loc_code";
	return db_query($sql, 'could not get location children');
}

//----------------------------------------------------------------------
// Stock Forecast (PO, SO, WO projections)
//----------------------------------------------------------------------

/**
 * Get projected stock changes from open Purchase Orders, Sales Orders, and Work Orders.
 *
 * @param string $warehouse_loc_code Warehouse code (empty = all)
 * @param int    $days_ahead         How many days to project (default 30)
 * @return array ['incoming' => [...], 'outgoing' => [...], 'production' => [...]]
 */
function get_dashboard_stock_forecast($warehouse_loc_code = '', $days_ahead = 30) {
	$forecast = array(
		'incoming'   => array('total_qty' => 0, 'total_value' => 0, 'order_count' => 0),
		'outgoing'   => array('total_qty' => 0, 'total_value' => 0, 'order_count' => 0),
		'production' => array('total_qty' => 0, 'total_value' => 0, 'order_count' => 0),
	);

	// Incoming: open PO lines not yet fully received
	$sql = "SELECT
			COUNT(DISTINCT po.order_no) AS order_count,
			COALESCE(SUM(pol.quantity_ordered - pol.quantity_received), 0) AS pending_qty,
			COALESCE(SUM((pol.quantity_ordered - pol.quantity_received) * pol.unit_price), 0) AS pending_value
		FROM " . TB_PREF . "purch_order_details pol
		INNER JOIN " . TB_PREF . "purch_orders po ON pol.order_no = po.order_no
		WHERE pol.quantity_ordered > pol.quantity_received
		AND pol.delivery_date <= DATE_ADD(CURDATE(), INTERVAL " . (int)$days_ahead . " DAY)";

	if ($warehouse_loc_code) {
		$sql .= " AND po.into_stock_location = " . db_escape($warehouse_loc_code);
	}

	$result = db_query($sql, 'could not get PO forecast');
	$row = db_fetch($result);
	if ($row) {
		$forecast['incoming']['total_qty'] = (float)$row['pending_qty'];
		$forecast['incoming']['total_value'] = round((float)$row['pending_value'], 2);
		$forecast['incoming']['order_count'] = (int)$row['order_count'];
	}

	// Outgoing: open SO lines not yet fully delivered
	$sql = "SELECT
			COUNT(DISTINCT so.order_no) AS order_count,
			COALESCE(SUM(sol.quantity - sol.qty_sent), 0) AS pending_qty,
			COALESCE(SUM((sol.quantity - sol.qty_sent) * sol.unit_price), 0) AS pending_value
		FROM " . TB_PREF . "sales_order_details sol
		INNER JOIN " . TB_PREF . "sales_orders so ON sol.order_no = so.order_no AND sol.trans_type = so.trans_type
		WHERE sol.quantity > sol.qty_sent
		AND so.trans_type = " . ST_SALESORDER
		. " AND so.delivery_date <= DATE_ADD(CURDATE(), INTERVAL " . (int)$days_ahead . " DAY)";

	$result = db_query($sql, 'could not get SO forecast');
	$row = db_fetch($result);
	if ($row) {
		$forecast['outgoing']['total_qty'] = (float)$row['pending_qty'];
		$forecast['outgoing']['total_value'] = round((float)$row['pending_value'], 2);
		$forecast['outgoing']['order_count'] = (int)$row['order_count'];
	}

	// Production: open WO not yet completed
	$sql = "SELECT
			COUNT(*) AS order_count,
			COALESCE(SUM(wo.units_reqd - wo.units_issued), 0) AS pending_qty
		FROM " . TB_PREF . "workorders wo
		WHERE wo.closed = 0
		AND (wo.units_reqd - wo.units_issued) > 0
		AND wo.required_by <= DATE_ADD(CURDATE(), INTERVAL " . (int)$days_ahead . " DAY)";

	$result = db_query($sql, 'could not get WO forecast');
	$row = db_fetch($result);
	if ($row) {
		$forecast['production']['total_qty'] = (float)$row['pending_qty'];
		$forecast['production']['order_count'] = (int)$row['order_count'];
	}

	return $forecast;
}

/**
 * Get top items by stock value in a warehouse (for top stock widget).
 *
 * @param string $warehouse_loc_code Warehouse code (empty = all)
 * @param int    $limit              Max items
 * @return resource DB result
 */
function get_dashboard_top_stock_items($warehouse_loc_code = '', $limit = 10) {
	$where_parts = array("bs.qty_on_hand > 0");

	if ($warehouse_loc_code) {
		$where_parts[] = "wl.warehouse_loc_code = " . db_escape($warehouse_loc_code);
	}

	$sql = "SELECT bs.stock_id, sm.description, sm.units,
			SUM(bs.qty_on_hand) AS total_qty,
			COALESCE(sm.material_cost, 0) AS unit_cost,
			SUM(bs.qty_on_hand) * COALESCE(sm.material_cost, 0) AS total_value,
			COUNT(DISTINCT bs.wh_loc_id) AS bin_count
		FROM " . TB_PREF . "wh_bin_stock bs
		INNER JOIN " . TB_PREF . "wh_locations wl ON bs.wh_loc_id = wl.loc_id
		LEFT JOIN " . TB_PREF . "stock_master sm ON bs.stock_id = sm.stock_id
		WHERE " . implode(' AND ', $where_parts)
		. " GROUP BY bs.stock_id"
		. " ORDER BY total_value DESC"
		. " LIMIT " . (int)$limit;

	return db_query($sql, 'could not get top stock items');
}

/**
 * Get operations count by type for the last N days (for activity chart data).
 *
 * @param string $warehouse_loc_code Warehouse code (empty = all)
 * @param int    $days               Number of days to look back
 * @return array Associative: op_type => count
 */
function get_dashboard_operations_by_type($warehouse_loc_code = '', $days = 7) {
	$where_parts = array(
		"op.op_status = 'done'",
		"op.completed_at >= DATE_SUB(NOW(), INTERVAL " . (int)$days . " DAY)"
	);

	if ($warehouse_loc_code) {
		$where_parts[] = "(op.from_loc_id IN (SELECT loc_id FROM " . TB_PREF . "wh_locations WHERE warehouse_loc_code=" . db_escape($warehouse_loc_code) . ")"
			. " OR op.to_loc_id IN (SELECT loc_id FROM " . TB_PREF . "wh_locations WHERE warehouse_loc_code=" . db_escape($warehouse_loc_code) . "))";
	}

	$sql = "SELECT op.op_type, COUNT(*) AS cnt
		FROM " . TB_PREF . "wh_operations op
		WHERE " . implode(' AND ', $where_parts)
		. " GROUP BY op.op_type"
		. " ORDER BY cnt DESC";

	$result = db_query($sql, 'could not get operations by type');

	$data = array();
	while ($row = db_fetch($result))
		$data[$row['op_type']] = (int)$row['cnt'];

	return $data;
}
