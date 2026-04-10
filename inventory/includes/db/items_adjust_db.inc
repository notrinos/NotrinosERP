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
						if ($serial && $serial['status'] === 'available') {
							$sid = (int)$serial['id'];

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
						}
					}
				}
			}
		}

		$line_no++;
	}
}
