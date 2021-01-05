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
/*
	item_codes table is used to store both multiply foreign codes (aliases) and 
	sales kits definition.
*/
function update_item_code($id, $item_code, $stock_id, $description, $category, $qty, $foreign=0) {
	$sql = "UPDATE ".TB_PREF."item_codes SET
		item_code = ".db_escape($item_code).",
		stock_id = ".db_escape($stock_id).",
		description = ".db_escape($description).",
		category_id = ".db_escape($category).",
		quantity = ".db_escape($qty).",
		is_foreign = ".db_escape($foreign)."
			WHERE ";
			
	if ($id == -1) // update with unknown $id i.e. from items table editor
		$sql .= "item_code = ".db_escape($item_code)
		." AND stock_id = ".db_escape($stock_id);
	else
		$sql .= "id = ".db_escape($id);

	db_query($sql, 'an item code could not be updated');
}

function add_item_code($item_code, $stock_id, $description, $category, $qty, $foreign=0) {
	$sql = "INSERT INTO ".TB_PREF."item_codes
			(item_code, stock_id, description, category_id, quantity, is_foreign) 
			VALUES( ".db_escape($item_code).",".db_escape($stock_id).",
			".db_escape($description).",".db_escape($category)
			.",".db_escape($qty).",".db_escape($foreign).")";

	db_query($sql, 'an item code could not be added');
}

function delete_item_code($id) {
	$sql="DELETE FROM ".TB_PREF."item_codes WHERE id=".db_escape($id);
	db_query($sql, 'an item code could not be deleted');
}

function get_item_code($id) {
	$sql="SELECT * FROM ".TB_PREF."item_codes WHERE id=".db_escape($id);

	$result = db_query($sql, 'item code could not be retrieved');

	return db_fetch($result);
}

function get_all_item_codes($stock_id, $foreign=1) {
	$sql="SELECT i.*, c.description as cat_name FROM "
		.TB_PREF."item_codes as i,"
		.TB_PREF."stock_category as c
		WHERE stock_id=".db_escape($stock_id)."
		AND i.category_id=c.category_id
		AND i.is_foreign=".db_escape($foreign);

	$result = db_query($sql, 'all item codes could not be retrieved');

	return $result;
}

function delete_item_kit($item_code) {
	$sql="DELETE FROM ".TB_PREF."item_codes WHERE item_code=".db_escape($item_code);
	db_query($sql, 'an item kit could not be deleted');
}

function get_item_kit($item_code) {
	$sql="SELECT DISTINCT kit.*, item.units, comp.description as comp_name 
		FROM "
		.TB_PREF."item_codes kit,"
		.TB_PREF."item_codes comp
		LEFT JOIN "
		.TB_PREF."stock_master item
		ON 
			item.stock_id=comp.item_code
		WHERE
			kit.stock_id=comp.item_code
			AND kit.item_code=".db_escape($item_code);

	$result = db_query($sql, 'item kit could not be retrieved');

	return $result;
}

function get_item_code_dflts($stock_id) {
	$sql = "SELECT units, decimals, description, category_id
		FROM ".TB_PREF."stock_master,".TB_PREF."item_units
		WHERE stock_id=".db_escape($stock_id);

	$result = db_query($sql, 'item code defaults could not be retrieved');
	return db_fetch($result);
}
//
//	Check if kit contains given item, optionally recursive.
//
function check_item_in_kit($old_id, $kit_code, $item_code, $recurse=false) {
	$result = get_item_kit($kit_code);
	if ($result) {
		while ($myrow = db_fetch($result)) {
			if ($myrow['id'] == $old_id) 
				continue;
				
			if ($myrow['stock_id'] == $item_code)
				return 1;

			if ($recurse && $myrow['item_code'] != $myrow['stock_id'] && check_item_in_kit($old_id, $item_code, $myrow['stock_id'], true))
				return 1;
		}
	}
	return 0;
}

function get_kit_props($kit_code) {
	$sql = "SELECT description, category_id FROM ".TB_PREF."item_codes "
		. " WHERE item_code=".db_escape($kit_code);
	$res = db_query($sql, 'kit name query failed');
	return db_fetch($res);
}

function update_kit_props($kit_code, $name, $category) {
	$sql = "UPDATE ".TB_PREF."item_codes SET description="
		. db_escape($name).",category_id=".db_escape($category)
		. " WHERE item_code=".db_escape($kit_code);
	db_query($sql, 'kit name update failed');
}

function get_where_used($item_code) {
	$sql = "SELECT item_code, description FROM "
		.TB_PREF."item_codes "
		. " WHERE stock_id=".db_escape($item_code)."
			AND item_code!=".db_escape($item_code);
	return db_query($sql, 'where used query failed');
}