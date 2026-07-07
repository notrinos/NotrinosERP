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

// Buffer any HTML error output from session/CSRF checks so it doesn't
// corrupt the JSON response body.
ob_start();

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

/**
 * Emit a clean JSON response, discarding any buffered page/session output.
 *
 * @param array       $response
 * @param string|null $status_header
 * @return void
 */
function mobile_emit_json($response, $status_header = null) {
	while (ob_get_level() > 0) {
		ob_end_clean();
	}

	if ($status_header !== null)
		header($status_header);

	global $messages;
	$messages = array();
	echo json_encode($response);
	exit();
}

$action = '';
if (isset($_POST['action']))
	$action = trim($_POST['action']);
elseif (isset($_GET['action']))
	$action = trim($_GET['action']);

if (empty($action)) {
	mobile_emit_json(array('success' => false, 'error' => 'No action specified'));
}

/**
 * Resolve required security areas for a given mobile AJAX action.
 *
 * @param string $action
 * @return array
 */
function mobile_get_action_security_areas($action) {
	$action_security_map = array(
		'scan_lookup' => array('SA_WAREHOUSE_OPERATIONS', 'SA_DISPATCH_OPERATIONS', 'SA_LOCATIONTRANSFER', 'SA_WAREHOUSE_CYCLE_COUNT', 'SA_WAREHOUSE_PICKING', 'SA_SERIALINQUIRY'),
		'confirm_receive' => array('SA_WAREHOUSE_OPERATIONS'),
		'confirm_ship' => array('SA_DISPATCH_OPERATIONS'),
		'confirm_transfer' => array('SA_LOCATIONTRANSFER'),
		'count_line' => array('SA_WAREHOUSE_CYCLE_COUNT'),
		'serial_lookup' => array('SA_SERIALINQUIRY'),
		'confirm_putaway' => array('SA_WAREHOUSE_OPERATIONS'),
		'confirm_pick' => array('SA_WAREHOUSE_PICKING'),
		'get_pending_receive' => array('SA_WAREHOUSE_OPERATIONS'),
		'get_pending_picks' => array('SA_WAREHOUSE_PICKING'),
		'get_pending_counts' => array('SA_WAREHOUSE_CYCLE_COUNT'),
		'get_bin_contents' => array('SA_WAREHOUSE_OPERATIONS', 'SA_LOCATIONTRANSFER', 'SA_WAREHOUSE_PICKING', 'SA_WAREHOUSE_CYCLE_COUNT'),
	);

	return isset($action_security_map[$action]) ? $action_security_map[$action] : array();
}

/**
 * Check whether current user has access to at least one security area.
 *
 * @param array $security_areas
 * @return bool
 */
function mobile_user_has_any_access($security_areas) {
	if (empty($security_areas))
		return false;

	foreach ($security_areas as $security_area) {
		if (user_check_access($security_area))
			return true;
	}

	return false;
}

$required_security_areas = mobile_get_action_security_areas($action);
if (!mobile_user_has_any_access($required_security_areas)) {
	mobile_emit_json(array(
		'success' => false,
		'error' => _('Access denied for this mobile action')
	), 'HTTP/1.1 403 Forbidden');
}

$response = array('success' => false, 'error' => 'Unknown action');

try {
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
} catch (Throwable $e) {
	$response = array('success' => false, 'error' => $e->getMessage());
}

mobile_emit_json($response);

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
				$item = stock_master_entity::find($serial['stock_id']);
				$result['matches'][] = array(
					'type' => 'serial',
					'serial_id' => $serial['id'],
					'serial_no' => $serial['serial_no'],
					'stock_id' => $serial['stock_id'],
					'description' => $item ? $item['description'] : '',
					'track_by' => $item ? $item['track_by'] : 'serial',
					'status' => $serial['status'],
					'loc_code' => $serial['loc_code'],
					'wh_loc_id' => $serial['wh_loc_id'],
				);
			}
		}
		if (isset($gs1['10'])) {
			$batch = get_stock_batch_by_code($gs1['10']);
			if ($batch) {
				$item = stock_master_entity::find($batch['stock_id']);
				$result['matches'][] = array(
					'type' => 'batch',
					'batch_id' => $batch['id'],
					'batch_no' => $batch['batch_no'],
					'stock_id' => $batch['stock_id'],
					'description' => $item ? $item['description'] : '',
					'track_by' => $item ? $item['track_by'] : 'batch',
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
		$item = stock_master_entity::find($serial['stock_id']);
		$result['matches'][] = array(
			'type' => 'serial',
			'serial_id' => $serial['id'],
			'serial_no' => $serial['serial_no'],
			'stock_id' => $serial['stock_id'],
			'description' => $item ? $item['description'] : '',
			'track_by' => $item ? $item['track_by'] : 'serial',
			'status' => $serial['status'],
			'loc_code' => $serial['loc_code'],
			'wh_loc_id' => $serial['wh_loc_id'],
		);
		return $result;
	}

	// 3. Try batch number exact match
	$batch = get_stock_batch_by_code($scan);
	if ($batch) {
		$item = stock_master_entity::find($batch['stock_id']);
		$result['matches'][] = array(
			'type' => 'batch',
			'batch_id' => $batch['id'],
			'batch_no' => $batch['batch_no'],
			'stock_id' => $batch['stock_id'],
			'description' => $item ? $item['description'] : '',
			'track_by' => $item ? $item['track_by'] : 'batch',
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
	$item = stock_master_entity::find($scan);
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
 * Check whether a scalar value contains ASCII control characters.
 *
 * @param mixed $value
 * @return bool
 */
function mobile_has_control_chars($value) {
	if (!is_scalar($value))
		return true;

	return preg_match('/[\x00-\x1F\x7F]/', (string)$value) === 1;
}

/**
 * Validate positive numeric input and reject non-finite values.
 *
 * @param mixed $value
 * @param float|null $normalized_value
 * @return bool
 */
function mobile_parse_positive_numeric($value, &$normalized_value) {
	$normalized_value = null;

	if (!is_scalar($value))
		return false;

	$trimmed = trim((string)$value);
	if ($trimmed === '' || !is_numeric($trimmed))
		return false;

	$parsed = (float)$trimmed;
	if (is_nan($parsed) || is_infinite($parsed) || $parsed <= 0)
		return false;

	$normalized_value = $parsed;
	return true;
}

/**
 * Validate stock item code and load active item master row.
 *
 * @param mixed $stock_id_raw
 * @param array|null $item_row
 * @return array
 */
function mobile_validate_stock_item($stock_id_raw, &$item_row) {
	$item_row = null;

	if (!is_scalar($stock_id_raw))
		return array('success' => false, 'error' => _('Invalid item code'));

	$stock_id = trim((string)$stock_id_raw);
	if ($stock_id === '')
		return array('success' => false, 'error' => _('Item is required'));

	if (mobile_has_control_chars($stock_id))
		return array('success' => false, 'error' => _('Invalid item code format'));

	$item = stock_master_entity::find($stock_id);
	if (!$item || (isset($item['inactive']) && (int)$item['inactive'] !== 0))
		return array('success' => false, 'error' => sprintf(_('Item "%s" not found'), $stock_id));

	$item_row = $item;
	return array('success' => true, 'stock_id' => $stock_id);
}

/**
 * Validate a mobile action's operation linkage before completion.
 *
 * @param int   $op_id             Operation id from payload.
 * @param array $allowed_op_types  Allowed operation types for the action.
 * @return array Validation result array with success/error.
 */
function mobile_validate_operation_for_completion($op_id, $allowed_op_types) {
	if ($op_id <= 0)
		return array('success' => true, 'operation' => null);

	$operation = get_wh_operation($op_id);
	if (!$operation)
		return array('success' => false, 'error' => _('Operation not found'));

	if (!in_array($operation['op_type'], $allowed_op_types)) {
		return array(
			'success' => false,
			'error' => sprintf(
				_('Operation #%d type %s is invalid for this action.'),
				$op_id,
				$operation['op_type']
			)
		);
	}

	if (!in_array($operation['op_status'], array('draft', 'ready', 'in_progress'))) {
		return array(
			'success' => false,
			'error' => sprintf(
				_('Operation #%d is already %s and cannot be completed again.'),
				$op_id,
				$operation['op_status']
			)
		);
	}

	return array('success' => true, 'operation' => $operation);
}

/**
 * Apply a mobile putaway confirmation to matching planned operation lines.
 *
 * @param int      $op_id       Operation id.
 * @param string   $stock_id    Item code.
 * @param int      $bin_loc_id  Destination bin id.
 * @param float    $qty         Confirmed quantity.
 * @param int|null $batch_id    Batch id.
 * @param int|null $serial_id   Serial id.
 * @return array
 */
function mobile_complete_putaway_operation_lines($op_id, $stock_id, $bin_loc_id, $qty, $batch_id = null, $serial_id = null) {
	if ($op_id <= 0)
		return array('success' => true, 'completed' => false);

	$where = "op_id=" . (int)$op_id
		. " AND stock_id=" . db_escape($stock_id)
		. " AND to_loc_id=" . (int)$bin_loc_id;

	if ($serial_id !== null)
		$where .= " AND serial_id=" . (int)$serial_id;
	elseif ($batch_id !== null)
		$where .= " AND batch_id=" . (int)$batch_id;

	$sql = "SELECT line_id, qty_planned, qty_done FROM " . TB_PREF . "wh_operation_lines "
		. "WHERE " . $where . " ORDER BY line_id";
	$result = db_query($sql, 'could not get putaway operation lines for mobile confirmation');
	$lines = array();
	$available = 0;
	while ($line = db_fetch($result)) {
		$remaining_line_qty = (float)$line['qty_planned'] - (float)$line['qty_done'];
		if ($remaining_line_qty <= 0)
			continue;
		$line['remaining_qty'] = $remaining_line_qty;
		$available += $remaining_line_qty;
		$lines[] = $line;
	}

	if ($available + 0.000001 < (float)$qty) {
		return array(
			'success' => false,
			'error' => _('Confirmed quantity exceeds the remaining planned putaway quantity for this item and bin.'),
		);
	}

	$remaining = (float)$qty;
	foreach ($lines as $line) {
		if ($remaining <= 0)
			break;

		$done_now = min($remaining, (float)$line['remaining_qty']);
		update_wh_operation_line_done($line['line_id'], (float)$line['qty_done'] + $done_now);
		$remaining -= $done_now;
	}

	$totals_sql = "SELECT COUNT(*) AS line_count, "
		. "SUM(CASE WHEN qty_done + 0.000001 >= qty_planned THEN 0 ELSE 1 END) AS open_lines "
		. "FROM " . TB_PREF . "wh_operation_lines WHERE op_id=" . (int)$op_id;
	$totals_result = db_query($totals_sql, 'could not summarize putaway operation completion');
	$totals = db_fetch($totals_result);
	$completed = $totals && (int)$totals['line_count'] > 0 && (int)$totals['open_lines'] === 0;

	update_wh_operation_status($op_id, $completed ? 'done' : 'in_progress');
	return array('success' => true, 'completed' => $completed);
}

/**
 * Validate destination/source bin and ensure the location is active and storable.
 *
 * @param mixed $bin_loc_id_raw
 * @param int|null $bin_loc_id
 * @return array
 */
function mobile_validate_storable_bin($bin_loc_id_raw, &$bin_loc_id) {
	$bin_loc_id = 0;

	if (!is_scalar($bin_loc_id_raw))
		return array('success' => false, 'error' => _('Invalid bin value'));

	$parsed_bin = (int)$bin_loc_id_raw;
	if ($parsed_bin <= 0)
		return array('success' => false, 'error' => _('Bin is required'));

	$sql = 'SELECT wl.loc_id, wl.loc_code FROM ' . TB_PREF . 'wh_locations wl '
		. 'INNER JOIN ' . TB_PREF . 'wh_location_types lt ON wl.location_type_id=lt.id '
		. 'WHERE wl.loc_id=' . $parsed_bin . ' AND wl.is_active=1 AND lt.can_store=1 LIMIT 1';
	$result = db_query($sql, 'could not validate bin location');
	$bin = db_fetch($result);
	if (!$bin)
		return array('success' => false, 'error' => _('Bin not found or inactive'));

	$bin_loc_id = (int)$bin['loc_id'];
	return array('success' => true, 'bin' => $bin);
}

/**
 * Check whether a tracking mode requires serial capture.
 *
 * @param string $track_by Item tracking mode.
 * @return bool
 */
function mobile_item_has_serial_tracking($track_by) {
	return in_array($track_by, array('serial', 'serial_batch', 'both'));
}

/**
 * Check whether a tracking mode requires batch capture.
 *
 * @param string $track_by Item tracking mode.
 * @return bool
 */
function mobile_item_has_batch_tracking($track_by) {
	return in_array($track_by, array('batch', 'serial_batch', 'both'));
}

/**
 * Validate tracked input requirements for mobile warehouse actions.
 *
 * @param array  $stock_item   Active item master row.
 * @param float  $qty          Requested quantity.
 * @param string $serial_no    Serial number input.
 * @param string $batch_no     Batch number input.
 * @param string $action_label Action label for error text.
 * @return array
 */
function mobile_validate_tracking_payload($stock_item, $qty, $serial_no, $batch_no, $action_label) {
	$track_by = isset($stock_item['track_by']) && is_scalar($stock_item['track_by'])
		? trim((string)$stock_item['track_by']) : 'none';
	$stock_id = isset($stock_item['stock_id']) ? $stock_item['stock_id'] : '';
	$requires_serial = mobile_item_has_serial_tracking($track_by);
	$requires_batch = mobile_item_has_batch_tracking($track_by);

	if ($requires_serial && abs((float)$qty - 1.0) > 0.000001) {
		return array(
			'success' => false,
			'error' => sprintf(_('Serial-tracked %s requires quantity 1'), $action_label),
		);
	}

	if ($requires_serial && $serial_no === '') {
		return array(
			'success' => false,
			'error' => sprintf(_('Serial number is required for item "%s".'), $stock_id),
		);
	}

	if ($requires_batch && $batch_no === '') {
		return array(
			'success' => false,
			'error' => sprintf(_('Batch number is required for item "%s".'), $stock_id),
		);
	}

	return array(
		'success' => true,
		'track_by' => $track_by,
		'requires_serial' => $requires_serial,
		'requires_batch' => $requires_batch,
	);
}

/**
 * Confirm receipt of item into a bin (part of inbound flow).
 *
 * @return array JSON response
 */
function mobile_confirm_receive() {
	$op_id = isset($_POST['op_id']) ? (int)$_POST['op_id'] : 0;
	$loc_code = isset($_POST['loc_code']) && is_scalar($_POST['loc_code']) ? trim((string)$_POST['loc_code']) : '';
	$serial_no = isset($_POST['serial_no']) && is_scalar($_POST['serial_no']) ? trim((string)$_POST['serial_no']) : '';
	$batch_no = isset($_POST['batch_no']) && is_scalar($_POST['batch_no']) ? trim((string)$_POST['batch_no']) : '';

	$qty = null;
	if (!mobile_parse_positive_numeric(isset($_POST['qty']) ? $_POST['qty'] : null, $qty))
		return array('success' => false, 'error' => _('Quantity must be a positive finite number'));

	$stock_item = null;
	$stock_validation = mobile_validate_stock_item(isset($_POST['stock_id']) ? $_POST['stock_id'] : '', $stock_item);
	if (!$stock_validation['success'])
		return $stock_validation;
	$stock_id = $stock_validation['stock_id'];

	$bin_loc_id = 0;
	$bin_validation = mobile_validate_storable_bin(isset($_POST['bin_loc_id']) ? $_POST['bin_loc_id'] : 0, $bin_loc_id);
	if (!$bin_validation['success'])
		return $bin_validation;

	if (mobile_has_control_chars($loc_code) || mobile_has_control_chars($serial_no) || mobile_has_control_chars($batch_no))
		return array('success' => false, 'error' => _('Input contains unsupported control characters'));

	$operation_validation = mobile_validate_operation_for_completion($op_id, array('receipt'));
	if (!$operation_validation['success'])
		return $operation_validation;

	$tracking_validation = mobile_validate_tracking_payload($stock_item, $qty, $serial_no, $batch_no, 'receipt');
	if (!$tracking_validation['success'])
		return $tracking_validation;

	$serial_id = null;
	$batch_id = null;
	$batch = null;

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

	$received_stock_status = !empty($stock_item['quality_inspection_required']) ? 'quarantine' : 'available';
	$serial_reconcile = array('success' => true, 'already_in_target' => false, 'moved_from_bin_ids' => array());
	if ($serial_id) {
		$reconcile_error = '';
		$serial_reconcile = reconcile_serial_bin_stock_location(
			$stock_id,
			$serial_id,
			$bin_loc_id,
			false,
			$serial_no,
			$reconcile_error
		);
		if (!$serial_reconcile['success'])
			return array('success' => false, 'error' => $reconcile_error);
	}

	// Capacity check if bin specified
	if ($bin_loc_id > 0) {
		$weight = $stock_item ? (float)$stock_item['item_weight'] * $qty : 0;
		$volume = $stock_item ? (float)$stock_item['item_volume'] * $qty : 0;
		$cap = check_can_store($bin_loc_id, $stock_id, $qty, $weight, $volume, $batch_id);
		if (!$cap['ok'])
			return array('success' => false, 'error' => implode(' ', $cap['errors']));
	}

	begin_transaction();

	// Update bin stock
	if ($bin_loc_id > 0) {
		$bin_qty = ($serial_id && !empty($serial_reconcile['already_in_target'])) ? 0 : $qty;
		update_bin_stock($bin_loc_id, $stock_id, $bin_qty, $batch_id, $serial_id, null, $received_stock_status);
	}

	// Update serial location if applicable
	if ($serial_id) {
		$serial_state_changed = !empty($serial_reconcile['moved_from_bin_ids'])
			|| empty($serial_reconcile['already_in_target'])
			|| $serial['status'] !== $received_stock_status
			|| (int)$serial['wh_loc_id'] !== $bin_loc_id
			|| $serial['loc_code'] !== $loc_code;

		$sql = "UPDATE " . TB_PREF . "serial_numbers SET status='available', loc_code="
			. db_escape($loc_code) . ", wh_loc_id=" . (int)$bin_loc_id
			. " WHERE id=" . (int)$serial_id;
		$sql = str_replace("status='available'", "status=" . db_escape($received_stock_status), $sql);
		db_query($sql);
		if ($serial_state_changed) {
			add_serial_movement($serial_id, ST_SUPPRECEIVE, 0, $serial['loc_code'], $loc_code,
				$serial['status'], $received_stock_status, Today(), 'Mobile receive', 'Received via mobile scanner');
		}
	}

	if ($batch_id && $batch) {
		$desired_batch_status = $received_stock_status === 'quarantine' ? 'quarantine' : 'active';
		if (!isset($batch['status']) || $batch['status'] !== $desired_batch_status)
			update_batch_status($batch_id, $desired_batch_status, 'Mobile receive');
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

	$operation_validation = mobile_validate_operation_for_completion($op_id, array('ship'));
	if (!$operation_validation['success'])
		return $operation_validation;

	$serial = get_serial_number_by_code($serial_no, $stock_id);
	if (!$serial)
		return array('success' => false, 'error' => sprintf(_('Serial "%s" not found for item %s'), $serial_no, $stock_id));

	if ($serial['status'] !== 'available' && $serial['status'] !== 'reserved')
		return array('success' => false, 'error' => sprintf(_('Serial "%s" status is %s — cannot ship'), $serial_no, $serial['status']));

	begin_transaction();

	// Mark serial as delivered
	update_serial_status($serial['id'], 'delivered', ST_CUSTDELIVERY, 0,
		$serial['loc_code'], '', Today(), 'Mobile ship', 'Shipped via mobile scanner');

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
	$stock_id = isset($_POST['stock_id']) && is_scalar($_POST['stock_id']) ? trim((string)$_POST['stock_id']) : '';
	$qty = null;
	if (!mobile_parse_positive_numeric(isset($_POST['qty']) ? $_POST['qty'] : null, $qty))
		return array('success' => false, 'error' => _('Quantity must be a positive finite number'));
	$from_bin_id = isset($_POST['from_bin_id']) ? (int)$_POST['from_bin_id'] : 0;
	$to_bin_id = isset($_POST['to_bin_id']) ? (int)$_POST['to_bin_id'] : 0;
	$serial_no = isset($_POST['serial_no']) && is_scalar($_POST['serial_no']) ? trim((string)$_POST['serial_no']) : '';
	$batch_no = isset($_POST['batch_no']) && is_scalar($_POST['batch_no']) ? trim((string)$_POST['batch_no']) : '';
	$loc_code = isset($_POST['loc_code']) && is_scalar($_POST['loc_code']) ? trim((string)$_POST['loc_code']) : '';

	if (empty($stock_id) || $qty <= 0 || $from_bin_id <= 0 || $to_bin_id <= 0)
		return array('success' => false, 'error' => _('Item, quantity, source bin, and destination bin are required'));

	if (mobile_has_control_chars($stock_id) || mobile_has_control_chars($serial_no) || mobile_has_control_chars($batch_no) || mobile_has_control_chars($loc_code))
		return array('success' => false, 'error' => _('Input contains unsupported control characters'));

	if ($from_bin_id === $to_bin_id)
		return array('success' => false, 'error' => _('Source and destination bins cannot be the same'));

	$item = stock_master_entity::find($stock_id);
	if (!$item || (isset($item['inactive']) && (int)$item['inactive'] !== 0))
		return array('success' => false, 'error' => sprintf(_('Item "%s" not found'), $stock_id));

	$tracking_validation = mobile_validate_tracking_payload($item, $qty, $serial_no, $batch_no, 'transfer');
	if (!$tracking_validation['success'])
		return $tracking_validation;

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
	$weight = $item ? (float)$item['item_weight'] * $qty : 0;
	$volume = $item ? (float)$item['item_volume'] * $qty : 0;
	$cap = check_can_store($to_bin_id, $stock_id, $qty, $weight, $volume, $batch_id);
	if (!$cap['ok'])
		return array('success' => false, 'error' => implode(' ', $cap['errors']));

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
			Today(), 'Mobile transfer', 'Bin-to-bin transfer via mobile');
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
		$_res = db_query($sql);
		$_row = db_fetch_row($_res);
		$system_qty = $_row ? (float)$_row[0] : 0.0;
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
	$item = stock_master_entity::find($serial['stock_id']);

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
	$serial_no = isset($_POST['serial_no']) && is_scalar($_POST['serial_no']) ? trim((string)$_POST['serial_no']) : '';
	$batch_no = isset($_POST['batch_no']) && is_scalar($_POST['batch_no']) ? trim((string)$_POST['batch_no']) : '';
	$op_id = isset($_POST['op_id']) ? (int)$_POST['op_id'] : 0;
	$loc_code = isset($_POST['loc_code']) && is_scalar($_POST['loc_code']) ? trim((string)$_POST['loc_code']) : '';

	$qty = null;
	if (!mobile_parse_positive_numeric(isset($_POST['qty']) ? $_POST['qty'] : null, $qty))
		return array('success' => false, 'error' => _('Quantity must be a positive finite number'));

	$stock_item = null;
	$stock_validation = mobile_validate_stock_item(isset($_POST['stock_id']) ? $_POST['stock_id'] : '', $stock_item);
	if (!$stock_validation['success'])
		return $stock_validation;
	$stock_id = $stock_validation['stock_id'];

	$bin_loc_id = 0;
	$bin_validation = mobile_validate_storable_bin(isset($_POST['bin_loc_id']) ? $_POST['bin_loc_id'] : 0, $bin_loc_id);
	if (!$bin_validation['success'])
		return $bin_validation;

	if (mobile_has_control_chars($loc_code) || mobile_has_control_chars($serial_no) || mobile_has_control_chars($batch_no))
		return array('success' => false, 'error' => _('Input contains unsupported control characters'));

	$operation_validation = mobile_validate_operation_for_completion($op_id, array('putaway'));
	if (!$operation_validation['success'])
		return $operation_validation;

	$tracking_validation = mobile_validate_tracking_payload($stock_item, $qty, $serial_no, $batch_no, 'putaway');
	if (!$tracking_validation['success'])
		return $tracking_validation;

	$serial_id = null;
	$batch_id = null;
	$batch = null;

	if (!empty($serial_no)) {
		$serial = get_serial_number_by_code($serial_no, $stock_id);
		if (!$serial)
			return array('success' => false, 'error' => sprintf(_('Serial "%s" not found'), $serial_no));
		if (isset($serial['status']) && $serial['status'] === 'quarantine') {
			return array(
				'success' => false,
				'error' => sprintf(_('Serial "%s" is in quarantine. Complete quality inspection before putaway.'), $serial_no),
			);
		}
		$serial_id = $serial['id'];
	}

	if (!empty($batch_no)) {
		$batch = get_stock_batch_by_code($batch_no, $stock_id);
		if (!$batch)
			return array('success' => false, 'error' => sprintf(_('Batch "%s" not found'), $batch_no));
		if (isset($batch['status']) && $batch['status'] === 'quarantine') {
			return array(
				'success' => false,
				'error' => sprintf(_('Batch "%s" is in quarantine. Complete quality inspection before putaway.'), $batch_no),
			);
		}
		$batch_id = $batch['id'];
	}

	$serial_reconcile = array('success' => true, 'already_in_target' => false, 'moved_from_bin_ids' => array());
	if ($serial_id) {
		$reconcile_error = '';
		$serial_reconcile = reconcile_serial_bin_stock_location(
			$stock_id,
			$serial_id,
			$bin_loc_id,
			true,
			$serial_no,
			$reconcile_error
		);
		if (!$serial_reconcile['success'])
			return array('success' => false, 'error' => $reconcile_error);
	}

	// Capacity check
	$weight = $stock_item ? (float)$stock_item['item_weight'] * $qty : 0;
	$volume = $stock_item ? (float)$stock_item['item_volume'] * $qty : 0;
	$cap = check_can_store($bin_loc_id, $stock_id, $qty, $weight, $volume, $batch_id);
	if (!$cap['ok'])
		return array('success' => false, 'error' => implode(' ', $cap['errors']));

	begin_transaction();

	$line_completion = mobile_complete_putaway_operation_lines($op_id, $stock_id, $bin_loc_id, $qty, $batch_id, $serial_id);
	if (!$line_completion['success']) {
		cancel_transaction();
		return $line_completion;
	}

	$bin_qty = ($serial_id && !empty($serial_reconcile['already_in_target'])) ? 0 : $qty;
	update_bin_stock($bin_loc_id, $stock_id, $bin_qty, $batch_id, $serial_id, null, 'available');

	if ($serial_id) {
		$serial_state_changed = !empty($serial_reconcile['moved_from_bin_ids'])
			|| empty($serial_reconcile['already_in_target'])
			|| $serial['status'] !== 'available'
			|| (int)$serial['wh_loc_id'] !== $bin_loc_id
			|| $serial['loc_code'] !== $loc_code;

		$sql = "UPDATE " . TB_PREF . "serial_numbers SET wh_loc_id=" . (int)$bin_loc_id
			. ", loc_code=" . db_escape($loc_code)
			. ", status='available'"
			. " WHERE id=" . (int)$serial_id;
		db_query($sql);
		if ($serial_state_changed) {
			add_serial_movement($serial_id, ST_INVADJUST, 0, $serial['loc_code'], $loc_code,
				$serial['status'], 'available', Today(), 'Mobile putaway', 'Putaway via mobile scanner');
		}
	}

	if ($batch_id && $batch && isset($batch['status']) && $batch['status'] !== 'active') {
		update_batch_status($batch_id, 'active', 'Mobile putaway');
	}

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

	$operation_validation = mobile_validate_operation_for_completion($op_id, array('pick'));
	if (!$operation_validation['success'])
		return $operation_validation;

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
			'', '', Today(), 'Mobile pick', 'Picked via mobile scanner');
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
			'notes' => $op['memo'],
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
				'description' => $line['item_description'],
				'qty' => $line['qty_planned'],
				'from_bin_id' => $line['from_loc_id'],
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
