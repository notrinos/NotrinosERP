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
 * Serial Number CRUD, queries, bulk operations, status management, and movement logging.
 *
 * Tables: 0_serial_numbers, 0_serial_movements
 * Dependencies: serial_batch_db.inc (for generate_serial_number())
 */

/**
 * Valid serial number statuses.
 *
 * @return array Associative array of status_code => display_label
 */
function get_serial_statuses() {
	return array(
		'available'     => _('Available'),
		'reserved'      => _('Reserved'),
		'delivered'     => _('Delivered'),
		'in_transit'    => _('In Transit'),
		'in_production' => _('In Production'),
		'in_repair'     => _('In Repair'),
		'quarantine'    => _('Quarantine'),
		'recalled'      => _('Recalled'),
		'scrapped'      => _('Scrapped'),
		'returned'      => _('Returned'),
		'expired'       => _('Expired'),
	);
}

/**
 * Get a human-readable label for a serial status.
 *
 * @param string $status Status code
 * @return string Translated label
 */
function get_serial_status_label($status) {
	$statuses = get_serial_statuses();
	return isset($statuses[$status]) ? $statuses[$status] : $status;
}

/**
 * Get a CSS class/color hint for a serial status.
 *
 * @param string $status Status code
 * @return string CSS color string
 */
function get_serial_status_color($status) {
	$colors = array(
		'available'     => '#28a745',
		'reserved'      => '#007bff',
		'delivered'     => '#6c757d',
		'in_transit'    => '#17a2b8',
		'in_production' => '#ffc107',
		'in_repair'     => '#fd7e14',
		'quarantine'    => '#dc3545',
		'recalled'      => '#dc3545',
		'scrapped'      => '#343a40',
		'returned'      => '#6610f2',
		'expired'       => '#6c757d',
	);
	return isset($colors[$status]) ? $colors[$status] : '#6c757d';
}

//----------------------------------------------------------------------
// CRUD Operations
//----------------------------------------------------------------------

/**
 * Add a single serial number record.
 *
 * @param string $serial_no       The serial number string
 * @param string $stock_id        Item code (FK to stock_master)
 * @param string $status          Initial status (default 'available')
 * @param string $loc_code        Warehouse location code
 * @param int    $wh_loc_id       Bin location id (FK to wh_locations)
 * @param int    $batch_id        Batch id (FK to stock_batches)
 * @param int    $supplier_id     Supplier who provided the serial
 * @param string $purchase_date   Purchase date
 * @param string $manufacturing_date Manufacturing date
 * @param string $expiry_date     Expiry date
 * @param string $warranty_start  Warranty start date
 * @param string $warranty_end    Warranty end date
 * @param string $warranty_type   Warranty type
 * @param float  $purchase_cost   Cost for this unit
 * @param int    $grn_id          GRN that received this serial
 * @param string $notes           Free text notes
 * @return int   The inserted serial id
 */
function add_serial_number($serial_no, $stock_id, $status = 'available', $loc_code = null,
	$wh_loc_id = null, $batch_id = null, $supplier_id = null, $purchase_date = null,
	$manufacturing_date = null, $expiry_date = null, $warranty_start = null,
	$warranty_end = null, $warranty_type = null, $purchase_cost = 0, $grn_id = null,
	$notes = null)
{
	$sql = "INSERT INTO " . TB_PREF . "serial_numbers (
		serial_no, stock_id, status, loc_code, wh_loc_id, batch_id,
		supplier_id, purchase_date, manufacturing_date, expiry_date,
		warranty_start, warranty_end, warranty_type, purchase_cost,
		grn_id, notes, created_by, created_at
	) VALUES (
		" . db_escape($serial_no) . ",
		" . db_escape($stock_id) . ",
		" . db_escape($status) . ",
		" . db_escape($loc_code) . ",
		" . ($wh_loc_id !== null ? (int)$wh_loc_id : 'NULL') . ",
		" . ($batch_id !== null ? (int)$batch_id : 'NULL') . ",
		" . ($supplier_id !== null ? (int)$supplier_id : 'NULL') . ",
		" . ($purchase_date ? db_escape(date2sql($purchase_date)) : 'NULL') . ",
		" . ($manufacturing_date ? db_escape(date2sql($manufacturing_date)) : 'NULL') . ",
		" . ($expiry_date ? db_escape(date2sql($expiry_date)) : 'NULL') . ",
		" . ($warranty_start ? db_escape(date2sql($warranty_start)) : 'NULL') . ",
		" . ($warranty_end ? db_escape(date2sql($warranty_end)) : 'NULL') . ",
		" . db_escape($warranty_type) . ",
		" . (float)$purchase_cost . ",
		" . ($grn_id !== null ? (int)$grn_id : 'NULL') . ",
		" . db_escape($notes) . ",
		" . (int)$_SESSION['wa_current_user']->user . ",
		NOW()
	)";
	db_query($sql, 'could not add serial number');
	return db_insert_id();
}

/**
 * Update an existing serial number record.
 *
 * @param int    $id              Serial record id
 * @param string $serial_no       Serial number string
 * @param string $status          Status
 * @param string $loc_code        Location code
 * @param int    $wh_loc_id       Bin location id
 * @param int    $batch_id        Batch id
 * @param int    $supplier_id     Supplier id
 * @param int    $customer_id     Customer id
 * @param string $purchase_date   Purchase date
 * @param string $delivery_date   Delivery date
 * @param string $manufacturing_date Manufacturing date
 * @param string $expiry_date     Expiry date
 * @param string $warranty_start  Warranty start
 * @param string $warranty_end    Warranty end
 * @param string $warranty_type   Warranty type
 * @param float  $purchase_cost   Purchase cost
 * @param int    $grn_id          GRN id
 * @param int    $delivery_id     Delivery note id
 * @param int    $work_order_id   Work order id
 * @param string $notes           Notes
 */
function update_serial_number($id, $serial_no, $status = null, $loc_code = null,
	$wh_loc_id = null, $batch_id = null, $supplier_id = null, $customer_id = null,
	$purchase_date = null, $delivery_date = null, $manufacturing_date = null,
	$expiry_date = null, $warranty_start = null, $warranty_end = null,
	$warranty_type = null, $purchase_cost = null, $grn_id = null,
	$delivery_id = null, $work_order_id = null, $notes = null)
{
	$sets = array();
	$sets[] = "serial_no=" . db_escape($serial_no);

	if ($status !== null)
		$sets[] = "status=" . db_escape($status);
	if ($loc_code !== null)
		$sets[] = "loc_code=" . db_escape($loc_code);

	// Nullable integer fields
	$sets[] = "wh_loc_id=" . ($wh_loc_id !== null && $wh_loc_id !== '' ? (int)$wh_loc_id : 'NULL');
	$sets[] = "batch_id=" . ($batch_id !== null && $batch_id !== '' ? (int)$batch_id : 'NULL');
	$sets[] = "supplier_id=" . ($supplier_id !== null && $supplier_id !== '' ? (int)$supplier_id : 'NULL');
	$sets[] = "customer_id=" . ($customer_id !== null && $customer_id !== '' ? (int)$customer_id : 'NULL');
	$sets[] = "grn_id=" . ($grn_id !== null && $grn_id !== '' ? (int)$grn_id : 'NULL');
	$sets[] = "delivery_id=" . ($delivery_id !== null && $delivery_id !== '' ? (int)$delivery_id : 'NULL');
	$sets[] = "work_order_id=" . ($work_order_id !== null && $work_order_id !== '' ? (int)$work_order_id : 'NULL');

	// Date fields
	$sets[] = "purchase_date=" . ($purchase_date ? db_escape(date2sql($purchase_date)) : 'NULL');
	$sets[] = "delivery_date=" . ($delivery_date ? db_escape(date2sql($delivery_date)) : 'NULL');
	$sets[] = "manufacturing_date=" . ($manufacturing_date ? db_escape(date2sql($manufacturing_date)) : 'NULL');
	$sets[] = "expiry_date=" . ($expiry_date ? db_escape(date2sql($expiry_date)) : 'NULL');
	$sets[] = "warranty_start=" . ($warranty_start ? db_escape(date2sql($warranty_start)) : 'NULL');
	$sets[] = "warranty_end=" . ($warranty_end ? db_escape(date2sql($warranty_end)) : 'NULL');

	// Other fields
	$sets[] = "warranty_type=" . db_escape($warranty_type);
	if ($purchase_cost !== null)
		$sets[] = "purchase_cost=" . (float)$purchase_cost;
	$sets[] = "notes=" . db_escape($notes);

	$sql = "UPDATE " . TB_PREF . "serial_numbers SET "
		. implode(', ', $sets)
		. " WHERE id=" . (int)$id;
	db_query($sql, 'could not update serial number');
}

/**
 * Get a single serial number record by id.
 *
 * @param int $id Serial record id
 * @return array|false Row array or false
 */
function get_serial_number($id) {
	$sql = "SELECT sn.*, sm.description AS item_description, sm.units AS item_units,
			COALESCE(sup.supp_name, '') AS supplier_name,
			COALESCE(cust.name, '') AS customer_name,
			COALESCE(loc.location_name, '') AS location_name
		FROM " . TB_PREF . "serial_numbers sn
		LEFT JOIN " . TB_PREF . "stock_master sm ON sn.stock_id = sm.stock_id
		LEFT JOIN " . TB_PREF . "suppliers sup ON sn.supplier_id = sup.supplier_id
		LEFT JOIN " . TB_PREF . "debtors_master cust ON sn.customer_id = cust.debtor_no
		LEFT JOIN " . TB_PREF . "locations loc ON sn.loc_code = loc.loc_code
		WHERE sn.id=" . (int)$id;
	$result = db_query($sql, 'could not get serial number');
	return db_fetch($result);
}

/**
 * Get a serial number by its serial_no string and stock_id.
 *
 * @param string $serial_no The serial string
 * @param string $stock_id  The item code
 * @return array|false Row array or false
 */
function get_serial_number_by_code($serial_no, $stock_id=null) {
	$sql = "SELECT * FROM " . TB_PREF . "serial_numbers
		WHERE serial_no=" . db_escape($serial_no);
	if ($stock_id !== null && $stock_id !== '')
		$sql .= " AND stock_id=" . db_escape($stock_id);
	$result = db_query($sql, 'could not get serial by code');
	return db_fetch($result);
}

/**
 * Search serial numbers with flexible filters.
 *
 * @param string $stock_id     Filter by item ('' for all)
 * @param string $status       Filter by status ('' for all)
 * @param string $loc_code     Filter by location ('' for all)
 * @param string $serial_like  Search serial_no with LIKE ('' for all)
 * @param bool   $show_inactive Include inactive records
 * @param string $customer_id  Filter by customer
 * @param string $warranty_status 'active'|'expired'|'' for all
 * @param int    $limit        Max rows (0 = unlimited)
 * @param int    $offset       Pagination offset
 * @return resource DB result
 */
function get_serial_numbers($stock_id = '', $status = '', $loc_code = '', $serial_like = '',
	$show_inactive = false, $customer_id = '', $warranty_status = '', $limit = 0, $offset = 0)
{
	$where = array();

	if ($stock_id !== '' && $stock_id !== null)
		$where[] = "sn.stock_id=" . db_escape($stock_id);
	if ($status !== '' && $status !== null)
		$where[] = "sn.status=" . db_escape($status);
	if ($loc_code !== '' && $loc_code !== null)
		$where[] = "sn.loc_code=" . db_escape($loc_code);
	if ($serial_like !== '' && $serial_like !== null)
		$where[] = "sn.serial_no LIKE " . db_escape('%' . $serial_like . '%');
	if (!$show_inactive)
		$where[] = "sn.inactive=0";
	if ($customer_id !== '' && $customer_id !== null)
		$where[] = "sn.customer_id=" . (int)$customer_id;
	if ($warranty_status === 'active')
		$where[] = "sn.warranty_end IS NOT NULL AND sn.warranty_end >= CURDATE()";
	elseif ($warranty_status === 'expired')
		$where[] = "sn.warranty_end IS NOT NULL AND sn.warranty_end < CURDATE()";

	$sql = "SELECT sn.*, sm.description AS item_description,
			COALESCE(loc.location_name, '') AS location_name,
			COALESCE(sup.supp_name, '') AS supplier_name,
			COALESCE(cust.name, '') AS customer_name
		FROM " . TB_PREF . "serial_numbers sn
		LEFT JOIN " . TB_PREF . "stock_master sm ON sn.stock_id = sm.stock_id
		LEFT JOIN " . TB_PREF . "locations loc ON sn.loc_code = loc.loc_code
		LEFT JOIN " . TB_PREF . "suppliers sup ON sn.supplier_id = sup.supplier_id
		LEFT JOIN " . TB_PREF . "debtors_master cust ON sn.customer_id = cust.debtor_no";

	if (count($where) > 0)
		$sql .= " WHERE " . implode(' AND ', $where);

	$sql .= " ORDER BY sn.created_at DESC";

	if ($limit > 0)
		$sql .= " LIMIT " . (int)$offset . ", " . (int)$limit;

	return db_query($sql, 'could not get serial numbers');
}

/**
 * Count serial numbers matching filters (for pagination).
 *
 * @param string $stock_id     Filter by item
 * @param string $status       Filter by status
 * @param string $loc_code     Filter by location
 * @param string $serial_like  Search term
 * @param bool   $show_inactive Include inactive
 * @return int Total count
 */
function count_serial_numbers($stock_id = '', $status = '', $loc_code = '', $serial_like = '',
	$show_inactive = false)
{
	$where = array();

	if ($stock_id !== '' && $stock_id !== null)
		$where[] = "stock_id=" . db_escape($stock_id);
	if ($status !== '' && $status !== null)
		$where[] = "status=" . db_escape($status);
	if ($loc_code !== '' && $loc_code !== null)
		$where[] = "loc_code=" . db_escape($loc_code);
	if ($serial_like !== '' && $serial_like !== null)
		$where[] = "serial_no LIKE " . db_escape('%' . $serial_like . '%');
	if (!$show_inactive)
		$where[] = "inactive=0";

	$sql = "SELECT COUNT(*) FROM " . TB_PREF . "serial_numbers";
	if (count($where) > 0)
		$sql .= " WHERE " . implode(' AND ', $where);

	$result = db_query($sql, 'could not count serial numbers');
	$row = db_fetch_row($result);
	return (int)$row[0];
}

/**
 * Check if a serial number string already exists for an item.
 *
 * @param string $serial_no Serial string
 * @param string $stock_id  Item code
 * @param int    $exclude_id Exclude this record id (for updates)
 * @return bool True if duplicate exists
 */
function is_serial_number_duplicate($serial_no, $stock_id, $exclude_id = 0) {
	$sql = "SELECT COUNT(*) FROM " . TB_PREF . "serial_numbers
		WHERE serial_no=" . db_escape($serial_no) . " AND stock_id=" . db_escape($stock_id);
	if ($exclude_id > 0)
		$sql .= " AND id<>" . (int)$exclude_id;
	$result = db_query($sql, 'could not check serial duplicate');
	$row = db_fetch_row($result);
	return $row[0] > 0;
}

/**
 * Check if a serial number can be deleted (no movements exist).
 *
 * @param int $id Serial record id
 * @return bool|string True if deletable, error message string if not
 */
function can_delete_serial_number($id) {
	$sql = "SELECT COUNT(*) FROM " . TB_PREF . "serial_movements WHERE serial_id=" . (int)$id;
	$result = db_query($sql, 'could not check serial movements');
	$row = db_fetch_row($result);
	if ($row[0] > 0)
		return _('Cannot delete this serial number because it has movement history.');

	return true;
}

/**
 * Delete a serial number record.
 *
 * @param int $id Serial record id
 */
function delete_serial_number($id) {
	$sql = "DELETE FROM " . TB_PREF . "serial_numbers WHERE id=" . (int)$id;
	db_query($sql, 'could not delete serial number');
}

//----------------------------------------------------------------------
// Bulk operations
//----------------------------------------------------------------------

/**
 * Bulk-create serial numbers for an item using auto-generation.
 *
 * @param string $stock_id  Item code
 * @param int    $count     Number of serials to generate
 * @param string $status    Initial status
 * @param string $loc_code  Location code
 * @param int    $wh_loc_id Bin location id
 * @param int    $supplier_id Supplier id
 * @param string $purchase_date Purchase date
 * @param string $notes     Notes for all generated serials
 * @return array Array of generated serial_no strings
 */
function bulk_create_serial_numbers($stock_id, $count, $status = 'available', $loc_code = null,
	$wh_loc_id = null, $supplier_id = null, $purchase_date = null, $notes = null)
{
	$generated = array();

	begin_transaction();

	for ($i = 0; $i < $count; $i++) {
		$serial_no = generate_serial_number($stock_id);

		// Safety: ensure no duplicate (regenerate if collision)
		$attempts = 0;
		while (is_serial_number_duplicate($serial_no, $stock_id) && $attempts < 10) {
			$serial_no = generate_serial_number($stock_id);
			$attempts++;
		}

		if (is_serial_number_duplicate($serial_no, $stock_id)) {
			// If still duplicate after 10 attempts, append random suffix
			$serial_no .= '-' . strtoupper(substr(md5(uniqid('', true)), 0, 4));
		}

		add_serial_number($serial_no, $stock_id, $status, $loc_code, $wh_loc_id,
			null, $supplier_id, $purchase_date, null, null, null, null, null, 0, null, $notes);

		$generated[] = $serial_no;
	}

	commit_transaction();
	return $generated;
}

/**
 * Import serial numbers from a CSV string (one serial per line, or comma-separated).
 *
 * @param string $stock_id     Item code
 * @param string $csv_text     CSV text: serial numbers separated by newline or comma
 * @param string $status       Initial status
 * @param string $loc_code     Location code
 * @param int    $supplier_id  Supplier id
 * @return array ['imported' => count, 'duplicates' => array of skipped serials, 'errors' => array]
 */
function import_serial_numbers($stock_id, $csv_text, $status = 'available', $loc_code = null,
	$supplier_id = null)
{
	$result = array('imported' => 0, 'duplicates' => array(), 'errors' => array());

	// Split by newlines and commas
	$lines = preg_split('/[\r\n,]+/', $csv_text);

	begin_transaction();

	foreach ($lines as $line) {
		$serial_no = trim($line);
		if ($serial_no === '')
			continue;

		// Validate length
		if (strlen($serial_no) > 100) {
			$result['errors'][] = sprintf(_('Serial "%s" exceeds maximum 100 characters.'), substr($serial_no, 0, 20) . '...');
			continue;
		}

		// Check duplicate
		if (is_serial_number_duplicate($serial_no, $stock_id)) {
			$result['duplicates'][] = $serial_no;
			continue;
		}

		add_serial_number($serial_no, $stock_id, $status, $loc_code, null,
			null, $supplier_id, null, null, null, null, null, null, 0, null, null);
		$result['imported']++;
	}

	commit_transaction();
	return $result;
}

//----------------------------------------------------------------------
// Status management
//----------------------------------------------------------------------

/**
 * Update the status of a serial number and log a movement.
 *
 * @param int    $serial_id    Serial record id
 * @param string $new_status   New status
 * @param int    $trans_type   Transaction type (ST_ constant)
 * @param int    $trans_no     Transaction number
 * @param string $from_loc     Source location code
 * @param string $to_loc       Destination location code
 * @param string $tran_date    Transaction date
 * @param string $reference    Document reference
 * @param string $notes        Movement notes
 * @param int    $stock_move_id FK to stock_moves.trans_id
 */
function update_serial_status($serial_id, $new_status, $trans_type = 0, $trans_no = 0,
	$from_loc = null, $to_loc = null, $tran_date = null, $reference = '',
	$notes = '', $stock_move_id = null)
{
	// Get current status for movement logging
	$serial = get_serial_number($serial_id);
	if (!$serial)
		return;

	$old_status = $serial['status'];

	// Update serial status and location
	$sql = "UPDATE " . TB_PREF . "serial_numbers SET status=" . db_escape($new_status);
	if ($to_loc !== null)
		$sql .= ", loc_code=" . db_escape($to_loc);
	$sql .= " WHERE id=" . (int)$serial_id;
	db_query($sql, 'could not update serial status');

	// Log the movement
	if ($tran_date === null)
		$tran_date = Today();

	add_serial_movement($serial_id, $trans_type, $trans_no, $from_loc, $to_loc,
		$old_status, $new_status, $tran_date, $reference, $notes, $stock_move_id);
}

//----------------------------------------------------------------------
// Movement logging
//----------------------------------------------------------------------

/**
 * Add a serial movement record.
 *
 * @param int    $serial_id    Serial record id
 * @param int    $trans_type   Transaction type
 * @param int    $trans_no     Transaction number
 * @param string $from_loc     Source location
 * @param string $to_loc       Destination location
 * @param string $from_status  Previous status
 * @param string $to_status    New status
 * @param string $tran_date    Movement date
 * @param string $reference    Reference string
 * @param string $notes        Notes
 * @param int    $stock_move_id FK to stock_moves.trans_id
 * @return int Inserted movement id
 */
function add_serial_movement($serial_id, $trans_type, $trans_no, $from_loc, $to_loc,
	$from_status, $to_status, $tran_date, $reference = '', $notes = '', $stock_move_id = null)
{
	$sql = "INSERT INTO " . TB_PREF . "serial_movements (
		serial_id, trans_type, trans_no, from_loc, to_loc,
		from_status, to_status, tran_date, reference, notes,
		stock_move_id, created_by, created_at
	) VALUES (
		" . (int)$serial_id . ",
		" . (int)$trans_type . ",
		" . (int)$trans_no . ",
		" . db_escape($from_loc) . ",
		" . db_escape($to_loc) . ",
		" . db_escape($from_status) . ",
		" . db_escape($to_status) . ",
		" . db_escape(date2sql($tran_date)) . ",
		" . db_escape($reference) . ",
		" . db_escape($notes) . ",
		" . ($stock_move_id !== null ? (int)$stock_move_id : 'NULL') . ",
		" . (int)$_SESSION['wa_current_user']->user . ",
		NOW()
	)";
	db_query($sql, 'could not add serial movement');
	return db_insert_id();
}

/**
 * Get movement history for a serial number.
 *
 * @param int $serial_id Serial record id
 * @return resource DB result
 */
function get_serial_movements($serial_id) {
	$sql = "SELECT sm.*,
			COALESCE(fl.location_name, sm.from_loc) AS from_location_name,
			COALESCE(tl.location_name, sm.to_loc) AS to_location_name
		FROM " . TB_PREF . "serial_movements sm
		LEFT JOIN " . TB_PREF . "locations fl ON sm.from_loc = fl.loc_code
		LEFT JOIN " . TB_PREF . "locations tl ON sm.to_loc = tl.loc_code
		WHERE sm.serial_id=" . (int)$serial_id
		. " ORDER BY sm.tran_date DESC, sm.id DESC";
	return db_query($sql, 'could not get serial movements');
}

/**
 * Count movements for a serial number.
 *
 * @param int $serial_id Serial record id
 * @return int
 */
function count_serial_movements($serial_id) {
	$sql = "SELECT COUNT(*) FROM " . TB_PREF . "serial_movements WHERE serial_id=" . (int)$serial_id;
	$result = db_query($sql, 'could not count serial movements');
	$row = db_fetch_row($result);
	return (int)$row[0];
}

//----------------------------------------------------------------------
// Summary / statistics
//----------------------------------------------------------------------

/**
 * Get serial number counts grouped by status for an item.
 *
 * @param string $stock_id Item code ('' for all items)
 * @return array Associative array of status => count
 */
function get_serial_status_summary($stock_id = '') {
	$sql = "SELECT status, COUNT(*) AS cnt FROM " . TB_PREF . "serial_numbers WHERE inactive=0";
	if ($stock_id !== '' && $stock_id !== null)
		$sql .= " AND stock_id=" . db_escape($stock_id);
	$sql .= " GROUP BY status ORDER BY cnt DESC";

	$result = db_query($sql, 'could not get serial status summary');
	$summary = array();
	while ($row = db_fetch($result)) {
		$summary[$row['status']] = (int)$row['cnt'];
	}
	return $summary;
}

/**
 * Get serial numbers for a specific item at a specific location.
 *
 * @param string $stock_id Item code
 * @param string $loc_code Location code
 * @param string $status   Filter by status ('' for all)
 * @return resource DB result
 */
function get_serial_numbers_at_location($stock_id, $loc_code, $status = 'available') {
	$sql = "SELECT * FROM " . TB_PREF . "serial_numbers
		WHERE stock_id=" . db_escape($stock_id) . "
		AND loc_code=" . db_escape($loc_code) . "
		AND inactive=0";
	if ($status !== '')
		$sql .= " AND status=" . db_escape($status);
	$sql .= " ORDER BY serial_no";
	return db_query($sql, 'could not get serials at location');
}

/**
 * Get items that have serial tracking enabled.
 *
 * @return resource DB result
 */
function get_serial_tracked_items() {
	$sql = "SELECT stock_id, description, track_by, serial_no_prefix
		FROM " . TB_PREF . "stock_master
		WHERE (track_by='serial' OR track_by='serial_batch')
		AND inactive=0
		ORDER BY description";
	return db_query($sql, 'could not get serial tracked items');
}
