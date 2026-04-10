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
 * Consignment & VMI (Vendor-Managed Inventory) Database Functions.
 *
 * Handles vendor-owned stock tracking, consumption with GL/AP posting,
 * return to vendor, and VMI level management.
 *
 * Table: 0_wh_consignment_stock (created in migration 001)
 *
 * @package NotrinosERP
 * @subpackage Inventory/Warehouse
 */

// =====================================================================
// CONSIGNMENT STATUS HELPERS
// =====================================================================

/**
 * Get available consignment statuses.
 *
 * @return array Associative array of status => label
 */
function get_consignment_statuses()
{
	return array(
		'on_hand'  => _('On Hand'),
		'consumed' => _('Consumed'),
		'returned' => _('Returned'),
	);
}

/**
 * Get a display label for a consignment status.
 *
 * @param string $status Status code
 * @return string Display label
 */
function get_consignment_status_label($status)
{
	$statuses = get_consignment_statuses();
	return isset($statuses[$status]) ? $statuses[$status] : $status;
}

/**
 * Get CSS color for a consignment status.
 *
 * @param string $status Status code
 * @return string Hex color
 */
function get_consignment_status_color($status)
{
	$colors = array(
		'on_hand'  => '#28a745',
		'consumed' => '#007bff',
		'returned' => '#6c757d',
	);
	return isset($colors[$status]) ? $colors[$status] : '#999';
}

/**
 * Render a colored status badge.
 *
 * @param string $status Status code
 * @return string HTML span badge
 */
function consignment_status_badge($status)
{
	$label = get_consignment_status_label($status);
	$color = get_consignment_status_color($status);
	return '<span style="display:inline-block;padding:2px 8px;border-radius:3px;background:'
		. $color . ';color:#fff;font-size:11px;font-weight:600;">'
		. $label . '</span>';
}

// =====================================================================
// CONSIGNMENT STOCK CRUD
// =====================================================================

/**
 * Receive consignment stock from a supplier.
 *
 * Creates a record in 0_wh_consignment_stock and optionally a stock move
 * (with stock_status='consignment' so it doesn't affect owned inventory).
 *
 * @param string $stock_id     Item code
 * @param int    $supplier_id  Supplier ID
 * @param float  $qty          Quantity received
 * @param float  $unit_cost    Unit cost (agreed price)
 * @param string $date_        Receipt date (display format)
 * @param string $loc_code     Warehouse location code
 * @param int    $wh_loc_id    Bin location ID (nullable)
 * @param int    $batch_id     Batch ID (nullable)
 * @param string $memo         Notes / reference
 * @return int|false Consignment stock record ID or false on failure
 */
function receive_consignment_stock($stock_id, $supplier_id, $qty, $unit_cost, $date_, $loc_code, $wh_loc_id = null, $batch_id = null, $memo = '')
{
	begin_transaction();

	$date_sql = date2sql($date_);

	$sql = "INSERT INTO " . TB_PREF . "wh_consignment_stock
		(stock_id, supplier_id, wh_loc_id, qty_on_hand, qty_consumed, unit_cost,
		 receipt_date, batch_id, status, memo, loc_code)
		VALUES ("
		. db_escape($stock_id) . ", "
		. db_escape($supplier_id) . ", "
		. db_escape($wh_loc_id ? $wh_loc_id : 0) . ", "
		. db_escape($qty) . ", "
		. "0, "
		. db_escape($unit_cost) . ", "
		. "'" . $date_sql . "', "
		. ($batch_id ? db_escape($batch_id) : "NULL") . ", "
		. "'on_hand', "
		. db_escape($memo) . ", "
		. db_escape($loc_code) . ")";

	db_query($sql, 'Could not insert consignment stock record');
	$consignment_id = db_insert_id();

	// Log a consignment receipt movement
	add_consignment_movement($consignment_id, 'receive', $qty, $date_,
		_('Consignment receipt from supplier'), null, $memo);

	commit_transaction();

	return $consignment_id;
}

/**
 * Get a single consignment stock record.
 *
 * @param int $consignment_id Record ID
 * @return array|false Row data or false
 */
function get_consignment_record($consignment_id)
{
	$sql = "SELECT cs.*, sm.description AS item_name, sm.units,
			supp.supp_name AS supplier_name
		FROM " . TB_PREF . "wh_consignment_stock cs
		LEFT JOIN " . TB_PREF . "stock_master sm ON cs.stock_id = sm.stock_id
		LEFT JOIN " . TB_PREF . "suppliers supp ON cs.supplier_id = supp.supplier_id
		WHERE cs.id = " . db_escape($consignment_id);

	$result = db_query($sql, 'Could not get consignment record');
	return db_fetch($result);
}

/**
 * Get consignment stock records with filters.
 *
 * @param array $filters Filter options (stock_id, supplier_id, status, loc_code, show_zero)
 * @param int   $limit   Max rows (0 = all)
 * @param int   $offset  Row offset
 * @return resource DB result set
 */
function get_consignment_stocks($filters = array(), $limit = 0, $offset = 0)
{
	$sql = "SELECT cs.*, sm.description AS item_name, sm.units,
			supp.supp_name AS supplier_name,
			(cs.qty_on_hand * cs.unit_cost) AS total_value
		FROM " . TB_PREF . "wh_consignment_stock cs
		LEFT JOIN " . TB_PREF . "stock_master sm ON cs.stock_id = sm.stock_id
		LEFT JOIN " . TB_PREF . "suppliers supp ON cs.supplier_id = supp.supplier_id
		WHERE 1=1";

	if (!empty($filters['stock_id']))
		$sql .= " AND cs.stock_id = " . db_escape($filters['stock_id']);

	if (!empty($filters['supplier_id']))
		$sql .= " AND cs.supplier_id = " . db_escape($filters['supplier_id']);

	if (!empty($filters['status']))
		$sql .= " AND cs.status = " . db_escape($filters['status']);

	if (!empty($filters['loc_code']))
		$sql .= " AND cs.loc_code = " . db_escape($filters['loc_code']);

	if (empty($filters['show_zero']))
		$sql .= " AND cs.qty_on_hand > 0";

	$sql .= " ORDER BY cs.receipt_date DESC, cs.id DESC";

	if ($limit > 0)
		$sql .= " LIMIT " . (int)$offset . ", " . (int)$limit;

	return db_query($sql, 'Could not get consignment stocks');
}

/**
 * Count consignment stock records with filters.
 *
 * @param array $filters Filter options
 * @return int Count
 */
function count_consignment_stocks($filters = array())
{
	$sql = "SELECT COUNT(*) FROM " . TB_PREF . "wh_consignment_stock cs WHERE 1=1";

	if (!empty($filters['stock_id']))
		$sql .= " AND cs.stock_id = " . db_escape($filters['stock_id']);

	if (!empty($filters['supplier_id']))
		$sql .= " AND cs.supplier_id = " . db_escape($filters['supplier_id']);

	if (!empty($filters['status']))
		$sql .= " AND cs.status = " . db_escape($filters['status']);

	if (!empty($filters['loc_code']))
		$sql .= " AND cs.loc_code = " . db_escape($filters['loc_code']);

	if (empty($filters['show_zero']))
		$sql .= " AND cs.qty_on_hand > 0";

	$result = db_query($sql, 'Could not count consignment stocks');
	$row = db_fetch_row($result);
	return $row[0];
}

// =====================================================================
// CONSIGNMENT CONSUMPTION — Convert to owned stock with GL/AP posting
// =====================================================================

/**
 * Consume consignment stock — convert vendor-owned to company-owned.
 *
 * GL posting: Debit inventory_account, Credit supplier's payable_account.
 * Also creates a supp_trans (supplier invoice) record for AP tracking.
 *
 * @param int    $consignment_id Consignment stock record ID
 * @param float  $qty            Quantity to consume
 * @param string $date_          Consumption date (display format)
 * @param string $memo           Notes
 * @return array|false Array with keys: trans_no, consignment_id, amount — or false on error
 */
function consume_consignment_stock($consignment_id, $qty, $date_, $memo = '')
{
	$record = get_consignment_record($consignment_id);
	if (!$record) {
		display_error(_('Consignment record not found.'));
		return false;
	}

	if ($qty <= 0) {
		display_error(_('Consume quantity must be greater than zero.'));
		return false;
	}

	if ($qty > $record['qty_on_hand']) {
		display_error(sprintf(_('Cannot consume %s — only %s on hand.'),
			number_format2($qty, get_qty_dec($record['stock_id'])),
			number_format2($record['qty_on_hand'], get_qty_dec($record['stock_id']))));
		return false;
	}

	begin_transaction();

	$stock_id = $record['stock_id'];
	$supplier_id = $record['supplier_id'];
	$unit_cost = $record['unit_cost'];
	$total_cost = round2($qty * $unit_cost, user_price_dec());
	$loc_code = $record['loc_code'];

	// 1. Update consignment record — reduce qty_on_hand, increase qty_consumed
	$sql = "UPDATE " . TB_PREF . "wh_consignment_stock
		SET qty_on_hand = qty_on_hand - " . db_escape($qty) . ",
			qty_consumed = qty_consumed + " . db_escape($qty) . "
		WHERE id = " . db_escape($consignment_id);
	db_query($sql, 'Could not update consignment stock on consume');

	// Check if fully consumed
	$updated = get_consignment_record($consignment_id);
	if ($updated && $updated['qty_on_hand'] <= 0) {
		$sql = "UPDATE " . TB_PREF . "wh_consignment_stock
			SET status = 'consumed'
			WHERE id = " . db_escape($consignment_id);
		db_query($sql, 'Could not update consignment status');
	}

	// 2. Create stock move — positive qty = stock in (now owned)
	$item_row = get_item($stock_id);
	$std_cost = $item_row ? $item_row['material_cost'] : $unit_cost;
	$move_id = add_stock_move(ST_INVADJUST, $stock_id, 0, $loc_code, $date_,
		'CONSIGN-' . $consignment_id, $qty, $std_cost, $unit_cost);

	// Update the stock move trans_no with the actual adjustment number
	$adj_no = get_next_trans_no(ST_INVADJUST);

	// 3. GL posting — Debit inventory, Credit supplier payable
	$stock_gl = get_stock_gl_code($stock_id);
	$supplier = get_supplier($supplier_id);

	if (!$supplier) {
		display_error(_('Supplier not found.'));
		cancel_transaction();
		return false;
	}

	$payable_account = $supplier['payable_account'];
	$inventory_account = $stock_gl['inventory_account'];
	$dimension_id = $stock_gl['dimension_id'];
	$dimension2_id = $stock_gl['dimension2_id'];

	$gl_memo = sprintf(_('Consignment consumption: %s x%s from %s'),
		$stock_id, number_format2($qty, get_qty_dec($stock_id)), $supplier['supp_name']);

	// Debit inventory account (stock comes in)
	add_gl_trans_std_cost(ST_INVADJUST, $adj_no, $date_,
		$inventory_account, $dimension_id, $dimension2_id,
		$gl_memo, $total_cost);

	// Credit supplier payable (we owe the supplier)
	add_gl_trans_std_cost(ST_INVADJUST, $adj_no, $date_,
		$payable_account, 0, 0,
		$gl_memo, -$total_cost,
		PT_SUPPLIER, $supplier_id);

	// 4. Create supplier transaction for AP tracking
	$reference = 'CONSIGN-' . $consignment_id;
	$due_date = add_days($date_, 30); // 30 days payment term

	$trans_no = write_supp_trans(ST_SUPPINVOICE, 0, $supplier_id, $date_, $due_date,
		$reference, $reference, $total_cost, 0, 0,
		'Could not create consignment consumption AP entry');

	// 5. Log movement
	add_consignment_movement($consignment_id, 'consume', $qty, $date_,
		sprintf(_('Consumed %s units — GL adj #%s, AP inv #%s'), $qty, $adj_no, $trans_no),
		$trans_no, $memo);

	add_audit_trail(ST_INVADJUST, $adj_no, $date_);

	commit_transaction();

	return array(
		'trans_no'       => $trans_no,
		'adj_no'         => $adj_no,
		'consignment_id' => $consignment_id,
		'amount'         => $total_cost,
	);
}

// =====================================================================
// CONSIGNMENT RETURN — Return vendor-owned stock
// =====================================================================

/**
 * Return consignment stock to vendor.
 *
 * No GL impact — the stock was never ours. Just decrements qty_on_hand.
 *
 * @param int    $consignment_id Consignment stock record ID
 * @param float  $qty            Quantity to return
 * @param string $date_          Return date (display format)
 * @param string $memo           Notes / return reason
 * @return bool Success
 */
function return_consignment_stock($consignment_id, $qty, $date_, $memo = '')
{
	$record = get_consignment_record($consignment_id);
	if (!$record) {
		display_error(_('Consignment record not found.'));
		return false;
	}

	if ($qty <= 0) {
		display_error(_('Return quantity must be greater than zero.'));
		return false;
	}

	if ($qty > $record['qty_on_hand']) {
		display_error(sprintf(_('Cannot return %s — only %s on hand.'),
			number_format2($qty, get_qty_dec($record['stock_id'])),
			number_format2($record['qty_on_hand'], get_qty_dec($record['stock_id']))));
		return false;
	}

	begin_transaction();

	// Update consignment record
	$sql = "UPDATE " . TB_PREF . "wh_consignment_stock
		SET qty_on_hand = qty_on_hand - " . db_escape($qty) . "
		WHERE id = " . db_escape($consignment_id);
	db_query($sql, 'Could not update consignment stock on return');

	// Check if fully returned
	$updated = get_consignment_record($consignment_id);
	if ($updated && $updated['qty_on_hand'] <= 0) {
		$sql = "UPDATE " . TB_PREF . "wh_consignment_stock
			SET status = 'returned'
			WHERE id = " . db_escape($consignment_id);
		db_query($sql, 'Could not update consignment status');
	}

	// Log movement
	add_consignment_movement($consignment_id, 'return', $qty, $date_,
		sprintf(_('Returned %s units to supplier'), $qty), null, $memo);

	commit_transaction();

	return true;
}

// =====================================================================
// CONSIGNMENT MOVEMENT LOG
// =====================================================================

/**
 * Log a consignment stock movement.
 *
 * @param int    $consignment_id Consignment stock record ID
 * @param string $movement_type  Type: receive, consume, return, adjust
 * @param float  $qty            Quantity affected
 * @param string $date_          Movement date (display format)
 * @param string $description    Movement description
 * @param int    $reference_no   Reference transaction number (nullable)
 * @param string $memo           Additional notes
 * @return int Movement ID
 */
function add_consignment_movement($consignment_id, $movement_type, $qty, $date_, $description = '', $reference_no = null, $memo = '')
{
	$date_sql = date2sql($date_);

	// Ensure the movements table exists — use a simple table
	// We store movements in a generic structure
	$sql = "INSERT INTO " . TB_PREF . "wh_consignment_movements
		(consignment_id, movement_type, qty, movement_date, description, reference_no, memo, created_by)
		VALUES ("
		. db_escape($consignment_id) . ", "
		. db_escape($movement_type) . ", "
		. db_escape($qty) . ", "
		. "'" . $date_sql . "', "
		. db_escape($description) . ", "
		. ($reference_no !== null ? db_escape($reference_no) : "NULL") . ", "
		. db_escape($memo) . ", "
		. db_escape($_SESSION['wa_current_user']->user) . ")";

	db_query($sql, 'Could not insert consignment movement');
	return db_insert_id();
}

/**
 * Get consignment movements for a record.
 *
 * @param int $consignment_id Consignment stock record ID (0 = all)
 * @param int $limit          Max rows
 * @param int $offset         Row offset
 * @return resource DB result set
 */
function get_consignment_movements($consignment_id = 0, $limit = 50, $offset = 0)
{
	$sql = "SELECT cm.*, cs.stock_id, sm.description AS item_name,
			supp.supp_name AS supplier_name
		FROM " . TB_PREF . "wh_consignment_movements cm
		LEFT JOIN " . TB_PREF . "wh_consignment_stock cs ON cm.consignment_id = cs.id
		LEFT JOIN " . TB_PREF . "stock_master sm ON cs.stock_id = sm.stock_id
		LEFT JOIN " . TB_PREF . "suppliers supp ON cs.supplier_id = supp.supplier_id
		WHERE 1=1";

	if ($consignment_id > 0)
		$sql .= " AND cm.consignment_id = " . db_escape($consignment_id);

	$sql .= " ORDER BY cm.movement_date DESC, cm.id DESC";

	if ($limit > 0)
		$sql .= " LIMIT " . (int)$offset . ", " . (int)$limit;

	return db_query($sql, 'Could not get consignment movements');
}

// =====================================================================
// CONSIGNMENT BALANCE & SUMMARY
// =====================================================================

/**
 * Get consignment balance summary grouped by supplier.
 *
 * @param string $stock_id   Filter by item (optional)
 * @param string $loc_code   Filter by location (optional)
 * @return resource DB result set
 */
function get_consignment_balance_by_supplier($stock_id = '', $loc_code = '')
{
	$sql = "SELECT cs.supplier_id, supp.supp_name AS supplier_name,
			COUNT(DISTINCT cs.stock_id) AS item_count,
			SUM(cs.qty_on_hand) AS total_on_hand,
			SUM(cs.qty_consumed) AS total_consumed,
			SUM(cs.qty_on_hand * cs.unit_cost) AS total_value_on_hand,
			SUM(cs.qty_consumed * cs.unit_cost) AS total_value_consumed
		FROM " . TB_PREF . "wh_consignment_stock cs
		LEFT JOIN " . TB_PREF . "suppliers supp ON cs.supplier_id = supp.supplier_id
		WHERE 1=1";

	if (!empty($stock_id))
		$sql .= " AND cs.stock_id = " . db_escape($stock_id);

	if (!empty($loc_code))
		$sql .= " AND cs.loc_code = " . db_escape($loc_code);

	$sql .= " GROUP BY cs.supplier_id ORDER BY supp.supp_name";

	return db_query($sql, 'Could not get consignment balance by supplier');
}

/**
 * Get consignment summary metrics for dashboard cards.
 *
 * @return array Summary with total_records, total_on_hand_qty, total_on_hand_value,
 *               total_consumed_qty, total_consumed_value, supplier_count, item_count
 */
function get_consignment_summary()
{
	$sql = "SELECT
			COUNT(*) AS total_records,
			COALESCE(SUM(qty_on_hand), 0) AS total_on_hand_qty,
			COALESCE(SUM(qty_on_hand * unit_cost), 0) AS total_on_hand_value,
			COALESCE(SUM(qty_consumed), 0) AS total_consumed_qty,
			COALESCE(SUM(qty_consumed * unit_cost), 0) AS total_consumed_value,
			COUNT(DISTINCT supplier_id) AS supplier_count,
			COUNT(DISTINCT stock_id) AS item_count
		FROM " . TB_PREF . "wh_consignment_stock
		WHERE status = 'on_hand' AND qty_on_hand > 0";

	$result = db_query($sql, 'Could not get consignment summary');
	$row = db_fetch($result);

	if (!$row) {
		return array(
			'total_records'       => 0,
			'total_on_hand_qty'   => 0,
			'total_on_hand_value' => 0,
			'total_consumed_qty'  => 0,
			'total_consumed_value'=> 0,
			'supplier_count'      => 0,
			'item_count'          => 0,
		);
	}

	return $row;
}

/**
 * Get consignment stock balance for a specific item and supplier.
 *
 * @param string $stock_id    Item code
 * @param int    $supplier_id Supplier ID
 * @return float Total on-hand quantity
 */
function get_consignment_qty_on_hand($stock_id, $supplier_id)
{
	$sql = "SELECT COALESCE(SUM(qty_on_hand), 0) AS qty
		FROM " . TB_PREF . "wh_consignment_stock
		WHERE stock_id = " . db_escape($stock_id) . "
		AND supplier_id = " . db_escape($supplier_id) . "
		AND status = 'on_hand'";

	$result = db_query($sql, 'Could not get consignment qty on hand');
	$row = db_fetch_row($result);
	return $row[0];
}

// =====================================================================
// CONSIGNMENT INVOICE GENERATION
// =====================================================================

/**
 * Generate a consolidated supplier invoice for all consumed consignment stock
 * within a date range that hasn't been invoiced yet.
 *
 * @param int    $supplier_id Supplier ID
 * @param string $from_date   Start date (display format)
 * @param string $to_date     End date (display format)
 * @return array|false Invoice summary or false if nothing to invoice
 */
function generate_consignment_invoice($supplier_id, $from_date, $to_date)
{
	$from_sql = date2sql($from_date);
	$to_sql = date2sql($to_date);

	// Find consumed movements not yet invoiced
	$sql = "SELECT cm.consignment_id, cm.qty, cs.stock_id, cs.unit_cost,
			sm.description AS item_name,
			(cm.qty * cs.unit_cost) AS line_total
		FROM " . TB_PREF . "wh_consignment_movements cm
		JOIN " . TB_PREF . "wh_consignment_stock cs ON cm.consignment_id = cs.id
		LEFT JOIN " . TB_PREF . "stock_master sm ON cs.stock_id = sm.stock_id
		WHERE cm.movement_type = 'consume'
		AND cs.supplier_id = " . db_escape($supplier_id) . "
		AND cm.movement_date BETWEEN '" . $from_sql . "' AND '" . $to_sql . "'
		AND cm.invoiced = 0
		ORDER BY cm.movement_date, cm.id";

	$result = db_query($sql, 'Could not get uninvoiced consignment consumption');

	$lines = array();
	$total = 0;
	$movement_ids = array();

	while ($row = db_fetch($result)) {
		$lines[] = $row;
		$total += $row['line_total'];
		$movement_ids[] = $row['consignment_id'];
	}

	if (empty($lines)) {
		return false;
	}

	begin_transaction();

	// Create supplier invoice
	$reference = 'CONSIGN-INV-' . $supplier_id . '-' . date('Ymd');
	$due_date = add_days($to_date, 30);

	$trans_no = write_supp_trans(ST_SUPPINVOICE, 0, $supplier_id, $to_date, $due_date,
		$reference, $reference, $total, 0, 0,
		'Could not create consignment invoice');

	// Mark movements as invoiced
	$sql = "UPDATE " . TB_PREF . "wh_consignment_movements
		SET invoiced = 1, invoice_trans_no = " . db_escape($trans_no) . "
		WHERE movement_type = 'consume'
		AND movement_date BETWEEN '" . $from_sql . "' AND '" . $to_sql . "'
		AND invoiced = 0
		AND consignment_id IN (
			SELECT id FROM " . TB_PREF . "wh_consignment_stock
			WHERE supplier_id = " . db_escape($supplier_id) . "
		)";
	db_query($sql, 'Could not mark consignment movements as invoiced');

	add_audit_trail(ST_SUPPINVOICE, $trans_no, $to_date);

	commit_transaction();

	return array(
		'trans_no'    => $trans_no,
		'supplier_id' => $supplier_id,
		'line_count'  => count($lines),
		'total'       => $total,
		'lines'       => $lines,
	);
}

// =====================================================================
// VMI (VENDOR-MANAGED INVENTORY) FUNCTIONS
// =====================================================================

/**
 * Get VMI levels for items managed by a supplier.
 *
 * @param int    $supplier_id Supplier ID (0 = all)
 * @param string $stock_id    Item code (optional)
 * @return resource DB result set
 */
function get_vmi_levels($supplier_id = 0, $stock_id = '')
{
	$sql = "SELECT vmi.*, sm.description AS item_name, sm.units,
			supp.supp_name AS supplier_name,
			COALESCE(cs_sum.total_on_hand, 0) AS current_on_hand,
			COALESCE(cs_sum.total_value, 0) AS current_value
		FROM " . TB_PREF . "wh_vmi_levels vmi
		LEFT JOIN " . TB_PREF . "stock_master sm ON vmi.stock_id = sm.stock_id
		LEFT JOIN " . TB_PREF . "suppliers supp ON vmi.supplier_id = supp.supplier_id
		LEFT JOIN (
			SELECT stock_id, supplier_id,
				SUM(qty_on_hand) AS total_on_hand,
				SUM(qty_on_hand * unit_cost) AS total_value
			FROM " . TB_PREF . "wh_consignment_stock
			WHERE status = 'on_hand'
			GROUP BY stock_id, supplier_id
		) cs_sum ON vmi.stock_id = cs_sum.stock_id AND vmi.supplier_id = cs_sum.supplier_id
		WHERE 1=1";

	if ($supplier_id > 0)
		$sql .= " AND vmi.supplier_id = " . db_escape($supplier_id);

	if (!empty($stock_id))
		$sql .= " AND vmi.stock_id = " . db_escape($stock_id);

	$sql .= " ORDER BY supp.supp_name, sm.description";

	return db_query($sql, 'Could not get VMI levels');
}

/**
 * Add or update a VMI level setting.
 *
 * @param string $stock_id    Item code
 * @param int    $supplier_id Supplier ID
 * @param float  $min_level   Minimum stock level (reorder point)
 * @param float  $max_level   Maximum stock level
 * @param float  $reorder_qty Suggested reorder quantity
 * @param string $loc_code    Location code
 * @return bool Success
 */
function set_vmi_level($stock_id, $supplier_id, $min_level, $max_level, $reorder_qty, $loc_code = '')
{
	// Check if record already exists
	$sql = "SELECT id FROM " . TB_PREF . "wh_vmi_levels
		WHERE stock_id = " . db_escape($stock_id) . "
		AND supplier_id = " . db_escape($supplier_id);

	$result = db_query($sql, 'Could not check VMI level');
	$existing = db_fetch($result);

	if ($existing) {
		$sql = "UPDATE " . TB_PREF . "wh_vmi_levels
			SET min_level = " . db_escape($min_level) . ",
				max_level = " . db_escape($max_level) . ",
				reorder_qty = " . db_escape($reorder_qty) . ",
				loc_code = " . db_escape($loc_code) . ",
				updated_at = NOW()
			WHERE id = " . db_escape($existing['id']);
	} else {
		$sql = "INSERT INTO " . TB_PREF . "wh_vmi_levels
			(stock_id, supplier_id, min_level, max_level, reorder_qty, loc_code, created_at, updated_at)
			VALUES ("
			. db_escape($stock_id) . ", "
			. db_escape($supplier_id) . ", "
			. db_escape($min_level) . ", "
			. db_escape($max_level) . ", "
			. db_escape($reorder_qty) . ", "
			. db_escape($loc_code) . ", "
			. "NOW(), NOW())";
	}

	db_query($sql, 'Could not save VMI level');
	return true;
}

/**
 * Delete a VMI level setting.
 *
 * @param int $vmi_id VMI level record ID
 * @return bool Success
 */
function delete_vmi_level($vmi_id)
{
	$sql = "DELETE FROM " . TB_PREF . "wh_vmi_levels
		WHERE id = " . db_escape($vmi_id);
	db_query($sql, 'Could not delete VMI level');
	return true;
}

/**
 * Get VMI alerts — items below minimum level.
 *
 * @param int $supplier_id Supplier ID (0 = all)
 * @return resource DB result set
 */
function get_vmi_alerts($supplier_id = 0)
{
	$sql = "SELECT vmi.*, sm.description AS item_name, sm.units,
			supp.supp_name AS supplier_name,
			COALESCE(cs_sum.total_on_hand, 0) AS current_on_hand,
			(vmi.min_level - COALESCE(cs_sum.total_on_hand, 0)) AS deficit
		FROM " . TB_PREF . "wh_vmi_levels vmi
		LEFT JOIN " . TB_PREF . "stock_master sm ON vmi.stock_id = sm.stock_id
		LEFT JOIN " . TB_PREF . "suppliers supp ON vmi.supplier_id = supp.supplier_id
		LEFT JOIN (
			SELECT stock_id, supplier_id,
				SUM(qty_on_hand) AS total_on_hand
			FROM " . TB_PREF . "wh_consignment_stock
			WHERE status = 'on_hand'
			GROUP BY stock_id, supplier_id
		) cs_sum ON vmi.stock_id = cs_sum.stock_id AND vmi.supplier_id = cs_sum.supplier_id
		WHERE COALESCE(cs_sum.total_on_hand, 0) < vmi.min_level";

	if ($supplier_id > 0)
		$sql .= " AND vmi.supplier_id = " . db_escape($supplier_id);

	$sql .= " ORDER BY (vmi.min_level - COALESCE(cs_sum.total_on_hand, 0)) DESC";

	return db_query($sql, 'Could not get VMI alerts');
}

/**
 * Export VMI stock levels as an array for vendor sharing.
 *
 * @param int $supplier_id Supplier ID
 * @return array Array of stock level data rows
 */
function export_vmi_stock_levels($supplier_id)
{
	$sql = "SELECT vmi.stock_id, sm.description AS item_name, sm.units,
			vmi.min_level, vmi.max_level, vmi.reorder_qty, vmi.loc_code,
			COALESCE(cs_sum.total_on_hand, 0) AS current_on_hand,
			GREATEST(0, vmi.min_level - COALESCE(cs_sum.total_on_hand, 0)) AS suggested_replenish,
			CASE
				WHEN COALESCE(cs_sum.total_on_hand, 0) < vmi.min_level THEN 'BELOW MIN'
				WHEN COALESCE(cs_sum.total_on_hand, 0) > vmi.max_level THEN 'ABOVE MAX'
				ELSE 'OK'
			END AS status
		FROM " . TB_PREF . "wh_vmi_levels vmi
		LEFT JOIN " . TB_PREF . "stock_master sm ON vmi.stock_id = sm.stock_id
		LEFT JOIN (
			SELECT stock_id, supplier_id,
				SUM(qty_on_hand) AS total_on_hand
			FROM " . TB_PREF . "wh_consignment_stock
			WHERE status = 'on_hand'
			GROUP BY stock_id, supplier_id
		) cs_sum ON vmi.stock_id = cs_sum.stock_id AND vmi.supplier_id = cs_sum.supplier_id
		WHERE vmi.supplier_id = " . db_escape($supplier_id) . "
		ORDER BY sm.description";

	$result = db_query($sql, 'Could not export VMI stock levels');

	$rows = array();
	while ($row = db_fetch($result)) {
		$rows[] = $row;
	}
	return $rows;
}

/**
 * Get count of consignment records with on-hand stock.
 *
 * @return int Count of active consignment records
 */
function get_active_consignment_count()
{
	$sql = "SELECT COUNT(*) FROM " . TB_PREF . "wh_consignment_stock
		WHERE status = 'on_hand' AND qty_on_hand > 0";
	$result = db_query($sql, 'Could not count active consignment records');
	$row = db_fetch_row($result);
	return $row[0];
}

/**
 * Get count of VMI items below minimum level.
 *
 * @return int Count of VMI alerts
 */
function get_vmi_alert_count()
{
	$sql = "SELECT COUNT(*)
		FROM " . TB_PREF . "wh_vmi_levels vmi
		LEFT JOIN (
			SELECT stock_id, supplier_id,
				SUM(qty_on_hand) AS total_on_hand
			FROM " . TB_PREF . "wh_consignment_stock
			WHERE status = 'on_hand'
			GROUP BY stock_id, supplier_id
		) cs_sum ON vmi.stock_id = cs_sum.stock_id AND vmi.supplier_id = cs_sum.supplier_id
		WHERE COALESCE(cs_sum.total_on_hand, 0) < vmi.min_level";

	$result = db_query($sql, 'Could not count VMI alerts');
	$row = db_fetch_row($result);
	return $row[0];
}
