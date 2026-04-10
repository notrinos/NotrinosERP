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
 * RFID Webhook Interface
 *
 * Provides a webhook endpoint and helper functions for RFID reader integration.
 * Supports receiving scan events from RFID hardware via HTTP POST,
 * and provides functions for RFID tag encoding/decoding using EPC standards.
 *
 * Integration pattern:
 *   RFID Reader → HTTP POST to /inventory/includes/rfid_interface.inc?action=webhook
 *   → Parse EPC → Look up item/serial/batch → Return JSON result
 *
 * Supported EPC tag types:
 *   - SGTIN-96 (Serialized Global Trade Item Number)
 *   - SGTIN-198 (SGTIN with alphanumeric serial)
 *   - SSCC (Serial Shipping Container Code)
 *
 * Security: Webhook access requires a valid API key configured in company preferences.
 */

/**
 * Process an RFID scan event.
 *
 * Called by RFID hardware/middleware that sends scanned tag data via HTTP POST.
 * Expects JSON body with 'epc' (hex string) or 'tags' (array of hex strings).
 *
 * @param string $epc_hex   EPC tag data as hexadecimal string
 * @param string $reader_id Optional reader identifier
 * @return array Result with decoded tag info and database matches
 */
function process_rfid_scan($epc_hex, $reader_id = '') {
	$result = array(
		'success'   => false,
		'reader_id' => $reader_id,
		'epc_hex'   => $epc_hex,
		'tag_type'  => 'unknown',
		'decoded'   => array(),
		'matches'   => array(),
	);

	if (empty($epc_hex)) {
		$result['error'] = 'Empty EPC data';
		return $result;
	}

	// Clean hex string
	$epc_hex = strtoupper(preg_replace('/[^0-9A-Fa-f]/', '', $epc_hex));

	// Determine tag type from header
	$header = hexdec(substr($epc_hex, 0, 2));

	if ($header === 0x30) {
		// SGTIN-96
		$decoded = decode_sgtin96($epc_hex);
		$result['tag_type'] = 'SGTIN-96';
		$result['decoded'] = $decoded;
	} elseif ($header === 0x36) {
		// SGTIN-198
		$decoded = decode_sgtin198($epc_hex);
		$result['tag_type'] = 'SGTIN-198';
		$result['decoded'] = $decoded;
	} elseif ($header === 0x31) {
		// SSCC-96
		$decoded = decode_sscc96($epc_hex);
		$result['tag_type'] = 'SSCC-96';
		$result['decoded'] = $decoded;
	} else {
		$result['error'] = 'Unknown EPC header: 0x' . dechex($header);
		return $result;
	}

	// Try to look up the decoded data in the database
	if (isset($decoded['gtin'])) {
		// Include lookup functions (guard against double-include)
		$item_match = _rfid_lookup_item($decoded['gtin']);
		if ($item_match) {
			$result['matches'][] = $item_match;
		}
	}

	if (isset($decoded['serial'])) {
		$serial_match = _rfid_lookup_serial($decoded['serial']);
		if ($serial_match) {
			$result['matches'][] = $serial_match;
		}
	}

	$result['success'] = !empty($result['matches']);
	return $result;
}

/**
 * Decode SGTIN-96 EPC tag. (96-bit tag = 24 hex chars)
 *
 * Structure: Header(8) | Filter(3) | Partition(3) | CompanyPrefix(20-40) | ItemRef(24-4) | Serial(38)
 *
 * @param string $epc_hex 24-character hex string
 * @return array Decoded fields: filter, company_prefix, item_ref, serial, gtin
 */
function decode_sgtin96($epc_hex) {
	// Convert to binary
	$bin = '';
	for ($i = 0; $i < strlen($epc_hex); $i++) {
		$bin .= str_pad(decbin(hexdec($epc_hex[$i])), 4, '0', STR_PAD_LEFT);
	}

	$result = array();

	// Bits 0-7: Header (0x30 = 00110000)
	// Bits 8-10: Filter value
	$result['filter'] = bindec(substr($bin, 8, 3));

	// Bits 11-13: Partition
	$partition = bindec(substr($bin, 11, 3));
	$result['partition'] = $partition;

	// Partition table for SGTIN-96
	$partition_table = array(
		0 => array('cp_bits' => 40, 'cp_digits' => 12, 'ir_bits' => 4,  'ir_digits' => 1),
		1 => array('cp_bits' => 37, 'cp_digits' => 11, 'ir_bits' => 7,  'ir_digits' => 2),
		2 => array('cp_bits' => 34, 'cp_digits' => 10, 'ir_bits' => 10, 'ir_digits' => 3),
		3 => array('cp_bits' => 30, 'cp_digits' => 9,  'ir_bits' => 14, 'ir_digits' => 4),
		4 => array('cp_bits' => 27, 'cp_digits' => 8,  'ir_bits' => 17, 'ir_digits' => 5),
		5 => array('cp_bits' => 24, 'cp_digits' => 7,  'ir_bits' => 20, 'ir_digits' => 6),
		6 => array('cp_bits' => 20, 'cp_digits' => 6,  'ir_bits' => 24, 'ir_digits' => 7),
	);

	if (!isset($partition_table[$partition])) {
		$result['error'] = 'Invalid partition value';
		return $result;
	}

	$pt = $partition_table[$partition];

	// Company prefix
	$cp_val = bindec(substr($bin, 14, $pt['cp_bits']));
	$result['company_prefix'] = str_pad($cp_val, $pt['cp_digits'], '0', STR_PAD_LEFT);

	// Item reference
	$ir_val = bindec(substr($bin, 14 + $pt['cp_bits'], $pt['ir_bits']));
	$result['item_ref'] = str_pad($ir_val, $pt['ir_digits'], '0', STR_PAD_LEFT);

	// Serial number (38 bits)
	$serial_val = bindec(substr($bin, 58, 38));
	$result['serial'] = (string)$serial_val;

	// Reconstruct GTIN-14
	$indicator = substr($result['item_ref'], 0, 1);
	$gtin_body = $indicator . $result['company_prefix'] . substr($result['item_ref'], 1);
	$gtin_body = str_pad($gtin_body, 13, '0', STR_PAD_LEFT);
	$result['gtin'] = calculate_gtin_check_digit($gtin_body);

	return $result;
}

/**
 * Decode SGTIN-198 EPC tag. (198-bit tag = ~50 hex chars)
 *
 * Like SGTIN-96 but with an alphanumeric serial field (140 bits).
 *
 * @param string $epc_hex Hex string
 * @return array Decoded fields
 */
function decode_sgtin198($epc_hex) {
	$bin = '';
	for ($i = 0; $i < strlen($epc_hex); $i++) {
		$bin .= str_pad(decbin(hexdec($epc_hex[$i])), 4, '0', STR_PAD_LEFT);
	}

	$result = array();

	$result['filter'] = bindec(substr($bin, 8, 3));
	$partition = bindec(substr($bin, 11, 3));
	$result['partition'] = $partition;

	$partition_table = array(
		0 => array('cp_bits' => 40, 'cp_digits' => 12, 'ir_bits' => 4,  'ir_digits' => 1),
		1 => array('cp_bits' => 37, 'cp_digits' => 11, 'ir_bits' => 7,  'ir_digits' => 2),
		2 => array('cp_bits' => 34, 'cp_digits' => 10, 'ir_bits' => 10, 'ir_digits' => 3),
		3 => array('cp_bits' => 30, 'cp_digits' => 9,  'ir_bits' => 14, 'ir_digits' => 4),
		4 => array('cp_bits' => 27, 'cp_digits' => 8,  'ir_bits' => 17, 'ir_digits' => 5),
		5 => array('cp_bits' => 24, 'cp_digits' => 7,  'ir_bits' => 20, 'ir_digits' => 6),
		6 => array('cp_bits' => 20, 'cp_digits' => 6,  'ir_bits' => 24, 'ir_digits' => 7),
	);

	if (!isset($partition_table[$partition])) {
		$result['error'] = 'Invalid partition value';
		return $result;
	}

	$pt = $partition_table[$partition];

	$cp_val = bindec(substr($bin, 14, $pt['cp_bits']));
	$result['company_prefix'] = str_pad($cp_val, $pt['cp_digits'], '0', STR_PAD_LEFT);

	$ir_val = bindec(substr($bin, 14 + $pt['cp_bits'], $pt['ir_bits']));
	$result['item_ref'] = str_pad($ir_val, $pt['ir_digits'], '0', STR_PAD_LEFT);

	// Alphanumeric serial (140 bits = 20 x 7-bit ASCII chars)
	$serial = '';
	$serial_start = 14 + $pt['cp_bits'] + $pt['ir_bits'];
	for ($i = 0; $i < 20; $i++) {
		$char_val = bindec(substr($bin, $serial_start + ($i * 7), 7));
		if ($char_val > 0) {
			$serial .= chr($char_val);
		}
	}
	$result['serial'] = rtrim($serial, "\0");

	// Reconstruct GTIN-14
	$indicator = substr($result['item_ref'], 0, 1);
	$gtin_body = $indicator . $result['company_prefix'] . substr($result['item_ref'], 1);
	$gtin_body = str_pad($gtin_body, 13, '0', STR_PAD_LEFT);

	if (function_exists('calculate_gtin_check_digit')) {
		$result['gtin'] = calculate_gtin_check_digit($gtin_body);
	} else {
		$result['gtin'] = $gtin_body . '0'; // placeholder
	}

	return $result;
}

/**
 * Decode SSCC-96 EPC tag. (Serial Shipping Container Code)
 *
 * @param string $epc_hex Hex string
 * @return array Decoded fields: filter, company_prefix, serial_ref, sscc
 */
function decode_sscc96($epc_hex) {
	$bin = '';
	for ($i = 0; $i < strlen($epc_hex); $i++) {
		$bin .= str_pad(decbin(hexdec($epc_hex[$i])), 4, '0', STR_PAD_LEFT);
	}

	$result = array();

	$result['filter'] = bindec(substr($bin, 8, 3));
	$partition = bindec(substr($bin, 11, 3));
	$result['partition'] = $partition;

	$partition_table = array(
		0 => array('cp_bits' => 40, 'cp_digits' => 12, 'sr_bits' => 18, 'sr_digits' => 5),
		1 => array('cp_bits' => 37, 'cp_digits' => 11, 'sr_bits' => 21, 'sr_digits' => 6),
		2 => array('cp_bits' => 34, 'cp_digits' => 10, 'sr_bits' => 24, 'sr_digits' => 7),
		3 => array('cp_bits' => 30, 'cp_digits' => 9,  'sr_bits' => 28, 'sr_digits' => 8),
		4 => array('cp_bits' => 27, 'cp_digits' => 8,  'sr_bits' => 31, 'sr_digits' => 9),
		5 => array('cp_bits' => 24, 'cp_digits' => 7,  'sr_bits' => 34, 'sr_digits' => 10),
		6 => array('cp_bits' => 20, 'cp_digits' => 6,  'sr_bits' => 38, 'sr_digits' => 11),
	);

	if (!isset($partition_table[$partition])) {
		$result['error'] = 'Invalid partition value';
		return $result;
	}

	$pt = $partition_table[$partition];

	$cp_val = bindec(substr($bin, 14, $pt['cp_bits']));
	$result['company_prefix'] = str_pad($cp_val, $pt['cp_digits'], '0', STR_PAD_LEFT);

	$sr_val = bindec(substr($bin, 14 + $pt['cp_bits'], $pt['sr_bits']));
	$result['serial_ref'] = str_pad($sr_val, $pt['sr_digits'], '0', STR_PAD_LEFT);

	// Reconstruct SSCC-18
	$sscc_body = '0' . $result['company_prefix'] . $result['serial_ref'];
	$sscc_body = str_pad($sscc_body, 17, '0', STR_PAD_LEFT);
	// Calculate check digit
	$sum = 0;
	for ($i = 0; $i < 17; $i++) {
		$digit = (int)$sscc_body[$i];
		$weight = ($i % 2 === 0) ? 3 : 1;
		$sum += $digit * $weight;
	}
	$check = (10 - ($sum % 10)) % 10;
	$result['sscc'] = $sscc_body . $check;

	return $result;
}

/**
 * Look up an item by GTIN for RFID result context.
 *
 * @param string $gtin GTIN-14 string
 * @return array|false Match data or false
 */
function _rfid_lookup_item($gtin) {
	$sql = "SELECT stock_id, description, units
			FROM " . TB_PREF . "stock_master
			WHERE stock_id = " . db_escape($gtin) . "
			AND inactive = 0
			LIMIT 1";
	$res = db_query($sql, 'RFID item lookup');
	$row = db_fetch_assoc($res);

	if ($row) {
		return array(
			'match_type'  => 'item',
			'stock_id'    => $row['stock_id'],
			'description' => $row['description'],
			'units'       => $row['units'],
		);
	}

	// Try item_codes table
	$sql = "SELECT fi.stock_id, sm.description, sm.units
			FROM " . TB_PREF . "item_codes fi
			JOIN " . TB_PREF . "stock_master sm ON sm.stock_id = fi.stock_id
			WHERE fi.item_code = " . db_escape($gtin) . "
			AND sm.inactive = 0
			LIMIT 1";
	$res = db_query($sql, 'RFID GTIN lookup');
	$row = db_fetch_assoc($res);

	if ($row) {
		return array(
			'match_type'  => 'item',
			'stock_id'    => $row['stock_id'],
			'description' => $row['description'],
			'units'       => $row['units'],
		);
	}

	return false;
}

/**
 * Look up a serial number for RFID result context.
 *
 * @param string $serial Serial number string
 * @return array|false Match data or false
 */
function _rfid_lookup_serial($serial) {
	$sql = "SELECT sn.id, sn.serial_no, sn.stock_id, sn.status, sm.description
			FROM " . TB_PREF . "serial_numbers sn
			LEFT JOIN " . TB_PREF . "stock_master sm ON sm.stock_id = sn.stock_id
			WHERE sn.serial_no = " . db_escape($serial) . "
			LIMIT 1";
	$res = db_query($sql, 'RFID serial lookup');
	$row = db_fetch_assoc($res);

	if ($row) {
		return array(
			'match_type'  => 'serial',
			'serial_id'   => (int)$row['id'],
			'serial_no'   => $row['serial_no'],
			'stock_id'    => $row['stock_id'],
			'description' => $row['description'],
			'status'      => $row['status'],
		);
	}

	return false;
}

/**
 * Validate RFID webhook API key from request headers.
 *
 * The API key is stored in company preferences under 'rfid_api_key'.
 * The webhook client must send the key in the X-RFID-API-Key header.
 *
 * @return bool True if API key is valid
 */
function validate_rfid_api_key() {
	$stored_key = get_company_pref('rfid_api_key');
	if (empty($stored_key)) {
		return false;
	}

	$provided_key = '';
	if (isset($_SERVER['HTTP_X_RFID_API_KEY'])) {
		$provided_key = $_SERVER['HTTP_X_RFID_API_KEY'];
	} elseif (isset($_GET['api_key'])) {
		$provided_key = $_GET['api_key'];
	}

	return hash_equals($stored_key, $provided_key);
}

/**
 * Generate a random API key for RFID webhook authentication.
 *
 * @return string 64-character hex string
 */
function generate_rfid_api_key() {
	if (function_exists('random_bytes')) {
		return bin2hex(random_bytes(32));
	}
	// Fallback for PHP < 7.0
	$chars = '0123456789abcdef';
	$key = '';
	for ($i = 0; $i < 64; $i++) {
		$key .= $chars[mt_rand(0, 15)];
	}
	return $key;
}

/**
 * Log an RFID scan event for audit trail.
 *
 * Stores the scan in the serial_movements or batch_movements table
 * when a match is found.
 *
 * @param array  $scan_result Result from process_rfid_scan()
 * @param string $context     Scan context (e.g., 'warehouse_entry', 'shipping_dock')
 * @return void
 */
function log_rfid_scan_event($scan_result, $context = '') {
	// Only log if we found matches
	if (empty($scan_result['matches'])) {
		return;
	}

	foreach ($scan_result['matches'] as $match) {
		if ($match['match_type'] === 'serial' && isset($match['serial_id'])) {
			if (function_exists('add_serial_movement')) {
				add_serial_movement(
					$match['serial_id'],
					'rfid_scan',
					'',   // trans_type
					0,    // trans_no
					isset($match['loc_code']) ? $match['loc_code'] : '',
					isset($match['loc_code']) ? $match['loc_code'] : '',
					isset($match['status']) ? $match['status'] : '',
					isset($match['status']) ? $match['status'] : '',
					'RFID scan' . ($context ? ' (' . $context . ')' : '')
						. ' Reader: ' . $scan_result['reader_id']
				);
			}
		}
	}
}
