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
 * Warehouse Returns — CRUD, lifecycle management, inspection, disposition routing.
 *
 * Return order lifecycle: Draft → Received → Inspected → Completed (or Cancelled)
 *
 * Supports both Customer Returns and Supplier Returns with disposition codes:
 *   restock  — Return to available inventory
 *   refurbish — Send to refurbishment area
 *   scrap    — Write off as scrap (creates scrap entry)
 *   return_to_vendor — Ship back to supplier
 *
 * Tables: 0_wh_return_orders, 0_wh_return_order_lines, 0_stock_moves, 0_gl_trans
 *
 * @package NotrinosERP
 * @subpackage Inventory/Warehouse
 */

/**
 * Get valid return order types.
 *
 * @return array type_code => label
 */
function get_return_order_types() {
	return array(
		'customer'  => _('Customer Return'),
		'supplier'  => _('Supplier Return'),
	);
}

/**
 * Get valid return order statuses.
 *
 * @return array status_code => label
 */
function get_return_order_statuses() {
	return array(
		'draft'     => _('Draft'),
		'received'  => _('Received'),
		'inspected' => _('Inspected'),
		'completed' => _('Completed'),
		'cancelled' => _('Cancelled'),
	);
}

/**
 * Get CSS color for a return order status.
 *
 * @param string $status Status code
 * @return string Hex color
 */
function get_return_status_color($status) {
	$colors = array(
		'draft'     => '#6c757d',
		'received'  => '#007bff',
		'inspected' => '#ffc107',
		'completed' => '#28a745',
		'cancelled' => '#dc3545',
	);
	return isset($colors[$status]) ? $colors[$status] : '#6c757d';
}

/**
 * Get valid disposition codes.
 *
 * @return array code => label
 */
function get_return_disposition_codes() {
	return array(
		'restock'          => _('Restock'),
		'refurbish'        => _('Refurbish'),
		'scrap'            => _('Scrap'),
		'return_to_vendor' => _('Return to Vendor'),
	);
}

/**
 * Get CSS color for a disposition code.
 *
 * @param string $code Disposition code
 * @return string Hex color
 */
function get_disposition_color($code) {
	$colors = array(
		'restock'          => '#28a745',
		'refurbish'        => '#17a2b8',
		'scrap'            => '#dc3545',
		'return_to_vendor' => '#fd7e14',
	);
	return isset($colors[$code]) ? $colors[$code] : '#6c757d';
}

/**
 * Get the next sequential return order number.
 *
 * @return int Next return_no
 */
function get_next_return_order_no() {
	$sql = "SELECT COALESCE(MAX(return_no), 0) + 1 AS next_no FROM " . TB_PREF . "wh_return_orders";
	$result = db_query($sql, 'Could not get next return order number');
	$row = db_fetch($result);
	return (int)$row['next_no'];
}

/**
 * Create a new return order header.
 *
 * @param string      $return_type       'customer' or 'supplier'
 * @param string      $warehouse_loc_code Warehouse location code (0_locations.loc_code)
 * @param string      $return_date       Return date (display format)
 * @param int|null    $customer_id       Customer ID (for customer returns)
 * @param int|null    $supplier_id       Supplier ID (for supplier returns)
 * @param int|null    $source_doc_type   Source document type (ST_ constant)
 * @param int|null    $source_doc_no     Source document number
 * @param string|null $reference         External reference
 * @param string|null $memo              Notes
 * @return int return_id
 */
function add_return_order($return_type, $warehouse_loc_code, $return_date,
	$customer_id = null, $supplier_id = null,
	$source_doc_type = null, $source_doc_no = null,
	$reference = null, $memo = null) {

	$return_no = get_next_return_order_no();

	$sql = "INSERT INTO " . TB_PREF . "wh_return_orders (
		return_no, return_type, status, warehouse_loc_code, return_date,
		customer_id, supplier_id, source_doc_type, source_doc_no,
		reference, memo
	) VALUES ("
		. (int)$return_no . ", "
		. db_escape($return_type) . ", "
		. "'draft', "
		. db_escape($warehouse_loc_code) . ", "
		. "'" . date2sql($return_date) . "', "
		. ($customer_id ? (int)$customer_id : 'NULL') . ", "
		. ($supplier_id ? (int)$supplier_id : 'NULL') . ", "
		. ($source_doc_type !== null ? (int)$source_doc_type : 'NULL') . ", "
		. ($source_doc_no !== null ? (int)$source_doc_no : 'NULL') . ", "
		. ($reference ? db_escape($reference) : 'NULL') . ", "
		. ($memo ? db_escape($memo) : 'NULL')
		. ")";
	db_query($sql, 'Could not create return order');
	return db_insert_id();
}

/**
 * Update a return order header (only when in Draft status).
 *
 * @param int         $return_id         Return order ID
 * @param string      $return_type       'customer' or 'supplier'
 * @param string      $warehouse_loc_code Warehouse location code
 * @param string      $return_date       Return date (display format)
 * @param int|null    $customer_id       Customer ID
 * @param int|null    $supplier_id       Supplier ID
 * @param int|null    $source_doc_type   Source document type
 * @param int|null    $source_doc_no     Source document number
 * @param string|null $reference         External reference
 * @param string|null $memo              Notes
 * @return bool Success
 */
function update_return_order($return_id, $return_type, $warehouse_loc_code, $return_date,
	$customer_id = null, $supplier_id = null,
	$source_doc_type = null, $source_doc_no = null,
	$reference = null, $memo = null) {

	$sql = "UPDATE " . TB_PREF . "wh_return_orders SET "
		. "return_type = " . db_escape($return_type) . ", "
		. "warehouse_loc_code = " . db_escape($warehouse_loc_code) . ", "
		. "return_date = '" . date2sql($return_date) . "', "
		. "customer_id = " . ($customer_id ? (int)$customer_id : 'NULL') . ", "
		. "supplier_id = " . ($supplier_id ? (int)$supplier_id : 'NULL') . ", "
		. "source_doc_type = " . ($source_doc_type !== null ? (int)$source_doc_type : 'NULL') . ", "
		. "source_doc_no = " . ($source_doc_no !== null ? (int)$source_doc_no : 'NULL') . ", "
		. "reference = " . ($reference ? db_escape($reference) : 'NULL') . ", "
		. "memo = " . ($memo ? db_escape($memo) : 'NULL')
		. " WHERE return_id = " . (int)$return_id
		. " AND status = 'draft'";
	db_query($sql, 'Could not update return order');
	return db_num_affected_rows() > 0;
}

/**
 * Add a line item to a return order.
 *
 * @param int         $return_id       Return order ID
 * @param string      $stock_id        Item code
 * @param float       $qty_expected    Expected return quantity
 * @param int|null    $serial_id       Serial number FK
 * @param int|null    $batch_id        Batch FK
 * @param string|null $reason_code     Reason for return
 * @param string|null $memo            Line memo
 * @return int line_id
 */
function add_return_order_line($return_id, $stock_id, $qty_expected,
	$serial_id = null, $batch_id = null, $reason_code = null, $memo = null) {

	$sql = "INSERT INTO " . TB_PREF . "wh_return_order_lines (
		return_id, stock_id, qty_expected, serial_id, batch_id,
		reason_code, memo
	) VALUES ("
		. (int)$return_id . ", "
		. db_escape($stock_id) . ", "
		. db_escape($qty_expected) . ", "
		. ($serial_id ? (int)$serial_id : 'NULL') . ", "
		. ($batch_id ? (int)$batch_id : 'NULL') . ", "
		. ($reason_code ? db_escape($reason_code) : 'NULL') . ", "
		. ($memo ? db_escape($memo) : 'NULL')
		. ")";
	db_query($sql, 'Could not add return order line');
	return db_insert_id();
}

/**
 * Update a return order line (only when return is in Draft status).
 *
 * @param int         $line_id         Line ID
 * @param string      $stock_id        Item code
 * @param float       $qty_expected    Expected quantity
 * @param int|null    $serial_id       Serial number FK
 * @param int|null    $batch_id        Batch FK
 * @param string|null $reason_code     Reason code
 * @param string|null $memo            Line memo
 * @return bool Success
 */
function update_return_order_line($line_id, $stock_id, $qty_expected,
	$serial_id = null, $batch_id = null, $reason_code = null, $memo = null) {

	$sql = "UPDATE " . TB_PREF . "wh_return_order_lines SET "
		. "stock_id = " . db_escape($stock_id) . ", "
		. "qty_expected = " . db_escape($qty_expected) . ", "
		. "serial_id = " . ($serial_id ? (int)$serial_id : 'NULL') . ", "
		. "batch_id = " . ($batch_id ? (int)$batch_id : 'NULL') . ", "
		. "reason_code = " . ($reason_code ? db_escape($reason_code) : 'NULL') . ", "
		. "memo = " . ($memo ? db_escape($memo) : 'NULL')
		. " WHERE line_id = " . (int)$line_id;
	db_query($sql, 'Could not update return order line');
	return db_num_affected_rows() > 0;
}

/**
 * Delete a return order line.
 *
 * @param int $line_id Line ID
 * @return bool Success
 */
function delete_return_order_line($line_id) {
	$sql = "DELETE FROM " . TB_PREF . "wh_return_order_lines WHERE line_id = " . (int)$line_id;
	db_query($sql, 'Could not delete return order line');
	return true;
}

/**
 * Get a single return order with joined data.
 *
 * @param int $return_id Return order ID
 * @return array|false Row data or false
 */
function get_return_order($return_id) {
	$sql = "SELECT ro.*,
		c.name AS customer_name,
		sup.supp_name AS supplier_name,
		loc.location_name AS warehouse_name
		FROM " . TB_PREF . "wh_return_orders ro
		LEFT JOIN " . TB_PREF . "debtors_master c ON c.debtor_no = ro.customer_id
		LEFT JOIN " . TB_PREF . "suppliers sup ON sup.supplier_id = ro.supplier_id
		LEFT JOIN " . TB_PREF . "locations loc ON loc.loc_code = ro.warehouse_loc_code
		WHERE ro.return_id = " . (int)$return_id;
	$result = db_query($sql, 'Could not get return order');
	return db_fetch($result);
}

/**
 * Get return order lines with joined item data.
 *
 * @param int $return_id Return order ID
 * @return resource DB result
 */
function get_return_order_lines($return_id) {
	$sql = "SELECT rl.*,
		sm.description, sm.units, sm.material_cost,
		sn.serial_no, sb.batch_no,
		bl.loc_name AS bin_name
		FROM " . TB_PREF . "wh_return_order_lines rl
		LEFT JOIN " . TB_PREF . "stock_master sm ON sm.stock_id = rl.stock_id
		LEFT JOIN " . TB_PREF . "serial_numbers sn ON sn.id = rl.serial_id
		LEFT JOIN " . TB_PREF . "stock_batches sb ON sb.id = rl.batch_id
		LEFT JOIN " . TB_PREF . "wh_locations bl ON bl.loc_id = rl.to_bin_id
		WHERE rl.return_id = " . (int)$return_id
		. " ORDER BY rl.line_id";
	return db_query($sql, 'Could not get return order lines');
}

/**
 * Get return orders with optional filters.
 *
 * @param array $filters Associative array:
 *   'return_type'  => string  'customer' or 'supplier'
 *   'status'       => string  Status code
 *   'customer_id'  => int     Customer filter
 *   'supplier_id'  => int     Supplier filter
 *   'date_from'    => string  Start date (display format)
 *   'date_to'      => string  End date (display format)
 *   'loc_code'     => string  Warehouse location code
 * @return resource DB result
 */
function get_return_orders($filters = array()) {
	$sql = "SELECT ro.*,
		c.name AS customer_name,
		sup.supp_name AS supplier_name,
		loc.location_name AS warehouse_name,
		(SELECT COUNT(*) FROM " . TB_PREF . "wh_return_order_lines rl WHERE rl.return_id = ro.return_id) AS line_count,
		(SELECT COALESCE(SUM(rl.qty_expected), 0) FROM " . TB_PREF . "wh_return_order_lines rl WHERE rl.return_id = ro.return_id) AS total_expected,
		(SELECT COALESCE(SUM(rl.qty_received), 0) FROM " . TB_PREF . "wh_return_order_lines rl WHERE rl.return_id = ro.return_id) AS total_received
		FROM " . TB_PREF . "wh_return_orders ro
		LEFT JOIN " . TB_PREF . "debtors_master c ON c.debtor_no = ro.customer_id
		LEFT JOIN " . TB_PREF . "suppliers sup ON sup.supplier_id = ro.supplier_id
		LEFT JOIN " . TB_PREF . "locations loc ON loc.loc_code = ro.warehouse_loc_code
		WHERE 1=1";

	if (!empty($filters['return_type']))
		$sql .= " AND ro.return_type = " . db_escape($filters['return_type']);
	if (!empty($filters['status']))
		$sql .= " AND ro.status = " . db_escape($filters['status']);
	if (!empty($filters['customer_id']))
		$sql .= " AND ro.customer_id = " . (int)$filters['customer_id'];
	if (!empty($filters['supplier_id']))
		$sql .= " AND ro.supplier_id = " . (int)$filters['supplier_id'];
	if (!empty($filters['date_from']))
		$sql .= " AND ro.return_date >= '" . date2sql($filters['date_from']) . "'";
	if (!empty($filters['date_to']))
		$sql .= " AND ro.return_date <= '" . date2sql($filters['date_to']) . "'";
	if (!empty($filters['loc_code']))
		$sql .= " AND ro.warehouse_loc_code = " . db_escape($filters['loc_code']);

	$sql .= " ORDER BY ro.return_date DESC, ro.return_id DESC";

	return db_query($sql, 'Could not get return orders');
}

/**
 * Update return order status.
 *
 * @param int    $return_id Return order ID
 * @param string $status    New status code
 * @return bool Success
 */
function update_return_order_status($return_id, $status) {
	$updates = "status = " . db_escape($status);
	if ($status === 'received') {
		$updates .= ", received_date = '" . date2sql(Today()) . "'";
	}

	$sql = "UPDATE " . TB_PREF . "wh_return_orders SET "
		. $updates
		. " WHERE return_id = " . (int)$return_id;
	db_query($sql, 'Could not update return order status');
	return db_num_affected_rows() > 0;
}

/**
 * Receive a return order — mark as received, create inbound stock moves.
 *
 * For customer returns: creates positive stock moves (goods coming back in).
 * For supplier returns: no stock move at this stage (items already in stock, will be removed at ship).
 *
 * @param int    $return_id Return order ID
 * @param string $date_     Receive date (display format)
 * @return bool Success
 */
function receive_return_order($return_id, $date_ = null) {
	global $Refs, $path_to_root;

	if (!$date_) $date_ = Today();

	$order = get_return_order($return_id);
	if (!$order || $order['status'] !== 'draft') return false;

	begin_transaction();

	// For customer returns, create positive stock moves (receiving items back)
	if ($order['return_type'] === 'customer') {
		$adj_id = get_next_trans_no(ST_INVADJUST);
		$ref = $Refs->get_next(ST_INVADJUST, null, $date_);

		$lines = get_return_order_lines($return_id);
		while ($line = db_fetch($lines)) {
			$qty = $line['qty_expected'];
			$cost = get_unit_cost($line['stock_id']);

			update_average_material_cost(null, $line['stock_id'], $cost, $qty, $date_);

			$extended = array();
			if ($line['serial_id']) $extended['serial_id'] = $line['serial_id'];
			if ($line['batch_id']) $extended['batch_id'] = $line['batch_id'];
			if ($line['to_bin_id']) $extended['to_bin_id'] = $line['to_bin_id'];
			$extended['stock_status'] = 'quarantine';

			add_stock_move_extended(ST_INVADJUST, $line['stock_id'], $adj_id,
				$order['warehouse_loc_code'], $date_, $ref, $qty, $cost, 0, $extended);

			// Update serial status to 'returned'
			if ($line['serial_id']) {
				include_once($path_to_root . '/inventory/includes/db/serial_numbers_db.inc');
				if (function_exists('update_serial_status')) {
					update_serial_status($line['serial_id'], 'returned', ST_INVADJUST, $adj_id,
						null, $order['warehouse_loc_code'], $date_, $ref, _('Customer return received'));
				}
			}

			// Log batch movement
			if ($line['batch_id']) {
				include_once($path_to_root . '/inventory/includes/db/stock_batches_db.inc');
				if (function_exists('add_batch_movement')) {
					add_batch_movement($line['batch_id'], 'return', ST_INVADJUST, $adj_id,
						null, $order['warehouse_loc_code'], $qty, _('Customer return received'));
				}
			}

			// Update bin stock
			if ($line['to_bin_id']) {
				$wh_ops_path = $path_to_root . '/inventory/warehouse/includes/warehouse_operations.inc';
				if (file_exists($wh_ops_path)) {
					include_once($wh_ops_path);
				}
				if (function_exists('update_bin_stock')) {
					update_bin_stock($line['to_bin_id'], $line['stock_id'], $qty,
						$line['batch_id'], $line['serial_id'], null, 'quarantine');
				}
			}

			// Update line qty_received
			$sql = "UPDATE " . TB_PREF . "wh_return_order_lines SET qty_received = "
				. db_escape($qty) . " WHERE line_id = " . (int)$line['line_id'];
			db_query($sql, 'Could not update return line received qty');

			// GL: debit inventory, credit COGS (reverse of delivery)
			$stock_gl_codes = get_stock_gl_code($line['stock_id']);
			$value = round2($cost * $qty, user_price_dec());
			if ($value > 0) {
				$gl_memo = sprintf(_('Customer return RO#%s — %s'), $order['return_no'], $line['stock_id']);
				add_gl_trans_std_cost(ST_INVADJUST, $adj_id, $date_,
					$stock_gl_codes['inventory_account'],
					$stock_gl_codes['dimension_id'], $stock_gl_codes['dimension2_id'],
					$gl_memo, $value);
				add_gl_trans_std_cost(ST_INVADJUST, $adj_id, $date_,
					$stock_gl_codes['cogs_account'],
					$stock_gl_codes['dimension_id'], $stock_gl_codes['dimension2_id'],
					$gl_memo, -$value);
			}
		}

		$Refs->save(ST_INVADJUST, $adj_id, $ref);
		add_comments(ST_INVADJUST, $adj_id, $date_, sprintf(_('Customer return RO#%s received'), $order['return_no']));
		add_audit_trail(ST_INVADJUST, $adj_id, $date_);
	}

	update_return_order_status($return_id, 'received');

	// Create warehouse operation
	include_once(dirname(__FILE__) . '/warehouse_operations_db.inc');
	if (function_exists('add_wh_operation')) {
		$op_id = add_wh_operation('return', 'done', null, $return_id,
			null, null, null, null, null, 5, null,
			sprintf(_('Return order RO#%s received'), $order['return_no']));
	}

	commit_transaction();
	return true;
}

/**
 * Inspect a return order line — classify received quantities into good/damaged/scrap.
 *
 * @param int   $line_id       Line ID
 * @param float $qty_good      Quantity in good condition
 * @param float $qty_damaged   Quantity damaged (for refurbish)
 * @param float $qty_scrap     Quantity to scrap
 * @param string|null $disposition_code Overall disposition for the line
 * @return bool Success
 */
function inspect_return_line($line_id, $qty_good, $qty_damaged, $qty_scrap, $disposition_code = null) {
	$sql = "UPDATE " . TB_PREF . "wh_return_order_lines SET "
		. "qty_good = " . db_escape($qty_good) . ", "
		. "qty_damaged = " . db_escape($qty_damaged) . ", "
		. "qty_scrap = " . db_escape($qty_scrap) . ", "
		. "disposition_code = " . ($disposition_code ? db_escape($disposition_code) : 'NULL')
		. " WHERE line_id = " . (int)$line_id;
	db_query($sql, 'Could not inspect return line');
	return true;
}

/**
 * Mark a return order as inspected (all lines have been inspected).
 *
 * @param int $return_id Return order ID
 * @return bool Success
 */
function mark_return_inspected($return_id) {
	return update_return_order_status($return_id, 'inspected');
}

/**
 * Complete a return order — execute disposition routing for all lines.
 *
 * For each line:
 *   - qty_good: restock (move to available bin, update serial to 'available')
 *   - qty_damaged: keep in quarantine for refurbishment
 *   - qty_scrap: create scrap entry (negative stock move + GL)
 *
 * @param int    $return_id Return order ID
 * @param string $date_     Completion date (display format)
 * @return bool Success
 */
function complete_return_order($return_id, $date_ = null) {
	global $Refs, $path_to_root;

	if (!$date_) $date_ = Today();

	$order = get_return_order($return_id);
	if (!$order || !in_array($order['status'], array('received', 'inspected'))) return false;

	begin_transaction();

	$lines = get_return_order_lines($return_id);
	while ($line = db_fetch($lines)) {
		// Handle qty_good — restock: update serial/batch status
		if ($line['qty_good'] > 0) {
			if ($line['serial_id']) {
				include_once($path_to_root . '/inventory/includes/db/serial_numbers_db.inc');
				if (function_exists('update_serial_status')) {
					update_serial_status($line['serial_id'], 'available', 0, 0,
						$order['warehouse_loc_code'], $order['warehouse_loc_code'],
						$date_, '', _('Restocked from return order RO#') . $order['return_no']);
				}
			}
			if ($line['batch_id']) {
				include_once($path_to_root . '/inventory/includes/db/stock_batches_db.inc');
				if (function_exists('add_batch_movement')) {
					add_batch_movement($line['batch_id'], 'return', 0, 0,
						null, $order['warehouse_loc_code'], $line['qty_good'],
						_('Restocked from return order RO#') . $order['return_no']);
				}
			}
			// Update bin stock status to available
			if ($line['to_bin_id'] && function_exists('update_bin_stock')) {
				// Set available status; the qty is already there from receive step
				$sql = "UPDATE " . TB_PREF . "wh_bin_stock SET stock_status = 'available'"
					. " WHERE wh_loc_id = " . (int)$line['to_bin_id']
					. " AND stock_id = " . db_escape($line['stock_id']);
				db_query($sql, 'Could not update bin stock status');
			}
		}

		// Handle qty_scrap — create scrap entry
		if ($line['qty_scrap'] > 0) {
			include_once(dirname(__FILE__) . '/warehouse_scrap_db.inc');
			if (function_exists('add_scrap_entry')) {
				add_scrap_entry(
					$line['stock_id'],
					$line['qty_scrap'],
					$order['warehouse_loc_code'],
					$date_,
					'customer_return',
					sprintf(_('From return order RO#%s'), $order['return_no']),
					$line['to_bin_id'],
					null,
					$line['serial_id'],
					$line['batch_id'],
					sprintf(_('Scrap from return order RO#%s'), $order['return_no'])
				);
			}
		}

		// Handle qty_damaged — keep in quarantine area (no stock move needed; already in quarantine from receive)
		// A separate refurbishment workflow could be added later.
	}

	// For supplier returns, create negative stock moves (shipping items back)
	if ($order['return_type'] === 'supplier') {
		$adj_id = get_next_trans_no(ST_INVADJUST);
		$ref = $Refs->get_next(ST_INVADJUST, null, $date_);

		$lines2 = get_return_order_lines($return_id);
		while ($line = db_fetch($lines2)) {
			$qty = $line['qty_expected'];
			$cost = get_unit_cost($line['stock_id']);

			update_average_material_cost(null, $line['stock_id'], $cost, -$qty, $date_);

			$extended = array();
			if ($line['serial_id']) $extended['serial_id'] = $line['serial_id'];
			if ($line['batch_id']) $extended['batch_id'] = $line['batch_id'];
			if ($line['to_bin_id']) $extended['from_bin_id'] = $line['to_bin_id'];

			add_stock_move_extended(ST_INVADJUST, $line['stock_id'], $adj_id,
				$order['warehouse_loc_code'], $date_, $ref, -$qty, $cost, 0, $extended);

			// Update bin stock
			if ($line['to_bin_id']) {
				$wh_ops_path = $path_to_root . '/inventory/warehouse/includes/warehouse_operations.inc';
				if (file_exists($wh_ops_path)) {
					include_once($wh_ops_path);
				}
				if (function_exists('update_bin_stock')) {
					update_bin_stock($line['to_bin_id'], $line['stock_id'], -$qty,
						$line['batch_id'], $line['serial_id']);
				}
			}

			// GL: debit purchase returns account, credit inventory
			$stock_gl_codes = get_stock_gl_code($line['stock_id']);
			$value = round2($cost * $qty, user_price_dec());
			if ($value > 0) {
				$gl_memo = sprintf(_('Supplier return RO#%s — %s'), $order['return_no'], $line['stock_id']);
				add_gl_trans_std_cost(ST_INVADJUST, $adj_id, $date_,
					$stock_gl_codes['adjustment_account'],
					$stock_gl_codes['dimension_id'], $stock_gl_codes['dimension2_id'],
					$gl_memo, -$value);
				add_gl_trans_std_cost(ST_INVADJUST, $adj_id, $date_,
					$stock_gl_codes['inventory_account'],
					$stock_gl_codes['dimension_id'], $stock_gl_codes['dimension2_id'],
					$gl_memo, $value);
			}
		}

		$Refs->save(ST_INVADJUST, $adj_id, $ref);
		add_comments(ST_INVADJUST, $adj_id, $date_,
			sprintf(_('Supplier return RO#%s completed'), $order['return_no']));
		add_audit_trail(ST_INVADJUST, $adj_id, $date_);
	}

	update_return_order_status($return_id, 'completed');
	commit_transaction();
	return true;
}

/**
 * Cancel a return order (any status except completed).
 *
 * @param int $return_id Return order ID
 * @return bool Success
 */
function cancel_return_order($return_id) {
	$order = get_return_order($return_id);
	if (!$order || $order['status'] === 'completed') return false;

	return update_return_order_status($return_id, 'cancelled');
}

/**
 * Delete a return order and its lines (only when in Draft status).
 *
 * @param int $return_id Return order ID
 * @return bool Success
 */
function delete_return_order($return_id) {
	$order = get_return_order($return_id);
	if (!$order || $order['status'] !== 'draft') return false;

	begin_transaction();
	$sql = "DELETE FROM " . TB_PREF . "wh_return_order_lines WHERE return_id = " . (int)$return_id;
	db_query($sql, 'Could not delete return order lines');

	$sql = "DELETE FROM " . TB_PREF . "wh_return_orders WHERE return_id = " . (int)$return_id;
	db_query($sql, 'Could not delete return order');
	commit_transaction();

	return true;
}

/**
 * Get return order summary statistics.
 *
 * @param string|null $loc_code Warehouse location code filter
 * @return array Summary with counts by status and type
 */
function get_return_order_summary($loc_code = null) {
	$where = "1=1";
	if ($loc_code) {
		$where = "ro.warehouse_loc_code = " . db_escape($loc_code);
	}

	$sql = "SELECT status, COUNT(*) AS cnt
		FROM " . TB_PREF . "wh_return_orders ro
		WHERE " . $where
		. " GROUP BY status";
	$result = db_query($sql, 'Could not get return order summary');
	$by_status = array();
	while ($row = db_fetch($result)) {
		$by_status[$row['status']] = (int)$row['cnt'];
	}

	$sql = "SELECT return_type, COUNT(*) AS cnt
		FROM " . TB_PREF . "wh_return_orders ro
		WHERE " . $where
		. " GROUP BY return_type";
	$result = db_query($sql, 'Could not get return type summary');
	$by_type = array();
	while ($row = db_fetch($result)) {
		$by_type[$row['return_type']] = (int)$row['cnt'];
	}

	return array(
		'by_status' => $by_status,
		'by_type'   => $by_type,
	);
}
