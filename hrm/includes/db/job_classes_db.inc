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

function update_job_class($job_class_id, $class_name, $pay_basis) {
	$sql = "UPDATE ".TB_PREF."job_classes SET class_name = ".db_escape($class_name).", pay_basis = ".db_escape($pay_basis)." WHERE job_class_id = ".db_escape($job_class_id);
	db_query($sql, 'could not update job class record');
}

function add_job_class($class_name, $pay_basis) {
	$sql = "INSERT INTO ".TB_PREF."job_classes (class_name, pay_basis) VALUES (".db_escape($class_name).",".db_escape($pay_basis).")";
	db_query($sql, 'could not add job class data');
}

function get_job_class($job_class_id) {
	$sql = "SELECT * FROM ".TB_PREF."job_classes WHERE job_class_id = ".db_escape($job_class_id);
	$result = db_query($sql, 'could not get job class record');

	return db_fetch($result);
}

function get_job_classes($all=false) {
	$sql = "SELECT * FROM ".TB_PREF."job_classes";

	if(!$all)
		$sql .= " WHERE !inactive";

	return db_query($sql, 'could not get job classes data');
}

function delete_job_class($job_class_id) {
	$sql = "DELETE FROM ".TB_PREF."job_classes WHERE job_class_id = ".db_escape($job_class_id);
	db_query($sql, 'Could not delete job class.');
}

function db_has_job_classes() {
	return check_empty_result("SELECT COUNT(*) FROM ".TB_PREF."job_classes");
}
