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

function get_salary_structure_amount($position_id, $grade_id, $element_id) {
	$sql = "SELECT pay_amount FROM ".TB_PREF."salary_structure WHERE position_id = ".db_escape($position_id)." AND grade_id = ".db_escape($grade_id)." AND element_id = ".db_escape($element_id);
	$result = db_query($sql, 'could not get a salary structure record');

	return db_fetch($result)[0];
}

function get_salary_structures() {
	$sql = "SELECT * FROM ".TB_PREF."salary_structure";
	return db_query($sql, 'could not get salary structure data');
}

function add_salary_structure_element($position_id, $grade_id, $element_id, $amount) {
	$sql = "INSERT INTO ".TB_PREF."salary_structure (position_id, grade_id, element_id, pay_amount) VALUES (".db_escape($position_id).",".db_escape($grade_id).",".db_escape($element_id).",".db_escape($amount).")";
	db_query($sql, 'could not insert a salary structure element');
}

function update_salary_structure($position_id, $grade_id, $element_id, $pay_amount) {
	$sql = "UPDATE ".TB_PREF."salary_structure SET pay_amount = ".db_escape($pay_amount)." WHERE position_id = ".db_escape($position_id)." AND grade_id = ".db_escape($grade_id)." AND element_id = ".db_escape($element_id);
	db_query($sql, 'could not update salary structure record');
}

function delete_salary_structure($position_id, $grade_id, $element_id) {
	$sql = "DELETE FROM ".TB_PREF."salary_structure WHERE position_id = ".db_escape($position_id)." AND grade_id = ".db_escape($grade_id)." AND element_id = ".db_escape($element_id);
	db_query($sql, 'could not delete a salary structure record');
}

function salary_structure_element_exist($position_id, $grade_id, $element_id) {
	return check_empty_result("SELECT COUNT(*) FROM ".TB_PREF."salary_structure WHERE position_id = ".db_escape($position_id)." AND grade_id = ".db_escape($grade_id)." AND element_id = ".db_escape($element_id));
}
