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
 * Database query functions for warehouse management reports (Session 29 — Plan B).
 *
 * Reports: Bin contents, stock by location, stock movement ledger,
 *   stock aging, dead stock, warehouse utilization, turn rate,
 *   transfer order status, cycle count accuracy.
 */

/**
 * Get bin-level stock contents.
 *
 * @param string|null $loc_code   Inventory location code
 * @param int|null    $wh_loc_id  Warehouse location (bin) id
 * @param string|null $stock_id   Filter by item
 * @param int         $category   Filter by category (0 = all)
 * @return resource DB result
 */
function get_bin_contents_data($loc_code = null, $wh_loc_id = null, $stock_id = null, $category = 0)
{
	$sql = "SELECT bs.id, bs.wh_loc_id, bs.stock_id,
			si.description AS item_description,
			si.category_id, cat.description AS category_name,
			wl.location_code, wl.location_name, wl.location_path,
			lt.type_name AS location_type,
			bs.quantity, bs.reserved_qty,
			(bs.quantity - bs.reserved_qty) AS qty_available,
			bs.batch_id, sb.batch_no,
			bs.serial_id, sn.serial_no,
			bs.last_movement_date,
			(bs.quantity * si.material_cost) AS stock_value
		FROM " . TB_PREF . "wh_bin_stock bs
		LEFT JOIN " . TB_PREF . "stock_master si ON si.stock_id = bs.stock_id
		LEFT JOIN " . TB_PREF . "stock_category cat ON cat.category_id = si.category_id
		LEFT JOIN " . TB_PREF . "wh_locations wl ON wl.id = bs.wh_loc_id
		LEFT JOIN " . TB_PREF . "wh_location_types lt ON lt.id = wl.location_type_id
		LEFT JOIN " . TB_PREF . "stock_batches sb ON sb.id = bs.batch_id
		LEFT JOIN " . TB_PREF . "serial_numbers sn ON sn.id = bs.serial_id
		WHERE bs.quantity != 0";

	if ($loc_code && $loc_code != 'all')
		$sql .= " AND wl.loc_code = " . db_escape($loc_code);
	if ($wh_loc_id > 0)
		$sql .= " AND bs.wh_loc_id = " . (int)$wh_loc_id;
	if ($stock_id)
		$sql .= " AND bs.stock_id = " . db_escape($stock_id);
	if ($category > 0)
		$sql .= " AND si.category_id = " . db_escape($category);

	$sql .= " ORDER BY wl.location_code, bs.stock_id, sb.batch_no, sn.serial_no";

	return db_query($sql, 'Could not retrieve bin contents data');
}

/**
 * Get stock by hierarchical location (warehouse → zone → bin summary).
 *
 * @param string|null $loc_code Inventory location code
 * @param int         $category Filter by category (0 = all)
 * @return resource DB result
 */
function get_stock_by_location_data($loc_code = null, $category = 0)
{
	$sql = "SELECT wl.id AS wh_loc_id, wl.location_code, wl.location_name, wl.location_path,
			lt.type_name AS location_type, lt.hierarchy_level,
			bs.stock_id,
			si.description AS item_description,
			SUM(bs.quantity) AS total_qty,
			SUM(bs.reserved_qty) AS total_reserved,
			SUM(bs.quantity - bs.reserved_qty) AS total_available,
			SUM(bs.quantity * si.material_cost) AS total_value,
			COUNT(DISTINCT bs.stock_id) AS unique_items
		FROM " . TB_PREF . "wh_locations wl
		LEFT JOIN " . TB_PREF . "wh_location_types lt ON lt.id = wl.location_type_id
		LEFT JOIN " . TB_PREF . "wh_bin_stock bs ON bs.wh_loc_id = wl.id
		LEFT JOIN " . TB_PREF . "stock_master si ON si.stock_id = bs.stock_id
		WHERE 1=1";

	if ($loc_code && $loc_code != 'all')
		$sql .= " AND wl.loc_code = " . db_escape($loc_code);
	if ($category > 0)
		$sql .= " AND (si.category_id = " . db_escape($category) . " OR bs.stock_id IS NULL)";

	$sql .= " GROUP BY wl.id, wl.location_code, wl.location_name, wl.location_path,
			lt.type_name, lt.hierarchy_level, bs.stock_id, si.description
		ORDER BY wl.location_path, wl.location_code, bs.stock_id";

	return db_query($sql, 'Could not retrieve stock by location data');
}

/**
 * Get stock movement ledger at bin precision.
 *
 * @param string|null $from_date Start date
 * @param string|null $to_date   End date
 * @param string|null $loc_code  Inventory location
 * @param string|null $stock_id  Filter by item
 * @return resource DB result
 */
function get_bin_movement_ledger_data($from_date = null, $to_date = null, $loc_code = null, $stock_id = null)
{
	$sql = "SELECT sm.trans_id, sm.type, sm.trans_no, sm.tran_date, sm.stock_id,
			si.description AS item_description,
			sm.loc_code, sm.qty, sm.standard_cost, sm.price,
			sm.from_bin_id, fb.location_code AS from_bin_code,
			sm.to_bin_id, tb.location_code AS to_bin_code,
			sm.serial_id, sn.serial_no,
			sm.batch_id, sb.batch_no,
			sm.quality_status, sm.stock_status
		FROM " . TB_PREF . "stock_moves sm
		LEFT JOIN " . TB_PREF . "stock_master si ON si.stock_id = sm.stock_id
		LEFT JOIN " . TB_PREF . "wh_locations fb ON fb.id = sm.from_bin_id
		LEFT JOIN " . TB_PREF . "wh_locations tb ON tb.id = sm.to_bin_id
		LEFT JOIN " . TB_PREF . "serial_numbers sn ON sn.id = sm.serial_id
		LEFT JOIN " . TB_PREF . "stock_batches sb ON sb.id = sm.batch_id
		WHERE (sm.from_bin_id IS NOT NULL OR sm.to_bin_id IS NOT NULL)";

	if ($from_date)
		$sql .= " AND sm.tran_date >= '" . date2sql($from_date) . "'";
	if ($to_date)
		$sql .= " AND sm.tran_date <= '" . date2sql($to_date) . "'";
	if ($loc_code && $loc_code != 'all')
		$sql .= " AND sm.loc_code = " . db_escape($loc_code);
	if ($stock_id)
		$sql .= " AND sm.stock_id = " . db_escape($stock_id);

	$sql .= " ORDER BY sm.tran_date DESC, sm.trans_id DESC";

	return db_query($sql, 'Could not retrieve bin movement ledger data');
}

/**
 * Get stock aging in bins — how long items have been sitting.
 *
 * @param string|null $loc_code Inventory location
 * @param int         $category Filter by category (0 = all)
 * @param int         $min_days Show only items sitting longer than N days
 * @return resource DB result
 */
function get_bin_stock_aging_data($loc_code = null, $category = 0, $min_days = 0)
{
	$sql = "SELECT bs.wh_loc_id, wl.location_code, wl.location_name,
			bs.stock_id,
			si.description AS item_description,
			si.category_id, cat.description AS category_name,
			bs.quantity,
			bs.last_movement_date,
			DATEDIFF(NOW(), bs.last_movement_date) AS days_since_movement,
			(bs.quantity * si.material_cost) AS stock_value,
			bs.batch_id, sb.batch_no, sb.expiry_date
		FROM " . TB_PREF . "wh_bin_stock bs
		LEFT JOIN " . TB_PREF . "stock_master si ON si.stock_id = bs.stock_id
		LEFT JOIN " . TB_PREF . "stock_category cat ON cat.category_id = si.category_id
		LEFT JOIN " . TB_PREF . "wh_locations wl ON wl.id = bs.wh_loc_id
		LEFT JOIN " . TB_PREF . "stock_batches sb ON sb.id = bs.batch_id
		WHERE bs.quantity > 0";

	if ($loc_code && $loc_code != 'all')
		$sql .= " AND wl.loc_code = " . db_escape($loc_code);
	if ($category > 0)
		$sql .= " AND si.category_id = " . db_escape($category);
	if ($min_days > 0)
		$sql .= " AND DATEDIFF(NOW(), bs.last_movement_date) >= " . (int)$min_days;

	$sql .= " ORDER BY days_since_movement DESC, bs.stock_id";

	return db_query($sql, 'Could not retrieve stock aging data');
}

/**
 * Get dead stock — items with no movement in N days.
 *
 * @param int         $no_move_days Days without movement to qualify as dead stock
 * @param string|null $loc_code     Inventory location
 * @param int         $category     Filter by category (0 = all)
 * @return resource DB result
 */
function get_dead_stock_data($no_move_days = 180, $loc_code = null, $category = 0)
{
	$sql = "SELECT si.stock_id, si.description AS item_description,
			si.category_id, cat.description AS category_name,
			si.units, si.material_cost,
			ls.loc_code, loc.location_name,
			ls.quantity AS qty_on_hand,
			(ls.quantity * si.material_cost) AS stock_value,
			(SELECT MAX(sm.tran_date) 
			 FROM " . TB_PREF . "stock_moves sm 
			 WHERE sm.stock_id = si.stock_id 
			   AND sm.loc_code = ls.loc_code
			) AS last_movement_date,
			DATEDIFF(NOW(), 
				(SELECT MAX(sm2.tran_date) 
				 FROM " . TB_PREF . "stock_moves sm2 
				 WHERE sm2.stock_id = si.stock_id 
				   AND sm2.loc_code = ls.loc_code
				)
			) AS days_since_movement
		FROM " . TB_PREF . "loc_stock ls
		LEFT JOIN " . TB_PREF . "stock_master si ON si.stock_id = ls.stock_id
		LEFT JOIN " . TB_PREF . "stock_category cat ON cat.category_id = si.category_id
		LEFT JOIN " . TB_PREF . "locations loc ON loc.loc_code = ls.loc_code
		WHERE ls.quantity > 0
		AND si.mb_flag <> 'D' AND si.mb_flag <> 'F'
		HAVING days_since_movement >= " . (int)$no_move_days . " OR last_movement_date IS NULL";

	if ($loc_code && $loc_code != 'all')
		$sql .= " AND ls.loc_code = " . db_escape($loc_code);
	if ($category > 0)
		$sql .= " AND si.category_id = " . db_escape($category);

	$sql .= " ORDER BY days_since_movement DESC, stock_value DESC";

	return db_query($sql, 'Could not retrieve dead stock data');
}

/**
 * Get warehouse utilization — capacity usage by location.
 *
 * @param string|null $loc_code Inventory location
 * @return resource DB result
 */
function get_warehouse_utilization_data($loc_code = null)
{
	$sql = "SELECT wl.id AS wh_loc_id, wl.location_code, wl.location_name,
			lt.type_name AS location_type,
			wl.max_weight, wl.max_volume,
			sc.max_weight AS cat_max_weight, sc.max_volume AS cat_max_volume,
			IFNULL(stock.total_qty, 0) AS items_stored,
			IFNULL(stock.total_weight, 0) AS weight_used,
			IFNULL(stock.total_volume, 0) AS volume_used,
			IFNULL(stock.total_value, 0) AS stock_value,
			IFNULL(stock.unique_items, 0) AS unique_items,
			CASE WHEN wl.max_weight > 0 THEN ROUND(IFNULL(stock.total_weight, 0) * 100 / wl.max_weight, 1) ELSE 0 END AS weight_pct,
			CASE WHEN wl.max_volume > 0 THEN ROUND(IFNULL(stock.total_volume, 0) * 100 / wl.max_volume, 1) ELSE 0 END AS volume_pct
		FROM " . TB_PREF . "wh_locations wl
		LEFT JOIN " . TB_PREF . "wh_location_types lt ON lt.id = wl.location_type_id
		LEFT JOIN " . TB_PREF . "wh_storage_categories sc ON sc.id = wl.storage_category_id
		LEFT JOIN (
			SELECT bs2.wh_loc_id,
				SUM(bs2.quantity) AS total_qty,
				SUM(bs2.quantity * IFNULL(si2.item_weight, 0)) AS total_weight,
				SUM(bs2.quantity * IFNULL(si2.item_volume, 0)) AS total_volume,
				SUM(bs2.quantity * si2.material_cost) AS total_value,
				COUNT(DISTINCT bs2.stock_id) AS unique_items
			FROM " . TB_PREF . "wh_bin_stock bs2
			LEFT JOIN " . TB_PREF . "stock_master si2 ON si2.stock_id = bs2.stock_id
			WHERE bs2.quantity > 0
			GROUP BY bs2.wh_loc_id
		) stock ON stock.wh_loc_id = wl.id
		WHERE wl.is_active = 1";

	if ($loc_code && $loc_code != 'all')
		$sql .= " AND wl.loc_code = " . db_escape($loc_code);

	$sql .= " ORDER BY wl.location_path, wl.location_code";

	return db_query($sql, 'Could not retrieve warehouse utilization data');
}

/**
 * Get inventory turn rate by item.
 *
 * @param string|null $from_date  Period start
 * @param string|null $to_date    Period end
 * @param string|null $location   Filter by location
 * @param int         $category   Filter by category (0 = all)
 * @return resource DB result
 */
function get_inventory_turn_rate_data($from_date = null, $to_date = null, $location = null, $category = 0)
{
	$to_sql = $to_date ? date2sql($to_date) : date('Y-m-d');
	$from_sql = $from_date ? date2sql($from_date) : date('Y-m-d', strtotime('-1 year'));

	$sql = "SELECT si.stock_id, si.description AS item_description,
			si.category_id, cat.description AS category_name,
			si.material_cost,
			ABS(IFNULL(cogs.total_cogs, 0)) AS cogs_value,
			IFNULL(stk.avg_inventory, 0) AS avg_inventory_value,
			CASE WHEN IFNULL(stk.avg_inventory, 0) > 0 
				THEN ROUND(ABS(IFNULL(cogs.total_cogs, 0)) / stk.avg_inventory, 2)
				ELSE 0 
			END AS turnover_ratio,
			CASE WHEN ABS(IFNULL(cogs.total_cogs, 0)) > 0 AND IFNULL(stk.avg_inventory, 0) > 0
				THEN ROUND(DATEDIFF('$to_sql', '$from_sql') / (ABS(cogs.total_cogs) / stk.avg_inventory), 0)
				ELSE 0
			END AS days_of_supply
		FROM " . TB_PREF . "stock_master si
		LEFT JOIN " . TB_PREF . "stock_category cat ON cat.category_id = si.category_id
		LEFT JOIN (
			SELECT sm.stock_id,
				SUM(CASE WHEN sm.qty < 0 AND sm.type IN (13, 11) THEN sm.qty * sm.standard_cost ELSE 0 END) AS total_cogs
			FROM " . TB_PREF . "stock_moves sm
			WHERE sm.tran_date BETWEEN '$from_sql' AND '$to_sql'";

	if ($location && $location != 'all')
		$sql .= " AND sm.loc_code = " . db_escape($location);

	$sql .= " GROUP BY sm.stock_id
		) cogs ON cogs.stock_id = si.stock_id
		LEFT JOIN (
			SELECT ls.stock_id,
				SUM(ls.quantity * smi.material_cost) AS avg_inventory
			FROM " . TB_PREF . "loc_stock ls
			LEFT JOIN " . TB_PREF . "stock_master smi ON smi.stock_id = ls.stock_id";

	if ($location && $location != 'all')
		$sql .= " WHERE ls.loc_code = " . db_escape($location);

	$sql .= " GROUP BY ls.stock_id
		) stk ON stk.stock_id = si.stock_id
		WHERE si.mb_flag <> 'D' AND si.mb_flag <> 'F'";

	if ($category > 0)
		$sql .= " AND si.category_id = " . db_escape($category);

	$sql .= " HAVING cogs_value > 0 OR avg_inventory_value > 0
		ORDER BY turnover_ratio DESC, si.stock_id";

	return db_query($sql, 'Could not retrieve inventory turn rate data');
}

/**
 * Get transfer order status.
 *
 * @param string|null $from_date Start date
 * @param string|null $to_date   End date
 * @param string      $status    Filter by status (empty = all)
 * @return resource DB result
 */
function get_transfer_order_status_data($from_date = null, $to_date = null, $status = '')
{
	$sql = "SELECT t.id AS transfer_id, t.reference, t.transfer_date,
			t.from_loc, fl.location_name AS from_location_name,
			t.to_loc, tl.location_name AS to_location_name,
			t.status, t.ship_date, t.receive_date,
			t.notes,
			COUNT(tl2.id) AS line_count,
			SUM(tl2.quantity) AS total_qty,
			SUM(tl2.received_qty) AS total_received
		FROM " . TB_PREF . "wh_transfer_orders t
		LEFT JOIN " . TB_PREF . "locations fl ON fl.loc_code = t.from_loc
		LEFT JOIN " . TB_PREF . "locations tl ON tl.loc_code = t.to_loc
		LEFT JOIN " . TB_PREF . "wh_transfer_order_lines tl2 ON tl2.transfer_id = t.id
		WHERE 1=1";

	if ($from_date)
		$sql .= " AND t.transfer_date >= '" . date2sql($from_date) . "'";
	if ($to_date)
		$sql .= " AND t.transfer_date <= '" . date2sql($to_date) . "'";
	if ($status)
		$sql .= " AND t.status = " . db_escape($status);

	$sql .= " GROUP BY t.id
		ORDER BY t.transfer_date DESC, t.id DESC";

	return db_query($sql, 'Could not retrieve transfer order status data');
}

/**
 * Get cycle count accuracy analysis.
 *
 * @param string|null $from_date Start date
 * @param string|null $to_date   End date
 * @param string|null $loc_code  Inventory location
 * @return resource DB result
 */
function get_cycle_count_accuracy_data($from_date = null, $to_date = null, $loc_code = null)
{
	$sql = "SELECT cc.id AS count_id, cc.reference, cc.count_date,
			cc.loc_code, loc.location_name,
			cc.status,
			COUNT(ccl.id) AS total_lines,
			SUM(CASE WHEN ccl.variance_qty = 0 THEN 1 ELSE 0 END) AS accurate_lines,
			SUM(CASE WHEN ccl.variance_qty != 0 THEN 1 ELSE 0 END) AS variance_lines,
			ROUND(SUM(CASE WHEN ccl.variance_qty = 0 THEN 1 ELSE 0 END) * 100.0 / COUNT(ccl.id), 1) AS accuracy_pct,
			SUM(ABS(ccl.variance_qty)) AS total_abs_variance,
			SUM(ccl.variance_qty * ccl.standard_cost) AS total_variance_value
		FROM " . TB_PREF . "wh_cycle_counts cc
		LEFT JOIN " . TB_PREF . "locations loc ON loc.loc_code = cc.loc_code
		LEFT JOIN " . TB_PREF . "wh_cycle_count_lines ccl ON ccl.cycle_count_id = cc.id
		WHERE 1=1";

	if ($from_date)
		$sql .= " AND cc.count_date >= '" . date2sql($from_date) . "'";
	if ($to_date)
		$sql .= " AND cc.count_date <= '" . date2sql($to_date) . "'";
	if ($loc_code && $loc_code != 'all')
		$sql .= " AND cc.loc_code = " . db_escape($loc_code);

	$sql .= " GROUP BY cc.id
		ORDER BY cc.count_date DESC, cc.id DESC";

	return db_query($sql, 'Could not retrieve cycle count accuracy data');
}
