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
 * UDI (Unique Device Identification) Compliance Functions.
 *
 * Implements:
 *   - UDI-DI (Device Identifier) management
 *   - UDI-PI (Production Identifier) encoding
 *   - GS1 / HIBCC / ICCBBA issuing agency support
 *   - FDA GUDID database submission readiness
 *   - Barcode label generation with UDI data
 *
 * Tables: 0_udi_registrations
 * Dependencies: regulatory_compliance_db.inc, gs1_standards.inc
 *
 * @package NotrinosERP
 * @subpackage Inventory/Regulatory/UDI
 */

// =====================================================================
// UDI CONSTANTS
// =====================================================================

/**
 * Get UDI issuing agencies.
 *
 * @return array Associative array of agency_code => display_label
 */
function get_udi_issuing_agencies()
{
	return array(
		'GS1'    => _('GS1 (GTIN-based)'),
		'HIBCC'  => _('HIBCC (Health Industry)'),
		'ICCBBA' => _('ICCBBA (Blood/Tissue/Cells)'),
	);
}

/**
 * Get MRI safety classifications.
 *
 * @return array
 */
function get_udi_mri_safety_options()
{
	return array(
		''              => _('Not Specified'),
		'MR Safe'       => _('MR Safe'),
		'MR Conditional' => _('MR Conditional'),
		'MR Unsafe'     => _('MR Unsafe'),
	);
}

// =====================================================================
// UDI REGISTRATION CRUD
// =====================================================================

/**
 * Add a UDI registration for a device item.
 *
 * @param array $data Associative array of UDI registration fields
 * @return int Inserted id
 */
function add_udi_registration($data)
{
	$sql = "INSERT INTO " . TB_PREF . "udi_registrations
		(stock_id, udi_di, udi_pi, issuing_agency,
		 device_description, brand_name, version_model, company_name,
		 mri_safety, device_sterile, single_use, implantable, rx_only, otc,
		 fda_listing_number, fda_premarket_number,
		 gtin, serial_id, batch_id, manufacturing_date, expiry_date,
		 notes, created_by, created_date, custom_data)
		VALUES ("
		. db_escape($data['stock_id']) . ", "
		. db_escape($data['udi_di']) . ", "
		. db_escape(isset($data['udi_pi']) ? $data['udi_pi'] : null) . ", "
		. db_escape(isset($data['issuing_agency']) ? $data['issuing_agency'] : 'GS1') . ", "
		. db_escape(isset($data['device_description']) ? $data['device_description'] : '') . ", "
		. db_escape(isset($data['brand_name']) ? $data['brand_name'] : '') . ", "
		. db_escape(isset($data['version_model']) ? $data['version_model'] : '') . ", "
		. db_escape(isset($data['company_name']) ? $data['company_name'] : '') . ", "
		. db_escape(isset($data['mri_safety']) ? $data['mri_safety'] : null) . ", "
		. (isset($data['device_sterile']) && $data['device_sterile'] ? 1 : 0) . ", "
		. (isset($data['single_use']) && $data['single_use'] ? 1 : 0) . ", "
		. (isset($data['implantable']) && $data['implantable'] ? 1 : 0) . ", "
		. (isset($data['rx_only']) && $data['rx_only'] ? 1 : 0) . ", "
		. (isset($data['otc']) && $data['otc'] ? 1 : 0) . ", "
		. db_escape(isset($data['fda_listing_number']) ? $data['fda_listing_number'] : null) . ", "
		. db_escape(isset($data['fda_premarket_number']) ? $data['fda_premarket_number'] : null) . ", "
		. db_escape(isset($data['gtin']) ? $data['gtin'] : null) . ", "
		. db_escape(isset($data['serial_id']) ? $data['serial_id'] : null) . ", "
		. db_escape(isset($data['batch_id']) ? $data['batch_id'] : null) . ", "
		. db_escape(isset($data['manufacturing_date']) ? $data['manufacturing_date'] : null) . ", "
		. db_escape(isset($data['expiry_date']) ? $data['expiry_date'] : null) . ", "
		. db_escape(isset($data['notes']) ? $data['notes'] : '') . ", "
		. (int)$_SESSION['wa_current_user']->user . ", "
		. "NOW(), "
		. db_escape(isset($data['custom_data']) ? json_encode($data['custom_data']) : null)
		. ")";

	db_query($sql, 'could not add UDI registration');
	return db_insert_id();
}

/**
 * Update a UDI registration.
 *
 * @param int   $id   Registration id
 * @param array $data Fields to update
 * @return void
 */
function update_udi_registration($id, $data)
{
	$updates = array();

	$fields = array('udi_di', 'udi_pi', 'issuing_agency', 'device_description',
		'brand_name', 'version_model', 'company_name', 'mri_safety',
		'fda_listing_number', 'fda_premarket_number', 'gtin',
		'serial_id', 'batch_id', 'manufacturing_date', 'expiry_date', 'notes');

	foreach ($fields as $f) {
		if (isset($data[$f]))
			$updates[] = "$f=" . db_escape($data[$f]);
	}

	$bool_fields = array('device_sterile', 'single_use', 'implantable', 'rx_only', 'otc', 'gudid_submitted');
	foreach ($bool_fields as $f) {
		if (isset($data[$f]))
			$updates[] = "$f=" . ($data[$f] ? 1 : 0);
	}

	if (isset($data['gudid_submitted']) && $data['gudid_submitted']) {
		$updates[] = "gudid_submission_date=NOW()";
	}

	if (empty($updates))
		return;

	$sql = "UPDATE " . TB_PREF . "udi_registrations SET "
		. implode(', ', $updates)
		. " WHERE id=" . (int)$id;
	db_query($sql, 'could not update UDI registration');
}

/**
 * Delete a UDI registration.
 *
 * @param int $id Registration id
 * @return void
 */
function delete_udi_registration($id)
{
	$sql = "DELETE FROM " . TB_PREF . "udi_registrations WHERE id=" . (int)$id;
	db_query($sql, 'could not delete UDI registration');
}

/**
 * Get a single UDI registration.
 *
 * @param int $id Registration id
 * @return array|null
 */
function get_udi_registration($id)
{
	$sql = "SELECT ur.*, sm.description AS item_name
		FROM " . TB_PREF . "udi_registrations ur
		LEFT JOIN " . TB_PREF . "stock_master sm ON ur.stock_id = sm.stock_id
		WHERE ur.id=" . (int)$id;
	$result = db_query($sql, 'could not get UDI registration');
	return db_fetch($result);
}

/**
 * Get UDI registrations with optional filters.
 *
 * @param array $filters Optional: stock_id, issuing_agency, udi_di, gudid_submitted
 * @return resource DB result set
 */
function get_udi_registrations($filters = array())
{
	$sql = "SELECT ur.*, sm.description AS item_name
		FROM " . TB_PREF . "udi_registrations ur
		LEFT JOIN " . TB_PREF . "stock_master sm ON ur.stock_id = sm.stock_id
		WHERE 1=1";

	if (!empty($filters['stock_id']))
		$sql .= " AND ur.stock_id=" . db_escape($filters['stock_id']);
	if (!empty($filters['issuing_agency']))
		$sql .= " AND ur.issuing_agency=" . db_escape($filters['issuing_agency']);
	if (!empty($filters['udi_di']))
		$sql .= " AND ur.udi_di LIKE " . db_escape('%' . $filters['udi_di'] . '%');
	if (isset($filters['gudid_submitted']) && $filters['gudid_submitted'] !== '')
		$sql .= " AND ur.gudid_submitted=" . ($filters['gudid_submitted'] ? 1 : 0);

	$sql .= " ORDER BY ur.stock_id, ur.udi_di";
	return db_query($sql, 'could not get UDI registrations');
}

// =====================================================================
// UDI ENCODING
// =====================================================================

/**
 * Generate UDI-PI (Production Identifier) from serial/batch/date data.
 *
 * The PI portion includes: serial number, lot/batch, manufacturing date, expiry date.
 *
 * @param string|null $serial_no        Serial number
 * @param string|null $lot_number       Lot/batch number
 * @param string|null $manufacturing_date Manufacturing date (SQL)
 * @param string|null $expiry_date      Expiry date (SQL)
 * @return string UDI-PI string
 */
function generate_udi_pi($serial_no = null, $lot_number = null, $manufacturing_date = null, $expiry_date = null)
{
	$parts = array();

	if ($serial_no)
		$parts[] = '(21)' . $serial_no;
	if ($lot_number)
		$parts[] = '(10)' . $lot_number;
	if ($manufacturing_date) {
		$d = explode('-', $manufacturing_date);
		if (count($d) === 3)
			$parts[] = '(11)' . substr($d[0], 2) . $d[1] . $d[2];
	}
	if ($expiry_date) {
		$d = explode('-', $expiry_date);
		if (count($d) === 3)
			$parts[] = '(17)' . substr($d[0], 2) . $d[1] . $d[2];
	}

	return implode('', $parts);
}

/**
 * Generate a full UDI barcode string (DI + PI).
 *
 * For GS1 issuing agency: (01)GTIN + PI elements
 * For HIBCC: +[HIBCC code] + PI elements
 *
 * @param string $udi_di              Device Identifier
 * @param string $issuing_agency      GS1|HIBCC|ICCBBA
 * @param string|null $serial_no      Serial
 * @param string|null $lot_number     Lot
 * @param string|null $manufacturing_date Mfg date
 * @param string|null $expiry_date    Expiry
 * @return string Full barcode string
 */
function generate_udi_barcode($udi_di, $issuing_agency, $serial_no = null, $lot_number = null,
	$manufacturing_date = null, $expiry_date = null)
{
	if ($issuing_agency === 'GS1') {
		// GS1 format: (01)GTIN + production identifiers
		$ai_values = array();

		// DI = GTIN (pad to 14 digits)
		$gtin = str_pad(preg_replace('/[^0-9]/', '', $udi_di), 14, '0', STR_PAD_LEFT);
		$ai_values['01'] = $gtin;

		if ($serial_no)
			$ai_values['21'] = $serial_no;
		if ($lot_number)
			$ai_values['10'] = $lot_number;
		if ($manufacturing_date) {
			$d = explode('-', $manufacturing_date);
			if (count($d) === 3)
				$ai_values['11'] = substr($d[0], 2) . $d[1] . $d[2];
		}
		if ($expiry_date) {
			$d = explode('-', $expiry_date);
			if (count($d) === 3)
				$ai_values['17'] = substr($d[0], 2) . $d[1] . $d[2];
		}

		return encode_gs1_barcode($ai_values);

	} elseif ($issuing_agency === 'HIBCC') {
		// HIBCC format: +[LIC][PCN]/[unit] + secondary data
		$result = '+' . $udi_di;
		if ($lot_number || $serial_no || $expiry_date) {
			$result .= '+';
			if ($lot_number)
				$result .= '$' . $lot_number;
			if ($serial_no)
				$result .= '+S' . $serial_no;
			if ($expiry_date) {
				$d = explode('-', $expiry_date);
				if (count($d) === 3)
					$result .= '$$' . substr($d[0], 2) . $d[1] . $d[2];
			}
		}
		return $result;

	} else {
		// ICCBBA: simplified format
		return '=' . $udi_di . (($serial_no || $lot_number) ? '/' . $serial_no . '/' . $lot_number : '');
	}
}

/**
 * Generate UDI label data ready for printing.
 *
 * @param int $registration_id UDI registration id
 * @return array|null Label data or null if not found
 */
function generate_udi_label_data($registration_id)
{
	$reg = get_udi_registration($registration_id);
	if (!$reg)
		return null;

	$serial_no = null;
	$lot_number = null;

	if ($reg['serial_id']) {
		$sql = "SELECT serial_no FROM " . TB_PREF . "serial_numbers WHERE id=" . (int)$reg['serial_id'];
		$qr = db_query($sql, 'could not get serial for UDI label');
		$sn = db_fetch($qr);
		if ($sn)
			$serial_no = $sn['serial_no'];
	}
	if ($reg['batch_id']) {
		$sql = "SELECT batch_no FROM " . TB_PREF . "stock_batches WHERE id=" . (int)$reg['batch_id'];
		$qr = db_query($sql, 'could not get batch for UDI label');
		$bn = db_fetch($qr);
		if ($bn)
			$lot_number = $bn['batch_no'];
	}

	$barcode = generate_udi_barcode(
		$reg['udi_di'], $reg['issuing_agency'],
		$serial_no, $lot_number,
		$reg['manufacturing_date'], $reg['expiry_date']
	);

	return array(
		'registration'   => $reg,
		'barcode_string' => $barcode,
		'udi_di'         => $reg['udi_di'],
		'udi_pi'         => generate_udi_pi($serial_no, $lot_number, $reg['manufacturing_date'], $reg['expiry_date']),
		'serial_no'      => $serial_no,
		'lot_number'     => $lot_number,
		'brand_name'     => $reg['brand_name'],
		'device_description' => $reg['device_description'],
		'company_name'   => $reg['company_name'],
		'expiry_date'    => $reg['expiry_date'],
		'sterile'        => $reg['device_sterile'],
		'single_use'     => $reg['single_use'],
		'rx_only'        => $reg['rx_only'],
	);
}

/**
 * Get UDI compliance statistics.
 *
 * @return array
 */
function get_udi_compliance_stats()
{
	$stats = array(
		'total_registrations' => 0,
		'by_agency'           => array(),
		'gudid_submitted'     => 0,
		'gudid_pending'       => 0,
		'implantable_count'   => 0,
	);

	$sql = "SELECT issuing_agency, COUNT(*) AS cnt FROM " . TB_PREF . "udi_registrations
		GROUP BY issuing_agency";
	$result = db_query($sql, 'could not get UDI stats');
	while ($row = db_fetch($result)) {
		$stats['by_agency'][$row['issuing_agency']] = (int)$row['cnt'];
		$stats['total_registrations'] += (int)$row['cnt'];
	}

	$sql = "SELECT
		SUM(CASE WHEN gudid_submitted=1 THEN 1 ELSE 0 END) AS submitted,
		SUM(CASE WHEN gudid_submitted=0 THEN 1 ELSE 0 END) AS pending,
		SUM(CASE WHEN implantable=1 THEN 1 ELSE 0 END) AS implantable
		FROM " . TB_PREF . "udi_registrations";
	$result = db_query($sql, 'could not get UDI summary');
	$row = db_fetch($result);
	if ($row) {
		$stats['gudid_submitted'] = (int)$row['submitted'];
		$stats['gudid_pending'] = (int)$row['pending'];
		$stats['implantable_count'] = (int)$row['implantable'];
	}

	return $stats;
}
