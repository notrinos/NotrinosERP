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
function get_wo_requirements($woid) {
	$sql = "SELECT req.*, item.description, item.mb_flag, loc.location_name, 
		center.name AS WorkCentreDescription,
		item.material_cost as ComponentCost,
		item.inventory_account,
		item.cogs_account
	FROM (".TB_PREF."wo_requirements req,"
		.TB_PREF."locations loc,"
		.TB_PREF."workcentres center)
		INNER JOIN ".TB_PREF."stock_master item ON req.stock_id=item.stock_id 
	WHERE workorder_id=".db_escape($woid)."
		AND loc.loc_code = req.loc_code
		AND center.id=workcentre";

	return db_query($sql, 'The work order requirements could not be retrieved');    
}

function get_requirements_costs($woid) {
	$sql = "SELECT SUM(units_req*unit_cost) cost
		FROM ".TB_PREF."wo_requirements
		WHERE workorder_id=".db_escape($woid);

	$result = db_query($sql, 'The work order requirements costs could not be retrieved');
	$costs = db_fetch($result);

	return $costs['cost'] ? $costs['cost'] : 0;
}

//--------------------------------------------------------------------------------------

function create_wo_requirements($woid, $stock_id) {
	$sql = "INSERT INTO ".TB_PREF."wo_requirements (workorder_id, stock_id, workcentre, units_req, loc_code)
		SELECT ".db_escape($woid).", component, workcentre_added, quantity, loc_code
		FROM ".TB_PREF."bom WHERE parent=".db_escape($stock_id);

	db_query($sql, 'The work order requirements could not be added');
}

//--------------------------------------------------------------------------------------

function delete_wo_requirements($woid) {
	$sql="DELETE FROM ".TB_PREF."wo_requirements WHERE workorder_id=".db_escape($woid);
	db_query($sql, 'The work order requirements could not be deleted');	
}


//--------------------------------------------------------------------------------------
/*
	Update total component units issued and average component cost
*/
function update_wo_requirement_issued($id, $quantity, $cost) {
	$sql = "UPDATE ".TB_PREF."wo_requirements SET 
		unit_cost = (units_issued*unit_cost+".$quantity*$cost.")/(units_issued+".$quantity."),
		units_issued = units_issued + ".db_escape($quantity)."
		WHERE id = ".db_escape($id);

	db_query($sql, "The work requirements issued quantity couldn't be updated");
}

//--------------------------------------------------------------------------------------

function void_wo_requirements($woid) {
	$sql = "UPDATE ".TB_PREF."wo_requirements SET units_issued = 0
		WHERE workorder_id = ".db_escape($woid);

	db_query($sql, "The work requirements issued quantity couldn't be voided");	
}