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
 * This endpoint does NOT require SA_ATTACHDOCUMENT. Employee-document bytes
 * are a restricted HR field class and require SA_EMPLOYEE. The broader
 * SA_EMPLOYEEREP capability may view document metadata only.
 *
 * Usage:
 *   view:  hrm/view/employee_document_attachment.php?key=<opaque-key>
 *   download: hrm/view/employee_document_attachment.php?key=<opaque-key>&dl=1
 */

$path_to_root = '../..';
$page_security = 'SA_OPEN'; // Authorization is handled explicitly below.

include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/includes/ui/ui_msgs.inc');
include_once($path_to_root . '/hrm/includes/db/employee_document_db.inc');
include_once($path_to_root . '/hrm/includes/db/employee_db.inc');
include_once($path_to_root . '/hrm/includes/hrm_security.inc');
include_once($path_to_root . '/admin/db/attachments_db.inc');
include_once($path_to_root . '/includes/attachment_service.inc');

// --- Authorization ---

// Require authentication.
if (!isset($_SESSION['wa_current_user']) || !$_SESSION['wa_current_user']->logged_in()) {
    header('HTTP/1.0 403 Forbidden');
    exit();
}

// Classified document metadata does not yet supply subject/relationship/legal-
// entity authority. Treat every current category as restricted; broader report
// access and unknown classes fail shut.
if (!hrm_user_can_access_sensitive_field(
	HRM_FIELD_RESTRICTED_DOCUMENT,
	HRM_FIELD_ACTION_VIEW
)) {
	hrm_log_sensitive_field_access(
		HRM_FIELD_RESTRICTED_DOCUMENT,
		HRM_FIELD_ACTION_VIEW,
		'denied_content_stream'
	);
	header('Cache-Control: private, no-store, max-age=0');
	header('X-Content-Type-Options: nosniff');
    header('HTTP/1.0 403 Forbidden');
    exit();
}

// --- Input validation ---

$access_key = isset($_GET['key']) ? strtolower(trim((string)$_GET['key'])) : '';
if (!preg_match('/^[a-f0-9]{64}$/D', $access_key)) {
    header('HTTP/1.0 400 Bad Request');
    echo _('Invalid document reference.');
    exit();
}

// --- Load the employee document ---

$doc = get_employee_document_by_access_key($access_key);
if (!$doc) {
    header('HTTP/1.0 404 Not Found');
    echo _('Document not found.');
    exit();
}

// --- Verify linked core attachment ---

if (empty($doc['attachment_id']) || !isset($doc['content_state']) || $doc['content_state'] !== 'available') {
    // Legacy fallback is retired after PAY-SEC-002 reconciliation. Metadata-only,
    // quarantined, failed and unavailable content never streams.
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

$custom = attachment_custom_data_array(isset($attachment['custom_data']) ? $attachment['custom_data'] : '');
if (!isset($custom['storage_backend']) || $custom['storage_backend'] !== 'hrm_private_v1'
	|| !isset($custom['content_state']) || $custom['content_state'] !== 'available'
	|| !isset($custom['plaintext_sha256'])
	|| !attachment_hash_equals($custom['plaintext_sha256'], $doc['content_sha256'])
) {
	header('HTTP/1.0 404 Not Found');
	exit();
}

// --- Stream via core attachment service ---

$outcome = isset($_GET['dl']) ? 'granted_content_download' : 'granted_content_view';
hrm_log_sensitive_field_access(
	HRM_FIELD_RESTRICTED_DOCUMENT,
	HRM_FIELD_ACTION_VIEW,
	$outcome
);
$mode = isset($_GET['dl']) ? 'download' : 'inline';
stream_attachment_file($attachment, $mode);
