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
 * Remove tracked batch quantity from warehouse bins for an adjustment.
 *
 * @param string $stock_id      Item code.
 * @param int    $batch_id      Batch id.
 * @param string $location_code Warehouse location code.
 * @param float  $qty           Quantity to remove.
 * @return float
 */
function adjustment_remove_batch_quantity_from_bin_stock($stock_id, $batch_id, $location_code, $qty) {
	if (!function_exists('update_bin_stock') || $stock_id === '' || !$batch_id
		|| $location_code === '' || (float)$qty <= 0)
		return 0;

	$remaining_qty = (float)$qty;
	$removed_qty = 0;

	$sql = "SELECT bs.wh_loc_id, bs.qty_on_hand, bs.stock_status
		FROM " . TB_PREF . "wh_bin_stock bs
		INNER JOIN " . TB_PREF . "wh_locations wl ON wl.loc_id = bs.wh_loc_id
		WHERE wl.warehouse_loc_code=" . db_escape($location_code)
			. " AND bs.stock_id=" . db_escape($stock_id)
			. " AND bs.batch_id=" . (int)$batch_id
			. " AND bs.qty_on_hand > 0
		ORDER BY CASE WHEN bs.stock_status='available' THEN 0 ELSE 1 END, bs.id";
	$result = db_query($sql, 'could not get tracked batch bin stock for adjustment');

	while (($row = db_fetch($result)) && $remaining_qty > 0) {
		$pick_qty = min($remaining_qty, (float)$row['qty_on_hand']);
		if ($pick_qty <= 0)
			continue;

		update_bin_stock(
			(int)$row['wh_loc_id'],
			$stock_id,
			-$pick_qty,
			(int)$batch_id,
			null,
			null,
			$row['stock_status']
		);
		$remaining_qty -= $pick_qty;
		$removed_qty += $pick_qty;
	}

	return $removed_qty;
}

/**
 * Validate tracked adjustment input before processing the document.
 *
 * @param array  $items         Adjustment cart line items.
 * @param string $location      Adjustment location code.
 * @param string $error_message Validation error output.
 * @return bool
 */
function validate_adjustment_tracking_input($items, $location, &$error_message) {
	global $path_to_root;

	include_once($path_to_root . '/inventory/includes/db/serial_batch_db.inc');
	include_once($path_to_root . '/inventory/includes/db/serial_numbers_db.inc');
	include_once($path_to_root . '/inventory/includes/db/stock_batches_db.inc');

	$error_message = '';
	$line_no = 0;

	foreach ($items as $line_item) {
		$stock_id = $line_item->stock_id;
		$qty = (float)$line_item->quantity;
		$required_qty = abs($qty);

		if (get_item_tracking_mode($stock_id) === 'none') {
			$line_no++;
			continue;
		}

		if (item_has_serial_tracking($stock_id)) {
			if (abs($required_qty - round($required_qty)) > 0.000001) {
				$error_message = sprintf(_('Serial-tracked item %s requires a whole-number quantity.'), $stock_id);
				return false;
			}

			$serial_key = ($qty >= 0 ? 'adj_serial_' : 'adj_scrap_serial_') . $line_no;
			$serial_text = isset($_POST[$serial_key]) ? trim($_POST[$serial_key]) : '';
			if ($serial_text === '') {
				$error_message = sprintf(_('Serial numbers are required for item %s.'), $stock_id);
				return false;
			}

			$serial_entries = preg_split('/[\r\n,]+/', $serial_text);
			$serial_entries = array_values(array_filter(array_map('trim', $serial_entries), 'strlen'));
			if (count($serial_entries) !== (int)round($required_qty)) {
				$error_message = sprintf(_('Item %s requires %d serial number(s).'), $stock_id, (int)round($required_qty));
				return false;
			}

			$seen_serials = array();
			foreach ($serial_entries as $serial_no) {
				if (preg_match('/[\x00-\x1F\x7F]/', $serial_no)) {
					$error_message = sprintf(_('Serial number for item %s contains unsupported control characters.'), $stock_id);
					return false;
				}
				if (isset($seen_serials[$serial_no])) {
					$error_message = sprintf(_('Duplicate serial number %s entered for item %s.'), $serial_no, $stock_id);
					return false;
				}
				$seen_serials[$serial_no] = true;

				$serial = get_serial_number_by_code($serial_no, $stock_id);
				if ($qty >= 0) {
					if ($serial) {
						$error_message = sprintf(_('Serial %s already exists for item %s.'), $serial_no, $stock_id);
						return false;
					}
				} else {
					if (!$serial || !in_array($serial['status'], array('available', 'quarantine')) || $serial['loc_code'] !== $location) {
						$error_message = sprintf(_('Serial %s is not available at location %s for item %s.'), $serial_no, $location, $stock_id);
						return false;
					}
				}
			}
		}

		if (item_has_batch_tracking($stock_id)) {
			if ($qty >= 0) {
				$batch_no = isset($_POST['adj_batch_no_' . $line_no]) ? trim($_POST['adj_batch_no_' . $line_no]) : '';
				if ($batch_no === '') {
					$error_message = sprintf(_('Batch number is required for item %s.'), $stock_id);
					return false;
				}
				if (preg_match('/[\x00-\x1F\x7F]/', $batch_no)) {
					$error_message = sprintf(_('Batch number for item %s contains unsupported control characters.'), $stock_id);
					return false;
				}
			} else {
				$batch_input = isset($_POST['adj_scrap_batch_' . $line_no]) ? trim($_POST['adj_scrap_batch_' . $line_no]) : '';
				if ($batch_input === '') {
					$error_message = sprintf(_('Batch allocation is required for item %s.'), $stock_id);
					return false;
				}

				$batch_entries = preg_split('/[\r\n;]+/', $batch_input);
				$remaining_qty = $required_qty;
				$allocated_qty = 0;
				$seen_batches = array();

				foreach ($batch_entries as $entry) {
					$entry = trim($entry);
					if ($entry === '')
						continue;

					if (strpos($entry, ':') !== false) {
						list($batch_id_raw, $batch_qty_raw) = explode(':', $entry, 2);
						$batch_id_raw = trim($batch_id_raw);
						$batch_qty_raw = trim($batch_qty_raw);
						if (!preg_match('/^\d+$/', $batch_id_raw) || !is_numeric($batch_qty_raw) || (float)$batch_qty_raw <= 0) {
							$error_message = sprintf(_('Invalid batch allocation entry "%s" for item %s.'), $entry, $stock_id);
							return false;
						}
						$batch_id = (int)$batch_id_raw;
						$batch_qty = (float)$batch_qty_raw;
					} else {
						if (!preg_match('/^\d+$/', $entry)) {
							$error_message = sprintf(_('Invalid batch allocation entry "%s" for item %s.'), $entry, $stock_id);
							return false;
						}
						$batch_id = (int)$entry;
						$batch_qty = $remaining_qty;
					}

					if (isset($seen_batches[$batch_id])) {
						$error_message = sprintf(_('Duplicate batch id %d entered for item %s.'), $batch_id, $stock_id);
						return false;
					}
					$seen_batches[$batch_id] = true;

					$batch = get_stock_batch($batch_id);
					if (!$batch || $batch['stock_id'] !== $stock_id) {
						$error_message = sprintf(_('Batch id %d is invalid for item %s.'), $batch_id, $stock_id);
						return false;
					}

					$alloc_qty = min($batch_qty, $remaining_qty);
					$allocated_qty += $alloc_qty;
					$remaining_qty -= $alloc_qty;
				}

				if ($allocated_qty <= 0 || abs($allocated_qty - $required_qty) > 0.000001) {
					$error_message = sprintf(_('Allocated batch quantity does not match adjustment quantity for item %s.'), $stock_id);
					return false;
				}
			}
		}

		$line_no++;
	}

	return true;
}

function add_stock_adjustment($items, $location, $date_, $reference, $memo_) {
	global $SysPrefs, $path_to_root, $Refs;

	begin_transaction();
	$args = func_get_args();
	$args = (object)array_combine(array('items', 'location', 'date_', 'reference', 'memo_'), $args);
	$args->trans_no = 0;
	hook_db_prewrite($args, ST_INVADJUST);

	$adj_id = get_next_trans_no(ST_INVADJUST);

	if ($SysPrefs->loc_notification() == 1) {
		include_once($path_to_root . '/inventory/includes/inventory_db.inc');
		$st_ids = array();
		$st_names = array();
		$st_num = array();
		$st_reorder = array();
	}
	$line_no = 0;
	foreach ($items as $line_item) {

		if ($SysPrefs->loc_notification() == 1 && $line_item->quantity < 0) {
			$tmp = $line_item->quantity;
			$line_item->quantity = -$line_item->quantity;	// calculate_reorder_level expect positive qty
			$loc = calculate_reorder_level($location, $line_item, $st_ids, $st_names, $st_num, $st_reorder);
			$line_item->quantity = $tmp;
		}

		add_stock_adjustment_item($adj_id, $line_item->stock_id, $location, $date_, $reference, $line_item->quantity, $line_item->standard_cost, $memo_);
		$line_no++;
	}

	// --- Serial/batch tracking for adjustments ---
	process_adjustment_tracking($items, $adj_id, $location, $date_, $reference);

	add_comments(ST_INVADJUST, $adj_id, $date_, $memo_);

	$Refs->save(ST_INVADJUST, $adj_id, $reference);
	add_audit_trail(ST_INVADJUST, $adj_id, $date_);

	$args->trans_no = $adj_id;
	hook_db_postwrite($args, ST_INVADJUST);
	commit_transaction();
	if ($SysPrefs->loc_notification() == 1 && count($st_ids) > 0)
		send_reorder_email($loc, $st_ids, $st_names, $st_num, $st_reorder);

	return $adj_id;
}

//-------------------------------------------------------------------------------------------------------------

function void_stock_adjustment($type_no) {
	hook_db_prevoid(ST_INVADJUST, $type_no);
	void_gl_trans(ST_INVADJUST, $type_no);
	void_stock_move(ST_INVADJUST, $type_no);
}

//-------------------------------------------------------------------------------------------------------------

function get_stock_adjustment_items($trans_no) {
	$result = get_stock_moves(ST_INVADJUST, $trans_no);

	if (db_num_rows($result) == 0)
		return null;

	return $result;
}

//--------------------------------------------------------------------------------------------------

function add_stock_adjustment_item($adj_id, $stock_id, $location, $date_, $reference, $quantity, $standard_cost, $memo_) {
	$mb_flag = get_mb_flag($stock_id);

	if (is_service($mb_flag))
		display_db_error("Cannot do inventory adjustment for Service item : $stock_id", '');

	update_average_material_cost(null, $stock_id, $standard_cost, $quantity, $date_);

	if (is_fixed_asset($mb_flag)) {
		$sql = "UPDATE ".TB_PREF."stock_master SET inactive = 1 WHERE stock_id = ".db_escape($stock_id);
		db_query($sql, 'The inactive flag for the fixed asset could not be updated');
	}

	add_stock_move(ST_INVADJUST, $stock_id, $adj_id, $location, $date_, $reference, $quantity, $standard_cost);

	$inv_value = $standard_cost * $quantity;
	$adj_value = $standard_cost * -($quantity);

	if (is_fixed_asset($mb_flag)) {
		// get the initial value of the fixed assset.
		$row = get_fixed_asset_move($stock_id, ST_SUPPRECEIVE);
		$inv_value = $row['price'] * $quantity;
		$adj_value = (-($row['price']) + $standard_cost) * $quantity;
	}

	if ($standard_cost > 0 || is_fixed_asset($mb_flag)) {

		$stock_gl_codes = get_stock_gl_code($stock_id);

		add_gl_trans_std_cost(ST_INVADJUST, $adj_id, $date_, $stock_gl_codes['adjustment_account'], $stock_gl_codes['dimension_id'], $stock_gl_codes['dimension2_id'], $memo_, $adj_value  );

		add_gl_trans_std_cost(ST_INVADJUST, $adj_id, $date_, $stock_gl_codes['inventory_account'], $stock_gl_codes['dimension_id'], $stock_gl_codes['dimension2_id'], $memo_, $inv_value);
	}

	if (is_fixed_asset($mb_flag)) {
		// Additional gl entry for fixed asset.
		$grn_act = get_company_pref('default_loss_on_asset_disposal_act');
		add_gl_trans_std_cost(ST_INVADJUST, $adj_id, $date_, $grn_act, $stock_gl_codes['dimension_id'], $stock_gl_codes['dimension2_id'], $memo_, ($standard_cost * -($quantity)));
	}
}

/**
 * Process serial/batch tracking for inventory adjustments.
 *
 * Positive adjustments: create new serial numbers or batch records.
 * Negative adjustments: mark existing serials as 'scrapped', log batch outbound movements.
 *
 * Reads POST data: adj_serial_{line_no}, adj_batch_no_{line_no}, adj_batch_expiry_{line_no},
 * adj_batch_mfg_{line_no}, adj_scrap_serial_{line_no}, adj_scrap_batch_{line_no}
 *
 * @param array  $items       Line items from cart
 * @param int    $adj_id      Adjustment transaction number
 * @param string $location    Location code
 * @param string $date_       Adjustment date (display format)
 * @param string $reference   Reference string
 */
function process_adjustment_tracking($items, $adj_id, $location, $date_, $reference) {
	global $path_to_root;

	include_once($path_to_root . '/inventory/includes/db/serial_batch_db.inc');
	include_once($path_to_root . '/inventory/includes/db/serial_numbers_db.inc');
	include_once($path_to_root . '/inventory/includes/db/stock_batches_db.inc');
	include_once($path_to_root . '/inventory/warehouse/includes/warehouse_operations.inc');

	$line_no = 0;
	foreach ($items as $line_item) {
		$stock_id = $line_item->stock_id;
		$qty = $line_item->quantity;
		$tracking_mode = get_item_tracking_mode($stock_id);

		if ($tracking_mode === 'none') {
			$line_no++;
			continue;
		}

		if ($qty > 0) {
			// === POSITIVE ADJUSTMENT: Create new serial/batch records ===
			$created_serial_ids = array();

			// --- Serial numbers: create new ---
			if (item_has_serial_tracking($stock_id)) {
				$serial_key = 'adj_serial_' . $line_no;
				$serial_text = isset($_POST[$serial_key]) ? trim($_POST[$serial_key]) : '';

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
							null,   // wh_loc_id
							null,   // batch_id
							null,   // supplier_id
							null,   // purchase_date
							null,   // manufacturing_date
							null,   // expiry_date
							null,   // warranty_start
							null,   // warranty_end
							null,   // warranty_type
							$line_item->standard_cost,
							null,   // grn_id
							sprintf(_('Created via adjustment #%d'), $adj_id)
						);

						if ($sid) {
							$created_serial_ids[] = (int)$sid;
							add_serial_movement(
								$sid,
								ST_INVADJUST,
								$adj_id,
								'',         // from_loc
								$location,  // to_loc
								'',
								'available',
								$date_,
								$reference,
								sprintf(_('Adjustment in #%d'), $adj_id)
							);
						}
					}
				}
			}

			// --- Batch: create new or add to existing ---
			if (item_has_batch_tracking($stock_id)) {
				$batch_no_key = 'adj_batch_no_' . $line_no;
				$batch_no = isset($_POST[$batch_no_key]) ? trim($_POST[$batch_no_key]) : '';
				$expiry_key = 'adj_batch_expiry_' . $line_no;
				$expiry_date = isset($_POST[$expiry_key]) ? $_POST[$expiry_key] : '';
				$mfg_key = 'adj_batch_mfg_' . $line_no;
				$mfg_date = isset($_POST[$mfg_key]) ? $_POST[$mfg_key] : '';

				if (!empty($batch_no)) {
					$existing = get_stock_batch_by_code($batch_no, $stock_id);
					if ($existing) {
						$batch_id = (int)$existing['id'];
					} else {
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
							$mfg_sql,
							$expiry_sql,
							null, null, null,
							null,   // supplier_id
							null,   // supplier_batch_no
							null,   // grn_id
							null,   // work_order_id
							$qty,
							null, null,
							sprintf(_('Created via adjustment #%d'), $adj_id)
						);
					}

					if ($batch_id) {
						if (!empty($created_serial_ids) && $tracking_mode === 'serial_batch') {
							$sql_link_serial_batch = "UPDATE " . TB_PREF . "serial_numbers"
								. " SET batch_id=" . (int)$batch_id
								. " WHERE id IN (" . implode(',', array_map('intval', $created_serial_ids)) . ")";
							db_query($sql_link_serial_batch, 'could not link adjusted serials to batch');
						}

						add_batch_movement(
							$batch_id,
							ST_INVADJUST,
							$adj_id,
							$location,
							null,
							$qty,
							$date_,
							$reference,
							sprintf(_('Adjustment in #%d'), $adj_id)
						);
					}
				}
			}

		} else {
			// === NEGATIVE ADJUSTMENT: Mark serials as scrapped, log batch outbound ===

			// --- Serial numbers: mark as scrapped ---
			if (item_has_serial_tracking($stock_id)) {
				$serial_key = 'adj_scrap_serial_' . $line_no;
				$serial_text = isset($_POST[$serial_key]) ? trim($_POST[$serial_key]) : '';

				if (!empty($serial_text)) {
					$serial_entries = preg_split('/[\r\n,]+/', $serial_text);
					foreach ($serial_entries as $sn) {
						$sn = trim($sn);
						if (empty($sn)) continue;

						$serial = get_serial_number_by_code($sn, $stock_id);
						if ($serial && in_array($serial['status'], array('available', 'quarantine'))) {
							$sid = (int)$serial['id'];

							if (!empty($serial['wh_loc_id']) && function_exists('update_bin_stock')) {
								update_bin_stock(
									(int)$serial['wh_loc_id'],
									$stock_id,
									-1,
									!empty($serial['batch_id']) ? (int)$serial['batch_id'] : null,
									$sid,
									null,
									$serial['status']
								);
							}

							update_serial_status(
								$sid,
								'scrapped',
								ST_INVADJUST,
								$adj_id,
								$location,
								'',
								$date_,
								$reference,
								sprintf(_('Scrapped via adjustment #%d'), $adj_id)
							);

							$sql_clear_bin = "UPDATE " . TB_PREF . "serial_numbers SET wh_loc_id=NULL WHERE id=" . $sid;
							db_query($sql_clear_bin, 'could not clear scrapped serial bin location');
						}
					}
				}
			}

			// --- Batch: log outbound movement ---
			if (item_has_batch_tracking($stock_id)) {
				$batch_key = 'adj_scrap_batch_' . $line_no;
				$batch_input = isset($_POST[$batch_key]) ? trim($_POST[$batch_key]) : '';

				if (!empty($batch_input)) {
					$batch_entries = preg_split('/[\r\n;]+/', $batch_input);
					$remaining_qty = abs($qty);

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

							add_batch_movement(
								$bid,
								ST_INVADJUST,
								$adj_id,
								$location,
								null,
								-$alloc_qty,
								$date_,
								$reference,
								sprintf(_('Scrapped via adjustment #%d'), $adj_id)
							);

							adjustment_remove_batch_quantity_from_bin_stock(
								$stock_id,
								$bid,
								$location,
								$alloc_qty
							);

							if (function_exists('get_batch_quantity_on_hand')
								&& function_exists('update_batch_status')
								&& get_batch_quantity_on_hand($bid) <= 0.000001) {
								update_batch_status($bid, 'scrapped', sprintf(_('Scrapped via adjustment #%d'), $adj_id));
							}
						}
					}
				}
			}
		}

		$line_no++;
	}
}
