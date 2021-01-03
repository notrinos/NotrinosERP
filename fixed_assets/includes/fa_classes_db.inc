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

function get_fixed_asset_classes()
{
	$sql="SELECT * FROM ".TB_PREF."stock_fa_class";

	$result = db_query($sql,"fixed asset classes could not be retrieved");

	return $result;
}

function get_fixed_asset_classname($class)
{
	$sql="SELECT description FROM ".TB_PREF."stock_fa_class WHERE fa_class_id = ".db_escape($class);

	$result = db_query($sql,"fixed asset class name could not be retrieved");

	$row = db_fetch_row($result);
	
	return $row[0];
}

function update_fixed_asset_class($fa_class_id, $parent_id, $description, $long_description, $depreciation_rate)
{
	$sql = "UPDATE ".TB_PREF."stock_fa_class SET
		parent_id=".db_escape($parent_id).",
		description=".db_escape($description).",
		long_description=".db_escape($long_description).",
    depreciation_rate=".db_escape($depreciation_rate)."
    WHERE fa_class_id=".db_escape($fa_class_id);

	db_query($sql, "The fixed asset class could not be updated");
}

function add_fixed_asset_class($fa_class_id, $parent_id, $description, $long_description, $depreciation_rate)
{
  $sql = "INSERT INTO ".TB_PREF."stock_fa_class (fa_class_id, parent_id, description, long_description,
     depreciation_rate) VALUES ("
    .db_escape($fa_class_id).", ".db_escape($parent_id).", "
	.db_escape($description).", ".db_escape($long_description).", "
    .db_escape($depreciation_rate).")";

	db_query($sql, "The fixed asset class could not be added");
}

function delete_fixed_asset_class($fa_class_id)
{
  $sql = "DELETE FROM ".TB_PREF."stock_fa_class WHERE fa_class_id=".db_escape($fa_class_id);

	db_query($sql, "The fixed asset class could not be deleted");
}
