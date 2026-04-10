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
 * Warehouse Material Requests — CRUD and lifecycle management.
 *
 * Material request lifecycle: Draft → Submitted → Approved → Ordered → Fulfilled → (Cancelled)
 * Types: purchase, transfer, manufacturing, issue
 * Supports conversion to PO, Transfer Order, or Work Order.
 * Tracks fulfillment per line.
 *
 * Tables: 0_wh_material_requests, 0_wh_material_request_lines
 *
 * @package NotrinosERP
 * @subpackage Inventory/Warehouse
 */

/**
 * Get all valid material request statuses.
 *
 * @return array status_code => label
 */
function get_material_request_statuses() {
	return array(
		'draft'     => _('Draft'),
		'submitted' => _('Submitted'),
		'approved'  => _('Approved'),
		'ordered'   => _('Ordered'),
		'fulfilled' => _('Fulfilled'),
		'cancelled' => _('Cancelled'),
	);
}

/**
 * Get the CSS color for a material request status.
 *
 * @param string $status Status code
 * @return string Hex color
 */
function get_material_request_status_color($status) {
	$colors = array(
		'draft'     => '#6c757d',
		'submitted' => '#17a2b8',
		'approved'  => '#007bff',
		'ordered'   => '#fd7e14',
		'fulfilled' => '#28a745',
		'cancelled' => '#dc3545',
	);
	return isset($colors[$status]) ? $colors[$status] : '#6c757d';
}

/**
 * Get all valid material request types.
 *
 * @return array type_code => label
 */
function get_material_request_types() {
	return array(
		'purchase'      => _('Purchase'),
		'transfer'      => _('Transfer'),
		'manufacturing' => _('Manufacturing'),
		'issue'         => _('Issue'),
	);
}

/**
 * Get the CSS color for a material request type.
 *
 * @param string $type Request type
 * @return string Hex color
 */
function get_material_request_type_color($type) {
	$colors = array(
		'purchase'      => '#007bff',
		'transfer'      => '#17a2b8',
		'manufacturing' => '#6f42c1',
		'issue'         => '#fd7e14',
	);
	return isset($colors[$type]) ? $colors[$type] : '#007bff';
}

/**
 * Render an HTML status badge for a material request.
 *
 * @param string $status Status code
 * @return string HTML span element
 */
function material_request_status_badge($status) {
	$statuses = get_material_request_statuses();
	$label = isset($statuses[$status]) ? $statuses[$status] : $status;
	$color = get_material_request_status_color($status);
	return '<span style="display:inline-block;padding:2px 8px;border-radius:3px;font-size:11px;'
		. 'font-weight:bold;color:#fff;background:' . $color . ';">' . $label . '</span>';
}

/**
 * Render an HTML type badge for a material request.
 *
 * @param string $type Request type
 * @return string HTML span element
 */
function material_request_type_badge($type) {
	$types = get_material_request_types();
	$label = isset($types[$type]) ? $types[$type] : $type;
	$color = get_material_request_type_color($type);
	return '<span style="display:inline-block;padding:2px 8px;border-radius:3px;font-size:11px;'
		. 'font-weight:bold;color:#fff;background:' . $color . ';">' . $label . '</span>';
}

// =====================================================================
// CRUD — Material Request Header
// =====================================================================

/**
 * Get the next sequential request number.
 *
 * @return int Next request_no
 */
function get_next_material_request_no() {
	$sql = "SELECT COALESCE(MAX(request_no), 0) + 1 AS next_no FROM " . TB_PREF . "wh_material_requests";
	$result = db_query($sql, 'could not get next material request no');
	$row = db_fetch($result);
	return (int)$row['next_no'];
}

/**
 * Format a material request number for display.
 *
 * @param int $request_no The raw request number
 * @return string Formatted string e.g. "MR-0001"
 */
function format_material_request_no($request_no) {
	return 'MR-' . str_pad($request_no, 4, '0', STR_PAD_LEFT);
}

/**
 * Add a new material request header.
 *
 * @param string      $request_type     purchase|transfer|manufacturing|issue
 * @param string      $warehouse_loc    Requesting warehouse location code
 * @param string      $request_date     Request date (SQL format)
 * @param string|null $required_date    Required by date (SQL format)
 * @param string|null $reference        Optional reference
 * @param string|null $memo             Optional memo
 * @return int Inserted request_id
 */
function add_material_request($request_type, $warehouse_loc, $request_date,
	$required_date = null, $reference = null, $memo = null)
{
	$request_no = get_next_material_request_no();

	$sql = "INSERT INTO " . TB_PREF . "wh_material_requests (
		request_no, request_type, status, warehouse_loc_code,
		requested_by, request_date, required_date,
		reference, memo, created_at
	) VALUES ("
		. (int)$request_no . ","
		. db_escape($request_type) . ","
		. "'draft',"
		. db_escape($warehouse_loc) . ","
		. (int)$_SESSION['wa_current_user']->user . ","
		. db_escape($request_date) . ","
		. ($required_date ? db_escape($required_date) : 'NULL') . ","
		. ($reference ? db_escape($reference) : 'NULL') . ","
		. ($memo ? db_escape($memo) : 'NULL') . ","
		. "NOW()"
		. ")";

	db_query($sql, 'could not insert material request');
	return db_insert_id();
}

/**
 * Update a material request header (draft only).
 *
 * @param int         $request_id       Request ID
 * @param string      $request_type     purchase|transfer|manufacturing|issue
 * @param string      $warehouse_loc    Requesting warehouse location code
 * @param string      $request_date     Request date (SQL format)
 * @param string|null $required_date    Required by date (SQL format)
 * @param string|null $reference        Optional reference
 * @param string|null $memo             Optional memo
 * @return bool Success
 */
function update_material_request($request_id, $request_type, $warehouse_loc,
	$request_date, $required_date = null, $reference = null, $memo = null)
{
	$sql = "UPDATE " . TB_PREF . "wh_material_requests SET "
		. "request_type = " . db_escape($request_type) . ","
		. "warehouse_loc_code = " . db_escape($warehouse_loc) . ","
		. "request_date = " . db_escape($request_date) . ","
		. "required_date = " . ($required_date ? db_escape($required_date) : 'NULL') . ","
		. "reference = " . ($reference ? db_escape($reference) : 'NULL') . ","
		. "memo = " . ($memo ? db_escape($memo) : 'NULL') . ","
		. "updated_at = NOW()"
		. " WHERE request_id = " . (int)$request_id
		. " AND status = 'draft'";

	db_query($sql, 'could not update material request');
	return db_num_affected_rows() > 0;
}

/**
 * Get a material request header with joined display names.
 *
 * @param int $request_id Request ID
 * @return array|false Request row or false
 */
function get_material_request($request_id) {
	$sql = "SELECT mr.*,
		loc.location_name AS warehouse_name,
		u1.real_name AS requested_by_name,
		u2.real_name AS approved_by_name
	FROM " . TB_PREF . "wh_material_requests mr
	LEFT JOIN " . TB_PREF . "locations loc ON loc.loc_code = mr.warehouse_loc_code
	LEFT JOIN " . TB_PREF . "users u1 ON u1.id = mr.requested_by
	LEFT JOIN " . TB_PREF . "users u2 ON u2.id = mr.approved_by
	WHERE mr.request_id = " . (int)$request_id;

	$result = db_query($sql, 'could not get material request');
	return db_fetch($result);
}

/**
 * Check if a material request can be deleted.
 *
 * @param int $request_id Request ID
 * @return bool True if deletion is allowed (draft or cancelled)
 */
function can_delete_material_request($request_id) {
	$mr = get_material_request($request_id);
	if (!$mr) return false;
	return in_array($mr['status'], array('draft', 'cancelled'));
}

/**
 * Delete a material request and its lines.
 *
 * @param int $request_id Request ID
 * @return bool Success
 */
function delete_material_request($request_id) {
	if (!can_delete_material_request($request_id))
		return false;

	begin_transaction();

	$sql = "DELETE FROM " . TB_PREF . "wh_material_request_lines WHERE request_id = " . (int)$request_id;
	db_query($sql, 'could not delete material request lines');

	$sql = "DELETE FROM " . TB_PREF . "wh_material_requests WHERE request_id = " . (int)$request_id;
	db_query($sql, 'could not delete material request');

	commit_transaction();
	return true;
}

// =====================================================================
// CRUD — Material Request Lines
// =====================================================================

/**
 * Add a line to a material request.
 *
 * @param int         $request_id    Request header ID
 * @param string      $stock_id      Item stock_id
 * @param float       $qty_requested Requested quantity
 * @param string|null $required_date Line-level required date (SQL)
 * @param string|null $memo          Line memo
 * @return int Inserted line_id
 */
function add_material_request_line($request_id, $stock_id, $qty_requested,
	$required_date = null, $memo = null)
{
	$sql = "INSERT INTO " . TB_PREF . "wh_material_request_lines (
		request_id, stock_id, qty_requested, qty_fulfilled, required_date, memo
	) VALUES ("
		. (int)$request_id . ","
		. db_escape($stock_id) . ","
		. db_escape($qty_requested) . ","
		. "0,"
		. ($required_date ? db_escape($required_date) : 'NULL') . ","
		. ($memo ? db_escape($memo) : 'NULL')
		. ")";

	db_query($sql, 'could not insert material request line');
	return db_insert_id();
}

/**
 * Update a material request line.
 *
 * @param int         $line_id        Line ID
 * @param float       $qty_requested  Requested quantity
 * @param string|null $required_date  Line-level required date (SQL)
 * @param string|null $memo           Line memo
 * @return bool Success
 */
function update_material_request_line($line_id, $qty_requested, $required_date = null, $memo = null) {
	$sql = "UPDATE " . TB_PREF . "wh_material_request_lines SET "
		. "qty_requested = " . db_escape($qty_requested) . ","
		. "required_date = " . ($required_date ? db_escape($required_date) : 'NULL') . ","
		. "memo = " . ($memo ? db_escape($memo) : 'NULL')
		. " WHERE line_id = " . (int)$line_id;

	db_query($sql, 'could not update material request line');
	return db_num_affected_rows() > 0;
}

/**
 * Delete a material request line.
 *
 * @param int $line_id Line ID
 * @return void
 */
function delete_material_request_line($line_id) {
	$sql = "DELETE FROM " . TB_PREF . "wh_material_request_lines WHERE line_id = " . (int)$line_id;
	db_query($sql, 'could not delete material request line');
}

/**
 * Get lines for a material request with joined item info.
 *
 * @param int $request_id Request ID
 * @return resource DB result set
 */
function get_material_request_lines($request_id) {
	$sql = "SELECT mrl.*,
		sm.description AS item_description,
		sm.units AS item_units,
		sm.material_cost,
		sm.track_by
	FROM " . TB_PREF . "wh_material_request_lines mrl
	LEFT JOIN " . TB_PREF . "stock_master sm ON sm.stock_id = mrl.stock_id
	WHERE mrl.request_id = " . (int)$request_id
	. " ORDER BY mrl.line_id";

	return db_query($sql, 'could not get material request lines');
}

/**
 * Get a single material request line.
 *
 * @param int $line_id Line ID
 * @return array|false Line row or false
 */
function get_material_request_line($line_id) {
	$sql = "SELECT mrl.*,
		sm.description AS item_description,
		sm.units AS item_units
	FROM " . TB_PREF . "wh_material_request_lines mrl
	LEFT JOIN " . TB_PREF . "stock_master sm ON sm.stock_id = mrl.stock_id
	WHERE mrl.line_id = " . (int)$line_id;

	$result = db_query($sql, 'could not get material request line');
	return db_fetch($result);
}

/**
 * Count lines in a material request.
 *
 * @param int $request_id Request ID
 * @return int Line count
 */
function count_material_request_lines($request_id) {
	$sql = "SELECT COUNT(*) AS cnt FROM " . TB_PREF . "wh_material_request_lines WHERE request_id = " . (int)$request_id;
	$result = db_query($sql, 'could not count lines');
	$row = db_fetch($result);
	return (int)$row['cnt'];
}

// =====================================================================
// LIFECYCLE — Status Transitions
// =====================================================================

/**
 * Submit a material request for approval.
 * Transitions: draft → submitted
 *
 * @param int $request_id Request ID
 * @return bool Success
 */
function submit_material_request($request_id) {
	$mr = get_material_request($request_id);
	if (!$mr || $mr['status'] !== 'draft')
		return false;

	// Must have at least one line
	if (count_material_request_lines($request_id) == 0)
		return false;

	$sql = "UPDATE " . TB_PREF . "wh_material_requests SET "
		. "status = 'submitted',"
		. "updated_at = NOW()"
		. " WHERE request_id = " . (int)$request_id
		. " AND status = 'draft'";

	db_query($sql, 'could not submit material request');
	return db_num_affected_rows() > 0;
}

/**
 * Approve a material request.
 * Transitions: submitted → approved
 *
 * @param int $request_id Request ID
 * @return bool Success
 */
function approve_material_request($request_id) {
	$mr = get_material_request($request_id);
	if (!$mr || $mr['status'] !== 'submitted')
		return false;

	$sql = "UPDATE " . TB_PREF . "wh_material_requests SET "
		. "status = 'approved',"
		. "approved_by = " . (int)$_SESSION['wa_current_user']->user . ","
		. "updated_at = NOW()"
		. " WHERE request_id = " . (int)$request_id
		. " AND status = 'submitted'";

	db_query($sql, 'could not approve material request');
	return db_num_affected_rows() > 0;
}

/**
 * Reject a material request (return to draft for revision).
 * Transitions: submitted → draft
 *
 * @param int $request_id Request ID
 * @return bool Success
 */
function reject_material_request($request_id) {
	$mr = get_material_request($request_id);
	if (!$mr || $mr['status'] !== 'submitted')
		return false;

	$sql = "UPDATE " . TB_PREF . "wh_material_requests SET "
		. "status = 'draft',"
		. "updated_at = NOW()"
		. " WHERE request_id = " . (int)$request_id
		. " AND status = 'submitted'";

	db_query($sql, 'could not reject material request');
	return db_num_affected_rows() > 0;
}

/**
 * Cancel a material request.
 * Transitions: draft|submitted|approved → cancelled
 *
 * @param int $request_id Request ID
 * @return bool Success
 */
function cancel_material_request($request_id) {
	$mr = get_material_request($request_id);
	if (!$mr || !in_array($mr['status'], array('draft', 'submitted', 'approved')))
		return false;

	$sql = "UPDATE " . TB_PREF . "wh_material_requests SET "
		. "status = 'cancelled',"
		. "updated_at = NOW()"
		. " WHERE request_id = " . (int)$request_id
		. " AND status IN ('draft', 'submitted', 'approved')";

	db_query($sql, 'could not cancel material request');
	return db_num_affected_rows() > 0;
}

/**
 * Mark a material request as ordered (linked to downstream document).
 * Transitions: approved → ordered
 *
 * @param int    $request_id    Request ID
 * @param int    $linked_type   Linked document type (ST_PURCHORDER, ST_LOCTRANSFER, ST_WORKORDER)
 * @param int    $linked_no     Linked document number
 * @return bool Success
 */
function mark_material_request_ordered($request_id, $linked_type, $linked_no) {
	$mr = get_material_request($request_id);
	if (!$mr || $mr['status'] !== 'approved')
		return false;

	$sql = "UPDATE " . TB_PREF . "wh_material_requests SET "
		. "status = 'ordered',"
		. "linked_doc_type = " . (int)$linked_type . ","
		. "linked_doc_no = " . (int)$linked_no . ","
		. "updated_at = NOW()"
		. " WHERE request_id = " . (int)$request_id
		. " AND status = 'approved'";

	db_query($sql, 'could not mark material request as ordered');
	return db_num_affected_rows() > 0;
}

/**
 * Update fulfillment quantity for a material request line.
 *
 * @param int   $line_id        Line ID
 * @param float $qty_fulfilled  Total fulfilled quantity
 * @return void
 */
function update_material_request_line_fulfillment($line_id, $qty_fulfilled) {
	$sql = "UPDATE " . TB_PREF . "wh_material_request_lines SET "
		. "qty_fulfilled = " . db_escape($qty_fulfilled)
		. " WHERE line_id = " . (int)$line_id;

	db_query($sql, 'could not update line fulfillment');
}

/**
 * Check if all lines of a material request are fully fulfilled.
 * If so, automatically transition status to 'fulfilled'.
 *
 * @param int $request_id Request ID
 * @return bool True if all lines fulfilled
 */
function check_material_request_fulfillment($request_id) {
	$sql = "SELECT COUNT(*) AS total,
		SUM(CASE WHEN qty_fulfilled >= qty_requested THEN 1 ELSE 0 END) AS fulfilled_count
	FROM " . TB_PREF . "wh_material_request_lines
	WHERE request_id = " . (int)$request_id;

	$result = db_query($sql, 'could not check fulfillment');
	$row = db_fetch($result);

	if ($row['total'] > 0 && $row['fulfilled_count'] == $row['total']) {
		$sql = "UPDATE " . TB_PREF . "wh_material_requests SET "
			. "status = 'fulfilled',"
			. "updated_at = NOW()"
			. " WHERE request_id = " . (int)$request_id
			. " AND status IN ('ordered', 'approved')";
		db_query($sql, 'could not mark request as fulfilled');
		return true;
	}
	return false;
}

// =====================================================================
// CONVERSION — Create Downstream Documents
// =====================================================================

/**
 * Convert an approved material request to a Purchase Order.
 * Creates a new PO header and lines from the MR lines.
 *
 * @param int    $request_id  Material request ID
 * @param int    $supplier_id Supplier to order from
 * @param string $delivery_to Location code for delivery
 * @return int|false PO transaction number or false on failure
 */
function convert_material_request_to_po($request_id, $supplier_id, $delivery_to) {
	global $path_to_root;

	$mr = get_material_request($request_id);
	if (!$mr || $mr['status'] !== 'approved')
		return false;

	include_once($path_to_root . '/purchasing/includes/db/po_db.inc');
	include_once($path_to_root . '/includes/ui/items_cart.inc');

	// Build a purchase order cart
	$cart = new items_cart(ST_PURCHORDER);
	$cart->supplier_id = $supplier_id;
	$cart->delivery_address = $delivery_to;
	$cart->Comments = sprintf(_('Created from Material Request %s'), format_material_request_no($mr['request_no']));
	$cart->Location = $mr['warehouse_loc_code'];
	$cart->tran_date = Today();
	$cart->due_date = $mr['required_date'] ? sql2date($mr['required_date']) : Today();

	$lines = get_material_request_lines($request_id);
	while ($line = db_fetch($lines)) {
		$outstanding = $line['qty_requested'] - $line['qty_fulfilled'];
		if ($outstanding <= 0)
			continue;

		$cart->add_to_cart(
			count($cart->line_items),
			$line['stock_id'],
			$outstanding,
			0, // unit_price — user should set this in the PO UI
			'',
			0, 0,
			$line['item_description'],
			$line['item_units']
		);
	}

	if (count($cart->line_items) == 0)
		return false;

	$po_no = add_po($cart);

	if ($po_no) {
		mark_material_request_ordered($request_id, ST_PURCHORDER, $po_no);
	}

	return $po_no;
}

/**
 * Convert an approved material request to a Transfer Order.
 * Creates a new TO from the MR lines.
 *
 * @param int    $request_id      Material request ID
 * @param string $from_loc_code   Source location code
 * @return int|false Transfer Order ID or false on failure
 */
function convert_material_request_to_transfer($request_id, $from_loc_code) {
	global $path_to_root;

	$mr = get_material_request($request_id);
	if (!$mr || $mr['status'] !== 'approved')
		return false;

	include_once($path_to_root . '/inventory/warehouse/includes/db/warehouse_transfers_db.inc');

	$request_date = $mr['request_date'];
	$expected_recv = $mr['required_date'];
	$reference = sprintf(_('From MR %s'), format_material_request_no($mr['request_no']));

	begin_transaction();

	$to_id = add_transfer_order(
		$from_loc_code,
		$mr['warehouse_loc_code'],
		$request_date,
		'standard',
		$reference,
		$mr['memo'],
		null,
		$expected_recv
	);

	$lines = get_material_request_lines($request_id);
	while ($line = db_fetch($lines)) {
		$outstanding = $line['qty_requested'] - $line['qty_fulfilled'];
		if ($outstanding <= 0)
			continue;

		add_transfer_order_line(
			$to_id,
			$line['stock_id'],
			$outstanding
		);
	}

	mark_material_request_ordered($request_id, ST_LOCTRANSFER, $to_id);

	commit_transaction();

	return $to_id;
}

/**
 * Convert an approved material request to a Work Order.
 *
 * This creates a simple work order reference link.
 * The actual WO creation requires specific BOM, quantity and scheduling
 * which the user should complete in the manufacturing module.
 *
 * @param int $request_id Material request ID
 * @return array Lines to produce (for user to create WOs from)
 */
function get_material_request_manufacturing_lines($request_id) {
	$mr = get_material_request($request_id);
	if (!$mr || $mr['status'] !== 'approved')
		return array();

	$items = array();
	$lines = get_material_request_lines($request_id);
	while ($line = db_fetch($lines)) {
		$outstanding = $line['qty_requested'] - $line['qty_fulfilled'];
		if ($outstanding <= 0)
			continue;

		$items[] = array(
			'line_id'     => $line['line_id'],
			'stock_id'    => $line['stock_id'],
			'description' => $line['item_description'],
			'qty'         => $outstanding,
			'units'       => $line['item_units'],
		);
	}
	return $items;
}

// =====================================================================
// QUERIES — Filtered Lists and Summaries
// =====================================================================

/**
 * Get material requests filtered by criteria.
 *
 * @param array $filters    Associative array of filter criteria
 * @param int   $limit      Max rows to return
 * @param int   $offset     Offset for pagination
 * @return resource DB result set
 */
function get_material_requests_filtered($filters = array(), $limit = 100, $offset = 0) {
	$sql = "SELECT mr.*,
		loc.location_name AS warehouse_name,
		u1.real_name AS requested_by_name,
		u2.real_name AS approved_by_name,
		COUNT(mrl.line_id) AS line_count,
		SUM(mrl.qty_requested) AS total_qty_requested,
		SUM(mrl.qty_fulfilled) AS total_qty_fulfilled
	FROM " . TB_PREF . "wh_material_requests mr
	LEFT JOIN " . TB_PREF . "locations loc ON loc.loc_code = mr.warehouse_loc_code
	LEFT JOIN " . TB_PREF . "users u1 ON u1.id = mr.requested_by
	LEFT JOIN " . TB_PREF . "users u2 ON u2.id = mr.approved_by
	LEFT JOIN " . TB_PREF . "wh_material_request_lines mrl ON mrl.request_id = mr.request_id
	WHERE 1=1";

	if (!empty($filters['status']))
		$sql .= " AND mr.status = " . db_escape($filters['status']);
	if (!empty($filters['request_type']))
		$sql .= " AND mr.request_type = " . db_escape($filters['request_type']);
	if (!empty($filters['warehouse_loc_code']) && $filters['warehouse_loc_code'] !== 'all')
		$sql .= " AND mr.warehouse_loc_code = " . db_escape($filters['warehouse_loc_code']);
	if (!empty($filters['date_from']))
		$sql .= " AND mr.request_date >= " . db_escape($filters['date_from']);
	if (!empty($filters['date_to']))
		$sql .= " AND mr.request_date <= " . db_escape($filters['date_to']);

	$sql .= " GROUP BY mr.request_id"
		. " ORDER BY mr.request_id DESC"
		. " LIMIT " . (int)$limit . " OFFSET " . (int)$offset;

	return db_query($sql, 'could not get material requests');
}

/**
 * Count material requests matching filters.
 *
 * @param array $filters Filter criteria
 * @return int Count
 */
function count_material_requests_filtered($filters = array()) {
	$sql = "SELECT COUNT(*) AS cnt FROM " . TB_PREF . "wh_material_requests mr WHERE 1=1";

	if (!empty($filters['status']))
		$sql .= " AND mr.status = " . db_escape($filters['status']);
	if (!empty($filters['request_type']))
		$sql .= " AND mr.request_type = " . db_escape($filters['request_type']);
	if (!empty($filters['warehouse_loc_code']) && $filters['warehouse_loc_code'] !== 'all')
		$sql .= " AND mr.warehouse_loc_code = " . db_escape($filters['warehouse_loc_code']);

	$result = db_query($sql, 'could not count material requests');
	$row = db_fetch($result);
	return (int)$row['cnt'];
}

/**
 * Get summary counts by status.
 *
 * @return array status => count
 */
function get_material_request_summary() {
	$sql = "SELECT status, COUNT(*) AS cnt FROM " . TB_PREF . "wh_material_requests GROUP BY status";
	$result = db_query($sql, 'could not get material request summary');

	$summary = array();
	while ($row = db_fetch($result)) {
		$summary[$row['status']] = (int)$row['cnt'];
	}
	return $summary;
}

/**
 * Get fulfillment percentage for a material request.
 *
 * @param int $request_id Request ID
 * @return float Percentage fulfilled (0-100)
 */
function get_material_request_fulfillment_pct($request_id) {
	$sql = "SELECT
		SUM(qty_requested) AS total_requested,
		SUM(qty_fulfilled) AS total_fulfilled
	FROM " . TB_PREF . "wh_material_request_lines
	WHERE request_id = " . (int)$request_id;

	$result = db_query($sql, 'could not get fulfillment pct');
	$row = db_fetch($result);

	if (!$row || $row['total_requested'] == 0)
		return 0;

	return round(($row['total_fulfilled'] / $row['total_requested']) * 100, 1);
}

/**
 * Get material requests that are pending approval.
 *
 * @return resource DB result set
 */
function get_pending_material_requests() {
	return get_material_requests_filtered(array('status' => 'submitted'));
}

/**
 * Get material requests awaiting conversion (approved but not yet ordered).
 *
 * @return resource DB result set
 */
function get_approvable_material_requests() {
	return get_material_requests_filtered(array('status' => 'approved'));
}

/**
 * Get total estimated value for a material request.
 *
 * @param int $request_id Request ID
 * @return float Estimated total value
 */
function get_material_request_total_value($request_id) {
	$sql = "SELECT SUM(mrl.qty_requested * sm.material_cost) AS total_value
	FROM " . TB_PREF . "wh_material_request_lines mrl
	LEFT JOIN " . TB_PREF . "stock_master sm ON sm.stock_id = mrl.stock_id
	WHERE mrl.request_id = " . (int)$request_id;

	$result = db_query($sql, 'could not get material request total value');
	$row = db_fetch($result);
	return $row ? (float)$row['total_value'] : 0;
}
