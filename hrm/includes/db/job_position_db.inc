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

function update_job_position($position_id, $position_name, $amount, $class) {
	$sql = "UPDATE ".TB_PREF."positions SET position_name = ".db_escape($position_name).", basic_amount = ".db_escape($amount).", job_class_id = ".db_escape($class)." WHERE position_id = ".db_escape($position_id);
	db_query($sql, 'could not update job position record');
}

function add_job_position($position_name, $basic_amount, $class) {
	$sql = "INSERT INTO ".TB_PREF."positions (position_name, basic_amount, job_class_id) VALUES (".db_escape($position_name).",".db_escape($basic_amount).",".db_escape($class).")";
	db_query($sql, 'could not add job position data');
}

function get_job_position($position_id) {
	$sql = "SELECT * FROM ".TB_PREF."positions WHERE position_id = ".db_escape($position_id);
	$result = db_query($sql, 'could not get job position record');

	return db_fetch($result);
}

function get_job_positions($all=false) {
	$sql = "SELECT * FROM ".TB_PREF."positions";

	if(!$all)
		$sql .= " WHERE !inactive";

	return db_query($sql, 'could not get job positions data');
}

function delete_job_position($position_id) {
	$sql = "DELETE FROM ".TB_PREF."positions WHERE position_id = ".db_escape($position_id);
	db_query($sql, 'Could not delete job position');
}

function db_has_job_position() {
	return check_empty_result("SELECT COUNT(*) FROM ".TB_PREF."positions");
}
