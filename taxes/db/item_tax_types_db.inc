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
function add_item_tax_type($name, $exempt, $exempt_from)
{
	begin_transaction();
	
	$sql = "INSERT INTO ".TB_PREF."item_tax_types (name, exempt) 
		VALUES (".db_escape($name).",".db_escape($exempt).")";
		
	db_query($sql, "could not add item tax type");	
	
	$id = db_insert_id();
	
	// add the exemptions
	add_item_tax_type_exemptions($id, $exempt_from);
	
	commit_transaction();
}

function update_item_tax_type($id, $name, $exempt, $exempt_from)
{
	begin_transaction();
	
	$sql = "UPDATE ".TB_PREF."item_tax_types SET name=".db_escape($name).
	",	exempt=".db_escape($exempt)." WHERE id=".db_escape($id);
	
	db_query($sql, "could not update item tax type");	
	
	// readd the exemptions
	delete_item_tax_type_exemptions($id);
	add_item_tax_type_exemptions($id, $exempt_from);		
	
	commit_transaction();	
}

function get_all_item_tax_types($also_inactive=false)
{
	$sql = "SELECT * FROM ".TB_PREF."item_tax_types";
	if (!$also_inactive)
		$sql .= " WHERE !inactive";

	return db_query($sql, "could not get all item tax type");
} 

function get_item_tax_type($id)
{
	$sql = "SELECT * FROM ".TB_PREF."item_tax_types WHERE id=".db_escape($id);
	
	$result = db_query($sql, "could not get item tax type");
	
	return db_fetch($result);
}

function get_item_tax_type_for_item($stock_id)
{
	$sql = "SELECT item_tax_type.*
		FROM ".TB_PREF."item_tax_types item_tax_type,"
			.TB_PREF."stock_master item
		WHERE item.stock_id=".db_escape($stock_id)."
		AND item_tax_type.id=item.tax_type_id";
	
	$result = db_query($sql, "could not get item tax type");
	
	return db_fetch($result);	
}

function delete_item_tax_type($id)
{
	begin_transaction();
		
	$sql = "DELETE FROM ".TB_PREF."item_tax_types WHERE id=".db_escape($id);
		
	db_query($sql, "could not delete item tax type");
	// also delete all exemptions
	delete_item_tax_type_exemptions($id);
	
	commit_transaction();	
}

function add_item_tax_type_exemptions($id, $exemptions)
{
	for ($i = 0; $i < count($exemptions); $i++) 
	{
		$sql = "INSERT INTO ".TB_PREF."item_tax_type_exemptions (item_tax_type_id, tax_type_id)
			VALUES (".db_escape($id).",  ".db_escape($exemptions[$i]).")";
		db_query($sql, "could not add item tax type exemptions");					
	}		
}

function delete_item_tax_type_exemptions($id)
{
	$sql = "DELETE FROM ".TB_PREF."item_tax_type_exemptions WHERE item_tax_type_id=".db_escape($id);
	
	db_query($sql, "could not delete item tax type exemptions");					
}

function get_item_tax_type_exemptions($id)
{
	$sql = "SELECT * FROM ".TB_PREF."item_tax_type_exemptions WHERE item_tax_type_id=".db_escape($id);
	
	return db_query($sql, "could not get item tax type exemptions");
}

function item_type_inactive($id)
{
	$type = get_item_tax_type($id);
	return @$type['inactive'];
}
