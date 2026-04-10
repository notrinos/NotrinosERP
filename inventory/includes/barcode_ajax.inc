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
 * Barcode AJAX Lookup Endpoint
 *
 * Handles barcode scan input and returns matching item/serial/batch data.
 * Called via AJAX from barcode_scanner.js or barcode label pages.
 *
 * Request: POST or GET with 'scan' parameter containing scanned barcode data
 * Response: JSON with match results
 *
 * Security: Requires valid session (SA_OPEN level — any authenticated user)
 */

$page_security = 'SA_OPEN';
$path_to_root = '../..';

// Minimal session bootstrap (no page rendering)
include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/includes/db/connect_db.inc');
include_once($path_to_root . '/inventory/includes/db/items_db.inc');
include_once($path_to_root . '/inventory/includes/db/serial_numbers_db.inc');
include_once($path_to_root . '/inventory/includes/db/stock_batches_db.inc');
include_once($path_to_root . '/inventory/includes/db/serial_batch_db.inc');
include_once($path_to_root . '/inventory/includes/gs1_standards.inc');

header('Content-Type: application/json; charset=utf-8');
header('Cache-Control: no-cache, no-store, must-revalidate');

// Get scanned data
$scan_data = '';
if (isset($_POST['scan'])) {
	$scan_data = trim($_POST['scan']);
} elseif (isset($_GET['scan'])) {
	$scan_data = trim($_GET['scan']);
}

// Get optional action
$action = '';
if (isset($_POST['action'])) {
	$action = trim($_POST['action']);
} elseif (isset($_GET['action'])) {
	$action = trim($_GET['action']);
}

if (empty($scan_data) && $action !== 'generate_image') {
	echo json_encode(array(
		'success' => false,
		'error'   => 'No scan data provided',
		'matches' => array(),
	));
	exit();
}

// Handle barcode image generation request
if ($action === 'generate_image') {
	include_once($path_to_root . '/inventory/includes/barcode_generator.inc');

	$code = isset($_GET['code']) ? trim($_GET['code']) : '';
	$type = isset($_GET['type']) ? trim($_GET['type']) : 'C128B';

	if (empty($code)) {
		echo json_encode(array('success' => false, 'error' => 'No code provided'));
		exit();
	}

	// Return as data URI
	$uri = get_barcode_data_uri($code, $type);
	if ($uri) {
		echo json_encode(array('success' => true, 'data_uri' => $uri));
	} else {
		echo json_encode(array('success' => false, 'error' => 'Barcode generation failed'));
	}
	exit();
}

// Main scan lookup
$response = barcode_lookup($scan_data);
echo json_encode($response);
exit();

/**
 * Perform a barcode lookup — identify what was scanned and return matches.
 *
 * Search order:
 * 1. Try GS1 parsing (if data looks like GS1)
 * 2. Try serial number exact match
 * 3. Try batch number exact match
 * 4. Try item stock_id exact match
 * 5. Try item barcode field match
 * 6. Try EAN/UPC match
 *
 * @param string $scan_data Raw scanned data
 * @return array Response array with 'success', 'type', 'matches', 'gs1_data'
 */
function barcode_lookup($scan_data) {
	$result = array(
		'success' => false,
		'type'    => 'unknown',
		'matches' => array(),
		'scan'    => $scan_data,
	);

	// Step 1: Try GS1 parsing
	$gs1 = identify_barcode_data($scan_data);

	if ($gs1['type'] === 'gs1') {
		$parsed = $gs1['data'];
		$result['gs1_data'] = $parsed;
		$result['type'] = 'gs1';

		// Look up by GTIN (AI 01)
		if (isset($parsed['01'])) {
			$gtin = $parsed['01'];
			$item_match = lookup_item_by_gtin($gtin);
			if ($item_match) {
				$result['matches'][] = $item_match;
			}
		}

		// Look up serial number (AI 21)
		if (isset($parsed['21'])) {
			$serial_match = lookup_serial_number($parsed['21']);
			if ($serial_match) {
				$result['matches'][] = $serial_match;
			}
		}

		// Look up batch/lot (AI 10)
		if (isset($parsed['10'])) {
			$batch_match = lookup_batch_number($parsed['10']);
			if ($batch_match) {
				$result['matches'][] = $batch_match;
			}
		}

		if (!empty($result['matches'])) {
			$result['success'] = true;
			return $result;
		}
	}

	// Step 2: Try serial number exact match
	$serial_match = lookup_serial_number($scan_data);
	if ($serial_match) {
		$result['success'] = true;
		$result['type'] = 'serial';
		$result['matches'][] = $serial_match;
		return $result;
	}

	// Step 3: Try batch number exact match
	$batch_match = lookup_batch_number($scan_data);
	if ($batch_match) {
		$result['success'] = true;
		$result['type'] = 'batch';
		$result['matches'][] = $batch_match;
		return $result;
	}

	// Step 4: Try item stock_id exact match
	$item = get_item($scan_data);
	if ($item) {
		$result['success'] = true;
		$result['type'] = 'item';
		$result['matches'][] = format_item_match($item);
		return $result;
	}

	// Step 5: Try EAN identification
	if ($gs1['type'] === 'ean') {
		$result['type'] = 'ean';
		// Search items table for this EAN
		$item_match = lookup_item_by_gtin($scan_data);
		if ($item_match) {
			$result['success'] = true;
			$result['matches'][] = $item_match;
			return $result;
		}
	}

	// Step 6: Try partial/fuzzy match on item description
	$fuzzy_matches = lookup_items_by_description($scan_data);
	if (!empty($fuzzy_matches)) {
		$result['success'] = true;
		$result['type'] = 'item_search';
		$result['matches'] = $fuzzy_matches;
		return $result;
	}

	// No match found
	$result['error'] = 'No matching item, serial, or batch found for: ' . $scan_data;
	return $result;
}

/**
 * Look up a serial number by its code.
 *
 * @param string $serial_no Serial number string
 * @return array|false Match data or false
 */
function lookup_serial_number($serial_no) {
	$sql = "SELECT sn.*, sm.description, sm.units
			FROM " . TB_PREF . "serial_numbers sn
			LEFT JOIN " . TB_PREF . "stock_master sm ON sm.stock_id = sn.stock_id
			WHERE sn.serial_no = " . db_escape($serial_no) . "
			LIMIT 1";
	$res = db_query($sql, 'Could not look up serial number');
	$row = db_fetch_assoc($res);

	if (!$row) {
		return false;
	}

	return array(
		'match_type'  => 'serial',
		'serial_id'   => (int)$row['id'],
		'serial_no'   => $row['serial_no'],
		'stock_id'    => $row['stock_id'],
		'description' => $row['description'],
		'status'      => $row['status'],
		'loc_code'    => $row['loc_code'],
		'expiry_date' => $row['expiry_date'],
		'units'       => $row['units'],
	);
}

/**
 * Look up a batch/lot number by its code.
 *
 * @param string $batch_no Batch/Lot number string
 * @return array|false Match data or false
 */
function lookup_batch_number($batch_no) {
	$sql = "SELECT b.*, sm.description, sm.units
			FROM " . TB_PREF . "stock_batches b
			LEFT JOIN " . TB_PREF . "stock_master sm ON sm.stock_id = b.stock_id
			WHERE b.batch_no = " . db_escape($batch_no) . "
			LIMIT 1";
	$res = db_query($sql, 'Could not look up batch number');
	$row = db_fetch_assoc($res);

	if (!$row) {
		return false;
	}

	return array(
		'match_type'        => 'batch',
		'batch_id'          => (int)$row['id'],
		'batch_no'          => $row['batch_no'],
		'stock_id'          => $row['stock_id'],
		'description'       => $row['description'],
		'status'            => $row['status'],
		'expiry_date'       => $row['expiry_date'],
		'manufacturing_date'=> $row['manufacturing_date'],
		'initial_qty'       => (float)$row['initial_qty'],
		'units'             => $row['units'],
	);
}

/**
 * Look up an item by GTIN/EAN barcode.
 *
 * Searches foreign codes and stock_id padded to GTIN length.
 *
 * @param string $gtin GTIN/EAN barcode
 * @return array|false Match data or false
 */
function lookup_item_by_gtin($gtin) {
	// Try foreign item codes first (where foreign_code matches)
	$sql = "SELECT fi.stock_id, fi.foreign_code, sm.description, sm.units, sm.inactive
			FROM " . TB_PREF . "item_codes fi
			JOIN " . TB_PREF . "stock_master sm ON sm.stock_id = fi.stock_id
			WHERE fi.item_code = " . db_escape($gtin) . "
			AND sm.inactive = 0
			LIMIT 1";
	$res = db_query($sql, 'Could not look up GTIN');
	$row = db_fetch_assoc($res);

	if ($row) {
		return format_item_match_from_row($row);
	}

	// Try stock_id directly (for items where stock_id is the barcode)
	$item = get_item($gtin);
	if ($item) {
		return format_item_match($item);
	}

	// Try numeric padding
	$numeric = preg_replace('/[^0-9]/', '', $gtin);
	if ($numeric !== $gtin) {
		$sql = "SELECT fi.stock_id, fi.item_code as foreign_code, sm.description, sm.units, sm.inactive
				FROM " . TB_PREF . "item_codes fi
				JOIN " . TB_PREF . "stock_master sm ON sm.stock_id = fi.stock_id
				WHERE fi.item_code = " . db_escape($numeric) . "
				AND sm.inactive = 0
				LIMIT 1";
		$res = db_query($sql, 'Could not look up GTIN numeric');
		$row = db_fetch_assoc($res);

		if ($row) {
			return format_item_match_from_row($row);
		}
	}

	return false;
}

/**
 * Search items by description (partial match).
 *
 * @param string $search Search text
 * @return array Array of matching items (max 5)
 */
function lookup_items_by_description($search) {
	$matches = array();
	$sql = "SELECT stock_id, description, units, inactive
			FROM " . TB_PREF . "stock_master
			WHERE (description LIKE " . db_escape('%' . $search . '%') . "
				OR stock_id LIKE " . db_escape('%' . $search . '%') . ")
			AND inactive = 0
			ORDER BY stock_id
			LIMIT 5";
	$res = db_query($sql, 'Could not search items');

	while ($row = db_fetch_assoc($res)) {
		$matches[] = array(
			'match_type'  => 'item',
			'stock_id'    => $row['stock_id'],
			'description' => $row['description'],
			'units'       => $row['units'],
		);
	}

	return $matches;
}

/**
 * Format an item record as a match result.
 *
 * @param array $item Item record from get_item()
 * @return array Formatted match data
 */
function format_item_match($item) {
	return array(
		'match_type'  => 'item',
		'stock_id'    => $item['stock_id'],
		'description' => $item['description'],
		'units'       => $item['units'],
		'track_by'    => isset($item['track_by']) ? $item['track_by'] : 'none',
	);
}

/**
 * Format an item row from a GTIN lookup as a match result.
 *
 * @param array $row DB row
 * @return array Formatted match data
 */
function format_item_match_from_row($row) {
	return array(
		'match_type'  => 'item',
		'stock_id'    => $row['stock_id'],
		'description' => $row['description'],
		'units'       => $row['units'],
		'barcode'     => isset($row['foreign_code']) ? $row['foreign_code'] : '',
	);
}
