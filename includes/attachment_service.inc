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
 * Centralized Attachment Service
 *
 * Provides a reusable service layer for core attachment file operations:
 * validation, storage, replacement, deletion, and streaming.
 *
 * This service is UI-independent and does not perform authorization.
 * The calling page must authorize the user and entity before calling.
 *
 * Used by:
 *   - admin/attachments.php (customer, supplier, item, fixed asset, bank account)
 *   - includes/ui/attachment.inc (shared attachment UI class)
 *   - hrm/manage/employees.php (employee document attachments)
 *
 * @package Attachment
 */

/**
 * Validate an uploaded file against an attachment policy.
 *
 * @param array  $upload  The $_FILES element to validate.
 * @param array  $policy  Validation policy:
 *                        - 'max_size'        int   Maximum file size in bytes (default 5 MB).
 *                        - 'allowed_types'   array List of allowed MIME types (null = no MIME check).
 *                        - 'allowed_ext'     array List of allowed lowercase extensions (null = no ext check).
 *                        - 'ext_from_mime'   bool  Derive allowed extensions from allowed_types (default false).
 * @return array  ['ok' => bool, 'error' => string, 'filename' => string, 'size' => int, 'tmp_name' => string]
 */
function validate_attachment_upload($upload, $policy = array()) {
    $default_policy = array(
        'max_size'      => 5 * 1024 * 1024, // 5 MB
        'allowed_types' => null,
        'allowed_ext'   => null,
        'ext_from_mime' => false,
    );
    $policy = array_merge($default_policy, $policy);

    // Check upload errors.
    if (!isset($upload['error']) || is_array($upload['error'])) {
        return array('ok' => false, 'error' => _('Invalid upload parameters.'), 'filename' => '', 'size' => 0, 'tmp_name' => '');
    }

    $error = (int)$upload['error'];
    if ($error === UPLOAD_ERR_NO_FILE) {
        return array('ok' => false, 'error' => _('No file was uploaded.'), 'filename' => '', 'size' => 0, 'tmp_name' => '');
    }
    if ($error === UPLOAD_ERR_INI_SIZE || $error === UPLOAD_ERR_FORM_SIZE) {
        return array('ok' => false, 'error' => _('The file size is over the maximum allowed.'), 'filename' => '', 'size' => 0, 'tmp_name' => '');
    }
    if ($error !== UPLOAD_ERR_OK) {
        return array('ok' => false, 'error' => _('File upload failed with error code ') . $error . '.', 'filename' => '', 'size' => 0, 'tmp_name' => '');
    }

    $tmp_name = $upload['tmp_name'];
    $size     = (int)$upload['size'];
    $client   = isset($upload['name']) ? $upload['name'] : '';

    // Basic sanity checks.
    if ($size < 0) {
        return array('ok' => false, 'error' => _('Invalid file size.'), 'filename' => $client, 'size' => 0, 'tmp_name' => $tmp_name);
    }
    if ($tmp_name === '' || !is_uploaded_file($tmp_name)) {
        return array('ok' => false, 'error' => _('File is missing or not a valid upload.'), 'filename' => $client, 'size' => 0, 'tmp_name' => '');
    }
    if ($size === 0) {
        return array('ok' => false, 'error' => _('Uploaded file is empty.'), 'filename' => $client, 'size' => 0, 'tmp_name' => $tmp_name);
    }
    if ($size > $policy['max_size']) {
        return array('ok' => false, 'error' => _('File size exceeds the allowed limit.'), 'filename' => $client, 'size' => $size, 'tmp_name' => $tmp_name);
    }

    $filename = sanitize_attachment_filename($client);
    if ($filename === '') {
        return array('ok' => false, 'error' => _('Invalid file name.'), 'filename' => $client, 'size' => $size, 'tmp_name' => $tmp_name);
    }

    // Determine extension and MIME type.
    $ext     = strtolower(pathinfo($filename, PATHINFO_EXTENSION));
    $mime    = detect_attachment_mime($tmp_name, $upload);

    // Extension allowlist check.
    if (is_array($policy['allowed_ext']) && !empty($policy['allowed_ext'])) {
        if (!in_array($ext, $policy['allowed_ext'])) {
            return array('ok' => false, 'error' => _('File type not allowed. Allowed types: ') . implode(', ', $policy['allowed_ext']), 'filename' => $filename, 'size' => $size, 'tmp_name' => $tmp_name);
        }
    }

    // MIME allowlist check.
    if (is_array($policy['allowed_types']) && !empty($policy['allowed_types'])) {
        if (!in_array($mime, $policy['allowed_types'])) {
            return array('ok' => false, 'error' => _('File content type not allowed.'), 'filename' => $filename, 'size' => $size, 'tmp_name' => $tmp_name);
        }
    }

    return array(
        'ok'       => true,
        'error'    => '',
        'filename' => $filename,
        'size'     => $size,
        'mime'     => $mime,
        'tmp_name' => $tmp_name,
    );
}

/**
 * Detect MIME type server-side. Does not trust $_FILES['type'] alone.
 *
 * @param string $tmp_path  Path to the uploaded temporary file.
 * @param array  $upload    The $_FILES element (used as fallback).
 * @return string  Detected MIME type or 'application/octet-stream'.
 */
function detect_attachment_mime($tmp_path, $upload = array()) {
    // Use finfo when available (PHP 5.3+).
    if (function_exists('finfo_open') && file_exists($tmp_path)) {
        $finfo = finfo_open(FILEINFO_MIME_TYPE);
        if ($finfo) {
            $mime = finfo_file($finfo, $tmp_path);
            finfo_close($finfo);
            if ($mime !== false && $mime !== '') {
                return $mime;
            }
        }
    }

    // Fallback: mime_content_type (deprecated in PHP 8.1 but available).
    if (function_exists('mime_content_type') && file_exists($tmp_path)) {
        $mime = @mime_content_type($tmp_path);
        if ($mime !== false && $mime !== '') {
            return $mime;
        }
    }

    // Last resort: use the browser-supplied type.
    if (!empty($upload['type'])) {
        return $upload['type'];
    }

    return 'application/octet-stream';
}

/**
 * Sanitize a client-supplied filename to a safe basename.
 * Strips path separators, control characters, and restricts length.
 *
 * @param string $filename  Raw client filename.
 * @return string  Safe basename or empty string if invalid.
 */
function sanitize_attachment_filename($filename) {
    if ($filename === '' || $filename === null) {
        return '';
    }

    // Strip path components; we only want the basename.
    $filename = basename($filename);

    // Reject filenames with path separators or null bytes after basename extraction.
    if (strpbrk($filename, "\\/\0") !== false || strpos($filename, '..') !== false) {
        return '';
    }

    // Remove control characters and trim.
    $filename = preg_replace('/[\x00-\x1f\x7f]/', '', $filename);
    $filename = trim($filename);

    // Limit length to 200 characters (column size).
    if (strlen($filename) > 200) {
        $filename = substr($filename, 0, 200);
    }

    return $filename;
}

/**
 * Get the absolute safe path for an attachment unique_name within company_path()/attachments.
 * Prevents path traversal attacks.
 *
 * @param string $unique_name  The stored attachment filename.
 * @return string|false  Resolved safe absolute path, or false if unsafe/missing.
 */
function get_safe_attachment_path($unique_name) {
    if ($unique_name === '' || $unique_name === null) {
        return false;
    }
    // Reject any path separators, null bytes, or parent directory traversal sequences.
    if (strpbrk($unique_name, "\\/\0") !== false || strpos($unique_name, '..') !== false) {
        return false;
    }

    $attach_dir = company_path() . '/attachments';
    $full_path  = realpath($attach_dir . '/' . $unique_name);

    if ($full_path === false) {
        return false;
    }

    $real_attach_dir = realpath($attach_dir);
    if ($real_attach_dir === false) {
        return false;
    }

    // Ensure the resolved path is strictly within the attachments directory.
    if (strpos($full_path, $real_attach_dir . DIRECTORY_SEPARATOR) !== 0) {
        return false;
    }

    return $full_path;
}

/**
 * Ensure the company attachments directory exists with an index.php redirect.
 *
 * @return string  The absolute path to the attachments directory.
 */
function ensure_attachments_directory() {
    $dir = company_path() . '/attachments';

    if (!file_exists($dir)) {
        @mkdir($dir, 0777, true);
    }

    // Ensure index.php redirect exists.
    $index_file = $dir . '/index.php';
    if (!file_exists($index_file)) {
        $content = "<?php\nheader(\"Location: ../index.php\");\n";
        @file_put_contents($index_file, $content);
    }

    return $dir;
}

/**
 * Generate a random server-side unique storage name.
 * The caller should verify uniqueness before using.
 *
 * @return string  A random hex-based filename (no extension).
 */
function generate_attachment_unique_name() {
    if (function_exists('random_id')) {
        return random_id();
    }
    // Fallback: generate a random hex string.
    return bin2hex(openssl_random_pseudo_bytes(16));
}

/**
 * Create a new core attachment record and store the uploaded file.
 *
 * @param int    $type_no     Attachment entity type (ST_CUSTOMER, ST_SUPPLIER, ST_EMPLOYEE, etc.).
 * @param int    $trans_no    Numeric entity identifier (customer_id, supplier_id, employee_number, etc.).
 * @param string $description Human-readable document title / description.
 * @param array  $upload_info Result from validate_attachment_upload() with keys: filename, size, mime, tmp_name.
 * @param string $tran_date   SQL date string for the attachment record (use Today() for current date).
 * @param array  $custom_data Optional associative array stored as JSON in attachments.custom_data.
 * @return int|false  The new attachment ID, or false on failure.
 */
function create_attachment($type_no, $trans_no, $description, $upload_info, $tran_date = '', $custom_data = array()) {
    if ($tran_date === '') {
        $tran_date = date2sql(Today());
    }

    $filename    = $upload_info['filename'];
    $filesize    = $upload_info['size'];
    $filetype    = $upload_info['mime'];
    $tmp_name    = $upload_info['tmp_name'];
    $unique_name = generate_attachment_unique_name();

    $dir = ensure_attachments_directory();

    // Stage the uploaded file.
    $dest_path = $dir . '/' . $unique_name;
    if (!move_uploaded_file($tmp_name, $dest_path)) {
        return false;
    }

    // Verify the write.
    clearstatcache(true, $dest_path);
    if (!file_exists($dest_path) || filesize($dest_path) !== $filesize) {
        @unlink($dest_path);
        return false;
    }

    // Build custom_data JSON.
    $custom_json = '{}';
    if (!empty($custom_data) && is_array($custom_data)) {
        $custom_json = json_encode($custom_data, JSON_UNESCAPED_UNICODE);
        if ($custom_json === false) {
            $custom_json = '{}';
        }
    }

    $sql = "INSERT INTO " . TB_PREF . "attachments
        (type_no, trans_no, description, filename, unique_name, filesize, filetype, tran_date, custom_data)
        VALUES ("
        . db_escape($type_no) . ", "
        . db_escape((int)$trans_no) . ", "
        . db_escape($description) . ", "
        . db_escape($filename) . ", "
        . db_escape($unique_name) . ", "
        . db_escape($filesize) . ", "
        . db_escape($filetype) . ", '"
        . $tran_date . "', "
        . db_escape($custom_json) . ")";

    db_query($sql, 'Attachment could not be inserted');

    return db_insert_id();
}

/**
 * Replace the file of an existing attachment with a new upload.
 * Atomic: old file is only deleted after the DB is updated successfully.
 *
 * @param int   $attachment_id  The attachment record ID to update.
 * @param array $upload_info    Result from validate_attachment_upload() with keys: filename, size, mime, tmp_name.
 * @return bool  True on success, false on failure.
 */
function replace_attachment_file($attachment_id, $upload_info) {
    // Load the existing attachment.
    $existing = get_attachment($attachment_id);
    if (!$existing) {
        return false;
    }

    $filename    = $upload_info['filename'];
    $filesize    = $upload_info['size'];
    $filetype    = $upload_info['mime'];
    $tmp_name    = $upload_info['tmp_name'];
    $unique_name = generate_attachment_unique_name();

    $dir = ensure_attachments_directory();

    // Stage the new file under a new random name.
    $new_path = $dir . '/' . $unique_name;
    if (!move_uploaded_file($tmp_name, $new_path)) {
        return false;
    }

    // Verify the write.
    clearstatcache(true, $new_path);
    if (!file_exists($new_path) || filesize($new_path) !== $filesize) {
        @unlink($new_path);
        return false;
    }

    // Update database record.
    $sql = "UPDATE " . TB_PREF . "attachments SET
        filename    = " . db_escape($filename) . ",
        unique_name = " . db_escape($unique_name) . ",
        filesize    = " . db_escape($filesize) . ",
        filetype    = " . db_escape($filetype) . ",
        tran_date   = '" . date2sql(Today()) . "'
        WHERE id    = " . db_escape($attachment_id);

    $result = db_query($sql, 'Attachment file could not be replaced');

    if ($result) {
        // Success: delete the old physical file.
        $old_path = get_safe_attachment_path($existing['unique_name']);
        if ($old_path !== false && file_exists($old_path)) {
            @unlink($old_path);
        }
        return true;
    }

    // Database update failed: clean up the new file.
    @unlink($new_path);
    return false;
}

/**
 * Update attachment metadata without changing the file.
 *
 * @param int    $attachment_id  The attachment record ID.
 * @param string $description    New description/title.
 * @param string $tran_date      New transaction date (SQL format).
 * @param array  $custom_data    New custom data associative array.
 * @return bool  True on success.
 */
function update_attachment_metadata($attachment_id, $description, $tran_date = '', $custom_data = null) {
    $sets = array(
        'description = ' . db_escape($description),
    );

    if ($tran_date !== '') {
        $sets[] = "tran_date = '" . date2sql($tran_date) . "'";
    }

    if ($custom_data !== null && is_array($custom_data)) {
        $custom_json = json_encode($custom_data, JSON_UNESCAPED_UNICODE);
        if ($custom_json === false) {
            $custom_json = '{}';
        }
        $sets[] = 'custom_data = ' . db_escape($custom_json);
    }

    $sql = "UPDATE " . TB_PREF . "attachments SET " . implode(', ', $sets)
        . " WHERE id = " . db_escape($attachment_id);

    db_query($sql, 'Attachment metadata could not be updated');
    return true;
}

/**
 * Delete an attachment record and its physical file.
 * The caller must authorize this operation.
 *
 * @param int $attachment_id  The attachment record ID to delete.
 * @return bool  True if deleted (or already missing), false on DB error.
 */
function delete_attachment_with_file($attachment_id) {
    $existing = get_attachment($attachment_id);
    if (!$existing) {
        // Already gone.
        return true;
    }

    // Delete the physical file first.
    $safe_path = get_safe_attachment_path($existing['unique_name']);
    if ($safe_path !== false && file_exists($safe_path)) {
        @unlink($safe_path);
    }

    // Delete the database record.
    delete_attachment($attachment_id);

    return true;
}

/**
 * Stream an attachment file to the browser.
 * Mode = 'inline' displays in browser; 'download' forces download.
 *
 * This function outputs the file and exits. The caller must have already
 * performed authorization checks and cleaned output buffers.
 *
 * @param array  $attachment  Attachment DB row (from get_attachment()).
 * @param string $mode        'inline' or 'download'.
 */
function stream_attachment_file($attachment, $mode = 'inline') {
    $safe_path = get_safe_attachment_path($attachment['unique_name']);
    if ($safe_path === false || !file_exists($safe_path)) {
        display_error(_('Attachment file not found or path is invalid.'));
        exit();
    }

    $type     = (!empty($attachment['filetype'])) ? $attachment['filetype'] : 'application/octet-stream';
    $filename = $attachment['filename'];

    // Prevent inline execution of active content.
    $dangerous_inline = array('text/html', 'application/xhtml+xml', 'text/javascript',
        'application/javascript', 'application/x-httpd-php', 'application/x-shockwave-flash');
    if (in_array($type, $dangerous_inline)) {
        $type = 'application/octet-stream';
        $mode = 'download';
    }

    // Clean output buffer to prevent output_html callback from corrupting binary data.
    while (ob_get_level()) {
        ob_end_clean();
    }

    header('Content-Type: ' . $type);
    header('Content-Length: ' . $attachment['filesize']);

    if ($mode === 'download') {
        // Escape filename for Content-Disposition header.
        $safe_filename = str_replace(array('"', '\\', "\n", "\r"), '_', $filename);
        if ($safe_filename === '') {
            $safe_filename = 'attachment';
        }
        header('Content-Disposition: attachment; filename="' . $safe_filename . '"');
    } else {
        header('Content-Disposition: inline');
    }

    readfile($safe_path);
    exit();
}

/**
 * Verify that an attachment belongs to a specific entity type and ID.
 *
 * @param int $attachment_id  The attachment record ID.
 * @param int $type_no        Expected entity type (ST_CUSTOMER, ST_SUPPLIER, ST_EMPLOYEE, etc.).
 * @param int $trans_no       Expected entity numeric identifier.
 * @return bool  True if the attachment belongs to the specified entity.
 */
function attachment_belongs_to_entity($attachment_id, $type_no, $trans_no) {
    $attachment = get_attachment($attachment_id);
    if (!$attachment) {
        return false;
    }
    return ((int)$attachment['type_no'] === (int)$type_no && (int)$attachment['trans_no'] === (int)$trans_no);
}

/**
 * Get the full HR-document-friendly display column set for an attachment.
 * Joins attachments to employee_documents and returns display-safe fields.
 *
 * @param int $attachment_id  The attachment ID.
 * @return array|false  Attachment row with display metadata, or false.
 */
function get_attachment_display_info($attachment_id) {
    $sql = "SELECT id, description AS doc_title, filename, filesize, filetype, tran_date, type_no, trans_no
        FROM " . TB_PREF . "attachments WHERE id = " . db_escape($attachment_id);
    $result = db_query($sql, 'Could not retrieve attachment display info');
    return db_fetch_assoc($result);
}

// -----------------------------------------------------------------------
// HR Employee Document helpers
// -----------------------------------------------------------------------

/**
 * Check whether an attachment row is an HR employee document.
 *
 * This centralized helper blocks generic attachment administration from
 * accessing employee documents. Call this before any view/download/edit/
 * replace/delete operation on a loaded attachment row.
 *
 * @param array|false $attachment  The attachment row from get_attachment().
 * @return bool  True if the attachment is an ST_EMPLOYEE attachment.
 */
function attachment_is_hr_employee_document($attachment) {
    return $attachment
        && isset($attachment['type_no'])
        && (int)$attachment['type_no'] === ST_EMPLOYEE;
}

/**
 * Canonicalize and safely validate a legacy employee document file_path.
 *
 * Only accepts paths strictly inside company_path()/documents/employees/.
 * Rejects traversal, null bytes, symlink escapes, and non-existent files.
 *
 * @param string $file_path  Raw file_path from employee_documents.
 * @return string|false  Canonical absolute path, or false if unsafe/missing.
 */
function canonical_legacy_employee_document_path($file_path) {
    if (empty($file_path) || !is_string($file_path)) {
        return false;
    }

    // Reject traversal sequences and null bytes.
    if (strpbrk($file_path, "\0") !== false
        || strpos($file_path, '..') !== false
    ) {
        return false;
    }

    $legacy_dir = company_path() . '/documents/employees';
    $real_legacy_dir = realpath($legacy_dir);
    if ($real_legacy_dir === false) {
        return false;
    }

    // Resolve the full canonical path.
    $full_path = realpath($file_path);
    if ($full_path === false) {
        return false;
    }

    // Ensure the resolved path is strictly within the legacy employee documents directory.
    $dir_sep = DIRECTORY_SEPARATOR;
    if (strpos($full_path, $real_legacy_dir . $dir_sep) !== 0
        && $full_path !== $real_legacy_dir
    ) {
        return false;
    }

    if (!file_exists($full_path) || !is_file($full_path)) {
        return false;
    }

    return $full_path;
}

/**
 * Stream a legacy employee document file safely.
 *
 * Sends proper security headers and prevents inline rendering of active content.
 * Does NOT expose the raw filesystem path to the browser.
 * Exits after streaming.
 *
 * @param string $canonical_path  Validated canonical path from canonical_legacy_employee_document_path().
 * @param string $display_name    Document display name (used only for download filename).
 */
function stream_legacy_file_safely($canonical_path, $display_name = '') {
    if ($canonical_path === false || !file_exists($canonical_path)) {
        header('HTTP/1.0 404 Not Found');
        exit();
    }

    $filesize = filesize($canonical_path);
    if ($filesize === false) {
        header('HTTP/1.0 500 Internal Server Error');
        exit();
    }

    // Detect MIME type server-side.
    $mime = detect_attachment_mime($canonical_path);
    if (empty($mime)) {
        $mime = 'application/octet-stream';
    }

    // Prevent inline execution of active content.
    $dangerous_mimes = array(
        'text/html', 'application/xhtml+xml', 'text/javascript',
        'application/javascript', 'application/x-httpd-php', 'application/x-shockwave-flash',
    );
    $disposition = 'inline';
    if (in_array($mime, $dangerous_mimes)) {
        $mime = 'application/octet-stream';
        $disposition = 'attachment';
    }

    // Clean output buffers.
    while (ob_get_level()) {
        ob_end_clean();
    }

    // Security headers.
    header('X-Content-Type-Options: nosniff');
    header('Cache-Control: private, no-store, max-age=0');
    header('Pragma: no-cache');

    header('Content-Type: ' . $mime);
    header('Content-Length: ' . $filesize);

    // Build safe Content-Disposition.
    if ($disposition === 'attachment') {
        $safe_name = $display_name;
        if (empty($safe_name)) {
            $safe_name = basename($canonical_path);
        }
        $safe_name = str_replace(array('"', '\\', "\n", "\r"), '_', $safe_name);
        if ($safe_name === '') {
            $safe_name = 'file';
        }
        header('Content-Disposition: attachment; filename="' . $safe_name . '"');
    } else {
        header('Content-Disposition: inline');
    }

    readfile($canonical_path);
    exit();
}
