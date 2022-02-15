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

function update_department($department_id, $department_name, $account) {
	$sql = "UPDATE ".TB_PREF."departments SET department_name = ".db_escape($department_name).", payroll_expense_account = ".db_escape($account)." WHERE department_id = ".db_escape($department_id);
	db_query($sql, 'could not update department record');
}

function add_department($department_name, $account) {
	$sql = "INSERT INTO ".TB_PREF."departments (department_name, payroll_expense_account) VALUES (".db_escape($department_name).",".db_escape($account).")";
	db_query($sql, 'could not add department data');
}

function get_department($department_id) {
	$sql = "SELECT * FROM ".TB_PREF."departments WHERE department_id = ".db_escape($department_id);
	$result = db_query($sql, 'could not get department record');

	return db_fetch($result);
}

function get_departments($all=false) {
	$sql = "SELECT * FROM ".TB_PREF."departments";

	if(!$all)
		$sql .= " WHERE !inactive";

	return db_query($sql, 'could not get departments data');
}

function delete_department($department_id) {
	$sql = "DELETE FROM ".TB_PREF."departments WHERE department_id = ".db_escape($department_id);
	db_query($sql, 'Could not delete department record');
}
