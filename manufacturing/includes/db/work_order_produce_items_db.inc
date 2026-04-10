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
					$item_data = get_item($product['stock_id']);
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

			// --- Update traceability: link finished serials/batch to component records ---
			// Find existing traceability records for this WO (created during component issue)
			// and update them with the finished product identifiers
			if (!empty($finished_serial_ids) || $finished_batch_id) {
				$trace_sql = "SELECT id FROM " . TB_PREF . "production_traceability"
					. " WHERE work_order_id=" . (int)$woid
					. " AND finished_serial_id IS NULL AND finished_batch_id IS NULL";
				$trace_result = db_query($trace_sql, 'could not get traceability records');

				while ($trace_row = db_fetch($trace_result)) {
					$update_sets = array();
					if (!empty($finished_serial_ids)) {
						// Always link the first finished serial to existing records
						$update_sets[] = "finished_serial_id=" . (int)$finished_serial_ids[0];
					}
					if ($finished_batch_id) {
						$update_sets[] = "finished_batch_id=" . (int)$finished_batch_id;
					}
					if (!empty($update_sets)) {
						$sql = "UPDATE " . TB_PREF . "production_traceability SET "
							. implode(', ', $update_sets)
							. " WHERE id=" . (int)$trace_row['id'];
						db_query($sql, 'could not update traceability');
					}
				}

				// If multiple finished serials, we need to create additional traceability records
				// for each finished serial beyond the first (the first was updated in place)
				if (count($finished_serial_ids) > 1) {
					// Get component records for this WO
					$comp_sql = "SELECT DISTINCT component_stock_id, component_serial_id, component_batch_id, component_qty"
						. " FROM " . TB_PREF . "production_traceability"
						. " WHERE work_order_id=" . (int)$woid;
					$comp_result = db_query($comp_sql, 'could not get component records');
					$components = array();
					while ($cr = db_fetch($comp_result)) {
						$components[] = $cr;
					}

					// For each additional finished serial, duplicate the component links
					for ($i = 1; $i < count($finished_serial_ids); $i++) {
						foreach ($components as $comp) {
							add_production_traceability(
								$woid,
								$finished_serial_ids[$i],
								$finished_batch_id,
								$comp['component_stock_id'],
								!empty($comp['component_serial_id']) ? (int)$comp['component_serial_id'] : null,
								!empty($comp['component_batch_id']) ? (int)$comp['component_batch_id'] : null,
								(float)$comp['component_qty']
							);
						}
					}
				}
			}

			// --- QC check: if item requires quality inspection, create inspection record ---
			$item_data = get_item($product['stock_id']);
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
