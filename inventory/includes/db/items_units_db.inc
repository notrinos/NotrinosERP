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
function write_item_unit($selected, $abbr, $description, $decimals)
{
    if($selected!='')
		$sql = "UPDATE ".TB_PREF."item_units SET
	 	abbr = ".db_escape($abbr).",
	 	name = ".db_escape($description).",
	 	decimals = ".db_escape($decimals)."
        	WHERE abbr = ".db_escape($selected);
    else
		$sql = "INSERT INTO ".TB_PREF."item_units
			(abbr, name, decimals) VALUES( ".db_escape($abbr).",
	  		".db_escape($description).", ".db_escape($decimals).")";

	db_query($sql,"an item unit could not be updated");
}

function delete_item_unit($unit)
{
	$sql="DELETE FROM ".TB_PREF."item_units WHERE abbr=".db_escape($unit);

	db_query($sql,"an unit of measure could not be deleted");
}

function get_item_unit($unit)
{
	$sql="SELECT * FROM ".TB_PREF."item_units WHERE abbr=".db_escape($unit);

	$result = db_query($sql,"an unit of measure could not be retrieved");

	return db_fetch($result);
}

function get_unit_descr($unit)
{
	$sql = "SELECT name FROM ".TB_PREF."item_units WHERE abbr=".db_escape($unit);

	$result = db_query($sql, "could not retrieve unit description");

	$row = db_fetch_row($result);
	return $row[0];
}

function item_unit_used($unit) {
	$sql= "SELECT COUNT(*) FROM ".TB_PREF."stock_master WHERE units=".db_escape($unit);
	$result = db_query($sql, "could not query stock master");
	$myrow = db_fetch_row($result);
	return ($myrow[0] > 0);
}

function get_all_item_units($all=false) {
    $sql = "SELECT * FROM ".TB_PREF."item_units";
	if (!$all) $sql .= " WHERE !inactive";
	$sql .= " ORDER BY name";
    return  db_query($sql, "could not get stock categories");
}
// 2008-06-15. Added to get a measure of unit by given stock_id
function get_unit_dec($stock_id)
{
	$sql = "SELECT decimals FROM ".TB_PREF."item_units,	".TB_PREF."stock_master
		WHERE abbr=units AND stock_id=".db_escape($stock_id)." LIMIT 1";
	$result = db_query($sql, "could not get unit decimals");

	$row = db_fetch_row($result);
	return $row[0];
}

