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
 * Employee document management CRUD
 */

function add_employee_document($employee_id, $doc_type_id, $doc_name, $file_path='', $issue_date='', $expiry_date='', $notes='') {
	$sql = "INSERT INTO ".TB_PREF."employee_documents (employee_id, doc_type_id, doc_name, file_path";

	if (!empty($issue_date))  $sql .= ", issue_date";
	if (!empty($expiry_date)) $sql .= ", expiry_date";

	$sql .= ", notes, uploaded_by) VALUES ("
		.db_escape($employee_id).","
		.db_escape($doc_type_id).","
		.db_escape($doc_name).","
		.db_escape($file_path);

	if (!empty($issue_date))  $sql .= ", '".date2sql($issue_date)."'";
	if (!empty($expiry_date)) $sql .= ", '".date2sql($expiry_date)."'";

	$sql .= ", ".db_escape($notes)
		.", ".db_escape($_SESSION['wa_current_user']->user).")";

	db_query($sql, 'could not add employee document');
	return db_insert_id();
}

function update_employee_document($doc_id, $doc_type_id, $doc_name, $file_path='', $issue_date='', $expiry_date='', $notes='') {
	$sql = "UPDATE ".TB_PREF."employee_documents SET "
		."doc_type_id = ".db_escape($doc_type_id).", "
		."doc_name = ".db_escape($doc_name).", "
		."file_path = ".db_escape($file_path).", "
		."notes = ".db_escape($notes);

	if (!empty($issue_date))
		$sql .= ", issue_date = '".date2sql($issue_date)."'";
	else
		$sql .= ", issue_date = NULL";

	if (!empty($expiry_date))
		$sql .= ", expiry_date = '".date2sql($expiry_date)."'";
	else
		$sql .= ", expiry_date = NULL";

	$sql .= " WHERE doc_id = ".db_escape($doc_id);

	db_query($sql, 'could not update employee document');
}

function get_employee_document($doc_id) {
	$sql = "SELECT d.*, t.type_name
			FROM ".TB_PREF."employee_documents d
			LEFT JOIN ".TB_PREF."document_types t ON d.doc_type_id = t.doc_type_id
			WHERE d.doc_id = ".db_escape($doc_id);
	$result = db_query($sql, 'could not get employee document');
	return db_fetch_assoc($result);
}

function get_employee_documents($employee_id) {
	$sql = "SELECT d.*, t.type_name
			FROM ".TB_PREF."employee_documents d
			LEFT JOIN ".TB_PREF."document_types t ON d.doc_type_id = t.doc_type_id
			WHERE d.employee_id = ".db_escape($employee_id)."
			ORDER BY d.expiry_date DESC, d.doc_name";
	return db_query($sql, 'could not get employee documents');
}

function delete_employee_document($doc_id) {
	$sql = "DELETE FROM ".TB_PREF."employee_documents WHERE doc_id = ".db_escape($doc_id);
	db_query($sql, 'could not delete employee document');
}

function get_expiring_documents($days_ahead=30) {
	$future = date('Y-m-d', strtotime("+$days_ahead days"));
	$sql = "SELECT d.*, t.type_name, e.first_name, e.last_name, e.employee_id
			FROM ".TB_PREF."employee_documents d
			JOIN ".TB_PREF."document_types t ON d.doc_type_id = t.doc_type_id
			JOIN ".TB_PREF."employees e ON d.employee_id = e.employee_id
			WHERE d.expiry_date IS NOT NULL
			AND d.expiry_date <= '$future'
			AND d.expiry_date >= CURDATE()
			ORDER BY d.expiry_date ASC";
	return db_query($sql, 'could not get expiring documents');
}

