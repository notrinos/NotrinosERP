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
//--------------------------------------------------------------------------------------

function add_work_order_issue($woid, $ref, $to_work_order, $items, $location, $workcentre, $date_, $memo_) {
	global $Refs, $path_to_root;

	begin_transaction();
	$args = func_get_args();
	$args = (object)array_combine(array('woid', 'ref', 'to_work_order', 'items', 'location', 
		'workcentre', 'date_', 'memo_'), $args);
	$args->trans_no = 0;
	hook_db_prewrite($args, ST_MANUISSUE);

	$sql = "INSERT INTO ".TB_PREF."wo_issues (workorder_id, reference, issue_date, loc_code, workcentre_id)
		VALUES (".db_escape($woid).", ".db_escape($ref).", '" .
		date2sql($date_) . "', ".db_escape($location).", ".db_escape($workcentre).")";
	db_query($sql, 'The work order issue could not be added');

	$number = db_insert_id();

	$issue_total = $total_cost = 0;

	$wo = get_work_order($woid);

	// Load tracking functions if available
	$tracking_root = $path_to_root . '/inventory/includes/db';
	$has_tracking = false;
	if (file_exists($tracking_root . '/serial_batch_db.inc')) {
		include_once($tracking_root . '/serial_batch_db.inc');
		include_once($tracking_root . '/serial_numbers_db.inc');
		include_once($tracking_root . '/stock_batches_db.inc');
		include_once($path_to_root . '/manufacturing/includes/db/production_traceability_db.inc');
		$has_tracking = true;
	}

	foreach ($items as $line_no => $item) {
		if ($to_work_order)	// otherwise it is material return to inventory
			$item->quantity = -$item->quantity;

		$unit_cost = get_unit_cost($item->stock_id);

		// --- Serial/Batch tracking for component issues ---
		$tracking_mode = $has_tracking ? get_item_tracking_mode($item->stock_id) : 'none';
		$component_serial_ids = array();
		$component_batch_ids = array();

		if ($has_tracking && $tracking_mode !== 'none' && !$to_work_order) {
			// Process serial numbers for this component
			if (item_has_serial_tracking($item->stock_id)) {
				$serial_key = 'issue_serial_' . $line_no;
				$serial_text = isset($_POST[$serial_key]) ? trim($_POST[$serial_key]) : '';
				if (!empty($serial_text)) {
					$serial_entries = preg_split('/[\r\n,]+/', $serial_text);
					foreach ($serial_entries as $sn) {
						$sn = trim($sn);
						if (empty($sn)) continue;
						$serial = get_serial_number_by_code($sn, $item->stock_id);
						if ($serial && $serial['status'] === 'available') {
							$sid = (int)$serial['id'];
							$component_serial_ids[] = $sid;
							update_serial_status(
								$sid,
								'in_production',
								ST_MANUISSUE,
								$number,
								$location,
								null,
								$date_,
								$ref,
								sprintf(_('Issued to WO #%d'), $woid)
							);
							// Create extended stock move per serial
							add_stock_move_extended(ST_MANUISSUE, $item->stock_id, $number, $location, $date_, $memo_,
								-1, $unit_cost, 0, array('serial_id' => $sid));
						}
					}
					// Remaining qty not covered by serials gets a plain move
					$serial_qty = count($component_serial_ids);
					$remaining = abs($item->quantity) - $serial_qty;
					if ($remaining > 0) {
						add_stock_move(ST_MANUISSUE, $item->stock_id, $number, $location, $date_, $memo_, -$remaining, $unit_cost);
					}
				} else {
					// No serials provided — plain stock move
					add_stock_move(ST_MANUISSUE, $item->stock_id, $number, $location, $date_, $memo_, -$item->quantity, $unit_cost);
				}
			}
			// Process batch allocations for this component
			elseif (item_has_batch_tracking($item->stock_id)) {
				$batch_key = 'issue_batch_' . $line_no;
				$batch_text = isset($_POST[$batch_key]) ? trim($_POST[$batch_key]) : '';
				$batch_qty_allocated = 0;

				if (!empty($batch_text)) {
					$batch_lines = preg_split('/[\r\n]+/', $batch_text);
					foreach ($batch_lines as $bl) {
						$bl = trim($bl);
						if (empty($bl)) continue;
						// Format: batch_id:qty
						$parts = explode(':', $bl, 2);
						if (count($parts) === 2) {
							$bid = (int)trim($parts[0]);
							$bqty = (float)trim($parts[1]);
							if ($bid > 0 && $bqty > 0) {
								$batch = get_stock_batch($bid);
								if ($batch) {
									$component_batch_ids[] = array('batch_id' => $bid, 'qty' => $bqty);
									$batch_qty_allocated += $bqty;
									add_batch_movement($bid, ST_MANUISSUE, $number, $location,
										null, -$bqty, $date_, $ref,
										sprintf(_('Issued to WO #%d'), $woid));
									add_stock_move_extended(ST_MANUISSUE, $item->stock_id, $number, $location, $date_, $memo_,
										-$bqty, $unit_cost, 0,
										array('batch_id' => $bid, 'expiry_date' => isset($batch['expiry_date']) ? $batch['expiry_date'] : null));
								}
							}
						}
					}
					// Remaining qty not covered by batches
					$remaining = abs($item->quantity) - $batch_qty_allocated;
					if ($remaining > 0) {
						add_stock_move(ST_MANUISSUE, $item->stock_id, $number, $location, $date_, $memo_, -$remaining, $unit_cost);
					}
				} else {
					// Auto-allocate batches via FEFO
					if (function_exists('get_fefo_batches')) {
						$fefo = get_fefo_batches($item->stock_id, $location, abs($item->quantity));
						$remaining_qty = abs($item->quantity);
						foreach ($fefo as $fb) {
							if ($remaining_qty <= 0) break;
							$alloc = min($remaining_qty, $fb['qty_available']);
							$component_batch_ids[] = array('batch_id' => (int)$fb['batch_id'], 'qty' => $alloc);
							add_batch_movement((int)$fb['batch_id'], ST_MANUISSUE, $number, $location,
								null, -$alloc, $date_, $ref,
								sprintf(_('Issued to WO #%d (auto-FEFO)'), $woid));
							add_stock_move_extended(ST_MANUISSUE, $item->stock_id, $number, $location, $date_, $memo_,
								-$alloc, $unit_cost, 0,
								array('batch_id' => (int)$fb['batch_id'], 'expiry_date' => isset($fb['expiry_date']) ? $fb['expiry_date'] : null));
							$remaining_qty -= $alloc;
						}
						if ($remaining_qty > 0) {
							add_stock_move(ST_MANUISSUE, $item->stock_id, $number, $location, $date_, $memo_, -$remaining_qty, $unit_cost);
						}
					} else {
						add_stock_move(ST_MANUISSUE, $item->stock_id, $number, $location, $date_, $memo_, -$item->quantity, $unit_cost);
					}
				}
			} else {
				// Tracked but neither serial nor batch (shouldn't happen, fallback)
				add_stock_move(ST_MANUISSUE, $item->stock_id, $number, $location, $date_, $memo_, -$item->quantity, $unit_cost);
			}

			// Record traceability — components linked to this WO (finished product link added during production receipt)
			foreach ($component_serial_ids as $csid) {
				add_production_traceability($woid, null, null, $item->stock_id, $csid, null, 1);
			}
			foreach ($component_batch_ids as $cb) {
				add_production_traceability($woid, null, null, $item->stock_id, null, $cb['batch_id'], $cb['qty']);
			}
		} else {
			// No tracking — original stock move
			add_stock_move(ST_MANUISSUE, $item->stock_id, $number, $location, $date_, $memo_, -$item->quantity, $unit_cost);
		}

		$sql = "INSERT INTO ".TB_PREF."wo_issue_items (issue_id, stock_id, qty_issued, unit_cost)
			SELECT ".db_escape($number).",".db_escape($item->stock_id).",".db_escape($item->quantity).", material_cost
			FROM ".TB_PREF."stock_master
			WHERE stock_id=".db_escape($item->stock_id);

		db_query($sql, 'A work order issue item could not be added');

		$unit_cost = get_unit_cost($item->stock_id);
		$issue_cost = $unit_cost * $item->quantity;

		$stockitem = get_item($item->stock_id);

		// Compatibility for Service Items
		if (!is_service($stockitem['mb_flag']))
			$ivaccount = $stockitem['inventory_account'];
		else
			$ivaccount = $stockitem['cogs_account'];

		$total_cost += add_gl_trans_std_cost(ST_MANUISSUE, $number, $date_, $ivaccount, 0, 0, $date_.': '._('Issue of').' '.$stockitem['description'], -$issue_cost);
	}

	$stockitem = get_item($wo['stock_id']);

	$wip_account = $stockitem['wip_account'];

	add_gl_trans_std_cost(ST_MANUISSUE, $number, $date_, $wip_account, 0, 0, $date_.': '._('Issue to').' '.$stockitem['description'], -$total_cost);

	if ($memo_)
		add_comments(ST_MANUISSUE, $number, $date_, $memo_);

	$Refs->save(ST_MANUISSUE, $number, $ref);
	add_audit_trail(ST_MANUISSUE, $number, $date_);

	$args->trans_no = $number;
	hook_db_postwrite($args, ST_MANUISSUE);

	commit_transaction();
}

//--------------------------------------------------------------------------------------

function get_work_order_issues($woid) {
	$sql = "SELECT * FROM ".TB_PREF."wo_issues WHERE workorder_id=".db_escape($woid)
	." ORDER BY issue_no";
	return db_query($sql, "The work order issues could not be retrieved");
}

function get_additional_issues($woid) {
	$sql = "SELECT issue.*, item.*, stock.mb_flag
		FROM ".TB_PREF."wo_issues issue, "
			.TB_PREF."wo_issue_items item
			LEFT JOIN ".TB_PREF."stock_master stock ON stock.stock_id=item.stock_id
		WHERE issue.issue_no=item.issue_id
		AND issue.workorder_id=".db_escape($woid)
		." ORDER BY item.id";
	return db_query($sql, 'The work order issues could not be retrieved');
}

//--------------------------------------------------------------------------------------

function get_work_order_issue($issue_no) {
	$sql = "SELECT DISTINCT issue.*, wo.stock_id, wo.closed,
		item.description, loc.location_name, center.name AS WorkCentreName
		FROM ".TB_PREF."wo_issues issue,"
			.TB_PREF."workorders wo,"
			.TB_PREF."stock_master item,"
			.TB_PREF."locations loc,"
			.TB_PREF."workcentres center
		WHERE issue_no=".db_escape($issue_no)."
		AND wo.id = issue.workorder_id
		AND loc.loc_code = issue.loc_code
		AND center.id = issue.workcentre_id
		AND item.stock_id = wo.stock_id";
	$result = db_query($sql, 'A work order issue could not be retrieved');

	return db_fetch($result);
}

//--------------------------------------------------------------------------------------

function get_work_order_issue_details($issue_no) {
	$sql = "SELECT issue.*, item.description, item.units
		FROM ".TB_PREF."wo_issue_items issue,"
			.TB_PREF."stock_master item
		WHERE issue_id=".db_escape($issue_no)."
		AND item.stock_id=issue.stock_id
		ORDER BY issue.id";
	return db_query($sql, 'The work order issue items could not be retrieved');
}

//--------------------------------------------------------------------------------------

function exists_work_order_issue($issue_no) {
	$sql = "SELECT issue_no FROM ".TB_PREF."wo_issues WHERE issue_no=".db_escape($issue_no);
	$result = db_query($sql, 'Cannot retreive a wo issue');

	return (db_num_rows($result) > 0);
}

//--------------------------------------------------------------------------------------

function check_void_wo_issue($issue_no) {
	$issue = get_work_order_issue($issue_no);

	return $issue['closed'] ? $issue['workorder_id'] : 0;
}

//--------------------------------------------------------------------------------------

function void_work_order_issue($type_no) {
	begin_transaction();
	hook_db_prevoid(ST_MANUISSUE, $type_no);

	$void_entry = get_voided_entry(ST_MANUISSUE, $type_no);
	if ($void_entry)
		return;

	if (check_void_wo_issue($type_no))
		return;

	// FIXME update issued material cost

	// void the actual issue items and their quantities
	$sql = "UPDATE ".TB_PREF."wo_issue_items SET qty_issued = 0 WHERE issue_id=".db_escape($type_no);

	db_query($sql, 'A work order issue item could not be voided');

	void_gl_trans(ST_MANUISSUE, $type_no);

	void_stock_move(ST_MANUISSUE, $type_no);

	commit_transaction();
}
