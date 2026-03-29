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

function update_leave_type($leave_id, $leave_name, $leave_code, $pay_rate) {
	$sql = "UPDATE ".TB_PREF."leave_types SET leave_name = ".db_escape($leave_name).", leave_code = ".db_escape($leave_code).", pay_rate = ".db_escape($pay_rate)." WHERE leave_id = ".db_escape($leave_id);
	db_query($sql, 'could not update leave type record');
}

function add_leave_type($leave_name, $leave_code, $pay_rate) {
	$sql = "INSERT INTO ".TB_PREF."leave_types (leave_name, leave_code, pay_rate) VALUES (".db_escape($leave_name).",".db_escape($leave_code).",".db_escape($pay_rate).")";
	db_query($sql, 'could not add leave type data');
}

function get_leave_type($leave_id) {
	$sql = "SELECT * FROM ".TB_PREF."leave_types WHERE leave_id = ".db_escape($leave_id);
	$result = db_query($sql, 'could not get leave type record');

	return db_fetch($result);
}

function get_leave_types($all=false) {
	$sql = "SELECT * FROM ".TB_PREF."leave_types";

	if(!$all)
		$sql .= " WHERE !inactive";

	return db_query($sql, 'could not get leave types data');
}

function delete_leave_type($leave_id) {
	$sql = "DELETE FROM ".TB_PREF."leave_types WHERE leave_id = ".db_escape($leave_id);
	db_query($sql, 'Could not delete leave type.');
}
