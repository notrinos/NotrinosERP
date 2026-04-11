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
 * Database query functions for advanced inventory reports (Session 29).
 *
 * Plan A reports: Serial ledger, batch balance, batch aging, expiry,
 *   QC summary, warranty claims, warranty status, FEFO compliance,
 *   serial cost, supplier quality.
 *
 * Plan B reports: Bin contents, stock by location, stock movement ledger,
 *   stock aging, dead stock, warehouse utilization, turn rate,
 *   transfer order status, cycle count accuracy.
 */

/**
 * Get serial number movement ledger.
 *
 * @param string|null $stock_id  Filter by item
 * @param string|null $serial_no Filter by serial number (LIKE)
 * @param string|null $from_date Start date
 * @param string|null $to_date   End date
 * @param string|null $location  Filter by location code
 * @return resource DB result
 */
function get_serial_ledger_data($stock_id = null, $serial_no = null, $from_date = null, $to_date = null, $location = null)
{
	$sql = "SELECT sm.id, sm.serial_id, sn.serial_no, sn.stock_id,
			si.description AS item_description,
			sm.trans_type, sm.trans_no, sm.tran_date,
			sm.from_loc, sm.to_loc, sm.from_status, sm.to_status,
			sm.reference, sm.notes,
			COALESCE(fl.location_name, sm.from_loc) AS from_location,
			COALESCE(tl.location_name, sm.to_loc) AS to_location,
			CONCAT(u.real_name) AS user_name
		FROM " . TB_PREF . "serial_movements sm
		LEFT JOIN " . TB_PREF . "serial_numbers sn ON sn.id = sm.serial_id
		LEFT JOIN " . TB_PREF . "stock_master si ON si.stock_id = sn.stock_id
		LEFT JOIN " . TB_PREF . "locations fl ON fl.loc_code = sm.from_loc
		LEFT JOIN " . TB_PREF . "locations tl ON tl.loc_code = sm.to_loc
		LEFT JOIN " . TB_PREF . "users u ON u.id = sm.created_by
		WHERE 1=1";

	if ($stock_id)
		$sql .= " AND sn.stock_id = " . db_escape($stock_id);
	if ($serial_no)
		$sql .= " AND sn.serial_no LIKE " . db_escape('%' . $serial_no . '%');
	if ($from_date)
		$sql .= " AND sm.tran_date >= '" . date2sql($from_date) . "'";
	if ($to_date)
		$sql .= " AND sm.tran_date <= '" . date2sql($to_date) . "'";
	if ($location)
		$sql .= " AND (sm.from_loc = " . db_escape($location) . " OR sm.to_loc = " . db_escape($location) . ")";

	$sql .= " ORDER BY sn.serial_no, sm.tran_date DESC, sm.id DESC";

	return db_query($sql, 'Could not retrieve serial ledger data');
}

/**
 * Get batch-wise stock balance by location.
 *
 * @param string|null $stock_id Filter by item
 * @param string|null $location Filter by location
 * @param int         $category Filter by category (0 = all)
 * @param string|null $as_of_date Balance as of date
 * @return resource DB result
 */
function get_batch_balance_data($stock_id = null, $location = null, $category = 0, $as_of_date = null)
{
	$sql = "SELECT b.id AS batch_id, b.batch_no, b.stock_id,
			si.description AS item_description,
			si.category_id, cat.description AS category_name,
			bm.loc_code, COALESCE(loc.location_name, bm.loc_code) AS location_name,
			SUM(bm.quantity) AS quantity_on_hand,
			b.manufacturing_date, b.expiry_date, b.status,
			(SUM(bm.quantity) * IF(b.batch_cost > 0, b.batch_cost, si.material_cost)) AS stock_value
		FROM " . TB_PREF . "stock_batches b
		INNER JOIN " . TB_PREF . "batch_movements bm ON bm.batch_id = b.id
		LEFT JOIN " . TB_PREF . "stock_master si ON si.stock_id = b.stock_id
		LEFT JOIN " . TB_PREF . "stock_category cat ON cat.category_id = si.category_id
		LEFT JOIN " . TB_PREF . "locations loc ON loc.loc_code = bm.loc_code
		WHERE b.inactive = 0";

	if ($stock_id)
		$sql .= " AND b.stock_id = " . db_escape($stock_id);
	if ($location && $location != 'all')
		$sql .= " AND bm.loc_code = " . db_escape($location);
	if ($category > 0)
		$sql .= " AND si.category_id = " . db_escape($category);

	$sql .= " GROUP BY b.id, b.batch_no, b.stock_id,
		si.description, si.category_id, cat.description,
		bm.loc_code, loc.location_name,
		b.manufacturing_date, b.expiry_date, b.status,
		b.batch_cost, si.material_cost
		HAVING quantity_on_hand != 0
		ORDER BY si.category_id, b.stock_id, bm.loc_code, b.batch_no";

	return db_query($sql, 'Could not retrieve batch balance data');
}

/**
 * Get batch aging analysis.
 *
 * @param string|null $stock_id Filter by item
 * @param string|null $location Filter by location
 * @param int         $category Filter by category (0 = all)
 * @return resource DB result
 */
function get_batch_aging_data($stock_id = null, $location = null, $category = 0, $as_of_date = null)
{
	$as_of_expr = $as_of_date ? ("'" . date2sql($as_of_date) . "'") : "CURDATE()";

	$sql = "SELECT b.id AS batch_id, b.batch_no, b.stock_id,
			si.description AS item_description,
			bm.loc_code, COALESCE(loc.location_name, bm.loc_code) AS location_name,
			SUM(bm.quantity) AS quantity_on_hand,
			b.manufacturing_date, b.expiry_date, b.status,
			b.created_at,
			DATEDIFF($as_of_expr, DATE(b.created_at)) AS days_in_stock,
			IF(b.expiry_date IS NOT NULL, DATEDIFF(b.expiry_date, $as_of_expr), NULL) AS days_until_expiry,
			(SUM(bm.quantity) * IF(b.batch_cost > 0, b.batch_cost, si.material_cost)) AS stock_value
		FROM " . TB_PREF . "stock_batches b
		INNER JOIN " . TB_PREF . "batch_movements bm ON bm.batch_id = b.id
		LEFT JOIN " . TB_PREF . "stock_master si ON si.stock_id = b.stock_id
		LEFT JOIN " . TB_PREF . "stock_category cat ON cat.category_id = si.category_id
		LEFT JOIN " . TB_PREF . "locations loc ON loc.loc_code = bm.loc_code
		WHERE b.inactive = 0";

	if ($stock_id)
		$sql .= " AND b.stock_id = " . db_escape($stock_id);
	if ($location && $location != 'all')
		$sql .= " AND bm.loc_code = " . db_escape($location);
	if ($category > 0)
		$sql .= " AND si.category_id = " . db_escape($category);

	$sql .= " GROUP BY b.id, b.batch_no, b.stock_id,
		si.description, bm.loc_code, loc.location_name,
		b.manufacturing_date, b.expiry_date, b.status, b.created_at,
		b.batch_cost, si.material_cost
		HAVING quantity_on_hand > 0
		ORDER BY days_in_stock DESC, b.stock_id, b.batch_no";

	return db_query($sql, 'Could not retrieve batch aging data');
}

/**
 * Get items approaching or past expiry.
 *
 * @param int         $days_threshold Show items expiring within N days (0 = all expired + expiring)
 * @param string|null $stock_id       Filter by item
 * @param string|null $location       Filter by location
 * @param int         $category       Filter by category (0 = all)
 * @param int         $include_expired 1 to include already expired items
 * @return resource DB result
 */
function get_expiry_report_data($days_threshold = 90, $stock_id = null, $location = null, $category = 0, $include_expired = 1, $as_of_date = null)
{
	$as_of_expr = $as_of_date ? ("'" . date2sql($as_of_date) . "'") : "CURDATE()";

	$sql = "SELECT b.id AS batch_id, b.batch_no, b.stock_id,
			si.description AS item_description,
			bm.loc_code, COALESCE(loc.location_name, bm.loc_code) AS location_name,
			SUM(bm.quantity) AS quantity_on_hand,
			b.expiry_date,
			DATEDIFF(b.expiry_date, $as_of_expr) AS days_until_expiry,
			b.status,
			(SUM(bm.quantity) * IF(b.batch_cost > 0, b.batch_cost, si.material_cost)) AS stock_value
		FROM " . TB_PREF . "stock_batches b
		INNER JOIN " . TB_PREF . "batch_movements bm ON bm.batch_id = b.id
		LEFT JOIN " . TB_PREF . "stock_master si ON si.stock_id = b.stock_id
		LEFT JOIN " . TB_PREF . "stock_category cat ON cat.category_id = si.category_id
		LEFT JOIN " . TB_PREF . "locations loc ON loc.loc_code = bm.loc_code
		WHERE b.inactive = 0
		AND b.expiry_date IS NOT NULL";

	if ($days_threshold > 0)
		$sql .= " AND b.expiry_date <= DATE_ADD($as_of_expr, INTERVAL " . (int)$days_threshold . " DAY)";
	if (!$include_expired)
		$sql .= " AND b.expiry_date >= $as_of_expr";
	if ($stock_id)
		$sql .= " AND b.stock_id = " . db_escape($stock_id);
	if ($location && $location != 'all')
		$sql .= " AND bm.loc_code = " . db_escape($location);
	if ($category > 0)
		$sql .= " AND si.category_id = " . db_escape($category);

	$sql .= " GROUP BY b.id, b.batch_no, b.stock_id,
		si.description, bm.loc_code, loc.location_name,
		b.expiry_date, b.status, b.batch_cost, si.material_cost
		HAVING quantity_on_hand > 0
		ORDER BY b.expiry_date ASC, b.stock_id, b.batch_no";

	return db_query($sql, 'Could not retrieve expiry report data');
}

/**
 * Get quality inspection summary statistics.
 *
 * @param string|null $from_date Start date
 * @param string|null $to_date   End date
 * @param string|null $stock_id  Filter by item
 * @param string|null $location  Filter by location
 * @return resource DB result
 */
function get_quality_summary_data($from_date = null, $to_date = null, $stock_id = null, $location = null)
{
	$sql = "SELECT qi.stock_id,
			si.description AS item_description,
			COUNT(*) AS total_inspections,
			SUM(CASE WHEN qi.result = 'pass' THEN 1 ELSE 0 END) AS passed,
			SUM(CASE WHEN qi.result = 'fail' THEN 1 ELSE 0 END) AS failed,
			SUM(CASE WHEN qi.result = 'pending' THEN 1 ELSE 0 END) AS pending,
			ROUND(SUM(CASE WHEN qi.result = 'pass' THEN 1 ELSE 0 END) * 100.0 / COUNT(*), 1) AS pass_rate
		FROM " . TB_PREF . "quality_inspections qi
		LEFT JOIN " . TB_PREF . "stock_master si ON si.stock_id = qi.stock_id
		WHERE 1=1";

	if ($from_date)
		$sql .= " AND qi.inspection_date >= '" . date2sql($from_date) . "'";
	if ($to_date)
		$sql .= " AND qi.inspection_date <= '" . date2sql($to_date) . "'";
	if ($stock_id)
		$sql .= " AND qi.stock_id = " . db_escape($stock_id);
	if ($location && $location != 'all')
		$sql .= " AND qi.loc_code = " . db_escape($location);

	$sql .= " GROUP BY qi.stock_id, si.description
		ORDER BY pass_rate ASC, total_inspections DESC";

	return db_query($sql, 'Could not retrieve quality summary data');
}

/**
 * Get warranty claims summary.
 *
 * @param string|null $from_date Start date
 * @param string|null $to_date   End date
 * @param string|null $stock_id  Filter by item
 * @return resource DB result
 */
function get_warranty_claims_summary_data($from_date = null, $to_date = null, $stock_id = null)
{
	$sql = "SELECT wc.stock_id,
			si.description AS item_description,
			COUNT(*) AS total_claims,
			SUM(CASE WHEN wc.status = 'open' THEN 1 ELSE 0 END) AS open_claims,
			SUM(CASE WHEN wc.status = 'in_progress' THEN 1 ELSE 0 END) AS in_progress,
			SUM(CASE WHEN wc.status = 'resolved' THEN 1 ELSE 0 END) AS resolved,
			SUM(CASE WHEN wc.status = 'rejected' THEN 1 ELSE 0 END) AS rejected,
			SUM(IFNULL(wc.repair_cost, 0)) AS total_repair_cost,
			SUM(CASE WHEN wc.replacement_serial_id IS NOT NULL THEN IFNULL(si.material_cost, 0) ELSE 0 END) AS total_replacement_cost
		FROM " . TB_PREF . "warranty_claims wc
		LEFT JOIN " . TB_PREF . "stock_master si ON si.stock_id = wc.stock_id
		WHERE 1=1";

	if ($from_date)
		$sql .= " AND wc.claim_date >= '" . date2sql($from_date) . "'";
	if ($to_date)
		$sql .= " AND wc.claim_date <= '" . date2sql($to_date) . "'";
	if ($stock_id)
		$sql .= " AND wc.stock_id = " . db_escape($stock_id);

	$sql .= " GROUP BY wc.stock_id, si.description
		ORDER BY total_claims DESC";

	return db_query($sql, 'Could not retrieve warranty claims summary data');
}

/**
 * Get serial warranty status listing.
 *
 * @param string|null $stock_id    Filter by item
 * @param int         $active_only 1 = only active warranties
 * @return resource DB result
 */
function get_warranty_status_data($stock_id = null, $active_only = 0)
{
	$sql = "SELECT sn.id AS serial_id, sn.serial_no, sn.stock_id,
			si.description AS item_description,
			sn.status AS serial_status,
			sn.warranty_start, sn.warranty_end, sn.warranty_type,
			sn.customer_id,
			d.name AS customer_name,
			DATEDIFF(sn.warranty_end, NOW()) AS days_remaining,
			CASE
				WHEN sn.warranty_end IS NULL THEN 'no_warranty'
				WHEN sn.warranty_end < CURDATE() THEN 'expired'
				WHEN DATEDIFF(sn.warranty_end, NOW()) <= 30 THEN 'expiring_soon'
				ELSE 'active'
			END AS warranty_status
		FROM " . TB_PREF . "serial_numbers sn
		LEFT JOIN " . TB_PREF . "stock_master si ON si.stock_id = sn.stock_id
		LEFT JOIN " . TB_PREF . "debtors_master d ON d.debtor_no = sn.customer_id
		WHERE sn.warranty_end IS NOT NULL";

	if ($active_only)
		$sql .= " AND sn.warranty_end >= CURDATE()";
	if ($stock_id)
		$sql .= " AND sn.stock_id = " . db_escape($stock_id);

	$sql .= " ORDER BY warranty_status ASC, sn.warranty_end ASC, sn.stock_id, sn.serial_no";

	return db_query($sql, 'Could not retrieve warranty status data');
}

/**
 * Get FEFO compliance analysis — finds cases where older batches were skipped.
 *
 * @param string|null $from_date Start date
 * @param string|null $to_date   End date
 * @param string|null $stock_id  Filter by item
 * @param string|null $location  Filter by location
 * @return resource DB result
 */
function get_fefo_compliance_data($from_date = null, $to_date = null, $stock_id = null, $location = null)
{
	// Find outbound movements (deliveries) where a batch with a later expiry was picked
	// while an earlier-expiring batch was still available
	$sql = "SELECT sm.tran_date, sm.stock_id,
			si.description AS item_description,
			sm.loc_code, loc.location_name,
			sb.batch_no AS picked_batch,
			sb.expiry_date AS picked_expiry,
			sm.qty AS qty_picked,
			sm.type AS trans_type, sm.trans_no,
			(SELECT MIN(sb2.expiry_date)
			 FROM " . TB_PREF . "stock_batches sb2
			 WHERE sb2.stock_id = sm.stock_id
			   AND sb2.expiry_date IS NOT NULL
			   AND sb2.inactive = 0
			   AND (SELECT COALESCE(SUM(bm2.quantity), 0)
					FROM " . TB_PREF . "batch_movements bm2
					WHERE bm2.batch_id = sb2.id
					  AND bm2.loc_code = sm.loc_code) > 0
			) AS oldest_available_expiry
		FROM " . TB_PREF . "stock_moves sm
		LEFT JOIN " . TB_PREF . "stock_master si ON si.stock_id = sm.stock_id
		LEFT JOIN " . TB_PREF . "locations loc ON loc.loc_code = sm.loc_code
		LEFT JOIN " . TB_PREF . "stock_batches sb ON sb.id = sm.batch_id
		WHERE sm.qty < 0
		AND sm.batch_id IS NOT NULL
		AND sb.expiry_date IS NOT NULL";

	if ($from_date)
		$sql .= " AND sm.tran_date >= '" . date2sql($from_date) . "'";
	if ($to_date)
		$sql .= " AND sm.tran_date <= '" . date2sql($to_date) . "'";
	if ($stock_id)
		$sql .= " AND sm.stock_id = " . db_escape($stock_id);
	if ($location && $location != 'all')
		$sql .= " AND sm.loc_code = " . db_escape($location);

	$sql .= " HAVING oldest_available_expiry IS NOT NULL 
		AND oldest_available_expiry < picked_expiry
		ORDER BY sm.tran_date DESC, sm.stock_id";

	return db_query($sql, 'Could not retrieve FEFO compliance data');
}

/**
 * Get cost tracking per serial number.
 *
 * @param string|null $stock_id Filter by item
 * @param string|null $location Filter by location
 * @return resource DB result
 */
function get_serial_cost_data($stock_id = null, $location = null)
{
	$sql = "SELECT sn.id AS serial_id, sn.serial_no, sn.stock_id,
			si.description AS item_description,
			sn.status, sn.loc_code, loc.location_name,
			sn.purchase_cost,
			IFNULL(sn.purchase_cost, si.material_cost) AS unit_cost,
			(SELECT COUNT(*) FROM " . TB_PREF . "serial_movements sm2 WHERE sm2.serial_id = sn.id) AS movement_count,
			sn.created_date,
			sn.purchase_date,
			sn.supplier_id,
			sup.supp_name AS supplier_name
		FROM " . TB_PREF . "serial_numbers sn
		LEFT JOIN " . TB_PREF . "stock_master si ON si.stock_id = sn.stock_id
		LEFT JOIN " . TB_PREF . "locations loc ON loc.loc_code = sn.loc_code
		LEFT JOIN " . TB_PREF . "suppliers sup ON sup.supplier_id = sn.supplier_id
		WHERE 1=1";

	if ($stock_id)
		$sql .= " AND sn.stock_id = " . db_escape($stock_id);
	if ($location && $location != 'all')
		$sql .= " AND sn.loc_code = " . db_escape($location);

	$sql .= " ORDER BY sn.stock_id, sn.serial_no";

	return db_query($sql, 'Could not retrieve serial cost data');
}

/**
 * Get supplier quality analysis — rejection rates by supplier.
 *
 * @param string|null $from_date Start date
 * @param string|null $to_date   End date
 * @param int|null    $supplier_id Filter by supplier
 * @return resource DB result
 */
function get_supplier_quality_data($from_date = null, $to_date = null, $supplier_id = null)
{
	$sql = "SELECT qi.supplier_id,
			sup.supp_name AS supplier_name,
			COUNT(*) AS total_inspections,
			SUM(CASE WHEN qi.result = 'pass' THEN 1 ELSE 0 END) AS passed,
			SUM(CASE WHEN qi.result = 'fail' THEN 1 ELSE 0 END) AS failed,
			SUM(CASE WHEN qi.result = 'conditional_pass' THEN 1 ELSE 0 END) AS conditional,
			ROUND(SUM(CASE WHEN qi.result = 'fail' THEN 1 ELSE 0 END) * 100.0 / COUNT(*), 1) AS rejection_rate,
			COUNT(DISTINCT qi.stock_id) AS unique_items
		FROM " . TB_PREF . "quality_inspections qi
		LEFT JOIN " . TB_PREF . "suppliers sup ON sup.supplier_id = qi.supplier_id
		WHERE qi.supplier_id IS NOT NULL AND qi.supplier_id > 0";

	if ($from_date)
		$sql .= " AND qi.inspection_date >= '" . date2sql($from_date) . "'";
	if ($to_date)
		$sql .= " AND qi.inspection_date <= '" . date2sql($to_date) . "'";
	if ($supplier_id && $supplier_id > 0)
		$sql .= " AND qi.supplier_id = " . db_escape($supplier_id);

	$sql .= " GROUP BY qi.supplier_id, sup.supp_name
		ORDER BY rejection_rate DESC, total_inspections DESC";

	return db_query($sql, 'Could not retrieve supplier quality data');
}
