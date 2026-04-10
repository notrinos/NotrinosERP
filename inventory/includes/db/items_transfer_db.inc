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

function add_stock_transfer($Items, $location_from, $location_to, $date_, $reference, $memo_) {
	global $SysPrefs, $path_to_root, $Refs;

	begin_transaction();
	$args = func_get_args();
	$args = (object)array_combine(array('Items', 'location_from', 'location_to', 'date_', 'reference', 'memo_'), $args);
	$args->trans_no = 0;
	hook_db_prewrite($args, ST_LOCTRANSFER);

	$transfer_id = get_next_trans_no(ST_LOCTRANSFER);

	if ($SysPrefs->loc_notification() == 1) {
		include_once($path_to_root.'/inventory/includes/inventory_db.inc');
		$st_ids = array();
		$st_names = array();
		$st_num = array();
		$st_reorder = array();
	}
	$line_no = 0;
	foreach ($Items as $line_item) {
		if ($SysPrefs->loc_notification() == 1)
			$loc = calculate_reorder_level($location_from, $line_item, $st_ids, $st_names, $st_num, $st_reorder);
		add_stock_transfer_item($transfer_id, $line_item->stock_id, $location_from, $location_to, $date_, $reference, $line_item->quantity);
		$line_no++;
	}

	// --- Serial/batch tracking for simple transfers ---
	process_transfer_tracking($Items, $transfer_id, $location_from, $location_to, $date_, $reference);

	add_comments(ST_LOCTRANSFER, $transfer_id, $date_, $memo_);

	$Refs->save(ST_LOCTRANSFER, $transfer_id, $reference);
	add_audit_trail(ST_LOCTRANSFER, $transfer_id, $date_);

	$args->trans_no = $transfer_id;
	hook_db_postwrite($args, ST_LOCTRANSFER);

	commit_transaction();
	
	if ($SysPrefs->loc_notification() == 1 && count($st_ids) > 0)
		send_reorder_email($loc, $st_ids, $st_names, $st_num, $st_reorder);

	return $transfer_id;
}

// add 2 stock_moves entries for a stock transfer
// $date_ is display date (not sql)
// std_cost is in HOME currency
// it seems the standard_cost field is not used at all

function add_stock_transfer_item($transfer_id, $stock_id, $location_from, $location_to, $date_, $reference, $quantity) {
	add_stock_move(ST_LOCTRANSFER, $stock_id, $transfer_id, $location_from, $date_, $reference, -$quantity, 0);
	add_stock_move(ST_LOCTRANSFER, $stock_id, $transfer_id, $location_to, $date_, $reference, $quantity, 0);
}

//
//	Get inventory transfer common data (currently this is still stored in stock_moves table).
//
function get_stock_transfer($trans_no) {
	// retrieve common data from any two from/to move records
	$sql = "SELECT loc_from.*, loc_to.*
	FROM
	(SELECT trans_no, type, tran_date, reference, move.loc_code as from_loc, loc.location_name as from_name
		FROM ".TB_PREF."stock_moves move
			LEFT JOIN ".TB_PREF."locations loc ON loc.loc_code=move.loc_code
		WHERE type=".ST_LOCTRANSFER." AND trans_no=".db_escape($trans_no). " AND qty<0 LIMIT 1) loc_from,

		(SELECT move.loc_code as to_loc, loc.location_name as to_name
		FROM ".TB_PREF."stock_moves move
			LEFT JOIN ".TB_PREF."locations loc ON loc.loc_code=move.loc_code
		WHERE type=".ST_LOCTRANSFER." AND trans_no=".db_escape($trans_no). " AND qty>0 LIMIT 1) loc_to";

	$result = db_query($sql, 'Could not get transfer common data');

	return db_fetch($result);
}

function get_stock_transfer_items($trans_no) {

	$result = get_stock_moves(ST_LOCTRANSFER, $trans_no);
	if (db_num_rows($result) == 0)
		return null;

	return $result;
}

function void_stock_transfer($type_no) {
	hook_db_prevoid(ST_LOCTRANSFER, $type_no);
	void_stock_move(ST_LOCTRANSFER, $type_no);
}

/**
 * Process serial/batch tracking for simple inventory transfers.
 *
 * Reads POST data (transfer_serial_{line_no}, transfer_batch_{line_no}) and
 * updates serial locations, logs batch movements, and updates bin stock.
 *
 * @param array  $items         Line items from cart
 * @param int    $transfer_id   Transaction number
 * @param string $location_from Source location code
 * @param string $location_to   Destination location code
 * @param string $date_         Transfer date (display format)
 * @param string $reference     Reference string
 */
function process_transfer_tracking($items, $transfer_id, $location_from, $location_to, $date_, $reference) {
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

		// --- Serial number processing ---
		if (item_has_serial_tracking($stock_id)) {
			$serial_key = 'transfer_serial_' . $line_no;
			$serial_text = isset($_POST[$serial_key]) ? trim($_POST[$serial_key]) : '';

			if (!empty($serial_text)) {
				$serial_entries = preg_split('/[\r\n,]+/', $serial_text);
				foreach ($serial_entries as $sn) {
					$sn = trim($sn);
					if (empty($sn)) continue;

					// Find the serial by number and item at source location
					$serial = get_serial_number_by_code($sn, $stock_id);
					if ($serial && $serial['status'] === 'available') {
						$sid = (int)$serial['id'];

						// Update serial location to destination
						$sql = "UPDATE " . TB_PREF . "serial_numbers SET "
							. "loc_code=" . db_escape($location_to)
							. " WHERE id=" . (int)$sid;
						db_query($sql, 'could not update serial location for transfer');

						// Log serial movement
						add_serial_movement(
							$sid,
							ST_LOCTRANSFER,
							$transfer_id,
							$location_from,
							$location_to,
							'available',
							'available',
							$date_,
							$reference,
							sprintf(_('Transferred from %s to %s'), $location_from, $location_to)
						);
					}
				}
			}
		}

		// --- Batch/Lot processing ---
		if (item_has_batch_tracking($stock_id)) {
			$batch_key = 'transfer_batch_' . $line_no;
			$batch_input = isset($_POST[$batch_key]) ? trim($_POST[$batch_key]) : '';

			if (!empty($batch_input)) {
				// Format: "batch_id:qty,batch_id:qty" or "batch_id" for full qty
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

						// Log outbound batch movement from source
						add_batch_movement(
							$bid,
							ST_LOCTRANSFER,
							$transfer_id,
							$location_from,
							null,
							-$alloc_qty,
							$date_,
							$reference,
							sprintf(_('Transfer out to %s'), $location_to)
						);

						// Log inbound batch movement at destination
						add_batch_movement(
							$bid,
							ST_LOCTRANSFER,
							$transfer_id,
							$location_to,
							null,
							$alloc_qty,
							$date_,
							$reference,
							sprintf(_('Transfer in from %s'), $location_from)
						);
					}
				}
			}
		}

		$line_no++;
	}
}
