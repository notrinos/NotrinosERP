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

include_once($path_to_root . '/includes/date_functions.inc');
include_once($path_to_root . '/includes/hooks.inc');

/**
 * Normalize one date value into SQL date format.
 *
 * @param string $date_value
 * @param string $fallback
 * @return string
 */
function normalize_purchase_analytics_sql_date($date_value, $fallback = '')
{
	if ($date_value === '' || $date_value === null)
		return $fallback;

	if (preg_match('/^\d{4}-\d{2}-\d{2}$/', $date_value))
		return $date_value;

	return is_date($date_value) ? date2sql($date_value) : $fallback;
}

/**
 * Build a default analytics date range.
 *
 * @param string $date_from
 * @param string $date_to
 * @return array
 */
function get_purchase_analytics_range($date_from = '', $date_to = '')
{
	$default_from = date2sql(begin_fiscalyear());
	$default_to = date2sql(Today());

	$sql_date_from = normalize_purchase_analytics_sql_date($date_from, $default_from);
	$sql_date_to = normalize_purchase_analytics_sql_date($date_to, $default_to);

	if (strtotime($sql_date_from) > strtotime($sql_date_to)) {
		$temp = $sql_date_from;
		$sql_date_from = $sql_date_to;
		$sql_date_to = $temp;
	}

	return array($sql_date_from, $sql_date_to);
}

/**
 * Check whether one table exists in current company database.
 *
 * @param string $table_name
 * @return bool
 */
function purchase_analytics_table_exists($table_name)
{
	$sql = "SHOW TABLES LIKE " . db_escape($table_name);
	$result = db_query($sql, 'could not check analytics table existence');
	return db_num_rows($result) > 0;
}

/**
 * Check whether one column exists in a table.
 *
 * @param string $table_name
 * @param string $column_name
 * @return bool
 */
function purchase_analytics_column_exists($table_name, $column_name)
{
	$sql = "SHOW COLUMNS FROM " . $table_name . " LIKE " . db_escape($column_name);
	$result = db_query($sql, 'could not check analytics column existence');
	return $result && db_num_rows($result) > 0;
}

/**
 * Execute scalar analytics query.
 *
 * @param string $sql
 * @param string $error_message
 * @return float
 */
function purchase_analytics_scalar($sql, $error_message)
{
	$result = db_query($sql, $error_message);
	$row = db_fetch_row($result);
	if (!$row)
		return 0;

	return (float)$row[0];
}

/**
 * Build previous-period range from current range.
 *
 * @param string $date_from
 * @param string $date_to
 * @return array
 */
function get_previous_purchase_analytics_range($date_from, $date_to)
{
	$from_ts = strtotime($date_from);
	$to_ts = strtotime($date_to);
	$day_count = (int)floor(($to_ts - $from_ts) / 86400) + 1;
	if ($day_count < 1)
		$day_count = 1;

	$prev_to = date('Y-m-d', strtotime($date_from . ' -1 day'));
	$prev_from = date('Y-m-d', strtotime($prev_to . ' -' . ($day_count - 1) . ' days'));

	return array($prev_from, $prev_to);
}

/**
 * Get total purchase spend for one period.
 *
 * @param string $date_from
 * @param string $date_to
 * @return float
 */
function get_purchase_spend_total($date_from, $date_to)
{
	$sql = "SELECT COALESCE(SUM(ABS(trans.ov_amount + trans.ov_gst + trans.ov_discount)), 0)
		FROM " . TB_PREF . "supp_trans trans
		LEFT JOIN " . TB_PREF . "voided v ON v.type = trans.type AND v.id = trans.trans_no
		WHERE trans.type = " . ST_SUPPINVOICE . "
			AND v.id IS NULL
			AND trans.tran_date >= " . db_escape($date_from) . "
			AND trans.tran_date <= " . db_escape($date_to);

	return purchase_analytics_scalar($sql, 'could not get purchase spend total');
}

/**
 * Return purchase dashboard metrics and datasets.
 *
 * @param string $date_from
 * @param string $date_to
 * @return array
 */
function get_purchase_dashboard_data($date_from, $date_to)
{
	list($sql_date_from, $sql_date_to) = get_purchase_analytics_range($date_from, $date_to);
	list($prev_from, $prev_to) = get_previous_purchase_analytics_range($sql_date_from, $sql_date_to);

	$total_spend = get_purchase_spend_total($sql_date_from, $sql_date_to);
	$previous_spend = get_purchase_spend_total($prev_from, $prev_to);

	$po_total_sql = "SELECT COALESCE(SUM(line.quantity_ordered * line.unit_price), 0)
		FROM " . TB_PREF . "purch_orders po
		INNER JOIN " . TB_PREF . "purch_order_details line ON line.order_no = po.order_no
		WHERE po.ord_date >= " . db_escape($sql_date_from) . "
			AND po.ord_date <= " . db_escape($sql_date_to);
	$po_total_value = purchase_analytics_scalar($po_total_sql, 'could not get purchase order totals');

	$po_count_sql = "SELECT COUNT(*)
		FROM " . TB_PREF . "purch_orders po
		WHERE po.ord_date >= " . db_escape($sql_date_from) . "
			AND po.ord_date <= " . db_escape($sql_date_to);
	$po_count = (int)purchase_analytics_scalar($po_count_sql, 'could not count purchase orders');

	$open_po_value_sql = "SELECT COALESCE(SUM((line.quantity_ordered - line.quantity_received) * line.unit_price), 0)
		FROM " . TB_PREF . "purch_order_details line
		WHERE line.quantity_ordered > line.quantity_received";
	$open_po_value = purchase_analytics_scalar($open_po_value_sql, 'could not compute open purchase order value');

	$pending_grn_count_sql = "SELECT COUNT(*)
		FROM " . TB_PREF . "purch_order_details line
		WHERE line.quantity_ordered > line.quantity_received";
	$pending_grn_count = (int)purchase_analytics_scalar($pending_grn_count_sql, 'could not count pending GRN lines');
	$pending_grn_value = $open_po_value;

	$pending_invoice_count = 0;
	$pending_invoice_value = 0;
	if (purchase_analytics_table_exists(TB_PREF . 'grn_items')) {
		$pending_invoice_count_sql = "SELECT COUNT(*)
			FROM " . TB_PREF . "grn_items item
			WHERE item.qty_recd > item.quantity_inv";
		$pending_invoice_count = (int)purchase_analytics_scalar($pending_invoice_count_sql, 'could not count pending invoice GRN lines');

		$pending_invoice_value_sql = "SELECT COALESCE(SUM((item.qty_recd - item.quantity_inv) * line.unit_price), 0)
			FROM " . TB_PREF . "grn_items item
			LEFT JOIN " . TB_PREF . "purch_order_details line ON line.po_detail_item = item.po_detail_item
			WHERE item.qty_recd > item.quantity_inv";
		$pending_invoice_value = purchase_analytics_scalar($pending_invoice_value_sql, 'could not calculate pending invoice value');
	}

	$matching_exceptions_count = 0;
	if (purchase_analytics_table_exists(TB_PREF . 'purch_matching_exceptions')) {
		$matching_exceptions_count_sql = "SELECT COUNT(*)
			FROM " . TB_PREF . "purch_matching_exceptions
			WHERE status = 'open'";
		$matching_exceptions_count = (int)purchase_analytics_scalar($matching_exceptions_count_sql, 'could not count purchase matching exceptions');
	}

	$reorder_alerts_count = 0;
	if (function_exists('evaluate_replenishment_rules')) {
		$reorder_suggestions = evaluate_replenishment_rules(null, null);
		$reorder_alerts_count = is_array($reorder_suggestions) ? count($reorder_suggestions) : 0;
	} else {
		$reorder_alerts_count_sql = "SELECT COUNT(*)
			FROM " . TB_PREF . "loc_stock
			WHERE reorder_level > 0";
		$reorder_alerts_count = (int)purchase_analytics_scalar($reorder_alerts_count_sql, 'could not count reorder alerts');
	}

	$lead_time_average_days = 0;
	if (purchase_analytics_table_exists(TB_PREF . 'grn_batch')) {
		$lead_time_sql = "SELECT COALESCE(AVG(DATEDIFF(grn.delivery_date, po.ord_date)), 0)
			FROM " . TB_PREF . "grn_batch grn
			INNER JOIN " . TB_PREF . "purch_orders po ON po.order_no = grn.purch_order_no
			WHERE grn.delivery_date >= " . db_escape($sql_date_from) . "
				AND grn.delivery_date <= " . db_escape($sql_date_to) . "
				AND po.ord_date <> '0000-00-00'";
		$lead_time_average_days = purchase_analytics_scalar($lead_time_sql, 'could not calculate average lead time');
	}

	$dashboard_data = array(
		'date_from' => $sql_date_from,
		'date_to' => $sql_date_to,
		'previous_date_from' => $prev_from,
		'previous_date_to' => $prev_to,
		'total_spend' => $total_spend,
		'previous_spend' => $previous_spend,
		'spend_change_pct' => $previous_spend > 0 ? (($total_spend - $previous_spend) / $previous_spend) * 100 : 0,
		'po_count' => $po_count,
		'avg_po_value' => $po_count > 0 ? ($po_total_value / $po_count) : 0,
		'open_po_value' => $open_po_value,
		'pending_grn_count' => $pending_grn_count,
		'pending_grn_value' => $pending_grn_value,
		'pending_invoice_count' => $pending_invoice_count,
		'pending_invoice_value' => $pending_invoice_value,
		'matching_exceptions' => $matching_exceptions_count,
		'reorder_alerts' => $reorder_alerts_count,
		'lead_time_average_days' => $lead_time_average_days,
		'top_vendors' => get_vendor_spend_analysis($sql_date_from, $sql_date_to, 10),
		'top_items' => get_category_spend_analysis($sql_date_from, $sql_date_to),
		'spend_trend' => get_spend_trend('monthly', 12),
	);

	hook_invoke_all('purch_dashboard_data', $dashboard_data);
	return $dashboard_data;
}

/**
 * Get spend trend rows.
 *
 * @param string $period
 * @param int $months
 * @return array
 */
function get_spend_trend($period = 'monthly', $months = 12)
{
	$months = (int)$months;
	if ($months < 1)
		$months = 1;

	$date_from = date('Y-m-01', strtotime('-' . ($months - 1) . ' months'));
	$sql = "SELECT DATE_FORMAT(trans.tran_date, '%Y-%m') AS period_key,
			COALESCE(SUM(ABS(trans.ov_amount + trans.ov_gst + trans.ov_discount)), 0) AS period_spend
		FROM " . TB_PREF . "supp_trans trans
		LEFT JOIN " . TB_PREF . "voided v ON v.type = trans.type AND v.id = trans.trans_no
		WHERE trans.type = " . ST_SUPPINVOICE . "
			AND v.id IS NULL
			AND trans.tran_date >= " . db_escape($date_from) . "
		GROUP BY DATE_FORMAT(trans.tran_date, '%Y-%m')
		ORDER BY period_key ASC";

	$result = db_query($sql, 'could not get spend trend data');
	$rows = array();
	while ($row = db_fetch($result))
		$rows[] = $row;

	return $rows;
}

/**
 * Get vendor spend analysis rows.
 *
 * @param string $date_from
 * @param string $date_to
 * @param int $top_n
 * @return array
 */
function get_vendor_spend_analysis($date_from, $date_to, $top_n = 20)
{
	list($sql_date_from, $sql_date_to) = get_purchase_analytics_range($date_from, $date_to);
	$top_n = (int)$top_n;
	if ($top_n < 1)
		$top_n = 20;

	$sql = "SELECT trans.supplier_id,
			supp.supp_name,
			COUNT(*) AS invoice_count,
			COALESCE(SUM(ABS(trans.ov_amount + trans.ov_gst + trans.ov_discount)), 0) AS total_spend
		FROM " . TB_PREF . "supp_trans trans
		INNER JOIN " . TB_PREF . "suppliers supp ON supp.supplier_id = trans.supplier_id
		LEFT JOIN " . TB_PREF . "voided v ON v.type = trans.type AND v.id = trans.trans_no
		WHERE trans.type = " . ST_SUPPINVOICE . "
			AND v.id IS NULL
			AND trans.tran_date >= " . db_escape($sql_date_from) . "
			AND trans.tran_date <= " . db_escape($sql_date_to) . "
		GROUP BY trans.supplier_id, supp.supp_name
		ORDER BY total_spend DESC
		LIMIT " . $top_n;

	$result = db_query($sql, 'could not get vendor spend analysis');
	$rows = array();
	while ($row = db_fetch($result))
		$rows[] = $row;

	return $rows;
}

/**
 * Get spend analysis by purchased items.
 *
 * @param string $date_from
 * @param string $date_to
 * @return array
 */
function get_category_spend_analysis($date_from, $date_to)
{
	if (!purchase_analytics_table_exists(TB_PREF . 'supp_invoice_items'))
		return array();

	list($sql_date_from, $sql_date_to) = get_purchase_analytics_range($date_from, $date_to);
	$sql = "SELECT item.stock_id,
			COALESCE(item.description, stock.description, item.stock_id) AS item_description,
			COALESCE(SUM(item.quantity), 0) AS total_quantity,
			COALESCE(SUM(item.quantity * item.unit_price), 0) AS total_spend
		FROM " . TB_PREF . "supp_invoice_items item
		INNER JOIN " . TB_PREF . "supp_trans trans ON trans.trans_no = item.supp_trans_no AND trans.type = item.supp_trans_type
		LEFT JOIN " . TB_PREF . "stock_master stock ON stock.stock_id = item.stock_id
		LEFT JOIN " . TB_PREF . "voided v ON v.type = trans.type AND v.id = trans.trans_no
		WHERE trans.type = " . ST_SUPPINVOICE . "
			AND v.id IS NULL
			AND trans.tran_date >= " . db_escape($sql_date_from) . "
			AND trans.tran_date <= " . db_escape($sql_date_to) . "
		GROUP BY item.stock_id, COALESCE(item.description, stock.description, item.stock_id)
		ORDER BY total_spend DESC
		LIMIT 10";

	$result = db_query($sql, 'could not get item spend analysis');
	$rows = array();
	while ($row = db_fetch($result))
		$rows[] = $row;

	return $rows;
}

/**
 * Get lead time analysis by supplier.
 *
 * @param string $date_from
 * @param string $date_to
 * @param int $supplier_id
 * @return array
 */
function get_lead_time_analysis($date_from, $date_to, $supplier_id = 0)
{
	if (!purchase_analytics_table_exists(TB_PREF . 'grn_batch'))
		return array();

	list($sql_date_from, $sql_date_to) = get_purchase_analytics_range($date_from, $date_to);

	$sql = "SELECT grn.supplier_id,
			supp.supp_name,
			COUNT(*) AS receipt_count,
			COALESCE(AVG(DATEDIFF(grn.delivery_date, po.ord_date)), 0) AS avg_lead_days,
			COALESCE(MIN(DATEDIFF(grn.delivery_date, po.ord_date)), 0) AS min_lead_days,
			COALESCE(MAX(DATEDIFF(grn.delivery_date, po.ord_date)), 0) AS max_lead_days
		FROM " . TB_PREF . "grn_batch grn
		INNER JOIN " . TB_PREF . "purch_orders po ON po.order_no = grn.purch_order_no
		LEFT JOIN " . TB_PREF . "suppliers supp ON supp.supplier_id = grn.supplier_id
		WHERE grn.delivery_date >= " . db_escape($sql_date_from) . "
			AND grn.delivery_date <= " . db_escape($sql_date_to);

	if ((int)$supplier_id > 0)
		$sql .= " AND grn.supplier_id = " . db_escape((int)$supplier_id);

	$sql .= " GROUP BY grn.supplier_id, supp.supp_name
		ORDER BY avg_lead_days ASC";

	$result = db_query($sql, 'could not get lead time analysis');
	$rows = array();
	while ($row = db_fetch($result))
		$rows[] = $row;

	return $rows;
}

/**
 * Get price variance report rows against supplier purchase baseline.
 *
 * @param string $date_from
 * @param string $date_to
 * @return array
 */
function get_price_variance_report($date_from, $date_to)
{
	if (!purchase_analytics_table_exists(TB_PREF . 'supp_invoice_items'))
		return array();

	list($sql_date_from, $sql_date_to) = get_purchase_analytics_range($date_from, $date_to);
	$sql = "SELECT trans.supplier_id,
			supp.supp_name,
			item.stock_id,
			COALESCE(stock.description, item.description, item.stock_id) AS item_description,
			AVG(item.unit_price) AS avg_actual_price,
			AVG(COALESCE(pdata.price, item.unit_price)) AS avg_reference_price,
			AVG(item.unit_price - COALESCE(pdata.price, item.unit_price)) AS avg_variance_amount,
			AVG(CASE WHEN COALESCE(pdata.price, 0) = 0 THEN 0 ELSE ((item.unit_price - pdata.price) / pdata.price) * 100 END) AS avg_variance_pct,
			SUM(item.quantity) AS total_quantity
		FROM " . TB_PREF . "supp_invoice_items item
		INNER JOIN " . TB_PREF . "supp_trans trans ON trans.trans_no = item.supp_trans_no AND trans.type = item.supp_trans_type
		LEFT JOIN " . TB_PREF . "suppliers supp ON supp.supplier_id = trans.supplier_id
		LEFT JOIN " . TB_PREF . "stock_master stock ON stock.stock_id = item.stock_id
		LEFT JOIN " . TB_PREF . "purch_data pdata ON pdata.supplier_id = trans.supplier_id AND pdata.stock_id = item.stock_id
		LEFT JOIN " . TB_PREF . "voided v ON v.type = trans.type AND v.id = trans.trans_no
		WHERE trans.type = " . ST_SUPPINVOICE . "
			AND v.id IS NULL
			AND trans.tran_date >= " . db_escape($sql_date_from) . "
			AND trans.tran_date <= " . db_escape($sql_date_to) . "
		GROUP BY trans.supplier_id, supp.supp_name, item.stock_id, COALESCE(stock.description, item.description, item.stock_id)
		ORDER BY ABS(avg_variance_amount) DESC
		LIMIT 200";

	$result = db_query($sql, 'could not get purchase price variance report');
	$rows = array();
	while ($row = db_fetch($result))
		$rows[] = $row;

	return $rows;
}

/**
 * Get purchase savings report summary.
 *
 * @param string $date_from
 * @param string $date_to
 * @return array
 */
function get_purchase_savings_report($date_from, $date_to)
{
	$variance_rows = get_price_variance_report($date_from, $date_to);
	$total_variance = 0;
	$total_savings = 0;
	$total_overspend = 0;

	foreach ($variance_rows as $row) {
		$line_variance = (float)$row['avg_variance_amount'] * (float)$row['total_quantity'];
		$total_variance += $line_variance;
		if ($line_variance < 0)
			$total_savings += abs($line_variance);
		elseif ($line_variance > 0)
			$total_overspend += $line_variance;
	}

	return array(
		'total_variance' => $total_variance,
		'total_savings' => $total_savings,
		'total_overspend' => $total_overspend,
		'line_count' => count($variance_rows),
	);
}

/**
 * Compare vendors for one item over a period.
 *
 * @param string $stock_id
 * @param string $date_from
 * @param string $date_to
 * @return array
 */
function get_vendor_comparison_report($stock_id, $date_from, $date_to)
{
	if (!purchase_analytics_table_exists(TB_PREF . 'supp_invoice_items') || $stock_id === '')
		return array();

	list($sql_date_from, $sql_date_to) = get_purchase_analytics_range($date_from, $date_to);
	$sql = "SELECT trans.supplier_id,
			supp.supp_name,
			COUNT(*) AS invoice_line_count,
			SUM(item.quantity) AS total_quantity,
			AVG(item.unit_price) AS avg_price,
			MIN(item.unit_price) AS min_price,
			MAX(item.unit_price) AS max_price
		FROM " . TB_PREF . "supp_invoice_items item
		INNER JOIN " . TB_PREF . "supp_trans trans ON trans.trans_no = item.supp_trans_no AND trans.type = item.supp_trans_type
		LEFT JOIN " . TB_PREF . "suppliers supp ON supp.supplier_id = trans.supplier_id
		LEFT JOIN " . TB_PREF . "voided v ON v.type = trans.type AND v.id = trans.trans_no
		WHERE trans.type = " . ST_SUPPINVOICE . "
			AND v.id IS NULL
			AND item.stock_id = " . db_escape($stock_id) . "
			AND trans.tran_date >= " . db_escape($sql_date_from) . "
			AND trans.tran_date <= " . db_escape($sql_date_to) . "
		GROUP BY trans.supplier_id, supp.supp_name
		ORDER BY avg_price ASC";

	$result = db_query($sql, 'could not compare vendors for item');
	$rows = array();
	while ($row = db_fetch($result))
		$rows[] = $row;

	return $rows;
}

/**
 * Calculate procurement efficiency summary metrics.
 *
 * @param string $date_from
 * @param string $date_to
 * @return array
 */
function get_procurement_efficiency_report($date_from, $date_to)
{
	list($sql_date_from, $sql_date_to) = get_purchase_analytics_range($date_from, $date_to);

	$po_to_grn_days = 0;
	if (purchase_analytics_table_exists(TB_PREF . 'grn_batch')) {
		$sql = "SELECT COALESCE(AVG(DATEDIFF(grn.delivery_date, po.ord_date)), 0)
			FROM " . TB_PREF . "grn_batch grn
			INNER JOIN " . TB_PREF . "purch_orders po ON po.order_no = grn.purch_order_no
			WHERE grn.delivery_date >= " . db_escape($sql_date_from) . "
				AND grn.delivery_date <= " . db_escape($sql_date_to);
		$po_to_grn_days = purchase_analytics_scalar($sql, 'could not calculate PO to GRN cycle time');
	}

	$grn_to_invoice_days = 0;
	if (purchase_analytics_table_exists(TB_PREF . 'grn_items') && purchase_analytics_table_exists(TB_PREF . 'supp_invoice_items')) {
		$sql = "SELECT COALESCE(AVG(DATEDIFF(trans.tran_date, grn.delivery_date)), 0)
			FROM " . TB_PREF . "supp_invoice_items item
			INNER JOIN " . TB_PREF . "supp_trans trans ON trans.trans_no = item.supp_trans_no AND trans.type = item.supp_trans_type
			INNER JOIN " . TB_PREF . "grn_items gitem ON gitem.id = item.grn_item_id
			INNER JOIN " . TB_PREF . "grn_batch grn ON grn.id = gitem.grn_batch_id
			WHERE trans.type = " . ST_SUPPINVOICE . "
				AND trans.tran_date >= " . db_escape($sql_date_from) . "
				AND trans.tran_date <= " . db_escape($sql_date_to);
		$grn_to_invoice_days = purchase_analytics_scalar($sql, 'could not calculate GRN to invoice cycle time');
	}

	$requisition_to_po_days = 0;
	if (purchase_analytics_table_exists(TB_PREF . 'purch_requisitions') && purchase_analytics_column_exists(TB_PREF . 'purch_orders', 'requisition_id')) {
		$sql = "SELECT COALESCE(AVG(DATEDIFF(po.ord_date, req.request_date)), 0)
			FROM " . TB_PREF . "purch_orders po
			INNER JOIN " . TB_PREF . "purch_requisitions req ON req.id = po.requisition_id
			WHERE po.ord_date >= " . db_escape($sql_date_from) . "
				AND po.ord_date <= " . db_escape($sql_date_to);
		$requisition_to_po_days = purchase_analytics_scalar($sql, 'could not calculate requisition to PO cycle time');
	}

	return array(
		'requisition_to_po_days' => $requisition_to_po_days,
		'po_to_grn_days' => $po_to_grn_days,
		'grn_to_invoice_days' => $grn_to_invoice_days,
	);
}

/**
 * Compare budget and actual purchase-related expenses.
 *
 * @param string $date_from
 * @param string $date_to
 * @param int $dimension_id
 * @return array
 */
function get_purchase_budget_variance($date_from, $date_to, $dimension_id = 0)
{
	list($sql_date_from, $sql_date_to) = get_purchase_analytics_range($date_from, $date_to);
	$dimension_id = (int)$dimension_id;

	$actual_sql = "SELECT COALESCE(SUM(ABS(trans.ov_amount + trans.ov_gst + trans.ov_discount)), 0)
		FROM " . TB_PREF . "supp_trans trans
		LEFT JOIN " . TB_PREF . "voided v ON v.type = trans.type AND v.id = trans.trans_no
		WHERE trans.type = " . ST_SUPPINVOICE . "
			AND v.id IS NULL
			AND trans.tran_date >= " . db_escape($sql_date_from) . "
			AND trans.tran_date <= " . db_escape($sql_date_to);
	$actual_amount = purchase_analytics_scalar($actual_sql, 'could not compute purchase actual amount');

	$budget_amount = 0;
	if (purchase_analytics_table_exists(TB_PREF . 'budget_trans')) {
		$budget_sql = "SELECT COALESCE(SUM(amount), 0)
			FROM " . TB_PREF . "budget_trans
			WHERE tran_date >= " . db_escape($sql_date_from) . "
				AND tran_date <= " . db_escape($sql_date_to);
		if ($dimension_id > 0)
			$budget_sql .= " AND (dimension_id = " . db_escape($dimension_id) . " OR dimension2_id = " . db_escape($dimension_id) . ")";

		$budget_amount = purchase_analytics_scalar($budget_sql, 'could not compute purchase budget amount');
	}

	return array(
		'actual_amount' => $actual_amount,
		'budget_amount' => $budget_amount,
		'variance_amount' => $actual_amount - $budget_amount,
		'variance_pct' => $budget_amount != 0 ? (($actual_amount - $budget_amount) / $budget_amount) * 100 : 0,
	);
}

/**
 * Refresh purchase KPI cache table.
 *
 * @return bool
 */
function refresh_purch_kpi_cache()
{
	if (!purchase_analytics_table_exists(TB_PREF . 'purch_kpi_cache'))
		return false;

	$dashboard_data = get_purchase_dashboard_data(date2sql(begin_fiscalyear()), date2sql(Today()));
	$kpis = array(
		'monthly_spend' => array('value_1' => (float)$dashboard_data['total_spend']),
		'open_po_value' => array('value_1' => (float)$dashboard_data['open_po_value']),
		'pending_grn' => array('value_1' => (float)$dashboard_data['pending_grn_count'], 'value_2' => (float)$dashboard_data['pending_grn_value']),
		'pending_invoice' => array('value_1' => (float)$dashboard_data['pending_invoice_count'], 'value_2' => (float)$dashboard_data['pending_invoice_value']),
		'matching_exceptions' => array('value_1' => (float)$dashboard_data['matching_exceptions']),
	);

	hook_invoke_all('purch_kpi_refresh', $kpis);

	$period_key = date('Y-m');
	$updated_at = date('Y-m-d H:i:s');

	foreach ($kpis as $kpi_type => $kpi_values) {
		$sql = "INSERT INTO " . TB_PREF . "purch_kpi_cache
			(kpi_type, period_key, dimension_key, value_1, value_2, value_3, detail_json, updated_at)
			VALUES (
				" . db_escape($kpi_type) . ",
				" . db_escape($period_key) . ",
				'',
				" . db_escape(isset($kpi_values['value_1']) ? $kpi_values['value_1'] : 0) . ",
				" . db_escape(isset($kpi_values['value_2']) ? $kpi_values['value_2'] : 0) . ",
				" . db_escape(isset($kpi_values['value_3']) ? $kpi_values['value_3'] : 0) . ",
				" . db_escape(json_encode($kpi_values)) . ",
				" . db_escape($updated_at) . "
			)
			ON DUPLICATE KEY UPDATE
				value_1 = VALUES(value_1),
				value_2 = VALUES(value_2),
				value_3 = VALUES(value_3),
				detail_json = VALUES(detail_json),
				updated_at = VALUES(updated_at)";
		db_query($sql, 'could not refresh purchase KPI cache row');
	}

	return true;
}
