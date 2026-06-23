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
 * Normalize manual finished-serial input into a unique ordered list.
 *
 * @param string $serial_text Raw textarea input.
 * @return array
 */
function parse_work_order_production_serials($serial_text) {
	$serial_text = trim((string)$serial_text);
	if ($serial_text === '')
		return array();

	$entries = preg_split('/[\r\n,]+/', $serial_text);
	$serials = array();
	$seen = array();

	foreach ($entries as $entry) {
		$serial_no = trim($entry);
		if ($serial_no === '')
			continue;

		$serial_key = strtoupper($serial_no);
		if (isset($seen[$serial_key]))
			continue;

		$seen[$serial_key] = true;
		$serials[] = $serial_no;
	}

	return $serials;
}

/**
 * Validate finished-goods tracking input before production is posted.
 *
 * @param array $product Work order product row.
 * @param float $quantity Production quantity.
 * @param string $date_ Production date in display format.
 * @return bool
 */
function validate_work_order_production_tracking($product, $quantity, $date_) {
	global $path_to_root;

	$tracking_root = $path_to_root . '/inventory/includes/db';
	if (!file_exists($tracking_root . '/serial_batch_db.inc'))
		return true;

	include_once($tracking_root . '/serial_batch_db.inc');
	include_once($tracking_root . '/serial_numbers_db.inc');
	include_once($tracking_root . '/stock_batches_db.inc');

	$stock_id = isset($product['stock_id']) ? $product['stock_id'] : '';
	if ($stock_id === '')
		return true;

	$tracking_mode = get_item_tracking_mode($stock_id);
	if ($tracking_mode === 'none' || $quantity <= 0)
		return true;

	if (item_has_serial_tracking($stock_id)) {
		if ((float)(int)$quantity != (float)$quantity) {
			display_error(_('Serial-tracked finished goods require a whole-number production quantity.'));
			set_focus('quantity');
			return false;
		}

		$serials = parse_work_order_production_serials(isset($_POST['produce_serials']) ? $_POST['produce_serials'] : '');
		if (!empty($serials) && count($serials) != (int)$quantity) {
			display_error(_('The number of finished serial numbers must exactly match the production quantity.'));
			set_focus('produce_serials');
			return false;
		}

		$seen_serials = array();
		foreach ($serials as $serial_no) {
			$serial_key = strtoupper($serial_no);
			if (isset($seen_serials[$serial_key])) {
				display_error(sprintf(_('Finished serial number %s is entered more than once.'), $serial_no));
				set_focus('produce_serials');
				return false;
			}

			$seen_serials[$serial_key] = true;
			if (get_serial_number_by_code($serial_no, $stock_id)) {
				display_error(sprintf(_('Finished serial number %s already exists for this item.'), $serial_no));
				set_focus('produce_serials');
				return false;
			}
		}
	}

	if (item_has_batch_tracking($stock_id)) {
		$batch_no = isset($_POST['produce_batch_no']) ? trim($_POST['produce_batch_no']) : '';
		if ($batch_no !== '' && get_stock_batch_by_code($batch_no, $stock_id)) {
			display_error(sprintf(_('Batch/Lot number %s already exists for this item.'), $batch_no));
			set_focus('produce_batch_no');
			return false;
		}

		$batch_expiry = isset($_POST['produce_batch_expiry']) ? trim($_POST['produce_batch_expiry']) : '';
		if ($batch_expiry !== '') {
			if (!is_date($batch_expiry)) {
				display_error(_('The entered batch expiry date is invalid.'));
				set_focus('produce_batch_expiry');
				return false;
			}
			if (date_diff2($date_, $batch_expiry, 'd') > 0) {
				display_error(_('The batch expiry date cannot be before the manufacturing date.'));
				set_focus('produce_batch_expiry');
				return false;
			}
		}
	}

	return true;
}

/**
 * Attach pending component trace rows to the finished outputs for one production event.
 *
 * Existing pending rows are updated in place, and batch-only rows are split across
 * multiple finished serials when needed so component quantities are preserved without
 * cloning the full genealogy for every output serial.
 *
 * @param int $woid Work order id.
 * @param array $finished_serial_ids Finished serial ids created by this production event.
 * @param int|null $finished_batch_id Finished batch id created by this production event.
 * @param float $quantity Produced quantity.
 * @return void
 */
function assign_work_order_production_traceability($woid, $finished_serial_ids, $finished_batch_id, $quantity) {
	$woid = (int)$woid;
	$finished_batch_id = $finished_batch_id ? (int)$finished_batch_id : null;
	$serial_ids = array();

	if (is_array($finished_serial_ids)) {
		foreach ($finished_serial_ids as $serial_id) {
			$serial_id = (int)$serial_id;
			if ($serial_id > 0)
				$serial_ids[] = $serial_id;
		}
	}

	$serial_ids = array_values(array_unique($serial_ids));
	if ($woid <= 0 || (empty($serial_ids) && $finished_batch_id === null))
		return;

	$pending_sql = "SELECT id, component_stock_id, component_serial_id, component_batch_id, component_qty"
		. " FROM " . TB_PREF . "production_traceability"
		. " WHERE work_order_id=" . $woid
		. " AND finished_serial_id IS NULL AND finished_batch_id IS NULL"
		. " ORDER BY id";
	$pending_result = db_query($pending_sql, 'could not get pending production traceability rows');
	$pending_rows = array();
	while ($pending_row = db_fetch($pending_result))
		$pending_rows[] = $pending_row;

	if (empty($pending_rows))
		return;

	if (count($serial_ids) <= 1) {
		$sets = array();
		if (!empty($serial_ids))
			$sets[] = "finished_serial_id=" . (int)$serial_ids[0];
		if ($finished_batch_id !== null)
			$sets[] = "finished_batch_id=" . $finished_batch_id;

		if (!empty($sets)) {
			$sql = "UPDATE " . TB_PREF . "production_traceability SET " . implode(', ', $sets)
				. " WHERE work_order_id=" . $woid
				. " AND finished_serial_id IS NULL AND finished_batch_id IS NULL";
			db_query($sql, 'could not update production traceability rows');
		}
		return;
	}

	$serial_count = count($serial_ids);
	$serial_component_index = 0;
	foreach ($pending_rows as $pending_row) {
		$row_id = (int)$pending_row['id'];
		$component_qty = (float)$pending_row['component_qty'];
		$component_serial_id = !empty($pending_row['component_serial_id']) ? (int)$pending_row['component_serial_id'] : null;
		$component_batch_id = !empty($pending_row['component_batch_id']) ? (int)$pending_row['component_batch_id'] : null;

		if ($component_serial_id !== null) {
			$target_serial_id = (int)$serial_ids[$serial_component_index % $serial_count];
			$serial_component_index++;

			$sets = array("finished_serial_id=" . $target_serial_id);
			if ($finished_batch_id !== null)
				$sets[] = "finished_batch_id=" . $finished_batch_id;

			$sql = "UPDATE " . TB_PREF . "production_traceability SET " . implode(', ', $sets)
				. " WHERE id=" . $row_id;
			db_query($sql, 'could not assign serial component traceability');
			continue;
		}

		$remaining_qty = $component_qty;
		$per_serial_qty = $quantity != 0 ? round($component_qty / (float)$quantity, 6) : 0;
		for ($index = 0; $index < $serial_count; $index++) {
			$target_serial_id = (int)$serial_ids[$index];
			$allocated_qty = ($index == $serial_count - 1) ? $remaining_qty : $per_serial_qty;
			$remaining_qty = round($remaining_qty - $allocated_qty, 6);

			if (abs($allocated_qty) < 0.000001)
				continue;

			if ($index == 0) {
				$sets = array(
					"finished_serial_id=" . $target_serial_id,
					"component_qty=" . (float)$allocated_qty
				);
				if ($finished_batch_id !== null)
					$sets[] = "finished_batch_id=" . $finished_batch_id;

				$sql = "UPDATE " . TB_PREF . "production_traceability SET " . implode(', ', $sets)
					. " WHERE id=" . $row_id;
				db_query($sql, 'could not update split production traceability row');
				continue;
			}

			add_production_traceability(
				$woid,
				$target_serial_id,
				$finished_batch_id,
				$pending_row['component_stock_id'],
				$component_serial_id,
				$component_batch_id,
				$allocated_qty
			);
		}
	}
}

function work_order_produce($woid, $ref, $quantity, $date_, $memo, $close_wo) {
	global $Refs, $path_to_root;

	// FIXME: support for WO_UNASSEMBLY case
	begin_transaction();

	$args = func_get_args();
	$args = (object)array_combine(array('woid', 'ref', 'quantity', 'date_', 'memo','close_wo'), $args);
	$args->trans_no = 0;
	hook_db_prewrite($args, ST_MANURECEIVE);

	$product = get_work_order($woid);

	$date = date2sql($date_);

	$sql = "INSERT INTO ".TB_PREF."wo_manufacture (workorder_id, reference, quantity, date_) VALUES (".db_escape($woid).", ".db_escape($ref).", ".db_escape($quantity).", '$date')";

	db_query($sql, 'A work order manufacture could not be added');

	$id = db_insert_id();

	// insert -ve and update averaged component unit cost for BOM usage (in wo_requirements)
	work_order_production_gl($woid, $product['stock_id'], $quantity, $date_, $id);

	// update wo quantity and close wo if requested (or finished)
	$closed = work_order_update_finished_quantity($woid, $quantity, $close_wo);

	// unit_cost is known when WO is finished, then generate +ve for all items
	if ($closed) {
		// 1. calculate sums of material/labour/overhead costs

		// sum collected BOM material & labour costs (no way for separate overhead here for now - needs flag in bom or stock_master)
		$bom = get_wo_requirements($woid);
		$m_cost = $l_cost = 0;
		while ($component = db_fetch($bom)) {
			if (!is_service($component['mb_flag']))
				$m_cost += $component['unit_cost']*$component['units_issued'];
			else
				$l_cost += $component['unit_cost']*$component['units_issued'];
		}

		// add additional material issues
		$issues = get_additional_issues($woid);
		while ($issue = db_fetch($issues)) {
			if (!is_service($issue['mb_flag']))
				$m_cost += $issue['unit_cost']*$issue['qty_issued'];
			else
				$l_cost += $issue['unit_cost']*$issue['qty_issued'];
		}

		// and additional costs
		$o_cost = get_gl_wo_cost($woid, WO_OVERHEAD);
		$l_cost += get_gl_wo_cost($woid, WO_LABOUR);

		$total_cost = $o_cost + $m_cost + $l_cost;

		add_gl_trans_std_cost(ST_WORKORDER, $woid, $date_, $product['wip_account'], 0, 0, $memo, -$total_cost);

		add_gl_trans_std_cost(ST_WORKORDER, $woid, $date_, $product['inventory_account'], 0, 0, $memo, $total_cost);

		$unit_cost = $total_cost/($product['units_issued']+$quantity);

		// first update material cost
		update_material_cost($product['stock_id'], $product['units_issued']+$quantity, $unit_cost, $date_);

		add_stock_move(ST_WORKORDER, $product['stock_id'], $woid, $product['loc_code'], $date_, $ref, $product['units_issued']+$quantity, $unit_cost);
	}

	// --- Serial/Batch tracking for finished goods production ---
	$tracking_root = $path_to_root . '/inventory/includes/db';
	if (file_exists($tracking_root . '/serial_batch_db.inc')) {
		include_once($tracking_root . '/serial_batch_db.inc');
		include_once($tracking_root . '/serial_numbers_db.inc');
		include_once($tracking_root . '/stock_batches_db.inc');
		include_once($path_to_root . '/manufacturing/includes/db/production_traceability_db.inc');

		$tracking_mode = get_item_tracking_mode($product['stock_id']);
		$finished_serial_ids = array();
		$finished_batch_id = null;

		// Only process if producing (not unassembling, quantity > 0)
		if ($tracking_mode !== 'none' && $quantity > 0) {

			// --- Serial tracking for finished goods ---
			if (item_has_serial_tracking($product['stock_id'])) {
				$serial_text = isset($_POST['produce_serials']) ? trim($_POST['produce_serials']) : '';
				if (!empty($serial_text)) {
					$serial_entries = preg_split('/[\r\n,]+/', $serial_text);
					foreach ($serial_entries as $sn) {
						$sn = trim($sn);
						if (empty($sn)) continue;
						// Check for duplicates
						$existing = get_serial_number_by_code($sn, $product['stock_id']);
						if ($existing) continue;
						// Create new serial for produced item
						$sid = add_serial_number(
							$sn,
							$product['stock_id'],
							'available',
							$product['loc_code'],
							null, // wh_loc_id
							null, // batch_id (set below if both serial+batch)
							null, // supplier_id
							null, // purchase_date
							$date_, // manufacturing_date
							null, // expiry_date
							null, // warranty_start
							null, // warranty_end
							null, // warranty_type
							$closed ? $unit_cost : 0,
							null, // grn_id
							sprintf(_('Produced from WO #%d'), $woid)
						);
						if ($sid) {
							$finished_serial_ids[] = $sid;
							add_serial_movement($sid, ST_MANURECEIVE, $id,
								null, $product['loc_code'],
								'', 'available',
								$date_, $ref,
								sprintf(_('Produced from WO #%d'), $woid));
						}
					}
				} else {
					// Auto-generate serials
					for ($i = 0; $i < (int)$quantity; $i++) {
						$sn = generate_serial_number($product['stock_id']);
						$sid = add_serial_number(
							$sn,
							$product['stock_id'],
							'available',
							$product['loc_code'],
							null, null, null, null,
							$date_, null, null, null, null,
							$closed ? $unit_cost : 0,
							null,
							sprintf(_('Auto-generated from WO #%d'), $woid)
						);
						if ($sid) {
							$finished_serial_ids[] = $sid;
							add_serial_movement($sid, ST_MANURECEIVE, $id,
								null, $product['loc_code'],
								'', 'available',
								$date_, $ref,
								sprintf(_('Auto-produced from WO #%d'), $woid));
						}
					}
				}
			}

			// --- Batch tracking for finished goods ---
			if (item_has_batch_tracking($product['stock_id'])) {
				$batch_no_input = isset($_POST['produce_batch_no']) ? trim($_POST['produce_batch_no']) : '';
				$batch_expiry = isset($_POST['produce_batch_expiry']) ? trim($_POST['produce_batch_expiry']) : '';
				$batch_mfg = $date_;

				if (empty($batch_no_input)) {
					$batch_no_input = generate_batch_number($product['stock_id']);
				}

				// Calculate expiry from shelf life if not provided
				if (empty($batch_expiry)) {
					$item_data = stock_master_entity::find($product['stock_id']);
					if ($item_data && !empty($item_data['shelf_life_days'])) {
						$batch_expiry = add_days($date_, (int)$item_data['shelf_life_days']);
					}
				}

				$finished_batch_id = add_stock_batch(
					$batch_no_input,
					$product['stock_id'],
					'active',
					$batch_mfg,      // manufacturing_date
					$batch_expiry,    // expiry_date
					null, null, null, // best_before, retest, shelf_life_days
					null,             // supplier_id
					null,             // supplier_batch_no
					null,             // grn_id
					$woid,            // work_order_id
					$quantity,        // initial_qty
					null, null,       // country, certification
					sprintf(_('Produced from WO #%d'), $woid)
				);

				if ($finished_batch_id) {
					add_batch_movement($finished_batch_id, ST_MANURECEIVE, $id,
						$product['loc_code'], null, $quantity, $date_, $ref,
						sprintf(_('Produced from WO #%d'), $woid));

					// If we also generated serials, link them to the batch
					foreach ($finished_serial_ids as $fs_id) {
						$sql = "UPDATE " . TB_PREF . "serial_numbers SET batch_id=" . (int)$finished_batch_id
							. " WHERE id=" . (int)$fs_id;
						db_query($sql, 'could not link serial to batch');
					}
				}
			}

			if (!empty($finished_serial_ids) || $finished_batch_id) {
				assign_work_order_production_traceability($woid, $finished_serial_ids, $finished_batch_id, $quantity);
			}

			// --- QC check: if item requires quality inspection, create inspection record ---
			$item_data = stock_master_entity::find($product['stock_id']);
			if ($item_data && !empty($item_data['quality_inspection_required'])) {
				// Mark serials as quarantine pending inspection
				foreach ($finished_serial_ids as $fs_id) {
					update_serial_status(
						$fs_id,
						'quarantine',
						ST_MANURECEIVE,
						$id,
						$product['loc_code'],
						$product['loc_code'],
						$date_,
						$ref,
						_('Held for quality inspection after production')
					);
				}
			}
		}
	}

	if ($memo)
		add_comments(ST_MANURECEIVE, $id, $date_, $memo);

	$Refs->save(ST_MANURECEIVE, $id, $ref);
	add_audit_trail(ST_MANURECEIVE, $id, $date_, _('Production.'));

	$args->trans_no = $id;
	hook_db_postwrite($args, ST_MANURECEIVE);

	commit_transaction();
}

function get_work_order_produce($id) {
	$sql = "SELECT prod.*, wo.stock_id, item.description AS StockDescription, wo.closed
		FROM ".TB_PREF."wo_manufacture prod,"
		.TB_PREF."workorders wo,"
		.TB_PREF."stock_master item
		WHERE prod.workorder_id=wo.id
		AND item.stock_id=wo.stock_id
		AND prod.id=".db_escape($id);
	$result = db_query($sql, 'The work order production could not be retrieved');

	return db_fetch($result);
}

function get_work_order_productions($woid) {
	$sql = "SELECT * FROM ".TB_PREF."wo_manufacture WHERE workorder_id=".db_escape($woid)." ORDER BY id";
	return db_query($sql, 'The work order issues could not be retrieved');
}

function exists_work_order_produce($id) {
	$sql = "SELECT id FROM ".TB_PREF."wo_manufacture WHERE id=".db_escape($id);
	$result = db_query($sql, 'Cannot retreive a wo production');

	return (db_num_rows($result) > 0);
}

function check_void_wo_production($prod_no) {
	$prod = get_work_order_produce($prod_no);
	return $prod['closed'] ? $prod['workorder_id'] : 0;
}

function void_work_order_produce($type_no) {
	begin_transaction();
	hook_db_prevoid(ST_MANURECEIVE, $type_no);

	// Skip processing already voided entry i.e. explicitly voided
	$void_entry = get_voided_entry(ST_MANURECEIVE, $type_no);
	if ($void_entry)
		return;

	$prod = get_work_order_produce($type_no);

	if (work_order_is_closed($prod['workorder_id']))
		return;

	// deduct the quantity of this production from the parent work order
	work_order_update_finished_quantity($prod['workorder_id'], -$prod['quantity']);

	// clear the production record
	$sql = "UPDATE ".TB_PREF."wo_manufacture SET quantity=0 WHERE id=".db_escape($type_no);
	db_query($sql, 'Cannot void a wo production');

	void_gl_trans(ST_MANURECEIVE, $type_no);

	void_stock_move(ST_MANURECEIVE, $type_no);

	commit_transaction();
}
