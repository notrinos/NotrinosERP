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
 * HR-Authorized Employee Document Attachment Viewer / Downloader.
 *
 * Provides a safe, permission-checked endpoint for viewing and downloading
 * employee document files stored through the core attachment subsystem.
 *
 * This endpoint does NOT require SA_ATTACHDOCUMENT. Instead, it uses
 * SA_EMPLOYEE and SA_EMPLOYEEREP, matching the HR permissions already
 * required to view the employee's Documents tab or Employee Card.
 *
 * Usage:
 *   view:  hrm/view/employee_document_attachment.php?doc_id=123
 *   download: hrm/view/employee_document_attachment.php?doc_id=123&dl=1
 */

$path_to_root = '../..';
$page_security = 'SA_OPEN'; // Authorization is handled explicitly below.

include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/includes/ui/ui_msgs.inc');
include_once($path_to_root . '/hrm/includes/db/employee_document_db.inc');
include_once($path_to_root . '/hrm/includes/db/employee_db.inc');
include_once($path_to_root . '/admin/db/attachments_db.inc');
include_once($path_to_root . '/includes/attachment_service.inc');

// --- Authorization ---

// Require authentication.
if (!isset($_SESSION['wa_current_user']) || !$_SESSION['wa_current_user']->logged_in()) {
    header('HTTP/1.0 403 Forbidden');
    exit();
}

// Require at least one HR permission that exposes document viewing:
// SA_EMPLOYEE (Manage Employees) or SA_EMPLOYEEREP (Employee Card / Reports).
// Uses the standard NotrinosERP access-control API.
if (!user_check_access('SA_EMPLOYEE') && !user_check_access('SA_EMPLOYEEREP')) {
    header('HTTP/1.0 403 Forbidden');
    exit();
}

// --- Input validation ---

$doc_id = isset($_GET['doc_id']) ? (int)$_GET['doc_id'] : 0;
if ($doc_id <= 0) {
    header('HTTP/1.0 400 Bad Request');
    echo _('Invalid document ID.');
    exit();
}

// --- Load the employee document ---

$doc = get_employee_document($doc_id);
if (!$doc) {
    header('HTTP/1.0 404 Not Found');
    echo _('Document not found.');
    exit();
}

// --- Verify linked core attachment ---

if (empty($doc['attachment_id'])) {
    // Legacy fallback: document has no core attachment.
    // Stream the legacy file only after safe-path validation.
    if (!empty($doc['file_path'])) {
        $canonical_path = canonical_legacy_employee_document_path($doc['file_path']);
        if ($canonical_path === false) {
            header('HTTP/1.0 404 Not Found');
            exit();
        }
        // Safety: never expose the raw file_path in HTML or headers.
        stream_legacy_file_safely($canonical_path, $doc['doc_name']);
        // stream_legacy_file_safely() exits internally.
    }
    // Metadata-only document (no file at all).
    header('HTTP/1.0 404 Not Found');
    exit();
}

$attachment = get_attachment((int)$doc['attachment_id']);
if (!$attachment) {
    header('HTTP/1.0 404 Not Found');
    exit();
}

// --- Verify attachment belongs to ST_EMPLOYEE ---

if ((int)$attachment['type_no'] !== ST_EMPLOYEE) {
    header('HTTP/1.0 403 Forbidden');
    exit();
}

// --- Verify attachment belongs to the correct employee ---

$employee_number = resolve_employee_number($doc['employee_id']);
if ($employee_number === false) {
    header('HTTP/1.0 404 Not Found');
    exit();
}

if ((int)$attachment['trans_no'] !== $employee_number) {
    header('HTTP/1.0 403 Forbidden');
    exit();
}

// --- Stream via core attachment service ---

$mode = isset($_GET['dl']) ? 'download' : 'inline';
stream_attachment_file($attachment, $mode);
