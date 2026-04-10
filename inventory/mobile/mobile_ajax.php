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
 * Mobile Scanner AJAX Endpoint
 *
 * Handles server-side operations for mobile warehouse pages:
 *   - scan_lookup: Barcode/serial/batch/bin lookup
 *   - confirm_receive: Confirm receipt of item into bin
 *   - confirm_ship: Confirm serial selection for delivery
 *   - confirm_transfer: Confirm bin-to-bin transfer
 *   - count_line: Submit a cycle count line
 *   - serial_lookup: Quick serial info lookup
 *   - confirm_putaway: Confirm putaway to bin
 *   - confirm_pick: Confirm pick from bin
 *
 * All responses are JSON. Requires valid session.
 */

$page_security = 'SA_OPEN';
$path_to_root = '../..';

include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/includes/db/connect_db.inc');
include_once($path_to_root . '/includes/date_functions.inc');
include_once($path_to_root . '/inventory/includes/db/items_db.inc');
include_once($path_to_root . '/inventory/includes/db/serial_numbers_db.inc');
include_once($path_to_root . '/inventory/includes/db/stock_batches_db.inc');
include_once($path_to_root . '/inventory/includes/db/serial_batch_db.inc');
include_once($path_to_root . '/inventory/includes/gs1_standards.inc');
include_once($path_to_root . '/inventory/warehouse/includes/db/warehouse_locations_db.inc');
include_once($path_to_root . '/inventory/warehouse/includes/db/warehouse_operations_db.inc');
include_once($path_to_root . '/inventory/warehouse/includes/warehouse_operations.inc');

// Load optional DB modules
$counting_db = $path_to_root . '/inventory/warehouse/includes/db/warehouse_counting_db.inc';
if (file_exists($counting_db))
	include_once($counting_db);

$transfers_db = $path_to_root . '/inventory/warehouse/includes/db/warehouse_transfers_db.inc';
if (file_exists($transfers_db))
	include_once($transfers_db);

$picking_db = $path_to_root . '/inventory/warehouse/includes/db/warehouse_picking_db.inc';
if (file_exists($picking_db))
	include_once($picking_db);

header('Content-Type: application/json; charset=utf-8');
header('Cache-Control: no-cache, no-store, must-revalidate');

$action = '';
if (isset($_POST['action']))
	$action = trim($_POST['action']);
elseif (isset($_GET['action']))
	$action = trim($_GET['action']);

if (empty($action)) {
	echo json_encode(array('success' => false, 'error' => 'No action specified'));
	exit();
}

$response = array('success' => false, 'error' => 'Unknown action');

switch ($action) {

	case 'scan_lookup':
		$response = mobile_scan_lookup();
		break;

	case 'confirm_receive':
		$response = mobile_confirm_receive();
		break;

	case 'confirm_ship':
		$response = mobile_confirm_ship();
		break;

	case 'confirm_transfer':
		$response = mobile_confirm_transfer();
		break;

	case 'count_line':
		$response = mobile_count_line();
		break;

	case 'serial_lookup':
		$response = mobile_serial_lookup();
		break;

	case 'confirm_putaway':
		$response = mobile_confirm_putaway();
		break;

	case 'confirm_pick':
		$response = mobile_confirm_pick();
		break;

	case 'get_pending_receive':
		$response = mobile_get_pending_receive();
		break;

	case 'get_pending_picks':
		$response = mobile_get_pending_picks();
		break;

	case 'get_pending_counts':
		$response = mobile_get_pending_counts();
		break;

	case 'get_bin_contents':
		$response = mobile_get_bin_contents();
		break;
}

echo json_encode($response);
exit();

// ======================================================================
// Action handlers
// ======================================================================

/**
 * Scan lookup — identify barcode content (item, serial, batch, or bin location).
 *
 * @return array JSON response
 */
function mobile_scan_lookup() {
	$scan = isset($_POST['scan']) ? trim($_POST['scan']) : '';
	if (empty($scan))
		return array('success' => false, 'error' => _('No scan data'));

	$result = array('success' => true, 'scan' => $scan, 'matches' => array());

	// 1. Try GS1 parse
	$gs1 = parse_gs1_barcode($scan);
	if (!empty($gs1)) {
		$result['gs1'] = $gs1;
		if (isset($gs1['21'])) {
			$serial = get_serial_number_by_code($gs1['21']);
			if ($serial) {
				$result['matches'][] = array(
					'type' => 'serial',
					'serial_id' => $serial['id'],
					'serial_no' => $serial['serial_no'],
					'stock_id' => $serial['stock_id'],
					'status' => $serial['status'],
					'loc_code' => $serial['loc_code'],
					'wh_loc_id' => $serial['wh_loc_id'],
				);
			}
		}
		if (isset($gs1['10'])) {
			$batch = get_stock_batch_by_code($gs1['10']);
			if ($batch) {
				$result['matches'][] = array(
					'type' => 'batch',
					'batch_id' => $batch['id'],
					'batch_no' => $batch['batch_no'],
					'stock_id' => $batch['stock_id'],
					'status' => $batch['status'],
					'expiry_date' => $batch['expiry_date'],
				);
			}
		}
		if (!empty($result['matches']))
			return $result;
	}

	// 2. Try serial number exact match
	$serial = get_serial_number_by_code($scan);
	if ($serial) {
		$result['matches'][] = array(
			'type' => 'serial',
			'serial_id' => $serial['id'],
			'serial_no' => $serial['serial_no'],
			'stock_id' => $serial['stock_id'],
			'status' => $serial['status'],
			'loc_code' => $serial['loc_code'],
			'wh_loc_id' => $serial['wh_loc_id'],
		);
		return $result;
	}

	// 3. Try batch number exact match
	$batch = get_stock_batch_by_code($scan);
	if ($batch) {
		$result['matches'][] = array(
			'type' => 'batch',
			'batch_id' => $batch['id'],
			'batch_no' => $batch['batch_no'],
			'stock_id' => $batch['stock_id'],
			'status' => $batch['status'],
			'expiry_date' => $batch['expiry_date'],
		);
		return $result;
	}

	// 4. Try bin location code
	$bin = get_warehouse_location_by_code($scan);
	if ($bin) {
		$result['matches'][] = array(
			'type' => 'bin',
			'loc_id' => $bin['loc_id'],
			'loc_code' => $bin['loc_code'],
			'loc_name' => $bin['loc_name'],
			'can_store_stock' => $bin['can_store_stock'],
		);
		return $result;
	}

	// 5. Try item stock_id
	$item = get_item($scan);
	if ($item) {
		$result['matches'][] = array(
			'type' => 'item',
			'stock_id' => $item['stock_id'],
			'description' => $item['description'],
			'units' => $item['units'],
			'track_by' => $item['track_by'],
		);
		return $result;
	}

	$result['success'] = false;
	$result['error'] = sprintf(_('No match found for "%s"'), $scan);
	return $result;
}

/**
 * Confirm receipt of item into a bin (part of inbound flow).
 *
 * @return array JSON response
 */
function mobile_confirm_receive() {
	$op_id = isset($_POST['op_id']) ? (int)$_POST['op_id'] : 0;
	$stock_id = isset($_POST['stock_id']) ? trim($_POST['stock_id']) : '';
	$qty = isset($_POST['qty']) ? (float)$_POST['qty'] : 0;
	$bin_loc_id = isset($_POST['bin_loc_id']) ? (int)$_POST['bin_loc_id'] : 0;
	$serial_no = isset($_POST['serial_no']) ? trim($_POST['serial_no']) : '';
	$batch_no = isset($_POST['batch_no']) ? trim($_POST['batch_no']) : '';
	$loc_code = isset($_POST['loc_code']) ? trim($_POST['loc_code']) : '';

	if (empty($stock_id) || $qty <= 0)
		return array('success' => false, 'error' => _('Item and quantity are required'));

	$serial_id = null;
	$batch_id = null;

	// Resolve serial
	if (!empty($serial_no)) {
		$serial = get_serial_number_by_code($serial_no, $stock_id);
		if (!$serial)
			return array('success' => false, 'error' => sprintf(_('Serial "%s" not found'), $serial_no));
		$serial_id = $serial['id'];
	}

	// Resolve batch
	if (!empty($batch_no)) {
		$batch = get_stock_batch_by_code($batch_no, $stock_id);
		if (!$batch)
			return array('success' => false, 'error' => sprintf(_('Batch "%s" not found'), $batch_no));
		$batch_id = $batch['id'];
	}

	// Capacity check if bin specified
	if ($bin_loc_id > 0) {
		$item = get_item($stock_id);
		$weight = $item ? (float)$item['item_weight'] * $qty : 0;
		$volume = $item ? (float)$item['item_volume'] * $qty : 0;
		$cap = check_can_store($bin_loc_id, $stock_id, $qty, $weight, $volume, $batch_id);
		if (!$cap['allowed'])
			return array('success' => false, 'error' => $cap['reason']);
	}

	begin_transaction();

	// Update bin stock
	if ($bin_loc_id > 0) {
		update_bin_stock($bin_loc_id, $stock_id, $qty, $batch_id, $serial_id, null, 'available');
	}

	// Update serial location if applicable
	if ($serial_id) {
		$sql = "UPDATE " . TB_PREF . "serial_numbers SET status='available', loc_code="
			. db_escape($loc_code) . ", wh_loc_id=" . (int)$bin_loc_id
			. " WHERE id=" . (int)$serial_id;
		db_query($sql);
		add_serial_movement($serial_id, ST_SUPPRECEIVE, 0, '', $loc_code,
			'', 'available', date('Y-m-d'), 'Mobile receive', 'Received via mobile scanner');
	}

	// Update operation status if provided
	if ($op_id > 0) {
		complete_wh_operation($op_id);
	}

	commit_transaction();

	return array(
		'success' => true,
		'message' => sprintf(_('%s x %s received into bin'), $stock_id, number_format2($qty, get_qty_dec($stock_id))),
	);
}

/**
 * Confirm serial selection for outbound shipment.
 *
 * @return array JSON response
 */
function mobile_confirm_ship() {
	$serial_no = isset($_POST['serial_no']) ? trim($_POST['serial_no']) : '';
	$stock_id = isset($_POST['stock_id']) ? trim($_POST['stock_id']) : '';
	$op_id = isset($_POST['op_id']) ? (int)$_POST['op_id'] : 0;

	if (empty($serial_no) || empty($stock_id))
		return array('success' => false, 'error' => _('Serial number and item are required'));

	$serial = get_serial_number_by_code($serial_no, $stock_id);
	if (!$serial)
		return array('success' => false, 'error' => sprintf(_('Serial "%s" not found for item %s'), $serial_no, $stock_id));

	if ($serial['status'] !== 'available' && $serial['status'] !== 'reserved')
		return array('success' => false, 'error' => sprintf(_('Serial "%s" status is %s — cannot ship'), $serial_no, $serial['status']));

	begin_transaction();

	// Mark serial as delivered
	update_serial_status($serial['id'], 'delivered', ST_CUSTDELIVERY, 0,
		$serial['loc_code'], '', date('Y-m-d'), 'Mobile ship', 'Shipped via mobile scanner');

	// Remove from bin stock
	if ($serial['wh_loc_id']) {
		update_bin_stock($serial['wh_loc_id'], $stock_id, -1, null, $serial['id'], null, 'available');
	}

	if ($op_id > 0)
		complete_wh_operation($op_id);

	commit_transaction();

	return array(
		'success' => true,
		'message' => sprintf(_('Serial %s shipped'), $serial_no),
	);
}

/**
 * Confirm bin-to-bin transfer.
 *
 * @return array JSON response
 */
function mobile_confirm_transfer() {
	$stock_id = isset($_POST['stock_id']) ? trim($_POST['stock_id']) : '';
	$qty = isset($_POST['qty']) ? (float)$_POST['qty'] : 0;
	$from_bin_id = isset($_POST['from_bin_id']) ? (int)$_POST['from_bin_id'] : 0;
	$to_bin_id = isset($_POST['to_bin_id']) ? (int)$_POST['to_bin_id'] : 0;
	$serial_no = isset($_POST['serial_no']) ? trim($_POST['serial_no']) : '';
	$batch_no = isset($_POST['batch_no']) ? trim($_POST['batch_no']) : '';
	$loc_code = isset($_POST['loc_code']) ? trim($_POST['loc_code']) : '';

	if (empty($stock_id) || $qty <= 0 || $from_bin_id <= 0 || $to_bin_id <= 0)
		return array('success' => false, 'error' => _('Item, quantity, source bin, and destination bin are required'));

	if ($from_bin_id === $to_bin_id)
		return array('success' => false, 'error' => _('Source and destination bins cannot be the same'));

	$serial_id = null;
	$batch_id = null;

	if (!empty($serial_no)) {
		$serial = get_serial_number_by_code($serial_no, $stock_id);
		if (!$serial)
			return array('success' => false, 'error' => sprintf(_('Serial "%s" not found'), $serial_no));
		$serial_id = $serial['id'];
	}

	if (!empty($batch_no)) {
		$batch = get_stock_batch_by_code($batch_no, $stock_id);
		if (!$batch)
			return array('success' => false, 'error' => sprintf(_('Batch "%s" not found'), $batch_no));
		$batch_id = $batch['id'];
	}

	// Capacity check on destination bin
	$item = get_item($stock_id);
	$weight = $item ? (float)$item['item_weight'] * $qty : 0;
	$volume = $item ? (float)$item['item_volume'] * $qty : 0;
	$cap = check_can_store($to_bin_id, $stock_id, $qty, $weight, $volume, $batch_id);
	if (!$cap['allowed'])
		return array('success' => false, 'error' => $cap['reason']);

	begin_transaction();

	// Remove from source bin
	update_bin_stock($from_bin_id, $stock_id, -$qty, $batch_id, $serial_id, null, 'available');

	// Add to destination bin
	update_bin_stock($to_bin_id, $stock_id, $qty, $batch_id, $serial_id, null, 'available');

	// Update serial location if applicable
	if ($serial_id) {
		$sql = "UPDATE " . TB_PREF . "serial_numbers SET wh_loc_id=" . (int)$to_bin_id
			. " WHERE id=" . (int)$serial_id;
		db_query($sql);
		add_serial_movement($serial_id, ST_LOCTRANSFER, 0,
			$loc_code, $loc_code, 'available', 'available',
			date('Y-m-d'), 'Mobile transfer', 'Bin-to-bin transfer via mobile');
	}

	commit_transaction();

	$from_bin = get_warehouse_location($from_bin_id);
	$to_bin = get_warehouse_location($to_bin_id);

	return array(
		'success' => true,
		'message' => sprintf(_('%s x %s moved from %s to %s'),
			$stock_id, number_format2($qty, get_qty_dec($stock_id)),
			$from_bin ? $from_bin['loc_code'] : $from_bin_id,
			$to_bin ? $to_bin['loc_code'] : $to_bin_id),
	);
}

/**
 * Submit a cycle count line.
 *
 * @return array JSON response
 */
function mobile_count_line() {
	$count_id = isset($_POST['count_id']) ? (int)$_POST['count_id'] : 0;
	$stock_id = isset($_POST['stock_id']) ? trim($_POST['stock_id']) : '';
	$bin_loc_id = isset($_POST['bin_loc_id']) ? (int)$_POST['bin_loc_id'] : 0;
	$counted_qty = isset($_POST['counted_qty']) ? (float)$_POST['counted_qty'] : 0;
	$batch_no = isset($_POST['batch_no']) ? trim($_POST['batch_no']) : '';
	$serial_no = isset($_POST['serial_no']) ? trim($_POST['serial_no']) : '';

	if ($count_id <= 0 || empty($stock_id))
		return array('success' => false, 'error' => _('Count session and item are required'));

	$batch_id = null;
	$serial_id = null;

	if (!empty($batch_no)) {
		$batch = get_stock_batch_by_code($batch_no, $stock_id);
		if ($batch) $batch_id = $batch['id'];
	}

	if (!empty($serial_no)) {
		$serial = get_serial_number_by_code($serial_no, $stock_id);
		if ($serial) $serial_id = $serial['id'];
	}

	// Get system qty from bin stock for variance calculation
	$system_qty = 0;
	if ($bin_loc_id > 0) {
		$sql = "SELECT COALESCE(SUM(qty_on_hand), 0) FROM " . TB_PREF . "wh_bin_stock"
			. " WHERE wh_loc_id=" . (int)$bin_loc_id
			. " AND stock_id=" . db_escape($stock_id);
		if ($batch_id)
			$sql .= " AND batch_id=" . (int)$batch_id;
		if ($serial_id)
			$sql .= " AND serial_id=" . (int)$serial_id;
		$system_qty = (float)db_query($sql)->fetch_row()[0];
	}

	$variance = $counted_qty - $system_qty;

	if (function_exists('add_cycle_count_line')) {
		add_cycle_count_line($count_id, $stock_id, $bin_loc_id, $system_qty, $counted_qty,
			$batch_id, $serial_id);
	}

	return array(
		'success' => true,
		'message' => sprintf(_('Counted %s: %s (system: %s, variance: %s)'),
			$stock_id,
			number_format2($counted_qty, get_qty_dec($stock_id)),
			number_format2($system_qty, get_qty_dec($stock_id)),
			number_format2($variance, get_qty_dec($stock_id))),
		'system_qty' => $system_qty,
		'counted_qty' => $counted_qty,
		'variance' => $variance,
	);
}

/**
 * Quick serial number lookup — returns full serial detail.
 *
 * @return array JSON response
 */
function mobile_serial_lookup() {
	$serial_no = isset($_POST['serial_no']) ? trim($_POST['serial_no']) : '';
	$scan = isset($_POST['scan']) ? trim($_POST['scan']) : '';

	$search = !empty($serial_no) ? $serial_no : $scan;
	if (empty($search))
		return array('success' => false, 'error' => _('Serial number is required'));

	$serial = get_serial_number_by_code($search);
	if (!$serial)
		return array('success' => false, 'error' => sprintf(_('Serial "%s" not found'), $search));

	// Get item info
	$item = get_item($serial['stock_id']);

	// Get location name
	$loc_name = '';
	if ($serial['wh_loc_id']) {
		$loc = get_warehouse_location($serial['wh_loc_id']);
		if ($loc)
			$loc_name = $loc['loc_code'] . ' — ' . $loc['loc_name'];
	}

	// Get recent movements
	$movements = array();
	$movs = get_serial_movements($serial['id'], 0, 5);
	while ($mov = db_fetch($movs)) {
		$movements[] = array(
			'date' => $mov['tran_date'],
			'type' => $mov['movement_type'],
			'from_status' => $mov['from_status'],
			'to_status' => $mov['to_status'],
			'from_loc' => $mov['from_loc'],
			'to_loc' => $mov['to_loc'],
			'reference' => $mov['reference'],
		);
	}

	return array(
		'success' => true,
		'serial' => array(
			'id' => $serial['id'],
			'serial_no' => $serial['serial_no'],
			'stock_id' => $serial['stock_id'],
			'item_description' => $item ? $item['description'] : '',
			'status' => $serial['status'],
			'loc_code' => $serial['loc_code'],
			'location' => $loc_name,
			'wh_loc_id' => $serial['wh_loc_id'],
			'batch_id' => $serial['batch_id'],
			'supplier_id' => $serial['supplier_id'],
			'customer_id' => $serial['customer_id'],
			'purchase_date' => $serial['purchase_date'],
			'manufacturing_date' => $serial['manufacturing_date'],
			'expiry_date' => $serial['expiry_date'],
			'warranty_start' => $serial['warranty_start'],
			'warranty_end' => $serial['warranty_end'],
			'purchase_cost' => $serial['purchase_cost'],
			'notes' => $serial['notes'],
		),
		'movements' => $movements,
	);
}

/**
 * Confirm putaway to specific bin.
 *
 * @return array JSON response
 */
function mobile_confirm_putaway() {
	$stock_id = isset($_POST['stock_id']) ? trim($_POST['stock_id']) : '';
	$qty = isset($_POST['qty']) ? (float)$_POST['qty'] : 0;
	$bin_loc_id = isset($_POST['bin_loc_id']) ? (int)$_POST['bin_loc_id'] : 0;
	$serial_no = isset($_POST['serial_no']) ? trim($_POST['serial_no']) : '';
	$batch_no = isset($_POST['batch_no']) ? trim($_POST['batch_no']) : '';
	$op_id = isset($_POST['op_id']) ? (int)$_POST['op_id'] : 0;
	$loc_code = isset($_POST['loc_code']) ? trim($_POST['loc_code']) : '';

	if (empty($stock_id) || $qty <= 0 || $bin_loc_id <= 0)
		return array('success' => false, 'error' => _('Item, quantity, and bin are required'));

	$serial_id = null;
	$batch_id = null;

	if (!empty($serial_no)) {
		$serial = get_serial_number_by_code($serial_no, $stock_id);
		if (!$serial)
			return array('success' => false, 'error' => sprintf(_('Serial "%s" not found'), $serial_no));
		$serial_id = $serial['id'];
	}

	if (!empty($batch_no)) {
		$batch = get_stock_batch_by_code($batch_no, $stock_id);
		if (!$batch)
			return array('success' => false, 'error' => sprintf(_('Batch "%s" not found'), $batch_no));
		$batch_id = $batch['id'];
	}

	// Capacity check
	$item = get_item($stock_id);
	$weight = $item ? (float)$item['item_weight'] * $qty : 0;
	$volume = $item ? (float)$item['item_volume'] * $qty : 0;
	$cap = check_can_store($bin_loc_id, $stock_id, $qty, $weight, $volume, $batch_id);
	if (!$cap['allowed'])
		return array('success' => false, 'error' => $cap['reason']);

	begin_transaction();

	update_bin_stock($bin_loc_id, $stock_id, $qty, $batch_id, $serial_id, null, 'available');

	if ($serial_id) {
		$sql = "UPDATE " . TB_PREF . "serial_numbers SET wh_loc_id=" . (int)$bin_loc_id
			. ", status='available'"
			. " WHERE id=" . (int)$serial_id;
		db_query($sql);
		add_serial_movement($serial_id, ST_INVADJUST, 0, '', $loc_code,
			'', 'available', date('Y-m-d'), 'Mobile putaway', 'Putaway via mobile scanner');
	}

	if ($op_id > 0)
		complete_wh_operation($op_id);

	commit_transaction();

	$bin = get_warehouse_location($bin_loc_id);
	return array(
		'success' => true,
		'message' => sprintf(_('%s x %s put away to %s'),
			$stock_id, number_format2($qty, get_qty_dec($stock_id)),
			$bin ? $bin['loc_code'] : $bin_loc_id),
	);
}

/**
 * Confirm pick from bin.
 *
 * @return array JSON response
 */
function mobile_confirm_pick() {
	$stock_id = isset($_POST['stock_id']) ? trim($_POST['stock_id']) : '';
	$qty = isset($_POST['qty']) ? (float)$_POST['qty'] : 0;
	$bin_loc_id = isset($_POST['bin_loc_id']) ? (int)$_POST['bin_loc_id'] : 0;
	$serial_no = isset($_POST['serial_no']) ? trim($_POST['serial_no']) : '';
	$batch_no = isset($_POST['batch_no']) ? trim($_POST['batch_no']) : '';
	$op_id = isset($_POST['op_id']) ? (int)$_POST['op_id'] : 0;

	if (empty($stock_id) || $qty <= 0 || $bin_loc_id <= 0)
		return array('success' => false, 'error' => _('Item, quantity, and bin are required'));

	$serial_id = null;
	$batch_id = null;

	if (!empty($serial_no)) {
		$serial = get_serial_number_by_code($serial_no, $stock_id);
		if (!$serial)
			return array('success' => false, 'error' => sprintf(_('Serial "%s" not found'), $serial_no));
		$serial_id = $serial['id'];
	}

	if (!empty($batch_no)) {
		$batch = get_stock_batch_by_code($batch_no, $stock_id);
		if (!$batch)
			return array('success' => false, 'error' => sprintf(_('Batch "%s" not found'), $batch_no));
		$batch_id = $batch['id'];
	}

	begin_transaction();

	// Remove from bin stock
	update_bin_stock($bin_loc_id, $stock_id, -$qty, $batch_id, $serial_id, null, 'available');

	if ($serial_id) {
		update_serial_status($serial_id, 'reserved', ST_CUSTDELIVERY, 0,
			'', '', date('Y-m-d'), 'Mobile pick', 'Picked via mobile scanner');
	}

	if ($op_id > 0)
		complete_wh_operation($op_id);

	commit_transaction();

	$bin = get_warehouse_location($bin_loc_id);
	return array(
		'success' => true,
		'message' => sprintf(_('%s x %s picked from %s'),
			$stock_id, number_format2($qty, get_qty_dec($stock_id)),
			$bin ? $bin['loc_code'] : $bin_loc_id),
	);
}

/**
 * Get pending receipt operations for a warehouse.
 *
 * @return array JSON response
 */
function mobile_get_pending_receive() {
	$loc_code = isset($_POST['loc_code']) ? trim($_POST['loc_code']) : '';
	if (empty($loc_code))
		return array('success' => false, 'error' => _('Location code is required'));

	$ops = get_wh_operations_by_type('receipt', $loc_code, 'pending');
	$items = array();
	while ($op = db_fetch($ops)) {
		$items[] = array(
			'op_id' => $op['op_id'],
			'op_type' => $op['op_type'],
			'status' => $op['op_status'],
			'source_doc_type' => $op['source_doc_type'],
			'source_doc_no' => $op['source_doc_no'],
			'scheduled_date' => $op['scheduled_date'],
			'notes' => $op['notes'],
		);
	}

	return array('success' => true, 'operations' => $items);
}

/**
 * Get pending pick operations.
 *
 * @return array JSON response
 */
function mobile_get_pending_picks() {
	$loc_code = isset($_POST['loc_code']) ? trim($_POST['loc_code']) : '';
	if (empty($loc_code))
		return array('success' => false, 'error' => _('Location code is required'));

	$ops = get_wh_operations_by_type('pick', $loc_code, 'pending');
	$items = array();
	while ($op = db_fetch($ops)) {
		$lines = get_wh_operation_lines($op['op_id']);
		$line_items = array();
		while ($line = db_fetch($lines)) {
			$line_items[] = array(
				'stock_id' => $line['stock_id'],
				'description' => $line['description'],
				'qty' => $line['qty'],
				'from_bin_id' => $line['from_bin_id'],
				'from_bin_code' => $line['from_bin_code'],
			);
		}
		$items[] = array(
			'op_id' => $op['op_id'],
			'status' => $op['op_status'],
			'source_doc_no' => $op['source_doc_no'],
			'lines' => $line_items,
		);
	}

	return array('success' => true, 'operations' => $items);
}

/**
 * Get pending cycle count sessions.
 *
 * @return array JSON response
 */
function mobile_get_pending_counts() {
	$loc_code = isset($_POST['loc_code']) ? trim($_POST['loc_code']) : '';

	if (!function_exists('get_cycle_counts'))
		return array('success' => false, 'error' => _('Cycle counting module not available'));

	$counts = get_cycle_counts($loc_code, 'in_progress');
	$items = array();
	while ($count = db_fetch($counts)) {
		$items[] = array(
			'count_id' => $count['count_id'],
			'count_no' => $count['count_no'],
			'count_date' => $count['count_date'],
			'loc_code' => $count['loc_code'],
			'status' => $count['status'],
			'notes' => $count['notes'],
		);
	}

	return array('success' => true, 'counts' => $items);
}

/**
 * Get bin contents.
 *
 * @return array JSON response
 */
function mobile_get_bin_contents() {
	$bin_loc_id = isset($_POST['bin_loc_id']) ? (int)$_POST['bin_loc_id'] : 0;
	$bin_code = isset($_POST['bin_code']) ? trim($_POST['bin_code']) : '';

	if ($bin_loc_id <= 0 && !empty($bin_code)) {
		$bin = get_warehouse_location_by_code($bin_code);
		if ($bin) $bin_loc_id = $bin['loc_id'];
	}

	if ($bin_loc_id <= 0)
		return array('success' => false, 'error' => _('Bin ID or code is required'));

	$sql = "SELECT bs.*, sm.description, sm.units, sm.track_by,"
		. " sn.serial_no, sb.batch_no, sb.expiry_date AS batch_expiry"
		. " FROM " . TB_PREF . "wh_bin_stock bs"
		. " LEFT JOIN " . TB_PREF . "stock_master sm ON bs.stock_id = sm.stock_id"
		. " LEFT JOIN " . TB_PREF . "serial_numbers sn ON bs.serial_id = sn.id"
		. " LEFT JOIN " . TB_PREF . "stock_batches sb ON bs.batch_id = sb.id"
		. " WHERE bs.wh_loc_id=" . (int)$bin_loc_id
		. " AND bs.qty_on_hand > 0"
		. " ORDER BY bs.stock_id, sb.expiry_date";

	$result = db_query($sql);
	$items = array();
	while ($row = db_fetch($result)) {
		$items[] = array(
			'stock_id' => $row['stock_id'],
			'description' => $row['description'],
			'qty_on_hand' => $row['qty_on_hand'],
			'qty_available' => $row['qty_available'],
			'qty_reserved' => $row['qty_reserved'],
			'serial_no' => $row['serial_no'],
			'batch_no' => $row['batch_no'],
			'batch_expiry' => $row['batch_expiry'],
			'stock_status' => $row['stock_status'],
		);
	}

	$bin = get_warehouse_location($bin_loc_id);
	return array(
		'success' => true,
		'bin' => $bin ? array(
			'loc_id' => $bin['loc_id'],
			'loc_code' => $bin['loc_code'],
			'loc_name' => $bin['loc_name'],
		) : null,
		'items' => $items,
	);
}
