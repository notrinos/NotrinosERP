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
include_once($path_to_root . '/purchasing/includes/po_class.inc');
include_once($path_to_root . '/purchasing/includes/db/po_db.inc');

/**
 * Generate the next procurement plan reference.
 *
 * @param int $plan_id
 * @return string
 */
function format_procurement_plan_reference($plan_id)
{
	return 'PPLAN-' . str_pad((int)$plan_id, 5, '0', STR_PAD_LEFT);
}

/**
 * Return the procurement plan status label map.
 *
 * @return array
 */
function get_procurement_plan_statuses()
{
	return array(
		'draft' => _('Draft'),
		'confirmed' => _('Confirmed'),
		'in_progress' => _('In Progress'),
		'completed' => _('Completed'),
		'cancelled' => _('Cancelled'),
	);
}

/**
 * Return the procurement line status label map.
 *
 * @return array
 */
function get_procurement_plan_line_statuses()
{
	return array(
		'pending' => _('Pending'),
		'approved' => _('Approved'),
		'ordered' => _('Ordered'),
		'skipped' => _('Skipped'),
	);
}

/**
 * Convert arbitrary custom data to JSON string.
 *
 * @param mixed $custom_data
 * @return string|null
 */
function encode_procurement_plan_custom_data($custom_data)
{
	if ($custom_data === null || $custom_data === '' || $custom_data === array())
		return null;

	if (is_string($custom_data))
		return $custom_data;

	$encoded = json_encode($custom_data);
	return $encoded === false ? null : $encoded;
}

/**
 * Build an SQL-safe JSON literal.
 *
 * @param mixed $custom_data
 * @return string
 */
function sql_procurement_plan_custom_data($custom_data)
{
	$encoded = encode_procurement_plan_custom_data($custom_data);
	return $encoded === null ? 'NULL' : "'" . db_escape_function($encoded) . "'";
}

/**
 * Generate one procurement plan from current replenishment suggestions.
 *
 * @param string $plan_type
 * @param string $location
 * @param mixed  $date_range
 * @return int|false
 */
function generate_procurement_plan($plan_type, $location = '', $date_range = null)
{
	$valid_plan_types = array('auto_reorder', 'demand_based', 'manual');
	if (!in_array($plan_type, $valid_plan_types))
		$plan_type = 'auto_reorder';

	$suggestions = evaluate_replenishment_rules($location !== '' ? $location : null, null);
	if (empty($suggestions))
		return false;

	begin_transaction();

	$sql = "INSERT INTO " . TB_PREF . "procurement_plan
		(reference, plan_date, plan_type, status, created_by, notes, custom_data)
		VALUES ('', " . db_escape(date2sql(Today())) . ", " . db_escape($plan_type) . ", 'draft', "
		. (int)$_SESSION['wa_current_user']->user . ", '', " . sql_procurement_plan_custom_data($date_range) . ")";
	db_query($sql, 'could not create procurement plan header');

	$plan_id = db_insert_id();
	if (!$plan_id) {
		cancel_transaction();
		return false;
	}

	$reference = format_procurement_plan_reference($plan_id);
	$sql = "UPDATE " . TB_PREF . "procurement_plan SET reference=" . db_escape($reference)
		. " WHERE id=" . (int)$plan_id;
	db_query($sql, 'could not update procurement plan reference');

	$line_count = 0;
	foreach ($suggestions as $suggestion) {
		if ((float)$suggestion['suggested_qty'] <= 0)
			continue;

		$supplier_id = resolve_reorder_suggestion_supplier_id($suggestion);
		$pricing_context = get_best_vendor_price(
			$suggestion['stock_id'],
			(float)$suggestion['suggested_qty'],
			'',
			date2sql(Today()),
			$supplier_id
		);

		$estimated_price = isset($pricing_context['price']) ? (float)$pricing_context['price'] : 0;
		$estimated_lead_time = isset($pricing_context['lead_time_days']) ? (int)$pricing_context['lead_time_days'] : 0;

		$line_custom_data = array(
			'rule_id' => isset($suggestion['rule_id']) ? (int)$suggestion['rule_id'] : 0,
			'rule_name' => isset($suggestion['rule_name']) ? $suggestion['rule_name'] : '',
			'reason' => isset($suggestion['reason']) ? $suggestion['reason'] : '',
			'priority' => isset($suggestion['priority']) ? $suggestion['priority'] : 'normal',
			'price_source' => isset($pricing_context['source']) ? $pricing_context['source'] : '',
		);

		$sql = "INSERT INTO " . TB_PREF . "procurement_plan_lines
			(plan_id, stock_id, location, replenishment_rule_id, current_stock, required_qty,
			 suggested_order_qty, supplier_id, estimated_price, estimated_lead_time, priority, status, po_number, custom_data)
			VALUES ("
			. (int)$plan_id . ","
			. db_escape($suggestion['stock_id']) . ","
			. db_escape($suggestion['warehouse_loc_code']) . ","
			. (int)$line_custom_data['rule_id'] . ","
			. db_escape(isset($suggestion['current_qty']) ? (float)$suggestion['current_qty'] : 0) . ","
			. db_escape(isset($suggestion['outgoing_qty']) ? (float)$suggestion['outgoing_qty'] : 0) . ","
			. db_escape((float)$suggestion['suggested_qty']) . ","
			. db_escape((int)$supplier_id) . ","
			. db_escape($estimated_price) . ","
			. db_escape($estimated_lead_time) . ","
			. db_escape(isset($suggestion['priority']) ? $suggestion['priority'] : 'normal') . ","
			. "'pending',0,"
			. sql_procurement_plan_custom_data($line_custom_data)
			. ")";
		db_query($sql, 'could not insert procurement plan line');
		$line_count++;
	}

	if ($line_count === 0) {
		$sql = "DELETE FROM " . TB_PREF . "procurement_plan WHERE id=" . (int)$plan_id;
		db_query($sql, 'could not rollback empty procurement plan');
		cancel_transaction();
		return false;
	}

	commit_transaction();
	hook_invoke_all('procurement_plan_generated', $plan_id);
	return $plan_id;
}

/**
 * Retrieve one procurement plan header.
 *
 * @param int $id
 * @return array|false
 */
function get_procurement_plan($id)
{
	$sql = "SELECT plan.*,
		user.real_name AS created_by_name,
		COUNT(line.id) AS line_count,
		SUM(CASE WHEN line.status = 'pending' THEN 1 ELSE 0 END) AS pending_count,
		SUM(CASE WHEN line.status = 'approved' THEN 1 ELSE 0 END) AS approved_count,
		SUM(CASE WHEN line.status = 'ordered' THEN 1 ELSE 0 END) AS ordered_count,
		SUM(CASE WHEN line.status = 'skipped' THEN 1 ELSE 0 END) AS skipped_count,
		COALESCE(SUM(line.suggested_order_qty * line.estimated_price), 0) AS estimated_total
	FROM " . TB_PREF . "procurement_plan plan
	LEFT JOIN " . TB_PREF . "users user ON user.id = plan.created_by
	LEFT JOIN " . TB_PREF . "procurement_plan_lines line ON line.plan_id = plan.id
	WHERE plan.id = " . (int)$id . "
	GROUP BY plan.id";
	$result = db_query($sql, 'could not retrieve procurement plan');
	return db_fetch($result);
}

/**
 * Retrieve procurement plan headers by filter.
 *
 * @param string $status
 * @param string $date_from
 * @param string $date_to
 * @return resource
 */
function get_procurement_plans($status = '', $date_from = '', $date_to = '')
{
	$sql = "SELECT plan.*,
		COUNT(line.id) AS line_count,
		COALESCE(SUM(line.suggested_order_qty * line.estimated_price), 0) AS estimated_total
	FROM " . TB_PREF . "procurement_plan plan
	LEFT JOIN " . TB_PREF . "procurement_plan_lines line ON line.plan_id = plan.id
	WHERE 1=1";

	if ($status !== '' && $status !== ALL_TEXT)
		$sql .= " AND plan.status = " . db_escape($status);
	if ($date_from !== '')
		$sql .= " AND plan.plan_date >= " . db_escape($date_from);
	if ($date_to !== '')
		$sql .= " AND plan.plan_date <= " . db_escape($date_to);

	$sql .= " GROUP BY plan.id ORDER BY plan.id DESC";
	return db_query($sql, 'could not retrieve procurement plans');
}

/**
 * Retrieve detail lines for one procurement plan.
 *
 * @param int $plan_id
 * @return resource
 */
function get_plan_lines($plan_id)
{
	$sql = "SELECT line.*,
		stock.description AS stock_description,
		stock.units AS stock_units,
		supplier.supp_name AS supplier_name,
		location.location_name AS location_name
	FROM " . TB_PREF . "procurement_plan_lines line
	LEFT JOIN " . TB_PREF . "stock_master stock ON stock.stock_id = line.stock_id
	LEFT JOIN " . TB_PREF . "suppliers supplier ON supplier.supplier_id = line.supplier_id
	LEFT JOIN " . TB_PREF . "locations location ON location.loc_code = line.location
	WHERE line.plan_id = " . (int)$plan_id . "
	ORDER BY line.priority DESC, line.id";
	return db_query($sql, 'could not retrieve procurement plan lines');
}

/**
 * Approve a procurement plan line.
 *
 * @param int   $line_id
 * @param float $approved_qty
 * @param int   $supplier_id
 * @return bool
 */
function approve_plan_line($line_id, $approved_qty = 0, $supplier_id = 0)
{
	$line_id = (int)$line_id;
	$sql = "SELECT * FROM " . TB_PREF . "procurement_plan_lines WHERE id = " . $line_id;
	$result = db_query($sql, 'could not retrieve procurement plan line');
	$line = db_fetch($result);
	if (!$line)
		return false;

	$effective_qty = (float)$approved_qty > 0 ? (float)$approved_qty : (float)$line['suggested_order_qty'];
	$effective_supplier = (int)$supplier_id > 0 ? (int)$supplier_id : (int)$line['supplier_id'];

	$sql = "UPDATE " . TB_PREF . "procurement_plan_lines SET
		suggested_order_qty = " . db_escape($effective_qty) . ",
		supplier_id = " . db_escape($effective_supplier) . ",
		status = 'approved'
	WHERE id = " . $line_id;
	db_query($sql, 'could not approve procurement plan line');

	return true;
}

/**
 * Mark one procurement plan line as skipped.
 *
 * @param int    $line_id
 * @param string $reason
 * @return bool
 */
function skip_plan_line($line_id, $reason = '')
{
	$line_id = (int)$line_id;
	$sql = "SELECT custom_data FROM " . TB_PREF . "procurement_plan_lines WHERE id = " . $line_id;
	$result = db_query($sql, 'could not retrieve procurement plan line custom data');
	$row = db_fetch($result);
	$custom_data = array();

	if (!empty($row['custom_data'])) {
		$decoded = json_decode($row['custom_data'], true);
		if (is_array($decoded))
			$custom_data = $decoded;
	}

	$custom_data['skip_reason'] = $reason;
	$custom_data['skipped_by'] = (int)$_SESSION['wa_current_user']->user;
	$custom_data['skipped_date'] = date2sql(Today());

	$sql = "UPDATE " . TB_PREF . "procurement_plan_lines SET
		status = 'skipped',
		custom_data = " . sql_procurement_plan_custom_data($custom_data) . "
	WHERE id = " . $line_id;
	db_query($sql, 'could not skip procurement plan line');

	return true;
}

/**
 * Create supplier-grouped purchase orders from approved plan lines.
 *
 * @param int $plan_id
 * @return array
 */
function create_pos_from_plan($plan_id)
{
	global $Refs;

	$sql = "SELECT * FROM " . TB_PREF . "procurement_plan_lines
		WHERE plan_id = " . (int)$plan_id . "
		AND status = 'approved'
		AND po_number = 0
		ORDER BY supplier_id, id";
	$result = db_query($sql, 'could not retrieve approved procurement plan lines');

	$grouped_lines = array();
	while ($line = db_fetch($result)) {
		if ((int)$line['supplier_id'] <= 0 || (float)$line['suggested_order_qty'] <= 0)
			continue;

		if (!isset($grouped_lines[$line['supplier_id']]))
			$grouped_lines[$line['supplier_id']] = array();

		$grouped_lines[$line['supplier_id']][] = $line;
	}

	if (empty($grouped_lines))
		return array();

	$po_numbers = array();
	begin_transaction();

	foreach ($grouped_lines as $supplier_id => $supplier_lines) {
		$purchase_order = new purch_order();
		$purchase_order->trans_type = ST_PURCHORDER;
		$purchase_order->orig_order_date = Today();
		$purchase_order->due_date = Today();
		$purchase_order->Comments = sprintf(_('Created from Procurement Plan %s'), (int)$plan_id);

		$first_location = '';
		foreach ($supplier_lines as $line) {
			if ($line['location'] !== '') {
				$first_location = $line['location'];
				break;
			}
		}
		if ($first_location !== '')
			$purchase_order->Location = $first_location;

		get_supplier_details_to_order($purchase_order, (int)$supplier_id);
		$purchase_order->reference = $Refs->get_next(ST_PURCHORDER, null, array(
			'supplier_id' => (int)$supplier_id,
			'date' => $purchase_order->orig_order_date,
		));

		foreach ($supplier_lines as $line) {
			$price = (float)$line['estimated_price'] > 0
				? (float)$line['estimated_price']
				: (float)get_purchase_price((int)$supplier_id, $line['stock_id'], (float)$line['suggested_order_qty']);

			$description = isset($line['stock_description']) ? $line['stock_description'] : null;
			$purchase_order->add_to_order(
				count($purchase_order->line_items),
				$line['stock_id'],
				(float)$line['suggested_order_qty'],
				$description,
				$price,
				null,
				$purchase_order->due_date,
				0,
				0
			);
		}

		if (!$purchase_order->order_has_items())
			continue;

		$po_number = add_po($purchase_order);
		if (!$po_number)
			continue;

		$po_numbers[] = (int)$po_number;
		foreach ($supplier_lines as $line) {
			$sql = "UPDATE " . TB_PREF . "procurement_plan_lines
				SET status = 'ordered', po_number = " . (int)$po_number . "
				WHERE id = " . (int)$line['id'];
			db_query($sql, 'could not mark procurement plan line as ordered');
		}
	}

	if (!empty($po_numbers)) {
		$sql = "UPDATE " . TB_PREF . "procurement_plan
			SET status = 'in_progress'
			WHERE id = " . (int)$plan_id;
		db_query($sql, 'could not update procurement plan status');

		$sql = "SELECT COUNT(*) AS pending_count FROM " . TB_PREF . "procurement_plan_lines
			WHERE plan_id = " . (int)$plan_id . "
			AND status IN ('pending', 'approved')";
		$result = db_query($sql, 'could not check remaining procurement plan lines');
		$pending_row = db_fetch($result);
		if ($pending_row && (int)$pending_row['pending_count'] === 0) {
			$sql = "UPDATE " . TB_PREF . "procurement_plan
				SET status = 'completed'
				WHERE id = " . (int)$plan_id;
			db_query($sql, 'could not set procurement plan completed status');
		}
	}

	commit_transaction();
	$hook_payload = array('plan_id' => (int)$plan_id, 'po_numbers' => $po_numbers);
	hook_invoke_all('procurement_plan_pos_created', $hook_payload);
	return $po_numbers;
}

/**
 * Build a simple demand forecast from open demand and historical outbound usage.
 *
 * @param string $stock_id
 * @param string $location
 * @param int    $months_ahead
 * @return array
 */
function get_demand_forecast($stock_id, $location, $months_ahead = 3)
{
	$months_ahead = max(1, (int)$months_ahead);

	$sql = "SELECT COALESCE(SUM(sod.quantity - sod.qty_sent), 0) AS open_sales_demand
		FROM " . TB_PREF . "sales_order_details sod
		INNER JOIN " . TB_PREF . "sales_orders so ON so.order_no = sod.order_no AND so.trans_type = sod.trans_type
		WHERE sod.stk_code = " . db_escape($stock_id) . "
		AND so.from_stk_loc = " . db_escape($location) . "
		AND so.trans_type = " . ST_SALESORDER . "
		AND (sod.quantity - sod.qty_sent) > 0";
	$result = db_query($sql, 'could not get open sales demand for forecast');
	$open_demand_row = db_fetch($result);
	$open_sales_demand = $open_demand_row ? (float)$open_demand_row['open_sales_demand'] : 0;

	$history_days = 180;
	$sql = "SELECT COALESCE(SUM(ABS(qty)), 0) AS outbound_qty
		FROM " . TB_PREF . "stock_moves
		WHERE stock_id = " . db_escape($stock_id) . "
		AND loc_code = " . db_escape($location) . "
		AND qty < 0
		AND tran_date >= DATE_SUB(CURDATE(), INTERVAL " . (int)$history_days . " DAY)";
	$result = db_query($sql, 'could not calculate historical outbound quantity for forecast');
	$history_row = db_fetch($result);
	$avg_monthly_usage = $history_row ? ((float)$history_row['outbound_qty'] / 6) : 0;

	$forecast_quantity = round(($avg_monthly_usage * $months_ahead) + $open_sales_demand, 2);
	$forecast = array(
		'stock_id' => $stock_id,
		'location' => $location,
		'months_ahead' => $months_ahead,
		'open_sales_demand' => round($open_sales_demand, 2),
		'average_monthly_usage' => round($avg_monthly_usage, 2),
		'forecast_quantity' => $forecast_quantity,
	);

	hook_invoke_all('demand_forecast_calculated', $forecast);
	return $forecast;
}
