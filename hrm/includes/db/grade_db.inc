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

function update_pay_grade($grade_id, $grade_name, $position_id, $pay_amount) {
	$sql = "UPDATE ".TB_PREF."pay_grades SET grade_name = ".db_escape($grade_name).", position_id = ".db_escape($position_id).", pay_amount = ".db_escape($pay_amount)." WHERE grade_id = ".db_escape($grade_id);
	db_query($sql, 'could not update grade record');
}

function add_pay_grade($grade_name, $position_id, $pay_amount=0) {
	$sql = "INSERT INTO ".TB_PREF."pay_grades (grade_name, position_id, pay_amount) VALUES (".db_escape($grade_name).",".db_escape($position_id).",".db_escape($pay_amount).")";
	db_query($sql, 'could not insert grade record');
}

function get_pay_grade($grade_id) {
	$sql = "SELECT * FROM ".TB_PREF."pay_grades WHERE grade_id = ".db_escape($grade_id);
	$result = db_query($sql, 'could not get grade record');

	return db_fetch($result);
}

function get_pay_grades($all=false) {
	$sql = "SELECT * FROM ".TB_PREF."pay_grades";

	if(empty($all))
		$sql .= " WHERE !inactive";

	return db_query($sql, 'could not get grades data');
}

function delete_pay_grade($grade_id) {
	$sql = "DELETE FROM ".TB_PREF."pay_grades WHERE grade_id = ".db_escape($grade_id);
	db_query($sql, 'could not delete a pay grade record');
}
