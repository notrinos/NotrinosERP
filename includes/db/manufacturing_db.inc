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
//----------------------------------------------------------------------------------------
function get_demand_qty($stock_id, $location)
{
	$sql = "SELECT SUM(line.quantity - line.qty_sent) AS QtyDemand
			FROM ".TB_PREF."sales_order_details line,
				".TB_PREF."sales_orders sorder
				WHERE line.order_no=sorder.order_no AND sorder.trans_type=".ST_SALESORDER
				." AND sorder.trans_type=line.trans_type"
				." AND line.stk_code = ".db_escape($stock_id);
	if ($location != "")
		$sql .= " AND sorder.from_stk_loc =".db_escape($location);

    $result = db_query($sql,"No transactions were returned");
	$row = db_fetch($result);
	if ($row === false)
		return 0;
	return $row['QtyDemand'];
}

$bom_list = array(); 
$qoh_stock = NULL;

function load_stock_levels($location)
{
	$date = date2sql(Today());

	$qoh_stock = array();
	$sql = "SELECT stock_id, SUM(qty)
		FROM ".TB_PREF."stock_moves
		WHERE tran_date <= '$date'";
	if ($location != '')
		$sql .= " AND loc_code = ".db_escape($location);
	$sql .= " GROUP BY stock_id";

	$result = db_query($sql, "QOH calculation failed");
	while ($row = db_fetch($result))
		$qoh_stock[$row[0]] = $row[1];

	return $qoh_stock;
}

// recursion fixed. Max 10 recursion levels.
function stock_demand_manufacture($stock_id, $qty, $demand_id, $location, $level=0) 
{
	global $bom_list, $qoh_stock;

	$demand = 0.0;
	if ($level > 10) {
		display_warning("BOM Too many Manufacturing levels deep $level");
		return $demand;
	}
	// Load all stock levels (stock moves) into $qoh_stock
	if ($qoh_stock == NULL)
		$qoh_stock = load_stock_levels($location);

	if (empty($qoh_stock[$stock_id]))
		$stock_qty = 0;
	else
		$stock_qty = $qoh_stock[$stock_id];

	if ($qty <= $stock_qty)
		return $demand;
	$bom = @$bom_list[$stock_id];
	if ($bom == NULL) {
		$sql = "SELECT parent, component, quantity FROM "
			.TB_PREF."bom WHERE parent = ".db_escape($stock_id);
		if ($location != "") $sql .= " AND loc_code = ".db_escape($location);
		$result = db_query($sql, "Could not search bom");
		$bom = array();
		// Even if we get no results, remember that fact 
		$bom[] = array($stock_id, '', 0); 
		while ($row = db_fetch_row($result)) {
			$bom[] = array($row[0], $row[1], $row[2]);
		}
		db_free_result($result);
		$bom_list[$stock_id] = $bom;
	}
	$len = count($bom);
	$i = 0;
	while ($i < $len) {
		$row = $bom[$i];
		$i++; 
		// Ignore the dummy entry
		if ($row[1] == '') continue;
		$q = $qty * $row[2];
		if ($row[1] == $demand_id) $demand += $q;
		$demand += stock_demand_manufacture($row[1], $q, $demand_id, $location, $level+1);
	}
	return $demand;
}

// recursion fixed by Tom Moulton
function get_demand_asm_qty($stock_id, $location) 
{
	$demand_qty = 0.0;
	$sql = "SELECT line.stk_code, SUM(line.quantity-line.qty_sent) AS Demmand
		   FROM ".TB_PREF."sales_order_details line,
				".TB_PREF."sales_orders sorder,
				".TB_PREF."stock_master item
		   WHERE sorder.order_no = line.order_no
		   		AND sorder.trans_type=".ST_SALESORDER
		   	 ." AND sorder.trans_type=line.trans_type
				AND line.quantity-line.qty_sent > 0
				AND item.stock_id=line.stk_code
				AND item.mb_flag='M'";
	if ($location != "")
		$sql .= " AND sorder.from_stk_loc =".db_escape($location);
	$sql .= " GROUP BY line.stk_code";
    $result = db_query($sql, "No transactions were returned");
	while ($row = db_fetch_row($result)) {
		$demand_qty += stock_demand_manufacture($row[0], $row[1], $stock_id, $location);
	}
	return $demand_qty;
}

function get_on_porder_qty($stock_id, $location)
{
	$sql = "SELECT SUM(line.quantity_ordered - line.quantity_received) AS qoo
		FROM ".TB_PREF."purch_order_details line 
			INNER JOIN ".TB_PREF."purch_orders po ON line.order_no=po.order_no
		WHERE line.item_code=".db_escape($stock_id);
	if ($location != "")
		$sql .= " AND po.into_stock_location=".db_escape($location);
	$sql .= " AND line.item_code=".db_escape($stock_id);
	$qoo_result = db_query($sql,"could not receive quantity on order for item");

	if (db_num_rows($qoo_result) == 1)
	{
		$qoo_row = db_fetch_row($qoo_result);
		$qoo =  $qoo_row[0];
	}
	else
	{
		$qoo = 0;
	}
	return $qoo;
}

function get_on_worder_qty($stock_id, $location)
{
 	$qoo = 0.0;
	$flag = get_mb_flag($stock_id);
	if ($flag == 'M')
	{
		$sql = "SELECT SUM((units_reqd-units_issued)) AS qoo
			FROM ".TB_PREF."workorders
			WHERE stock_id=".db_escape($stock_id)
				." AND released AND NOT closed";

		if ($location != "")
			$sql .= " AND loc_code=".db_escape($location);

		$qoo_result = db_query($sql,"could not receive quantity on order for item");
		if (db_num_rows($qoo_result) == 1)
		{
			$qoo_row = db_fetch_row($qoo_result);
			$qoo = $qoo_row[0];
		}
	}
	return $qoo;
}

//--------------------------------------------------------------------------------------

function add_bom($selected_parent, $component, $workcentre_added, $loc_code, $quantity)
{
	$sql = "INSERT INTO ".TB_PREF."bom (parent, component, workcentre_added, loc_code, quantity)
		VALUES (".db_escape($selected_parent).", ".db_escape($component) . ","
		.db_escape($workcentre_added) . ", ".db_escape($loc_code) . ", "
		. $quantity . ")";

	db_query($sql,"check failed");
}
//--------------------------------------------------------------------------------------

function update_bom($selected_parent, $selected_component, $workcentre_added, $loc_code, $quantity)
{
	$sql = "UPDATE ".TB_PREF."bom SET workcentre_added=".db_escape($workcentre_added)
	 . ",loc_code=".db_escape($loc_code) . ",
		quantity= " . $quantity . "
		WHERE parent=".db_escape($selected_parent) . "
		AND id=".db_escape($selected_component);
	check_db_error("Could not update this bom component", $sql);

	db_query($sql,"could not update bom");
}
	
function delete_bom($selected_id)
{
	$sql = "DELETE FROM ".TB_PREF."bom WHERE id=".db_escape($selected_id);
	db_query($sql,"Could not delete this bom components");
}

function get_bom($item)
{
	$sql = "SELECT bom.*, loc.location_name,
		centre.name AS WorkCentreDescription,
    	item.description, item.mb_flag AS ResourceType, 
    	item.material_cost AS ProductCost, units,
    	bom.quantity * item.material_cost AS ComponentCost 
    	FROM ".TB_PREF."workcentres centre,
    		".TB_PREF."locations loc,
    		".TB_PREF."bom bom
	    		INNER JOIN ".TB_PREF."stock_master item ON bom.component = item.stock_id 
    	WHERE bom.parent = ".db_escape($item)."
		AND centre.id=bom.workcentre_added
		AND bom.loc_code = loc.loc_code ORDER BY bom.id";
	
	return db_query($sql, "The bill of material could not be retrieved");
}

//--------------------------------------------------------------------------------------

function get_component_from_bom($selected_id)
{
	$sql = "SELECT bom.*, item.description
		FROM ".TB_PREF."bom bom,"
			.TB_PREF."stock_master item
		WHERE id=".db_escape($selected_id)."
		AND item.stock_id=bom.component";

	$result = db_query($sql, "could not get bom");
	return db_fetch($result);
}
//--------------------------------------------------------------------------------------

function has_bom($item)
{
    $result = get_bom($item);
    
    return (db_num_rows($result) != 0);
}

//--------------------------------------------------------------------------------------

function is_component_already_on_bom($component, $workcentre_added, $loc_code, $selected_parent)
{
	$sql = "SELECT component
		FROM ".TB_PREF."bom
		WHERE parent=".db_escape($selected_parent)."
		AND component=".db_escape($component) . "
		AND workcentre_added=".db_escape($workcentre_added) . "
		AND loc_code=".db_escape($loc_code);
	$result = db_query($sql,"check failed");

	return (db_num_rows($result) > 0);
}

//--------------------------------------------------------------------------------------

function check_for_recursive_bom($ultimate_parent, $component_to_check)
{

	/* returns true ie 1 if the bom contains the parent part as a component
	ie the bom is recursive otherwise false ie 0 */

	$sql = "SELECT component FROM ".TB_PREF."bom WHERE parent=".db_escape($component_to_check);
	$result = db_query($sql,"could not check recursive bom");

	if ($result)
	{
		while ($myrow = db_fetch_row($result))
		{
			if ($myrow[0] == $ultimate_parent)
			{
				return 1;
			}

			if (check_for_recursive_bom($ultimate_parent, $myrow[0]))
			{
				return 1;
			}
		}
	}

	return 0;
}

