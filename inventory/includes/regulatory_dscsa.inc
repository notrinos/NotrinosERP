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
 * DSCSA (Drug Supply Chain Security Act) Compliance Functions.
 *
 * Implements:
 *   - Transaction Information (TI) generation per delivery/GRN
 *   - Transaction History (TH) / Transaction Statement (TS) chain
 *   - Product verification (saleable return verification)
 *   - Suspect/illegitimate product handling
 *   - GS1 DataMatrix barcode with NDC+serial+lot+expiry
 *
 * Tables: 0_dscsa_transactions, 0_serial_numbers, 0_stock_batches
 * Dependencies: regulatory_compliance_db.inc, gs1_standards.inc
 *
 * @package NotrinosERP
 * @subpackage Inventory/Regulatory/DSCSA
 */

// =====================================================================
// DSCSA TRANSACTION TYPES & STATUSES
// =====================================================================

/**
 * Get DSCSA transaction types.
 *
 * @return array Associative array of type_code => display_label
 */
function get_dscsa_transaction_types()
{
	return array(
		'sale'         => _('Sale'),
		'return'       => _('Return'),
		'suspect'      => _('Suspect Product'),
		'illegitimate' => _('Illegitimate Product'),
	);
}

/**
 * Get DSCSA verification statuses.
 *
 * @return array Associative array of status_code => display_label
 */
function get_dscsa_verification_statuses()
{
	return array(
		'pending'  => _('Pending'),
		'verified' => _('Verified'),
		'failed'   => _('Failed'),
		'suspect'  => _('Suspect'),
	);
}

/**
 * Get CSS color for a verification status.
 *
 * @param string $status Status code
 * @return string CSS color
 */
function get_dscsa_status_color($status)
{
	$colors = array(
		'pending'  => '#ffc107',
		'verified' => '#28a745',
		'failed'   => '#dc3545',
		'suspect'  => '#fd7e14',
	);
	return isset($colors[$status]) ? $colors[$status] : '#6c757d';
}

/**
 * Get DSCSA quarantine actions.
 *
 * @return array
 */
function get_dscsa_quarantine_actions()
{
	return array(
		'quarantine' => _('Quarantine'),
		'destroy'    => _('Destroy'),
		'return'     => _('Return to Supplier'),
	);
}

// =====================================================================
// TRANSACTION INFORMATION (TI) GENERATION
// =====================================================================

/**
 * Generate DSCSA Transaction Information for a delivery.
 *
 * Creates TI records for each DSCSA-regulated serialized/batch item in the delivery.
 * Each record captures: product identifier, transaction info, and prior transaction history.
 *
 * @param int    $delivery_id   Delivery transaction number (trans_no for ST_CUSTDELIVERY)
 * @param string $delivery_date SQL date of delivery
 * @param int    $customer_id   Customer debtor_no
 * @return array Array of created DSCSA transaction ids
 */
function generate_dscsa_transaction_info($delivery_id, $delivery_date, $customer_id)
{
	$created_ids = array();

	// Get company info for sender
	$company = get_company_prefs();
	$sender_name = $company['coy_name'];
	$sender_license = get_company_pref('dscsa_company_license');

	// Get customer info for receiver
	$customer = get_customer($customer_id);
	$receiver_name = $customer ? $customer['name'] : '';

	// Get delivery line items
	$sql = "SELECT dl.stock_id, dl.qty_dispatched, dl.standard_cost,
			sm.description, sm.long_description
		FROM " . TB_PREF . "debtor_trans_details dl
		INNER JOIN " . TB_PREF . "stock_master sm ON dl.stock_id = sm.stock_id
		WHERE dl.debtor_trans_no=" . (int)$delivery_id
		. " AND dl.debtor_trans_type=" . ST_CUSTDELIVERY
		. " AND dl.qty_dispatched != 0";

	$lines = db_query($sql, 'could not get delivery lines for DSCSA');

	while ($line = db_fetch($lines)) {
		$stock_id = $line['stock_id'];

		// Check if item is DSCSA-regulated
		if (!is_item_regulated($stock_id, REG_DSCSA))
			continue;

		$settings = get_item_regulatory_settings($stock_id, REG_DSCSA);
		$ndc = isset($settings['ndc']) ? $settings['ndc'] : '';

		// Get serials assigned to this delivery for this item
		$serials = _get_delivery_serials($delivery_id, $stock_id);

		if (!empty($serials)) {
			// Create one TI record per serial number (unit-level traceability)
			foreach ($serials as $serial) {
				$ti_id = _create_dscsa_transaction(
					'sale', $delivery_id, null, $stock_id,
					$serial['id'], $serial['batch_id'],
					$ndc, $serial['serial_no'],
					$serial['batch_no'], $serial['expiry_date'],
					$sender_name, $sender_license,
					$receiver_name, '', $delivery_date
				);
				if ($ti_id)
					$created_ids[] = $ti_id;
			}
		} else {
			// Batch-level traceability (no individual serials)
			$batches = _get_delivery_batches($delivery_id, $stock_id);
			foreach ($batches as $batch) {
				$ti_id = _create_dscsa_transaction(
					'sale', $delivery_id, null, $stock_id,
					null, $batch['id'],
					$ndc, null,
					$batch['batch_no'], $batch['expiry_date'],
					$sender_name, $sender_license,
					$receiver_name, '', $delivery_date
				);
				if ($ti_id)
					$created_ids[] = $ti_id;
			}
		}
	}

	return $created_ids;
}

/**
 * Generate DSCSA Transaction Information for a GRN (inbound receipt).
 *
 * @param int    $grn_id       GRN batch id
 * @param string $grn_date     SQL date of receipt
 * @param int    $supplier_id  Supplier id
 * @return array Array of created DSCSA transaction ids
 */
function generate_dscsa_grn_transaction_info($grn_id, $grn_date, $supplier_id)
{
	$created_ids = array();

	$company = get_company_prefs();
	$receiver_name = $company['coy_name'];
	$receiver_license = get_company_pref('dscsa_company_license');

	// Get supplier info
	$supplier = get_supplier($supplier_id);
	$sender_name = $supplier ? $supplier['supp_name'] : '';

	// Get GRN items
	$sql = "SELECT gi.item_code AS stock_id, gi.qty_recd,
			sm.description
		FROM " . TB_PREF . "grn_items gi
		INNER JOIN " . TB_PREF . "stock_master sm ON gi.item_code = sm.stock_id
		WHERE gi.grn_batch_id=" . (int)$grn_id
		. " AND gi.qty_recd > 0";

	$lines = db_query($sql, 'could not get GRN lines for DSCSA');

	while ($line = db_fetch($lines)) {
		$stock_id = $line['stock_id'];

		if (!is_item_regulated($stock_id, REG_DSCSA))
			continue;

		$settings = get_item_regulatory_settings($stock_id, REG_DSCSA);
		$ndc = isset($settings['ndc']) ? $settings['ndc'] : '';

		// Get serials from this GRN
		$serials = _get_grn_serials($grn_id, $stock_id);

		if (!empty($serials)) {
			foreach ($serials as $serial) {
				$ti_id = _create_dscsa_transaction(
					'sale', null, $grn_id, $stock_id,
					$serial['id'], $serial['batch_id'],
					$ndc, $serial['serial_no'],
					$serial['batch_no'], $serial['expiry_date'],
					$sender_name, '',
					$receiver_name, $receiver_license, $grn_date
				);
				if ($ti_id)
					$created_ids[] = $ti_id;
			}
		}
	}

	return $created_ids;
}

// =====================================================================
// PRODUCT VERIFICATION
// =====================================================================

/**
 * Verify a DSCSA product by its identifiers.
 *
 * Checks that the serial/lot/NDC/expiry combination is legitimate
 * and not previously flagged as suspect or illegitimate.
 *
 * @param string      $serial_no Serial number
 * @param string|null $ndc       National Drug Code
 * @param string|null $lot       Lot/batch number
 * @param string|null $expiry    Expiry date (SQL format)
 * @return array Verification result: 'status' (verified|failed|suspect), 'details' => array
 */
function verify_dscsa_product($serial_no, $ndc = null, $lot = null, $expiry = null)
{
	$result = array(
		'status'  => 'failed',
		'details' => array(),
		'serial_found'  => false,
		'ndc_match'     => null,
		'lot_match'     => null,
		'expiry_match'  => null,
		'suspect_flag'  => false,
	);

	// Find the serial number in our system
	$sql = "SELECT sn.*, sb.batch_no, sb.expiry_date AS batch_expiry
		FROM " . TB_PREF . "serial_numbers sn
		LEFT JOIN " . TB_PREF . "stock_batches sb ON sn.batch_id = sb.id
		WHERE sn.serial_no=" . db_escape($serial_no);
	$qr = db_query($sql, 'could not verify DSCSA product');
	$serial = db_fetch($qr);

	if (!$serial) {
		$result['details'][] = _('Serial number not found in system.');
		return $result;
	}

	$result['serial_found'] = true;
	$stock_id = $serial['stock_id'];

	// Check if already flagged as suspect/illegitimate
	$sql = "SELECT id, transaction_type, suspect_reason FROM " . TB_PREF . "dscsa_transactions
		WHERE serial_no=" . db_escape($serial_no)
		. " AND transaction_type IN ('suspect','illegitimate')
		ORDER BY id DESC LIMIT 1";
	$qr = db_query($sql, 'could not check suspect status');
	$suspect = db_fetch($qr);
	if ($suspect) {
		$result['status'] = 'suspect';
		$result['suspect_flag'] = true;
		$result['details'][] = sprintf(_('ALERT: This serial was previously reported as %s. Reason: %s'),
			$suspect['transaction_type'], $suspect['suspect_reason']);
		return $result;
	}

	// Verify NDC if provided
	if ($ndc !== null) {
		$settings = get_item_regulatory_settings($stock_id, REG_DSCSA);
		$expected_ndc = isset($settings['ndc']) ? $settings['ndc'] : '';
		if ($expected_ndc && $ndc !== $expected_ndc) {
			$result['ndc_match'] = false;
			$result['details'][] = _('NDC mismatch.');
		} else {
			$result['ndc_match'] = true;
		}
	}

	// Verify lot number
	if ($lot !== null && $serial['batch_id']) {
		$batch_no = $serial['batch_no'] ? $serial['batch_no'] : '';
		if ($lot === $batch_no) {
			$result['lot_match'] = true;
		} else {
			$result['lot_match'] = false;
			$result['details'][] = _('Lot number mismatch.');
		}
	}

	// Verify expiry
	if ($expiry !== null && $serial['batch_id']) {
		$batch_expiry = $serial['batch_expiry'] ? $serial['batch_expiry'] : '';
		if ($expiry === $batch_expiry) {
			$result['expiry_match'] = true;
		} else {
			$result['expiry_match'] = false;
			$result['details'][] = _('Expiry date mismatch.');
		}
	}

	// Check serial status
	if (in_array($serial['status'], array('scrapped', 'recalled'))) {
		$result['details'][] = sprintf(_('Serial is in %s status.'), $serial['status']);
		$result['status'] = 'failed';
		return $result;
	}

	// All checks passed if no failures
	if ($result['ndc_match'] !== false && $result['lot_match'] !== false && $result['expiry_match'] !== false) {
		$result['status'] = 'verified';
		$result['details'][] = _('Product verification passed.');
	}

	return $result;
}

// =====================================================================
// SUSPECT / ILLEGITIMATE PRODUCT HANDLING
// =====================================================================

/**
 * Report a product as suspect.
 *
 * Creates a DSCSA transaction record and optionally quarantines the serial/batch.
 *
 * @param int    $serial_id       Serial number id (or null for batch-level)
 * @param int    $batch_id        Batch id (or null for serial-level)
 * @param string $stock_id        Item stock_id
 * @param string $reason          Reason for suspect designation
 * @param string $action          quarantine|destroy|return
 * @param bool   $quarantine_item Whether to quarantine the serial/batch
 * @return int DSCSA transaction id
 */
function report_dscsa_suspect_product($serial_id, $batch_id, $stock_id, $reason, $action = 'quarantine', $quarantine_item = true)
{
	$serial_no = null;
	$lot_number = null;
	$expiry_date = null;

	if ($serial_id) {
		$sql = "SELECT sn.serial_no, sb.batch_no, sb.expiry_date
			FROM " . TB_PREF . "serial_numbers sn
			LEFT JOIN " . TB_PREF . "stock_batches sb ON sn.batch_id = sb.id
			WHERE sn.id=" . (int)$serial_id;
		$qr = db_query($sql, 'could not get serial for suspect report');
		$row = db_fetch($qr);
		if ($row) {
			$serial_no = $row['serial_no'];
			$lot_number = $row['batch_no'];
			$expiry_date = $row['expiry_date'];
		}
	} elseif ($batch_id) {
		$sql = "SELECT batch_no, expiry_date FROM " . TB_PREF . "stock_batches WHERE id=" . (int)$batch_id;
		$qr = db_query($sql, 'could not get batch for suspect report');
		$row = db_fetch($qr);
		if ($row) {
			$lot_number = $row['batch_no'];
			$expiry_date = $row['expiry_date'];
		}
	}

	$settings = get_item_regulatory_settings($stock_id, REG_DSCSA);
	$ndc = isset($settings['ndc']) ? $settings['ndc'] : '';

	$id = _create_dscsa_transaction(
		'suspect', null, null, $stock_id,
		$serial_id, $batch_id,
		$ndc, $serial_no, $lot_number, $expiry_date,
		null, null, null, null, date('Y-m-d')
	);

	// Update the suspect fields
	$sql = "UPDATE " . TB_PREF . "dscsa_transactions SET
		suspect_reason=" . db_escape($reason) . ",
		suspect_reported_date=NOW(),
		quarantine_action=" . db_escape($action) . ",
		verification_status='suspect'
		WHERE id=" . (int)$id;
	db_query($sql, 'could not update suspect report');

	// Quarantine the serial/batch
	if ($quarantine_item) {
		if ($serial_id) {
			$sql = "UPDATE " . TB_PREF . "serial_numbers SET status='quarantine'
				WHERE id=" . (int)$serial_id;
			db_query($sql, 'could not quarantine serial');

			// Log movement
			if (function_exists('add_serial_movement')) {
				add_serial_movement($serial_id, 'suspect_quarantine', null, null, null,
					'DSCSA suspect: ' . $reason);
			}
		}
		if ($batch_id) {
			$sql = "UPDATE " . TB_PREF . "stock_batches SET status='quarantine'
				WHERE id=" . (int)$batch_id;
			db_query($sql, 'could not quarantine batch');
		}
	}

	return $id;
}

// =====================================================================
// TRANSACTION HISTORY / STATEMENT
// =====================================================================

/**
 * Build the DSCSA Transaction History for a serial number.
 *
 * Returns the complete chain of ownership from manufacturing/import to current holder.
 *
 * @param string $serial_no Serial number
 * @return array Array of transaction records in chronological order
 */
function get_dscsa_transaction_history($serial_no)
{
	$sql = "SELECT dt.*,
			sm.description AS item_name
		FROM " . TB_PREF . "dscsa_transactions dt
		LEFT JOIN " . TB_PREF . "stock_master sm ON dt.stock_id = sm.stock_id
		WHERE dt.serial_no=" . db_escape($serial_no)
		. " ORDER BY dt.transaction_date ASC, dt.id ASC";

	$result = db_query($sql, 'could not get DSCSA transaction history');
	$history = array();
	while ($row = db_fetch($result))
		$history[] = $row;
	return $history;
}

/**
 * Get DSCSA transaction records for a delivery.
 *
 * @param int $delivery_id Delivery trans_no
 * @return array Array of DSCSA transaction rows
 */
function get_dscsa_delivery_transactions($delivery_id)
{
	$sql = "SELECT dt.*, sm.description AS item_name
		FROM " . TB_PREF . "dscsa_transactions dt
		LEFT JOIN " . TB_PREF . "stock_master sm ON dt.stock_id = sm.stock_id
		WHERE dt.delivery_id=" . (int)$delivery_id
		. " ORDER BY dt.stock_id, dt.serial_no";

	$result = db_query($sql, 'could not get delivery DSCSA transactions');
	$rows = array();
	while ($row = db_fetch($result))
		$rows[] = $row;
	return $rows;
}

/**
 * Verify a DSCSA transaction record and update its status.
 *
 * @param int    $id     DSCSA transaction id
 * @param string $status New verification status (verified|failed|suspect)
 * @return void
 */
function update_dscsa_verification($id, $status)
{
	$sql = "UPDATE " . TB_PREF . "dscsa_transactions SET
		verification_status=" . db_escape($status) . ",
		verification_date=NOW(),
		verified_by=" . (int)$_SESSION['wa_current_user']->user
		. " WHERE id=" . (int)$id;
	db_query($sql, 'could not update DSCSA verification');
}

// =====================================================================
// GS1 DATAMATRIX FOR DSCSA
// =====================================================================

/**
 * Generate a DSCSA-compliant GS1 DataMatrix barcode string.
 *
 * Encodes: (01)GTIN (21)Serial (10)Lot (17)Expiry
 * The GTIN is derived from the NDC via NDC→GTIN conversion.
 *
 * @param string $ndc       National Drug Code (10 or 11 digits)
 * @param string $serial_no Serial number
 * @param string $lot       Lot/batch number
 * @param string $expiry    Expiry date (SQL format YYYY-MM-DD)
 * @return string GS1 parenthesized barcode string
 */
function generate_dscsa_barcode($ndc, $serial_no, $lot, $expiry)
{
	$gtin = ndc_to_gtin($ndc);
	$expiry_gs1 = '';
	if ($expiry) {
		$parts = explode('-', $expiry);
		if (count($parts) === 3) {
			$expiry_gs1 = substr($parts[0], 2) . $parts[1] . $parts[2]; // YYMMDD
		}
	}

	$ai_values = array(
		'01' => $gtin,
		'21' => $serial_no,
		'10' => $lot,
	);
	if ($expiry_gs1)
		$ai_values['17'] = $expiry_gs1;

	return encode_gs1_barcode($ai_values);
}

/**
 * Parse a DSCSA barcode and extract its components.
 *
 * @param string $raw_scan Raw barcode data
 * @return array Parsed components: ndc, gtin, serial, lot, expiry
 */
function parse_dscsa_barcode($raw_scan)
{
	$parsed = parse_gs1_barcode($raw_scan);

	$result = array(
		'gtin'      => isset($parsed['01']) ? $parsed['01'] : '',
		'serial'    => isset($parsed['21']) ? $parsed['21'] : '',
		'lot'       => isset($parsed['10']) ? $parsed['10'] : '',
		'expiry_raw' => isset($parsed['17']) ? $parsed['17'] : '',
		'expiry'    => isset($parsed['17_formatted']) ? $parsed['17_formatted'] : '',
		'ndc'       => '',
	);

	// Convert GTIN back to NDC
	if ($result['gtin'])
		$result['ndc'] = gtin_to_ndc($result['gtin']);

	return $result;
}

/**
 * Convert NDC (National Drug Code) to GTIN-14 format.
 *
 * FDA NDC is 10 digits in 3 formats: 4-4-2, 5-3-2, 5-4-1
 * GTIN-14 = indicator(0) + NDC-11 + check digit
 *
 * @param string $ndc NDC in any common format (dashes optional)
 * @return string 14-digit GTIN
 */
function ndc_to_gtin($ndc)
{
	// Remove dashes and spaces
	$ndc = preg_replace('/[^0-9]/', '', $ndc);

	// Pad to 11 digits (NDC-11 format)
	$ndc11 = str_pad($ndc, 11, '0', STR_PAD_LEFT);

	// GTIN-14: indicator "0" + "3" (pharmaceutical prefix) + NDC-11 + check
	$partial = '00' . $ndc11;  // 13 digits without check

	// Calculate GS1 check digit
	$check = _calculate_gs1_check_digit($partial);

	return $partial . $check;
}

/**
 * Convert GTIN-14 back to NDC.
 *
 * @param string $gtin 14-digit GTIN
 * @return string NDC (11 digits, no dashes)
 */
function gtin_to_ndc($gtin)
{
	// Strip indicator digit (first 2) and check digit (last 1)
	if (strlen($gtin) === 14) {
		return substr($gtin, 2, 11);
	}
	return $gtin;
}

/**
 * Calculate GS1 check digit for a numeric string.
 *
 * @param string $digits Numeric string (length 13 for GTIN-14)
 * @return string Single check digit
 */
function _calculate_gs1_check_digit($digits)
{
	$sum = 0;
	$len = strlen($digits);
	for ($i = 0; $i < $len; $i++) {
		$d = (int)$digits[$i];
		// Multiply odd positions (from right, 1-indexed) by 3, even by 1
		$sum += (($len - $i) % 2 === 0) ? $d * 3 : $d;
	}
	$check = (10 - ($sum % 10)) % 10;
	return (string)$check;
}

// =====================================================================
// DSCSA INQUIRY / LISTING
// =====================================================================

/**
 * Get DSCSA transactions with optional filters.
 *
 * @param array $filters Associative array of filters:
 *   'transaction_type', 'verification_status', 'stock_id', 'date_from', 'date_to',
 *   'serial_no', 'delivery_id', 'grn_id'
 * @return resource DB result set
 */
function get_dscsa_transactions($filters = array())
{
	$sql = "SELECT dt.*, sm.description AS item_name
		FROM " . TB_PREF . "dscsa_transactions dt
		LEFT JOIN " . TB_PREF . "stock_master sm ON dt.stock_id = sm.stock_id
		WHERE 1=1";

	if (!empty($filters['transaction_type']))
		$sql .= " AND dt.transaction_type=" . db_escape($filters['transaction_type']);
	if (!empty($filters['verification_status']))
		$sql .= " AND dt.verification_status=" . db_escape($filters['verification_status']);
	if (!empty($filters['stock_id']))
		$sql .= " AND dt.stock_id=" . db_escape($filters['stock_id']);
	if (!empty($filters['date_from']))
		$sql .= " AND dt.transaction_date>=" . db_escape($filters['date_from']);
	if (!empty($filters['date_to']))
		$sql .= " AND dt.transaction_date<=" . db_escape($filters['date_to']);
	if (!empty($filters['serial_no']))
		$sql .= " AND dt.serial_no LIKE " . db_escape('%' . $filters['serial_no'] . '%');
	if (isset($filters['delivery_id']) && $filters['delivery_id'] !== '')
		$sql .= " AND dt.delivery_id=" . (int)$filters['delivery_id'];
	if (isset($filters['grn_id']) && $filters['grn_id'] !== '')
		$sql .= " AND dt.grn_id=" . (int)$filters['grn_id'];

	$sql .= " ORDER BY dt.transaction_date DESC, dt.id DESC";
	return db_query($sql, 'could not get DSCSA transactions');
}

/**
 * Get a single DSCSA transaction by id.
 *
 * @param int $id Transaction id
 * @return array|null
 */
function get_dscsa_transaction($id)
{
	$sql = "SELECT dt.*, sm.description AS item_name
		FROM " . TB_PREF . "dscsa_transactions dt
		LEFT JOIN " . TB_PREF . "stock_master sm ON dt.stock_id = sm.stock_id
		WHERE dt.id=" . (int)$id;
	$result = db_query($sql, 'could not get DSCSA transaction');
	return db_fetch($result);
}

// =====================================================================
// PRIVATE HELPERS
// =====================================================================

/**
 * Create a DSCSA transaction record.
 *
 * @param string      $type           Transaction type
 * @param int|null    $delivery_id    Delivery id
 * @param int|null    $grn_id         GRN id
 * @param string      $stock_id       Item
 * @param int|null    $serial_id      Serial id
 * @param int|null    $batch_id       Batch id
 * @param string      $ndc            NDC
 * @param string|null $serial_no      Serial number
 * @param string|null $lot            Lot number
 * @param string|null $expiry         Expiry date
 * @param string|null $sender_name    Sender
 * @param string|null $sender_license Sender license
 * @param string|null $receiver_name  Receiver
 * @param string|null $receiver_license Receiver license
 * @param string      $transaction_date Date
 * @return int|false Inserted id
 */
function _create_dscsa_transaction($type, $delivery_id, $grn_id, $stock_id,
	$serial_id, $batch_id, $ndc, $serial_no, $lot, $expiry,
	$sender_name, $sender_license, $receiver_name, $receiver_license, $transaction_date)
{
	// Generate GTIN from NDC
	$gtin = $ndc ? ndc_to_gtin($ndc) : '';

	// Generate GS1 barcode string
	$gs1 = '';
	if ($ndc && $serial_no && $lot) {
		$gs1 = generate_dscsa_barcode($ndc, $serial_no, $lot, $expiry);
	}

	// Build prior transaction history JSON
	$history = array();
	if ($serial_no) {
		$prior = get_dscsa_transaction_history($serial_no);
		foreach ($prior as $p) {
			$history[] = array(
				'id'       => $p['id'],
				'type'     => $p['transaction_type'],
				'date'     => $p['transaction_date'],
				'sender'   => $p['sender_name'],
				'receiver' => $p['receiver_name'],
			);
		}
	}

	$sql = "INSERT INTO " . TB_PREF . "dscsa_transactions
		(transaction_type, delivery_id, grn_id, stock_id,
		 serial_id, batch_id, ndc, gtin, serial_no, lot_number, expiry_date,
		 sender_name, sender_license, receiver_name, receiver_license,
		 transaction_date, verification_status, gs1_barcode, transaction_history)
		VALUES ("
		. db_escape($type) . ", "
		. db_escape($delivery_id) . ", "
		. db_escape($grn_id) . ", "
		. db_escape($stock_id) . ", "
		. db_escape($serial_id) . ", "
		. db_escape($batch_id) . ", "
		. db_escape($ndc) . ", "
		. db_escape($gtin) . ", "
		. db_escape($serial_no) . ", "
		. db_escape($lot) . ", "
		. db_escape($expiry) . ", "
		. db_escape($sender_name) . ", "
		. db_escape($sender_license) . ", "
		. db_escape($receiver_name) . ", "
		. db_escape($receiver_license) . ", "
		. db_escape($transaction_date) . ", "
		. "'pending', "
		. db_escape($gs1) . ", "
		. db_escape(json_encode($history))
		. ")";

	db_query($sql, 'could not create DSCSA transaction');
	return db_insert_id();
}

/**
 * Get serials assigned to a delivery for a specific item.
 *
 * @param int    $delivery_id Delivery trans_no
 * @param string $stock_id    Item
 * @return array Array of serial rows with batch info
 */
function _get_delivery_serials($delivery_id, $stock_id)
{
	$sql = "SELECT sn.id, sn.serial_no, sn.batch_id, sb.batch_no, sb.expiry_date
		FROM " . TB_PREF . "serial_numbers sn
		LEFT JOIN " . TB_PREF . "stock_batches sb ON sn.batch_id = sb.id
		WHERE sn.delivery_id=" . (int)$delivery_id
		. " AND sn.stock_id=" . db_escape($stock_id);
	$result = db_query($sql, 'could not get delivery serials');
	$rows = array();
	while ($row = db_fetch($result))
		$rows[] = $row;
	return $rows;
}

/**
 * Get batches associated with a delivery for a specific item.
 *
 * @param int    $delivery_id Delivery trans_no
 * @param string $stock_id    Item
 * @return array Array of batch rows
 */
function _get_delivery_batches($delivery_id, $stock_id)
{
	$sql = "SELECT DISTINCT sb.id, sb.batch_no, sb.expiry_date
		FROM " . TB_PREF . "batch_movements bm
		INNER JOIN " . TB_PREF . "stock_batches sb ON bm.batch_id = sb.id
		WHERE bm.trans_type=" . ST_CUSTDELIVERY
		. " AND bm.trans_no=" . (int)$delivery_id
		. " AND bm.stock_id=" . db_escape($stock_id)
		. " AND bm.qty < 0";
	$result = db_query($sql, 'could not get delivery batches');
	$rows = array();
	while ($row = db_fetch($result))
		$rows[] = $row;
	return $rows;
}

/**
 * Get serials received in a GRN for a specific item.
 *
 * @param int    $grn_id   GRN batch id
 * @param string $stock_id Item
 * @return array Array of serial rows with batch info
 */
function _get_grn_serials($grn_id, $stock_id)
{
	$sql = "SELECT sn.id, sn.serial_no, sn.batch_id, sb.batch_no, sb.expiry_date
		FROM " . TB_PREF . "serial_numbers sn
		LEFT JOIN " . TB_PREF . "stock_batches sb ON sn.batch_id = sb.id
		WHERE sn.grn_id=" . (int)$grn_id
		. " AND sn.stock_id=" . db_escape($stock_id);
	$result = db_query($sql, 'could not get GRN serials');
	$rows = array();
	while ($row = db_fetch($result))
		$rows[] = $row;
	return $rows;
}
