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

function add_pay_element($element_name, $account_code, $is_deduction=0, $amount_type=0) {
	$sql= "INSERT INTO ".TB_PREF."pay_elements (element_name, account_code, is_deduction, amount_type) VALUES (".db_escape($element_name).",".db_escape($account_code).",".db_escape($is_deduction).",".db_escape($amount_type).")";
	
	db_query($sql, 'could not insert pay element');
}

function get_pay_element($id=false, $account_code=false) {
	$sql = "SELECT e.element_id, e.element_name, a.account_code, a.account_name, e.is_deduction, e.amount_type FROM ".TB_PREF."chart_master a, ".TB_PREF."pay_elements e WHERE a.account_code = e.account_code";

	if($id)
		$sql .= " AND e.element_id = ".db_escape($id);
	if($account_code)
		$sql .= " AND e.account_code = ".db_escape($account_code);
	
	$result = db_query($sql, 'Could not get pay element');

	return db_fetch($result);
}

function get_pay_elements() {
	$sql = "SELECT e.element_id, e.element_name, a.account_code, a.account_name, e.is_deduction, e.amount_type FROM ".TB_PREF."chart_master a, ".TB_PREF."pay_elements e WHERE a.account_code = e.account_code";

	return db_query($sql, 'Could not get pay elements data');
}

function update_pay_element($element_id, $element_name, $account_code, $is_deduction, $amount_type) {

	$sql = "UPDATE ".TB_PREF."pay_elements SET element_name = ".db_escape($element_name).", account_code = ".db_escape($account_code).", is_deduction = ".db_escape($is_deduction).", amount_type = ".db_escape($amount_type)." WHERE element_id = ".db_escape($element_id);

	db_query($sql, 'could not update pay element');
}

function delete_pay_element($element_id) {

	$sql = "DELETE FROM ".TB_PREF."pay_elements WHERE element_id=".db_escape($element_id);
	$result = db_query($sql, 'Could not delete pay element');
}

function check_pay_element_duplicated($elment_id, $account_code) {
	return check_empty_result("SELECT account_code FROM ".TB_PREF."pay_elements WHERE account_code=".db_escape($account_code)." AND element_id != ".db_escape($elment_id));
}

function pay_element_used($elment_id) {

	return check_empty_result("SELECT e.element_id, p.payroll_rule, e.account_code FROM ".TB_PREF."payroll_structure p, ".TB_PREF."pay_element e WHERE e.element_id=".db_escape($elment_id)." AND p.payroll_rule LIKE CONCAT('%',e.account_code,'%')");
}
