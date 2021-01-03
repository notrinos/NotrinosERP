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

function add_sales_group($description)
{
	$sql = "INSERT INTO ".TB_PREF."groups (description) VALUES (".db_escape($description) . ")";
   	db_query($sql,"The sales group could not be added");
}

function update_sales_group($selected_id, $description)
{
	$sql = "UPDATE ".TB_PREF."groups SET description=".db_escape($description)." WHERE id = ".db_escape($selected_id);
   	db_query($sql,"The sales group could not be updated");
}

function delete_sales_group($selected_id)
{
	$sql="DELETE FROM ".TB_PREF."groups WHERE id=".db_escape($selected_id);
	db_query($sql,"could not delete sales group");
}

function get_sales_groups($show_inactive)
{
	$sql = "SELECT * FROM ".TB_PREF."groups";
	if (!$show_inactive) $sql .= " WHERE !inactive";
	$sql .= " ORDER BY description";
	return db_query($sql,"could not get groups");
}

function get_sales_group($selected_id)
{
	$sql = "SELECT * FROM ".TB_PREF."groups WHERE id=".db_escape($selected_id);

	$result = db_query($sql,"could not get group");
	return db_fetch($result);
}

function get_sales_group_name($group_no)
{
	$sql = "SELECT description FROM ".TB_PREF."groups WHERE id = ".db_escape($group_no);
	$result = db_query($sql, "could not get group");
	$row = db_fetch($result);
	return $row[0];
}

function add_sales_area($description)
{
	$sql = "INSERT INTO ".TB_PREF."areas (description) VALUES (".db_escape($description) . ")";
   	db_query($sql,"The sales area could not be added");
}

function update_sales_area($selected_id, $description)
{
	$sql = "UPDATE ".TB_PREF."areas SET description=".db_escape($description)." WHERE area_code = ".db_escape($selected_id);
   	db_query($sql,"The sales area could not be updated");
}

function delete_sales_area($selected_id)
{
	$sql="DELETE FROM ".TB_PREF."areas WHERE area_code=".db_escape($selected_id);
	db_query($sql,"could not delete sales area");
}

function get_sales_areas($show_inactive)
{
	$sql = "SELECT * FROM ".TB_PREF."areas";
	if (!$show_inactive) $sql .= " WHERE !inactive";
	return db_query($sql,"could not get areas");
}

function get_sales_area($selected_id)
{
	$sql = "SELECT * FROM ".TB_PREF."areas WHERE area_code=".db_escape($selected_id);

	$result = db_query($sql,"could not get area");
	return db_fetch($result);
}

function get_area_name($id)
{
	$sql = "SELECT description FROM ".TB_PREF."areas WHERE area_code=".db_escape($id);

	$result = db_query($sql, "could not get sales type");

	$row = db_fetch_row($result);
	return $row[0];
}

function add_salesman($salesman_name, $salesman_phone, $salesman_fax,
	$salesman_email, $provision, $break_pt, $provision2)
{
	$sql = "INSERT INTO ".TB_PREF."salesman (salesman_name, salesman_phone, salesman_fax, salesman_email,
		provision, break_pt, provision2)
		VALUES (".db_escape($salesman_name) . ", "
		  .db_escape($salesman_phone) . ", "
		  .db_escape($salesman_fax) . ", "
		  .db_escape($salesman_email) . ", ".
		$provision.", ".$break_pt.", "
		.$provision2.")";
   	db_query($sql,"The insert of the sales person failed");
}

function update_salesman($selected_id, $salesman_name, $salesman_phone, $salesman_fax,
	$salesman_email, $provision, $break_pt, $provision2)
{
	$sql = "UPDATE ".TB_PREF."salesman SET salesman_name=".db_escape($salesman_name) . ",
		salesman_phone=".db_escape($salesman_phone) . ",
		salesman_fax=".db_escape($salesman_fax) . ",
		salesman_email=".db_escape($salesman_email) . ",
		provision=".$provision.",
		break_pt=".$break_pt.",
		provision2=".$provision2."
		WHERE salesman_code = ".db_escape($selected_id);
   	db_query($sql,"The update of the sales person failed");
}

function delete_salesman($selected_id)
{
	$sql="DELETE FROM ".TB_PREF."salesman WHERE salesman_code=".db_escape($selected_id);
	db_query($sql,"The sales-person could not be deleted");
}

function get_salesmen($show_inactive)
{
	$sql = "SELECT * FROM ".TB_PREF."salesman";
	if (!$show_inactive) $sql .= " WHERE !inactive";
	return db_query($sql,"could not get sales persons");
}

function get_salesman($selected_id)
{
	$sql = "SELECT *  FROM ".TB_PREF."salesman WHERE salesman_code=".db_escape($selected_id);

	$result = db_query($sql,"could not get sales person");
	return db_fetch($result);
}

function get_salesman_name($id)
{
	$sql = "SELECT salesman_name FROM ".TB_PREF."salesman WHERE salesman_code=".db_escape($id);

	$result = db_query($sql, "could not get sales type");

	$row = db_fetch_row($result);
	return $row[0];
}



