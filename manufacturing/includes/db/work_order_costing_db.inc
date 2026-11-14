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

require_once __DIR__ . '/manufacturing_cost_classification_db.inc';

function add_wo_costing($workorder_id, $cost_type, $trans_type, $trans_no, $factor=1) {
	$sql = "INSERT INTO ".TB_PREF."wo_costing (workorder_id, cost_type, trans_type, trans_no, factor)
		VALUES (".db_escape($workorder_id).","
		.db_escape($cost_type).",".db_escape($trans_type).","
		.db_escape($trans_no).",".db_escape($factor).")";

	db_query($sql, 'could not add work order costing');
}

function get_wo_costing($workorder_id) {
	$sql="SELECT * 
		FROM ".TB_PREF."wo_costing cost,
			".TB_PREF."journal gl
		WHERE
			cost.trans_type=gl.type AND cost.trans_no=gl.trans_no
			AND workorder_id=".db_escape($workorder_id);

	return db_query($sql, 'could not get work order costing');
}

function delete_wo_costing($trans_type, $trans_no) {
	$sql="DELETE FROM ".TB_PREF."wo_costing WHERE trans_type=".db_escape($trans_type)
		." AND trans_no=".db_escape($trans_no);

	db_query($sql, 'could not delete work order costing');
}

/*
	Updates average material cost for item.
*/
function update_material_cost($stock_id, $qty, $unit_cost, $date) {
	global $Refs;

	$item = stock_master_entity::find($stock_id);
	$avg_cost =  $item['material_cost'];

	$qoh = get_qoh_on_date($stock_id, null, $date);

	// unit_cost*qty is already posted on inventory GL
 
	// if ($qoh > 0)
	// {
	// 	 if ($qty<0)
	// 		$qty_done = max(-$qoh, $qty);
	// 	 else
	// 		$qty_done = $qty;
	// } else {
	// 	if ($qty>0)
	// 		$qty_done = max($qoh+$qty, 0);
	// 	else
	// 		$qty_done = 0;
	// }
	// $qty_delayed = $qty-$qty_done;

	if ($qoh < 0)
		$qty_delayed = min(-$qoh, $qty);
	else
		$qty_delayed = 0;
	
	$qty_new = $qty - $qty_delayed;

	if ($qty_delayed > 0 && ($unit_cost != $avg_cost)) {
		// post	$qty_delayed*($avg_cost-$unit_cost)
		$diff = $qty_delayed*($unit_cost-$avg_cost);

		$dec = user_price_dec();
		$old_cost = -round2($avg_cost, $dec);
		$new_cost = -round2($unit_cost, $dec);

		$cart = new items_cart(ST_COSTUPDATE);
		$cart->tran_date = $cart->doc_date = $cart->event_date = $date;

		if (!is_date_in_fiscalyear($cart->tran_date))
			$cart->tran_date = end_fiscalyear();

		$cart->reference = $Refs->get_next(ST_COSTUPDATE, null, $cart->tran_date);

		$cart->memo_ = sprintf(_("COGS changed from %s to %s for %d %s of '%s'"), $avg_cost, $unit_cost, $qty_delayed, $item['units'], $stock_id);

		// DIM-MFG-001: a negative-stock cost correction keeps its source lineage. The work order that
		// triggers it publishes its frozen pair as the running context, which outranks the finished
		// item's current default; an install without the companion schema keeps the legacy pair.
		$cost_dim1 = $item['dimension_id'];
		$cost_dim2 = $item['dimension2_id'];
		$classified = inventory_classification_resolve_correction(ST_COSTUPDATE, $stock_id, null,
			array('cost_source' => 'work_order_negative_stock', 'value_change' => inventory_classification_amount_text($diff)));
		if ($classified !== null) {
			if (!$classified['ok'])
				throw new DimensionFinancialReferenceWriteException(implode(' ', $classified['errors']));
			$cost_dim1 = (int)$classified['axes'][1]['value'];
			$cost_dim2 = (int)$classified['axes'][2]['value'];
		}

		$cart->add_gl_item($item['cogs_account'], $cost_dim1, $cost_dim2, -$diff);
		$cart->add_gl_item($item['inventory_account'], 0, 0, $diff);

		if (write_journal_entries($cart) !== false && $classified !== null)
			inventory_classification_record_correction(ST_COSTUPDATE, $cart->order_id, $cart->tran_date,
				$classified, $qty_delayed);
	}

	if ($qty > 0 && ($qoh != -$qty))
		$avg_cost = ($avg_cost*($qoh+$qty_delayed)+$unit_cost*$qty_new)/($qoh+$qty);

	$sql = "UPDATE ".TB_PREF."stock_master SET material_cost=".db_escape($avg_cost)."
			WHERE stock_id=".db_escape($stock_id);
	db_query($sql, 'The cost details for the inventory item could not be updated');
}

/*
	Create journal entry for WO related costs.
*/
function add_wo_costs_journal($wo_id, $amount, $cost_type, $cr_acc, $date, $dim1=null, $dim2=null, $memo=null, $ref=null) {
//-------- this should be done by single call to write_journal_entries() using items_cart()
	global $Refs, $wo_cost_types;

	begin_transaction();

	$journal_id = get_next_trans_no(ST_JOURNAL);

	$wo = get_work_order($wo_id);

	// DIM-MFG-001: the journal posts on the pair frozen on the work order (a caller pair that differs is
	// refused) and is snapshotted once before its first ledger row. No payroll labour is derived here.
	try {
		$cost_pair = manufacturing_cost_classification_cost_pair($wo_id, $wo['stock_id'], $date, $dim1, $dim2);
		manufacturing_cost_classification_capture_event(ST_JOURNAL, $journal_id, $date, $cost_pair['object'], $wo_id,
			array(array($wo['stock_id'], 1, array('cost_type' => (int)$cost_type, 'amount' => inventory_classification_amount_text($amount)))),
			'work_order_cost_journal', array('cost_type' => (int)$cost_type));
	} catch (DimensionFinancialReferenceWriteException $exception) {
		cancel_transaction();
		throw $exception;
	}
	$dim1 = $cost_pair[1];
	$dim2 = $cost_pair[2];

	if (!$ref) $ref = $Refs->get_next(ST_JOURNAL, null, $date);

	add_gl_trans_std_cost(ST_JOURNAL, $journal_id, $date, $cr_acc, $dim1, $dim2, $wo_cost_types[$cost_type], -$amount);

	$is_bank_to = is_bank_account($cr_acc);
	if ($is_bank_to) {
		add_bank_trans(ST_JOURNAL, $journal_id, $is_bank_to, '', $date, -$amount, PT_WORKORDER, $wo_id, get_company_currency(), 'Cannot insert a destination bank transaction');
	}
	add_journal(ST_JOURNAL, $journal_id, $amount, $date, get_company_currency(), $ref, '', 1, $date, $date);

	add_gl_trans_std_cost(ST_JOURNAL, $journal_id, $date, $wo['wip_account'], $dim1, $dim2,  $wo_cost_types[$cost_type], $amount);

	$wo = get_work_order($wo_id);
	$wip = $wo['wip_account'];

	$Refs->save(ST_JOURNAL, $journal_id, $ref);

	add_wo_costing($wo_id, $cost_type, ST_JOURNAL, $journal_id);

	add_comments(ST_JOURNAL, $journal_id, $date, $memo);
	add_audit_trail(ST_JOURNAL, $journal_id, $date);

	commit_transaction();
}

/*
	Process component usage: generate and post stock move, update average component cost.
*/
function work_order_production_gl($woid, $stock_id, $quantity, $date_, $rcv_no) {
	global $SysPrefs, $path_to_root;

	$memo = '';
	if ($SysPrefs->loc_notification() == 1) {
		include_once($path_to_root.'/sales/includes/cart_class.inc');
		include_once($path_to_root.'/inventory/includes/inventory_db.inc');
		$st_ids = array();
		$st_names = array();
		$st_num = array();
		$st_reorder = array();
	}
	
	$result = get_wo_requirements($woid);
	$bom_rows = array();
	while ($bom_row = db_fetch($result))
		$bom_rows[] = $bom_row;

	// DIM-MFG-001: bill-of-material consumption posts both legs on the pair frozen on the work order,
	// snapshotted once under the production receipt identity before the first stock or ledger row. A
	// refusal cancels the whole production; no companion schema keeps the legacy zero pair.
	$cost_pair = array(1 => 0, 2 => 0);
	try {
		$wo = get_work_order($woid);
		$cost_object = manufacturing_cost_classification_ensure_workorder($woid, $wo['stock_id'], $date_);
		$cost_lines = array();
		foreach ($bom_rows as $cost_row)
			$cost_lines[] = array($cost_row['stock_id'], $cost_row['units_req'] * $quantity,
				array('requirement' => (int)$cost_row['id']));
		manufacturing_cost_classification_capture_event(ST_MANURECEIVE, $rcv_no, $date_, $cost_object, $woid, $cost_lines,
			'work_order_bom_consumption', array('produced_quantity' => inventory_classification_amount_text($quantity)));
		if ($cost_object !== null)
			$cost_pair = array(1 => (int)$cost_object[1], 2 => (int)$cost_object[2]);
	} catch (DimensionFinancialReferenceWriteException $exception) {
		cancel_transaction();
		throw $exception;
	}

	// credit all the components
	$total_cost = 0;
	foreach ($bom_rows as $bom_item) {
		if ($SysPrefs->loc_notification() == 1 && is_inventory_item($bom_item['stock_id'])) {
			$line = new line_details($bom_item['stock_id'], $bom_item['units_req'] * $quantity, 0, 0, 0, 0, $bom_item['description']);
			$loc = calculate_reorder_level($bom_item['loc_code'], $line, $st_ids, $st_names, $st_num, $st_reorder); 
		}
		update_wo_requirement_issued($bom_item['id'], $bom_item['units_req'] * $quantity, $bom_item['ComponentCost']);

		// insert a -ve stock move for each item
		// --- Advanced Inventory: record serial/batch tracking in BOM component usage ---
		$extended = array();
		$serial_batch_db_path = $path_to_root . '/inventory/includes/db/serial_batch_db.inc';
		if (file_exists($serial_batch_db_path)) {
			include_once($serial_batch_db_path);
			$tracking_mode = get_item_tracking_mode($bom_item['stock_id']);
			if ($tracking_mode !== 'none') {
				// Look up batch/serial from the last issued stock move for this WO component
				$move_sql = "SELECT serial_id, batch_id, expiry_date FROM " . TB_PREF . "stock_moves "
					. "WHERE type=" . ST_MANUISSUE
					. " AND stock_id=" . db_escape($bom_item['stock_id'])
					. " AND trans_no IN (SELECT issue_no FROM " . TB_PREF . "wo_issues WHERE workorder_id=" . db_escape($woid) . ")"
					. " AND (serial_id IS NOT NULL OR batch_id IS NOT NULL)"
					. " ORDER BY trans_id DESC LIMIT 1";
				$move_result = db_query($move_sql, 'could not look up WO component tracking');
				$move_row = db_fetch($move_result);
				if ($move_row) {
					if ($move_row['serial_id'])
						$extended['serial_id'] = (int)$move_row['serial_id'];
					if ($move_row['batch_id'])
						$extended['batch_id'] = (int)$move_row['batch_id'];
					if ($move_row['expiry_date'])
						$extended['expiry_date'] = $move_row['expiry_date'];
				}
				$extended['stock_status'] = 'available';
			}
		}

		if (!empty($extended)) {
			add_stock_move_extended(ST_MANURECEIVE, $bom_item['stock_id'], $rcv_no,
				$bom_item['loc_code'], $date_, '',
				-$bom_item['units_req'] * $quantity, $bom_item['ComponentCost'], 0,
				$extended);
		} else {
			add_stock_move(ST_MANURECEIVE, $bom_item['stock_id'], $rcv_no,
				$bom_item['loc_code'], $date_, '',
				-$bom_item['units_req'] * $quantity, $bom_item['ComponentCost'], 0);
		}

		if (!is_service($bom_item['mb_flag']))
			$ivaccount = $bom_item['inventory_account'];
		else
			$ivaccount = $bom_item['cogs_account'];

		$memo = $date_.': '.$bom_item['units_req'].' * '.$bom_item['description'];

		$total_cost += add_gl_trans_std_cost(ST_MANURECEIVE, $rcv_no, $date_, $ivaccount, $cost_pair[1], $cost_pair[2], $memo, -$bom_item['ComponentCost'] * $bom_item['units_req'] * $quantity);
	}

	$wo = get_work_order($woid);
	add_gl_trans_std_cost(ST_MANURECEIVE, $rcv_no, $date_, $wo['wip_account'], $cost_pair[1], $cost_pair[2], $memo, -$total_cost);
	if ($SysPrefs->loc_notification() == 1 && count($st_ids) > 0)
		send_reorder_email($loc, $st_ids, $st_names, $st_num, $st_reorder);
}

function check_wo_costing($type, $trans_no) {
	$sql = "SELECT workorder_id FROM ".TB_PREF."wo_costing WHERE trans_type= ".db_escape($type)." AND trans_no=".db_escape($trans_no);
	$costs = db_query($sql, 'cannot check WO costing');

	if (!db_num_rows($costs))
		return 0;

	$wo = db_fetch($costs);
	return $wo['workorder_id'];
}

function void_wo_costing($wo_id) {
	global $Refs;

	$res = get_wo_costing($wo_id);

	while($row = db_fetch($res)) {
		$is_bank_to = is_bank_account($row['account']);
		if ($is_bank_to)
			void_bank_trans($row['trans_type'], $row['trans_no']);

		// void any related gl trans
		$date = Today();
		$type = $row['trans_type'];
		$trans_no = $row['trans_no'];
		$memo = sprintf(_("Voided WO #%s"), $wo_id);
		void_gl_trans($type, $trans_no, true);
		add_audit_trail($type, $trans_no, $date, $memo);
		add_voided_entry($type, $trans_no, $date, $memo);
		$Refs->restore_last($type, $trans_no);
	}

	$sql = "DELETE FROM ".TB_PREF."wo_costing WHERE workorder_id=".db_escape($wo_id);
	db_query($sql, 'could not delete work order costing');
}
