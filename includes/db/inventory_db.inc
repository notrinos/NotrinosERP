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
function get_qoh_on_date($stock_id, $location=null, $date_=null) {
	if ($date_ == null)
		$date_ = Today();

	 $date = date2sql($date_);
	 $sql = "SELECT SUM(qty)
		FROM ".TB_PREF."stock_moves st
		LEFT JOIN ".TB_PREF."voided v ON st.type=v.type AND st.trans_no=v.id
		WHERE ISNULL(v.id)
		AND stock_id=".db_escape($stock_id)."
		AND tran_date <= '$date'"; 

	if ($location != null)
		$sql .= " AND loc_code = ".db_escape($location);

	$result = db_query($sql, "QOH calculation failed");

	$myrow = db_fetch_row($result);

	$qoh =  $myrow[0];
		return $qoh ? $qoh : 0;
}

/**
*	Check whether change in stock on date would not cause negative qoh in stock history.
*	Returns null on success or max. available quantity with respective date otherwise.
*   Running balance is checked on daily basis only, as we do not control time of transaction.
*
*	$delta_qty - tested change in stock qty at $date.
*	$date - check date; when set to null checks all the stock history.
**/

function check_negative_stock($stock_id, $delta_qty, $location=null, $date=null) {

	if ($delta_qty >= 0)
		 return null;	// qty increese is always safe

	if (!isset($date))
		$date = Today();

	$date = date2sql($date);

	// check stock status on date
	$sql = "SELECT SUM(qty) qty, '$date' tran_date FROM ".TB_PREF."stock_moves
		WHERE stock_id=".db_escape($stock_id)."
		AND tran_date <= '$date'"; 

	if ($location)
		$sql .= " AND loc_code = ".db_escape($location);

	$result = db_query($sql, "QOH calculation failed");
	$qos = db_fetch_assoc($result);

	// check also all stock changes after the date to avoid negative stock in future
	$sql = TB_PREF."stock_moves WHERE stock_id=".db_escape($stock_id) . " AND tran_date > '$date'";

	if ($location)
		$sql .= " AND loc_code=".db_escape($location);

	$rt = running_total_sql($sql, 'qty', 'tran_date');

	$sql = "SELECT  {$qos['qty']}+total qty, tran_date FROM ($rt) stock_status ORDER by total, tran_date";
	$history = db_query($sql, 'cannot check stock history');
	$min_qos = db_fetch($history);

	if ($min_qos && ($min_qos['qty'] < $qos['qty']))
		$qos = $min_qos;

	return  -$delta_qty > $qos['qty'] ? $qos : null;
}

//--------------------------------------------------------------------------------------

function get_item_edit_info($stock_id) {
	$sql = "SELECT item.material_cost, item.units, unit.decimals
		FROM ".TB_PREF."stock_master item,"
		.TB_PREF."item_units unit
		WHERE stock_id=".db_escape($stock_id)
		." AND item.units=unit.abbr";
	$result = db_query($sql, 'The standard cost cannot be retrieved');

	return db_fetch($result);
}

//--------------------------------------------------------------------------------------

function get_unit_cost($stock_id) {
	$sql = "SELECT material_cost
		FROM ".TB_PREF."stock_master
		WHERE stock_id=".db_escape($stock_id);
	$result = db_query($sql, 'The standard cost cannot be retrieved');

	$myrow = db_fetch_row($result);

	return $myrow[0];
}

//--------------------------------------------------------------------------------------
 
function get_purchase_cost($stock_id) {
	$sql = "SELECT purchase_cost
		FROM ".TB_PREF."stock_master
		WHERE stock_id=".db_escape($stock_id);
	$result = db_query($sql, 'The purchase cost cannot be retrieved');

	$myrow = db_fetch_row($result);

	return $myrow[0];
}

//--------------------------------------------------------------------------------------

function is_inventory_item($stock_id) {
	$sql = "SELECT stock_id FROM "
		.TB_PREF."stock_master
		WHERE stock_id=".db_escape($stock_id)." AND mb_flag <> 'D'";
	$result = db_query($sql, 'Cannot query is inventory item or not');

	return db_num_rows($result) > 0;
}

//-------------------------------------------------------------------

function last_negative_stock_begin_date($stock_id, $to) {
	$to = date2sql($to);
	$sql ="SET @q = 0";
	db_query($sql);
	$sql = "SET @flag = 0";
	db_query($sql);
	$sql = "SELECT SUM(qty), @q:= @q + qty, IF(@q < 0 AND @flag=0, @flag:=1,@flag:=0), IF(@q < 0 AND @flag=1, tran_date,'') AS begin_date 
		FROM ".TB_PREF."stock_moves
		WHERE stock_id=".db_escape($stock_id)." AND tran_date<='$to' 
		AND qty <> 0
		GROUP BY stock_id ORDER BY tran_date";

	$result = db_query($sql, 'The dstock moves could not be retrieved');
	$row = db_fetch_row($result);
	return $row[3];
}

//-------------------------------------------------------------------

function get_already_delivered($stock_id, $location, $trans_no) {
	$sql = "SELECT qty
		FROM ".TB_PREF."stock_moves
		WHERE stock_id = ".db_escape($stock_id)."
		AND loc_code = ".db_escape($location)."
		AND type=".ST_CUSTDELIVERY." AND trans_no=".db_escape($trans_no);
	$result = db_query($sql, 'Could not get stock moves');
	$row = db_fetch_row($result);
	return $row[0];
}
/*
	Returns start move_id in latest negative status period for $stock_id
	FIXME: $to ? 
*/
function last_negative_stock_trans_id($stock_id, $to) {
	$sql = "SELECT * from ".TB_PREF."stock_moves
		WHERE stock_id=".db_escape($stock_id)." 
		AND qty <> 0 order by trans_id asc";
	
	$result = db_query($sql, 'The query on stock moves failed.');
	
	$qty = 0;
	$flag = 0;
	$negative_trans_id = -1;
	
	while ($myrow = db_fetch($result)) {
		$qty += $myrow['qty'];
		if ($qty < 0 && $flag == 0) {
			$flag = 1;
			$negative_trans_id = $myrow['trans_id'];
		}
		if ($qty >= 0)
			$flag = 0;
	}

	if ($flag == 1)
		return $negative_trans_id;
	else 
		return false;
}

//-------------------------------------------------------------------

function get_deliveries_between($stock_id, $from, $to) {
	$from = date2sql($from);
	$to = date2sql($to);
	$sql = "SELECT SUM(-qty), SUM(-qty*standard_cost) FROM ".TB_PREF."stock_moves
		WHERE type=".ST_CUSTDELIVERY." AND stock_id=".db_escape($stock_id)." AND
		tran_date>='$from' AND tran_date<='$to' GROUP BY stock_id";

	$result = db_query($sql, 'The deliveries could not be updated');
	return db_fetch_row($result);
}

/*
	Returns quantity and total cost of $stock_id sales, entered after record with $move_id
*/
function get_deliveries_from_trans($stock_id, $move_id) {
	// -ve qty is delivery either by ST_CUSTDELIVERY or inventory adjustment
	//Price for GRN and SUPPCREDIT and std_cost for other trans_types
	$sql = "SELECT SUM(-qty), SUM(-qty*IF(type=".ST_SUPPRECEIVE." OR type=".ST_SUPPCREDIT.", price, standard_cost))
		FROM ".TB_PREF."stock_moves
		WHERE stock_id=".db_escape($stock_id)." AND qty < 0 AND
		trans_id>='$move_id' GROUP BY stock_id";
	$result = db_query($sql, "The deliveries could not be updated");
	$row = db_fetch_row($result);
	
	$sql = "SELECT IF(type=".ST_SUPPRECEIVE." OR type=".ST_SUPPCREDIT.", price, standard_cost)
		FROM ".TB_PREF."stock_moves
		WHERE stock_id=".db_escape($stock_id)." 
		AND trans_id ='$move_id'";
	$result = db_query($sql, "The deliveries could not be updated");
	$cost = db_fetch_row($result);

	// Adjusting QOH valuation 
	$sql = "SELECT SUM(qty)
		FROM ".TB_PREF."stock_moves
		WHERE stock_id=".db_escape($stock_id)." AND 
		trans_id<'$move_id' GROUP BY stock_id";
	$result = db_query($sql, 'The deliveries could not be updated');
	$qoh = db_fetch_row($result);

	$qty = $row[0] - $qoh[0]; //QOH prior to -ve stock is subtracted
	$final_cost = $row[1] - $qoh[0]*$cost[0];
	
	return array($qty,$final_cost); 
}

/*
	Returns quantity and total cost of $stock_id purchases, entered after record with $move_id
*/
function get_purchases_from_trans($stock_id, $move_id) {
	// Calculate All inward stock moves i.e. qty > 0
	$sql = "SELECT SUM(qty), SUM(qty*standard_cost)
		FROM ".TB_PREF."stock_moves
		WHERE stock_id=".db_escape($stock_id)." AND qty > 0 AND 
			trans_id>'$move_id' GROUP BY stock_id";
	$result = db_query($sql, 'Could not get get_purchases_from_trans');
	$row = db_fetch_row($result);
	
	return $row;
}

//-------------------------------------------------------------------
/*
	This routine fixes stock and COGS balances for all $stock_id sales made during negative inventory status.
	This is called when delivery is received causing inventory status to be positive again.
*/
function adjust_deliveries($stock_id, $material_cost, $to) {
	global $Refs;

	if (!is_inventory_item($stock_id))
		return;
	
	$move_id = last_negative_stock_trans_id($stock_id, $to);
	if ($move_id == false || $move_id == -1)
		return;

	$row = get_deliveries_from_trans($stock_id, $move_id);

	if ($row == false)
		return;	
	$old_sales_cost = $row[1];
	$new_sales_cost = $row[0] * $material_cost;
	$sales_diff = $new_sales_cost - $old_sales_cost;
	
	$row = get_purchases_from_trans($stock_id, $move_id);
	$purchase_diff = 0;
	$old_purchase_cost = $new_purchase_cost = 0;
	if ($row != false) {
		$old_purchase_cost = $row[1];
		$new_purchase_cost = $row[0] * $material_cost;
		$purchase_diff = $new_purchase_cost - $old_purchase_cost;
	}

	$diff =  $sales_diff - $purchase_diff;

	if ($diff != 0) {
		$stock_gl_code = get_stock_gl_code($stock_id);

		$dec = user_price_dec();
		$old_cost = -round2($old_sales_cost-$old_purchase_cost,$dec);
		$new_cost = -round2($new_sales_cost-$new_purchase_cost,$dec);

		$cart = new items_cart(ST_COSTUPDATE);
		$cart->tran_date = $cart->doc_date = $cart->event_date = $to;
		if (!is_date_in_fiscalyear($cart->tran_date))
			$cart->tran_date = end_fiscalyear();
		$cart->reference = $Refs->get_next(ST_COSTUPDATE, null, $cart->tran_date, $to);

		$cart->memo_ = _('Cost was ') . $old_cost. _(' changed to ') . $new_cost . _(' for item ')."'$stock_id'";

		$cart->add_gl_item($stock_gl_code['cogs_account'], $stock_gl_code['dimension_id'], $stock_gl_code['dimension2_id'], $diff);
		$cart->add_gl_item($stock_gl_code['inventory_account'], 0, 0, -$diff);

		write_journal_entries($cart);
	}
}

function get_stock_gl_code($stock_id) {
	/*Gets the GL Codes relevant to the item account  */
	$sql = "SELECT mb_flag, inventory_account, cogs_account,
		adjustment_account, sales_account, wip_account, dimension_id, dimension2_id FROM
		".TB_PREF."stock_master WHERE stock_id = ".db_escape($stock_id);

	$get = db_query($sql,"retreive stock gl code");
	return db_fetch($get);
}

function get_purchase_value($stock_id) {
	$sql = "SELECT purchase_cost FROM
		".TB_PREF."stock_master WHERE stock_id = ".db_escape($stock_id);

	$result = db_query($sql,"retreive stock purchase price");
	$row = db_fetch_row($result);
	return $row[0];
}

function update_purchase_value($stock_id, $price) {
	$price = round2($price, user_price_dec());
	$sql = "UPDATE ".TB_PREF."stock_master SET purchase_cost=".db_escape($price)
		." WHERE stock_id=".db_escape($stock_id);
	db_query($sql, 'The stock master purchase_cost cannot be updated');
}	
//-----------------------------------------------------------------------------------------

function handle_negative_inventory($stock_id, $quantity, $standard_cost, $date_) {
	//If negative adjustment result in negative or zero inventory
	//then difference should be adjusted
	$qoh = get_qoh_on_date($stock_id);

	if ($qoh + $quantity <= 0 && $qoh > 0) {//Positive inventory turning zero/negative
		global $Refs;

		$id = get_next_trans_no(ST_JOURNAL);
		$ref = $Refs->get_next(ST_JOURNAL, null, $date_);
		$diff = round($qoh*get_unit_cost($stock_id) + $quantity*$standard_cost, user_price_dec());

		if ($diff != 0) {
			begin_transaction();
			add_journal(ST_JOURNAL, $id, $diff, $date_, get_company_currency(), $ref);
			$Refs->save(ST_JOURNAL, $id, $ref);

			$stock_gl_code = get_stock_gl_code($stock_id);
			$memo = _('Zero/negative inventory handling');
			//Reverse the inventory effect if $qoh <=0
			add_gl_trans_std_cost(ST_JOURNAL, $id, $date_, $stock_gl_code['inventory_account'], $stock_gl_code['dimension_id'], $stock_gl_code['dimension2_id'], $memo, -$diff);
			//GL Posting to inventory adjustment account
			add_gl_trans_std_cost(ST_JOURNAL, $id, $date_, $stock_gl_code['adjustment_account'], $stock_gl_code['dimension_id'], $stock_gl_code['dimension2_id'], $memo, $diff);

			add_audit_trail(ST_JOURNAL, $id, $date_);
			add_comments(ST_JOURNAL, $id, $date_, $memo);
			$Refs->save(ST_JOURNAL, $id, $ref);	
			commit_transaction();
		}
	}
}

//--------------------------------------------------------------------------------------

// $date_ - display / non-sql date
// $std_cost - in HOME currency
// $price - in transaction currency

function add_stock_move($type, $stock_id, $trans_no, $location, $date_, $reference, $quantity, $std_cost, $price=0) {
	$date = date2sql($date_);

	$sql = "INSERT INTO ".TB_PREF."stock_moves (stock_id, trans_no, type, loc_code,
		tran_date, reference, qty, standard_cost, price) VALUES ("
		.db_escape($stock_id).", ".db_escape($trans_no).", "
		.db_escape($type).", ".db_escape($location).", '$date', "
		.db_escape($reference).", "
		.db_escape($quantity).", ".db_escape($std_cost)."," .db_escape($price).")";

	db_query($sql, 'The stock movement record cannot be inserted');

	return db_insert_id();
}

function update_stock_move($type, $trans_no, $stock_id, $cost) {
	$sql = "UPDATE ".TB_PREF."stock_moves SET standard_cost=".db_escape($cost)
		." WHERE type=".db_escape($type)
		."	AND trans_no=".db_escape($trans_no)
		."	AND stock_id=".db_escape($stock_id);
	db_query($sql, 'The stock movement standard_cost cannot be updated');
}

//--------------------------------------------------------------------------------------------------

function get_stock_moves($type, $type_no) {
	$sql = "SELECT move.*, item.description, item.mb_flag, item.units, stock.location_name
		FROM ".TB_PREF."stock_moves move,"
			.TB_PREF."locations stock,"
			.TB_PREF."stock_master item
		WHERE move.stock_id = item.stock_id
		AND stock.loc_code=move.loc_code
		AND type=".db_escape($type)
		." AND trans_no=".db_escape($type_no)
		." ORDER BY trans_id";

	return db_query($sql, 'Could not get stock moves');
}

//--------------------------------------------------------------------------------------------------

function void_stock_move($type, $type_no) {
	$sql = "SELECT move.*, supplier.supplier_id
		FROM ".TB_PREF."stock_moves move
		LEFT JOIN ".TB_PREF."supp_trans credit ON credit.trans_no=move.trans_no AND credit.type=move.type
		LEFT JOIN ".TB_PREF."grn_batch grn ON grn.id=move.trans_no AND 25=move.type
		LEFT JOIN ".TB_PREF."suppliers supplier ON IFNULL(grn.supplier_id, credit.supplier_id)=supplier.supplier_id
		WHERE move.type=".db_escape($type)." AND move.trans_no=".db_escape($type_no);

	$result = db_query($sql, 'Could not void stock moves');
	while ($row = db_fetch($result)) {
		//Skip cost averaging of service items
		if (is_inventory_item($row['stock_id'])) {
			// The cost has to be adjusted.
			// Transaction rates are stored either as price or standard_cost depending on types
			$types = array(ST_SUPPCREDIT, ST_SUPPRECEIVE);
			if (in_array($type, $types))
				$unit_cost = $row['price'];
			else
				$unit_cost = $row['standard_cost'];

			update_average_material_cost($row['supplier_id'], $row['stock_id'], $unit_cost, -$row['qty'], sql2date($row['tran_date']));
		}
	}
	$sql = "DELETE FROM ".TB_PREF."stock_moves
			WHERE type=".db_escape($type)
			."	AND trans_no=".db_escape($type_no);
	db_query($sql, 'The stock movement cannot be delated');
}

//--------------------------------------------------------------------------------------------------

function get_location_name($loc_code) {
	$sql = "SELECT location_name FROM ".TB_PREF."locations
		WHERE loc_code=".db_escape($loc_code);

	$result = db_query($sql, "could not retreive the location name for $loc_code");

	if (db_num_rows($result) == 1) {
		$row = db_fetch_row($result);
		return $row[0];
	}

	display_db_error("could not retreive the location name for $loc_code", $sql, true);
}

function get_mb_flag($stock_id) {
	$sql = "SELECT mb_flag FROM ".TB_PREF."stock_master
		WHERE stock_id = ".db_escape($stock_id);
	$result = db_query($sql, 'retreive mb_flag from item');
	
	if (db_num_rows($result) == 0)
		return -1;

	$myrow = db_fetch_row($result);
	return $myrow[0];
}