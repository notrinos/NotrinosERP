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

function db_has_employees() {
	return check_empty_result("SELECT COUNT(*) FROM ".TB_PREF."employees");
}

function add_employee($employee_id, $first_name, $last_name, $gender, $birth_date, $address='', $mobile='', $email='', $national_id='', $passport='', $bank_account='', $tax_number='',$marital_status=0, $dependents_no=0, $notes='', $hire_date='', $department_id=0, $position_id=0, $grade_id=0, $personal_salary=0, $user_id=0) {
	$sql = "INSERT INTO ".TB_PREF."employees (employee_id, first_name, last_name, gender, birth_date, address, mobile, email, national_id, passport, bank_account, tax_number, marital_status, dependents_no, notes,";
	if(!empty($hire_date))
		$sql .= " hire_date,";
	$sql .= " department_id, position_id, grade_id, personal_salary, user_id) VALUES (".db_escape($employee_id).",".db_escape($first_name).",".db_escape($last_name).",".db_escape($gender).",'".date2sql($birth_date)."',".db_escape($address).",".db_escape($mobile).",".db_escape($email).",".db_escape($national_id).",".db_escape($passport).",".db_escape($bank_account).",".db_escape($tax_number).",".db_escape($marital_status).",".db_escape($dependents_no).",".db_escape($notes).",";
	if(!empty($hire_date))
		$sql .= "'".date2sql($hire_date)."',";
	$sql .= db_escape($department_id).",".db_escape($position_id).",".db_escape($grade_id).",".db_escape($personal_salary).",".db_escape($user_id).")";

	db_query($sql, 'could not add employee record');
}

function update_employee($employee_id, $first_name, $last_name, $gender, $birth_date, $address='', $mobile='', $email='', $national_id='', $passport='', $bank_account='', $tax_number='', $marital_status=0, $dependents_no=0, $notes='', $hire_date='', $department_id=0, $position_id=0, $grade_id=0, $personal_salary=0, $released_date='', $user_id=0) {
	$sql = "UPDATE ".TB_PREF."employees SET 
		first_name = ".db_escape($first_name).", 
		last_name = ".db_escape($last_name).", 
		gender = ".db_escape($gender).", 
		address = ".db_escape($address).", 
		mobile = ".db_escape($mobile).", 
		email = ".db_escape($email).", 
		birth_date = '".date2sql($birth_date)."', 
		national_id = ".db_escape($national_id).", 
		passport = ".db_escape($passport).", 
		bank_account = ".db_escape($bank_account).", 
		tax_number = ".db_escape($tax_number).", 
		marital_status = ".db_escape($marital_status).", 
		dependents_no = ".db_escape($dependents_no).", 
		notes = ".db_escape($notes).", ";

		if(!empty($hire_date))
			$sql .= " hire_date = '".date2sql($hire_date)."', ";

	$sql .=	"department_id = ".db_escape($department_id).", 
		position_id = ".db_escape($position_id).", 
		grade_id = ".db_escape($grade_id).", 
		personal_salary = ".db_escape($personal_salary).", ";

		if(!empty($released_date))
			$sql .= " released_date = '".date2sql($released_date)."', ";
		
	$sql .= "user_id = ".db_escape($user_id)." WHERE employee_id = ".db_escape($employee_id);

	db_query($sql, 'could not update employee');
}

function get_employee($employee_number) {
	$sql = "SELECT * FROM ".TB_PREF."employees WHERE employee_number = ".db_escape($employee_number);
	$result = db_query($sql, 'failed getting an employee record');

	return db_fetch_assoc($result);
}

function get_employee_by_code($employee_id) {
	$sql = "SELECT * FROM ".TB_PREF."employees WHERE employee_id = ".db_escape($employee_id);
	$result = db_query($sql, 'could not get employee data');

	return db_fetch_assoc($result);
}

function get_employees($department_id=false, $all=false) {
	$sql = "SELECT * FROM ".TB_PREF."employees WHERE !inactive";

	if(!empty($department_id))
		$sql .= " AND department_id = ".db_escape($department_id);
	if(!empty($all))
		$sql .= " AND inactive = 1";

	return db_query($sql, 'could not get employees data');
}

function delete_employee($employee_number) {
	$sql = "DELETE FROM ".TB_PREF."employee WHERE employee_number = ".db_escape($employee_number);
	db_query($sql, 'Could not delete employee data.');
}
