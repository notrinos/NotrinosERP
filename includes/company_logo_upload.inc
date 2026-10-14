<?php
/**
 * Validate and re-encode a genuine HTTP logo upload before writing public bytes.
 * Client filenames and MIME types never determine the stored name or format.
 * Returns the new basename, or false with a translated error message.
 */
function save_company_logo_upload($upload, $directory, $maximum_kilobytes, &$error)
{
	$error = _('Error uploading logo file.');
	if (!is_array($upload) || !isset($upload['error'], $upload['tmp_name'])
		|| $upload['error'] !== UPLOAD_ERR_OK || !is_string($upload['tmp_name'])
		|| !is_uploaded_file($upload['tmp_name']))
		return false;

	$maximum_bytes = (int)$maximum_kilobytes * 1024;
	$size = @filesize($upload['tmp_name']);
	if ($maximum_bytes <= 0 || $size === false || $size <= 0 || $size > $maximum_bytes) {
		$error = _('The file size is over the maximum allowed.');
		return false;
	}

	$image_info = @getimagesize($upload['tmp_name']);
	if (!$image_info || !in_array($image_info[2], array(IMAGETYPE_JPEG, IMAGETYPE_PNG), true)) {
		$error = _('Only valid jpg and png images can be uploaded.');
		return false;
	}
	// Bound decoding memory even for small, highly compressed uploads.
	if ($image_info[0] <= 0 || $image_info[1] <= 0
		|| $image_info[0] > 4096 || $image_info[1] > 4096
		|| $image_info[0] * $image_info[1] > 4000000) {
		$error = _('The logo dimensions are too large.');
		return false;
	}
	if (!function_exists('imagecreatefromstring') || !function_exists('imagejpeg')
		|| !function_exists('imagepng')) {
		$error = _('Image processing is unavailable. The logo was not stored.');
		return false;
	}
	$image = @imagecreatefromstring(file_get_contents($upload['tmp_name']));
	if (!$image) {
		$error = _('Only valid jpg and png images can be uploaded.');
		return false;
	}

	// Fresh encoding discards appended scripts and untrusted image metadata.
	ob_start();
	if ($image_info[2] === IMAGETYPE_PNG) {
		imagealphablending($image, false);
		imagesavealpha($image, true);
		$encoded = @imagepng($image, null, 9);
		$extension = 'png';
	} else {
		$encoded = @imagejpeg($image, null, 90);
		$extension = 'jpg';
	}
	$data = ob_get_clean();
	imagedestroy($image);
	if (!$encoded || $data === '' || strlen($data) > $maximum_bytes)
		return false;

	if (!is_dir($directory) && !@mkdir($directory, 0755, true))
		return false;
	$identifier = random_id();
	if (!is_string($identifier) || !preg_match('/\A[a-zA-Z0-9_-]{16,64}\z/', $identifier))
		return false;
	$basename = 'logo_'.$identifier.'.'.$extension;
	$filename = $directory.'/'.$basename;
	// Exclusive creation prevents replacing any existing company asset.
	$handle = @fopen($filename, 'xb');
	if (!$handle)
		return false;
	$offset = 0;
	$length = strlen($data);
	while ($offset < $length) {
		$written = @fwrite($handle, substr($data, $offset));
		if ($written === false || $written === 0)
			break;
		$offset += $written;
	}
	$closed = fclose($handle);
	if ($offset !== $length || !$closed) {
		@unlink($filename); // Only this request's newly created incomplete file.
		return false;
	}
	$error = '';
	return $basename;
}
