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
 * Preview the next native request number. Construction must use its locked
 * writer, which also accounts for retained approval/reservation numbers.
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
	global $path_to_root;
	require_once $path_to_root.'/inventory/warehouse/includes/db/warehouse_material_create_db.inc';
	return create_material_request_header($request_type, $warehouse_loc, $request_date,
		$required_date, $reference, $memo);
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
	require_once __DIR__.'/warehouse_material_header_db.inc';
	return write_standalone_material_header($request_id, $request_type, $warehouse_loc,
		$request_date, $required_date, $reference, $memo);
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
	require_once __DIR__.'/warehouse_material_core_db.inc';
	return delete_standalone_material_core($request_id);
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
	require_once __DIR__.'/warehouse_material_line_db.inc';
	return write_standalone_material_line('add', $request_id, array('stock_id'=>$stock_id,
		'quantity'=>$qty_requested, 'required_date'=>$required_date, 'memo'=>$memo));
}

/**
 * Update a material request line.
 *
 * @param int         $line_id        Line ID
 * @param float       $qty_requested  Requested quantity
 * @param string|null $required_date  Line-level required date (SQL)
 * @param string|null $memo           Line memo
 * @param int|null    $request_id     Optional expected parent from the native editor
 * @return bool Success
 */
function update_material_request_line($line_id, $qty_requested, $required_date = null, $memo = null, $request_id = null) {
	require_once __DIR__.'/warehouse_material_line_db.inc';
	return write_standalone_material_line('update', $line_id, array('quantity'=>$qty_requested,
		'required_date'=>$required_date, 'memo'=>$memo), $request_id);
}

/**
 * Delete a material request line.
 *
 * @param int $line_id Line ID
 * @param int|null $request_id Optional expected parent from the native editor
 * @return bool Success
 */
function delete_material_request_line($line_id, $request_id = null) {
	require_once __DIR__.'/warehouse_material_line_db.inc';
	return write_standalone_material_line('delete', $line_id, array(), $request_id);
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

/**
 * Find one exactly identified pending core review. This is a reader, not
 * authority to approve or recover ambiguous/legacy history.
 *
 * @param int $request_id Request ID
 * @return array|false Draft row or false
 */
function get_pending_material_request_approval_draft($request_id)
{
	require_once __DIR__.'/warehouse_material_core_db.inc';
	$context = material_core_context();
	$identity = dimension_writer_reference_id($request_id);
	if (!$context || !$identity) return false;
	$rows = material_core_rows('SELECT * FROM '.TB_PREF.'approval_drafts WHERE trans_type='
		.ST_WH_MATERIAL_REQUEST.' AND status=0 ORDER BY id');
	if ($rows === false) return false;
	$match = false;
	$approval = get_approval_workflow_service();
	foreach ($rows as $row) {
		$data = $approval->decodeStoredDraftData($row['draft_data']);
		if (!is_array($data) || !isset($data['request_id'])
			|| !($owner = dimension_writer_reference_id($data['request_id']))) return false;
		if ($owner !== $identity) continue;
		if ($match !== false || (isset($data['material_company_sha256'])
			&& $data['material_company_sha256'] !== material_core_context(true))) return false;
		$match = $row;
	}
	return material_core_context() === $context ? $match : false;
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
	return submit_material_request_for_core_approval($request_id) !== false;
}

/**
 * Build material request draft payload for the core approval engine.
 *
 * @param int $request_id Request ID
 * @return array|false
 */
function get_material_request_submission_data($request_id) {
	$mr = get_material_request($request_id);
	if (!$mr)
		return false;

	$lines_data = array();
	$lines = get_material_request_lines($request_id);
	while ($line = db_fetch($lines)) {
		$lines_data[] = array(
			'line_id' => (int)$line['line_id'],
			'stock_id' => $line['stock_id'],
			'description' => isset($line['item_description']) ? $line['item_description'] : '',
			'units' => isset($line['item_units']) ? $line['item_units'] : '',
			'qty_requested' => (float)$line['qty_requested'],
			'qty_fulfilled' => (float)$line['qty_fulfilled'],
			'unit_cost' => isset($line['material_cost']) ? (float)$line['material_cost'] : 0,
			'required_date' => $line['required_date'] ? $line['required_date'] : '',
			'memo' => $line['memo'] === null ? '' : $line['memo'],
		);
	}

	$statuses = get_material_request_statuses();
	$types = get_material_request_types();

	return array(
		'request_id' => (int)$mr['request_id'],
		'request_no' => (int)$mr['request_no'],
		'request_no_formatted' => format_material_request_no($mr['request_no']),
		'request_type' => $mr['request_type'],
		'request_type_label' => isset($types[$mr['request_type']]) ? $types[$mr['request_type']] : $mr['request_type'],
		'status' => $mr['status'],
		'status_label' => isset($statuses[$mr['status']]) ? $statuses[$mr['status']] : $mr['status'],
		'warehouse_loc_code' => $mr['warehouse_loc_code'],
		'warehouse_name' => isset($mr['warehouse_name']) ? $mr['warehouse_name'] : '',
		'requested_by' => (int)$mr['requested_by'],
		'requested_by_name' => isset($mr['requested_by_name']) ? $mr['requested_by_name'] : '',
		'request_date' => $mr['request_date'],
		'required_date' => $mr['required_date'] ? $mr['required_date'] : '',
		'reference' => $mr['reference'] === null ? '' : $mr['reference'],
		'memo' => $mr['memo'] === null ? '' : $mr['memo'],
		'total_estimated' => (float)get_material_request_total_value($request_id),
		'line_count' => count($lines_data),
		'lines' => $lines_data,
	);
}

/**
 * Submit one material request into the core approval workflow.
 *
 * @param int $request_id Request ID
 * @return array|bool
 */
function submit_material_request_for_core_approval($request_id) {
	require_once __DIR__.'/warehouse_material_core_db.inc';
	return submit_standalone_material_to_core($request_id);
}

/**
 * Approve a material request.
 * Transitions: submitted → approved
 *
 * @param int $request_id Request ID
 * @return bool Success
 */
function approve_material_request($request_id) {
	// Only the registered core executor owns final approval.
	return false;
}

/**
 * Reject a material request (return to draft for revision).
 * Transitions: submitted → draft
 *
 * @param int $request_id Request ID
 * @return bool Success
 */
function reject_material_request($request_id) {
	// Only the registered core rejector owns reviewed-source rejection.
	return false;
}

/**
 * Cancel a material request.
 * Transitions: draft|submitted|approved → cancelled
 *
 * @param int $request_id Request ID
 * @return bool Success
 */
function cancel_material_request($request_id, $comments = '') {
	require_once __DIR__.'/warehouse_material_core_db.inc';
	$request_id = dimension_writer_reference_id($request_id);
	if (!material_core_context() || !$request_id || !is_string($comments) || strpos($comments, "\0") !== false) return false;
	$mr = get_material_request($request_id);
	if (!$mr || !in_array($mr['status'], array('draft', 'submitted', 'approved')))
		return false;
	if ($mr['status'] === 'draft') {
		require_once __DIR__.'/warehouse_material_cancel_db.inc';
		return cancel_standalone_material_draft($request_id, $comments);
	}

	if ($mr['status'] === 'submitted') {
		require_once __DIR__.'/warehouse_material_core_db.inc';
		return cancel_standalone_material_core($request_id, $comments);
	}

	require_once __DIR__.'/warehouse_material_terminal_db.inc';
	return cancel_standalone_material_approved($request_id, $comments);
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
 * Build the current locked material source and its native zero-price PO intent.
 * The caller owns a transaction; this helper never consumes a reference or source.
 * @return array Locked source, source queries and company-bound native cart
 */
function prepare_material_request_purchase_order($request_id, $supplier_id, $order_date = null) {
	if (empty($GLOBALS['transaction_level']) || !isset($_SESSION['wa_current_user'])
		|| !$_SESSION['wa_current_user']->can_access('SA_MATERIALREQUEST')
		|| !$_SESSION['wa_current_user']->can_access('SA_PURCHASEORDER')
		|| !is_scalar($request_id) || (string)(int)$request_id !== (string)$request_id || (int)$request_id <= 0
		|| !is_scalar($supplier_id) || (string)(int)$supplier_id !== (string)$supplier_id || (int)$supplier_id <= 0)
		throw new RuntimeException('Material request purchase authority is unavailable.');
	$cart = new purch_order();
	if (!$cart->is_current_company())
		throw new RuntimeException('Material request company is unavailable.');
	include_once(__DIR__.'/warehouse_material_terminal_db.inc');
	// Take history before source/children, as native approval and cancellation do.
	// An old status alone cannot authorize a new downstream command.
	$custody = material_approved_custody((int)$request_id);
	if (!$custody)
		throw new RuntimeException('Material request approved-source custody requires recovery.');
	// Current locking reads also reject stale outer-transaction snapshots.
	$source_query = 'SELECT * FROM '.TB_PREF.'wh_material_requests WHERE request_id='.(int)$request_id.' FOR UPDATE';
	$mr = db_fetch_assoc(db_query($source_query, 'could not lock material request conversion'));
	if (!$mr || $mr['status'] !== 'approved' || $mr['request_type'] !== 'purchase'
		|| !empty($mr['linked_doc_no']))
		throw new RuntimeException('Material request is not available for conversion.');
	$location = db_fetch_assoc(db_query('SELECT * FROM '.TB_PREF.'locations WHERE loc_code='
		.db_escape($mr['warehouse_loc_code']).' AND inactive=0 FOR UPDATE', 'could not verify material request location'));
	$supplier = db_fetch_assoc(db_query('SELECT supplier_id FROM '.TB_PREF.'suppliers WHERE supplier_id='
		.(int)$supplier_id.' AND inactive=0 FOR UPDATE', 'could not verify material request supplier'));
	if (!$location || !$supplier)
		throw new RuntimeException('Material request destination is unavailable.');
	$cart->trans_type = ST_PURCHORDER;
	$cart->orig_order_date = $order_date === null ? Today() : $order_date;
	$cart->due_date = $mr['required_date'] ? sql2date($mr['required_date']) : $cart->orig_order_date;
	$cart->Location = $location['loc_code'];
	$cart->delivery_address = $location['delivery_address'] !== '' ? $location['delivery_address'] : $location['location_name'];
	$cart->Comments = sprintf(_('Created from Material Request %s'), format_material_request_no($mr['request_no']));
	get_supplier_details_to_order($cart, (int)$supplier_id, true);
	$cart->reference = 'auto';
	$line_query = 'SELECT line.*, item.description AS item_description, item.units AS item_units,
		item.editable AS item_editable, item.tax_type_id AS item_tax_type, tax.name AS item_tax_name,
		purchase.supplier_description AS supplier_description
		FROM '.TB_PREF.'wh_material_request_lines line
		LEFT JOIN '.TB_PREF.'stock_master item ON item.stock_id=line.stock_id
		LEFT JOIN '.TB_PREF.'item_tax_types tax ON tax.id=item.tax_type_id
		LEFT JOIN '.TB_PREF.'purch_data purchase ON purchase.stock_id=line.stock_id AND purchase.supplier_id='.(int)$supplier_id.'
		WHERE line.request_id='.(int)$request_id.' ORDER BY line.line_id FOR UPDATE';
	$lines = db_query($line_query, 'could not lock material request conversion lines');
	$source_lines = array();
	while ($line = db_fetch_assoc($lines)) {
		$source_lines[] = $line;
		$outstanding = (float)$line['qty_requested'] - (float)$line['qty_fulfilled'];
		if (!is_finite($outstanding) || $line['qty_requested'] < 0 || $line['qty_fulfilled'] < 0
			|| $line['item_description'] === null)
			throw new RuntimeException('Material request line is unavailable.');
		if ($outstanding <= 0)
			continue;
		// Preserve the existing zero-price conversion policy; price is edited in the PO UI.
		$cart->add_to_order(count($cart->line_items), $line['stock_id'], $outstanding,
			$line['item_description'], 0, $line['item_units'],
			$line['required_date'] ? sql2date($line['required_date']) : $cart->due_date, 0, 0);
		// Constructors use legacy nonlocking readers. The locked source owns the
		// material intent, including the native noneditable supplier description.
		$item = $cart->line_items[count($cart->line_items)-1];
		$item->descr_editable = $line['item_editable'];
		$item->units = $line['item_units'];
		$item->tax_type = $line['item_tax_type'];
		$item->tax_type_name = $line['item_tax_name'];
		if (!$item->descr_editable && $line['supplier_description'] !== null && $line['supplier_description'] !== '')
			$item->item_description = $line['supplier_description'];
	}
	if (!$cart->order_has_items())
		throw new RuntimeException('Material request has no outstanding lines.');
	return array('cart'=>$cart, 'header'=>$mr, 'lines'=>$source_lines, 'material_custody'=>$custody,
		'header_query'=>$source_query, 'line_query'=>$line_query);
}

/** Post and link the locked source atomically; callers own direct-policy or actual approved-draft admission. */
function write_material_request_purchase_order($snapshot, $approval_draft_id = 0) {
	if (empty($GLOBALS['transaction_level']))
		throw new RuntimeException('Material purchase linkage requires an owning transaction.');
	$cart = $snapshot['cart'];
	$request_id = (int)$snapshot['header']['request_id'];
	$context = material_core_context();
	if (!$context || !isset($snapshot['material_custody'])
		|| material_approved_custody($request_id) !== $snapshot['material_custody'])
		throw new RuntimeException('Material request approval history changed.');
	$intent = json_decode(json_encode($cart), true);
	if (!is_array($intent) || json_last_error() !== JSON_ERROR_NONE)
		throw new RuntimeException('Material purchase intent could not be encoded.');
	$po_no = add_po($cart, $approval_draft_id);
	if (!$po_no || !$cart->is_current_company()
		|| !$_SESSION['wa_current_user']->can_access('SA_MATERIALREQUEST')
		|| !$_SESSION['wa_current_user']->can_access('SA_PURCHASEORDER'))
		throw new RuntimeException('Material request purchase order failed.');
	verify_material_request_purchase_order_effect($intent, $cart, $po_no);
	// Locks exclude other connections; native hooks in this transaction can
	// still change source intent. Never link an order built from that old intent.
	if ($snapshot['header'] !== db_fetch_assoc(db_query($snapshot['header_query'], 'could not recheck material request conversion')))
		throw new RuntimeException('Material request source changed.');
	$lines = db_query($snapshot['line_query'], 'could not recheck material request conversion lines');
	$current_lines = array();
	while ($line = db_fetch_assoc($lines))
		$current_lines[] = $line;
	if ($snapshot['lines'] !== $current_lines)
		throw new RuntimeException('Material request source lines changed.');
	$clock = material_core_rows('SELECT NOW() AS now_');
	if (!$clock || !db_query('UPDATE '.TB_PREF."wh_material_requests SET status='ordered', linked_doc_type=".ST_PURCHORDER
		.', linked_doc_no='.(int)$po_no.", updated_at=NOW() WHERE request_id=".(int)$request_id
		." AND status='approved' AND (linked_doc_no IS NULL OR linked_doc_no=0)", 'could not link material request purchase order')
		|| db_num_affected_rows() !== 1)
		throw new RuntimeException('Material request linkage changed.');
	// Verify the whole terminal source after linking, including trigger/hook effects.
	$headers = material_core_rows($snapshot['header_query']);
	$finished = material_core_rows('SELECT NOW() AS now_');
	$expected = $snapshot['header'];
	if (!$headers || count($headers) !== 1 || !$finished || !$headers[0]['updated_at']
		|| $headers[0]['updated_at'] < $clock[0]['now_'] || $headers[0]['updated_at'] > $finished[0]['now_'])
		throw new RuntimeException('Material request completion is unavailable.');
	$expected['status'] = 'ordered';
	$expected['linked_doc_type'] = $headers[0]['linked_doc_type'];
	$expected['linked_doc_no'] = $headers[0]['linked_doc_no'];
	$expected['updated_at'] = $headers[0]['updated_at'];
	if ((int)$expected['linked_doc_type'] !== ST_PURCHORDER || (int)$expected['linked_doc_no'] !== (int)$po_no
		|| !material_purchase_source_matches($snapshot, $expected)
		|| material_core_context() !== $context)
		throw new RuntimeException('Material request completion changed approved source or history.');
	if ($approval_draft_id) {
		$state = material_po_completion('peek');
		if (!$state || $state['draft_id'] !== (int)$approval_draft_id || $state['context'] !== $context)
			throw new RuntimeException('Material purchase core completion custody is unavailable.');
		$state['terminal'] = $headers[0]; $state['intent'] = $intent;
		$state['snapshot'] = $snapshot;
		material_po_completion('set', $state);
	}
	return (int)$po_no;
}

/**
 * Convert an approved material request to a Purchase Order.
 * Creates a new PO header and lines from the MR lines.
 *
 * @param int    $request_id  Material request ID
 * @param int    $supplier_id Supplier to order from
 * @param string $delivery_to Legacy location argument; current source location remains authoritative
 * @return int|false PO transaction number or false on failure
 */
function convert_material_request_to_po($request_id, $supplier_id, $delivery_to) {
	global $path_to_root;
	if (!isset($_SESSION['wa_current_user'])
		|| !$_SESSION['wa_current_user']->can_access('SA_MATERIALREQUEST')
		|| !$_SESSION['wa_current_user']->can_access('SA_PURCHASEORDER')
		|| !is_scalar($request_id) || (string)(int)$request_id !== (string)$request_id || (int)$request_id <= 0
		|| !is_scalar($supplier_id) || (string)(int)$supplier_id !== (string)$supplier_id || (int)$supplier_id <= 0)
		return false;

	include_once($path_to_root . '/purchasing/includes/db/po_db.inc');
	include_once($path_to_root . '/purchasing/includes/po_class.inc');
	$cart = new purch_order();
	if (!$cart->is_current_company() || !begin_transaction())
		return false;
	try {
		include_once(__DIR__.'/warehouse_material_po_approval_db.inc');
		$snapshot = prepare_material_request_purchase_order($request_id, $supplier_id);
		$cart = $snapshot['cart'];
		if (material_request_purchase_order_has_pending($request_id))
			throw new RuntimeException('Material request purchase approval is pending.');
		// This legacy converter has no delayed-execution source envelope. Never
		// bypass a configured PO workflow by calling its native writer directly.
		if (!purchase_order_conversion_direct_authorized($cart))
			throw new RuntimeException('Material request requires purchase order approval.');
		if (!$cart->is_current_company() || !$_SESSION['wa_current_user']->can_access('SA_MATERIALREQUEST'))
			throw new RuntimeException('Material request authority changed.');
		$po_no = write_material_request_purchase_order($snapshot);
		if (!purchase_order_conversion_direct_authorized($cart))
			throw new RuntimeException('Material request purchase policy changed.');
		if (!commit_transaction()) {
			if (!empty($GLOBALS['transaction_level']))
				throw new RuntimeException('Material request conversion commit failed.');
			display_error(_('The purchase order conversion was committed, but resource cleanup needs recovery. Reload the source before continuing.'));
		}
		return (int)$po_no;
	} catch (Exception $exception) {
		cancel_transaction();
		return false;
	} catch (Throwable $exception) {
		cancel_transaction();
		throw $exception;
	}
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
