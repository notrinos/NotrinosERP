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
 * Employee dependents/family CRUD - per HRM_DEVELOPMENT_PLAN.md
 */

function add_employee_dependent($employee_id, $name, $relationship, $birth_date='', $gender=0, $national_id='', $is_beneficiary=0) {
	$sql = "INSERT INTO ".TB_PREF."employee_dependents (employee_id, name, relationship, gender, national_id, is_beneficiary";

	if (!empty($birth_date))
		$sql .= ", birth_date";

	$sql .= ") VALUES ("
		.db_escape($employee_id).","
		.db_escape($name).","
		.db_escape($relationship).","
		.db_escape($gender).","
		.db_escape($national_id).","
		.db_escape($is_beneficiary);

	if (!empty($birth_date))
		$sql .= ", '".date2sql($birth_date)."'";

	$sql .= ")";

	db_query($sql, 'could not add employee dependent');
	return db_insert_id();
}

function update_employee_dependent($dependent_id, $name, $relationship, $birth_date='', $gender=0, $national_id='', $is_beneficiary=0) {
	$sql = "UPDATE ".TB_PREF."employee_dependents SET "
		."name = ".db_escape($name).", "
		."relationship = ".db_escape($relationship).", "
		."gender = ".db_escape($gender).", "
		."national_id = ".db_escape($national_id).", "
		."is_beneficiary = ".db_escape($is_beneficiary);

	if (!empty($birth_date))
		$sql .= ", birth_date = '".date2sql($birth_date)."'";
	else
		$sql .= ", birth_date = NULL";

	$sql .= " WHERE dependent_id = ".db_escape($dependent_id);

	db_query($sql, 'could not update employee dependent');
}

function get_employee_dependent($dependent_id) {
	$sql = "SELECT * FROM ".TB_PREF."employee_dependents WHERE dependent_id = ".db_escape($dependent_id);
	$result = db_query($sql, 'could not get employee dependent');
	return db_fetch_assoc($result);
}

function get_employee_dependents($employee_id) {
	$sql = "SELECT * FROM ".TB_PREF."employee_dependents WHERE employee_id = ".db_escape($employee_id)." ORDER BY name";
	return db_query($sql, 'could not get employee dependents');
}

function delete_employee_dependent($dependent_id) {
	$sql = "DELETE FROM ".TB_PREF."employee_dependents WHERE dependent_id = ".db_escape($dependent_id);
	db_query($sql, 'could not delete employee dependent');
}

function get_employee_dependent_count($employee_id) {
	$sql = "SELECT COUNT(*) as cnt FROM ".TB_PREF."employee_dependents WHERE employee_id = ".db_escape($employee_id);
	$result = db_query($sql, 'could not count dependents');
	$row = db_fetch($result);
	return $row['cnt'];
}

