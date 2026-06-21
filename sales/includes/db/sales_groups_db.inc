<?php
/**********************************************************************
	Copyright (C) NotrinosERP.
	Released under the terms of the GNU General Public License, GPL, 
	as published by the Free Software Foundation, either version 3 
	of the License, or (at your option) any later version.
	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  
	See the License here <http://www.gnu.org/licenses/gpl-3.0.html>.
***********************************************************************/

function add_sales_group($description) {
	$sql = "INSERT INTO `".TB_PREF."groups` (description) VALUES (".db_escape($description) . ")";
	db_query($sql, 'The sales group could not be added');
}

function update_sales_group($selected_id, $description) {
	$sql = "UPDATE `".TB_PREF."groups` SET description=".db_escape($description)." WHERE id = ".db_escape($selected_id);
	db_query($sql, 'The sales group could not be updated');
}

function delete_sales_group($selected_id) {
	$sql="DELETE FROM `".TB_PREF."groups` WHERE id=".db_escape($selected_id);
	db_query($sql, 'could not delete sales group');
}

function get_sales_groups($show_inactive) {
	$sql = "SELECT * FROM `".TB_PREF."groups`";
	if (!$show_inactive) $sql .= " WHERE !inactive";
	$sql .= " ORDER BY description";
	return db_query($sql, 'could not get groups');
}

function get_sales_group($selected_id) {
	$sql = "SELECT * FROM `".TB_PREF."groups` WHERE id=".db_escape($selected_id);
	$result = db_query($sql, 'could not get group');

	return db_fetch($result);
}

function get_sales_group_name($group_no) {
	$sql = "SELECT description FROM `".TB_PREF."groups` WHERE id = ".db_escape($group_no);
	$result = db_query($sql, 'could not get group');
	$row = db_fetch($result);
	return $row[0];
}
