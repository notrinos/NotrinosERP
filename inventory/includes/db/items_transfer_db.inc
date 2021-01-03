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
//-------------------------------------------------------------------------------------------------------------

function add_stock_transfer($Items, $location_from, $location_to, $date_, $reference, $memo_)
{
	global $SysPrefs, $path_to_root, $Refs;

	begin_transaction();
	$args = func_get_args();
	$args = (object)array_combine(array('Items', 'location_from', 'location_to', 
	'date_', 'reference', 'memo_'), $args);
	$args->trans_no = 0;
	hook_db_prewrite($args, ST_LOCTRANSFER);

	$transfer_id = get_next_trans_no(ST_LOCTRANSFER);

	if ($SysPrefs->loc_notification() == 1)
	{
		include_once($path_to_root . "/inventory/includes/inventory_db.inc");
		$st_ids = array();
		$st_names = array();
		$st_num = array();
		$st_reorder = array();
	}
	foreach ($Items as $line_item)
	{
		if ($SysPrefs->loc_notification() == 1)
			$loc = calculate_reorder_level($location_from, $line_item, $st_ids, $st_names, $st_num, $st_reorder); 
		add_stock_transfer_item($transfer_id, $line_item->stock_id, $location_from,
			$location_to, $date_, $reference, $line_item->quantity);
	}

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

//-------------------------------------------------------------------------------------------------------------

// add 2 stock_moves entries for a stock transfer
// $date_ is display date (not sql)
// std_cost is in HOME currency
// it seems the standard_cost field is not used at all

function add_stock_transfer_item($transfer_id, $stock_id, $location_from, $location_to,
	$date_, $reference, $quantity)
{
	add_stock_move(ST_LOCTRANSFER, $stock_id, $transfer_id, $location_from,
       	$date_, $reference, -$quantity, 0);

	add_stock_move(ST_LOCTRANSFER, $stock_id, $transfer_id, $location_to,
	   	$date_, $reference, $quantity, 0);

}

//-------------------------------------------------------------------------------------------------------------
//
//	Get inventory transfer common data (currently this is still stored in stock_moves table).
//
function get_stock_transfer($trans_no)
{
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

	$result = db_query($sql, "Could not get transfer common data");

	$data = db_fetch($result);

	return $data;
}

//-------------------------------------------------------------------------------------------------------------

function get_stock_transfer_items($trans_no)
{
	$result = get_stock_moves(ST_LOCTRANSFER, $trans_no);

	if (db_num_rows($result) == 0)
	{
		return null;
	}

	return $result;
}

//-------------------------------------------------------------------------------------------------------------

function void_stock_transfer($type_no)
{
	hook_db_prevoid(ST_LOCTRANSFER, $type_no);
	void_stock_move(ST_LOCTRANSFER, $type_no);
}

