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
 * Work Centre CRUD functions — delegate to work_centres_entity.
 *
 * Simple CRUD (get/add/update/delete) is provided by the work_centres_entity class.
 * These wrapper functions exist only for backward compatibility.
 */

include_once($path_to_root . '/manufacturing/includes/db/work_centres_entity.inc');

function add_work_centre($name, $description) {
	return work_centres_entity::create(array(
		'name' => $name,
		'description' => $description
	));
}

function update_work_centre($type_id, $name, $description) {
	return work_centres_entity::modify($type_id, array(
		'name' => $name,
		'description' => $description
	));
}

function get_all_work_centres($all=false) {
	return work_centres_entity::all_db_resource($all ? '' : '!inactive');
}

function get_work_centre($type_id) {
	return work_centres_entity::find($type_id);
}

function delete_work_centre($type_id) {
	return work_centres_entity::remove($type_id);
}
