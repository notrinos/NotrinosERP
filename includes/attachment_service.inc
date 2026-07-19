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
 * Detect a MIME type without trusting browser-supplied metadata.
 *
 * @param string $path
 * @return string|false
 */
function detect_attachment_mime_strict($path) {
    if (!is_string($path) || !is_file($path)) {
        return false;
    }
    if (function_exists('finfo_open')) {
        $finfo = finfo_open(FILEINFO_MIME_TYPE);
        if ($finfo) {
            $mime = finfo_file($finfo, $path);
            finfo_close($finfo);
            if (is_string($mime) && $mime !== '') {
                return strtolower(trim($mime));
            }
        }
    }
    if (function_exists('mime_content_type')) {
        $mime = @mime_content_type($path);
        if (is_string($mime) && $mime !== '') {
            return strtolower(trim($mime));
        }
    }
    return false;
}

/**
 * Return the configured or locally available employee-document scanner command.
 *
 * The optional HRM_DOCUMENT_SCANNER_COMMAND value must contain a {file}
 * placeholder or the escaped file path is appended. Ordinary users cannot
 * override this server-side setting.
 *
 * @param string $path
 * @return string|false
 */
function hrm_document_scanner_command($path) {
    if (defined('HRM_DOCUMENT_SCANNER_COMMAND')) {
        $template = trim((string)HRM_DOCUMENT_SCANNER_COMMAND);
        if ($template === '') {
            return false;
        }
        $escaped = escapeshellarg($path);
        return strpos($template, '{file}') !== false
            ? str_replace('{file}', $escaped, $template)
            : $template.' '.$escaped;
    }

    if (DIRECTORY_SEPARATOR === '\\') {
        $candidates = array(
            'C:\\Program Files\\Windows Defender\\MpCmdRun.exe',
        );
        $platform = 'C:\\ProgramData\\Microsoft\\Windows Defender\\Platform';
        if (is_dir($platform)) {
            $versions = glob($platform.'\\*\\MpCmdRun.exe');
            if (is_array($versions)) {
                rsort($versions, SORT_STRING);
                $candidates = array_merge($versions, $candidates);
            }
        }
        foreach ($candidates as $candidate) {
            if (is_file($candidate)) {
                return escapeshellarg($candidate).' -Scan -ScanType 3 -File '
                    .escapeshellarg($path).' -DisableRemediation';
            }
        }
    } else {
        foreach (array('/usr/bin/clamscan', '/usr/local/bin/clamscan') as $candidate) {
            if (is_executable($candidate)) {
                return escapeshellarg($candidate).' --no-summary --infected -- '
                    .escapeshellarg($path);
            }
        }
    }
    return false;
}

/**
 * Scan an employee-document file and fail closed on rejection, error or timeout.
 *
 * @param string $path
 * @param int $timeout_seconds
 * @param string|null $command_override Test-only/server-owned command override.
 * @return array
 */
function scan_hrm_employee_document($path, $timeout_seconds = 30, $command_override = null) {
    $command = $command_override !== null
        ? str_replace('{file}', escapeshellarg($path), (string)$command_override)
        : hrm_document_scanner_command($path);
    if ($command === false || trim($command) === '' || !function_exists('proc_open')) {
        return array('ok' => false, 'status' => 'scanner_unavailable', 'error_code' => 'scanner_unavailable');
    }

    $descriptors = array(
        0 => array('pipe', 'r'),
        1 => array('pipe', 'w'),
        2 => array('pipe', 'w'),
    );
    $pipes = array();
    $process = @proc_open($command, $descriptors, $pipes);
    if (!is_resource($process)) {
        return array('ok' => false, 'status' => 'scanner_unavailable', 'error_code' => 'scanner_unavailable');
    }
    fclose($pipes[0]);
    stream_set_blocking($pipes[1], false);
    stream_set_blocking($pipes[2], false);

    $deadline = microtime(true) + max(1, (int)$timeout_seconds);
    $exit_code = -1;
    do {
        // Drain output without persisting filenames, paths, scanner text or content.
        stream_get_contents($pipes[1], 8192);
        stream_get_contents($pipes[2], 8192);
        $status = proc_get_status($process);
        if (!$status['running']) {
            $exit_code = (int)$status['exitcode'];
            break;
        }
        if (microtime(true) >= $deadline) {
            proc_terminate($process);
            fclose($pipes[1]);
            fclose($pipes[2]);
            proc_close($process);
            return array('ok' => false, 'status' => 'scanner_timeout', 'error_code' => 'scanner_timeout');
        }
        usleep(50000);
    } while (true);

    fclose($pipes[1]);
    fclose($pipes[2]);
    $closed_code = proc_close($process);
    if ($exit_code < 0) {
        $exit_code = (int)$closed_code;
    }
    if ($exit_code === 0) {
        return array('ok' => true, 'status' => 'clean', 'error_code' => '');
    }
    return array('ok' => false, 'status' => 'rejected', 'error_code' => 'scanner_rejected_or_failed');
}

/**
 * Validate one regular HR document using strict extension, MIME and magic rules.
 *
 * Office/archive formats are intentionally denied until a reviewed sandbox/CDR
 * adapter exists. PDF is accepted only without common active-content markers.
 *
 * @param string $path
 * @param string $filename
 * @param int $max_size
 * @param bool $scan
 * @param string|null $scanner_command
 * @return array
 */
function validate_hrm_employee_document_file($path, $filename, $max_size = 10485760, $scan = true, $scanner_command = null) {
    $failure = array(
        'ok' => false, 'error' => _('Employee document content was rejected.'),
        'error_code' => 'invalid_file', 'filename' => '', 'size' => 0,
        'mime' => '', 'tmp_name' => $path, 'sha256' => '', 'scanner_status' => 'not_scanned',
    );
    if (!is_string($path) || $path === '' || is_link($path) || !is_file($path) || !is_readable($path)) {
        $failure['error_code'] = 'not_regular_readable_file';
        return $failure;
    }
    $safe_name = sanitize_attachment_filename($filename);
    if ($safe_name === '' || basename(str_replace('\\', '/', $filename)) !== $safe_name) {
        $failure['error_code'] = 'invalid_filename';
        return $failure;
    }
    $size = filesize($path);
    if ($size === false || $size <= 0 || $size > (int)$max_size) {
        $failure['error_code'] = $size === 0 ? 'empty_file' : 'size_limit';
        return $failure;
    }
	$failure['filename'] = $safe_name;
	$failure['size'] = (int)$size;
    $mime = detect_attachment_mime_strict($path);
    if ($mime === false) {
        $failure['error_code'] = 'mime_unavailable';
        return $failure;
    }
	$failure['mime'] = $mime;
    $extension = strtolower(pathinfo($safe_name, PATHINFO_EXTENSION));
	// Some minimal valid images are reported as application/octet-stream by
	// platform magic databases. Derive only image MIME from getimagesize after
	// an exact image-type check; no client value participates.
	if ($mime === 'application/octet-stream' && in_array($extension, array('jpg', 'jpeg', 'png'), true)) {
		$image_info = @getimagesize($path);
		if (is_array($image_info) && isset($image_info[2])) {
			if ((int)$image_info[2] === IMAGETYPE_JPEG) $mime = 'image/jpeg';
			elseif ((int)$image_info[2] === IMAGETYPE_PNG) $mime = 'image/png';
		}
	}
    $allowed = array(
        'txt' => array('text/plain'),
        'csv' => array('text/plain', 'text/csv', 'application/csv'),
        'jpg' => array('image/jpeg'),
        'jpeg' => array('image/jpeg'),
        'png' => array('image/png'),
        'pdf' => array('application/pdf'),
    );
    if (!isset($allowed[$extension]) || !in_array($mime, $allowed[$extension], true)) {
        $failure['error_code'] = 'extension_mime_mismatch';
        return $failure;
    }
    $payload = file_get_contents($path);
    if ($payload === false || strlen($payload) !== (int)$size) {
        $failure['error_code'] = 'read_or_size_mismatch';
        return $failure;
    }
    $magic_ok = true;
    if ($extension === 'jpg' || $extension === 'jpeg') {
        $image = @getimagesize($path);
        $magic_ok = substr($payload, 0, 2) === "\xFF\xD8"
            && substr($payload, -2) === "\xFF\xD9"
            && is_array($image) && isset($image[2]) && (int)$image[2] === IMAGETYPE_JPEG;
    } elseif ($extension === 'png') {
        $image = @getimagesize($path);
        $magic_ok = substr($payload, 0, 8) === "\x89PNG\x0D\x0A\x1A\x0A"
            && substr($payload, -12, 8) === "\x00\x00\x00\x00IEND"
            && is_array($image) && isset($image[2]) && (int)$image[2] === IMAGETYPE_PNG;
    } elseif ($extension === 'pdf') {
        $tail = rtrim(substr($payload, -2048));
        $magic_ok = substr($payload, 0, 5) === '%PDF-'
            && substr($tail, -5) === '%%EOF'
            && !preg_match('/\/(?:JavaScript|JS|Launch|EmbeddedFile|OpenAction|AA)\b/i', $payload);
    } else {
        $magic_ok = strpos($payload, "\0") === false
            && stripos($payload, '<?php') === false
            && stripos($payload, '<script') === false;
    }
    if (!$magic_ok) {
        $failure['error_code'] = 'magic_or_active_content_rejected';
        return $failure;
    }

    $scan_result = $scan
        ? scan_hrm_employee_document($path, 30, $scanner_command)
        : array('ok' => true, 'status' => 'not_required', 'error_code' => '');
    if (!$scan_result['ok']) {
        $failure['error_code'] = $scan_result['error_code'];
        $failure['scanner_status'] = $scan_result['status'];
        return $failure;
    }

    return array(
        'ok' => true, 'error' => '', 'error_code' => '', 'filename' => $safe_name,
        'size' => (int)$size, 'mime' => $mime, 'tmp_name' => $path,
        'sha256' => hash('sha256', $payload), 'scanner_status' => $scan_result['status'],
    );
}

/**
 * Validate a browser upload using the strict employee-document policy.
 *
 * @param array $upload
 * @param string|null $scanner_command
 * @return array
 */
function validate_hrm_employee_document_upload($upload, $scanner_command = null) {
    if (!isset($upload['error']) || is_array($upload['error'])
        || (int)$upload['error'] !== UPLOAD_ERR_OK
        || empty($upload['tmp_name']) || !is_uploaded_file($upload['tmp_name'])
    ) {
        return array(
            'ok' => false, 'error' => _('Employee document upload failed.'),
            'error_code' => 'invalid_upload', 'filename' => '', 'size' => 0,
            'mime' => '', 'tmp_name' => '', 'sha256' => '', 'scanner_status' => 'not_scanned',
        );
    }
    return validate_hrm_employee_document_file(
        $upload['tmp_name'],
        isset($upload['name']) ? $upload['name'] : '',
        10 * 1024 * 1024,
        true,
        $scanner_command
    );
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

    // Deny direct web access independently of the index redirect. All content
    // must pass through an authorized streaming controller.
    $deny_file = $dir . '/.htaccess';
    if (!file_exists($deny_file)) {
        @file_put_contents($deny_file, "Require all denied\nDeny from all\n");
    }
    $web_config = $dir . '/web.config';
    if (!file_exists($web_config)) {
        @file_put_contents($web_config, '<?xml version="1.0" encoding="UTF-8"?>'
            .'<configuration><system.webServer><security><authorization>'
            .'<remove users="*" roles="" verbs="" />'
            .'<add accessType="Deny" users="*" />'
            .'</authorization></security></system.webServer></configuration>');
    }

    // Keep the compatibility redirect as a secondary directory-level guard.
    $index_file = $dir . '/index.php';
    if (!file_exists($index_file)) {
        $content = "<?php\nheader(\"Location: ../index.php\");\n";
        @file_put_contents($index_file, $content);
    }

    return $dir;
}

/**
 * Return the private employee-document root outside the application web root.
 *
 * Production may define HRM_DOCUMENT_PRIVATE_ROOT to an approved absolute
 * path. The default is a sibling of the web-root parent, never company_path().
 *
 * @param int|null $company
 * @return string|false
 */
function hrm_document_private_root($company = null) {
    global $path_to_root;

    if ($company === null) {
        $company = function_exists('user_company') ? user_company() : 0;
    }
    $application_root = realpath($path_to_root);
    if ($application_root === false) {
        return false;
    }
    if (defined('HRM_DOCUMENT_PRIVATE_ROOT')) {
        $base = rtrim((string)HRM_DOCUMENT_PRIVATE_ROOT, "\\/");
    } else {
        $web_parent = dirname($application_root);
        $base = dirname($web_parent).DIRECTORY_SEPARATOR.'.notrinoserp-private'
            .DIRECTORY_SEPARATOR.substr(hash('sha256', $application_root), 0, 16);
    }
    if ($base === '' || !preg_match('/^(?:[A-Za-z]:[\\\\\/]|\/)/', $base)) {
        return false;
    }
    $normalized_base = str_replace('\\', '/', $base);
    $normalized_app = str_replace('\\', '/', $application_root);
    if ($normalized_base === $normalized_app
        || strpos($normalized_base.'/', rtrim($normalized_app, '/').'/') === 0
    ) {
        return false;
    }
    return $base.DIRECTORY_SEPARATOR.'company-'.(int)$company;
}

/**
 * Create and harden private storage directories.
 *
 * @param int|null $company
 * @return array|false
 */
function ensure_hrm_document_private_storage($company = null) {
    $root = hrm_document_private_root($company);
    if ($root === false) {
        return false;
    }
    $paths = array(
        'root' => $root,
        'objects' => $root.DIRECTORY_SEPARATOR.'objects',
        'quarantine' => $root.DIRECTORY_SEPARATOR.'quarantine',
        'keys' => $root.DIRECTORY_SEPARATOR.'keys',
    );
    foreach ($paths as $path) {
        if (!is_dir($path) && !@mkdir($path, 0700, true) && !is_dir($path)) {
            return false;
        }
        @chmod($path, 0700);
    }
    foreach (array($paths['root'], $paths['objects'], $paths['quarantine'], $paths['keys']) as $path) {
        if (!file_exists($path.DIRECTORY_SEPARATOR.'.htaccess')) {
            @file_put_contents($path.DIRECTORY_SEPARATOR.'.htaccess', "Require all denied\nDeny from all\n");
        }
        if (!file_exists($path.DIRECTORY_SEPARATOR.'web.config')) {
            @file_put_contents($path.DIRECTORY_SEPARATOR.'web.config', '<?xml version="1.0" encoding="UTF-8"?>'
                .'<configuration><system.webServer><security><authorization>'
                .'<remove users="*" roles="" verbs="" />'
                .'<add accessType="Deny" users="*" />'
                .'</authorization></security></system.webServer></configuration>');
        }
    }
    return $paths;
}

/**
 * Load or securely create the per-company employee-document encryption key.
 *
 * @param int|null $company
 * @return string|false Raw 32-byte key.
 */
function hrm_document_encryption_key($company = null) {
    if (defined('HRM_DOCUMENT_ENCRYPTION_KEY')) {
        $decoded = base64_decode((string)HRM_DOCUMENT_ENCRYPTION_KEY, true);
        return is_string($decoded) && strlen($decoded) === 32 ? $decoded : false;
    }
    $paths = ensure_hrm_document_private_storage($company);
    if ($paths === false) {
        return false;
    }
    $key_path = $paths['keys'].DIRECTORY_SEPARATOR.'content-v1.key.pem';
    if (is_file($key_path)) {
        $encoded = trim((string)@file_get_contents($key_path));
        $decoded = base64_decode($encoded, true);
        return is_string($decoded) && strlen($decoded) === 32 ? $decoded : false;
    }
    if (!function_exists('openssl_random_pseudo_bytes')) {
        return false;
    }
    $strong = false;
    $key = openssl_random_pseudo_bytes(32, $strong);
    if ($key === false || strlen($key) !== 32 || !$strong) {
        return false;
    }
    $handle = @fopen($key_path, 'x+b');
    if ($handle === false) {
        // A concurrent creator may have completed first.
        if (is_file($key_path)) {
            $decoded = base64_decode(trim((string)@file_get_contents($key_path)), true);
            return is_string($decoded) && strlen($decoded) === 32 ? $decoded : false;
        }
        return false;
    }
    $encoded_key = base64_encode($key)."\n";
    $written = fwrite($handle, $encoded_key);
    $flushed = fflush($handle);
    fclose($handle);
    @chmod($key_path, 0600);
    if ($written !== strlen($encoded_key) || !$flushed) {
        @unlink($key_path);
        return false;
    }
    return $key;
}

/**
 * Compare authentication values in constant time on PHP 5.6+.
 *
 * @param string $known
 * @param string $actual
 * @return bool
 */
function attachment_hash_equals($known, $actual) {
    if (function_exists('hash_equals')) {
        return hash_equals($known, $actual);
    }
    if (!is_string($known) || !is_string($actual) || strlen($known) !== strlen($actual)) {
        return false;
    }
    $difference = 0;
    for ($i = 0, $length = strlen($known); $i < $length; $i++) {
        $difference |= ord($known[$i]) ^ ord($actual[$i]);
    }
    return $difference === 0;
}

/**
 * Encrypt bytes using the versioned employee-document storage envelope.
 *
 * @param string $plaintext
 * @param string $master_key
 * @return string|false
 */
function encrypt_hrm_document_bytes($plaintext, $master_key) {
    if (!function_exists('openssl_encrypt') || strlen($master_key) !== 32) {
        return false;
    }
    $strong = false;
    $iv = openssl_random_pseudo_bytes(16, $strong);
    if ($iv === false || strlen($iv) !== 16 || !$strong) {
        return false;
    }
    $encryption_key = hash_hmac('sha256', 'encryption-v1', $master_key, true);
    $authentication_key = hash_hmac('sha256', 'authentication-v1', $master_key, true);
    $ciphertext = openssl_encrypt($plaintext, 'AES-256-CBC', $encryption_key, OPENSSL_RAW_DATA, $iv);
    if ($ciphertext === false) {
        return false;
    }
    $magic = 'NERPHRD1';
    $mac = hash_hmac('sha256', $magic.$iv.$ciphertext, $authentication_key, true);
    return $magic.$iv.$mac.$ciphertext;
}

/**
 * Authenticate and decrypt a versioned employee-document storage envelope.
 *
 * @param string $payload
 * @param string $master_key
 * @return string|false
 */
function decrypt_hrm_document_bytes($payload, $master_key) {
    if (!function_exists('openssl_decrypt') || strlen($master_key) !== 32
        || !is_string($payload) || strlen($payload) < 72 || substr($payload, 0, 8) !== 'NERPHRD1'
    ) {
        return false;
    }
    $magic = substr($payload, 0, 8);
    $iv = substr($payload, 8, 16);
    $mac = substr($payload, 24, 32);
    $ciphertext = substr($payload, 56);
    $authentication_key = hash_hmac('sha256', 'authentication-v1', $master_key, true);
    $expected = hash_hmac('sha256', $magic.$iv.$ciphertext, $authentication_key, true);
    if (!attachment_hash_equals($expected, $mac)) {
        return false;
    }
    $encryption_key = hash_hmac('sha256', 'encryption-v1', $master_key, true);
    return openssl_decrypt($ciphertext, 'AES-256-CBC', $encryption_key, OPENSSL_RAW_DATA, $iv);
}

/**
 * Store a validated HR document in the private encrypted object directory.
 *
 * @param string $source_path
 * @param int|null $company
 * @return array|false
 */
function store_private_hrm_document($source_path, $company = null) {
    $paths = ensure_hrm_document_private_storage($company);
    $key = hrm_document_encryption_key($company);
    if ($paths === false || $key === false || !is_file($source_path)) {
        return false;
    }
    $plaintext = file_get_contents($source_path);
    if ($plaintext === false) {
        return false;
    }
    $payload = encrypt_hrm_document_bytes($plaintext, $key);
    if ($payload === false) {
        return false;
    }
    do {
        $object_key = generate_attachment_unique_name();
        $final_path = $paths['objects'].DIRECTORY_SEPARATOR.$object_key;
    } while (file_exists($final_path));
    if (!preg_match('/^[A-Za-z0-9_-]{16,128}$/', $object_key)) {
        return false;
    }
    $temporary = $paths['quarantine'].DIRECTORY_SEPARATOR.$object_key.'.staged';
    if (file_put_contents($temporary, $payload, LOCK_EX) !== strlen($payload)) {
        @unlink($temporary);
        return false;
    }
    @chmod($temporary, 0600);
    $verify_payload = file_get_contents($temporary);
    $verify_plaintext = $verify_payload === false ? false : decrypt_hrm_document_bytes($verify_payload, $key);
    if ($verify_plaintext === false || !attachment_hash_equals(hash('sha256', $plaintext), hash('sha256', $verify_plaintext))) {
        @unlink($temporary);
        return false;
    }
    if (!@rename($temporary, $final_path)) {
        @unlink($temporary);
        return false;
    }
    @chmod($final_path, 0600);
    return array(
        'object_key' => $object_key,
        'path' => $final_path,
        'plaintext_size' => strlen($plaintext),
        'plaintext_sha256' => hash('sha256', $plaintext),
        'stored_size' => strlen($payload),
    );
}

/**
 * Resolve a private HR object path from an opaque storage key.
 *
 * @param string $object_key
 * @param int|null $company
 * @return string|false
 */
function get_private_hrm_document_path($object_key, $company = null) {
    if (!is_string($object_key) || !preg_match('/^[A-Za-z0-9_-]{16,128}$/', $object_key)) {
        return false;
    }
    $paths = ensure_hrm_document_private_storage($company);
    if ($paths === false) {
        return false;
    }
    $root = realpath($paths['objects']);
    $path = realpath($paths['objects'].DIRECTORY_SEPARATOR.$object_key);
    if ($root === false || $path === false || strpos($path, $root.DIRECTORY_SEPARATOR) !== 0 || !is_file($path)) {
        return false;
    }
    return $path;
}

/**
 * Decode attachment custom data defensively.
 *
 * @param mixed $value
 * @return array
 */
function attachment_custom_data_array($value) {
    if (is_array($value)) {
        return $value;
    }
    if (!is_string($value) || $value === '') {
        return array();
    }
    $decoded = json_decode($value, true);
    if (!is_array($decoded) && strpos($value, '&quot;') !== false) {
        // Recover metadata written through the legacy HTML-oriented db_escape().
        $decoded = json_decode(html_entity_decode($value, ENT_QUOTES, 'UTF-8'), true);
    }
    return is_array($decoded) ? $decoded : array();
}

/**
 * Quote JSON for SQL without applying UI HTML encoding.
 *
 * @param string $json
 * @return string
 */
function attachment_db_json($json) {
    $escaped = function_exists('db_escape_function')
        ? db_escape_function($json) : addslashes($json);
    return "'".$escaped."'";
}

/**
 * Load and verify plaintext from a private encrypted attachment row.
 *
 * @param array $attachment
 * @return string|false
 */
function read_private_hrm_attachment($attachment) {
    $custom = attachment_custom_data_array(isset($attachment['custom_data']) ? $attachment['custom_data'] : '');
    if (!isset($custom['storage_backend']) || $custom['storage_backend'] !== 'hrm_private_v1'
        || !isset($custom['content_state']) || $custom['content_state'] !== 'available'
        || !isset($custom['encryption_format']) || $custom['encryption_format'] !== 'aes-256-cbc-hmac-sha256-v1'
    ) {
        return false;
    }
    $path = get_private_hrm_document_path($attachment['unique_name']);
    $key = hrm_document_encryption_key();
    if ($path === false || $key === false) {
        return false;
    }
    $payload = file_get_contents($path);
    $plaintext = $payload === false ? false : decrypt_hrm_document_bytes($payload, $key);
    if ($plaintext === false
        || !isset($custom['plaintext_sha256'])
        || !attachment_hash_equals($custom['plaintext_sha256'], hash('sha256', $plaintext))
        || !isset($custom['plaintext_size'])
        || (int)$custom['plaintext_size'] !== strlen($plaintext)
    ) {
        return false;
    }
    return $plaintext;
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
        . attachment_db_json($custom_json) . ")";

    db_query($sql, 'Attachment could not be inserted');

    return db_insert_id();
}

/**
 * Store a validated employee document in encrypted private storage and add its
 * core attachment metadata. The caller remains responsible for authorization
 * and for linking the returned attachment in the same database transaction.
 *
 * @param int $trans_no Numeric employee_number.
 * @param string $description
 * @param array $validated Result from validate_hrm_employee_document_*().
 * @param array $custom_data
 * @return array|false Attachment id, object path and object key.
 */
function create_private_hrm_attachment($trans_no, $description, $validated, $custom_data = array()) {
    if (empty($validated['ok']) || empty($validated['sha256'])
        || empty($validated['scanner_status']) || $validated['scanner_status'] !== 'clean'
    ) {
        return false;
    }
    $stored = store_private_hrm_document($validated['tmp_name']);
    if ($stored === false
        || !attachment_hash_equals($stored['plaintext_sha256'], $validated['sha256'])
        || (int)$stored['plaintext_size'] !== (int)$validated['size']
    ) {
        if ($stored !== false && isset($stored['path'])) {
            @unlink($stored['path']);
        }
        return false;
    }
    $metadata = array_merge($custom_data, array(
        'storage_backend' => 'hrm_private_v1',
        'encryption_format' => 'aes-256-cbc-hmac-sha256-v1',
        'content_state' => 'available',
        'plaintext_sha256' => $validated['sha256'],
        'plaintext_size' => (int)$validated['size'],
        'validated_mime' => $validated['mime'],
        'scanner_status' => 'clean',
        'content_policy_version' => 'hrm-document-v1',
    ));
    $custom_json = json_encode($metadata, JSON_UNESCAPED_UNICODE);
    if ($custom_json === false) {
        @unlink($stored['path']);
        return false;
    }
    $sql = "INSERT INTO ".TB_PREF."attachments
        (type_no, trans_no, description, filename, unique_name, filesize, filetype, tran_date, custom_data)
        VALUES (".db_escape(ST_EMPLOYEE).", ".db_escape((int)$trans_no).", "
        .db_escape($description).", ".db_escape($validated['filename']).", "
        .db_escape($stored['object_key']).", ".db_escape((int)$validated['size']).", "
        .db_escape($validated['mime']).", '".date2sql(Today())."', ".attachment_db_json($custom_json).")";
    if (!db_query($sql)) {
        @unlink($stored['path']);
        return false;
    }
    $stored['attachment_id'] = (int)db_insert_id();
    return $stored;
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
        $sets[] = 'custom_data = ' . attachment_db_json($custom_json);
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
    $custom = attachment_custom_data_array(isset($existing['custom_data']) ? $existing['custom_data'] : '');
    $safe_path = isset($custom['storage_backend']) && $custom['storage_backend'] === 'hrm_private_v1'
        ? get_private_hrm_document_path($existing['unique_name'])
        : get_safe_attachment_path($existing['unique_name']);
    if ($safe_path !== false && file_exists($safe_path)) {
        if (!@unlink($safe_path)) {
            return false;
        }
    } elseif (isset($custom['storage_backend']) && $custom['storage_backend'] === 'hrm_private_v1') {
        return false;
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
    $custom = attachment_custom_data_array(isset($attachment['custom_data']) ? $attachment['custom_data'] : '');
    $private = isset($custom['storage_backend']) && $custom['storage_backend'] === 'hrm_private_v1';
    $plaintext = false;
    $safe_path = false;
    if ($private) {
        $plaintext = read_private_hrm_attachment($attachment);
        if ($plaintext === false) {
            header('HTTP/1.0 404 Not Found');
            exit();
        }
    } else {
        $safe_path = get_safe_attachment_path($attachment['unique_name']);
        if ($safe_path === false || !file_exists($safe_path)) {
            display_error(_('Attachment file not found or path is invalid.'));
            exit();
        }
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

    if ($private) {
        header('X-Content-Type-Options: nosniff');
        header('Cache-Control: private, no-store, max-age=0');
        header('Pragma: no-cache');
        header("Content-Security-Policy: sandbox; default-src 'none'");
    }
    header('Content-Type: ' . $type);
    header('Content-Length: ' . ($private ? strlen($plaintext) : $attachment['filesize']));

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

    if ($private) {
        echo $plaintext;
    } else {
        readfile($safe_path);
    }
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
 * Resolve the one verified historical relative-path format for migration only.
 *
 * Legacy rows stored paths relative to hrm/manage as
 * ../../company/{company}/documents/employees/{basename}. This function never
 * follows that traversal. It extracts a plain basename and reconstructs the
 * candidate below the trusted current-company directory.
 *
 * @param string $file_path
 * @param int|null $company
 * @return string|false
 */
function canonical_historical_employee_document_path($file_path, $company = null) {
    if (!is_string($file_path) || $file_path === '' || strpos($file_path, "\0") !== false) {
        return false;
    }
    if ($company === null) {
        $company = function_exists('user_company') ? user_company() : 0;
    }
    $normalized = str_replace('\\', '/', $file_path);
    $pattern = '#^\.\./\.\./company/'.(int)$company.'/documents/employees/([^/]+)$#D';
    if (!preg_match($pattern, $normalized, $matches)) {
        return false;
    }
    $basename = $matches[1];
    if ($basename === '' || $basename === '.' || $basename === '..'
        || strpos($basename, '..') !== false || sanitize_attachment_filename($basename) !== $basename
    ) {
        return false;
    }
    $root = realpath(company_path($company).'/documents/employees');
    $candidate = realpath(company_path($company).'/documents/employees/'.$basename);
    if ($root === false || $candidate === false
        || strpos($candidate, $root.DIRECTORY_SEPARATOR) !== 0
        || is_link(company_path($company).'/documents/employees/'.$basename)
        || !is_file($candidate) || !is_readable($candidate)
    ) {
        return false;
    }
    return $candidate;
}

/**
 * Deny direct HTTP access to current and legacy employee-document directories.
 *
 * @param int|null $company
 * @return bool
 */
function harden_hrm_document_public_directories($company = null) {
    if ($company === null) {
        $company = function_exists('user_company') ? user_company() : 0;
    }
    $directories = array(
        ensure_attachments_directory(),
        company_path($company).'/documents/employees',
    );
    foreach ($directories as $directory) {
        if (!is_dir($directory)) {
            continue;
        }
        $apache = $directory.'/.htaccess';
        $iis = $directory.'/web.config';
        if ((!file_exists($apache) && @file_put_contents($apache, "Require all denied\nDeny from all\n") === false)
            || (!file_exists($iis) && @file_put_contents($iis, '<?xml version="1.0" encoding="UTF-8"?>'
                .'<configuration><system.webServer><security><authorization>'
                .'<remove users="*" roles="" verbs="" /><add accessType="Deny" users="*" />'
                .'</authorization></security></system.webServer></configuration>') === false)
        ) {
            return false;
        }
    }
    return true;
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
