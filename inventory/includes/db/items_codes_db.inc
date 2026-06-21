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
/*
	item_codes table is used to store both multiply foreign codes (aliases) and 
	sales kits definition.

	Simple CRUD operations delegate to item_codes_entity (static API).
	Complex/reporting queries (JOINs, recursive checks) remain standalone.
*/
include_once($path_to_root.'/inventory/includes/db/items_codes_entity.inc');

function update_item_code($id, $item_code, $stock_id, $description, $category, $qty, $foreign=0) {
	return item_codes_entity::update_item_code($id, $item_code, $stock_id, $description, $category, $qty, $foreign);
}

function add_item_code($item_code, $stock_id, $description, $category, $qty, $foreign=0) {
	return item_codes_entity::create(array(
		'item_code' => $item_code,
		'stock_id' => $stock_id,
		'description' => $description,
		'category_id' => $category,
		'quantity' => $qty,
		'is_foreign' => $foreign
	));
}

function delete_item_code($id) {
	return item_codes_entity::remove($id);
}

function get_item_code($id) {
	return item_codes_entity::find($id);
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
