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

/**
 * Get an employee document record with optional attachment metadata.
 *
 * @param int $doc_id
 * @return array|false
 */
function get_employee_document($doc_id) {
	$sql = "SELECT d.*, t.type_name,
				a.id AS attachment_id, a.filename AS attachment_filename,
				a.filesize AS attachment_filesize, a.filetype AS attachment_filetype,
				a.unique_name AS attachment_unique_name
			FROM ".TB_PREF."employee_documents d
			LEFT JOIN ".TB_PREF."document_types t ON d.doc_type_id = t.doc_type_id
			LEFT JOIN ".TB_PREF."attachments a ON a.id = d.attachment_id
			WHERE d.doc_id = ".db_escape($doc_id);
	$result = db_query($sql, 'could not get employee document');
	return db_fetch_assoc($result);
}

/**
 * Get all documents for an employee with optional attachment metadata.
 *
 * @param string $employee_id  Employee code (e.g. 'EMP001').
 * @return resource  DB result set.
 */
function get_employee_documents($employee_id) {
	$sql = "SELECT d.*, t.type_name,
				a.id AS attachment_id, a.filename AS attachment_filename,
				a.filesize AS attachment_filesize, a.filetype AS attachment_filetype
			FROM ".TB_PREF."employee_documents d
			LEFT JOIN ".TB_PREF."document_types t ON d.doc_type_id = t.doc_type_id
			LEFT JOIN ".TB_PREF."attachments a ON a.id = d.attachment_id
			WHERE d.employee_id = ".db_escape($employee_id)."
			ORDER BY d.expiry_date DESC, d.doc_name";
	return db_query($sql, 'could not get employee documents');
}

function delete_employee_document($doc_id) {
	// First, clean up any linked core attachment.
	$doc = get_employee_document($doc_id);
	if ($doc && !empty($doc['attachment_id'])) {
		delete_attachment_with_file((int)$doc['attachment_id']);
	}
	$sql = "DELETE FROM ".TB_PREF."employee_documents WHERE doc_id = ".db_escape($doc_id);
	db_query($sql, 'could not delete employee document');
}

/**
 * Delete a document's linked core attachment without deleting the document row.
 * Used when a user removes just the file from an existing document.
 *
 * @param int $doc_id
 * @return bool  True if an attachment was cleaned up, false otherwise.
 */
function remove_employee_document_attachment($doc_id) {
	$doc = get_employee_document($doc_id);
	if (!$doc || empty($doc['attachment_id'])) {
		return false;
	}

	$attachment_id = (int)$doc['attachment_id'];
	delete_attachment_with_file($attachment_id);

	// Clear the link.
	$sql = "UPDATE ".TB_PREF."employee_documents SET attachment_id = NULL
		WHERE doc_id = ".db_escape($doc_id);
	db_query($sql, 'could not clear employee document attachment link');
	return true;
}

include_once($path_to_root . '/admin/db/attachments_db.inc');
include_once($path_to_root . '/includes/attachment_service.inc');

/**
 * Add an employee document with optional core attachment integration.
 *
 * This is the preferred function for new document creation. It handles
 * core attachment creation in the same operation as HR metadata storage.
 *
 * @param string $employee_id   Employee code (e.g. 'EMP001').
 * @param int    $employee_number Numeric employee primary key.
 * @param int    $doc_type_id   Document type ID.
 * @param string $doc_name      Document name/title.
 * @param array  $upload_info   Result from validate_attachment_upload(), or empty array for no file.
 * @param string $issue_date    Issue date (user format).
 * @param string $expiry_date   Expiry date (user format).
 * @param string $notes         Notes.
 * @return int|false  The new doc_id, or false on failure.
 */
function add_employee_document_with_attachment($employee_id, $employee_number, $doc_type_id, $doc_name, $upload_info = array(), $issue_date = '', $expiry_date = '', $notes = '') {
	$attachment_id = null;

	// If a file was uploaded, create the core attachment first.
	if (!empty($upload_info) && !empty($upload_info['ok'])) {
		$attachment_id = create_attachment(
			ST_EMPLOYEE,
			(int)$employee_number,
			$doc_name,
			$upload_info
		);
		if ($attachment_id === false) {
			return false;
		}
	}

	// Build the document INSERT.
	$sql = "INSERT INTO ".TB_PREF."employee_documents
		(employee_id, doc_type_id, doc_name, file_path, attachment_id";

	if (!empty($issue_date))  $sql .= ", issue_date";
	if (!empty($expiry_date)) $sql .= ", expiry_date";

	$sql .= ", notes, uploaded_by) VALUES ("
		.db_escape($employee_id).", "
		.db_escape($doc_type_id).", "
		.db_escape($doc_name).", '', "
		.($attachment_id !== null ? db_escape($attachment_id) : 'NULL');

	if (!empty($issue_date))  $sql .= ", '".date2sql($issue_date)."'";
	if (!empty($expiry_date)) $sql .= ", '".date2sql($expiry_date)."'";

	$sql .= ", ".db_escape($notes)
		.", ".db_escape($_SESSION['wa_current_user']->user).")";

	db_query($sql, 'could not add employee document');
	$doc_id = db_insert_id();

	// Store diagnostic linkage in attachment custom_data.
	if ($attachment_id !== null && $doc_id) {
		update_attachment_metadata($attachment_id, $doc_name, '', array(
			'employee_document_id' => (int)$doc_id,
			'migration_source'     => 'core_attachment_service',
		));
	}

	return $doc_id;
}

/**
 * Update an employee document, with optional file replacement through core attachments.
 *
 * If a new file is uploaded and an attachment already exists, the file is replaced atomically.
 * If a new file is uploaded and no attachment exists, one is created and linked.
 * If no new file is uploaded, the existing attachment (if any) is preserved.
 *
 * @param int    $doc_id          Document ID.
 * @param int    $employee_number Numeric employee primary key (for attachment ownership).
 * @param int    $doc_type_id    Document type ID.
 * @param string $doc_name       Document name/title.
 * @param array  $upload_info    Result from validate_attachment_upload(), or empty array for no file change.
 * @param string $issue_date     Issue date.
 * @param string $expiry_date    Expiry date.
 * @param string $notes          Notes.
 * @return bool  True on success, false on failure.
 */
function update_employee_document_with_attachment($doc_id, $employee_number, $doc_type_id, $doc_name, $upload_info = array(), $issue_date = '', $expiry_date = '', $notes = '') {
	$doc = get_employee_document($doc_id);
	if (!$doc) {
		return false;
	}

	$attachment_id = (int)$doc['attachment_id'];

	// Handle file upload/replacement.
	if (!empty($upload_info) && !empty($upload_info['ok'])) {
		if ($attachment_id > 0) {
			// Replace existing attachment file.
			if (!replace_attachment_file($attachment_id, $upload_info)) {
				return false;
			}
			// Update description to match the new doc_name.
			update_attachment_metadata($attachment_id, $doc_name);
		} else {
			// Create a new core attachment and link it.
			$attachment_id = create_attachment(
				ST_EMPLOYEE,
				(int)$employee_number,
				$doc_name,
				$upload_info
			);
			if ($attachment_id === false) {
				return false;
			}
		}
	} elseif ($attachment_id > 0) {
		// No new file, but synchronize description if doc_name changed.
		update_attachment_metadata($attachment_id, $doc_name);
	}

	// Build the UPDATE.
	$sql = "UPDATE ".TB_PREF."employee_documents SET "
		."doc_type_id = ".db_escape($doc_type_id).", "
		."doc_name = ".db_escape($doc_name).", "
		."attachment_id = ".($attachment_id > 0 ? db_escape($attachment_id) : 'NULL').", "
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
	return true;
}

/**
 * Resolve an employee by code and return the numeric employee_number (primary key).
 *
 * @param string $employee_id  Employee code (e.g. 'EMP001').
 * @return int|false  Numeric employee_number, or false if not found.
 */
function resolve_employee_number($employee_id) {
	$sql = "SELECT employee_number FROM ".TB_PREF."employees
		WHERE employee_id = ".db_escape($employee_id);
	$result = db_query($sql, 'could not resolve employee number');
	$row = db_fetch($result);
	return $row ? (int)$row['employee_number'] : false;
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

