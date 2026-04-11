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
 * Warehouse Transfer Orders — CRUD and lifecycle management.
 *
 * Transfer order lifecycle: Draft → Approved → Shipped → In Transit → Received → (Cancelled)
 * Supports partial receipt with automatic backorder creation.
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
	$transfer_no = get_next_transfer_order_no();

	$sql = "INSERT INTO " . TB_PREF . "wh_transfer_orders (
		transfer_no, transfer_type, status, from_loc_code, to_loc_code,
		request_date, expected_ship_date, expected_recv_date,
		requested_by, reference, memo, created_at
	) VALUES ("
		. (int)$transfer_no . ","
		. db_escape($transfer_type) . ","
		. "'draft',"
		. db_escape($from_loc_code) . ","
		. db_escape($to_loc_code) . ","
		. db_escape($request_date) . ","
		. ($expected_ship ? db_escape($expected_ship) : 'NULL') . ","
		. ($expected_recv ? db_escape($expected_recv) : 'NULL') . ","
		. (int)$_SESSION['wa_current_user']->user . ","
		. ($reference ? db_escape($reference) : 'NULL') . ","
		. ($memo ? db_escape($memo) : 'NULL') . ","
		. "NOW()"
		. ")";

	db_query($sql, 'could not add transfer order');
	return db_insert_id();
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
	$from_bin_id = null, $to_bin_id = null,
	$batch_id = null, $serial_id = null,
	$unit_cost = 0, $memo = null)
{
	$sql = "INSERT INTO " . TB_PREF . "wh_transfer_order_lines (
		transfer_id, stock_id, qty_requested, from_bin_id, to_bin_id,
		batch_id, serial_id, unit_cost, memo
	) VALUES ("
		. (int)$transfer_id . ","
		. db_escape($stock_id) . ","
		. (float)$qty . ","
		. ($from_bin_id ? (int)$from_bin_id : 'NULL') . ","
		. ($to_bin_id ? (int)$to_bin_id : 'NULL') . ","
		. ($batch_id ? (int)$batch_id : 'NULL') . ","
		. ($serial_id ? (int)$serial_id : 'NULL') . ","
		. (float)$unit_cost . ","
		. ($memo ? db_escape($memo) : 'NULL')
		. ")";

	db_query($sql, 'could not add transfer order line');
	return db_insert_id();
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
	$sql = "UPDATE " . TB_PREF . "wh_transfer_orders SET "
		. "from_loc_code=" . db_escape($from_loc_code) . ","
		. "to_loc_code=" . db_escape($to_loc_code) . ","
		. "request_date=" . db_escape($request_date) . ","
		. "transfer_type=" . db_escape($transfer_type) . ","
		. "reference=" . ($reference ? db_escape($reference) : 'NULL') . ","
		. "memo=" . ($memo ? db_escape($memo) : 'NULL') . ","
		. "expected_ship_date=" . ($expected_ship ? db_escape($expected_ship) : 'NULL') . ","
		. "expected_recv_date=" . ($expected_recv ? db_escape($expected_recv) : 'NULL') . ","
		. "updated_at=NOW()"
		. " WHERE transfer_id=" . (int)$transfer_id;

	db_query($sql, 'could not update transfer order');
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
	$to_bin_id = null, $batch_id = null, $serial_id = null,
	$unit_cost = 0, $memo = null)
{
	$sql = "UPDATE " . TB_PREF . "wh_transfer_order_lines SET "
		. "qty_requested=" . (float)$qty . ","
		. "from_bin_id=" . ($from_bin_id ? (int)$from_bin_id : 'NULL') . ","
		. "to_bin_id=" . ($to_bin_id ? (int)$to_bin_id : 'NULL') . ","
		. "batch_id=" . ($batch_id ? (int)$batch_id : 'NULL') . ","
		. "serial_id=" . ($serial_id ? (int)$serial_id : 'NULL') . ","
		. "unit_cost=" . (float)$unit_cost . ","
		. "memo=" . ($memo ? db_escape($memo) : 'NULL')
		. " WHERE line_id=" . (int)$line_id;

	db_query($sql, 'could not update transfer order line');
}

/**
 * Delete a transfer order line.
 *
 * @param int $line_id Line ID
 */
function delete_transfer_order_line($line_id) {
	$sql = "DELETE FROM " . TB_PREF . "wh_transfer_order_lines WHERE line_id=" . (int)$line_id;
	db_query($sql, 'could not delete transfer order line');
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
	if (!can_delete_transfer_order($transfer_id))
		return false;

	begin_transaction();
	$sql = "DELETE FROM " . TB_PREF . "wh_transfer_order_lines WHERE transfer_id=" . (int)$transfer_id;
	db_query($sql, 'could not delete transfer order lines');

	$sql = "DELETE FROM " . TB_PREF . "wh_transfer_orders WHERE transfer_id=" . (int)$transfer_id;
	db_query($sql, 'could not delete transfer order');
	commit_transaction();

	return true;
}

// =====================================================================
// TRANSFER ORDER LIFECYCLE TRANSITIONS
// =====================================================================

/**
 * Approve a transfer order.
 *
 * @param int $transfer_id Transfer order ID
 * @return bool Success
 */
function approve_transfer_order($transfer_id) {
	$order = get_transfer_order($transfer_id);
	if (!$order || $order['status'] !== 'draft')
		return false;

	$sql = "UPDATE " . TB_PREF . "wh_transfer_orders SET "
		. "status='approved',"
		. "approved_by=" . (int)$_SESSION['wa_current_user']->user . ","
		. "approval_date=NOW(),"
		. "updated_at=NOW()"
		. " WHERE transfer_id=" . (int)$transfer_id;

	db_query($sql, 'could not approve transfer order');
	return true;
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

	$order = get_transfer_order($transfer_id);
	if (!$order || !in_array($order['status'], array('approved')))
		return false;

	$inv_root = dirname(dirname(dirname(dirname(dirname(__FILE__))))); // → project root
	include_once($inv_root . '/includes/db/inventory_db.inc');
	include_once($inv_root . '/inventory/includes/db/serial_batch_db.inc');
	include_once($inv_root . '/inventory/includes/db/serial_numbers_db.inc');
	include_once($inv_root . '/inventory/includes/db/stock_batches_db.inc');

	begin_transaction();

	// Create a system transfer transaction for stock moves
	$transfer_trans_no = get_next_trans_no(ST_LOCTRANSFER);

	$lines = get_transfer_order_lines($transfer_id);
	$sql_date = date2sql($ship_date);

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

		add_stock_move_extended(ST_LOCTRANSFER, $stock_id, $transfer_trans_no,
			$order['from_loc_code'], $ship_date, $order['reference'] ? $order['reference'] : 'TO-'.$order['transfer_no'],
			-$qty, $line['unit_cost'], 0, $extended);

		// Update shipped qty on line
		$sql = "UPDATE " . TB_PREF . "wh_transfer_order_lines SET "
			. "qty_shipped=" . (float)$qty
			. " WHERE line_id=" . (int)$line['line_id'];
		db_query($sql, 'could not update line shipped qty');

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
		. " WHERE transfer_id=" . (int)$transfer_id;
	db_query($sql, 'could not update transfer order status to shipped');

	add_audit_trail(ST_LOCTRANSFER, $transfer_trans_no, $ship_date);

	commit_transaction();
	return true;
}

/**
 * Receive a transfer order (full or partial): creates stock moves into destination,
 * updates serial/batch statuses, and optionally creates a backorder for remainder.
 *
 * @param int    $transfer_id Transfer order ID
 * @param string $recv_date   Receive date (display format)
 * @param array  $received    Array of line_id => qty_received
 * @return array Result: ['success'=>bool, 'backorder_id'=>int|null, 'message'=>string]
 */
function receive_transfer_order($transfer_id, $recv_date, $received = array()) {
	global $path_to_root;

	$result = array('success' => false, 'backorder_id' => null, 'message' => '');

	$order = get_transfer_order($transfer_id);
	if (!$order || !in_array($order['status'], array('in_transit', 'shipped'))) {
		$result['message'] = _('Transfer order is not in a receivable state.');
		return $result;
	}

	$inv_root = dirname(dirname(dirname(dirname(dirname(__FILE__))))); // → project root
	include_once($inv_root . '/includes/db/inventory_db.inc');
	include_once($inv_root . '/inventory/includes/db/serial_batch_db.inc');
	include_once($inv_root . '/inventory/includes/db/serial_numbers_db.inc');
	include_once($inv_root . '/inventory/includes/db/stock_batches_db.inc');

	begin_transaction();

	$transfer_trans_no = get_next_trans_no(ST_LOCTRANSFER);
	$sql_date = date2sql($recv_date);
	$has_shortage = false;
	$backorder_lines = array();

	$lines = get_transfer_order_lines($transfer_id);
	while ($line = db_fetch($lines)) {
		$line_id = (int)$line['line_id'];
		$qty_shipped = (float)$line['qty_shipped'];
		$stock_id = $line['stock_id'];

		// Determine qty to receive
		$qty_recv = isset($received[$line_id]) ? (float)$received[$line_id] : $qty_shipped;
		if ($qty_recv > $qty_shipped)
			$qty_recv = $qty_shipped;
		if ($qty_recv <= 0)
			continue;

		// Create inbound stock move to destination
		$extended = array();
		if ($line['serial_id'])
			$extended['serial_id'] = (int)$line['serial_id'];
		if ($line['batch_id'])
			$extended['batch_id'] = (int)$line['batch_id'];
		if ($line['to_bin_id'])
			$extended['to_bin_id'] = (int)$line['to_bin_id'];
		$extended['stock_status'] = 'available';

		add_stock_move_extended(ST_LOCTRANSFER, $stock_id, $transfer_trans_no,
			$order['to_loc_code'], $recv_date,
			$order['reference'] ? $order['reference'] : 'TO-'.$order['transfer_no'],
			$qty_recv, $line['unit_cost'], 0, $extended);

		// Update received qty on line
		$new_recv = (float)$line['qty_received'] + $qty_recv;
		$sql = "UPDATE " . TB_PREF . "wh_transfer_order_lines SET "
			. "qty_received=" . $new_recv
			. " WHERE line_id=" . $line_id;
		db_query($sql, 'could not update line received qty');

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
				db_query($sql_update, 'could not update serial location');
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
		$shortage = $qty_shipped - $qty_recv;
		if ($shortage > 0.0001) {
			$has_shortage = true;
			$backorder_lines[] = array(
				'stock_id'    => $stock_id,
				'qty'         => $shortage,
				'from_bin_id' => $line['from_bin_id'],
				'to_bin_id'   => $line['to_bin_id'],
				'batch_id'    => $line['batch_id'],
				'serial_id'   => null, // Don't carry serial to backorder
				'unit_cost'   => $line['unit_cost'],
			);
		}
	}

	// Mark order as received
	$sql = "UPDATE " . TB_PREF . "wh_transfer_orders SET "
		. "status='received',"
		. "received_by=" . (int)$_SESSION['wa_current_user']->user . ","
		. "actual_recv_date=" . db_escape($sql_date) . ","
		. "updated_at=NOW()"
		. " WHERE transfer_id=" . (int)$transfer_id;
	db_query($sql, 'could not update transfer order status to received');

	add_audit_trail(ST_LOCTRANSFER, $transfer_trans_no, $recv_date);

	// Create backorder for shortages
	$backorder_id = null;
	if ($has_shortage && !empty($backorder_lines)) {
		$backorder_id = add_transfer_order(
			$order['from_loc_code'],
			$order['to_loc_code'],
			$sql_date,
			'backorder',
			'BO-TO-' . $order['transfer_no'],
			sprintf(_('Backorder from Transfer Order #%d (partial receipt)'), $order['transfer_no'])
		);
		foreach ($backorder_lines as $bl) {
			add_transfer_order_line(
				$backorder_id,
				$bl['stock_id'],
				$bl['qty'],
				$bl['from_bin_id'],
				$bl['to_bin_id'],
				$bl['batch_id'],
				$bl['serial_id'],
				$bl['unit_cost']
			);
		}
	}

	commit_transaction();

	$result['success'] = true;
	$result['backorder_id'] = $backorder_id;
	if ($backorder_id) {
		$result['message'] = sprintf(_('Transfer order received (partial). Backorder #%d created for remaining items.'), $backorder_id);
	} else {
		$result['message'] = _('Transfer order fully received.');
	}

	return $result;
}

/**
 * Cancel a transfer order.
 *
 * @param int $transfer_id Transfer order ID
 * @return bool Success
 */
function cancel_transfer_order($transfer_id) {
	$order = get_transfer_order($transfer_id);
	if (!$order || in_array($order['status'], array('received', 'cancelled')))
		return false;

	// If already shipped/in_transit, we need to reverse stock moves
	if (in_array($order['status'], array('shipped', 'in_transit'))) {
		$inv_root = dirname(dirname(dirname(dirname(dirname(__FILE__)))));
		include_once($inv_root . '/includes/db/inventory_db.inc');
		include_once($inv_root . '/inventory/includes/db/serial_numbers_db.inc');
		include_once($inv_root . '/inventory/includes/db/stock_batches_db.inc');

		begin_transaction();

		$lines = get_transfer_order_lines($transfer_id);
		while ($line = db_fetch($lines)) {
			// Reverse serial status
			if ($line['serial_id'] && function_exists('update_serial_status')) {
				update_serial_status(
					(int)$line['serial_id'],
					'available',
					ST_LOCTRANSFER,
					0,
					$order['to_loc_code'],
					$order['from_loc_code'],
					Today(),
					'',
					sprintf(_('Transfer Order #%d cancelled'), $order['transfer_no'])
				);
				// Restore serial to source location
				$sql = "UPDATE " . TB_PREF . "serial_numbers SET "
					. "loc_code=" . db_escape($order['from_loc_code'])
					. " WHERE id=" . (int)$line['serial_id'];
				db_query($sql, 'could not restore serial location');
			}

			// Reverse batch movements
			if ($line['batch_id'] && function_exists('add_batch_movement')) {
				add_batch_movement(
					(int)$line['batch_id'],
					ST_LOCTRANSFER,
					0,
					$order['from_loc_code'],
					null,
					$line['qty_shipped'],
					Today(),
					'',
					sprintf(_('Transfer Order #%d cancelled — stock returned'), $order['transfer_no'])
				);
			}

			// Restore bin stock at source
			if ($line['from_bin_id'] && function_exists('update_bin_stock')) {
				update_bin_stock(
					(int)$line['from_bin_id'],
					$line['stock_id'],
					$line['qty_shipped'],
					$line['batch_id'] ? (int)$line['batch_id'] : null,
					$line['serial_id'] ? (int)$line['serial_id'] : null
				);
			}
		}

		$sql = "UPDATE " . TB_PREF . "wh_transfer_orders SET "
			. "status='cancelled', updated_at=NOW()"
			. " WHERE transfer_id=" . (int)$transfer_id;
		db_query($sql, 'could not cancel transfer order');

		commit_transaction();
	} else {
		$sql = "UPDATE " . TB_PREF . "wh_transfer_orders SET "
			. "status='cancelled', updated_at=NOW()"
			. " WHERE transfer_id=" . (int)$transfer_id;
		db_query($sql, 'could not cancel transfer order');
	}

	return true;
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
