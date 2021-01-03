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

//--------------------------------------------------------------------------------------

function add_shipper($shipper_name, $contact, $phone, $phone2, $address)
{
	$sql = "INSERT INTO ".TB_PREF."shippers (shipper_name, contact, phone, phone2, address)
		VALUES (" . db_escape($shipper_name) . ", " .
		db_escape($contact). ", " .
		db_escape($phone). ", " .
		db_escape($phone2). ", " .
		db_escape($address) . ")";

	db_query($sql,"The Shipping Company could not be added");
}

//--------------------------------------------------------------------------------------

function update_shipper($selected_id, $shipper_name, $contact, $phone, $phone2, $address)
{
	$sql = "UPDATE ".TB_PREF."shippers SET shipper_name=" . db_escape($shipper_name). " ,
		contact =" . db_escape($contact). " ,
		phone =" . db_escape($phone). " ,
		phone2 =" . db_escape($phone2). " ,
		address =" . db_escape($address). "
		WHERE shipper_id = ".db_escape($selected_id);

	db_query($sql,"The shipping company could not be updated");
}

//--------------------------------------------------------------------------------------

function delete_shipper($selected_id)
{
	$sql="DELETE FROM ".TB_PREF."shippers WHERE shipper_id=".db_escape($selected_id);
	
	db_query($sql,"could not delete shipper");
}

//--------------------------------------------------------------------------------------

function get_shippers($show_inactive)
{
	$sql = "SELECT * FROM ".TB_PREF."shippers";
	if (!$show_inactive) $sql .= " WHERE !inactive";
	$sql .= " ORDER BY shipper_id";
	
	return db_query($sql,"could not get shippers");
}

//--------------------------------------------------------------------------------------

function get_shipper($selected_id)
{
	$sql = "SELECT * FROM ".TB_PREF."shippers WHERE shipper_id=".db_escape($selected_id);

	$result = db_query($sql, "could not get shipper");
	return db_fetch($result);
}

