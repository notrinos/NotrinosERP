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

function update_overtime($overtime_id, $overtime_name, $pay_rate) {
	$sql = "UPDATE ".TB_PREF."overtime SET overtime_name = ".db_escape($overtime_name).", pay_rate = ".db_escape($pay_rate)." WHERE overtime_id = ".db_escape($overtime_id);
	db_query($sql, 'could not update overtime record');
}

function add_overtime($overtime_name, $pay_rate) {
	$sql = "INSERT INTO ".TB_PREF."overtime (overtime_name, pay_rate) VALUES (".db_escape($overtime_name).",".db_escape($pay_rate).")";
	db_query($sql, 'could not add overtime data');
}

function get_overtime($overtime_id) {
	$sql = "SELECT * FROM ".TB_PREF."overtime WHERE overtime_id = ".db_escape($overtime_id);
	$result = db_query($sql, 'could not get overtime record');

	return db_fetch($result);
}

function get_all_overtime($all=false) {
	$sql = "SELECT * FROM ".TB_PREF."overtime";

	if(!$all)
		$sql .= " WHERE !inactive";

	return db_query($sql, 'could not get overtime data');
}

function delete_overtime($overtime_id) {
	$sql = "DELETE FROM ".TB_PREF."overtime WHERE overtime_id = ".db_escape($overtime_id);
	db_query($sql, 'Could not delete overtime record');
}
