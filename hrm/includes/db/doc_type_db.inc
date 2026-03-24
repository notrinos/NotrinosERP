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
 * Employee document type CRUD
 */

function add_document_type($type_name, $notify_before=30, $is_required=0) {
	$sql = "INSERT INTO ".TB_PREF."document_types (type_name, notify_before, is_required) VALUES ("
		.db_escape($type_name).","
		.db_escape($notify_before).","
		.db_escape($is_required).")";
	db_query($sql, 'could not add document type');
	return db_insert_id();
}

function update_document_type($doc_type_id, $type_name, $notify_before=30, $is_required=0) {
	$sql = "UPDATE ".TB_PREF."document_types SET "
		."type_name = ".db_escape($type_name).", "
		."notify_before = ".db_escape($notify_before).", "
		."is_required = ".db_escape($is_required)
		." WHERE doc_type_id = ".db_escape($doc_type_id);
	db_query($sql, 'could not update document type');
}

function get_document_type($doc_type_id) {
	$sql = "SELECT * FROM ".TB_PREF."document_types WHERE doc_type_id = ".db_escape($doc_type_id);
	$result = db_query($sql, 'could not get document type');
	return db_fetch_assoc($result);
}

function get_document_types($all=false) {
	$sql = "SELECT * FROM ".TB_PREF."document_types";
	if (!$all)
		$sql .= " WHERE !inactive";
	$sql .= " ORDER BY type_name";
	return db_query($sql, 'could not get document types');
}

function delete_document_type($doc_type_id) {
	// Check if type is used
	if (check_empty_result("SELECT COUNT(*) FROM ".TB_PREF."employee_documents WHERE doc_type_id = ".db_escape($doc_type_id)))
		return false;
	$sql = "DELETE FROM ".TB_PREF."document_types WHERE doc_type_id = ".db_escape($doc_type_id);
	db_query($sql, 'could not delete document type');
	return true;
}

