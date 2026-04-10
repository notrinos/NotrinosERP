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
 * GS1 Standard Encoding and Parsing Library
 *
 * Implements GS1 Application Identifier (AI) encoding and parsing
 * for barcode data exchange, supporting:
 *   - AI 01 (GTIN — Global Trade Item Number, 14 digits)
 *   - AI 10 (Batch/Lot Number, up to 20 chars)
 *   - AI 17 (Expiry Date, YYMMDD)
 *   - AI 21 (Serial Number, up to 20 chars)
 *   - AI 11 (Production Date, YYMMDD)
 *   - AI 37 (Count of trade items, up to 8 digits)
 *   - AI 30 (Variable count, up to 8 digits)
 *
 * Standards: GS1 General Specifications v22
 * Compliance: DSCSA (Drug Supply Chain Security Act),
 *             FMD (Falsified Medicines Directive),
 *             FSMA 204 (Food Safety Modernization Act),
 *             UDI (Unique Device Identification)
 */

/**
 * GS1 Application Identifier definitions.
 *
 * Each AI entry contains:
 *   'name'       => Human-readable name
 *   'fixed_len'  => Fixed length (null if variable)
 *   'max_len'    => Maximum length for variable AIs
 *   'type'       => 'N' (numeric), 'AN' (alphanumeric), 'DATE' (YYMMDD)
 *   'fnc1'       => Whether FNC1/GS separator is required after this AI (for variable-length AIs)
 *
 * @return array Associative array of AI_code => AI definition
 */
function get_gs1_application_identifiers() {
	return array(
		'01' => array(
			'name'      => _('GTIN'),
			'fixed_len' => 14,
			'max_len'   => 14,
			'type'      => 'N',
			'fnc1'      => false,
		),
		'02' => array(
			'name'      => _('Content GTIN'),
			'fixed_len' => 14,
			'max_len'   => 14,
			'type'      => 'N',
			'fnc1'      => false,
		),
		'10' => array(
			'name'      => _('Batch/Lot'),
			'fixed_len' => null,
			'max_len'   => 20,
			'type'      => 'AN',
			'fnc1'      => true,
		),
		'11' => array(
			'name'      => _('Production Date'),
			'fixed_len' => 6,
			'max_len'   => 6,
			'type'      => 'DATE',
			'fnc1'      => false,
		),
		'13' => array(
			'name'      => _('Packaging Date'),
			'fixed_len' => 6,
			'max_len'   => 6,
			'type'      => 'DATE',
			'fnc1'      => false,
		),
		'15' => array(
			'name'      => _('Best Before Date'),
			'fixed_len' => 6,
			'max_len'   => 6,
			'type'      => 'DATE',
			'fnc1'      => false,
		),
		'17' => array(
			'name'      => _('Expiry Date'),
			'fixed_len' => 6,
			'max_len'   => 6,
			'type'      => 'DATE',
			'fnc1'      => false,
		),
		'21' => array(
			'name'      => _('Serial Number'),
			'fixed_len' => null,
			'max_len'   => 20,
			'type'      => 'AN',
			'fnc1'      => true,
		),
		'30' => array(
			'name'      => _('Variable Count'),
			'fixed_len' => null,
			'max_len'   => 8,
			'type'      => 'N',
			'fnc1'      => true,
		),
		'37' => array(
			'name'      => _('Count'),
			'fixed_len' => null,
			'max_len'   => 8,
			'type'      => 'N',
			'fnc1'      => true,
		),
		'240' => array(
			'name'      => _('Additional Product ID'),
			'fixed_len' => null,
			'max_len'   => 30,
			'type'      => 'AN',
			'fnc1'      => true,
		),
		'310' => array(
			'name'      => _('Net Weight (kg)'),
			'fixed_len' => 6,
			'max_len'   => 6,
			'type'      => 'N',
			'fnc1'      => false,
		),
	);
}

/**
 * Parse a GS1 barcode string into its component Application Identifiers.
 *
 * Handles both parenthesized format "(01)12345678901234(10)BATCH123"
 * and raw FNC1-separated format (where GS char chr(29) separates variable-length AIs).
 *
 * @param string $barcode_data Raw scanned barcode data
 * @return array Associative array of AI_code => decoded_value, plus '_raw' => original input
 */
function parse_gs1_barcode($barcode_data) {
	$result = array('_raw' => $barcode_data);

	if (empty($barcode_data)) {
		return $result;
	}

	$data = trim($barcode_data);
	$ai_defs = get_gs1_application_identifiers();

	// Detect format: parenthesized or raw
	if (strpos($data, '(') !== false) {
		// Parenthesized format: (01)12345678901234(10)BATCH123(17)261231(21)SN001
		$result = $result + _parse_gs1_parenthesized($data, $ai_defs);
	} else {
		// Raw FNC1 format: strip leading FNC1 if present
		if (substr($data, 0, 1) === chr(29) || substr($data, 0, 3) === ']d2' || substr($data, 0, 3) === ']C1') {
			// Remove symbology identifier
			if (substr($data, 0, 3) === ']d2' || substr($data, 0, 3) === ']C1') {
				$data = substr($data, 3);
			} elseif (substr($data, 0, 1) === chr(29)) {
				$data = substr($data, 1);
			}
		}
		$result = $result + _parse_gs1_raw($data, $ai_defs);
	}

	// Post-process: convert dates from YYMMDD to SQL format
	foreach ($result as $ai => $value) {
		if (isset($ai_defs[$ai]) && $ai_defs[$ai]['type'] === 'DATE' && strlen($value) === 6) {
			$result[$ai . '_formatted'] = gs1_date_to_sql($value);
		}
	}

	return $result;
}

/**
 * Parse parenthesized GS1 format.
 *
 * @param string $data    Parenthesized barcode string
 * @param array  $ai_defs Application Identifier definitions
 * @return array Parsed AI => value pairs
 */
function _parse_gs1_parenthesized($data, $ai_defs) {
	$result = array();

	// Match pattern: (AI)VALUE
	if (preg_match_all('/\((\d{2,4})\)([^()]*?)(?=\(\d{2,4}\)|$)/', $data, $matches, PREG_SET_ORDER)) {
		foreach ($matches as $match) {
			$ai = $match[1];
			$value = $match[2];

			if (isset($ai_defs[$ai])) {
				// Validate length
				$max_len = $ai_defs[$ai]['max_len'];
				if (strlen($value) > $max_len) {
					$value = substr($value, 0, $max_len);
				}
				$result[$ai] = $value;
			} else {
				// Unknown AI — store anyway
				$result[$ai] = $value;
			}
		}
	}

	return $result;
}

/**
 * Parse raw FNC1-separated GS1 format.
 *
 * @param string $data    Raw barcode data (FNC1 = chr(29) as separator)
 * @param array  $ai_defs Application Identifier definitions
 * @return array Parsed AI => value pairs
 */
function _parse_gs1_raw($data, $ai_defs) {
	$result = array();
	$pos = 0;
	$len = strlen($data);
	$gs = chr(29); // Group separator

	while ($pos < $len) {
		$ai = null;
		$ai_def = null;

		// Try 2-digit AI first, then 3, then 4
		foreach (array(2, 3, 4) as $ai_len) {
			$candidate = substr($data, $pos, $ai_len);
			if (isset($ai_defs[$candidate])) {
				$ai = $candidate;
				$ai_def = $ai_defs[$candidate];
				$pos += $ai_len;
				break;
			}
		}

		if ($ai === null) {
			// Unknown AI — skip one character
			$pos++;
			continue;
		}

		// Extract value
		if ($ai_def['fixed_len'] !== null) {
			// Fixed-length AI
			$value = substr($data, $pos, $ai_def['fixed_len']);
			$pos += $ai_def['fixed_len'];
		} else {
			// Variable-length AI — read until GS or end
			$gs_pos = strpos($data, $gs, $pos);
			if ($gs_pos !== false) {
				$value = substr($data, $pos, $gs_pos - $pos);
				$pos = $gs_pos + 1; // Skip the GS character
			} else {
				$value = substr($data, $pos);
				$pos = $len;
			}

			// Enforce max length
			if (strlen($value) > $ai_def['max_len']) {
				$value = substr($value, 0, $ai_def['max_len']);
			}
		}

		$result[$ai] = $value;
	}

	return $result;
}

/**
 * Encode data into a GS1 barcode string (parenthesized format).
 *
 * @param array $ai_values Associative array of AI_code => value
 * @return string Parenthesized GS1 barcode string
 */
function encode_gs1_barcode($ai_values) {
	$ai_defs = get_gs1_application_identifiers();
	$result = '';

	// Sort by AI code for consistent output
	ksort($ai_values);

	foreach ($ai_values as $ai => $value) {
		if (isset($ai_defs[$ai])) {
			$def = $ai_defs[$ai];

			// Validate and format
			if ($def['type'] === 'N') {
				$value = preg_replace('/[^0-9]/', '', $value);
			}
			if ($def['type'] === 'DATE') {
				$value = preg_replace('/[^0-9]/', '', $value);
				$value = str_pad($value, 6, '0', STR_PAD_RIGHT);
			}
			if ($def['fixed_len'] !== null) {
				$value = str_pad(substr($value, 0, $def['fixed_len']), $def['fixed_len'], '0', STR_PAD_LEFT);
			} elseif (strlen($value) > $def['max_len']) {
				$value = substr($value, 0, $def['max_len']);
			}
		}

		$result .= '(' . $ai . ')' . $value;
	}

	return $result;
}

/**
 * Encode data into a raw GS1 string for barcode generation (FNC1-separated).
 *
 * @param array $ai_values Associative array of AI_code => value
 * @return string Raw GS1 string with GS separators
 */
function encode_gs1_raw($ai_values) {
	$ai_defs = get_gs1_application_identifiers();
	$result = '';
	$gs = chr(29);

	ksort($ai_values);

	$keys = array_keys($ai_values);
	$count = count($keys);

	for ($i = 0; $i < $count; $i++) {
		$ai = $keys[$i];
		$value = $ai_values[$ai];

		if (isset($ai_defs[$ai])) {
			$def = $ai_defs[$ai];

			if ($def['type'] === 'N') {
				$value = preg_replace('/[^0-9]/', '', $value);
			}
			if ($def['type'] === 'DATE') {
				$value = preg_replace('/[^0-9]/', '', $value);
				$value = str_pad($value, 6, '0', STR_PAD_RIGHT);
			}
			if ($def['fixed_len'] !== null) {
				$value = str_pad(substr($value, 0, $def['fixed_len']), $def['fixed_len'], '0', STR_PAD_LEFT);
			}
		}

		$result .= $ai . $value;

		// Add GS separator after variable-length AIs (except last)
		if ($i < $count - 1 && isset($ai_defs[$ai]) && $ai_defs[$ai]['fnc1']) {
			$result .= $gs;
		}
	}

	return $result;
}

/**
 * Convert a GS1 date (YYMMDD) to SQL date format (YYYY-MM-DD).
 *
 * @param string $gs1_date 6-digit YYMMDD string
 * @return string SQL date string YYYY-MM-DD
 */
function gs1_date_to_sql($gs1_date) {
	if (strlen($gs1_date) !== 6) {
		return null;
	}

	$yy = (int)substr($gs1_date, 0, 2);
	$mm = substr($gs1_date, 2, 2);
	$dd = substr($gs1_date, 4, 2);

	// GS1 rule: YY 51-99 = 19xx, YY 00-50 = 20xx
	$yyyy = ($yy >= 51) ? (1900 + $yy) : (2000 + $yy);

	// DD = 00 means last day of month
	if ($dd === '00') {
		$dd = date('t', mktime(0, 0, 0, (int)$mm, 1, $yyyy));
	}

	return sprintf('%04d-%s-%s', $yyyy, $mm, $dd);
}

/**
 * Convert a SQL date (YYYY-MM-DD) to GS1 date format (YYMMDD).
 *
 * @param string $sql_date SQL date string YYYY-MM-DD
 * @return string 6-digit YYMMDD string
 */
function sql_date_to_gs1($sql_date) {
	if (empty($sql_date) || $sql_date === '0000-00-00') {
		return '000000';
	}

	$parts = explode('-', $sql_date);
	if (count($parts) !== 3) {
		return '000000';
	}

	$yy = substr($parts[0], 2, 2);
	return $yy . $parts[1] . $parts[2];
}

/**
 * Build a GS1 barcode string for an item with serial/batch/expiry data.
 *
 * This is the main convenience function for encoding product data.
 *
 * @param string $gtin        GTIN (14 digits, or item stock_id padded)
 * @param string $serial_no   Serial number (optional)
 * @param string $batch_no    Batch/Lot number (optional)
 * @param string $expiry_date SQL format expiry date (optional)
 * @param string $prod_date   SQL format production date (optional)
 * @return string GS1 barcode string in parenthesized format
 */
function build_gs1_barcode_string($gtin, $serial_no = '', $batch_no = '', $expiry_date = '', $prod_date = '') {
	$ai_values = array();

	// AI 01: GTIN (pad to 14 digits)
	$gtin = preg_replace('/[^0-9]/', '', $gtin);
	if (strlen($gtin) < 14) {
		$gtin = str_pad($gtin, 14, '0', STR_PAD_LEFT);
	}
	$ai_values['01'] = substr($gtin, 0, 14);

	// AI 10: Batch/Lot
	if (!empty($batch_no)) {
		$ai_values['10'] = substr($batch_no, 0, 20);
	}

	// AI 17: Expiry Date
	if (!empty($expiry_date) && $expiry_date !== '0000-00-00') {
		$ai_values['17'] = sql_date_to_gs1($expiry_date);
	}

	// AI 21: Serial Number
	if (!empty($serial_no)) {
		$ai_values['21'] = substr($serial_no, 0, 20);
	}

	// AI 11: Production Date
	if (!empty($prod_date) && $prod_date !== '0000-00-00') {
		$ai_values['11'] = sql_date_to_gs1($prod_date);
	}

	return encode_gs1_barcode($ai_values);
}

/**
 * Validate a GTIN check digit.
 *
 * @param string $gtin GTIN string (8, 12, 13, or 14 digits)
 * @return bool True if check digit is valid
 */
function validate_gtin_check_digit($gtin) {
	$gtin = preg_replace('/[^0-9]/', '', $gtin);
	$len = strlen($gtin);

	if (!in_array($len, array(8, 12, 13, 14))) {
		return false;
	}

	$sum = 0;
	for ($i = 0; $i < $len - 1; $i++) {
		$digit = (int)$gtin[$i];
		// Weight alternates: for GTIN-14, positions from right: odd=3, even=1
		$weight = (($len - 1 - $i) % 2 === 0) ? 1 : 3;
		$sum += $digit * $weight;
	}

	$check = (10 - ($sum % 10)) % 10;
	return $check === (int)$gtin[$len - 1];
}

/**
 * Calculate and append a GTIN check digit.
 *
 * @param string $gtin_without_check GTIN without check digit (7, 11, 12, or 13 digits)
 * @return string Complete GTIN with check digit
 */
function calculate_gtin_check_digit($gtin_without_check) {
	$gtin = preg_replace('/[^0-9]/', '', $gtin_without_check);
	$len = strlen($gtin);

	$sum = 0;
	for ($i = 0; $i < $len; $i++) {
		$digit = (int)$gtin[$i];
		$weight = (($len - $i) % 2 === 0) ? 1 : 3;
		$sum += $digit * $weight;
	}

	$check = (10 - ($sum % 10)) % 10;
	return $gtin . $check;
}

/**
 * Try to identify what type of barcode data was scanned.
 *
 * @param string $data Scanned barcode data
 * @return array Array with keys: 'type' (gs1|serial|batch|item|ean|unknown), 'data' (parsed data)
 */
function identify_barcode_data($data) {
	$data = trim($data);

	if (empty($data)) {
		return array('type' => 'unknown', 'data' => array());
	}

	// Check for GS1 format (parenthesized or with FNC1 prefix)
	if (strpos($data, '(01)') !== false || strpos($data, '(10)') !== false ||
		strpos($data, '(21)') !== false || substr($data, 0, 3) === ']d2' ||
		substr($data, 0, 3) === ']C1' || substr($data, 0, 2) === '01') {

		$parsed = parse_gs1_barcode($data);
		if (count($parsed) > 1) { // More than just _raw
			return array('type' => 'gs1', 'data' => $parsed);
		}
	}

	// Check for EAN-13/EAN-8/UPC
	$numeric_data = preg_replace('/[^0-9]/', '', $data);
	if (strlen($numeric_data) === strlen($data)) {
		if (in_array(strlen($data), array(8, 12, 13, 14)) && validate_gtin_check_digit($data)) {
			return array('type' => 'ean', 'data' => array('code' => $data, 'length' => strlen($data)));
		}
	}

	// Could be a serial number, batch number, or item code — return as-is for DB lookup
	return array('type' => 'unknown', 'data' => array('code' => $data));
}

/**
 * Get a human-readable summary of parsed GS1 data.
 *
 * @param array $parsed Parsed GS1 result from parse_gs1_barcode()
 * @return string Formatted summary text
 */
function format_gs1_summary($parsed) {
	$ai_defs = get_gs1_application_identifiers();
	$lines = array();

	foreach ($parsed as $ai => $value) {
		if ($ai === '_raw' || strpos($ai, '_formatted') !== false) {
			continue;
		}
		$name = isset($ai_defs[$ai]) ? $ai_defs[$ai]['name'] : _('AI') . ' ' . $ai;

		// Use formatted date if available
		if (isset($parsed[$ai . '_formatted'])) {
			$value .= ' (' . $parsed[$ai . '_formatted'] . ')';
		}

		$lines[] = $name . ': ' . $value;
	}

	return implode("\n", $lines);
}
