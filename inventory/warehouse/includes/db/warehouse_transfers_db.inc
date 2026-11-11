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
require_once __DIR__ . '/../../../includes/db/traceability_dimension_db.inc';

/**
 * Warehouse Transfer Orders — CRUD and lifecycle management.
 *
 * Transfer order lifecycle: Draft → Approved → Shipped → In Transit → Received → (Cancelled)
 * Partial receipts retain the outstanding dispatched quantity on this order.
 *
 * Tables: 0_wh_transfer_orders, 0_wh_transfer_order_lines
 *
 * @package NotrinosERP
 * @subpackage Inventory/Warehouse
 */

/**
 * Get all valid transfer order statuses.
 *
 * @return array status_code => label
 */
function get_transfer_order_statuses() {
	return array(
		'draft'      => _('Draft'),
		'submitted'  => _('Submitted'),
		'approved'   => _('Approved'),
		'shipped'    => _('Shipped'),
		'in_transit' => _('In Transit'),
		'received'   => _('Received'),
		'cancelled'  => _('Cancelled'),
	);
}

/**
 * Get the CSS color for a transfer order status.
 *
 * @param string $status Status code
 * @return string Hex color
 */
function get_transfer_order_status_color($status) {
	$colors = array(
		'draft'      => '#6c757d',
		'submitted'  => '#ffc107',
		'approved'   => '#007bff',
		'shipped'    => '#17a2b8',
		'in_transit' => '#fd7e14',
		'received'   => '#28a745',
		'cancelled'  => '#dc3545',
	);
	return isset($colors[$status]) ? $colors[$status] : '#6c757d';
}

/**
 * Get the CSS color for a transfer order type.
 *
 * @param string $type Transfer type
 * @return string Hex color
 */
function get_transfer_order_type_color($type) {
	$colors = array(
		'standard' => '#007bff',
		'urgent'   => '#dc3545',
		'backorder'=> '#fd7e14',
	);
	return isset($colors[$type]) ? $colors[$type] : '#007bff';
}

/**
 * Get the next sequential transfer number.
 *
 * @return int Next transfer_no
 */
function get_next_transfer_order_no() {
	$sql = "SELECT COALESCE(MAX(transfer_no), 0) + 1 AS next_no FROM " . TB_PREF . "wh_transfer_orders";
	$result = db_query($sql, 'could not get next transfer no');
	$row = db_fetch($result);
	return (int)$row['next_no'];
}

/**
 * Add a new transfer order header.
 *
 * @param string      $from_loc_code    Source location code
 * @param string      $to_loc_code      Destination location code
 * @param string      $request_date     Request date (SQL format)
 * @param string      $transfer_type    standard|urgent|backorder
 * @param string|null $reference        Optional reference
 * @param string|null $memo             Optional memo
 * @param string|null $expected_ship    Expected ship date (SQL)
 * @param string|null $expected_recv    Expected receive date (SQL)
 * @return int Inserted transfer_id
 */
function add_transfer_order($from_loc_code, $to_loc_code, $request_date,
	$transfer_type = 'standard', $reference = null, $memo = null,
	$expected_ship = null, $expected_recv = null)
{
    require_once __DIR__.'/warehouse_transfer_header_db.inc';
    return write_native_transfer_header(null, $from_loc_code, $to_loc_code, $request_date,
        $transfer_type, $reference, $memo, $expected_ship, $expected_recv);
}

/**
 * Add a line to a transfer order.
 *
 * @param int         $transfer_id  Transfer order ID
 * @param string      $stock_id     Item code
 * @param float       $qty          Requested quantity
 * @param int|null    $from_bin_id  Source bin (wh_locations.loc_id)
 * @param int|null    $to_bin_id    Destination bin
 * @param int|null    $batch_id     Batch ID
 * @param int|null    $serial_id    Serial ID
 * @param float       $unit_cost    Unit cost
 * @param string|null $memo         Line memo
 * @return int Inserted line_id
 */
function add_transfer_order_line($transfer_id, $stock_id, $qty,
    $from_bin_id = null, $to_bin_id = null, $batch_id = null, $serial_id = null, $unit_cost = 0, $memo = null)
{
    require_once __DIR__.'/warehouse_transfer_line_db.inc';
    return write_native_transfer_line('add', $transfer_id, array('stock_id'=>$stock_id, 'qty_requested'=>$qty,
        'from_bin_id'=>$from_bin_id, 'to_bin_id'=>$to_bin_id, 'batch_id'=>$batch_id, 'serial_id'=>$serial_id,
        'unit_cost'=>$unit_cost, 'memo'=>$memo));
}

/**
 * Update a transfer order header.
 *
 * @param int         $transfer_id     Transfer order ID
 * @param string      $from_loc_code   Source location
 * @param string      $to_loc_code     Destination location
 * @param string      $request_date    Request date (SQL)
 * @param string      $transfer_type   standard|urgent|backorder
 * @param string|null $reference       Reference
 * @param string|null $memo            Memo
 * @param string|null $expected_ship   Expected ship date
 * @param string|null $expected_recv   Expected receive date
 */
function update_transfer_order($transfer_id, $from_loc_code, $to_loc_code,
	$request_date, $transfer_type = 'standard', $reference = null, $memo = null,
	$expected_ship = null, $expected_recv = null)
{
    require_once __DIR__.'/warehouse_transfer_header_db.inc';
    return write_native_transfer_header($transfer_id, $from_loc_code, $to_loc_code, $request_date,
        $transfer_type, $reference, $memo, $expected_ship, $expected_recv);
}

/**
 * Update transfer order line.
 *
 * @param int         $line_id      Line ID
 * @param float       $qty          Requested quantity
 * @param int|null    $from_bin_id  Source bin
 * @param int|null    $to_bin_id    Destination bin
 * @param int|null    $batch_id     Batch ID
 * @param int|null    $serial_id    Serial ID
 * @param float       $unit_cost    Unit cost
 * @param string|null $memo         Line memo
 */
function update_transfer_order_line($line_id, $qty, $from_bin_id = null,
    $to_bin_id = null, $batch_id = null, $serial_id = null, $unit_cost = 0, $memo = null, $expected_parent = null)
{
    require_once __DIR__.'/warehouse_transfer_line_db.inc';
    return write_native_transfer_line('update', $line_id, array('qty_requested'=>$qty,
        'from_bin_id'=>$from_bin_id, 'to_bin_id'=>$to_bin_id, 'batch_id'=>$batch_id, 'serial_id'=>$serial_id,
        'unit_cost'=>$unit_cost, 'memo'=>$memo), $expected_parent);
}

/**
 * Delete a transfer order line.
 *
 * @param int $line_id Line ID
 */
function delete_transfer_order_line($line_id, $expected_parent = null) {
    require_once __DIR__.'/warehouse_transfer_line_db.inc';
    return write_native_transfer_line('delete', $line_id, array(), $expected_parent);
}

/**
 * Get a transfer order by ID.
 *
 * @param int $transfer_id Transfer order ID
 * @return array|false Transfer order row with location names
 */
function get_transfer_order($transfer_id) {
	$sql = "SELECT t.*,
		fl.location_name AS from_loc_name,
		tl.location_name AS to_loc_name,
		u1.real_name AS requested_by_name,
		u2.real_name AS approved_by_name,
		u3.real_name AS shipped_by_name,
		u4.real_name AS received_by_name
		FROM " . TB_PREF . "wh_transfer_orders t
		LEFT JOIN " . TB_PREF . "locations fl ON t.from_loc_code = fl.loc_code
		LEFT JOIN " . TB_PREF . "locations tl ON t.to_loc_code = tl.loc_code
		LEFT JOIN " . TB_PREF . "users u1 ON t.requested_by = u1.id
		LEFT JOIN " . TB_PREF . "users u2 ON t.approved_by = u2.id
		LEFT JOIN " . TB_PREF . "users u3 ON t.shipped_by = u3.id
		LEFT JOIN " . TB_PREF . "users u4 ON t.received_by = u4.id
		WHERE t.transfer_id=" . (int)$transfer_id;

	$result = db_query($sql, 'could not get transfer order');
	return db_fetch($result);
}

/**
 * Get transfer order lines.
 *
 * @param int $transfer_id Transfer order ID
 * @return resource DB result
 */
function get_transfer_order_lines($transfer_id) {
	$sql = "SELECT tl.*,
		sm.description AS item_description,
		sm.units AS item_units,
		sm.track_by,
		sm.has_serial_no,
		sm.has_batch_no,
		fb.loc_name AS from_bin_name,
		tb.loc_name AS to_bin_name,
		sn.serial_no,
		sb.batch_no
		FROM " . TB_PREF . "wh_transfer_order_lines tl
		LEFT JOIN " . TB_PREF . "stock_master sm ON tl.stock_id = sm.stock_id
		LEFT JOIN " . TB_PREF . "wh_locations fb ON tl.from_bin_id = fb.loc_id
		LEFT JOIN " . TB_PREF . "wh_locations tb ON tl.to_bin_id = tb.loc_id
		LEFT JOIN " . TB_PREF . "serial_numbers sn ON tl.serial_id = sn.id
		LEFT JOIN " . TB_PREF . "stock_batches sb ON tl.batch_id = sb.id
		WHERE tl.transfer_id=" . (int)$transfer_id
		. " ORDER BY tl.line_id ASC";

	return db_query($sql, 'could not get transfer order lines');
}

/**
 * Count lines for one transfer order.
 *
 * @param int $transfer_id Transfer order ID
 * @return int
 */
function count_transfer_order_lines($transfer_id)
{
	$sql = "SELECT COUNT(*) AS cnt FROM " . TB_PREF . "wh_transfer_order_lines"
		. " WHERE transfer_id = " . (int)$transfer_id;
	$result = db_query($sql, 'could not count transfer order lines');
	$row = db_fetch($result);
	return $row ? (int)$row['cnt'] : 0;
}

/**
 * Get filtered list of transfer orders.
 *
 * @param array $filters Associative array: status, from_loc_code, to_loc_code,
 *                       transfer_type, date_from, date_to
 * @param int   $limit   Max records
 * @param int   $offset  Offset
 * @return resource DB result
 */
function get_transfer_orders_filtered($filters = array(), $limit = 50, $offset = 0) {
	$sql = "SELECT t.*,
		fl.location_name AS from_loc_name,
		tl.location_name AS to_loc_name,
		u1.real_name AS requested_by_name,
		(SELECT COUNT(*) FROM " . TB_PREF . "wh_transfer_order_lines WHERE transfer_id = t.transfer_id) AS line_count,
		(SELECT SUM(qty_requested) FROM " . TB_PREF . "wh_transfer_order_lines WHERE transfer_id = t.transfer_id) AS total_qty
		FROM " . TB_PREF . "wh_transfer_orders t
		LEFT JOIN " . TB_PREF . "locations fl ON t.from_loc_code = fl.loc_code
		LEFT JOIN " . TB_PREF . "locations tl ON t.to_loc_code = tl.loc_code
		LEFT JOIN " . TB_PREF . "users u1 ON t.requested_by = u1.id
		WHERE 1=1";

	if (!empty($filters['status'])) {
		if (is_array($filters['status'])) {
			$statuses = array_map('db_escape', $filters['status']);
			$sql .= " AND t.status IN (" . implode(',', $statuses) . ")";
		} else {
			$sql .= " AND t.status=" . db_escape($filters['status']);
		}
	}
	if (!empty($filters['from_loc_code']))
		$sql .= " AND t.from_loc_code=" . db_escape($filters['from_loc_code']);
	if (!empty($filters['to_loc_code']))
		$sql .= " AND t.to_loc_code=" . db_escape($filters['to_loc_code']);
	if (!empty($filters['transfer_type']))
		$sql .= " AND t.transfer_type=" . db_escape($filters['transfer_type']);
	if (!empty($filters['date_from']))
		$sql .= " AND t.request_date>=" . db_escape($filters['date_from']);
	if (!empty($filters['date_to']))
		$sql .= " AND t.request_date<=" . db_escape($filters['date_to']);

	$sql .= " ORDER BY t.transfer_id DESC";
	$sql .= " LIMIT " . (int)$limit . " OFFSET " . (int)$offset;

	return db_query($sql, 'could not get transfer orders');
}

/**
 * Count filtered transfer orders.
 *
 * @param array $filters Same as get_transfer_orders_filtered
 * @return int Count
 */
function count_transfer_orders_filtered($filters = array()) {
	$sql = "SELECT COUNT(*) AS cnt FROM " . TB_PREF . "wh_transfer_orders t WHERE 1=1";

	if (!empty($filters['status'])) {
		if (is_array($filters['status'])) {
			$statuses = array_map('db_escape', $filters['status']);
			$sql .= " AND t.status IN (" . implode(',', $statuses) . ")";
		} else {
			$sql .= " AND t.status=" . db_escape($filters['status']);
		}
	}
	if (!empty($filters['from_loc_code']))
		$sql .= " AND t.from_loc_code=" . db_escape($filters['from_loc_code']);
	if (!empty($filters['to_loc_code']))
		$sql .= " AND t.to_loc_code=" . db_escape($filters['to_loc_code']);
	if (!empty($filters['transfer_type']))
		$sql .= " AND t.transfer_type=" . db_escape($filters['transfer_type']);
	if (!empty($filters['date_from']))
		$sql .= " AND t.request_date>=" . db_escape($filters['date_from']);
	if (!empty($filters['date_to']))
		$sql .= " AND t.request_date<=" . db_escape($filters['date_to']);

	$result = db_query($sql, 'could not count transfer orders');
	$row = db_fetch($result);
	return (int)$row['cnt'];
}

/**
 * Check if a transfer order can be deleted.
 *
 * @param int $transfer_id Transfer order ID
 * @return bool True if deletable (draft or cancelled only)
 */
function can_delete_transfer_order($transfer_id) {
	$order = get_transfer_order($transfer_id);
	if (!$order) return false;
	return in_array($order['status'], array('draft', 'cancelled'));
}

/**
 * Delete a transfer order and its lines.
 *
 * @param int $transfer_id Transfer order ID
 * @return bool Success
 */
function delete_transfer_order($transfer_id) {
    require_once __DIR__.'/warehouse_transfer_core_db.inc';
    $identity = dimension_writer_reference_id($transfer_id); $context = transfer_core_context();
    if (!$identity || !$context) return false;
    try {
        if (!begin_transaction()) return false;
        $history = transfer_core_history($identity);
        $rows = transfer_core_rows('SELECT status FROM '.TB_PREF.'wh_transfer_orders WHERE transfer_id='.$identity.' FOR UPDATE');
        $source = $rows ? transfer_core_source($identity, $rows[0]['status'], true) : false;
        if (!$history || $history['drafts'] || !$source || !in_array($source['header']['status'], array('draft','cancelled'), true)
            || (int)$source['header']['requested_by'] !== (int)$_SESSION['wa_current_user']->user
            || !empty($source['header']['approved_by']) || $source['header']['custom_data'] !== null) { cancel_transaction(); return false; }
        foreach ($source['lines'] as $line)
            if ($line['custom_data'] !== null) { cancel_transaction(); return false; }
        if (!db_query('DELETE FROM '.TB_PREF.'wh_transfer_order_lines WHERE transfer_id='.$identity)
            || !db_query('DELETE FROM '.TB_PREF.'wh_transfer_orders WHERE transfer_id='.$identity)
            || db_num_affected_rows() !== 1 || transfer_core_context() !== $context
            || transfer_core_history($identity) !== $history
            || transfer_core_rows('SELECT * FROM '.TB_PREF.'wh_transfer_orders WHERE transfer_id='.$identity.' FOR UPDATE') !== array()
            || transfer_core_rows('SELECT * FROM '.TB_PREF.'wh_transfer_order_lines WHERE transfer_id='.$identity.' FOR UPDATE') !== array()
            || !commit_transaction()) { cancel_transaction(); return false; }
        return true;
    } catch (Exception $exception) { cancel_transaction(); return false;
    } catch (Throwable $exception) { cancel_transaction(); return false; }
}

// =====================================================================
// TRANSFER ORDER LIFECYCLE TRANSITIONS
// =====================================================================

/**
 * Get the total estimated value of a transfer order.
 *
 * @param int $transfer_id Transfer order ID
 * @return float
 */
function get_transfer_order_total_value($transfer_id)
{
	$sql = "SELECT COALESCE(SUM(qty_requested * unit_cost), 0) AS total_value"
		. " FROM " . TB_PREF . "wh_transfer_order_lines"
		. " WHERE transfer_id = " . (int)$transfer_id;

	$result = db_query($sql, 'could not get transfer order total value');
	$row = db_fetch($result);

	return $row ? (float)$row['total_value'] : 0;
}

/**
 * Build transfer order draft payload for the core approval engine.
 *
 * @param int $transfer_id Transfer order ID
 * @return array|false
 */
function get_transfer_order_submission_data($transfer_id)
{
	$order = get_transfer_order($transfer_id);
	if (!$order)
		return false;

	$lines = get_transfer_order_lines($transfer_id);
	$line_data = array();
	while ($line = db_fetch($lines)) {
		$line_data[] = array(
			'line_id' => (int)$line['line_id'],
			'memo' => $line['memo'] === null ? '' : $line['memo'],
			'stock_id' => $line['stock_id'],
			'description' => isset($line['item_description']) ? $line['item_description'] : '',
			'units' => isset($line['item_units']) ? $line['item_units'] : '',
			'qty_requested' => (float)$line['qty_requested'],
			'qty_shipped' => (float)$line['qty_shipped'],
			'qty_received' => (float)$line['qty_received'],
			'unit_cost' => (float)$line['unit_cost'],
			'line_total' => (float)$line['qty_requested'] * (float)$line['unit_cost'],
			'from_bin_id' => $line['from_bin_id'] ? (int)$line['from_bin_id'] : 0,
			'to_bin_id' => $line['to_bin_id'] ? (int)$line['to_bin_id'] : 0,
			'batch_id' => $line['batch_id'] ? (int)$line['batch_id'] : 0,
			'serial_id' => $line['serial_id'] ? (int)$line['serial_id'] : 0,
			'batch_no' => isset($line['batch_no']) ? $line['batch_no'] : '',
			'serial_no' => isset($line['serial_no']) ? $line['serial_no'] : '',
		);
	}

	$statuses = get_transfer_order_statuses();

	return array(
		'transfer_id' => (int)$order['transfer_id'],
		'transfer_no' => (int)$order['transfer_no'],
		'transfer_type' => $order['transfer_type'],
		'status' => $order['status'],
		'status_label' => isset($statuses[$order['status']]) ? $statuses[$order['status']] : $order['status'],
		'from_loc_code' => $order['from_loc_code'],
		'from_loc_name' => isset($order['from_loc_name']) ? $order['from_loc_name'] : '',
		'to_loc_code' => $order['to_loc_code'],
		'to_loc_name' => isset($order['to_loc_name']) ? $order['to_loc_name'] : '',
		'request_date' => $order['request_date'],
		'expected_ship_date' => $order['expected_ship_date'] ? $order['expected_ship_date'] : '',
		'expected_recv_date' => $order['expected_recv_date'] ? $order['expected_recv_date'] : '',
		'reference' => $order['reference'] === null ? '' : $order['reference'],
		'memo' => $order['memo'] === null ? '' : $order['memo'],
		'requested_by' => (int)$order['requested_by'],
		'requested_by_name' => isset($order['requested_by_name']) ? $order['requested_by_name'] : '',
		'line_count' => count($line_data),
		'total_estimated' => (float)get_transfer_order_total_value($transfer_id),
		'lines' => $line_data,
	);
}

/**
 * Submit a transfer order for approval (draft -> submitted).
 *
 * @param int $transfer_id Transfer order ID
 * @return bool
 */
function submit_transfer_order($transfer_id)
{
    return submit_transfer_order_for_core_approval($transfer_id) !== false;
}

/**
 * Submit one transfer order into the core approval workflow.
 *
 * @param int $transfer_id Transfer order ID
 * @return array|bool
 */
function submit_transfer_order_for_core_approval($transfer_id)
{
    require_once __DIR__.'/warehouse_transfer_core_db.inc';
    return submit_native_transfer_to_core($transfer_id);
}

/**
 * Approve a transfer order.
 *
 * @param int $transfer_id Transfer order ID
 * @return bool Success
 */
function approve_transfer_order($transfer_id)
{
    // Final decisions belong exclusively to the registered shared-core handler.
    return false;
}

/**
 * Reject a submitted transfer order back to draft.
 *
 * @param int $transfer_id Transfer order ID
 * @return bool
 */
function reject_transfer_order($transfer_id)
{
    // Final decisions belong exclusively to the registered shared-core handler.
    return false;
}

/**
 * Ship a transfer order: creates stock moves (out from source),
 * updates serial/batch statuses, marks as shipped/in_transit.
 *
 * @param int    $transfer_id Transfer order ID
 * @param string $ship_date   Ship date (display format)
 * @return bool Success
 */
function ship_transfer_order($transfer_id, $ship_date) {
	global $path_to_root;
    require_once __DIR__.'/warehouse_transfer_fulfillment_db.inc';

    $context = transfer_core_context();
    $identity = dimension_writer_reference_id($transfer_id);
    if (!$context || !$identity || !is_string($ship_date) || !is_date($ship_date)) return false;
    try {
    if (!begin_transaction()) return false;
    // This is a stock-posting command, not an approval-status reader. The
    // independent core receipt and complete source are locked in the same
    // transaction as the move. Linked demand additionally requires its exact
    // valued origin; only actual destination receipts may satisfy that demand.
    require_once __DIR__.'/warehouse_transfer_terminal_db.inc';
    $custody = transfer_fulfillment_dispatch_custody($identity);
    if (!$custody || !transfer_core_dependencies($custody['source'], 'dispatch')) {
        cancel_transaction(); return false;
    }
    $order = get_transfer_order($identity);
    if (!$order || $order['status'] !== 'approved') { cancel_transaction(); return false; }
    $sql_date = date2sql($ship_date);
    if (!is_string($sql_date) || !preg_match('/^[0-9]{4}-[0-9]{2}-[0-9]{2}$/D', $sql_date)) {
        cancel_transaction(); return false;
    }
    if ($sql_date < $order['request_date'] || !transfer_fulfillment_period_allowed($sql_date)) {
        cancel_transaction(); return false;
    }
    if (isset($custody['material']) && !transfer_fulfillment_material_stock_allowed($custody['source'], $sql_date)) {
        cancel_transaction(); return false;
    }
    $movement_ids = array();

	$inv_root = dirname(dirname(dirname(dirname(dirname(__FILE__))))); // → project root
	include_once($inv_root . '/includes/db/inventory_db.inc');
	include_once($inv_root . '/inventory/includes/db/serial_batch_db.inc');
	include_once($inv_root . '/inventory/includes/db/serial_numbers_db.inc');
	include_once($inv_root . '/inventory/includes/db/stock_batches_db.inc');
	include_once($inv_root . '/inventory/warehouse/includes/warehouse_operations.inc');

	// Create a system transfer transaction for stock moves
	$transfer_trans_no = get_next_trans_no(ST_LOCTRANSFER);
    if (!dimension_writer_reference_id($transfer_trans_no)) { cancel_transaction(); return false; }

	$lines = get_transfer_order_lines($identity);
    if (!$lines) { cancel_transaction(); return false; }

	while ($line = db_fetch($lines)) {
		$qty = $line['qty_requested'];
		$stock_id = $line['stock_id'];

		// Create outbound stock move from source
		$extended = array();
		if ($line['serial_id'])
			$extended['serial_id'] = (int)$line['serial_id'];
		if ($line['batch_id'])
			$extended['batch_id'] = (int)$line['batch_id'];
		if ($line['from_bin_id'])
			$extended['from_bin_id'] = (int)$line['from_bin_id'];
		$extended['stock_status'] = 'in_transit';

		$movement_id = add_stock_move_extended(ST_LOCTRANSFER, $stock_id, $transfer_trans_no,
			$order['from_loc_code'], $ship_date, $order['reference'] ? $order['reference'] : 'TO-'.$order['transfer_no'],
			-$qty, $line['unit_cost'], 0, $extended);
        if (!dimension_writer_reference_id($movement_id)) { cancel_transaction(); return false; }
        $movement_ids[(int)$line['line_id']] = (int)$movement_id;

		// Update shipped qty on line
		$sql = "UPDATE " . TB_PREF . "wh_transfer_order_lines SET "
			. "qty_shipped=" . (float)$qty
			. " WHERE line_id=" . (int)$line['line_id'] . " AND transfer_id=" . $identity
            . " AND qty_shipped=0 AND qty_received=0";
		if (!db_query($sql, 'could not update line shipped qty') || db_num_affected_rows() !== 1) {
            cancel_transaction(); return false;
        }

		// Serial: mark as in_transit
		if ($line['serial_id']) {
			if (function_exists('update_serial_status')) {
				update_serial_status(
					(int)$line['serial_id'],
					'in_transit',
					ST_LOCTRANSFER,
					$transfer_trans_no,
					$order['from_loc_code'],
					$order['to_loc_code'],
					$ship_date,
					$order['reference'] ? $order['reference'] : '',
					sprintf(_('Shipped via Transfer Order #%d'), $order['transfer_no'])
				);

				$sql_clear_serial_bin = "UPDATE " . TB_PREF . "serial_numbers SET wh_loc_id=NULL WHERE id=" . (int)$line['serial_id'];
								traceability_dimension_update('serial_numbers', (int)$line['serial_id'], $sql_clear_serial_bin, 'could not clear shipped serial bin location');
			}
		}

		// Batch: log outbound movement
		if ($line['batch_id']) {
			if (function_exists('add_batch_movement')) {
				add_batch_movement(
					(int)$line['batch_id'],
					ST_LOCTRANSFER,
					$transfer_trans_no,
					$order['from_loc_code'],
					null,
					-$qty,
					$ship_date,
					$order['reference'] ? $order['reference'] : '',
					sprintf(_('Shipped via Transfer Order #%d'), $order['transfer_no'])
				);
			}
		}

		// Update bin stock (remove from source)
		if ($line['from_bin_id'] && function_exists('update_bin_stock')) {
			update_bin_stock(
				(int)$line['from_bin_id'],
				$stock_id,
				-$qty,
				$line['batch_id'] ? (int)$line['batch_id'] : null,
				$line['serial_id'] ? (int)$line['serial_id'] : null
			);
		}
	}

	// Update order status
	$sql = "UPDATE " . TB_PREF . "wh_transfer_orders SET "
		. "status='in_transit',"
		. "shipped_by=" . (int)$_SESSION['wa_current_user']->user . ","
		. "actual_ship_date=" . db_escape($sql_date) . ","
		. "updated_at=NOW()"
		. " WHERE transfer_id=" . $identity
        . " AND status='approved' AND shipped_by IS NULL AND actual_ship_date IS NULL";
	if (!db_query($sql, 'could not update transfer order status to shipped') || db_num_affected_rows() !== 1
        || transfer_core_context() !== $context) { cancel_transaction(); return false; }

	add_audit_trail(ST_LOCTRANSFER, $transfer_trans_no, $ship_date);

	if (!transfer_core_dependencies($custody['source'], 'receipt')
        || !transfer_fulfillment_period_allowed($sql_date)
        || !transfer_fulfillment_record($identity, $custody, 'dispatch', $sql_date, $movement_ids)
        || !transfer_fulfillment_period_allowed($sql_date)
        || (isset($custody['material']) && !transfer_fulfillment_material_stock_allowed($custody['source'], $sql_date, true))
        || !transfer_core_dependencies($custody['source'], 'receipt')
        || transfer_core_context() !== $context) { cancel_transaction(); return false; }
	if (!commit_transaction()) { cancel_transaction(); return false; }
	return true;
    } catch (Exception $exception) { cancel_transaction(); return false;
    } catch (Throwable $exception) { cancel_transaction(); return false; }
}

/**
 * Validate exactly one locked receipt request; rejects negatives, NaN,
 * unknown line IDs, and over-receipt before any stock mutation. A missing key
 * retains the form's ordinary "receive all outstanding" behavior.
 */
function transfer_fulfillment_receipt_plan($locked_lines, $received)
{
    if (!is_array($locked_lines) || !$locked_lines || !is_array($received)) return false;
    $plan = array(); $positive = false;
    foreach ($locked_lines as $line) {
        if (!is_array($line) || !isset($line['line_id'], $line['qty_requested'],
            $line['qty_shipped'], $line['qty_received'])) return false;
        $id = dimension_writer_reference_id($line['line_id']);
        if (!$id || isset($plan[$id])) return false;
        foreach (array('qty_requested','qty_shipped','qty_received') as $field)
            if (!is_numeric($line[$field]) || !is_finite((float)$line[$field])) return false;
        $shipped = (float)$line['qty_shipped'];
        $outstanding = round($shipped - (float)$line['qty_received'], 4);
        if ($shipped <= 0 || $shipped > (float)$line['qty_requested']
            || (float)$line['qty_received'] < 0 || $outstanding < 0) return false;
        $request = array_key_exists($id, $received) ? $received[$id] : $outstanding;
        if ((!is_int($request) && !is_float($request) && !is_string($request))
            || !is_numeric($request) || !is_finite((float)$request)) return false;
        $qty = (float)$request;
        if ($qty < 0 || $qty > $outstanding || round($qty, 4) !== $qty
            || (!empty($line['serial_id']) && $qty != 0 && $qty != 1)) return false;
        if ($qty > 0) $positive = true;
        $plan[$id] = $qty;
    }
    foreach ($received as $key=>$qty)
        if (!dimension_writer_reference_id($key) || !array_key_exists((int)$key, $plan)) return false;
    return $positive ? $plan : false;
}

/**
 * Receive a transfer order (full or partial): creates stock moves into destination,
 * updates serial/batch statuses, and retains outstanding shipped items on this order.
 *
 * @param int    $transfer_id Transfer order ID
 * @param string $recv_date   Receive date (display format)
 * @param array  $received    Array of line_id => qty_received
 * @param string|null $expected_source Optional exact rendered source fingerprint for stale-form denial
 * @return array Result: ['success'=>bool, 'backorder_id'=>int|null, 'message'=>string]
 */
function receive_transfer_order($transfer_id, $recv_date, $received = array(), $expected_source = null) {
	global $path_to_root;
    require_once __DIR__.'/warehouse_transfer_fulfillment_db.inc';

	$result = array('success' => false, 'backorder_id' => null, 'message' => '');

    $context = transfer_core_context();
    $identity = dimension_writer_reference_id($transfer_id);
    if (!$context || !$identity || !is_string($recv_date) || !is_date($recv_date) || !is_array($received)) return $result;
    try {
    if (!begin_transaction()) return $result;
    $custody = transfer_fulfillment_custody($identity);
    if (!$custody || ($expected_source !== null && (!is_string($expected_source)
        || !hash_equals(transfer_core_source_fingerprint($custody['source']), $expected_source)))) {
        cancel_transaction();
        $result['message'] = _('Transfer receipt evidence or quantities have changed. Reload this order before receiving items.');
        return $result;
    }
    if (!transfer_fulfillment_reference_locks($custody['source'], isset($custody['material']) ? $custody['material'] : null)) {
        cancel_transaction(); return $result;
    }
    // Serialize duplicate receipts and cancellation against the real current
    // header, not an earlier page read. All quantities are validated under the
    // same InnoDB locks before the first inbound stock move is written.
    $header = transfer_core_rows('SELECT * FROM '.TB_PREF.'wh_transfer_orders WHERE transfer_id='.$identity.' FOR UPDATE');
    $locked_lines = transfer_core_rows('SELECT * FROM '.TB_PREF.'wh_transfer_order_lines WHERE transfer_id='.$identity.' ORDER BY line_id FOR UPDATE');
    if (!$header || count($header) !== 1 || !in_array($header[0]['status'], array('in_transit','shipped'), true)
        || empty($header[0]['shipped_by']) || $header[0]['actual_ship_date'] === null
        || !empty($header[0]['received_by']) || $header[0]['actual_recv_date'] !== null
        || !$locked_lines) { cancel_transaction(); return $result; }
    $order = get_transfer_order($identity);
    if (!$order || $order['status'] !== $header[0]['status']) { cancel_transaction(); return $result; }
    $planned = transfer_fulfillment_receipt_plan($locked_lines, $received);
    if ($planned === false) {
        cancel_transaction();
        $result['message'] = _('Invalid receipt quantities or no items selected for receipt.');
        return $result;
    }
    // Only this event's selected lines acquire new receipt authority. A serial
    // received earlier may legitimately have moved again since a partial receipt.
    $receipt_source = $custody['source']; $receipt_source['lines'] = array();
    foreach ($locked_lines as $line)
        if ($planned[(int)$line['line_id']] > 0) $receipt_source['lines'][] = $line;
    if (!transfer_core_dependencies($receipt_source, 'receipt')) {
        cancel_transaction();
        $result['message'] = _('Transfer receipt item, location or tracking state is unavailable.');
        return $result;
    }

	$inv_root = dirname(dirname(dirname(dirname(dirname(__FILE__))))); // → project root
	include_once($inv_root . '/includes/db/inventory_db.inc');
	include_once($inv_root . '/inventory/includes/db/serial_batch_db.inc');
	include_once($inv_root . '/inventory/includes/db/serial_numbers_db.inc');
	include_once($inv_root . '/inventory/includes/db/stock_batches_db.inc');
	include_once($inv_root . '/inventory/warehouse/includes/warehouse_operations.inc');

	$transfer_trans_no = get_next_trans_no(ST_LOCTRANSFER);
	$sql_date = date2sql($recv_date);
    if (!dimension_writer_reference_id($transfer_trans_no) || !is_string($sql_date)
        || !preg_match('/^[0-9]{4}-[0-9]{2}-[0-9]{2}$/D', $sql_date)) { cancel_transaction(); return $result; }
    if ($sql_date < $order['actual_ship_date'] || !transfer_fulfillment_period_allowed($sql_date)) {
        cancel_transaction();
        $result['message'] = _('The receipt date must be in an authorized open accounting period.');
        return $result;
    }
	$movement_ids = array();
	$has_shortage = false;

	$lines = get_transfer_order_lines($identity);
    if (!$lines) { cancel_transaction(); return $result; }
	while ($line = db_fetch($lines)) {
		$line_id = (int)$line['line_id'];
		$qty_shipped = (float)$line['qty_shipped'];
		$stock_id = $line['stock_id'];

		// The locked plan retains zero receipts as outstanding shipped quantities.
        // Never clamp an over-receipt or silently skip an invalid quantity.
        if (!array_key_exists($line_id, $planned)) { cancel_transaction(); return $result; }
        $qty_recv = $planned[$line_id];
        if ($qty_recv <= 0) {
            if ($qty_shipped > (float)$line['qty_received']) $has_shortage = true;
            continue;
        }

		// Create inbound stock move to destination
		$extended = array();
		if ($line['serial_id'])
			$extended['serial_id'] = (int)$line['serial_id'];
		if ($line['batch_id'])
			$extended['batch_id'] = (int)$line['batch_id'];
		if ($line['to_bin_id'])
			$extended['to_bin_id'] = (int)$line['to_bin_id'];
		$extended['stock_status'] = 'available';

		$movement_id = add_stock_move_extended(ST_LOCTRANSFER, $stock_id, $transfer_trans_no,
			$order['to_loc_code'], $recv_date,
			$order['reference'] ? $order['reference'] : 'TO-'.$order['transfer_no'],
			$qty_recv, $line['unit_cost'], 0, $extended);
        if (!dimension_writer_reference_id($movement_id)) { cancel_transaction(); return $result; }
        $movement_ids[$line_id] = (int)$movement_id;

		// Update received qty on line
		$new_recv = round((float)$line['qty_received'] + $qty_recv, 4);
		$sql = "UPDATE " . TB_PREF . "wh_transfer_order_lines SET "
			. "qty_received=" . $new_recv
			. " WHERE line_id=" . $line_id . " AND transfer_id=" . $identity
            . " AND qty_received=" . db_escape($line['qty_received']);
		if (!db_query($sql, 'could not update line received qty') || db_num_affected_rows() !== 1) {
            cancel_transaction(); return $result;
        }

		// Serial: mark as available at destination
		if ($line['serial_id']) {
			if (function_exists('update_serial_status')) {
				update_serial_status(
					(int)$line['serial_id'],
					'available',
					ST_LOCTRANSFER,
					$transfer_trans_no,
					$order['from_loc_code'],
					$order['to_loc_code'],
					$recv_date,
					$order['reference'] ? $order['reference'] : '',
					sprintf(_('Received via Transfer Order #%d'), $order['transfer_no'])
				);
				// Update serial location
				$sql_update = "UPDATE " . TB_PREF . "serial_numbers SET "
					. "loc_code=" . db_escape($order['to_loc_code'])
					. ($line['to_bin_id'] ? ", wh_loc_id=" . (int)$line['to_bin_id'] : "")
					. " WHERE id=" . (int)$line['serial_id'];
								traceability_dimension_update('serial_numbers', (int)$line['serial_id'], $sql_update, 'could not update serial location');
			}
		}

		// Batch: log inbound movement
		if ($line['batch_id']) {
			if (function_exists('add_batch_movement')) {
				add_batch_movement(
					(int)$line['batch_id'],
					ST_LOCTRANSFER,
					$transfer_trans_no,
					$order['to_loc_code'],
					$line['to_bin_id'] ? (int)$line['to_bin_id'] : null,
					$qty_recv,
					$recv_date,
					$order['reference'] ? $order['reference'] : '',
					sprintf(_('Received via Transfer Order #%d'), $order['transfer_no'])
				);
			}
		}

		// Update bin stock (add to destination)
		if ($line['to_bin_id'] && function_exists('update_bin_stock')) {
			$batch_data = null;
			$expiry = null;
			if ($line['batch_id'] && function_exists('get_stock_batch')) {
				$batch_data = get_stock_batch($line['batch_id']);
				$expiry = $batch_data ? $batch_data['expiry_date'] : null;
			}
			if ($line['serial_id'] && function_exists('reconcile_serial_bin_stock_location')) {
				$serial_error = '';
				$serial_reconcile = reconcile_serial_bin_stock_location(
					$stock_id,
					(int)$line['serial_id'],
					(int)$line['to_bin_id'],
					true,
					isset($line['serial_no']) ? $line['serial_no'] : '',
					$serial_error
				);
				if (!$serial_reconcile['success']) {
					cancel_transaction();
					$result['message'] = $serial_error !== '' ? $serial_error : _('Transfer receipt serial reconciliation failed.');
					return $result;
				}
			}
			update_bin_stock(
				(int)$line['to_bin_id'],
				$stock_id,
				$qty_recv,
				$line['batch_id'] ? (int)$line['batch_id'] : null,
				$line['serial_id'] ? (int)$line['serial_id'] : null,
				$expiry
			);
		}

		// Check for shortage (partial receipt)
		$shortage = round($qty_shipped - $new_recv, 4);
        if ($shortage > 0) $has_shortage = true;
	}

    if (!$has_shortage) {
	// Mark order as received
	$sql = "UPDATE " . TB_PREF . "wh_transfer_orders SET "
		. "status='received',"
		. "received_by=" . (int)$_SESSION['wa_current_user']->user . ","
		. "actual_recv_date=" . db_escape($sql_date) . ","
		. "updated_at=NOW()"
		. " WHERE transfer_id=" . $identity
        . " AND status=" . db_escape($header[0]['status']) . " AND received_by IS NULL AND actual_recv_date IS NULL";
	if (!db_query($sql, 'could not update transfer order status to received') || db_num_affected_rows() !== 1
        || transfer_core_context() !== $context) { cancel_transaction(); return $result; }

    }
    if (transfer_core_context() !== $context) { cancel_transaction(); return $result; }

	add_audit_trail(ST_LOCTRANSFER, $transfer_trans_no, $recv_date);

    // A partial receipt leaves the already shipped remainder on this transfer.
    // Creating a dispatchable backorder here would issue the same goods twice.
    $backorder_id = null;

	if (!transfer_core_dependencies($receipt_source, 'received')
        || !transfer_fulfillment_period_allowed($sql_date)
        || !transfer_fulfillment_record($identity, $custody, 'receipt', $sql_date, $movement_ids)
        || !transfer_fulfillment_period_allowed($sql_date)
        || !transfer_core_dependencies($receipt_source, 'received')
        || transfer_core_context() !== $context) { cancel_transaction(); return $result; }
	if (!commit_transaction()) { cancel_transaction(); return $result; }

	$result['success'] = true;
	$result['backorder_id'] = $backorder_id;
	if ($has_shortage) {
		$result['message'] = _('Transfer order partially received. Remaining shipped items await receipt on this order.');
	} else {
		$result['message'] = _('Transfer order fully received.');
	}

	return $result;
    } catch (Exception $exception) { cancel_transaction(); return $result;
    } catch (Throwable $exception) { cancel_transaction(); return $result; }
}

/**
 * Cancel a transfer order.
 *
 * @param int $transfer_id Transfer order ID
 * @return bool Success
 */
function cancel_transfer_order($transfer_id, $comments = '') {
    require_once __DIR__.'/warehouse_transfer_terminal_db.inc';
    $identity = dimension_writer_reference_id($transfer_id);
    if (!$identity || !transfer_core_context()) return false;
    // Read-only hint: every writer re-reads and locks the exact state and history.
    $order = get_transfer_order($identity);
    if (!$order) return false;
    if ($order['status'] === 'submitted') return cancel_native_transfer_core($identity, $comments);
    if ($order['status'] === 'draft') return cancel_native_transfer_draft($identity);
    if ($order['status'] === 'approved') return cancel_native_transfer_approved($identity, $comments);
    // Shipped/in-transit stock, serial, batch and GL inverse custody is not
    // established. Deny rather than author an unbalanced or double reversal.
    return false;
}

/**
 * Get transfer order summary counts by status.
 *
 * @return array status => count
 */
function get_transfer_order_summary() {
	$sql = "SELECT status, COUNT(*) AS cnt
		FROM " . TB_PREF . "wh_transfer_orders
		GROUP BY status";
	$result = db_query($sql, 'could not get transfer order summary');

	$summary = array();
	while ($row = db_fetch($result)) {
		$summary[$row['status']] = (int)$row['cnt'];
	}
	return $summary;
}

/**
 * Get status badge HTML for a transfer order status.
 *
 * @param string $status Status code
 * @return string HTML span badge
 */
function transfer_order_status_badge($status) {
	$statuses = get_transfer_order_statuses();
	$label = isset($statuses[$status]) ? $statuses[$status] : $status;
	$color = get_transfer_order_status_color($status);
	return '<span style="display:inline-block;padding:2px 8px;border-radius:3px;'
		. 'background:' . $color . ';color:#fff;font-size:11px;font-weight:bold;">'
		. $label . '</span>';
}

/**
 * Get type badge HTML for a transfer order type.
 *
 * @param string $type Transfer type
 * @return string HTML span badge
 */
function transfer_order_type_badge($type) {
	$types = array(
		'standard'  => _('Standard'),
		'urgent'    => _('Urgent'),
		'backorder' => _('Backorder'),
	);
	$label = isset($types[$type]) ? $types[$type] : $type;
	$color = get_transfer_order_type_color($type);
	return '<span style="display:inline-block;padding:2px 8px;border-radius:3px;'
		. 'background:' . $color . ';color:#fff;font-size:11px;">'
		. $label . '</span>';
}
