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
function add_item_location($loc_code, $location_name, $delivery_address, $phone, $phone2, $fax, $email, $contact, $fixed_asset = 0)
{
	$sql = "INSERT INTO ".TB_PREF."locations (loc_code, location_name, delivery_address, phone, phone2, fax, email, contact, fixed_asset)
		VALUES (".db_escape($loc_code).", ".db_escape($location_name).", ".db_escape($delivery_address).", "
			.db_escape($phone).", ".db_escape($phone2).", ".db_escape($fax).", ".db_escape($email).", "
			.db_escape($contact).", ".db_escape($fixed_asset).")";

	db_query($sql,"a location could not be added");

	/* Also need to add loc_stock records for all existing items */
	$sql = "INSERT INTO ".TB_PREF."loc_stock (loc_code, stock_id, reorder_level)
		SELECT ".db_escape($loc_code).", ".TB_PREF."stock_master.stock_id, 0 FROM ".TB_PREF."stock_master";

	db_query($sql,"a location could not be added");
}

//------------------------------------------------------------------------------------

function update_item_location($loc_code, $location_name, $delivery_address, $phone, $phone2, $fax, $email, $contact, $fixed_asset = 0)

{
    $sql = "UPDATE ".TB_PREF."locations SET location_name=".db_escape($location_name).",
    	delivery_address=".db_escape($delivery_address).",
    	phone=".db_escape($phone).", phone2=".db_escape($phone2).", fax=".db_escape($fax).",
    	email=".db_escape($email).", contact=".db_escape($contact).",
      fixed_asset=".db_escape($fixed_asset)."
    	WHERE loc_code = ".db_escape($loc_code);

	db_query($sql,"a location could not be updated");
}

//------------------------------------------------------------------------------------

function delete_item_location($item_location)
{
	$sql="DELETE FROM ".TB_PREF."locations WHERE loc_code=".db_escape($item_location);
	db_query($sql,"a location could not be deleted");

	$sql = "DELETE FROM ".TB_PREF."loc_stock WHERE loc_code =".db_escape($item_location);
	db_query($sql,"a location could not be deleted");
}

//------------------------------------------------------------------------------------

function get_item_location($item_location)
{
	$sql="SELECT * FROM ".TB_PREF."locations WHERE loc_code=".db_escape($item_location);

	$result = db_query($sql,"a location could not be retrieved");

	return db_fetch($result);
}

//------------------------------------------------------------------------------------

function get_item_locations($show_inactive, $fixed_asset = 0)
{
	$sql = "SELECT * FROM ".TB_PREF."locations WHERE fixed_asset = ".db_escape($fixed_asset);
	if (!$show_inactive) $sql .= " AND !inactive";
	return db_query($sql, "could not query locations");
}

//------------------------------------------------------------------------------------

function set_reorder_level($stock_id, $loc_code, $reorder_level)
{
	$sql = "UPDATE ".TB_PREF."loc_stock SET reorder_level = $reorder_level
		WHERE stock_id = ".db_escape($stock_id)." AND loc_code = ".db_escape($loc_code);

	db_query($sql,"an item reorder could not be set");
}

//------------------------------------------------------------------------------------

function get_loc_details($stock_id, $fixed_asset = 0)
{
	$sql = "SELECT stock.loc_code, stock.location_name, "
	.db_escape($stock_id)." as stock_id, reorders.reorder_level
		FROM ".TB_PREF."locations stock LEFT JOIN ".TB_PREF."loc_stock reorders ON
		reorders.loc_code=stock.loc_code
		AND reorders.stock_id = ".db_escape($stock_id)
	    ." WHERE stock.fixed_asset = ".db_escape($fixed_asset)
		." ORDER BY reorders.loc_code";
	return db_query($sql,"an item reorder could not be retreived");
}

