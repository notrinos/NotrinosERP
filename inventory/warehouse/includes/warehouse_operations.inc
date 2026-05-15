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
 * Warehouse Operations Orchestrator — Multi-step inbound/outbound flow management.
 *
 * Handles creating operation chains for inbound (receipt → QC → putaway)
 * and outbound (pick → pack → ship) flows, based on the warehouse's
 * configured step count (inbound_steps / outbound_steps in 0_locations).
 *
 * Dependencies:
 *   - warehouse_operations_db.inc (CRUD)
 *   - warehouse_locations_db.inc (location hierarchy, root location, etc.)
 *   - putaway_engine.inc (for putaway bin determination)
 *
 * @package NotrinosERP
 * @subpackage Inventory/Warehouse
 */

include_once(dirname(__FILE__) . '/db/warehouse_operations_db.inc');

// Load route/rule DB functions if available
$routes_db_path = dirname(__FILE__) . '/db/warehouse_routes_db.inc';
if (file_exists($routes_db_path))
	include_once($routes_db_path);

// Load cross-dock / drop-ship DB functions if available
$crossdock_db_path = dirname(__FILE__) . '/db/warehouse_crossdock_db.inc';
if (file_exists($crossdock_db_path))
	include_once($crossdock_db_path);

/**
 * Normalize one warehouse operation item payload and reject unsafe input shapes.
 *
 * @param mixed  $item          One item row from operation payload.
 * @param int    $item_index    Zero-based row index for error context.
 * @param string $error_message Validation error output.
 * @return array|false Normalized payload or false when validation fails.
 */
function normalize_wh_operation_item_payload($item, $item_index, &$error_message) {
	$error_message = '';

	if (!is_array($item)) {
		$error_message = sprintf(_('Inbound item row %d must be an array.'), $item_index + 1);
		return false;
	}

	if (!isset($item['stock_id']) || !is_scalar($item['stock_id'])) {
		$error_message = sprintf(_('Inbound item row %d has an invalid item code.'), $item_index + 1);
		return false;
	}

	$stock_id = trim((string)$item['stock_id']);
	if ($stock_id === '') {
		$error_message = sprintf(_('Inbound item row %d has an empty item code.'), $item_index + 1);
		return false;
	}

	if (preg_match('/[\x00-\x1F\x7F]/', $stock_id)) {
		$error_message = sprintf(_('Inbound item row %d has invalid control characters in item code.'), $item_index + 1);
		return false;
	}

	if (!is_valid_wh_operation_stock_id($stock_id)) {
		$error_message = sprintf(_('Inbound item %s does not exist or is inactive.'), $stock_id);
		return false;
	}

	if (!isset($item['qty']) || !is_scalar($item['qty']) || !is_valid_finite_numeric($item['qty'])) {
		$error_message = sprintf(_('Inbound item %s has an invalid quantity.'), $stock_id);
		return false;
	}

	$qty = (float)$item['qty'];
	if ($qty <= 0) {
		$error_message = sprintf(_('Inbound item %s must have quantity greater than zero.'), $stock_id);
		return false;
	}

	$std_cost = 0;
	if (isset($item['std_cost']) && $item['std_cost'] !== null && $item['std_cost'] !== '') {
		if (!is_scalar($item['std_cost']) || !is_valid_finite_numeric($item['std_cost'])) {
			$error_message = sprintf(_('Inbound item %s has an invalid standard cost.'), $stock_id);
			return false;
		}
		$std_cost = (float)$item['std_cost'];
	}

	$batch_id = null;
	if (isset($item['batch_id']) && $item['batch_id'] !== null && $item['batch_id'] !== '') {
		if (!is_scalar($item['batch_id']) || !is_numeric($item['batch_id']) || (int)$item['batch_id'] <= 0) {
			$error_message = sprintf(_('Inbound item %s has an invalid batch id.'), $stock_id);
			return false;
		}
		$batch_id = (int)$item['batch_id'];
	}

	$serial_ids = array();
	if (isset($item['serial_ids']) && $item['serial_ids'] !== null && $item['serial_ids'] !== '') {
		if (!is_array($item['serial_ids'])) {
			$error_message = sprintf(_('Inbound item %s has an invalid serial list payload.'), $stock_id);
			return false;
		}

		$seen_serial_ids = array();

		foreach ($item['serial_ids'] as $serial_id) {
			if (!is_scalar($serial_id) || !is_numeric($serial_id) || (int)$serial_id <= 0) {
				$error_message = sprintf(_('Inbound item %s contains an invalid serial id value.'), $stock_id);
				return false;
			}

			$serial_id_int = (int)$serial_id;
			if (isset($seen_serial_ids[$serial_id_int])) {
				$error_message = sprintf(_('Inbound item %s contains duplicate serial id %d.'), $stock_id, $serial_id_int);
				return false;
			}

			$seen_serial_ids[$serial_id_int] = 1;
			$serial_ids[] = $serial_id_int;
		}

		if (!empty($serial_ids) && abs($qty - count($serial_ids)) > 0.000001) {
			$error_message = sprintf(
				_('Inbound item %s quantity %.2f does not match serial count %d.'),
				$stock_id,
				$qty,
				count($serial_ids)
			);
			return false;
		}
	}

	return array(
		'stock_id' => $stock_id,
		'qty' => $qty,
		'std_cost' => $std_cost,
		'batch_id' => $batch_id,
		'serial_ids' => $serial_ids,
	);
}

/**
 * Check whether a numeric input is finite and safe for SQL numeric fields.
 *
 * @param mixed $value Raw numeric input.
 * @return bool
 */
function is_valid_finite_numeric($value) {
	if (!is_numeric($value))
		return false;

	$float_value = (float)$value;
	if (is_infinite($float_value) || is_nan($float_value))
		return false;

	return true;
}

/**
 * Validate that a stock item exists and is active for operation payload use.
 *
 * @param string $stock_id Item code.
 * @return bool
 */
function is_valid_wh_operation_stock_id($stock_id) {
	$sql = "SELECT stock_id FROM " . TB_PREF . "stock_master WHERE stock_id=" . db_escape($stock_id)
		. " AND inactive=0 LIMIT 1";
	$result = db_query($sql, 'could not validate stock item for warehouse operation payload');
	$row = db_fetch($result);

	return (bool)$row;
}

/**
 * Normalize a warehouse operation items payload.
 *
 * @param mixed  $items          Raw items payload.
 * @param string $error_message  Validation error output.
 * @return array|false Normalized list or false on validation failure.
 */
function normalize_wh_operation_items_payload($items, &$error_message) {
	$error_message = '';

	if (!is_array($items) || empty($items)) {
		$error_message = _('At least one valid inbound item is required to create operations.');
		return false;
	}

	$normalized_items = array();
	foreach ($items as $item_index => $item) {
		$normalized_item = normalize_wh_operation_item_payload($item, $item_index, $error_message);
		if ($normalized_item === false)
			return false;

		$normalized_items[] = $normalized_item;
	}

	return $normalized_items;
}

/**
 * Create operations from a resolved route's rules.
 *
 * Given a route and its push rules, creates a chained series of WMS operations
 * for the specified items. Each rule becomes one operation in the chain.
 *
 * @param array  $route            Route row from resolve_route()
 * @param int    $source_doc_type  Source document type (e.g. ST_SUPPRECEIVE)
 * @param int    $source_doc_no    Source document number (e.g. GRN#)
 * @param array  $items            Items array [{stock_id, qty, serial_ids, batch_id, std_cost}]
 * @param string $warehouse_loc_code Warehouse code for zone lookups
 * @return array Result with 'success', 'operations', 'message'
 */
function create_operations_from_route($route, $source_doc_type, $source_doc_no, $items, $warehouse_loc_code) {
	$result = array('success' => false, 'operations' => array(), 'message' => '');

	$normalized_items = normalize_wh_operation_items_payload($items, $payload_error_message);
	if ($normalized_items === false) {
		$result['message'] = $payload_error_message;
		return $result;
	}

	if (!$route || !function_exists('get_route_rules')) {
		$result['message'] = _('Route system not available.');
		return $result;
	}

	$rules = get_route_rules($route['route_id'], true);

	$rule_list = array();
	while ($rule = db_fetch($rules)) {
		$rule_list[] = $rule;
	}

	if (empty($rule_list)) {
		$result['message'] = _('Route has no active rules.');
		return $result;
	}

	$single_step_direct_receipt = (count($rule_list) === 1 && $rule_list[0]['operation_type'] === 'receipt');

	$op_ids = array();
	$prev_op_id = null;
	$now = date('Y-m-d H:i:s');

	foreach ($rule_list as $idx => $rule) {
		$scheduled = $now;
		if ($rule['delay_days'] > 0) {
			$scheduled = date('Y-m-d H:i:s', strtotime('+' . (int)$rule['delay_days'] . ' days'));
		}

		// Determine initial status based on trigger method and position in chain
		$status = 'draft';
		if ($idx === 0 && $rule['trigger_method'] === 'auto') {
			$status = 'ready';
		}

		// Resolve from/to locations — use rule-specified locations,
		// or fall back to auto-detection for putaway/pick operations
		$from_loc = $rule['from_loc_id'] ? (int)$rule['from_loc_id'] : null;
		$to_loc = $rule['to_loc_id'] ? (int)$rule['to_loc_id'] : null;

		$op_id = add_wh_operation(
			$rule['operation_type'],
			$status,
			$source_doc_type,
			$source_doc_no,
			$prev_op_id,
			$from_loc,
			$to_loc,
			$scheduled,
			null,
			5,
			$route['route_id'],
			sprintf(_('Route "%s" step %d: %s'), $route['route_name'], $idx + 1, $rule['operation_type'])
		);

		if ($op_id && $prev_op_id) {
			link_wh_operations($prev_op_id, $op_id);
		}

		// Add operation lines
		if ($op_id) {
			foreach ($normalized_items as $item) {
				$stock_id = $item['stock_id'];
				$qty = $item['qty'];
				$std_cost = isset($item['std_cost']) ? $item['std_cost'] : 0;
				$batch_id = isset($item['batch_id']) ? $item['batch_id'] : null;

				// For putaway operations, try to find target bin per item
				$line_to = $to_loc;
				if ($rule['operation_type'] === 'putaway' && !$to_loc) {
					$putaway_bin = get_putaway_bin_for_item($stock_id, $warehouse_loc_code, $qty);
					$line_to = $putaway_bin;
				} elseif ($rule['operation_type'] === 'receipt' && !$line_to) {
					$putaway_bin = get_putaway_bin_for_item($stock_id, $warehouse_loc_code, $qty);
					$line_to = $putaway_bin;
				}

				if ($single_step_direct_receipt && !$line_to) {
					$result['message'] = sprintf(
						_('No putaway bin found for item %s in warehouse %s.'),
						$stock_id,
						$warehouse_loc_code
					);
					return $result;
				}

				if (($rule['operation_type'] === 'putaway' || $single_step_direct_receipt)
					&& $line_to
					&& !is_valid_putaway_bin_location($line_to, $warehouse_loc_code)) {
					$result['message'] = sprintf(
						_('Putaway destination for item %s must be an active bin location in warehouse %s.'),
						$stock_id,
						$warehouse_loc_code
					);
					return $result;
				}

				if ($rule['operation_type'] === 'putaway' && !$line_to) {
					$result['message'] = sprintf(
						_('No putaway bin found for item %s in warehouse %s for route putaway step.'),
						$stock_id,
						$warehouse_loc_code
					);
					return $result;
				}

				if (!empty($item['serial_ids'])) {
					foreach ($item['serial_ids'] as $serial_id) {
						add_wh_operation_line($op_id, $stock_id, 1, 0,
							$from_loc, $line_to, $batch_id, $serial_id, null, $std_cost);

						if ($single_step_direct_receipt && $line_to) {
							update_bin_stock($line_to, $stock_id, 1, $batch_id, $serial_id, null, 'available');

							$sql = "UPDATE " . TB_PREF . "serial_numbers SET "
								. "wh_loc_id=" . (int)$line_to . ", "
								. "loc_code=" . db_escape($warehouse_loc_code) . ", "
								. "status='available' "
								. "WHERE id=" . (int)$serial_id;
							db_query($sql, 'could not update serial location from route receipt');
						}
					}
				} else {
					add_wh_operation_line($op_id, $stock_id, $qty, 0,
						$from_loc, $line_to, $batch_id, null, null, $std_cost);

					if ($single_step_direct_receipt && $line_to) {
						$expiry = null;
						if ($batch_id) {
							$batch_data = get_stock_batch($batch_id);
							if ($batch_data && !empty($batch_data['expiry_date']))
								$expiry = $batch_data['expiry_date'];
						}
						update_bin_stock($line_to, $stock_id, $qty, $batch_id, null, $expiry, 'available');
					}
				}
			}

			$op_ids[] = $op_id;
		}

		$prev_op_id = $op_id;
	}

	// If first operation is 'ready' and has only 1 step (like 1-step inbound), auto-complete it
	if (count($op_ids) === 1 && count($rule_list) === 1) {
		$first_rule = $rule_list[0];
		if ($first_rule['trigger_method'] === 'auto' && in_array($first_rule['operation_type'], array('receipt', 'ship'))) {
			complete_wh_operation($op_ids[0]);
		}
	}

	$result['success'] = true;
	$result['operations'] = $op_ids;
	$result['message'] = sprintf(_('%d operation(s) created from route "%s".'),
		count($op_ids), $route['route_name']);
	return $result;
}

/**
 * Validate whether a resolved inbound route is safe for GRN operation creation.
 *
 * @param array $route Resolved route row.
 * @return bool
 */
function is_valid_grn_inbound_route($route) {
	if (!$route || !function_exists('get_route_rules'))
		return false;

	$allowed_operation_types = array('receipt', 'quality_check', 'putaway');
	$rules = get_route_rules($route['route_id'], true);
	$rule_count = 0;

	while ($rule = db_fetch($rules)) {
		$rule_count++;
		if (!in_array($rule['operation_type'], $allowed_operation_types))
			return false;
	}

	return $rule_count > 0;
}

/**
 * Check if a location (0_locations.loc_code) is WMS-enabled.
 *
 * @param string $loc_code The 0_locations.loc_code
 * @return array|false Location row with wh fields, or false
 */
function get_wms_location_settings($loc_code) {
	$sql = "SELECT * FROM " . TB_PREF . "locations WHERE loc_code=" . db_escape($loc_code);
	$result = db_query($sql, 'could not get location');
	$row = db_fetch($result);
	if (!$row || !$row['wh_enabled'])
		return false;
	return $row;
}

/**
 * Create the inbound operation chain for a GRN.
 *
 * Based on the warehouse's `inbound_steps` setting:
 *   1-step: Receipt (into putaway bin directly — goods to stock)
 *   2-step: Receipt (to input zone) → Putaway (to stock bin)
 *   3-step: Receipt (to input zone) → Quality Check (to QC zone) → Putaway (to stock bin)
 *
 * Creates one operation chain per GRN (all lines share the same chain),
 * with individual operation lines per item (optionally per serial/batch).
 *
 * @param int    $grn_no         GRN number (source_doc_no)
 * @param string $loc_code       Inventory location code (0_locations.loc_code)
 * @param array  $items          Array of items, each: ['stock_id', 'qty', 'std_cost',
 *                               'serial_ids'=>[], 'batch_id'=>null]
 * @return array Result with 'success', 'operations' (array of op_ids in chain order),
 *               'message'
 */
function create_inbound_operations($grn_no, $loc_code, $items) {
	$result = array('success' => false, 'operations' => array(), 'message' => '');

	$normalized_items = normalize_wh_operation_items_payload($items, $payload_error_message);
	if ($normalized_items === false) {
		$result['message'] = $payload_error_message;
		return $result;
	}

	// Check if warehouse is WMS-enabled
	$wms = get_wms_location_settings($loc_code);
	if (!$wms) {
		$result['message'] = _('Location is not WMS-enabled. No operations created.');
		return $result;
	}

	// Try route-based operation creation first
	if (function_exists('resolve_route')) {
		$route = resolve_route($loc_code, 'inbound');
		if ($route) {
			if (is_valid_grn_inbound_route($route))
				return create_operations_from_route($route, ST_SUPPRECEIVE, $grn_no, $normalized_items, $loc_code);
		}
	}

	// Fallback: legacy step-based creation (inbound_steps setting)
	$inbound_steps = (int)$wms['inbound_steps'];
	if ($inbound_steps < 1) $inbound_steps = 1;
	if ($inbound_steps > 3) $inbound_steps = 3;

	// Load warehouse location helpers
	$inc_path = dirname(__FILE__) . '/db/warehouse_locations_db.inc';
	if (file_exists($inc_path))
		include_once($inc_path);

	// Get warehouse root location
	$root = null;
	if (function_exists('get_warehouse_root_location'))
		$root = get_warehouse_root_location($loc_code);

	if (!$root) {
		$result['message'] = _('No warehouse root location found. Enable WMS on this location first.');
		return $result;
	}

	$warehouse_loc_code = $loc_code;

	// Find special zones (input/QC) by type
	$input_zone_id = find_zone_by_type($warehouse_loc_code, 'staging');
	$qc_zone_id = find_zone_by_type($warehouse_loc_code, 'quality');

	if ($inbound_steps >= 2 && !$input_zone_id) {
		$result['message'] = sprintf(
			_('No active staging zone found for warehouse %s. Inbound operations cannot be created.'),
			$warehouse_loc_code
		);
		return $result;
	}

	if ($inbound_steps === 3 && !$qc_zone_id) {
		$result['message'] = sprintf(
			_('No active quality zone found for warehouse %s. Inbound operations cannot be created.'),
			$warehouse_loc_code
		);
		return $result;
	}

	$op_ids = array();
	$now = date('Y-m-d H:i:s');

	// Step 1: Always create Receipt operation
	$receipt_to = null;
	if ($inbound_steps === 1) {
		// 1-step: receipt destination is the final putaway bin (determined per item)
		$receipt_to = null; // Per-line destination
	} elseif ($inbound_steps >= 2) {
		// 2/3-step: receipt goes to input/staging zone
		$receipt_to = $input_zone_id;
	}

	$receipt_op = add_wh_operation(
		'receipt',
		'ready',
		ST_SUPPRECEIVE,
		$grn_no,
		null,        // no parent
		null,        // from_loc_id: external (no bin)
		$receipt_to, // to input zone or null
		$now,
		null,        // unassigned
		5,           // normal priority
		null,        // no route
		sprintf(_('GRN #%d receipt'), $grn_no)
	);
	$op_ids[] = $receipt_op;

	// Add lines to receipt operation
	foreach ($normalized_items as $item) {
		$stock_id = $item['stock_id'];
		$qty = $item['qty'];
		$std_cost = isset($item['std_cost']) ? $item['std_cost'] : 0;

		// For 1-step: try to find putaway bin per item
		$to_bin = $receipt_to;
		if ($inbound_steps === 1) {
			$putaway_bin = get_putaway_bin_for_item($stock_id, $warehouse_loc_code, $qty);
			$to_bin = $putaway_bin;
			if (!$to_bin || !is_valid_putaway_bin_location($to_bin, $warehouse_loc_code)) {
				$result['message'] = sprintf(
					_('No putaway bin found for item %s in warehouse %s.'),
					$stock_id,
					$warehouse_loc_code
				);
				return $result;
			}
		}

		// If serial-tracked, create one line per serial
		if (!empty($item['serial_ids'])) {
			foreach ($item['serial_ids'] as $serial_id) {
				add_wh_operation_line($receipt_op, $stock_id, 1, 0, null, $to_bin,
					isset($item['batch_id']) ? $item['batch_id'] : null,
					$serial_id, null, $std_cost);

				if ($inbound_steps === 1) {
					$batch_id = isset($item['batch_id']) ? $item['batch_id'] : null;
					update_bin_stock($to_bin, $stock_id, 1, $batch_id, $serial_id, null, 'available');

					$sql = "UPDATE " . TB_PREF . "serial_numbers SET "
						. "wh_loc_id=" . (int)$to_bin . ", "
						. "loc_code=" . db_escape($warehouse_loc_code) . ", "
						. "status='available' "
						. "WHERE id=" . (int)$serial_id;
					db_query($sql, 'could not update serial location from 1-step inbound');
				}
			}
		} else {
			// One line for the whole quantity
			add_wh_operation_line($receipt_op, $stock_id, $qty, 0, null, $to_bin,
				isset($item['batch_id']) ? $item['batch_id'] : null,
				null, null, $std_cost);

			if ($inbound_steps === 1) {
				$batch_id = isset($item['batch_id']) ? $item['batch_id'] : null;
				$expiry = null;
				if ($batch_id) {
					$batch_data = get_stock_batch($batch_id);
					if ($batch_data && !empty($batch_data['expiry_date']))
						$expiry = $batch_data['expiry_date'];
				}
				update_bin_stock($to_bin, $stock_id, $qty, $batch_id, null, $expiry, 'available');
			}
		}
	}

	// For 1-step: mark receipt as done immediately (goods go directly to stock)
	if ($inbound_steps === 1) {
		complete_wh_operation($receipt_op);
	}

	// Step 2: Quality Check (only for 3-step)
	$qc_op = null;
	if ($inbound_steps === 3) {
		$qc_op = add_wh_operation(
			'quality_check',
			'draft',
			ST_SUPPRECEIVE,
			$grn_no,
			$receipt_op, // parent
			$input_zone_id,  // from input zone
			$qc_zone_id,     // to QC zone
			null,
			null,
			5,
			null,
			sprintf(_('GRN #%d quality check'), $grn_no)
		);
		$op_ids[] = $qc_op;
		link_wh_operations($receipt_op, $qc_op);

		// Add lines (same items)
		foreach ($normalized_items as $item) {
			if (!empty($item['serial_ids'])) {
				foreach ($item['serial_ids'] as $serial_id) {
					add_wh_operation_line($qc_op, $item['stock_id'], 1, 0,
						$input_zone_id, $qc_zone_id,
						isset($item['batch_id']) ? $item['batch_id'] : null,
						$serial_id, null, isset($item['std_cost']) ? $item['std_cost'] : 0);
				}
			} else {
				add_wh_operation_line($qc_op, $item['stock_id'], $item['qty'], 0,
					$input_zone_id, $qc_zone_id,
					isset($item['batch_id']) ? $item['batch_id'] : null,
					null, null, isset($item['std_cost']) ? $item['std_cost'] : 0);
			}
		}
	}

	// Step 3 (step 2 in 2-step; step 3 in 3-step): Putaway
	if ($inbound_steps >= 2) {
		$putaway_from = ($inbound_steps === 3) ? $qc_zone_id : $input_zone_id;
		$parent_for_putaway = ($inbound_steps === 3) ? $qc_op : $receipt_op;

		$putaway_op = add_wh_operation(
			'putaway',
			'draft',
			ST_SUPPRECEIVE,
			$grn_no,
			$parent_for_putaway,
			$putaway_from,
			null, // to_loc_id: per line (putaway engine decides)
			null,
			null,
			5,
			null,
			sprintf(_('GRN #%d putaway'), $grn_no)
		);
		$op_ids[] = $putaway_op;
		link_wh_operations($parent_for_putaway, $putaway_op);

		// Add lines with putaway destination per item
		foreach ($normalized_items as $item) {
			$stock_id = $item['stock_id'];
			$qty = $item['qty'];
			$putaway_bin = get_putaway_bin_for_item($stock_id, $warehouse_loc_code, $qty);

			if (!$putaway_bin || !is_valid_putaway_bin_location($putaway_bin, $warehouse_loc_code)) {
				$result['message'] = sprintf(
					_('No putaway bin found for item %s in warehouse %s.'),
					$stock_id,
					$warehouse_loc_code
				);
				return $result;
			}

			if (!empty($item['serial_ids'])) {
				foreach ($item['serial_ids'] as $serial_id) {
					add_wh_operation_line($putaway_op, $stock_id, 1, 0,
						$putaway_from, $putaway_bin,
						isset($item['batch_id']) ? $item['batch_id'] : null,
						$serial_id, null, isset($item['std_cost']) ? $item['std_cost'] : 0);
				}
			} else {
				add_wh_operation_line($putaway_op, $stock_id, $qty, 0,
					$putaway_from, $putaway_bin,
					isset($item['batch_id']) ? $item['batch_id'] : null,
					null, null, isset($item['std_cost']) ? $item['std_cost'] : 0);
			}
		}
	}

	$result['success'] = true;
	$result['operations'] = $op_ids;
	$step_names = array(1 => _('1-step (direct)'), 2 => _('2-step (receive+putaway)'), 3 => _('3-step (receive+QC+putaway)'));
	$result['message'] = sprintf(_('%d inbound operation(s) created (%s flow).'),
		count($op_ids), $step_names[$inbound_steps]);

	return $result;
}

/**
 * Find a zone by type within a warehouse.
 * Returns the first matching zone's loc_id, or null.
 *
 * @param string $warehouse_loc_code Warehouse code
 * @param string $zone_type_code     Type code (staging, quality, etc.)
 * @return int|null Zone loc_id or null
 */
function find_zone_by_type($warehouse_loc_code, $zone_type_code) {
	$sql = "SELECT wl.loc_id FROM " . TB_PREF . "wh_locations wl
		INNER JOIN " . TB_PREF . "wh_location_types lt ON wl.location_type_id = lt.id
		WHERE wl.warehouse_loc_code = " . db_escape($warehouse_loc_code)
		. " AND lt.type_code = " . db_escape($zone_type_code)
		. " AND wl.is_active = 1
		ORDER BY wl.loc_id ASC LIMIT 1";
	$result = db_query($sql, 'could not find zone by type');
	$row = db_fetch($result);
	return $row ? (int)$row['loc_id'] : null;
}

/**
 * Check whether a location can be used as a final putaway bin destination.
 *
 * @param int    $loc_id             Candidate location id.
 * @param string $warehouse_loc_code Warehouse scope code.
 * @return bool
 */
function is_valid_putaway_bin_location($loc_id, $warehouse_loc_code) {
	if (!$loc_id)
		return false;

	$sql = "SELECT wl.loc_id FROM " . TB_PREF . "wh_locations wl
		INNER JOIN " . TB_PREF . "wh_location_types lt ON wl.location_type_id = lt.id
		WHERE wl.loc_id=" . (int)$loc_id
		. " AND wl.warehouse_loc_code=" . db_escape($warehouse_loc_code)
		. " AND wl.is_active = 1
		AND lt.can_store = 1
		AND lt.type_code = 'bin'
		LIMIT 1";
	$result = db_query($sql, 'could not validate putaway bin location');
	$row = db_fetch($result);

	return (bool)$row;
}

/**
 * Get the best putaway bin for an item using the putaway engine.
 * Falls back to the first storable bin in the warehouse.
 *
 * @param string $stock_id            Item code
 * @param string $warehouse_loc_code  Warehouse code
 * @param float  $qty                 Quantity
 * @return int|null Bin loc_id or null
 */
function get_putaway_bin_for_item($stock_id, $warehouse_loc_code, $qty) {
	// Try putaway engine
	$engine_path = dirname(__FILE__) . '/putaway_engine.inc';
	if (file_exists($engine_path)) {
		include_once($engine_path);
		if (function_exists('execute_putaway')) {
			$assignments = execute_putaway($stock_id, $warehouse_loc_code, $qty);
			if (!empty($assignments)) {
				$candidate_bin = (int)$assignments[0]['bin_loc_id'];
				if (is_valid_putaway_bin_location($candidate_bin, $warehouse_loc_code))
					return $candidate_bin;
			} elseif (function_exists('find_putaway_rule_for_item')) {
				$matched_rule = find_putaway_rule_for_item($stock_id, $warehouse_loc_code);
				if ($matched_rule) {
					// Do not silently route to fallback bins when an explicit rule matched
					// but could not provide a valid assignment (inactive target/zone/capacity).
					return null;
				}
			}
		}
	}

	// Fallback: find any storable bin in the warehouse
	$sql = "SELECT wl.loc_id FROM " . TB_PREF . "wh_locations wl
		INNER JOIN " . TB_PREF . "wh_location_types lt ON wl.location_type_id = lt.id
		WHERE wl.warehouse_loc_code = " . db_escape($warehouse_loc_code)
		. " AND lt.can_store = 1
		AND lt.type_code = 'bin'
		AND wl.is_active = 1
		ORDER BY wl.pick_sequence ASC, wl.loc_id ASC
		LIMIT 1";
	$result = db_query($sql, 'could not find fallback bin');
	$row = db_fetch($result);
	return $row ? (int)$row['loc_id'] : null;
}

/**
 * Read a POST field as trimmed text, rejecting non-scalar payloads.
 *
 * @param string $key                 POST key.
 * @param array  $validation_errors   Validation errors array (by reference).
 * @param string $field_label         Human-readable field label.
 * @param string $stock_id            Item code for error context.
 * @return string Trimmed text value, or empty string if missing/invalid.
 */
function get_grn_post_text_field($key, &$validation_errors, $field_label, $stock_id) {
	if (!isset($_POST[$key]) || $_POST[$key] === null)
		return '';

	$value = $_POST[$key];
	if (!is_scalar($value)) {
		$validation_errors[] = sprintf(
			_('%s for item %s must be plain text.'),
			$field_label,
			$stock_id
		);
		return '';
	}

	return trim((string)$value);
}

/**
 * Validate GRN tracking payload before serial/batch processing.
 *
 * @param object $po                 Purchase order/cart object.
 * @param array  $validation_errors  Output list of validation errors.
 * @return bool
 */
function validate_grn_tracking_payload($po, &$validation_errors) {
	$validation_errors = array();

	$root = dirname(dirname(dirname(__FILE__))); // -> inventory/
	$serial_batch_path = $root . '/includes/db/serial_batch_db.inc';
	if (file_exists($serial_batch_path))
		include_once($serial_batch_path);

	if (!function_exists('item_has_serial_tracking') || !function_exists('item_has_batch_tracking'))
		return true;

	$seen_serials = array();

	foreach ($po->line_items as $line_no => $order_line) {
		if (!isset($order_line->receive_qty))
			continue;

		$receive_qty = (float)$order_line->receive_qty;
		if ($receive_qty <= 0)
			continue;

		$stock_id = $order_line->stock_id;

		if (item_has_serial_tracking($stock_id)) {
			$expected_serial_count = (int)round($receive_qty);
			if ($receive_qty != $expected_serial_count) {
				$validation_errors[] = sprintf(
					_('Item %s requires serial tracking and must be received in whole units.'),
					$stock_id
				);
				continue;
			}

			$serial_data_key = 'serial_' . $line_no;
			$serial_text = get_grn_post_text_field(
				$serial_data_key,
				$validation_errors,
				_('Serial input'),
				$stock_id
			);
			if ($serial_text === '' && isset($_POST[$serial_data_key]) && !is_scalar($_POST[$serial_data_key]))
				continue;
			if ($serial_text === '') {
				$validation_errors[] = sprintf(
					_('Item %s requires %d serial number(s), but none were provided.'),
					$stock_id,
					$expected_serial_count
				);
				continue;
			}

			$serial_entries = preg_split('/[\r\n,]+/', $serial_text);
			$clean_serials = array();
			foreach ($serial_entries as $serial_entry) {
				$serial_number = trim($serial_entry);
				if ($serial_number === '')
					continue;

				if (preg_match('/[\x00-\x1F\x7F]/', $serial_number)) {
					$validation_errors[] = sprintf(
						_('Serial number "%s" for item %s contains invalid control characters.'),
						$serial_number,
						$stock_id
					);
					continue;
				}

				if (strlen($serial_number) > 60) {
					$validation_errors[] = sprintf(
						_('Serial number "%s" for item %s exceeds 60 characters.'),
						$serial_number,
						$stock_id
					);
					continue;
				}

				$serial_key = strtoupper($serial_number);
				if (isset($seen_serials[$serial_key])) {
					$validation_errors[] = sprintf(
						_('Duplicate serial number "%s" was provided in this GRN payload.'),
						$serial_number
					);
					continue;
				}

				$seen_serials[$serial_key] = true;
				$clean_serials[] = $serial_number;
			}

			if (count($clean_serials) != $expected_serial_count) {
				$validation_errors[] = sprintf(
					_('Item %s requires exactly %d serial number(s); received %d.'),
					$stock_id,
					$expected_serial_count,
					count($clean_serials)
				);
			}
		}

		if (item_has_batch_tracking($stock_id)) {
			$batch_no_key = 'batch_no_' . $line_no;
			$batch_no = get_grn_post_text_field(
				$batch_no_key,
				$validation_errors,
				_('Batch number'),
				$stock_id
			);
			if ($batch_no === '' && isset($_POST[$batch_no_key]) && !is_scalar($_POST[$batch_no_key]))
				continue;

			if ($batch_no === '') {
				$validation_errors[] = sprintf(
					_('Item %s requires a batch number, but none was provided.'),
					$stock_id
				);
			} else {
				if (preg_match('/[\x00-\x1F\x7F]/', $batch_no)) {
					$validation_errors[] = sprintf(
						_('Batch number "%s" for item %s contains invalid control characters.'),
						$batch_no,
						$stock_id
					);
				}

				if (strlen($batch_no) > 60) {
					$validation_errors[] = sprintf(
						_('Batch number "%s" for item %s exceeds 60 characters.'),
						$batch_no,
						$stock_id
					);
				}
			}

			$expiry_key = 'batch_expiry_' . $line_no;
			$expiry_date = get_grn_post_text_field(
				$expiry_key,
				$validation_errors,
				_('Batch expiry date'),
				$stock_id
			);
			$mfg_key = 'batch_mfg_' . $line_no;
			$mfg_date = get_grn_post_text_field(
				$mfg_key,
				$validation_errors,
				_('Batch manufacturing date'),
				$stock_id
			);

			if ($expiry_date !== '' && !is_date($expiry_date)) {
				$validation_errors[] = sprintf(
					_('Batch expiry date "%s" for item %s is invalid.'),
					$expiry_date,
					$stock_id
				);
			}

			if ($mfg_date !== '' && !is_date($mfg_date)) {
				$validation_errors[] = sprintf(
					_('Batch manufacturing date "%s" for item %s is invalid.'),
					$mfg_date,
					$stock_id
				);
			}

			if ($expiry_date !== '' && $mfg_date !== '' && is_date($expiry_date) && is_date($mfg_date)) {
				if (date2sql($mfg_date) > date2sql($expiry_date)) {
					$validation_errors[] = sprintf(
						_('Batch manufacturing date cannot be later than expiry date for item %s.'),
						$stock_id
					);
				}
			}
		}
	}

	return empty($validation_errors);
}

/**
 * Update bin stock when stock is placed into a bin.
 *
 * Inserts or updates 0_wh_bin_stock for the given bin/item/batch/serial combo.
 * Also updates bin capacity via update_bin_current_capacity() if available.
 *
 * @param int         $wh_loc_id   Bin location id
 * @param string      $stock_id    Item code
 * @param float       $qty         Quantity (positive = add, negative = remove)
 * @param int|null    $batch_id    Batch id
 * @param int|null    $serial_id   Serial id
 * @param string|null $expiry_date SQL date
 * @param string      $status      Stock status (default 'available')
 */
function update_bin_stock($wh_loc_id, $stock_id, $qty, $batch_id = null,
	$serial_id = null, $expiry_date = null, $status = 'available')
{
	if (!$wh_loc_id) return;

	// Check if a matching bin_stock row exists
	$where = "wh_loc_id=" . (int)$wh_loc_id
		. " AND stock_id=" . db_escape($stock_id)
		. " AND " . ($batch_id !== null ? "batch_id=" . (int)$batch_id : "batch_id IS NULL")
		. " AND " . ($serial_id !== null ? "serial_id=" . (int)$serial_id : "serial_id IS NULL");

	$sql = "SELECT id, qty_on_hand FROM " . TB_PREF . "wh_bin_stock WHERE " . $where;
	$result = db_query($sql, 'could not check bin stock');
	$existing = db_fetch($result);

	if ($existing) {
		$new_qty = $existing['qty_on_hand'] + $qty;
		$sql = "UPDATE " . TB_PREF . "wh_bin_stock SET "
			. "qty_on_hand=" . (float)$new_qty . ","
			. "qty_available=" . (float)$new_qty . ","
			. "stock_status=" . db_escape($status)
			. " WHERE id=" . (int)$existing['id'];
		db_query($sql, 'could not update bin stock');
	} else {
		$sql = "INSERT INTO " . TB_PREF . "wh_bin_stock (
			wh_loc_id, stock_id, qty_on_hand, qty_reserved, qty_available,
			batch_id, serial_id, expiry_date, stock_status
		) VALUES (
			" . (int)$wh_loc_id . ",
			" . db_escape($stock_id) . ",
			" . (float)$qty . ",
			0,
			" . (float)$qty . ",
			" . ($batch_id !== null ? (int)$batch_id : 'NULL') . ",
			" . ($serial_id !== null ? (int)$serial_id : 'NULL') . ",
			" . ($expiry_date ? db_escape($expiry_date) : 'NULL') . ",
			" . db_escape($status) . "
		)";
		db_query($sql, 'could not insert bin stock');
	}

	// Update bin capacity tracking if available
	$loc_db_path = dirname(__FILE__) . '/db/warehouse_locations_db.inc';
	if (file_exists($loc_db_path)) {
		include_once($loc_db_path);
		if (function_exists('update_bin_current_capacity')) {
			// Get item weight/volume
			$item_sql = "SELECT item_weight, item_volume FROM " . TB_PREF
				. "stock_master WHERE stock_id=" . db_escape($stock_id);
			$item_result = db_query($item_sql, 'could not get item weight/volume');
			$item_row = db_fetch($item_result);
			if ($item_row) {
				$weight_delta = (float)$item_row['item_weight'] * $qty;
				$volume_delta = (float)$item_row['item_volume'] * $qty;
				update_bin_current_capacity($wh_loc_id, $weight_delta, $volume_delta, $qty);
			}
		}
	}
}

/**
 * Process GRN serial/batch data: creates serial records, batch records,
 * logs movements, updates bin stock, and creates WMS inbound operations.
 *
 * Called from add_grn() after the basic GRN save. This is the main
 * integration point between the purchasing system and the inventory
 * tracking + WMS systems.
 *
 * @param object $po       The purchase order object
 * @param int    $grn_no   GRN number
 * @param string $date_    Transaction date (display format)
 * @param string $location Location code (0_locations.loc_code)
 */
function process_grn_tracking_and_wms($po, $grn_no, $date_, $location) {
	// Load required DB functions
	$root = dirname(dirname(dirname(__FILE__))); // → inventory/
	include_once($root . '/includes/db/serial_batch_db.inc');
	include_once($root . '/includes/db/serial_numbers_db.inc');
	include_once($root . '/includes/db/stock_batches_db.inc');

	$validation_errors = array();
	if (!validate_grn_tracking_payload($po, $validation_errors)) {
		if (function_exists('display_error') && !empty($validation_errors))
			display_error(implode('<br>', $validation_errors));
		return false;
	}

	$wms_items = array(); // Collect items for WMS operation creation
	$is_wms = false;
	$has_qc_required_items = false;

	// Check if location is WMS-enabled
	$wms_settings = get_wms_location_settings($location);
	if ($wms_settings)
		$is_wms = true;

	foreach ($po->line_items as $line_no => $order_line) {
		if ($order_line->receive_qty == 0 || !isset($order_line->receive_qty))
			continue;

		$stock_id = $order_line->stock_id;
		$receive_qty = $order_line->receive_qty;
		$tracking_mode = get_item_tracking_mode($stock_id);

		if ($tracking_mode === 'none' && !$is_wms)
			continue;

		$serial_ids = array();
		$batch_id = null;
		$grn_item_id = isset($order_line->grn_item_id) ? $order_line->grn_item_id : null;

		// --- Serial Number Processing ---
		if (item_has_serial_tracking($stock_id)) {
			$serial_data_key = 'serial_' . $line_no;
			$serial_text = '';
			if (isset($_POST[$serial_data_key]) && is_scalar($_POST[$serial_data_key]))
				$serial_text = trim((string)$_POST[$serial_data_key]);

			if (!empty($serial_text)) {
				$serials = preg_split('/[\r\n,]+/', $serial_text);
				foreach ($serials as $sn) {
					$sn = trim($sn);
					if (empty($sn)) continue;

					$sid = add_serial_number(
						$sn,
						$stock_id,
						'available',
						$location,
						null,   // wh_loc_id (set by WMS putaway later)
						$batch_id,
						$po->supplier_id,
						$date_, // purchase_date
						null,   // manufacturing_date
						null,   // expiry_date
						null,   // warranty_start
						null,   // warranty_end
						null,   // warranty_type
						$order_line->standard_cost,
						$grn_no,
						null    // notes
					);

					if ($sid) {
						$serial_ids[] = $sid;

						// Log serial movement
						add_serial_movement(
							$sid,
							ST_SUPPRECEIVE,
							$grn_no,
							'',         // from_loc (external)
							$location,  // to_loc
							'',         // from_status
							'available', // to_status
							$date_,
							$po->reference,
							sprintf(_('Received via GRN #%d'), $grn_no)
						);
					}
				}
			}

			// Update grn_items with serial numbers JSON
			if (!empty($serial_ids) && $grn_item_id) {
				$serial_nos = array();
				foreach ($serial_ids as $sid) {
					$sn_row = get_serial_number($sid);
					if ($sn_row) $serial_nos[] = $sn_row['serial_no'];
				}
				$json = json_encode($serial_nos);
				$sql = "UPDATE " . TB_PREF . "grn_items SET serial_numbers_json="
					. db_escape($json) . " WHERE id=" . (int)$grn_item_id;
				db_query($sql, 'could not update grn_items serial JSON');
			}
		}

		// --- Batch/Lot Processing ---
		if (item_has_batch_tracking($stock_id)) {
			$batch_no_key = 'batch_no_' . $line_no;
			$batch_no = '';
			if (isset($_POST[$batch_no_key]) && is_scalar($_POST[$batch_no_key]))
				$batch_no = trim((string)$_POST[$batch_no_key]);
			$expiry_key = 'batch_expiry_' . $line_no;
			$expiry_date = '';
			if (isset($_POST[$expiry_key]) && is_scalar($_POST[$expiry_key]))
				$expiry_date = trim((string)$_POST[$expiry_key]);
			$mfg_key = 'batch_mfg_' . $line_no;
			$mfg_date = '';
			if (isset($_POST[$mfg_key]) && is_scalar($_POST[$mfg_key]))
				$mfg_date = trim((string)$_POST[$mfg_key]);

			if (!empty($batch_no)) {
				// Check if batch already exists for this item
				$existing_batch = get_stock_batch_by_code($batch_no, $stock_id);

				if ($existing_batch) {
					$batch_id = (int)$existing_batch['id'];
				} else {
					// Convert dates if provided
					$expiry_sql = null;
					$mfg_sql = null;
					if (!empty($expiry_date) && is_date($expiry_date))
						$expiry_sql = $expiry_date;
					if (!empty($mfg_date) && is_date($mfg_date))
						$mfg_sql = $mfg_date;

					$batch_id = add_stock_batch(
						$batch_no,
						$stock_id,
						'active',
						$mfg_sql,      // manufacturing_date
						$expiry_sql,   // expiry_date
						null,          // best_before_date
						null,          // retest_date
						null,          // shelf_life_days
						$po->supplier_id,
						null,          // supplier_batch_no
						$grn_no,       // grn_id
						null,          // work_order_id
						$receive_qty,  // initial_qty
						null,          // country_of_origin
						null,          // certification
						null           // notes
					);
				}

				if ($batch_id) {
					// Log batch movement
					add_batch_movement(
						$batch_id,
						ST_SUPPRECEIVE,
						$grn_no,
						$location,
						null,           // wh_loc_id
						$receive_qty,
						$date_,
						$po->reference,
						sprintf(_('Received via GRN #%d'), $grn_no)
					);

					// Update serial numbers with batch_id if both serial+batch
					foreach ($serial_ids as $sid) {
						$sql = "UPDATE " . TB_PREF . "serial_numbers SET batch_id="
							. (int)$batch_id . " WHERE id=" . (int)$sid;
						db_query($sql, 'could not link serial to batch');
					}
				}

				// Update grn_items with batch info
				if ($grn_item_id) {
					$sets = array("batch_id=" . (int)$batch_id);
					if (!empty($expiry_date) && is_date($expiry_date))
						$sets[] = "expiry_date=" . db_escape(date2sql($expiry_date));
					if (!empty($mfg_date) && is_date($mfg_date))
						$sets[] = "manufacturing_date=" . db_escape(date2sql($mfg_date));

					$sql = "UPDATE " . TB_PREF . "grn_items SET "
						. implode(', ', $sets)
						. " WHERE id=" . (int)$grn_item_id;
					db_query($sql, 'could not update grn_items batch info');
				}
			}
		}

		// Quality inspection required?
		$item_data = get_item($stock_id);
		$qc_required = isset($item_data['quality_inspection_required']) && $item_data['quality_inspection_required'];
		if ($qc_required)
			$has_qc_required_items = true;

		if ($qc_required && $grn_item_id) {
			$sql = "UPDATE " . TB_PREF . "grn_items SET inspection_status='pending'"
				. " WHERE id=" . (int)$grn_item_id;
			db_query($sql, 'could not set inspection status');
		}

		// Collect for WMS operations
		if ($is_wms) {
			$wms_items[] = array(
				'stock_id'   => $stock_id,
				'qty'        => $receive_qty,
				'std_cost'   => $order_line->standard_cost,
				'serial_ids' => $serial_ids,
				'batch_id'   => $batch_id,
			);
		}
	}

	// Auto-create QC inspections from this GRN for QC-required items.
	if ($has_qc_required_items) {
		$qc_db_path = $root . '/includes/db/quality_inspection_db.inc';
		if (file_exists($qc_db_path)) {
			include_once($qc_db_path);
			if (function_exists('auto_create_inspections_from_grn'))
				auto_create_inspections_from_grn((int)$grn_no, $location);
		}
	}

	// Create WMS inbound operations if warehouse is WMS-enabled
	if ($is_wms && !empty($wms_items)) {
		// --- Cross-dock detection (Session 25) ---
		// Check if location has cross-docking enabled. If so, try to match
		// received items against pending outbound demand (SO lines).
		// Items matched are routed receipt → ship, bypassing putaway.
		$crossdock_items = array();
		$normal_items = array();

		if (function_exists('is_cross_dock_enabled') && is_cross_dock_enabled($location)) {
			foreach ($wms_items as $item) {
				if (function_exists('is_item_cross_dock_eligible')
					&& is_item_cross_dock_eligible($item['stock_id']))
				{
					$allocations = allocate_cross_dock_qty(
						$item['stock_id'], $location, $item['qty']);

					if (!empty($allocations)) {
						$alloc_total = 0;
						foreach ($allocations as $a) {
							$alloc_total += $a['qty_allocated'];
						}

						$crossdock_item = $item;
						$crossdock_item['qty'] = $alloc_total;
						$remaining_serial_ids = array();

						if (!empty($item['serial_ids'])) {
							$allocated_serial_count = (int)round($alloc_total);
							if ($allocated_serial_count < 0)
								$allocated_serial_count = 0;
							if ($allocated_serial_count > count($item['serial_ids']))
								$allocated_serial_count = count($item['serial_ids']);

							$crossdock_item['serial_ids'] = array_slice($item['serial_ids'], 0, $allocated_serial_count);
							$remaining_serial_ids = array_slice($item['serial_ids'], $allocated_serial_count);
						}

						// Create cross-dock operations for the allocated portion
						$crossdock_result = create_cross_dock_operations($grn_no, $location, $allocations, $crossdock_item);
						if (!$crossdock_result['success']) {
							if (function_exists('display_error') && !empty($crossdock_result['message'])) {
								display_error($crossdock_result['message']);
							}
							$normal_items[] = $item;
							continue;
						}

						// Remaining qty goes through normal inbound
						$remaining = $item['qty'] - $alloc_total;
						if ($remaining > 0) {
							$normal_item = $item;
							$normal_item['qty'] = $remaining;
							if (!empty($item['serial_ids']))
								$normal_item['serial_ids'] = $remaining_serial_ids;
							$normal_items[] = $normal_item;
						}
					} else {
						// No matching demand — process normally
						$normal_items[] = $item;
					}
				} else {
					// Not cross-dock eligible — process normally
					$normal_items[] = $item;
				}
			}
		} else {
			$normal_items = $wms_items;
		}

		// Create standard inbound operations for remaining items
		if (!empty($normal_items)) {
			$inbound_result = create_inbound_operations($grn_no, $location, $normal_items);
			if (!$inbound_result['success']) {
				if (function_exists('display_error') && !empty($inbound_result['message']))
					display_error($inbound_result['message']);
				return false;
			}
		}
	}

	return true;
}

// =====================================================================
// OUTBOUND OPERATIONS (Session 10 — Sales/Delivery Integration)
// =====================================================================

/**
 * Create the outbound operation chain for a sales delivery.
 *
 * Based on the warehouse's `outbound_steps` setting:
 *   1-step: Ship directly (pick from stock, ship out)
 *   2-step: Pick → Ship
 *   3-step: Pick → Pack → Ship
 *
 * @param int    $delivery_no    Delivery note number (source_doc_no)
 * @param string $loc_code       Inventory location code (0_locations.loc_code)
 * @param array  $items          Array of items, each: ['stock_id', 'qty', 'std_cost',
 *                               'serial_ids'=>[], 'batch_allocations'=>[],
 *                               'bin_allocations'=>[]]
 * @return array Result with 'success', 'operations' (array of op_ids in chain order),
 *               'message'
 */
function create_outbound_operations($delivery_no, $loc_code, $items) {
	$result = array('success' => false, 'operations' => array(), 'message' => '');

	$wms = get_wms_location_settings($loc_code);
	if (!$wms) {
		$result['message'] = _('Location is not WMS-enabled. No operations created.');
		return $result;
	}

	// Try route-based operation creation first
	if (function_exists('resolve_route')) {
		$route = resolve_route($loc_code, 'outbound');
		if ($route) {
			return create_operations_from_route($route, ST_CUSTDELIVERY, $delivery_no, $items, $loc_code);
		}
	}

	// Fallback: legacy step-based creation (outbound_steps setting)
	$outbound_steps = (int)$wms['outbound_steps'];
	if ($outbound_steps < 1) $outbound_steps = 1;
	if ($outbound_steps > 3) $outbound_steps = 3;

	$inc_path = dirname(__FILE__) . '/db/warehouse_locations_db.inc';
	if (file_exists($inc_path))
		include_once($inc_path);

	$root = null;
	if (function_exists('get_warehouse_root_location'))
		$root = get_warehouse_root_location($loc_code);

	if (!$root) {
		$result['message'] = _('No warehouse root location found. Enable WMS on this location first.');
		return $result;
	}

	// Find special zones
	$staging_zone_id = find_zone_by_type($loc_code, 'staging');
	$packing_zone_id = find_zone_by_type($loc_code, 'packing');
	$dock_zone_id = find_zone_by_type($loc_code, 'dock');

	$op_ids = array();
	$now = date('Y-m-d H:i:s');

	// Step 1: Pick operation (always created for 2+ steps, or combined pick+ship for 1-step)
	if ($outbound_steps === 1) {
		// 1-step: single Ship operation (pick from stock bin → out)
		$ship_op = add_wh_operation(
			'ship',
			'ready',
			ST_CUSTDELIVERY,
			$delivery_no,
			null,
			null, // from_loc_id: per line (stock bin)
			$dock_zone_id,
			$now,
			null,
			5,
			null,
			sprintf(_('Delivery #%d ship'), $delivery_no)
		);
		$op_ids[] = $ship_op;

		foreach ($items as $item) {
			$stock_id = $item['stock_id'];
			$qty = $item['qty'];
			$std_cost = isset($item['std_cost']) ? $item['std_cost'] : 0;
			$from_bin = null;

			// Use bin allocations if available
			if (!empty($item['bin_allocations'])) {
				foreach ($item['bin_allocations'] as $alloc) {
					$batch_id = isset($alloc['batch_id']) ? $alloc['batch_id'] : null;
					$serial_id = isset($alloc['serial_id']) ? $alloc['serial_id'] : null;
					add_wh_operation_line($ship_op, $stock_id, $alloc['qty_to_pick'], 0,
						$alloc['bin_loc_id'], $dock_zone_id, $batch_id, $serial_id, null, $std_cost);
				}
			} elseif (!empty($item['serial_ids'])) {
				foreach ($item['serial_ids'] as $sid) {
					add_wh_operation_line($ship_op, $stock_id, 1, 0,
						null, $dock_zone_id,
						isset($item['batch_id']) ? $item['batch_id'] : null,
						$sid, null, $std_cost);
				}
			} else {
				add_wh_operation_line($ship_op, $stock_id, $qty, 0,
					null, $dock_zone_id,
					isset($item['batch_id']) ? $item['batch_id'] : null,
					null, null, $std_cost);
			}
		}

		complete_wh_operation($ship_op);

	} else {
		// 2+ steps: Pick operation first
		$pick_to = ($outbound_steps === 3) ? $packing_zone_id : ($staging_zone_id ? $staging_zone_id : $dock_zone_id);

		$pick_op = add_wh_operation(
			'pick',
			'ready',
			ST_CUSTDELIVERY,
			$delivery_no,
			null,
			null, // from per line
			$pick_to,
			$now,
			null,
			5,
			null,
			sprintf(_('Delivery #%d pick'), $delivery_no)
		);
		$op_ids[] = $pick_op;

		foreach ($items as $item) {
			$stock_id = $item['stock_id'];
			$qty = $item['qty'];
			$std_cost = isset($item['std_cost']) ? $item['std_cost'] : 0;

			if (!empty($item['bin_allocations'])) {
				foreach ($item['bin_allocations'] as $alloc) {
					$batch_id = isset($alloc['batch_id']) ? $alloc['batch_id'] : null;
					$serial_id = isset($alloc['serial_id']) ? $alloc['serial_id'] : null;
					add_wh_operation_line($pick_op, $stock_id, $alloc['qty_to_pick'], 0,
						$alloc['bin_loc_id'], $pick_to, $batch_id, $serial_id, null, $std_cost);
				}
			} elseif (!empty($item['serial_ids'])) {
				foreach ($item['serial_ids'] as $sid) {
					add_wh_operation_line($pick_op, $stock_id, 1, 0,
						null, $pick_to,
						isset($item['batch_id']) ? $item['batch_id'] : null,
						$sid, null, $std_cost);
				}
			} else {
				add_wh_operation_line($pick_op, $stock_id, $qty, 0,
					null, $pick_to,
					isset($item['batch_id']) ? $item['batch_id'] : null,
					null, null, $std_cost);
			}
		}

		// Step 2 (3-step only): Pack operation
		$pack_op = null;
		if ($outbound_steps === 3) {
			$pack_to = $staging_zone_id ? $staging_zone_id : $dock_zone_id;
			$pack_op = add_wh_operation(
				'pack',
				'draft',
				ST_CUSTDELIVERY,
				$delivery_no,
				$pick_op,
				$packing_zone_id,
				$pack_to,
				null,
				null,
				5,
				null,
				sprintf(_('Delivery #%d pack'), $delivery_no)
			);
			$op_ids[] = $pack_op;
			link_wh_operations($pick_op, $pack_op);

			foreach ($items as $item) {
				$stock_id = $item['stock_id'];
				$qty = $item['qty'];
				$std_cost = isset($item['std_cost']) ? $item['std_cost'] : 0;

				if (!empty($item['serial_ids'])) {
					foreach ($item['serial_ids'] as $sid) {
						add_wh_operation_line($pack_op, $stock_id, 1, 0,
							$packing_zone_id, $pack_to,
							isset($item['batch_id']) ? $item['batch_id'] : null,
							$sid, null, $std_cost);
					}
				} else {
					add_wh_operation_line($pack_op, $stock_id, $qty, 0,
						$packing_zone_id, $pack_to,
						isset($item['batch_id']) ? $item['batch_id'] : null,
						null, null, $std_cost);
				}
			}
		}

		// Final step: Ship operation
		$ship_from = ($outbound_steps === 3) ? ($staging_zone_id ? $staging_zone_id : $dock_zone_id) : $pick_to;
		$parent_for_ship = ($outbound_steps === 3) ? $pack_op : $pick_op;

		$ship_op = add_wh_operation(
			'ship',
			'draft',
			ST_CUSTDELIVERY,
			$delivery_no,
			$parent_for_ship,
			$ship_from,
			$dock_zone_id,
			null,
			null,
			5,
			null,
			sprintf(_('Delivery #%d ship'), $delivery_no)
		);
		$op_ids[] = $ship_op;
		link_wh_operations($parent_for_ship, $ship_op);

		foreach ($items as $item) {
			$stock_id = $item['stock_id'];
			$qty = $item['qty'];
			$std_cost = isset($item['std_cost']) ? $item['std_cost'] : 0;

			if (!empty($item['serial_ids'])) {
				foreach ($item['serial_ids'] as $sid) {
					add_wh_operation_line($ship_op, $stock_id, 1, 0,
						$ship_from, $dock_zone_id,
						isset($item['batch_id']) ? $item['batch_id'] : null,
						$sid, null, $std_cost);
				}
			} else {
				add_wh_operation_line($ship_op, $stock_id, $qty, 0,
					$ship_from, $dock_zone_id,
					isset($item['batch_id']) ? $item['batch_id'] : null,
					null, null, $std_cost);
			}
		}
	}

	$result['success'] = true;
	$result['operations'] = $op_ids;
	$step_names = array(
		1 => _('1-step (direct ship)'),
		2 => _('2-step (pick+ship)'),
		3 => _('3-step (pick+pack+ship)')
	);
	$result['message'] = sprintf(_('%d outbound operation(s) created (%s flow).'),
		count($op_ids), $step_names[$outbound_steps]);

	return $result;
}

/**
 * Process delivery serial/batch data: updates serial statuses, allocates batches,
 * updates bin stock, logs movements, and creates WMS outbound operations.
 *
 * Called from write_sales_delivery() after the line items loop. This is the main
 * integration point between the sales delivery system and inventory tracking + WMS.
 *
 * @param object $delivery     The delivery cart object
 * @param int    $delivery_no  Delivery note number
 * @param string $date_        Transaction date (display format)
 * @param string $location     Location code (0_locations.loc_code)
 */
function process_delivery_tracking_and_wms($delivery, $delivery_no, $date_, $location) {
	$root = dirname(dirname(dirname(__FILE__))); // → inventory/
	include_once($root . '/includes/db/serial_batch_db.inc');
	include_once($root . '/includes/db/serial_numbers_db.inc');
	include_once($root . '/includes/db/stock_batches_db.inc');

	$validation_errors = array();
	if (!validate_delivery_tracking_payload($delivery, $location, $validation_errors)) {
		if (function_exists('display_error') && !empty($validation_errors))
			display_error(implode('<br>', $validation_errors));
		return false;
	}

	$removal_path = dirname(__FILE__) . '/removal_engine.inc';
	if (file_exists($removal_path))
		include_once($removal_path);

	$wms_items = array();
	$is_wms = false;

	$wms_settings = get_wms_location_settings($location);
	if ($wms_settings)
		$is_wms = true;

	$delivery_source_reference = '';
	if (isset($delivery->reference) && is_scalar($delivery->reference))
		$delivery_source_reference = trim((string)$delivery->reference);
	if ($delivery_source_reference === '')
		$delivery_source_reference = sprintf(_('Delivered via DN #%d'), (int)$delivery_no);

	foreach ($delivery->line_items as $line_no => $delivery_line) {
		if ($delivery_line->qty_dispatched == 0)
			continue;

		$stock_id = $delivery_line->stock_id;
		$qty = $delivery_line->qty_dispatched;
		$tracking_mode = get_item_tracking_mode($stock_id);

		if ($tracking_mode === 'none' && !$is_wms)
			continue;

		$serial_ids = array();
		$batch_allocations = array();
		$bin_allocations = array();

		// --- Serial Number Processing ---
		if (item_has_serial_tracking($stock_id)) {
			$serial_data_key = 'delivery_serial_' . $line_no;
			$serial_text = isset($_POST[$serial_data_key]) ? trim($_POST[$serial_data_key]) : '';

			if (!empty($serial_text)) {
				$serial_entries = preg_split('/[\r\n,]+/', $serial_text);
				foreach ($serial_entries as $sn) {
					$sn = trim($sn);
					if (empty($sn)) continue;

					// Find the serial by number and item
					$serial = get_serial_number_by_code($sn, $stock_id);
					if ($serial && $serial['status'] === 'available') {
						$sid = (int)$serial['id'];
						$serial_ids[] = $sid;

						// Update serial status to 'delivered', set customer + warranty
						$warranty_start = $date_;
						$warranty_end = null;
						if (!empty($serial['warranty_days'])) {
							$warranty_end = add_days($date_, (int)$serial['warranty_days']);
						} else {
							// Check item-level warranty
							$item_data = get_item($stock_id);
							if ($item_data && !empty($item_data['warranty_days'])) {
								$warranty_end = add_days($date_, (int)$item_data['warranty_days']);
							}
						}

						update_serial_status(
							$sid,
							'delivered',
							ST_CUSTDELIVERY,
							$delivery_no,
							$location,    // from_loc
							'',           // to_loc (customer)
							$date_,
							$delivery->reference,
							sprintf(_('Delivered via DN #%d to customer %s'),
								$delivery_no, $delivery->customer_name)
						);

						// Update customer and warranty info on serial
						$sets = array("customer_id=" . (int)$delivery->customer_id);
						$sets[] = "delivery_id=" . (int)$delivery_no;
						$sets[] = "delivery_date=" . db_escape(date2sql($date_));
						if ($warranty_start)
							$sets[] = "warranty_start=" . db_escape(date2sql($warranty_start));
						if ($warranty_end)
							$sets[] = "warranty_end=" . db_escape(date2sql($warranty_end));
						$sql = "UPDATE " . TB_PREF . "serial_numbers SET "
							. implode(', ', $sets)
							. " WHERE id=" . (int)$sid;
						db_query($sql, 'could not update serial customer/warranty');

						// Update bin stock (remove from bin)
						if ($is_wms && !empty($serial['wh_loc_id'])) {
							update_bin_stock($serial['wh_loc_id'], $stock_id, -1, 
								!empty($serial['batch_id']) ? (int)$serial['batch_id'] : null,
								$sid);
							$bin_allocations[] = array(
								'bin_loc_id' => (int)$serial['wh_loc_id'],
								'qty_to_pick' => 1,
								'serial_id' => $sid,
								'batch_id' => !empty($serial['batch_id']) ? (int)$serial['batch_id'] : null,
							);
						}
					}
				}
			}
		}

		// --- Batch/Lot Processing ---
		if (item_has_batch_tracking($stock_id)) {
			$batch_data_key = 'delivery_batch_' . $line_no;
			$batch_input = isset($_POST[$batch_data_key]) ? trim($_POST[$batch_data_key]) : '';

			if (!empty($batch_input)) {
				// Format: "batch_id:qty,batch_id:qty" or just "batch_id" for full qty
				$batch_entries = preg_split('/[\r\n;]+/', $batch_input);
				$remaining_qty = $qty;

				foreach ($batch_entries as $entry) {
					$entry = trim($entry);
					if (empty($entry) || $remaining_qty <= 0) continue;

					if (strpos($entry, ':') !== false) {
						list($bid, $bqty) = explode(':', $entry, 2);
						$bid = (int)trim($bid);
						$bqty = (float)trim($bqty);
					} else {
						$bid = (int)$entry;
						$bqty = $remaining_qty;
					}

					if ($bid > 0 && $bqty > 0) {
						$alloc_qty = min($bqty, $remaining_qty);
						$batch_allocations[] = array('batch_id' => $bid, 'qty' => $alloc_qty);
						$remaining_qty -= $alloc_qty;

						// Log batch movement (negative = outbound)
						add_batch_movement(
							$bid,
							ST_CUSTDELIVERY,
							$delivery_no,
							$location,
							null,
							-$alloc_qty,
							$date_,
							$delivery->reference,
							sprintf(_('Delivered via DN #%d'), $delivery_no)
						);
					}
				}
			} else {
				// Auto-allocate using removal strategy (FEFO/FIFO)
				$allocated = allocate_batches($stock_id, $qty, $location);
				$allocated_total = 0;
				foreach ($allocated as $alloc) {
					$allocated_total += (float)$alloc['qty'];
					$batch_allocations[] = array(
						'batch_id' => $alloc['batch_id'],
						'qty' => $alloc['qty'],
					);

					add_batch_movement(
						$alloc['batch_id'],
						ST_CUSTDELIVERY,
						$delivery_no,
						$location,
						null,
						-$alloc['qty'],
						$date_,
						$delivery->reference,
						sprintf(_('Auto-allocated (FEFO/FIFO) via DN #%d'), $delivery_no)
					);
				}

				if ($allocated_total + 0.000001 < (float)$qty) {
					if (function_exists('display_error')) {
						display_error(sprintf(
							_('Item %s does not have enough batch quantity available at location %s for requested dispatch quantity %.2f.'),
							$stock_id,
							$location,
							(float)$qty
						));
					}
					return false;
				}
			}

			// Update bin stock for batch allocations if WMS
			if ($is_wms && !empty($batch_allocations) && function_exists('execute_removal')) {
				foreach ($batch_allocations as $ba) {
					// Find bins with this batch stock
					$sql = "SELECT bs.wh_loc_id, bs.qty_available FROM " . TB_PREF . "wh_bin_stock bs "
						. "INNER JOIN " . TB_PREF . "wh_locations wl ON bs.wh_loc_id = wl.loc_id "
						. "WHERE bs.stock_id = " . db_escape($stock_id)
						. " AND bs.batch_id = " . (int)$ba['batch_id']
						. " AND wl.warehouse_loc_code = " . db_escape($location)
						. " AND bs.stock_status = 'available'"
						. " AND bs.qty_available > 0"
						. " ORDER BY bs.id ASC";
					$bin_result = db_query($sql, 'could not get batch bin stock');
					$remaining = $ba['qty'];

					while ($bin_row = db_fetch($bin_result)) {
						if ($remaining <= 0) break;
						$pick = min($remaining, (float)$bin_row['qty_available']);
						update_bin_stock($bin_row['wh_loc_id'], $stock_id, -$pick,
							$ba['batch_id'], null);
						$bin_allocations[] = array(
							'bin_loc_id' => (int)$bin_row['wh_loc_id'],
							'qty_to_pick' => $pick,
							'batch_id' => $ba['batch_id'],
							'serial_id' => null,
						);
						$remaining -= $pick;
					}

					if ($remaining > 0.000001) {
						if (function_exists('display_error')) {
							display_error(sprintf(
								_('Item %s batch id %d has insufficient available bin stock in warehouse %s. Required %.2f, remaining %.2f after allocation.'),
								$stock_id,
								(int)$ba['batch_id'],
								$location,
								(float)$ba['qty'],
								(float)$remaining
							));
						}
						return false;
					}
				}
			}
		}

		if ($is_wms && $tracking_mode === 'none') {
			if (!function_exists('execute_removal')) {
				if (function_exists('display_error')) {
					display_error(sprintf(
						_('Removal strategy engine is unavailable for item %s in warehouse %s.'),
						$stock_id,
						$location
					));
				}
				return false;
			}

			$removal_allocations = execute_removal($stock_id, $location, $qty);
			$allocated_qty = 0;
			foreach ($removal_allocations as $allocation)
				$allocated_qty += isset($allocation['qty_to_pick']) ? (float)$allocation['qty_to_pick'] : 0;

			if ($allocated_qty + 0.000001 < (float)$qty) {
				if (function_exists('display_error')) {
					display_error(sprintf(
						_('Item %s does not have enough available bin stock in warehouse %s for requested dispatch quantity %.2f.'),
						$stock_id,
						$location,
						(float)$qty
					));
				}
				return false;
			}

			foreach ($removal_allocations as $allocation) {
				$pick_qty = isset($allocation['qty_to_pick']) ? (float)$allocation['qty_to_pick'] : 0;
				if ($pick_qty <= 0)
					continue;

				$from_bin_id = isset($allocation['bin_loc_id']) ? (int)$allocation['bin_loc_id'] : 0;
				if ($from_bin_id <= 0)
					continue;

				$alloc_batch_id = (isset($allocation['batch_id']) && $allocation['batch_id'] !== null)
					? (int)$allocation['batch_id'] : null;
				$alloc_expiry = null;
				if (isset($allocation['expiry_date']) && $allocation['expiry_date'] !== '')
					$alloc_expiry = $allocation['expiry_date'];

				update_bin_stock($from_bin_id, $stock_id, -$pick_qty, $alloc_batch_id, null, $alloc_expiry, 'available');
				$bin_allocations[] = array(
					'bin_loc_id' => $from_bin_id,
					'qty_to_pick' => $pick_qty,
					'batch_id' => $alloc_batch_id,
					'serial_id' => null,
				);

				add_stock_move(
					ST_CUSTDELIVERY,
					$stock_id,
					$delivery_no,
					$location,
					$date_,
					$delivery_source_reference,
					-$pick_qty,
					0,
					0,
					$from_bin_id,
					null,
					null,
					$alloc_batch_id
				);
			}
		}

		// WMS outbound items collection
		if ($is_wms) {
			$wms_items[] = array(
				'stock_id'          => $stock_id,
				'qty'               => $qty,
				'std_cost'          => $delivery_line->standard_cost,
				'serial_ids'        => $serial_ids,
				'batch_allocations' => $batch_allocations,
				'bin_allocations'   => $bin_allocations,
				'batch_id'          => !empty($batch_allocations) ? $batch_allocations[0]['batch_id'] : null,
			);
		}
	}

	// Create WMS outbound operations if warehouse is WMS-enabled
	if ($is_wms && !empty($wms_items)) {
		$outbound_result = create_outbound_operations($delivery_no, $location, $wms_items);
		if (is_array($outbound_result)
			&& isset($outbound_result['success'])
			&& !$outbound_result['success']) {
			if (function_exists('display_error') && !empty($outbound_result['message']))
				display_error($outbound_result['message']);
			return false;
		}
	}

	return true;
}

/**
 * Get a delivery post text field and validate scalar shape.
 *
 * @param string $key               Post field key.
 * @param array  $validation_errors Output validation errors.
 * @param string $field_label       Human label.
 * @param string $stock_id          Item code for context.
 * @return string
 */
function get_delivery_post_text_field($key, &$validation_errors, $field_label, $stock_id) {
	if (!isset($_POST[$key]) || $_POST[$key] === null)
		return '';

	$value = $_POST[$key];
	if (!is_scalar($value)) {
		$validation_errors[] = sprintf(
			_('%s for item %s must be plain text.'),
			$field_label,
			$stock_id
		);
		return '';
	}

	return trim((string)$value);
}

/**
 * Validate delivery serial/batch payload before stock mutation.
 *
 * @param object $delivery          Delivery cart object.
 * @param string $location          Delivery location code.
 * @param array  $validation_errors Output list of validation errors.
 * @return bool
 */
function validate_delivery_tracking_payload($delivery, $location, &$validation_errors) {
	$validation_errors = array();

	$root = dirname(dirname(dirname(__FILE__))); // -> inventory/
	$serial_batch_path = $root . '/includes/db/serial_batch_db.inc';
	if (file_exists($serial_batch_path))
		include_once($serial_batch_path);
	$serial_numbers_path = $root . '/includes/db/serial_numbers_db.inc';
	if (file_exists($serial_numbers_path))
		include_once($serial_numbers_path);
	$stock_batches_path = $root . '/includes/db/stock_batches_db.inc';
	if (file_exists($stock_batches_path))
		include_once($stock_batches_path);

	if (!function_exists('item_has_serial_tracking') || !function_exists('item_has_batch_tracking'))
		return true;

	$seen_serial_ids = array();

	foreach ($delivery->line_items as $line_no => $delivery_line) {
		if (!isset($delivery_line->qty_dispatched) || !is_valid_finite_numeric($delivery_line->qty_dispatched)) {
			$validation_errors[] = sprintf(
				_('Delivery line for item %s has invalid dispatch quantity.'),
				$delivery_line->stock_id
			);
			continue;
		}

		$dispatch_qty = (float)$delivery_line->qty_dispatched;
		if ($dispatch_qty <= 0)
			continue;

		$stock_id = $delivery_line->stock_id;

		if (item_has_serial_tracking($stock_id)) {
			$expected_serial_count = (int)round($dispatch_qty);
			if (abs($dispatch_qty - $expected_serial_count) > 0.000001) {
				$validation_errors[] = sprintf(
					_('Item %s requires serial tracking and must be dispatched in whole units.'),
					$stock_id
				);
				continue;
			}

			$serial_data_key = 'delivery_serial_' . $line_no;
			$serial_text = get_delivery_post_text_field(
				$serial_data_key,
				$validation_errors,
				_('Delivery serial input'),
				$stock_id
			);

			if ($serial_text === '' && isset($_POST[$serial_data_key]) && !is_scalar($_POST[$serial_data_key]))
				continue;

			if ($serial_text === '') {
				$validation_errors[] = sprintf(
					_('Item %s requires exactly %d serial number(s), but none were provided.'),
					$stock_id,
					$expected_serial_count
				);
				continue;
			}

			$serial_entries = preg_split('/[\r\n,]+/', $serial_text);
			$line_serial_count = 0;

			foreach ($serial_entries as $entry) {
				$serial_number = trim($entry);
				if ($serial_number === '')
					continue;

				if (preg_match('/[\x00-\x1F\x7F]/', $serial_number)) {
					$validation_errors[] = sprintf(
						_('Serial number "%s" for item %s contains invalid control characters.'),
						$serial_number,
						$stock_id
					);
					continue;
				}

				if (strlen($serial_number) > 60) {
					$validation_errors[] = sprintf(
						_('Serial number "%s" for item %s exceeds 60 characters.'),
						$serial_number,
						$stock_id
					);
					continue;
				}

				$serial = get_serial_number_by_code($serial_number, $stock_id);
				if (!$serial) {
					$validation_errors[] = sprintf(
						_('Serial number "%s" was not found for item %s.'),
						$serial_number,
						$stock_id
					);
					continue;
				}

				if (isset($serial['inactive']) && (int)$serial['inactive'] === 1) {
					$validation_errors[] = sprintf(
						_('Serial number "%s" for item %s is inactive.'),
						$serial_number,
						$stock_id
					);
					continue;
				}

				if (!isset($serial['status']) || strtolower(trim($serial['status'])) !== 'available') {
					$validation_errors[] = sprintf(
						_('Serial number "%s" for item %s is not available for dispatch.'),
						$serial_number,
						$stock_id
					);
					continue;
				}

				if (isset($serial['loc_code']) && $serial['loc_code'] !== '' && $serial['loc_code'] !== $location) {
					$validation_errors[] = sprintf(
						_('Serial number "%s" for item %s is at location %s, not %s.'),
						$serial_number,
						$stock_id,
						$serial['loc_code'],
						$location
					);
					continue;
				}

				$serial_id = (int)$serial['id'];
				if ($serial_id <= 0) {
					$validation_errors[] = sprintf(
						_('Serial number "%s" for item %s has invalid identifier.'),
						$serial_number,
						$stock_id
					);
					continue;
				}

				if (isset($seen_serial_ids[$serial_id])) {
					$validation_errors[] = sprintf(
						_('Duplicate serial number "%s" was provided in this delivery payload.'),
						$serial_number
					);
					continue;
				}

				$seen_serial_ids[$serial_id] = true;
				$line_serial_count++;
			}

			if ($line_serial_count !== $expected_serial_count) {
				$validation_errors[] = sprintf(
					_('Item %s requires exactly %d serial number(s); received %d valid serial(s).'),
					$stock_id,
					$expected_serial_count,
					$line_serial_count
				);
			}
		}

		if (item_has_batch_tracking($stock_id)) {
			$batch_data_key = 'delivery_batch_' . $line_no;
			$batch_text = get_delivery_post_text_field(
				$batch_data_key,
				$validation_errors,
				_('Delivery batch input'),
				$stock_id
			);

			if ($batch_text === '' && isset($_POST[$batch_data_key]) && !is_scalar($_POST[$batch_data_key]))
				continue;

			if ($batch_text === '')
				continue; // Auto-allocation path.

			$entries = preg_split('/[\r\n;]+/', $batch_text);
			$line_batch_ids = array();
			$total_batch_qty = 0;

			foreach ($entries as $entry) {
				$entry = trim($entry);
				if ($entry === '')
					continue;

				if (preg_match('/[\x00-\x1F\x7F]/', $entry)) {
					$validation_errors[] = sprintf(
						_('Batch allocation "%s" for item %s contains invalid control characters.'),
						$entry,
						$stock_id
					);
					continue;
				}

				$batch_id_raw = $entry;
				$qty_raw = $dispatch_qty;

				if (strpos($entry, ':') !== false) {
					$parts = explode(':', $entry, 2);
					$batch_id_raw = trim($parts[0]);
					$qty_raw = trim($parts[1]);
				}

				if ($batch_id_raw === '' || !preg_match('/^\d+$/', $batch_id_raw)) {
					$validation_errors[] = sprintf(
						_('Batch allocation "%s" for item %s has invalid batch id.'),
						$entry,
						$stock_id
					);
					continue;
				}

				$batch_id = (int)$batch_id_raw;
				if ($batch_id <= 0) {
					$validation_errors[] = sprintf(
						_('Batch allocation "%s" for item %s has invalid batch id.'),
						$entry,
						$stock_id
					);
					continue;
				}

				if (!is_scalar($qty_raw) || !is_valid_finite_numeric($qty_raw)) {
					$validation_errors[] = sprintf(
						_('Batch allocation "%s" for item %s has invalid quantity.'),
						$entry,
						$stock_id
					);
					continue;
				}

				$batch_qty = (float)$qty_raw;
				if ($batch_qty <= 0) {
					$validation_errors[] = sprintf(
						_('Batch allocation "%s" for item %s must have quantity greater than zero.'),
						$entry,
						$stock_id
					);
					continue;
				}

				if (isset($line_batch_ids[$batch_id])) {
					$validation_errors[] = sprintf(
						_('Batch id %d is duplicated in delivery payload for item %s.'),
						$batch_id,
						$stock_id
					);
					continue;
				}

				$batch = get_stock_batch($batch_id);
				if (!$batch) {
					$validation_errors[] = sprintf(
						_('Batch id %d was not found for item %s.'),
						$batch_id,
						$stock_id
					);
					continue;
				}

				if (!isset($batch['stock_id']) || $batch['stock_id'] !== $stock_id) {
					$validation_errors[] = sprintf(
						_('Batch id %d does not belong to item %s.'),
						$batch_id,
						$stock_id
					);
					continue;
				}

				$available_qty = (float)get_batch_quantity_on_hand($batch_id, $location);
				if ($batch_qty > $available_qty + 0.000001) {
					$validation_errors[] = sprintf(
						_('Batch id %d for item %s has only %.2f available at location %s, requested %.2f.'),
						$batch_id,
						$stock_id,
						$available_qty,
						$location,
						$batch_qty
					);
					continue;
				}

				$line_batch_ids[$batch_id] = true;
				$total_batch_qty += $batch_qty;
			}

			if (abs($total_batch_qty - $dispatch_qty) > 0.000001) {
				$validation_errors[] = sprintf(
					_('Item %s batch allocations must total %.2f; received %.2f.'),
					$stock_id,
					$dispatch_qty,
					$total_batch_qty
				);
			}
		}
	}

	return empty($validation_errors);
}

/**
 * Process credit note serial/batch returns: marks serials as returned,
 * reverses batch movements, and updates bin stock.
 *
 * Called from write_credit_note() for Return-type credits.
 *
 * @param object $credit_note  The credit note cart object
 * @param int    $credit_no    Credit note number
 * @param string $date_        Transaction date (display format)
 * @param string $location     Location code
 */
function process_credit_tracking_returns($credit_note, $credit_no, $date_, $location) {
	$root = dirname(dirname(dirname(__FILE__))); // → inventory/
	include_once($root . '/includes/db/serial_batch_db.inc');
	include_once($root . '/includes/db/serial_numbers_db.inc');
	include_once($root . '/includes/db/stock_batches_db.inc');

	foreach ($credit_note->line_items as $line_no => $credit_line) {
		if ($credit_line->qty_dispatched == 0)
			continue;

		$stock_id = $credit_line->stock_id;
		$qty = $credit_line->qty_dispatched;
		$tracking_mode = get_item_tracking_mode($stock_id);

		if ($tracking_mode === 'none')
			continue;

		// --- Serial Return Processing ---
		if (item_has_serial_tracking($stock_id)) {
			$serial_data_key = 'credit_serial_' . $line_no;
			$serial_text = isset($_POST[$serial_data_key]) ? trim($_POST[$serial_data_key]) : '';

			if (!empty($serial_text)) {
				$serial_entries = preg_split('/[\r\n,]+/', $serial_text);
				foreach ($serial_entries as $sn) {
					$sn = trim($sn);
					if (empty($sn)) continue;

					$serial = get_serial_number_by_code($sn, $stock_id);
					if ($serial && $serial['status'] === 'delivered') {
						$sid = (int)$serial['id'];

						update_serial_status(
							$sid,
							'returned',
							ST_CUSTCREDIT,
							$credit_no,
							'',           // from_loc (customer)
							$location,    // to_loc
							$date_,
							$credit_note->reference,
							sprintf(_('Returned via Credit Note #%d'), $credit_no)
						);

						// Clear customer from serial
						$sql = "UPDATE " . TB_PREF . "serial_numbers SET "
							. "customer_id=NULL"
							. " WHERE id=" . (int)$sid;
						db_query($sql, 'could not clear serial customer on return');

						// Restore bin stock if WMS
						$wms_settings = get_wms_location_settings($location);
						if ($wms_settings && !empty($serial['wh_loc_id'])) {
							update_bin_stock($serial['wh_loc_id'], $stock_id, 1,
								!empty($serial['batch_id']) ? (int)$serial['batch_id'] : null,
								$sid);
						}
					}
				}
			}
		}

		// --- Batch Return Processing ---
		if (item_has_batch_tracking($stock_id)) {
			$batch_data_key = 'credit_batch_' . $line_no;
			$batch_input = isset($_POST[$batch_data_key]) ? trim($_POST[$batch_data_key]) : '';

			if (!empty($batch_input)) {
				$batch_entries = preg_split('/[\r\n;]+/', $batch_input);
				$remaining_qty = $qty;

				foreach ($batch_entries as $entry) {
					$entry = trim($entry);
					if (empty($entry) || $remaining_qty <= 0) continue;

					if (strpos($entry, ':') !== false) {
						list($bid, $bqty) = explode(':', $entry, 2);
						$bid = (int)trim($bid);
						$bqty = (float)trim($bqty);
					} else {
						$bid = (int)$entry;
						$bqty = $remaining_qty;
					}

					if ($bid > 0 && $bqty > 0) {
						$alloc_qty = min($bqty, $remaining_qty);
						$remaining_qty -= $alloc_qty;

						// Log batch return movement (positive = inbound)
						add_batch_movement(
							$bid,
							ST_CUSTCREDIT,
							$credit_no,
							$location,
							null,
							$alloc_qty,
							$date_,
							$credit_note->reference,
							sprintf(_('Returned via Credit Note #%d'), $credit_no)
						);

						// Restore bin stock if WMS
						$wms_settings = get_wms_location_settings($location);
						if ($wms_settings) {
							$putaway_bin = get_putaway_bin_for_item($stock_id, $location, $alloc_qty);
							if ($putaway_bin) {
								$batch_data = get_stock_batch($bid);
								$expiry = $batch_data ? $batch_data['expiry_date'] : null;
								update_bin_stock($putaway_bin, $stock_id, $alloc_qty,
									$bid, null, $expiry);
							}
						}
					}
				}
			}
		}
	}
}

/**
 * Process supplier credit serial/batch tracking: reverses serial statuses,
 * adjusts batch quantities, and updates bin stock for WMS locations.
 *
 * Called from set_grn_item_credited() in grn_db.inc when processing supplier credits.
 *
 * @param object $entered_grn  The GRN line item being credited
 * @param int    $transno      Supplier credit transaction number
 * @param string $date_        Transaction date (display format)
 * @param string $location     Location code
 * @param array  $grn_data     The GRN+grn_items row data
 */
function process_supp_credit_tracking($entered_grn, $transno, $date_, $location, $grn_data) {
	$root = dirname(dirname(dirname(__FILE__))); // → inventory/
	include_once($root . '/includes/db/serial_batch_db.inc');
	include_once($root . '/includes/db/serial_numbers_db.inc');
	include_once($root . '/includes/db/stock_batches_db.inc');

	$stock_id = $entered_grn->item_code;
	$qty = abs($entered_grn->this_quantity_inv);
	$tracking_mode = get_item_tracking_mode($stock_id);

	if ($tracking_mode === 'none')
		return;

	// --- Serial Return to Supplier ---
	if (item_has_serial_tracking($stock_id)) {
		$serial_ids = array();
		if (isset($grn_data['serial_numbers_json']) && !empty($grn_data['serial_numbers_json'])) {
			$serial_ids = json_decode($grn_data['serial_numbers_json'], true);
		}
		// Also check POST for dynamically entered serial returns
		$serial_key = 'supp_credit_serial_' . $entered_grn->id;
		$serial_text = isset($_POST[$serial_key]) ? trim($_POST[$serial_key]) : '';
		if (!empty($serial_text)) {
			$entries = preg_split('/[\r\n,]+/', $serial_text);
			foreach ($entries as $sn) {
				$sn = trim($sn);
				if (empty($sn)) continue;
				$serial = get_serial_number_by_code($sn, $stock_id);
				if ($serial) {
					$serial_ids[] = (int)$serial['id'];
				}
			}
		}

		if (!empty($serial_ids) && is_array($serial_ids)) {
			$returned = 0;
			foreach ($serial_ids as $sid) {
				if ($returned >= $qty) break;
				$sid = (int)$sid;
				$serial = get_serial_number($sid);
				if (!$serial || $serial['status'] === 'scrapped') continue;

				update_serial_status(
					$sid,
					'scrapped',
					ST_SUPPCREDIT,
					$transno,
					$location,
					'',
					$date_,
					'',
					sprintf(_('Returned to supplier via Credit #%d'), $transno)
				);

				// Remove from bin stock if WMS
				$wms_settings = get_wms_location_settings($location);
				if ($wms_settings && !empty($serial['wh_loc_id'])) {
					update_bin_stock($serial['wh_loc_id'], $stock_id, -1,
						!empty($serial['batch_id']) ? (int)$serial['batch_id'] : null,
						$sid);
				}
				$returned++;
			}
		}
	}

	// --- Batch Return to Supplier ---
	if (item_has_batch_tracking($stock_id)) {
		$batch_id = isset($grn_data['batch_id']) ? (int)$grn_data['batch_id'] : 0;

		if ($batch_id > 0) {
			// Log batch return movement (negative = outbound to supplier)
			add_batch_movement(
				$batch_id,
				ST_SUPPCREDIT,
				$transno,
				$location,
				null,
				-$qty,
				$date_,
				'',
				sprintf(_('Returned to supplier via Credit #%d'), $transno)
			);

			// Update bin stock if WMS
			$wms_settings = get_wms_location_settings($location);
			if ($wms_settings) {
				// Find bin with this batch stock and deduct
				$sql = "SELECT bs.wh_loc_id, bs.qty_available FROM " . TB_PREF . "wh_bin_stock bs "
					. "INNER JOIN " . TB_PREF . "wh_locations wl ON wl.loc_id = bs.wh_loc_id "
					. "WHERE bs.stock_id = " . db_escape($stock_id)
					. " AND bs.batch_id = " . (int)$batch_id
					. " AND bs.qty_available > 0"
					. " ORDER BY bs.id ASC";
				$bin_result = db_query($sql, 'could not get batch bin stock');
				$remaining = $qty;
				while ($bin_row = db_fetch($bin_result)) {
					if ($remaining <= 0) break;
					$deduct = min($remaining, (float)$bin_row['qty_available']);
					update_bin_stock($bin_row['wh_loc_id'], $stock_id, -$deduct, $batch_id, null);
					$remaining -= $deduct;
				}
			}
		}
	}
}

/**
 * Process GRN removal serial/batch tracking: reverses serial/batch records
 * when uninvoiced GRN quantities are removed.
 *
 * Called from remove_not_invoice_item() in invoice_db.inc.
 *
 * @param array $grn_item  The GRN item row (from get_grn_item_detail)
 * @param array $grn       The GRN batch row
 * @param float $qty       Quantity being removed (negative)
 */
function process_grn_removal_tracking($grn_item, $grn, $qty) {
	$root = dirname(dirname(dirname(__FILE__))); // → inventory/
	include_once($root . '/includes/db/serial_batch_db.inc');
	include_once($root . '/includes/db/serial_numbers_db.inc');
	include_once($root . '/includes/db/stock_batches_db.inc');

	$stock_id = $grn_item['item_code'];
	$location = $grn['loc_code'];
	$abs_qty = abs($qty);
	$date_ = sql2date($grn['delivery_date']);
	$tracking_mode = get_item_tracking_mode($stock_id);

	if ($tracking_mode === 'none')
		return;

	// --- Serial Removal ---
	if (item_has_serial_tracking($stock_id)) {
		$serial_ids = array();
		if (isset($grn_item['serial_numbers_json']) && !empty($grn_item['serial_numbers_json'])) {
			$serial_ids = json_decode($grn_item['serial_numbers_json'], true);
		}

		if (!empty($serial_ids) && is_array($serial_ids)) {
			$removed = 0;
			foreach ($serial_ids as $sid) {
				if ($removed >= $abs_qty) break;
				$sid = (int)$sid;
				$serial = get_serial_number($sid);
				if (!$serial || $serial['status'] !== 'available') continue;

				update_serial_status(
					$sid,
					'scrapped',
					ST_SUPPRECEIVE,
					$grn_item['grn_batch_id'],
					$location,
					'',
					$date_,
					'',
					sprintf(_('Removed via GRN removal (GRN #%d)'), $grn_item['grn_batch_id'])
				);

				// Remove from bin stock if WMS
				$wms_settings = get_wms_location_settings($location);
				if ($wms_settings && !empty($serial['wh_loc_id'])) {
					update_bin_stock($serial['wh_loc_id'], $stock_id, -1,
						!empty($serial['batch_id']) ? (int)$serial['batch_id'] : null,
						$sid);
				}
				$removed++;
			}
		}
	}

	// --- Batch Removal ---
	if (item_has_batch_tracking($stock_id)) {
		$batch_id = isset($grn_item['batch_id']) ? (int)$grn_item['batch_id'] : 0;

		if ($batch_id > 0) {
			add_batch_movement(
				$batch_id,
				ST_SUPPRECEIVE,
				$grn_item['grn_batch_id'],
				$location,
				null,
				-$abs_qty,
				$date_,
				'',
				sprintf(_('Removed via GRN removal (GRN #%d)'), $grn_item['grn_batch_id'])
			);

			// Update bin stock if WMS
			$wms_settings = get_wms_location_settings($location);
			if ($wms_settings) {
				$sql = "SELECT bs.wh_loc_id, bs.qty_available FROM " . TB_PREF . "wh_bin_stock bs "
					. "INNER JOIN " . TB_PREF . "wh_locations wl ON wl.loc_id = bs.wh_loc_id "
					. "WHERE bs.stock_id = " . db_escape($stock_id)
					. " AND bs.batch_id = " . (int)$batch_id
					. " AND bs.qty_available > 0"
					. " ORDER BY bs.id ASC";
				$bin_result = db_query($sql, 'could not get batch bin stock');
				$remaining = $abs_qty;
				while ($bin_row = db_fetch($bin_result)) {
					if ($remaining <= 0) break;
					$deduct = min($remaining, (float)$bin_row['qty_available']);
					update_bin_stock($bin_row['wh_loc_id'], $stock_id, -$deduct, $batch_id, null);
					$remaining -= $deduct;
				}
			}
		}
	}
}
