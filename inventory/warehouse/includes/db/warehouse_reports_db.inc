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
 * Database query functions for warehouse management reports.
 *
 * Reports: Bin contents, stock by location, stock movement ledger,
 *   stock aging, dead stock, warehouse utilization, turn rate,
 *   transfer order status, cycle count accuracy.
 */

/**
 * Get bin-level stock contents.
 *
 * @param string|null $loc_code Inventory location code
 * @param string|null $bin_code Filter by warehouse bin code
 * @param string|null $stock_id Filter by item
 * @return resource DB result
 */
function get_bin_contents_data($loc_code = null, $bin_code = null, $stock_id = null)
{
	$sql = "SELECT bs.id, bs.wh_loc_id, bs.stock_id,
			si.description AS item_description,
			si.category_id, cat.description AS category_name,
			wl.loc_code AS location_code, wl.loc_name AS location_name, wl.location_path,
			lt.type_name AS location_type,
			bs.qty_on_hand AS quantity, bs.qty_reserved AS reserved_qty,
			(bs.qty_on_hand - bs.qty_reserved) AS qty_available,
			bs.batch_id, sb.batch_no,
			bs.serial_id, sn.serial_no,
			(bs.qty_on_hand * si.material_cost) AS stock_value
		FROM " . TB_PREF . "wh_bin_stock bs
		LEFT JOIN " . TB_PREF . "stock_master si ON si.stock_id = bs.stock_id
		LEFT JOIN " . TB_PREF . "stock_category cat ON cat.category_id = si.category_id
		LEFT JOIN " . TB_PREF . "wh_locations wl ON wl.loc_id = bs.wh_loc_id
		LEFT JOIN " . TB_PREF . "wh_location_types lt ON lt.id = wl.location_type_id
		LEFT JOIN " . TB_PREF . "stock_batches sb ON sb.id = bs.batch_id
		LEFT JOIN " . TB_PREF . "serial_numbers sn ON sn.id = bs.serial_id
		WHERE bs.qty_on_hand != 0";

	if ($loc_code && $loc_code != 'all')
		$sql .= " AND wl.loc_code = " . db_escape($loc_code);
	if ($bin_code)
		$sql .= " AND wl.loc_code LIKE " . db_escape('%' . $bin_code . '%');
	if ($stock_id)
		$sql .= " AND bs.stock_id = " . db_escape($stock_id);

	$sql .= " ORDER BY wl.loc_code, bs.stock_id, sb.batch_no, sn.serial_no";

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
	$sql = "SELECT wl.loc_id AS wh_loc_id, wl.loc_code AS location_code,
			wl.loc_name AS location_name, wl.location_path,
			lt.type_name AS location_type,
			CASE WHEN IFNULL(wl.location_path, '') = '' THEN 0
				ELSE LENGTH(wl.location_path) - LENGTH(REPLACE(wl.location_path, '/', ''))
			END AS hierarchy_level,
			bs.stock_id,
			si.description AS item_description,
			SUM(bs.qty_on_hand) AS total_qty,
			SUM(bs.qty_reserved) AS total_reserved,
			SUM(bs.qty_on_hand - bs.qty_reserved) AS total_available,
			SUM(bs.qty_on_hand * si.material_cost) AS total_value,
			COUNT(DISTINCT bs.stock_id) AS unique_items
		FROM " . TB_PREF . "wh_locations wl
		LEFT JOIN " . TB_PREF . "wh_location_types lt ON lt.id = wl.location_type_id
		LEFT JOIN " . TB_PREF . "wh_bin_stock bs ON bs.wh_loc_id = wl.loc_id
		LEFT JOIN " . TB_PREF . "stock_master si ON si.stock_id = bs.stock_id
		WHERE 1=1";

	if ($loc_code && $loc_code != 'all')
		$sql .= " AND wl.loc_code = " . db_escape($loc_code);
	if ($category > 0)
		$sql .= " AND (si.category_id = " . db_escape($category) . " OR bs.stock_id IS NULL)";

	$sql .= " GROUP BY wl.loc_id, wl.loc_code, wl.loc_name, wl.location_path,
			lt.type_name, bs.stock_id, si.description
		ORDER BY wl.location_path, wl.loc_code, bs.stock_id";

	return db_query($sql, 'Could not retrieve stock by location data');
}

/**
 * Get stock movement ledger at bin precision.
 *
 * @param string|null $from_date Start date
 * @param string|null $to_date   End date
 * @param string|null $loc_code  Inventory location
 * @param string|null $bin_code  Filter by warehouse bin code
 * @param string|null $stock_id  Filter by item
 * @return resource DB result
 */
function get_bin_movement_ledger_data($from_date = null, $to_date = null, $loc_code = null, $bin_code = null, $stock_id = null)
{
	$sql = "SELECT sm.trans_id, sm.type, sm.trans_no, sm.tran_date, sm.stock_id,
			si.description AS item_description,
			sm.loc_code, sm.qty, sm.standard_cost, sm.price,
			sm.from_bin_id, fb.loc_code AS from_bin_code,
			sm.to_bin_id, tb.loc_code AS to_bin_code,
			sm.serial_id, sn.serial_no,
			sm.batch_id, sb.batch_no,
			sm.quality_status, sm.stock_status
		FROM " . TB_PREF . "stock_moves sm
		LEFT JOIN " . TB_PREF . "stock_master si ON si.stock_id = sm.stock_id
		LEFT JOIN " . TB_PREF . "wh_locations fb ON fb.loc_id = sm.from_bin_id
		LEFT JOIN " . TB_PREF . "wh_locations tb ON tb.loc_id = sm.to_bin_id
		LEFT JOIN " . TB_PREF . "serial_numbers sn ON sn.id = sm.serial_id
		LEFT JOIN " . TB_PREF . "stock_batches sb ON sb.id = sm.batch_id
		WHERE (sm.from_bin_id IS NOT NULL OR sm.to_bin_id IS NOT NULL)";

	if ($from_date)
		$sql .= " AND sm.tran_date >= '" . date2sql($from_date) . "'";
	if ($to_date)
		$sql .= " AND sm.tran_date <= '" . date2sql($to_date) . "'";
	if ($loc_code && $loc_code != 'all')
		$sql .= " AND sm.loc_code = " . db_escape($loc_code);
	if ($bin_code)
		$sql .= " AND (fb.loc_code LIKE " . db_escape('%' . $bin_code . '%')
			. " OR tb.loc_code LIKE " . db_escape('%' . $bin_code . '%') . ")";
	if ($stock_id)
		$sql .= " AND sm.stock_id = " . db_escape($stock_id);

	$sql .= " ORDER BY sm.tran_date DESC, sm.trans_id DESC";

	return db_query($sql, 'Could not retrieve bin movement ledger data');
}

/**
 * Get stock aging in bins — how long items have been sitting.
 *
 * @param string|null $as_at_date Aging cutoff date
 * @param string|null $loc_code   Inventory location
 * @param int         $category   Filter by category (0 = all)
 * @return resource DB result
 */
function get_bin_stock_aging_data($as_at_date = null, $loc_code = null, $category = 0)
{
	$as_at_sql = $as_at_date ? date2sql($as_at_date) : date('Y-m-d');

	$sql = "SELECT bs.wh_loc_id, wl.loc_code AS location_code, wl.loc_name AS location_name,
			bs.stock_id,
			si.description AS item_description,
			si.category_id, cat.description AS category_name,
			bs.qty_on_hand AS quantity,
			COALESCE(last_move.last_movement_date, DATE(IFNULL(bs.updated_at, '$as_at_sql'))) AS last_movement_date,
			DATEDIFF('$as_at_sql', COALESCE(last_move.last_movement_date, DATE(IFNULL(bs.updated_at, '$as_at_sql')))) AS days_since_movement,
			(bs.qty_on_hand * si.material_cost) AS stock_value,
			bs.batch_id, sb.batch_no, COALESCE(bs.expiry_date, sb.expiry_date) AS expiry_date
		FROM " . TB_PREF . "wh_bin_stock bs
		LEFT JOIN " . TB_PREF . "stock_master si ON si.stock_id = bs.stock_id
		LEFT JOIN " . TB_PREF . "stock_category cat ON cat.category_id = si.category_id
		LEFT JOIN " . TB_PREF . "wh_locations wl ON wl.loc_id = bs.wh_loc_id
		LEFT JOIN " . TB_PREF . "stock_batches sb ON sb.id = bs.batch_id
		LEFT JOIN (
			SELECT stock_id, batch_id, serial_id, COALESCE(to_bin_id, from_bin_id) AS wh_loc_id,
				MAX(tran_date) AS last_movement_date
			FROM " . TB_PREF . "stock_moves
			WHERE (from_bin_id IS NOT NULL OR to_bin_id IS NOT NULL)
				AND tran_date <= '$as_at_sql'
			GROUP BY stock_id, batch_id, serial_id, COALESCE(to_bin_id, from_bin_id)
		) last_move ON last_move.stock_id = bs.stock_id
			AND last_move.wh_loc_id = bs.wh_loc_id
			AND (last_move.batch_id = bs.batch_id OR (last_move.batch_id IS NULL AND bs.batch_id IS NULL))
			AND (last_move.serial_id = bs.serial_id OR (last_move.serial_id IS NULL AND bs.serial_id IS NULL))
		WHERE bs.qty_on_hand > 0";

	if ($loc_code && $loc_code != 'all')
		$sql .= " AND wl.loc_code = " . db_escape($loc_code);
	if ($category > 0)
		$sql .= " AND si.category_id = " . db_escape($category);
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
function get_dead_stock_data($as_at_date = null, $no_move_days = 180, $loc_code = null, $category = 0)
{
	$as_at_sql = $as_at_date ? date2sql($as_at_date) : date('Y-m-d');

	$sql = "SELECT si.stock_id, si.description AS item_description,
			si.category_id, cat.description AS category_name,
			si.units, si.material_cost,
			ls.loc_code, loc.location_name,
			COALESCE(moves.qty_on_hand, 0) AS qty_on_hand,
			(COALESCE(moves.qty_on_hand, 0) * si.material_cost) AS stock_value,
			moves.last_movement_date,
			DATEDIFF('$as_at_sql', moves.last_movement_date) AS days_since_movement
		FROM " . TB_PREF . "loc_stock ls
		LEFT JOIN " . TB_PREF . "stock_master si ON si.stock_id = ls.stock_id
		LEFT JOIN " . TB_PREF . "stock_category cat ON cat.category_id = si.category_id
		LEFT JOIN " . TB_PREF . "locations loc ON loc.loc_code = ls.loc_code
		LEFT JOIN (
			SELECT stock_id, loc_code, SUM(qty) AS qty_on_hand,
				MAX(tran_date) AS last_movement_date
			FROM " . TB_PREF . "stock_moves
			WHERE tran_date <= '$as_at_sql'
			GROUP BY stock_id, loc_code
		) moves ON moves.stock_id = ls.stock_id AND moves.loc_code = ls.loc_code
		WHERE si.mb_flag <> 'D' AND si.mb_flag <> 'F'";

	if ($loc_code && $loc_code != 'all')
		$sql .= " AND ls.loc_code = " . db_escape($loc_code);
	if ($category > 0)
		$sql .= " AND si.category_id = " . db_escape($category);

	$sql .= " HAVING qty_on_hand > 0
		AND (days_since_movement >= " . (int)$no_move_days . " OR last_movement_date IS NULL)";

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
	$sql = "SELECT wl.loc_id AS wh_loc_id, wl.loc_code AS location_code, wl.loc_name AS location_name,
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
				SUM(bs2.qty_on_hand) AS total_qty,
				SUM(bs2.qty_on_hand * IFNULL(si2.item_weight, 0)) AS total_weight,
				SUM(bs2.qty_on_hand * IFNULL(si2.item_volume, 0)) AS total_volume,
				SUM(bs2.qty_on_hand * si2.material_cost) AS total_value,
				COUNT(DISTINCT bs2.stock_id) AS unique_items
			FROM " . TB_PREF . "wh_bin_stock bs2
			LEFT JOIN " . TB_PREF . "stock_master si2 ON si2.stock_id = bs2.stock_id
			WHERE bs2.qty_on_hand > 0
			GROUP BY bs2.wh_loc_id
		) stock ON stock.wh_loc_id = wl.loc_id
		WHERE wl.is_active = 1";

	if ($loc_code && $loc_code != 'all')
		$sql .= " AND wl.loc_code = " . db_escape($loc_code);

	$sql .= " ORDER BY wl.location_path, wl.loc_code";

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
				SUM(COALESCE(qoh.qty_on_hand, 0) * smi.material_cost) AS avg_inventory
			FROM " . TB_PREF . "loc_stock ls
			LEFT JOIN " . TB_PREF . "stock_master smi ON smi.stock_id = ls.stock_id
			LEFT JOIN (
				SELECT stock_id, loc_code, SUM(qty) AS qty_on_hand
				FROM " . TB_PREF . "stock_moves
				WHERE tran_date <= '$to_sql'
				GROUP BY stock_id, loc_code
			) qoh ON qoh.stock_id = ls.stock_id AND qoh.loc_code = ls.loc_code";

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
	$sql = "SELECT t.transfer_id, t.reference, t.request_date AS tran_date,
			t.from_loc_code, fl.location_name AS from_location,
			t.to_loc_code, tl.location_name AS to_location,
			t.status, t.status AS status_text, t.actual_ship_date, t.actual_recv_date,
			t.memo,
			COUNT(tl2.line_id) AS total_lines,
			SUM(tl2.qty_requested) AS total_qty,
			SUM(tl2.qty_received) AS received_qty
		FROM " . TB_PREF . "wh_transfer_orders t
		LEFT JOIN " . TB_PREF . "locations fl ON fl.loc_code = t.from_loc_code
		LEFT JOIN " . TB_PREF . "locations tl ON tl.loc_code = t.to_loc_code
		LEFT JOIN " . TB_PREF . "wh_transfer_order_lines tl2 ON tl2.transfer_id = t.transfer_id
		WHERE 1=1";

	if ($from_date)
		$sql .= " AND t.request_date >= '" . date2sql($from_date) . "'";
	if ($to_date)
		$sql .= " AND t.request_date <= '" . date2sql($to_date) . "'";
	if ($status && $status != 'all')
		$sql .= " AND t.status = " . db_escape($status);

	$sql .= " GROUP BY t.transfer_id
		ORDER BY t.request_date DESC, t.transfer_id DESC";

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
	$sql = "SELECT cc.count_id, CONCAT('#', cc.count_id) AS reference, cc.count_date,
			cc.warehouse_loc_code AS loc_code, loc.location_name,
			cc.status,
			COUNT(ccl.line_id) AS total_lines,
			SUM(CASE WHEN COALESCE(ccl.variance_qty, 0) = 0 THEN 1 ELSE 0 END) AS accurate_lines,
			SUM(CASE WHEN COALESCE(ccl.variance_qty, 0) != 0 THEN 1 ELSE 0 END) AS variance_lines,
			ROUND(SUM(CASE WHEN COALESCE(ccl.variance_qty, 0) = 0 THEN 1 ELSE 0 END) * 100.0 / NULLIF(COUNT(ccl.line_id), 0), 1) AS accuracy_pct,
			SUM(ABS(COALESCE(ccl.variance_qty, 0))) AS total_abs_variance,
			SUM(COALESCE(ccl.variance_qty, 0) * COALESCE(si.material_cost, 0)) AS variance_value
		FROM " . TB_PREF . "wh_cycle_counts cc
		LEFT JOIN " . TB_PREF . "locations loc ON loc.loc_code = cc.warehouse_loc_code
		LEFT JOIN " . TB_PREF . "wh_cycle_count_lines ccl ON ccl.count_id = cc.count_id
		LEFT JOIN " . TB_PREF . "stock_master si ON si.stock_id = ccl.stock_id
		WHERE 1=1";

	if ($from_date)
		$sql .= " AND cc.count_date >= '" . date2sql($from_date) . "'";
	if ($to_date)
		$sql .= " AND cc.count_date <= '" . date2sql($to_date) . "'";
	if ($loc_code && $loc_code != 'all')
		$sql .= " AND cc.warehouse_loc_code = " . db_escape($loc_code);

	$sql .= " GROUP BY cc.count_id
		ORDER BY cc.count_date DESC, cc.count_id DESC";

	return db_query($sql, 'Could not retrieve cycle count accuracy data');
}
