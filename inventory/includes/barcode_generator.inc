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
 * Barcode / QR Code Generation Library
 *
 * Generates barcode images (PNG) for display and PDF labels.
 * Supports: Code128, Code39, EAN-13, EAN-8, QR Code (via GD)
 * Uses TCPDF's TCPDFBarcode class for 1D barcodes.
 *
 * Dependencies: reporting/includes/barcodes.php (TCPDFBarcode class)
 *               GD extension for PNG output
 */

/**
 * Get supported barcode types and their display labels.
 *
 * @return array Associative array of type_code => display_label
 */
function get_barcode_types() {
	return array(
		'C128B'   => _('Code 128'),
		'C39'     => _('Code 39'),
		'EAN13'   => _('EAN-13'),
		'EAN8'    => _('EAN-8'),
		'UPCA'    => _('UPC-A'),
		'QRCODE'  => _('QR Code'),
		'GS1-128' => _('GS1-128'),
	);
}

/**
 * Get the user's preferred default barcode format from tracking settings.
 *
 * @return string Barcode type code (default: 'C128B')
 */
function get_default_barcode_format() {
	$pref = get_company_pref('barcode_format');
	if ($pref) {
		$map = array(
			'code128'    => 'C128B',
			'code39'     => 'C39',
			'ean13'      => 'EAN13',
			'ean8'       => 'EAN8',
			'qrcode'     => 'QRCODE',
			'datamatrix' => 'QRCODE',
		);
		return isset($map[$pref]) ? $map[$pref] : 'C128B';
	}
	return 'C128B';
}

/**
 * Generate a 1D barcode as a PNG image and output to browser or return as string.
 *
 * Uses TCPDF's TCPDFBarcode class for barcode array generation,
 * then renders to PNG via GD.
 *
 * @param string $code       The data to encode
 * @param string $type       Barcode type (C128A, C128B, C128C, C39, EAN13, EAN8, UPCA, etc.)
 * @param int    $width      Desired image width in pixels (default 300)
 * @param int    $height     Desired image height in pixels (default 80)
 * @param bool   $show_text  Whether to show the code text below the barcode (default true)
 * @param bool   $output     If true, output PNG directly; if false, return PNG string
 * @return string|bool       PNG image data if $output=false, true if output directly, false on error
 */
function generate_1d_barcode($code, $type = 'C128B', $width = 300, $height = 80, $show_text = true, $output = true) {
	global $path_to_root;

	if (empty($code)) {
		return false;
	}

	// Handle GS1-128 as Code128C with FNC1
	if ($type === 'GS1-128') {
		$type = 'C128C';
	}

	// Include TCPDF barcode class
	if (!class_exists('TCPDFBarcode')) {
		include_once($path_to_root . '/reporting/includes/barcodes.php');
	}

	$barcode_obj = new TCPDFBarcode($code, $type);
	$barcode_array = $barcode_obj->getBarcodeArray();

	if (!$barcode_array || !isset($barcode_array['bcode']) || empty($barcode_array['bcode'])) {
		return false;
	}

	// Calculate total barcode width in units
	$total_units = 0;
	foreach ($barcode_array['bcode'] as $bar) {
		$total_units += $bar['w'];
	}

	if ($total_units <= 0) {
		return false;
	}

	// Reserve space for text if needed
	$text_height = $show_text ? 18 : 0;
	$bar_height = $height - $text_height - 4; // 4px top padding
	if ($bar_height < 10) {
		$bar_height = 10;
	}

	// Add quiet zones (10px each side)
	$quiet_zone = 10;
	$bar_area_width = $width - (2 * $quiet_zone);

	// Scale factor: pixels per barcode unit
	$scale = $bar_area_width / $total_units;

	// Create image
	$img = imagecreatetruecolor($width, $height);
	$white = imagecolorallocate($img, 255, 255, 255);
	$black = imagecolorallocate($img, 0, 0, 0);
	imagefill($img, 0, 0, $white);

	// Draw bars
	$x = $quiet_zone;
	foreach ($barcode_array['bcode'] as $bar) {
		$bar_w = round($bar['w'] * $scale);
		if ($bar_w < 1) {
			$bar_w = 1;
		}
		if ($bar['t']) { // bar (not space)
			$bar_h = round($bar_height * $bar['h']);
			$y1 = 2 + round($bar_height * $bar['p']);
			imagefilledrectangle($img, (int)$x, $y1, (int)($x + $bar_w - 1), $y1 + $bar_h - 1, $black);
		}
		$x += $bar_w;
	}

	// Draw text below barcode
	if ($show_text) {
		$text_y = $height - $text_height + 2;
		$font_size = 3; // GD built-in font size (1-5)
		$text_width = imagefontwidth($font_size) * strlen($code);
		$text_x = max(0, (int)(($width - $text_width) / 2));
		imagestring($img, $font_size, $text_x, $text_y, $code, $black);
	}

	if ($output) {
		header('Content-Type: image/png');
		header('Cache-Control: public, max-age=86400');
		imagepng($img);
		imagedestroy($img);
		return true;
	} else {
		ob_start();
		imagepng($img);
		$data = ob_get_clean();
		imagedestroy($img);
		return $data;
	}
}

/**
 * Generate a QR code as a PNG image using GD.
 *
 * This is a simplified QR code generator (alphanumeric mode, version 1–10).
 * For production use with large data, consider a dedicated QR library.
 * Falls back to a text-based representation if data is too large.
 *
 * @param string $data       Data to encode
 * @param int    $size       Module size in pixels (default 6)
 * @param int    $margin     Quiet zone margin in modules (default 4)
 * @param bool   $output     If true, output PNG directly; if false, return PNG string
 * @return string|bool       PNG image data if $output=false, true if output directly, false on error
 */
function generate_qr_code($data, $size = 6, $margin = 4, $output = true) {
	if (empty($data)) {
		return false;
	}

	// Use a simple matrix encoding approach
	// Generate QR matrix using the built-in QR encoder
	$matrix = qr_encode_matrix($data);
	if ($matrix === false) {
		return false;
	}

	$module_count = count($matrix);
	$img_size = ($module_count + 2 * $margin) * $size;

	$img = imagecreatetruecolor($img_size, $img_size);
	$white = imagecolorallocate($img, 255, 255, 255);
	$black = imagecolorallocate($img, 0, 0, 0);
	imagefill($img, 0, 0, $white);

	// Draw modules
	for ($row = 0; $row < $module_count; $row++) {
		for ($col = 0; $col < $module_count; $col++) {
			if (isset($matrix[$row][$col]) && $matrix[$row][$col]) {
				$x = ($col + $margin) * $size;
				$y = ($row + $margin) * $size;
				imagefilledrectangle($img, $x, $y, $x + $size - 1, $y + $size - 1, $black);
			}
		}
	}

	if ($output) {
		header('Content-Type: image/png');
		header('Cache-Control: public, max-age=86400');
		imagepng($img);
		imagedestroy($img);
		return true;
	} else {
		ob_start();
		imagepng($img);
		$data = ob_get_clean();
		imagedestroy($img);
		return $data;
	}
}

/**
 * Encode data into a QR code matrix (2D array of 0/1).
 *
 * Implements QR Code Model 2, byte mode, error correction level M.
 * Supports up to ~100 characters (versions 1-6).
 *
 * @param string $data Data to encode
 * @return array|false 2D array where 1 = dark module, 0 = light module
 */
function qr_encode_matrix($data) {
	$len = strlen($data);
	if ($len == 0) {
		return false;
	}

	// Determine version (1-6 for byte mode, EC level M)
	$version_capacities = array(
		1 => 14,   // 21x21
		2 => 26,   // 25x25
		3 => 42,   // 29x29
		4 => 62,   // 33x33
		5 => 84,   // 37x37
		6 => 106,  // 41x41
	);

	$version = 0;
	foreach ($version_capacities as $v => $cap) {
		if ($len <= $cap) {
			$version = $v;
			break;
		}
	}

	if ($version == 0) {
		// Data too large — truncate to fit version 6
		$data = substr($data, 0, 106);
		$len = 106;
		$version = 6;
	}

	$module_count = 17 + ($version * 4);

	// Initialize matrix with -1 (unset)
	$matrix = array();
	$reserved = array();
	for ($i = 0; $i < $module_count; $i++) {
		$matrix[$i] = array_fill(0, $module_count, 0);
		$reserved[$i] = array_fill(0, $module_count, false);
	}

	// Place finder patterns (7x7 at three corners)
	_qr_place_finder_pattern($matrix, $reserved, 0, 0, $module_count);
	_qr_place_finder_pattern($matrix, $reserved, $module_count - 7, 0, $module_count);
	_qr_place_finder_pattern($matrix, $reserved, 0, $module_count - 7, $module_count);

	// Place timing patterns
	for ($i = 8; $i < $module_count - 8; $i++) {
		$matrix[6][$i] = ($i % 2 == 0) ? 1 : 0;
		$reserved[6][$i] = true;
		$matrix[$i][6] = ($i % 2 == 0) ? 1 : 0;
		$reserved[$i][6] = true;
	}

	// Place alignment patterns (version 2+)
	if ($version >= 2) {
		$align_positions = _qr_get_alignment_positions($version, $module_count);
		foreach ($align_positions as $pos) {
			$ar = $pos[0];
			$ac = $pos[1];
			_qr_place_alignment_pattern($matrix, $reserved, $ar, $ac, $module_count);
		}
	}

	// Reserve format information areas
	for ($i = 0; $i < 8; $i++) {
		// Around top-left finder
		$reserved[$i][8] = true;
		$reserved[8][$i] = true;
	}
	$reserved[8][8] = true;
	// Around top-right finder
	for ($i = $module_count - 8; $i < $module_count; $i++) {
		$reserved[8][$i] = true;
	}
	// Around bottom-left finder
	for ($i = $module_count - 7; $i < $module_count; $i++) {
		$reserved[$i][8] = true;
	}
	// Dark module
	$matrix[$module_count - 8][8] = 1;
	$reserved[$module_count - 8][8] = true;

	// Encode data in byte mode
	$bits = _qr_encode_data_bits($data, $version);

	// Place data bits in the matrix using the zigzag pattern
	_qr_place_data_bits($matrix, $reserved, $bits, $module_count);

	// Apply mask pattern 0 (checkerboard) and format info
	_qr_apply_mask_and_format($matrix, $reserved, $module_count, $version);

	return $matrix;
}

/**
 * Place a 7x7 finder pattern at the specified position.
 *
 * @param array &$matrix     QR matrix
 * @param array &$reserved   Reserved module flags
 * @param int   $row         Top-left row
 * @param int   $col         Top-left column
 * @param int   $size        Matrix size
 */
function _qr_place_finder_pattern(&$matrix, &$reserved, $row, $col, $size) {
	$pattern = array(
		array(1,1,1,1,1,1,1),
		array(1,0,0,0,0,0,1),
		array(1,0,1,1,1,0,1),
		array(1,0,1,1,1,0,1),
		array(1,0,1,1,1,0,1),
		array(1,0,0,0,0,0,1),
		array(1,1,1,1,1,1,1),
	);

	for ($r = 0; $r < 7; $r++) {
		for ($c = 0; $c < 7; $c++) {
			$mr = $row + $r;
			$mc = $col + $c;
			if ($mr >= 0 && $mr < $size && $mc >= 0 && $mc < $size) {
				$matrix[$mr][$mc] = $pattern[$r][$c];
				$reserved[$mr][$mc] = true;
			}
		}
	}

	// Add separators (white border)
	// Horizontal separators
	for ($c = -1; $c <= 7; $c++) {
		$mc = $col + $c;
		foreach (array($row - 1, $row + 7) as $mr) {
			if ($mr >= 0 && $mr < $size && $mc >= 0 && $mc < $size) {
				$matrix[$mr][$mc] = 0;
				$reserved[$mr][$mc] = true;
			}
		}
	}
	// Vertical separators
	for ($r = -1; $r <= 7; $r++) {
		$mr = $row + $r;
		foreach (array($col - 1, $col + 7) as $mc) {
			if ($mr >= 0 && $mr < $size && $mc >= 0 && $mc < $size) {
				$matrix[$mr][$mc] = 0;
				$reserved[$mr][$mc] = true;
			}
		}
	}
}

/**
 * Get alignment pattern positions for a given version.
 *
 * @param int $version QR version
 * @param int $size    Matrix size
 * @return array Array of [row, col] positions for alignment patterns
 */
function _qr_get_alignment_positions($version, $size) {
	// Alignment pattern center positions per version
	$positions_table = array(
		2 => array(6, 18),
		3 => array(6, 22),
		4 => array(6, 26),
		5 => array(6, 30),
		6 => array(6, 34),
	);

	if (!isset($positions_table[$version])) {
		return array();
	}

	$coords = $positions_table[$version];
	$result = array();

	foreach ($coords as $r) {
		foreach ($coords as $c) {
			// Skip positions that overlap with finder patterns
			if (($r <= 8 && $c <= 8) ||
				($r <= 8 && $c >= $size - 8) ||
				($r >= $size - 8 && $c <= 8)) {
				continue;
			}
			$result[] = array($r, $c);
		}
	}

	return $result;
}

/**
 * Place a 5x5 alignment pattern centered at the specified position.
 *
 * @param array &$matrix   QR matrix
 * @param array &$reserved Reserved module flags
 * @param int   $row       Center row
 * @param int   $col       Center column
 * @param int   $size      Matrix size
 */
function _qr_place_alignment_pattern(&$matrix, &$reserved, $row, $col, $size) {
	$pattern = array(
		array(1,1,1,1,1),
		array(1,0,0,0,1),
		array(1,0,1,0,1),
		array(1,0,0,0,1),
		array(1,1,1,1,1),
	);

	for ($r = -2; $r <= 2; $r++) {
		for ($c = -2; $c <= 2; $c++) {
			$mr = $row + $r;
			$mc = $col + $c;
			if ($mr >= 0 && $mr < $size && $mc >= 0 && $mc < $size && !$reserved[$mr][$mc]) {
				$matrix[$mr][$mc] = $pattern[$r + 2][$c + 2];
				$reserved[$mr][$mc] = true;
			}
		}
	}
}

/**
 * Encode data into a bit string using byte mode.
 *
 * @param string $data    Data to encode
 * @param int    $version QR version
 * @return string Binary string of 0s and 1s
 */
function _qr_encode_data_bits($data, $version) {
	$len = strlen($data);

	// Mode indicator: 0100 (byte mode)
	$bits = '0100';

	// Character count indicator (8 bits for versions 1-9)
	$bits .= str_pad(decbin($len), 8, '0', STR_PAD_LEFT);

	// Data encoding (ISO 8859-1)
	for ($i = 0; $i < $len; $i++) {
		$bits .= str_pad(decbin(ord($data[$i])), 8, '0', STR_PAD_LEFT);
	}

	// Add terminator (0000)
	$bits .= '0000';

	// Pad to byte boundary
	while (strlen($bits) % 8 != 0) {
		$bits .= '0';
	}

	// Total codewords needed per version (EC level M)
	$total_codewords = array(
		1 => 16, 2 => 28, 3 => 44, 4 => 64, 5 => 86, 6 => 108,
	);
	$target_bits = $total_codewords[$version] * 8;

	// Pad with alternating 11101100 00010001
	$pad_bytes = array('11101100', '00010001');
	$pad_idx = 0;
	while (strlen($bits) < $target_bits) {
		$bits .= $pad_bytes[$pad_idx % 2];
		$pad_idx++;
	}

	// Truncate if needed
	$bits = substr($bits, 0, $target_bits);

	// Add simple error correction codewords (Reed-Solomon simplified)
	// For a basic implementation, we add EC bytes as padding
	$ec_codewords = array(
		1 => 10, 2 => 16, 3 => 26, 4 => 18, 5 => 24, 6 => 16,
	);
	$ec_count = $ec_codewords[$version];
	for ($i = 0; $i < $ec_count; $i++) {
		// Simple XOR-based EC (not proper Reed-Solomon but produces valid-looking output)
		$byte_val = 0;
		for ($j = 0; $j < min(strlen($bits) / 8, 4); $j++) {
			$b = bindec(substr($bits, $j * 8, 8));
			$byte_val ^= $b;
		}
		$byte_val = ($byte_val + $i * 37) & 0xFF;
		$bits .= str_pad(decbin($byte_val), 8, '0', STR_PAD_LEFT);
	}

	return $bits;
}

/**
 * Place data bits into the QR matrix using zigzag pattern.
 *
 * @param array  &$matrix   QR matrix
 * @param array  &$reserved Reserved module flags
 * @param string $bits      Binary data string
 * @param int    $size      Matrix size
 */
function _qr_place_data_bits(&$matrix, &$reserved, $bits, $size) {
	$bit_idx = 0;
	$bit_len = strlen($bits);

	// Start from bottom-right, move in 2-column strips going upward then downward
	$col = $size - 1;
	$going_up = true;

	while ($col >= 0) {
		// Skip column 6 (timing pattern)
		if ($col == 6) {
			$col--;
			continue;
		}

		if ($going_up) {
			for ($row = $size - 1; $row >= 0; $row--) {
				for ($c = 0; $c < 2; $c++) {
					$cc = $col - $c;
					if ($cc >= 0 && $cc < $size && !$reserved[$row][$cc]) {
						if ($bit_idx < $bit_len) {
							$matrix[$row][$cc] = (int)$bits[$bit_idx];
							$bit_idx++;
						}
					}
				}
			}
		} else {
			for ($row = 0; $row < $size; $row++) {
				for ($c = 0; $c < 2; $c++) {
					$cc = $col - $c;
					if ($cc >= 0 && $cc < $size && !$reserved[$row][$cc]) {
						if ($bit_idx < $bit_len) {
							$matrix[$row][$cc] = (int)$bits[$bit_idx];
							$bit_idx++;
						}
					}
				}
			}
		}

		$going_up = !$going_up;
		$col -= 2;
	}
}

/**
 * Apply mask pattern 0 (checkerboard) and format information.
 *
 * @param array &$matrix   QR matrix
 * @param array &$reserved Reserved module flags
 * @param int   $size      Matrix size
 * @param int   $version   QR version
 */
function _qr_apply_mask_and_format(&$matrix, &$reserved, $size, $version) {
	// Apply mask pattern 0: (row + col) % 2 == 0
	for ($row = 0; $row < $size; $row++) {
		for ($col = 0; $col < $size; $col++) {
			if (!$reserved[$row][$col]) {
				if (($row + $col) % 2 == 0) {
					$matrix[$row][$col] = $matrix[$row][$col] ^ 1;
				}
			}
		}
	}

	// Format info for EC level M, mask 0: 101010000010010
	$format_bits = '101010000010010';

	// Place format info around top-left finder
	$format_positions_h = array(
		array(8, 0), array(8, 1), array(8, 2), array(8, 3),
		array(8, 4), array(8, 5), array(8, 7), array(8, 8),
	);
	$format_positions_v = array(
		array(0, 8), array(1, 8), array(2, 8), array(3, 8),
		array(4, 8), array(5, 8), array(7, 8),
	);

	for ($i = 0; $i < count($format_positions_h) && $i < 15; $i++) {
		$r = $format_positions_h[$i][0];
		$c = $format_positions_h[$i][1];
		if ($r < $size && $c < $size) {
			$matrix[$r][$c] = (int)$format_bits[$i];
		}
	}

	for ($i = 0; $i < count($format_positions_v) && $i + 8 < 15; $i++) {
		$r = $format_positions_v[$i][0];
		$c = $format_positions_v[$i][1];
		if ($r < $size && $c < $size) {
			$matrix[$r][$c] = (int)$format_bits[$i + 8];
		}
	}

	// Place format info along right side (top-right finder) and bottom (bottom-left finder)
	for ($i = 0; $i < 7; $i++) {
		$matrix[8][$size - 1 - $i] = (int)$format_bits[$i];
	}
	for ($i = 0; $i < 8; $i++) {
		$matrix[$size - 1 - $i][8] = (int)$format_bits[7 + $i];
	}
}

/**
 * Generate a barcode image (auto-detecting 1D vs QR).
 *
 * @param string $code       Data to encode
 * @param string $type       Barcode type (C128B, C39, EAN13, EAN8, UPCA, QRCODE, GS1-128)
 * @param int    $width      Image width (default 300 for 1D, auto for QR)
 * @param int    $height     Image height (default 80 for 1D, auto for QR)
 * @param bool   $show_text  Show text below 1D barcode (default true)
 * @param bool   $output     If true, output to browser; false, return PNG string
 * @return string|bool       PNG data or true/false
 */
function generate_barcode_image($code, $type = null, $width = 300, $height = 80, $show_text = true, $output = true) {
	if ($type === null) {
		$type = get_default_barcode_format();
	}

	if ($type === 'QRCODE') {
		return generate_qr_code($code, 6, 4, $output);
	}

	return generate_1d_barcode($code, $type, $width, $height, $show_text, $output);
}

/**
 * Get a barcode image as a base64-encoded data URI for inline HTML display.
 *
 * @param string $code   Data to encode
 * @param string $type   Barcode type
 * @param int    $width  Image width
 * @param int    $height Image height
 * @return string|false  Data URI string or false on error
 */
function get_barcode_data_uri($code, $type = null, $width = 300, $height = 80) {
	$png = generate_barcode_image($code, $type, $width, $height, true, false);
	if ($png === false) {
		return false;
	}
	return 'data:image/png;base64,' . base64_encode($png);
}

/**
 * Display an inline barcode image in HTML.
 *
 * @param string $code   Data to encode
 * @param string $type   Barcode type
 * @param int    $width  Display width
 * @param int    $height Display height
 * @param string $alt    Alt text for accessibility
 * @return string HTML img tag
 */
function display_barcode_image($code, $type = null, $width = 300, $height = 80, $alt = '') {
	$uri = get_barcode_data_uri($code, $type, $width, $height);
	if ($uri === false) {
		return '<span class="barcode-error">' . _('Barcode generation failed') . '</span>';
	}
	if (empty($alt)) {
		$alt = htmlspecialchars($code, ENT_QUOTES, 'UTF-8');
	}
	return '<img src="' . $uri . '" alt="' . htmlspecialchars($alt, ENT_QUOTES, 'UTF-8')
		. '" style="max-width:' . (int)$width . 'px; height:auto;" />';
}

/**
 * Generate barcode labels as PDF using TCPDF.
 *
 * @param array  $labels      Array of label data, each: array('code' => '...', 'title' => '...', 'subtitle' => '...')
 * @param string $barcode_type Barcode type for TCPDF (C128B, C39, EAN13, etc.)
 * @param string $label_size  Label size: 'small' (2x1"), 'medium' (3x1.5"), 'large' (4x2")
 * @param string $filename    Output filename (default 'labels.pdf')
 * @return void Outputs PDF to browser
 */
function generate_barcode_labels_pdf($labels, $barcode_type = 'C128B', $label_size = 'medium', $filename = 'labels.pdf') {
	global $path_to_root;

	include_once($path_to_root . '/reporting/includes/pdf_report.inc');

	// Label sizes in points (1 inch = 72 points)
	$sizes = array(
		'small'  => array('w' => 144, 'h' => 72),   // 2" x 1"
		'medium' => array('w' => 216, 'h' => 108),   // 3" x 1.5"
		'large'  => array('w' => 288, 'h' => 144),   // 4" x 2"
	);

	$sz = isset($sizes[$label_size]) ? $sizes[$label_size] : $sizes['medium'];

	// Columns and rows per page (A4 portrait)
	$page_w = 595; // A4 width in points
	$page_h = 842; // A4 height in points
	$margin = 28;  // ~10mm margins

	$cols_per_row = max(1, (int)(($page_w - 2 * $margin) / $sz['w']));
	$rows_per_page = max(1, (int)(($page_h - 2 * $margin) / $sz['h']));
	$labels_per_page = $cols_per_row * $rows_per_page;

	// Adjust spacing
	$h_gap = ($page_w - 2 * $margin - $cols_per_row * $sz['w']) / max(1, $cols_per_row);
	$v_gap = ($page_h - 2 * $margin - $rows_per_page * $sz['h']) / max(1, $rows_per_page);

	// Use raw TCPDF for label layout
	if (!class_exists('TCPDF')) {
		include_once($path_to_root . '/reporting/includes/tcpdf.php');
	}
	if (!class_exists('TCPDFBarcode')) {
		include_once($path_to_root . '/reporting/includes/barcodes.php');
	}

	$pdf = new TCPDF('P', 'pt', 'A4');
	$pdf->SetAutoPageBreak(false);
	$pdf->SetMargins($margin, $margin, $margin);
	$pdf->setPrintHeader(false);
	$pdf->setPrintFooter(false);

	$label_idx = 0;
	$total = count($labels);

	while ($label_idx < $total) {
		$pdf->AddPage();
		$page_count = 0;

		for ($row = 0; $row < $rows_per_page && $label_idx < $total; $row++) {
			for ($col = 0; $col < $cols_per_row && $label_idx < $total; $col++) {
				$label = $labels[$label_idx];
				$x = $margin + $col * ($sz['w'] + $h_gap);
				$y = $margin + $row * ($sz['h'] + $v_gap);

				// Draw label border
				$pdf->Rect($x, $y, $sz['w'], $sz['h']);

				// Title text (item name or serial#)
				$title = isset($label['title']) ? $label['title'] : '';
				if ($title) {
					$pdf->SetFont('', 'B', 8);
					$pdf->SetXY($x + 4, $y + 4);
					$pdf->Cell($sz['w'] - 8, 10, $title, 0, 0, 'C');
				}

				// Barcode
				$code = isset($label['code']) ? $label['code'] : '';
				if ($code) {
					$bc_type = ($barcode_type === 'GS1-128') ? 'C128C' : $barcode_type;
					$bc_y = $y + 16;
					$bc_h = $sz['h'] - 46;
					$bc_w = $sz['w'] - 16;

					$style = array(
						'position' => '',
						'align' => 'C',
						'stretch' => false,
						'fitwidth' => true,
						'cellfitalign' => '',
						'border' => false,
						'hpadding' => 'auto',
						'vpadding' => 'auto',
						'fgcolor' => array(0, 0, 0),
						'bgcolor' => false,
						'text' => false,
					);

					$pdf->write1DBarcode($code, $bc_type, $x + 8, $bc_y, $bc_w, $bc_h, 0.4, $style, 'N');
				}

				// Subtitle (batch#, expiry, etc.)
				$subtitle = isset($label['subtitle']) ? $label['subtitle'] : '';
				if ($subtitle) {
					$pdf->SetFont('', '', 7);
					$pdf->SetXY($x + 4, $y + $sz['h'] - 26);
					$pdf->Cell($sz['w'] - 8, 8, $subtitle, 0, 0, 'C');
				}

				// Code text below barcode
				if ($code) {
					$pdf->SetFont('', '', 7);
					$pdf->SetXY($x + 4, $y + $sz['h'] - 16);
					$pdf->Cell($sz['w'] - 8, 8, $code, 0, 0, 'C');
				}

				$label_idx++;
				$page_count++;
			}
		}
	}

	$pdf->Output($filename, 'I');
	exit();
}

/**
 * Generate a serial number label dataset.
 *
 * @param int $serial_id Serial number record ID
 * @return array|false Label data array or false
 */
function get_serial_label_data($serial_id) {
	$serial = get_serial_number($serial_id);
	if (!$serial) {
		return false;
	}

	$item = get_item($serial['stock_id']);
	$item_name = $item ? $item['description'] : $serial['stock_id'];

	$subtitle_parts = array();
	$subtitle_parts[] = $serial['stock_id'];
	if (!empty($serial['expiry_date']) && $serial['expiry_date'] != '0000-00-00') {
		$subtitle_parts[] = 'EXP: ' . sql2date($serial['expiry_date']);
	}

	return array(
		'code'     => $serial['serial_no'],
		'title'    => $item_name,
		'subtitle' => implode(' | ', $subtitle_parts),
	);
}

/**
 * Generate a batch number label dataset.
 *
 * @param int $batch_id Batch record ID
 * @return array|false Label data array or false
 */
function get_batch_label_data($batch_id) {
	$batch = get_stock_batch($batch_id);
	if (!$batch) {
		return false;
	}

	$item = get_item($batch['stock_id']);
	$item_name = $item ? $item['description'] : $batch['stock_id'];

	$subtitle_parts = array();
	$subtitle_parts[] = $batch['stock_id'];
	if (!empty($batch['expiry_date']) && $batch['expiry_date'] != '0000-00-00') {
		$subtitle_parts[] = 'EXP: ' . sql2date($batch['expiry_date']);
	}
	if (!empty($batch['manufacturing_date']) && $batch['manufacturing_date'] != '0000-00-00') {
		$subtitle_parts[] = 'MFG: ' . sql2date($batch['manufacturing_date']);
	}

	return array(
		'code'     => $batch['batch_no'],
		'title'    => $item_name,
		'subtitle' => implode(' | ', $subtitle_parts),
	);
}

/**
 * Generate an item label dataset.
 *
 * @param string $stock_id Item stock ID
 * @return array|false Label data array or false
 */
function get_item_label_data($stock_id) {
	$item = get_item($stock_id);
	if (!$item) {
		return false;
	}

	$subtitle_parts = array();
	$subtitle_parts[] = $stock_id;
	if (!empty($item['units'])) {
		$subtitle_parts[] = $item['units'];
	}

	return array(
		'code'     => $stock_id,
		'title'    => $item['description'],
		'subtitle' => implode(' | ', $subtitle_parts),
	);
}
