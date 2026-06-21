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

/**
 * Shipper CRUD functions — delegate to shippers_entity.
 *
 * Simple CRUD (get/add/update/delete) is provided by the shippers_entity class.
 * These wrapper functions exist only for backward compatibility.
 */

include_once($path_to_root . '/admin/db/shippers_entity.inc');

function add_shipper($shipper_name, $contact, $phone, $phone2, $address) {
	return shippers_entity::create(array(
		'shipper_name' => $shipper_name,
		'contact' => $contact,
		'phone' => $phone,
		'phone2' => $phone2,
		'address' => $address
	));
}

function update_shipper($selected_id, $shipper_name, $contact, $phone, $phone2, $address) {
	return shippers_entity::modify($selected_id, array(
		'shipper_name' => $shipper_name,
		'contact' => $contact,
		'phone' => $phone,
		'phone2' => $phone2,
		'address' => $address
	));
}

function delete_shipper($selected_id) {
	return shippers_entity::remove($selected_id);
}

function get_shippers($show_inactive) {
	return shippers_entity::all_db_resource($show_inactive ? '' : '!inactive');
}

function get_shipper($selected_id) {
	return shippers_entity::find($selected_id);
}
