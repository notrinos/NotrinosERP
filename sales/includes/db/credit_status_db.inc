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
 * Credit Status CRUD functions — delegate to credit_status_entity.
 *
 * Simple CRUD (get/add/update/delete) is provided by the credit_status_entity class.
 * These wrapper functions exist only for backward compatibility.
 */

include_once($path_to_root . '/sales/includes/db/credit_status_entity.inc');

function add_credit_status($description, $disallow_invoicing) {
	return credit_status_entity::create(array(
		'reason_description' => $description,
		'dissallow_invoices' => $disallow_invoicing
	));
}

function update_credit_status($status_id, $description, $disallow_invoicing) {
	return credit_status_entity::modify($status_id, array(
		'reason_description' => $description,
		'dissallow_invoices' => $disallow_invoicing
	));
}

function get_all_credit_status($all=false) {
	return credit_status_entity::all_db_resource($all ? '' : '!inactive');
}

function get_credit_status($status_id) {
	return credit_status_entity::find($status_id);
}

function delete_credit_status($status_id) {
	return credit_status_entity::remove($status_id);
}
