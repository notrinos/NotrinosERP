<?php
/**
 * Stable attachment security backport of NotrinosERP commit 1add590.
 * Callers retain authentication, page security and entity authorization.
 * No browser-supplied MIME or database MIME is trusted for serving content.
 */

/**
 * Return a filename safe for attachment metadata, or an empty string.
 * @param string $filename
 * @return string
 */
function sanitize_attachment_filename($filename) {
    if (!is_string($filename) || strpos($filename, "\0") !== false) return '';
    $filename = trim(basename(str_replace('\\', '/', $filename)));
    if ($filename === '' || strpos($filename, '..') !== false
        || preg_match('/[<>:"|?*\x00-\x1f\x7f]/', $filename)
        || strlen($filename) > 60) return '';
    return $filename;
}

/**
 * Validate a real multipart upload, ignoring its client MIME and size.
 * @param array $upload
 * @param int $max_size
 * @return array
 */
function validate_general_attachment_upload($upload, $max_size = 5242880) {
    $failure = array('ok' => false, 'error' => _('Invalid attachment upload.'),
        'error_code' => 'invalid_upload', 'filename' => '', 'size' => 0,
        'mime' => '', 'tmp_name' => '');
    if (!is_array($upload) || !isset($upload['error'], $upload['name'], $upload['tmp_name'])
        || !is_int($upload['error']) || !is_string($upload['name'])
        || !is_string($upload['tmp_name'])) return $failure;
    if ($upload['error'] !== UPLOAD_ERR_OK) {
        $failure['error'] = ($upload['error'] === UPLOAD_ERR_INI_SIZE
            || $upload['error'] === UPLOAD_ERR_FORM_SIZE)
            ? _('The file size is over the maximum allowed.') : _('Select attachment file.');
        return $failure;
    }
    if ($upload['tmp_name'] === '' || !is_uploaded_file($upload['tmp_name'])) return $failure;
    return validate_general_attachment_file($upload['tmp_name'], $upload['name'], $max_size);
}

/**
 * Return the reviewed content policy for ordinary entity attachments.
 *
 * These formats match the legacy attachment UI. Executable, markup and modern
 * OOXML Office formats remain denied; DOC and ODT documents are always
 * downloaded by the serving controller.
 *
 * @return array Map of lowercase extensions to accepted server MIME types.
 */
function general_attachment_content_policy() {
    return array(
        'jpg' => array('image/jpeg'),
        'jpeg' => array('image/jpeg'),
        'png' => array('image/png'),
        'gif' => array('image/gif'),
        'pdf' => array('application/pdf'),
        'doc' => array('application/msword', 'application/x-ole-storage',
            'application/vnd.ms-office', 'application/cdfv2', 'application/octet-stream'),
        'odt' => array('application/vnd.oasis.opendocument.text', 'application/zip',
            'application/x-zip', 'application/x-zip-compressed', 'application/octet-stream'),
    );
}

/**
 * Validate ordinary attachment content using extension, strict server MIME and
 * format-specific signatures. No browser-supplied MIME value participates.
 *
 * @param string $path
 * @param string $filename
 * @param int $max_size
 * @return array Validation result compatible with create_attachment().
 */
function validate_general_attachment_file($path, $filename, $max_size = 5242880) {
    $failure = array(
        'ok' => false, 'error' => _('Attachment content was rejected.'),
        'error_code' => 'invalid_file', 'filename' => '', 'size' => 0,
        'mime' => '', 'tmp_name' => $path,
    );
    if (!is_string($path) || $path === '' || is_link($path)
        || !is_file($path) || !is_readable($path)
    ) {
        $failure['error_code'] = 'not_regular_readable_file';
        return $failure;
    }

    $safe_name = sanitize_attachment_filename($filename);
    if ($safe_name === '') {
        $failure['error_code'] = 'invalid_filename';
        return $failure;
    }
    $failure['filename'] = $safe_name;

    $size = filesize($path);
    if ($size === false || $size <= 0 || $size > (int)$max_size) {
        $failure['error_code'] = $size === 0 ? 'empty_file' : 'size_limit';
        return $failure;
    }
    $failure['size'] = (int)$size;

    $extension = strtolower(pathinfo($safe_name, PATHINFO_EXTENSION));
    $policy = general_attachment_content_policy();
    if (!isset($policy[$extension])) {
        $failure['error_code'] = 'extension_not_allowed';
        return $failure;
    }

    $mime = detect_attachment_mime_strict($path);
    if ($mime === false) {
        $failure['error_code'] = 'mime_unavailable';
        return $failure;
    }
    $failure['mime'] = $mime;

    // Platform magic databases occasionally report tiny valid images as
    // octet-stream. getimagesize() may refine only a matching image format.
    $image_info = false;
    if (in_array($extension, array('jpg', 'jpeg', 'png', 'gif'), true)) {
        $image_info = @getimagesize($path);
        if ($mime === 'application/octet-stream' && is_array($image_info)
            && isset($image_info['mime']) && is_string($image_info['mime'])
        ) {
            $mime = strtolower($image_info['mime']);
            $failure['mime'] = $mime;
        }
    }
    if (!in_array($mime, $policy[$extension], true)) {
        $failure['error_code'] = 'extension_mime_mismatch';
        return $failure;
    }

    $payload = file_get_contents($path);
    if ($payload === false || strlen($payload) !== (int)$size) {
        $failure['error_code'] = 'read_or_size_mismatch';
        return $failure;
    }

    $content_ok = false;
    if ($extension === 'jpg' || $extension === 'jpeg') {
        $content_ok = substr($payload, 0, 2) === "\xFF\xD8"
            && substr($payload, -2) === "\xFF\xD9"
            && is_array($image_info) && isset($image_info[2])
            && (int)$image_info[2] === IMAGETYPE_JPEG;
    } elseif ($extension === 'png') {
        $content_ok = substr($payload, 0, 8) === "\x89PNG\x0D\x0A\x1A\x0A"
            && substr($payload, -12, 8) === "\x00\x00\x00\x00IEND"
            && is_array($image_info) && isset($image_info[2])
            && (int)$image_info[2] === IMAGETYPE_PNG;
    } elseif ($extension === 'gif') {
        $content_ok = in_array(substr($payload, 0, 6), array('GIF87a', 'GIF89a'), true)
            && substr(rtrim($payload), -1) === ';'
            && is_array($image_info) && isset($image_info[2])
            && (int)$image_info[2] === IMAGETYPE_GIF;
    } elseif ($extension === 'pdf') {
        $tail = rtrim(substr($payload, -2048));
        $content_ok = substr($payload, 0, 5) === '%PDF-'
            && substr($tail, -5) === '%%EOF'
            && !preg_match('/\/(?:JavaScript|JS|Launch|EmbeddedFile|OpenAction|AA)\b/i', $payload);
    } elseif ($extension === 'doc') {
        $content_ok = substr($payload, 0, 8) === "\xD0\xCF\x11\xE0\xA1\xB1\x1A\xE1";
    } elseif ($extension === 'odt') {
        $content_ok = substr($payload, 0, 4) === "PK\x03\x04" && class_exists('ZipArchive');
        if ($content_ok) {
            $archive = new ZipArchive();
            $opened = $archive->open($path);
            $mimetype_stat = $opened === true ? $archive->statName('mimetype') : false;
            $odt_mime = is_array($mimetype_stat) && isset($mimetype_stat['size'])
                && (int)$mimetype_stat['size'] <= 128
                ? $archive->getFromName('mimetype') : false;
            if ($opened === true) {
                $archive->close();
            }
            $content_ok = $opened === true
                && trim((string)$odt_mime) === 'application/vnd.oasis.opendocument.text';
        }
    }
    if (!$content_ok) {
        $failure['error_code'] = 'magic_or_active_content_rejected';
        return $failure;
    }

    return array(
        'ok' => true, 'error' => '', 'error_code' => '', 'filename' => $safe_name,
        'size' => (int)$size, 'mime' => $mime, 'tmp_name' => $path,
    );
}

/**
 * Return a safe inline MIME for a stored ordinary attachment, or false when it
 * must be downloaded. This protects legacy rows whose stored MIME is untrusted.
 *
 * @param string $path
 * @param string $filename
 * @return string|false
 */
function general_attachment_inline_mime($path, $filename) {
    $size = is_file($path) ? filesize($path) : false;
    if ($size === false || $size <= 0) {
        return false;
    }
    $validated = validate_general_attachment_file($path, $filename, 5242880);
    if (empty($validated['ok'])) {
        return false;
    }
    $extension = strtolower(pathinfo($validated['filename'], PATHINFO_EXTENSION));
    if (!in_array($extension, array('jpg', 'jpeg', 'png', 'gif', 'pdf'), true)) {
        return false;
    }
    return $validated['mime'];
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
 * Resolve an existing attachment strictly inside the current company directory.
 * @param string $unique_name
 * @return string|false
 */
function get_safe_attachment_path($unique_name) {
    if (!is_string($unique_name) || $unique_name === ''
        || strpbrk($unique_name, "\\/\0:") !== false || strpos($unique_name, '..') !== false) return false;
    $directory = realpath(company_path().'/attachments');
    $path = realpath(company_path().'/attachments/'.$unique_name);
    if ($directory === false || $path === false
        || strpos($path, $directory.DIRECTORY_SEPARATOR) !== 0
        || is_link(company_path().'/attachments/'.$unique_name)
        || !is_file($path) || !is_readable($path)) return false;
    return $path;
}

/**
 * Move validated content to a server-selected attachment filename.
 * Replacement uses only the database filename and never unlinks before a move.
 * @param array $validated
 * @param string $unique_name Existing database filename, or empty for a new file.
 * @return string|false Stored unique name on success.
 */
function store_general_attachment_upload($validated, $unique_name = '') {
    if (empty($validated['ok']) || !is_uploaded_file($validated['tmp_name'])) return false;
    $directory = company_path().'/attachments';
    if (!is_dir($directory)) {
        if (!mkdir($directory, 0777, true)) return false;
        if (file_put_contents($directory.'/index.php', "<?php\nheader(\"Location: ../index.php\");\n") === false) return false;
    }
    if ($unique_name !== '') {
        $destination = get_safe_attachment_path($unique_name);
        if ($destination === false) return false;
    } else {
        $unique_name = random_id();
        $destination = $directory.'/'.$unique_name;
        if (file_exists($destination)) return false;
    }
    return move_uploaded_file($validated['tmp_name'], $destination) ? $unique_name : false;
}

/**
 * Serve validated passive content inline; download all other stored content.
 * Applies to legacy rows too. Stored MIME and size are never used in headers.
 * @param array $attachment
 * @param bool $view
 * @return void
 */
function stream_general_attachment_file($attachment, $view = false) {
    $path = is_array($attachment) && isset($attachment['unique_name'])
        ? get_safe_attachment_path($attachment['unique_name']) : false;
    if ($path === false) {
        http_response_code(404);
        exit();
    }
    $filename = isset($attachment['filename']) ? sanitize_attachment_filename($attachment['filename']) : '';
    if ($filename === '') $filename = 'attachment';
    $mime = $view ? general_attachment_inline_mime($path, $filename) : false;
    $inline = $mime !== false;
    if (!$inline) $mime = 'application/octet-stream';
    while (ob_get_level()) ob_end_clean();
    header('Content-Type: '.$mime);
    header('X-Content-Type-Options: nosniff');
    header("Content-Security-Policy: sandbox; default-src 'none'");
    header('Cache-Control: private, no-store, max-age=0');
    header('Pragma: no-cache');
    header('Content-Length: '.filesize($path));
    header('Content-Disposition: '.($inline ? 'inline' : 'attachment')
        .'; filename="'.$filename.'"; filename*=UTF-8\'\''.rawurlencode($filename));
    readfile($path);
    exit();
}
