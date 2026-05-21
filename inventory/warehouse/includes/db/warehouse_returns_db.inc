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
 * Find an open return order line for a serial number.
 *
 * @param int $serial_id Serial number id
 * @return array|false Matching open return line row or false
 */
function find_open_return_line_by_serial($serial_id)
{
	$sql = "SELECT rl.line_id, ro.return_id, ro.return_no, ro.status
		FROM " . TB_PREF . "wh_return_order_lines rl
		INNER JOIN " . TB_PREF . "wh_return_orders ro ON ro.return_id = rl.return_id
		WHERE rl.serial_id = " . (int)$serial_id
		. " AND ro.status IN ('draft', 'received', 'inspected')
		ORDER BY rl.line_id DESC LIMIT 1";
	$result = db_query($sql, 'Could not find open return line by serial');
	return db_fetch($result);
}

/**
 * Normalize and validate optional return-order line text input.
 *
 * @param mixed  $value         Submitted value
 * @param int    $max_length    Maximum allowed length
 * @param string $field_label   Translated field label
 * @param string $error_message Output error text
 * @return string|null|false Normalized value, null for blank input, false on validation failure
 */
function normalize_return_order_line_text($value, $max_length, $field_label, &$error_message)
{
	if ($value === null || $value === '')
		return null;

	if (!is_scalar($value)) {
		$error_message = sprintf(_('%s must be plain text.'), $field_label);
		return false;
	}

	$value = trim((string)$value);
	if ($value === '')
		return null;

	$length = function_exists('mb_strlen') ? mb_strlen($value, 'UTF-8') : strlen($value);
	if ($length > $max_length) {
		$error_message = sprintf(_('%s cannot exceed %s characters.'), $field_label, $max_length);
		return false;
	}

	return $value;
}

/**
 * Validate a tracked return line before it is inserted.
 *
 * @param int         $return_id    Return order id
 * @param string      $stock_id     Item code
 * @param float       $qty_expected Expected quantity
 * @param int|null    $serial_id    Serial id
 * @param int|null    $batch_id     Batch id
 * @param string|null $error_message Output error text
 * @return bool
 */
function validate_return_order_line_request($return_id, $stock_id, $qty_expected,
	$serial_id = null, $batch_id = null, &$error_message = '')
{
	$error_message = '';
	$order = get_return_order($return_id);
	if (!$order || $order['status'] !== 'draft') {
		$error_message = _('Only draft return orders can accept new line items.');
		return false;
	}

	if (!is_scalar($qty_expected) || (float)$qty_expected <= 0) {
		$error_message = _('Return quantity must be greater than zero.');
		return false;
	}

	$tracking_mode = function_exists('get_item_tracking_mode')
		? get_item_tracking_mode($stock_id)
		: 'none';
	$requires_serial = in_array($tracking_mode, array('serial', 'serial_batch'));
	$requires_batch = in_array($tracking_mode, array('batch', 'serial_batch'));
	$qty_expected = (float)$qty_expected;

	if ($requires_serial && !$serial_id) {
		$error_message = _('A serial number is required for this tracked item.');
		return false;
	}

	if ($requires_batch && !$batch_id) {
		$error_message = _('A batch number is required for this tracked item.');
		return false;
	}

	if ($requires_serial && abs($qty_expected - 1.0) > 0.000001) {
		$error_message = _('Serial-tracked returns must use a quantity of exactly 1.');
		return false;
	}

	if ($serial_id) {
		$serial = function_exists('get_serial_number') ? get_serial_number($serial_id) : false;
		if (!$serial) {
			$error_message = _('The selected serial number was not found.');
			return false;
		}

		if ($serial['stock_id'] !== $stock_id) {
			$error_message = _('The selected serial number does not belong to the chosen item.');
			return false;
		}

		if ($order['return_type'] === 'customer') {
			$allowed_statuses = array('delivered', 'in_repair', 'recalled');
			if (!$order['customer_id'] || (int)$serial['customer_id'] !== (int)$order['customer_id']) {
				$error_message = _('The selected serial number is not assigned to this customer.');
				return false;
			}
			if (!in_array($serial['status'], $allowed_statuses)) {
				$error_message = sprintf(
					_('The selected serial number cannot be returned while its status is %s.'),
					$serial['status']
				);
				return false;
			}
		}

		$existing_line = find_open_return_line_by_serial($serial_id);
		if ($existing_line) {
			$error_message = sprintf(
				_('This serial number is already assigned to open return order RO#%s.'),
				$existing_line['return_no']
			);
			return false;
		}

		if ($requires_batch && (int)$serial['batch_id'] !== (int)$batch_id) {
			$error_message = _('The selected serial number does not belong to the chosen batch.');
			return false;
		}
	}

	if ($batch_id) {
		$batch = function_exists('get_stock_batch') ? get_stock_batch($batch_id) : false;
		if (!$batch) {
			$error_message = _('The selected batch number was not found.');
			return false;
		}

		if ($batch['stock_id'] !== $stock_id) {
			$error_message = _('The selected batch number does not belong to the chosen item.');
			return false;
		}
	}

	return true;
}

/**
 * Add a line item to a return order.
 *
 * @param int         $return_id       Return order ID
 * @param string      $stock_id        Item code
 * @param float       $qty_expected    Expected return quantity
 * @param int|null    $to_bin_id       Quarantine / destination bin
 * @param int|null    $serial_id       Serial number FK
 * @param int|null    $batch_id        Batch FK
 * @param string|null $reason_code     Reason for return
 * @param string|null $memo            Line memo
 * @return int|false line_id on success, false on validation failure
 */
function add_return_order_line($return_id, $stock_id, $qty_expected,
	$to_bin_id = null, $serial_id = null, $batch_id = null, $reason_code = null, $memo = null) {
	$error_message = '';
	$reason_code = normalize_return_order_line_text($reason_code, 20, _('Return reason'), $error_message);
	if ($reason_code === false) {
		display_error($error_message);
		return false;
	}
	$memo = normalize_return_order_line_text($memo, 255, _('Return memo'), $error_message);
	if ($memo === false) {
		display_error($error_message);
		return false;
	}

	if (!validate_return_order_line_request($return_id, $stock_id, $qty_expected,
		$serial_id, $batch_id, $error_message)) {
		if ($error_message !== '')
			display_error($error_message);
		return false;
	}

	$sql = "INSERT INTO " . TB_PREF . "wh_return_order_lines (
		return_id, stock_id, qty_expected, to_bin_id, serial_id, batch_id,
		reason_code, memo
	) VALUES ("
		. (int)$return_id . ", "
		. db_escape($stock_id) . ", "
		. db_escape($qty_expected) . ", "
		. ($to_bin_id ? (int)$to_bin_id : 'NULL') . ", "
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
 * @param int|null    $to_bin_id       Quarantine / destination bin
 * @param int|null    $serial_id       Serial number FK
 * @param int|null    $batch_id        Batch FK
 * @param string|null $reason_code     Reason code
 * @param string|null $memo            Line memo
 * @return bool Success
 */
function update_return_order_line($line_id, $stock_id, $qty_expected,
	$to_bin_id = null, $serial_id = null, $batch_id = null, $reason_code = null, $memo = null) {
	$error_message = '';
	$reason_code = normalize_return_order_line_text($reason_code, 20, _('Return reason'), $error_message);
	if ($reason_code === false) {
		display_error($error_message);
		return false;
	}
	$memo = normalize_return_order_line_text($memo, 255, _('Return memo'), $error_message);
	if ($memo === false) {
		display_error($error_message);
		return false;
	}

	$sql = "UPDATE " . TB_PREF . "wh_return_order_lines SET "
		. "stock_id = " . db_escape($stock_id) . ", "
		. "qty_expected = " . db_escape($qty_expected) . ", "
		. "to_bin_id = " . ($to_bin_id ? (int)$to_bin_id : 'NULL') . ", "
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

/**
 * Validate inspection quantities for a return line.
 *
 * @param int   $line_id           Return line ID
 * @param float $qty_good          Good quantity
 * @param float $qty_damaged        Damaged quantity
 * @param float $qty_scrap          Scrap quantity
 * @param bool  $require_exact_total Whether totals must match the received quantity exactly
 * @param string $error_message     Output error message
 * @return bool
 */
function validate_return_line_quantities($line_id, $qty_good, $qty_damaged, $qty_scrap, $require_exact_total, &$error_message) {
	$error_message = '';

	if (!is_scalar($qty_good) || !is_scalar($qty_damaged) || !is_scalar($qty_scrap)) {
		$error_message = _('Inspection quantities must be plain numeric values.');
		return false;
	}

	$qty_good = (float)$qty_good;
	$qty_damaged = (float)$qty_damaged;
	$qty_scrap = (float)$qty_scrap;

	if (is_nan($qty_good) || is_infinite($qty_good)
		|| is_nan($qty_damaged) || is_infinite($qty_damaged)
		|| is_nan($qty_scrap) || is_infinite($qty_scrap)) {
		$error_message = _('Inspection quantities must be finite numbers.');
		return false;
	}

	if ($qty_good < 0 || $qty_damaged < 0 || $qty_scrap < 0) {
		$error_message = _('Inspection quantities cannot be negative.');
		return false;
	}

	$sql = "SELECT qty_received FROM " . TB_PREF . "wh_return_order_lines WHERE line_id = " . (int)$line_id;
	$result = db_query($sql, 'Could not validate return order line quantities');
	$line = db_fetch($result);
	if (!$line) {
		$error_message = _('Return order line was not found.');
		return false;
	}

	$qty_received = (float)$line['qty_received'];
	$qty_total = round2($qty_good + $qty_damaged + $qty_scrap, user_qty_dec());

	if ($qty_total - $qty_received > 0.000001) {
		$error_message = sprintf(_('Inspection quantities must not exceed the received quantity of %s.'), number_format2($qty_received, user_qty_dec()));
		return false;
	}

	if ($require_exact_total && abs($qty_total - $qty_received) > 0.000001) {
		$error_message = sprintf(_('Inspection quantities must total %s before completion.'), number_format2($qty_received, user_qty_dec()));
		return false;
	}

	return true;
}

function inspect_return_line($line_id, $qty_good, $qty_damaged, $qty_scrap, $disposition_code = null) {
	$error_message = '';
	if (!validate_return_line_quantities($line_id, $qty_good, $qty_damaged, $qty_scrap, false, $error_message)) {
		return false;
	}
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
		$error_message = '';
		if (!validate_return_line_quantities($line['line_id'], $line['qty_good'], $line['qty_damaged'], $line['qty_scrap'], true, $error_message)) {
			cancel_transaction();
			return false;
		}

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
