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
function work_order_produce($woid, $ref, $quantity, $date_, $memo, $close_wo)
{
	global $Refs;

// FIXME: support for WO_UNASSEMBLY case
	begin_transaction();

	$args = func_get_args();
	$args = (object)array_combine(array('woid', 'ref', 'quantity', 'date_', 'memo','close_wo'),
		$args);
	$args->trans_no = 0;
	hook_db_prewrite($args, ST_MANURECEIVE);

	$product = get_work_order($woid);

    $date = date2sql($date_);

    $sql = "INSERT INTO ".TB_PREF."wo_manufacture (workorder_id, reference, quantity, date_)
		VALUES (".db_escape($woid).", ".db_escape($ref).", ".db_escape($quantity)
		.", '$date')";

	db_query($sql,"A work order manufacture could not be added");

	$id = db_insert_id();

	// -------------------------------------------------------------------------
	// insert -ve and update averaged component unit cost for BOM usage (in wo_requirements)
	work_order_production_gl($woid, $product["stock_id"], $quantity, $date_, $id);

	// update wo quantity and close wo if requested (or finished)
	$closed = work_order_update_finished_quantity($woid, $quantity, $close_wo);

	// unit_cost is known when WO is finished, then generate +ve for all items
	if ($closed)
	{
		// 1. calculate sums of material/labour/overhead costs

		// sum collected BOM material & labour costs (no way for separate overhead here for now - needs flag in bom or stock_master)
		$bom = get_wo_requirements($woid);
		$m_cost = $l_cost = 0;
		while ($component = db_fetch($bom))
		{
			if (!is_service($component["mb_flag"]))
				$m_cost += $component['unit_cost']*$component['units_issued'];
			else
				$l_cost += $component['unit_cost']*$component['units_issued'];
		}

		// add additional material issues
		$issues = get_additional_issues($woid);
		while ($issue = db_fetch($issues))
		{
			if (!is_service($issue["mb_flag"]))
				$m_cost += $issue['unit_cost']*$issue['qty_issued'];
			else
				$l_cost += $issue['unit_cost']*$issue['qty_issued'];
		}

		// and additional costs
		$o_cost = get_gl_wo_cost($woid, WO_OVERHEAD);
		$l_cost += get_gl_wo_cost($woid, WO_LABOUR);

		$total_cost = $o_cost + $m_cost + $l_cost;

	    add_gl_trans_std_cost(ST_WORKORDER, $woid, $date_, $product['wip_account'],
    	    0, 0, $memo, -$total_cost);

	    add_gl_trans_std_cost(ST_WORKORDER, $woid, $date_, $product['inventory_account'],
	   	    0, 0, $memo, $total_cost);

		$unit_cost = $total_cost/($product['units_issued']+$quantity);

		// first update material cost
		update_material_cost($product['stock_id'], $product['units_issued']+$quantity, $unit_cost, $date_);

		add_stock_move(ST_WORKORDER, $product["stock_id"], $woid,
			$product["loc_code"], $date_, $ref, $product['units_issued']+$quantity, $unit_cost);
	}

	if ($memo)
		add_comments(ST_MANURECEIVE, $id, $date_, $memo);

	$Refs->save(ST_MANURECEIVE, $id, $ref);
	add_audit_trail(ST_MANURECEIVE, $id, $date_, _("Production."));

	$args->trans_no = $id;
	hook_db_postwrite($args, ST_MANURECEIVE);

	commit_transaction();
}

//--------------------------------------------------------------------------------------------

function get_work_order_produce($id)
{
	$sql = "SELECT prod.*, wo.stock_id, item.description AS StockDescription, wo.closed
			FROM ".TB_PREF."wo_manufacture prod,"
				.TB_PREF."workorders wo,"
				.TB_PREF."stock_master item
		WHERE prod.workorder_id=wo.id
		AND item.stock_id=wo.stock_id
		AND prod.id=".db_escape($id);
    $result = db_query($sql, "The work order production could not be retrieved");

    return db_fetch($result);
}

//--------------------------------------------------------------------------------------

function get_work_order_productions($woid)
{
	$sql = "SELECT * FROM ".TB_PREF."wo_manufacture WHERE workorder_id="
		.db_escape($woid)." ORDER BY id";
    return db_query($sql, "The work order issues could not be retrieved");
}

//--------------------------------------------------------------------------------------

function exists_work_order_produce($id)
{
	$sql = "SELECT id FROM ".TB_PREF."wo_manufacture WHERE id=".db_escape($id);
	$result = db_query($sql, "Cannot retreive a wo production");

    return (db_num_rows($result) > 0);
}

//--------------------------------------------------------------------------------------

function check_void_wo_production($prod_no)
{
	$prod = get_work_order_produce($prod_no);
	return $prod['closed'] ? $prod['workorder_id'] : 0;
}

//--------------------------------------------------------------------------------------------

function void_work_order_produce($type_no)
{
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
	work_order_update_finished_quantity($prod["workorder_id"], -$prod["quantity"], false);

	// clear the production record
	$sql = "UPDATE ".TB_PREF."wo_manufacture SET quantity=0 WHERE id=".db_escape($type_no);
	db_query($sql, "Cannot void a wo production");

	void_gl_trans(ST_MANURECEIVE, $type_no);

	void_stock_move(ST_MANURECEIVE, $type_no);

	commit_transaction();
}


