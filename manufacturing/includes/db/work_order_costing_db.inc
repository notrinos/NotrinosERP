<?php
/**********************************************************************
    Copyright (C) FrontAccounting, LLC.
	Released under the terms of the GNU General Public License, GPL, 
	as published by the Free Software Foundation, either version 3 
	of the License, or (at your option) any later version.
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  
    See the License here <http://www.gnu.org/licenses/gpl-3.0.html>.
***********************************************************************/

function add_wo_costing($workorder_id, $cost_type, $trans_type, $trans_no, $factor=1)
{
	$sql = "INSERT INTO ".TB_PREF."wo_costing (workorder_id, cost_type, trans_type, trans_no, factor)
		VALUES (".db_escape($workorder_id).","
		.db_escape($cost_type).",".db_escape($trans_type).","
		.db_escape($trans_no).",".db_escape($factor).")";

	db_query($sql, "could not add work order costing");

}

function get_wo_costing($workorder_id)
{
	$sql="SELECT * 
		FROM ".TB_PREF."wo_costing cost,
			".TB_PREF."journal gl
		WHERE
			cost.trans_type=gl.type AND cost.trans_no=gl.trans_no
			AND workorder_id=".db_escape($workorder_id);

	return db_query($sql, "could not get work order costing");
}

function delete_wo_costing($trans_type, $trans_no)
{
	$sql="DELETE FROM ".TB_PREF."wo_costing WHERE trans_type=".db_escape($trans_type)
		." AND trans_no=".db_escape($trans_no);

	db_query($sql, "could not delete work order costing");
}

/*
	Updates average material cost for item.
*/
function update_material_cost($stock_id, $qty, $unit_cost, $date)
{
	global $Refs;

	$item = get_item($stock_id);
	$avg_cost =  $item['material_cost'];

	$qoh = get_qoh_on_date($stock_id, null, $date);

	// unit_cost*qty is already posted on inventory GL
/* 
	if ($qoh > 0)
	{
		 if ($qty<0)
			$qty_done = max(-$qoh, $qty);
		 else
			$qty_done = $qty;
	} else {
		if ($qty>0)
			$qty_done = max($qoh+$qty, 0);
		else
			$qty_done = 0;
	}
	$qty_delayed = $qty-$qty_done;
*/
	if ($qoh < 0)
	{
			$qty_delayed = min(-$qoh, $qty);
	} else {
			$qty_delayed = 0;
	}
	$qty_new = $qty - $qty_delayed;

	if ($qty_delayed > 0 && ($unit_cost != $avg_cost))
	{
		// post	$qty_delayed*($avg_cost-$unit_cost)
		$diff = $qty_delayed*($unit_cost-$avg_cost);
		$stock_gl_code = get_item($stock_id);

		$dec = user_price_dec();
		$old_cost = -round2($avg_cost, $dec);
		$new_cost = -round2($unit_cost, $dec);

		$cart = new items_cart(ST_COSTUPDATE);
		$cart->tran_date = $cart->doc_date = $cart->event_date = $date;

		if (!is_date_in_fiscalyear($cart->tran_date))
			$cart->tran_date = end_fiscalyear();

		$cart->reference = $Refs->get_next(ST_COSTUPDATE, null, $cart->tran_date);

		$cart->memo_ = sprintf(_("COGS changed from %s to %s for %d %s of '%s'"), $avg_cost, $unit_cost, $qty_delayed, $item['units'], $stock_id);

		$cart->add_gl_item($item["cogs_account"], $item["dimension_id"], $item["dimension2_id"], -$diff);
		$cart->add_gl_item($item["inventory_account"], 0, 0, $diff);

		write_journal_entries($cart);
	}

	if ($qty > 0 && ($qoh != -$qty))
        $avg_cost = ($avg_cost*($qoh+$qty_delayed)+$unit_cost*$qty_new)/($qoh+$qty);

	$sql = "UPDATE ".TB_PREF."stock_master SET material_cost=".db_escape($avg_cost)."
		    WHERE stock_id=".db_escape($stock_id);
	db_query($sql,"The cost details for the inventory item could not be updated");
}

/*
	Create journal entry for WO related costs.
*/
function add_wo_costs_journal($wo_id, $amount, $cost_type, $cr_acc, $date, $dim1=0, $dim2=0, $memo = null, $ref= null)
{
//-------- this should be done by single call to write_journal_entries() using items_cart()
//
	global $Refs, $wo_cost_types;

	begin_transaction();

    $journal_id = get_next_trans_no(ST_JOURNAL);

	$wo = get_work_order($wo_id);

    if (!$ref) $ref = $Refs->get_next(ST_JOURNAL, null, $date);

	add_gl_trans_std_cost(ST_JOURNAL, $journal_id, $date, $cr_acc,
		0, 0, $wo_cost_types[$cost_type], -$amount);

	$is_bank_to = is_bank_account($cr_acc);
	if ($is_bank_to)
	{
		add_bank_trans(ST_JOURNAL, $journal_id, $is_bank_to, "",
			$date, -$amount, PT_WORKORDER, $wo_id, get_company_currency(),
			"Cannot insert a destination bank transaction");
	}
	add_journal(ST_JOURNAL, $journal_id, $amount, $date, get_company_currency(), $ref, '', 1, $date, $date);

	add_gl_trans_std_cost(ST_JOURNAL, $journal_id, $date, $wo['wip_account'],
		$dim1, $dim2,  $wo_cost_types[$cost_type], $amount);

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
function work_order_production_gl($woid, $stock_id, $quantity, $date_, $rcv_no)
{
	global $SysPrefs, $path_to_root;

	$memo = "";
	if ($SysPrefs->loc_notification() == 1)
	{
		include_once($path_to_root . "/sales/includes/cart_class.inc");
		include_once($path_to_root . "/inventory/includes/inventory_db.inc");
		$st_ids = array();
		$st_names = array();
		$st_num = array();
		$st_reorder = array();
	}
	
	$result = get_wo_requirements($woid);

	// credit all the components
	$total_cost = 0;
	while ($bom_item = db_fetch($result))
	{
		if ($SysPrefs->loc_notification() == 1 && is_inventory_item($bom_item['stock_id']))
		{
			$line = new line_details($bom_item['stock_id'], $bom_item["units_req"] * $quantity, 0, 0, 0, 0, $bom_item['description']);
			$loc = calculate_reorder_level($bom_item["loc_code"], $line, $st_ids, $st_names, $st_num, $st_reorder); 
		}
		update_wo_requirement_issued($bom_item['id'], $bom_item["units_req"] * $quantity, $bom_item["ComponentCost"]);

		// insert a -ve stock move for each item
		add_stock_move(ST_MANURECEIVE, $bom_item["stock_id"], $rcv_no,
			$bom_item["loc_code"], $date_, "", -$bom_item["units_req"] * $quantity, $bom_item["ComponentCost"], 0);

		if (!is_service($bom_item["mb_flag"]))
			$ivaccount = $bom_item["inventory_account"];
		else
			$ivaccount = $bom_item["cogs_account"];

		$memo = $date_.": ".$bom_item["units_req"] ." * ".$bom_item["description"];

		$total_cost += add_gl_trans_std_cost(ST_MANURECEIVE, $rcv_no, $date_, $ivaccount, 0, 0,
			$memo, -$bom_item["ComponentCost"] * $bom_item["units_req"] * $quantity);
	}

	$wo = get_work_order($woid);
    add_gl_trans_std_cost(ST_MANURECEIVE, $rcv_no, $date_, $wo['wip_account'],
        0, 0, $memo, -$total_cost);
	if ($SysPrefs->loc_notification() == 1 && count($st_ids) > 0)
		send_reorder_email($loc, $st_ids, $st_names, $st_num, $st_reorder);
}

function check_wo_costing($type, $trans_no)
{
	$sql = "SELECT workorder_id FROM ".TB_PREF."wo_costing WHERE trans_type= ".db_escape($type)." AND trans_no=".db_escape($trans_no);
	$costs = db_query($sql, 'cannot check WO costing');

	if (!db_num_rows($costs))
		return 0;

	$wo = db_fetch($costs);
	return $wo['workorder_id'];
}

function void_wo_costing($wo_id)
{
	global $Refs;

	$res = get_wo_costing($wo_id);

	while($row = db_fetch($res))
	{
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
	db_query($sql, "could not delete work order costing");
}
