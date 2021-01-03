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
function add_work_centre($name, $description)
{
	$sql = "INSERT INTO ".TB_PREF."workcentres (name, description)
		VALUES (".db_escape($name).",".db_escape($description).")";

	db_query($sql, "could not add work centre");
}

function update_work_centre($type_id, $name, $description)
{
	$sql = "UPDATE ".TB_PREF."workcentres SET name=".db_escape($name).", description=".db_escape($description)."
		WHERE id=".db_escape($type_id);

	db_query($sql, "could not update work centre");
}

function get_all_work_centres($all=false)
{
	$sql = "SELECT * FROM ".TB_PREF."workcentres";
	if (!$all) $sql .= " WHERE !inactive";

	return db_query($sql, "could not get all work centres");
}

function get_work_centre($type_id)
{
	$sql = "SELECT * FROM ".TB_PREF."workcentres WHERE id=".db_escape($type_id);

	$result = db_query($sql, "could not get work centre");

	return db_fetch($result);
}

function delete_work_centre($type_id)
{
	$sql="DELETE FROM ".TB_PREF."workcentres WHERE id=".db_escape($type_id);

	db_query($sql, "could not delete work centre");
}

