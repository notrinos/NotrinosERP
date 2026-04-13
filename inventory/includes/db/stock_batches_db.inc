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
 * Batch/Lot CRUD, queries, FEFO/FIFO allocation, expiry management,
 * movement logging, merge/split operations.
 *
 * Tables: 0_stock_batches, 0_batch_movements
 * Dependencies: serial_batch_db.inc (for generate_batch_number())
 */

/**
 * Valid batch statuses.
 *
 * @return array Associative array of status_code => display_label
 */
function get_batch_statuses() {
	return array(
		'active'     => _('Active'),
		'quarantine' => _('Quarantine'),
		'recalled'   => _('Recalled'),
		'expired'    => _('Expired'),
		'consumed'   => _('Consumed'),
		'scrapped'   => _('Scrapped'),
	);
}

/**
 * Get a human-readable label for a batch status.
 *
 * @param string $status Status code
 * @return string Translated label
 */
function get_batch_status_label($status) {
	$statuses = get_batch_statuses();
	return isset($statuses[$status]) ? $statuses[$status] : $status;
}

/**
 * Get a CSS color for a batch status.
 *
 * @param string $status Status code
 * @return string CSS color string
 */
function get_batch_status_color($status) {
	$colors = array(
		'active'     => '#28a745',
		'quarantine' => '#ffc107',
		'recalled'   => '#dc3545',
		'expired'    => '#6c757d',
		'consumed'   => '#17a2b8',
		'scrapped'   => '#343a40',
	);
	return isset($colors[$status]) ? $colors[$status] : '#6c757d';
}

/**
 * Get a CSS color representing the expiry urgency of a batch.
 *
 * @param string|null $expiry_date SQL date string (Y-m-d) or null
 * @return string CSS color string — green (ok), yellow (warning), orange (soon), red (expired), grey (no expiry)
 */
function get_batch_expiry_color($expiry_date) {
	if (!$expiry_date)
		return '#999';

	$today = date('Y-m-d');
	if ($expiry_date < $today)
		return '#dc3545'; // red — expired

	$days_left = (int)((strtotime($expiry_date) - strtotime($today)) / 86400);
	$warning_days = (int)get_company_pref('expiry_warning_days');
	if ($warning_days <= 0)
		$warning_days = 90;

	if ($days_left <= 30)
		return '#fd7e14'; // orange — expiring soon (<=30 days)
	if ($days_left <= $warning_days)
		return '#ffc107'; // yellow — approaching expiry
	return '#28a745'; // green — ok
}

//----------------------------------------------------------------------
// CRUD Operations
//----------------------------------------------------------------------

/**
 * Add a new batch record.
 *
 * @param string $batch_no           Batch/Lot number string
 * @param string $stock_id           Item code (FK to stock_master)
 * @param string $status             Initial status (default 'active')
 * @param string $manufacturing_date Manufacturing date
 * @param string $expiry_date        Expiry date
 * @param string $best_before_date   Best-before date
 * @param string $retest_date        Retest date
 * @param int    $shelf_life_days    Shelf life in days
 * @param int    $supplier_id        Supplier id
 * @param string $supplier_batch_no  Supplier's own batch number
 * @param int    $grn_id             GRN id that received this batch
 * @param int    $work_order_id      Work order that produced this batch
 * @param float  $initial_qty        Initial quantity received/produced
 * @param string $country_of_origin  Country of origin
 * @param string $certification      Quality certifications
 * @param string $notes              Notes
 * @return int The inserted batch id
 */
function add_stock_batch($batch_no, $stock_id, $status = 'active',
	$manufacturing_date = null, $expiry_date = null, $best_before_date = null,
	$retest_date = null, $shelf_life_days = null, $supplier_id = null,
	$supplier_batch_no = null, $grn_id = null, $work_order_id = null,
	$initial_qty = 0, $country_of_origin = null, $certification = null,
	$notes = null)
{
	$sql = "INSERT INTO " . TB_PREF . "stock_batches (
		batch_no, stock_id, status, manufacturing_date, expiry_date,
		best_before_date, retest_date, shelf_life_days, supplier_id,
		supplier_batch_no, grn_id, work_order_id, initial_qty,
		country_of_origin, certification, notes, created_by, created_at
	) VALUES (
		" . db_escape($batch_no) . ",
		" . db_escape($stock_id) . ",
		" . db_escape($status) . ",
		" . ($manufacturing_date ? db_escape(date2sql($manufacturing_date)) : 'NULL') . ",
		" . ($expiry_date ? db_escape(date2sql($expiry_date)) : 'NULL') . ",
		" . ($best_before_date ? db_escape(date2sql($best_before_date)) : 'NULL') . ",
		" . ($retest_date ? db_escape(date2sql($retest_date)) : 'NULL') . ",
		" . ($shelf_life_days !== null && $shelf_life_days !== '' ? (int)$shelf_life_days : 'NULL') . ",
		" . ($supplier_id !== null && $supplier_id !== '' ? (int)$supplier_id : 'NULL') . ",
		" . db_escape($supplier_batch_no) . ",
		" . ($grn_id !== null && $grn_id !== '' ? (int)$grn_id : 'NULL') . ",
		" . ($work_order_id !== null && $work_order_id !== '' ? (int)$work_order_id : 'NULL') . ",
		" . (float)$initial_qty . ",
		" . db_escape($country_of_origin) . ",
		" . db_escape($certification) . ",
		" . db_escape($notes) . ",
		" . (int)$_SESSION['wa_current_user']->user . ",
		NOW()
	)";
	db_query($sql, 'could not add stock batch');
	return db_insert_id();
}

/**
 * Update an existing batch record.
 *
 * @param int    $id                 Batch record id
 * @param string $batch_no           Batch number string
 * @param string $status             Status
 * @param string $manufacturing_date Manufacturing date
 * @param string $expiry_date        Expiry date
 * @param string $best_before_date   Best-before date
 * @param string $retest_date        Retest date
 * @param int    $shelf_life_days    Shelf life days
 * @param int    $supplier_id        Supplier id
 * @param string $supplier_batch_no  Supplier batch number
 * @param int    $grn_id             GRN id
 * @param int    $work_order_id      Work order id
 * @param float  $initial_qty        Initial quantity
 * @param string $country_of_origin  Country
 * @param string $certification      Certifications
 * @param string $notes              Notes
 */
function update_stock_batch($id, $batch_no, $status = null,
	$manufacturing_date = null, $expiry_date = null, $best_before_date = null,
	$retest_date = null, $shelf_life_days = null, $supplier_id = null,
	$supplier_batch_no = null, $grn_id = null, $work_order_id = null,
	$initial_qty = null, $country_of_origin = null, $certification = null,
	$notes = null)
{
	$sets = array();
	$sets[] = "batch_no=" . db_escape($batch_no);

	if ($status !== null)
		$sets[] = "status=" . db_escape($status);

	// Date fields
	$sets[] = "manufacturing_date=" . ($manufacturing_date ? db_escape(date2sql($manufacturing_date)) : 'NULL');
	$sets[] = "expiry_date=" . ($expiry_date ? db_escape(date2sql($expiry_date)) : 'NULL');
	$sets[] = "best_before_date=" . ($best_before_date ? db_escape(date2sql($best_before_date)) : 'NULL');
	$sets[] = "retest_date=" . ($retest_date ? db_escape(date2sql($retest_date)) : 'NULL');

	// Nullable integer fields
	$sets[] = "shelf_life_days=" . ($shelf_life_days !== null && $shelf_life_days !== '' ? (int)$shelf_life_days : 'NULL');
	$sets[] = "supplier_id=" . ($supplier_id !== null && $supplier_id !== '' ? (int)$supplier_id : 'NULL');
	$sets[] = "grn_id=" . ($grn_id !== null && $grn_id !== '' ? (int)$grn_id : 'NULL');
	$sets[] = "work_order_id=" . ($work_order_id !== null && $work_order_id !== '' ? (int)$work_order_id : 'NULL');

	// Other fields
	$sets[] = "supplier_batch_no=" . db_escape($supplier_batch_no);
	if ($initial_qty !== null)
		$sets[] = "initial_qty=" . (float)$initial_qty;
	$sets[] = "country_of_origin=" . db_escape($country_of_origin);
	$sets[] = "certification=" . db_escape($certification);
	$sets[] = "notes=" . db_escape($notes);

	$sql = "UPDATE " . TB_PREF . "stock_batches SET "
		. implode(', ', $sets)
		. " WHERE id=" . (int)$id;
	db_query($sql, 'could not update stock batch');
}

/**
 * Get a single batch record by id.
 *
 * @param int $id Batch record id
 * @return array|false Row array or false
 */
function get_stock_batch($id) {
	$sql = "SELECT b.*, sm.description AS item_description, sm.units AS item_units,
			COALESCE(sup.supp_name, '') AS supplier_name
		FROM " . TB_PREF . "stock_batches b
		LEFT JOIN " . TB_PREF . "stock_master sm ON b.stock_id = sm.stock_id
		LEFT JOIN " . TB_PREF . "suppliers sup ON b.supplier_id = sup.supplier_id
		WHERE b.id=" . (int)$id;
	$result = db_query($sql, 'could not get stock batch');
	return db_fetch($result);
}

/**
 * Get a batch by its batch_no string and stock_id.
 *
 * @param string $batch_no The batch number string
 * @param string $stock_id The item code
 * @return array|false Row array or false
 */
function get_stock_batch_by_code($batch_no, $stock_id=null) {
	$sql = "SELECT * FROM " . TB_PREF . "stock_batches
		WHERE batch_no=" . db_escape($batch_no);
	if ($stock_id !== null && $stock_id !== '')
		$sql .= " AND stock_id=" . db_escape($stock_id);
	$result = db_query($sql, 'could not get batch by code');
	return db_fetch($result);
}

/**
 * Search batches with flexible filters.
 *
 * @param string $stock_id     Filter by item ('' for all)
 * @param string $status       Filter by status ('' for all)
 * @param string $batch_like   Search batch_no with LIKE ('' for all)
 * @param bool   $show_inactive Include inactive records
 * @param string $expiry_filter 'expired'|'expiring_soon'|'ok'|'' for all
 * @param int    $supplier_id  Filter by supplier (0 for all)
 * @param int    $limit        Max rows (0 = unlimited)
 * @param int    $offset       Pagination offset
 * @return resource DB result
 */
function get_stock_batches($stock_id = '', $status = '', $batch_like = '',
	$show_inactive = false, $expiry_filter = '', $supplier_id = 0, $limit = 0, $offset = 0)
{
	$where = array();

	if ($stock_id !== '' && $stock_id !== null)
		$where[] = "b.stock_id=" . db_escape($stock_id);
	if ($status !== '' && $status !== null)
		$where[] = "b.status=" . db_escape($status);
	if ($batch_like !== '' && $batch_like !== null)
		$where[] = "b.batch_no LIKE " . db_escape('%' . $batch_like . '%');
	if (!$show_inactive)
		$where[] = "b.inactive=0";
	if ($supplier_id > 0)
		$where[] = "b.supplier_id=" . (int)$supplier_id;

	// Expiry filter
	if ($expiry_filter === 'expired')
		$where[] = "b.expiry_date IS NOT NULL AND b.expiry_date < CURDATE()";
	elseif ($expiry_filter === 'expiring_soon') {
		$warning_days = (int)get_company_pref('expiry_warning_days');
		if ($warning_days <= 0)
			$warning_days = 90;
		$where[] = "b.expiry_date IS NOT NULL AND b.expiry_date >= CURDATE() AND b.expiry_date <= DATE_ADD(CURDATE(), INTERVAL " . $warning_days . " DAY)";
	} elseif ($expiry_filter === 'ok')
		$where[] = "(b.expiry_date IS NULL OR b.expiry_date > DATE_ADD(CURDATE(), INTERVAL 90 DAY))";

	$sql = "SELECT b.*, sm.description AS item_description,
			COALESCE(sup.supp_name, '') AS supplier_name,
			DATEDIFF(b.expiry_date, CURDATE()) AS days_until_expiry
		FROM " . TB_PREF . "stock_batches b
		LEFT JOIN " . TB_PREF . "stock_master sm ON b.stock_id = sm.stock_id
		LEFT JOIN " . TB_PREF . "suppliers sup ON b.supplier_id = sup.supplier_id";

	if (count($where) > 0)
		$sql .= " WHERE " . implode(' AND ', $where);

	$sql .= " ORDER BY b.expiry_date ASC, b.created_at DESC";

	if ($limit > 0)
		$sql .= " LIMIT " . (int)$offset . ", " . (int)$limit;

	return db_query($sql, 'could not get stock batches');
}

/**
 * Count batches matching filters (for pagination).
 *
 * @param string $stock_id     Filter by item
 * @param string $status       Filter by status
 * @param string $batch_like   Search term
 * @param bool   $show_inactive Include inactive
 * @param string $expiry_filter Expiry filter
 * @param int    $supplier_id  Filter by supplier
 * @return int Total count
 */
function count_stock_batches($stock_id = '', $status = '', $batch_like = '',
	$show_inactive = false, $expiry_filter = '', $supplier_id = 0)
{
	$where = array();

	if ($stock_id !== '' && $stock_id !== null)
		$where[] = "stock_id=" . db_escape($stock_id);
	if ($status !== '' && $status !== null)
		$where[] = "status=" . db_escape($status);
	if ($batch_like !== '' && $batch_like !== null)
		$where[] = "batch_no LIKE " . db_escape('%' . $batch_like . '%');
	if (!$show_inactive)
		$where[] = "inactive=0";
	if ($supplier_id > 0)
		$where[] = "supplier_id=" . (int)$supplier_id;

	if ($expiry_filter === 'expired')
		$where[] = "expiry_date IS NOT NULL AND expiry_date < CURDATE()";
	elseif ($expiry_filter === 'expiring_soon') {
		$warning_days = (int)get_company_pref('expiry_warning_days');
		if ($warning_days <= 0)
			$warning_days = 90;
		$where[] = "expiry_date IS NOT NULL AND expiry_date >= CURDATE() AND expiry_date <= DATE_ADD(CURDATE(), INTERVAL " . $warning_days . " DAY)";
	} elseif ($expiry_filter === 'ok')
		$where[] = "(expiry_date IS NULL OR expiry_date > DATE_ADD(CURDATE(), INTERVAL 90 DAY))";

	$sql = "SELECT COUNT(*) FROM " . TB_PREF . "stock_batches";
	if (count($where) > 0)
		$sql .= " WHERE " . implode(' AND ', $where);

	$result = db_query($sql, 'could not count stock batches');
	$row = db_fetch_row($result);
	return (int)$row[0];
}

/**
 * Check if a batch number string already exists for an item.
 *
 * @param string $batch_no  Batch number string
 * @param string $stock_id  Item code
 * @param int    $exclude_id Exclude this record id (for updates)
 * @return bool True if duplicate exists
 */
function is_batch_number_duplicate($batch_no, $stock_id, $exclude_id = 0) {
	$sql = "SELECT COUNT(*) FROM " . TB_PREF . "stock_batches
		WHERE batch_no=" . db_escape($batch_no) . " AND stock_id=" . db_escape($stock_id);
	if ($exclude_id > 0)
		$sql .= " AND id<>" . (int)$exclude_id;
	$result = db_query($sql, 'could not check batch duplicate');
	$row = db_fetch_row($result);
	return $row[0] > 0;
}

/**
 * Check if a batch can be deleted (no movements exist).
 *
 * @param int $id Batch record id
 * @return bool|string True if deletable, error message string if not
 */
function can_delete_stock_batch($id) {
	// Check batch_movements
	$sql = "SELECT COUNT(*) FROM " . TB_PREF . "batch_movements WHERE batch_id=" . (int)$id;
	$result = db_query($sql, 'could not check batch movements');
	$row = db_fetch_row($result);
	if ($row[0] > 0)
		return _('Cannot delete this batch because it has movement history.');

	// Check serial numbers linked to this batch
	$sql = "SELECT COUNT(*) FROM " . TB_PREF . "serial_numbers WHERE batch_id=" . (int)$id;
	$result = db_query($sql, 'could not check batch serials');
	$row = db_fetch_row($result);
	if ($row[0] > 0)
		return _('Cannot delete this batch because it has serial numbers linked to it.');

	return true;
}

/**
 * Delete a batch record.
 *
 * @param int $id Batch record id
 */
function delete_stock_batch($id) {
	$sql = "DELETE FROM " . TB_PREF . "stock_batches WHERE id=" . (int)$id;
	db_query($sql, 'could not delete stock batch');
}

//----------------------------------------------------------------------
// Bulk operations
//----------------------------------------------------------------------

/**
 * Bulk-create batch numbers for an item using auto-generation.
 *
 * @param string $stock_id         Item code
 * @param int    $count            Number of batches to generate
 * @param string $status           Initial status
 * @param string $manufacturing_date Manufacturing date (shared)
 * @param string $expiry_date      Expiry date (shared, if null uses shelf_life auto-calc)
 * @param int    $supplier_id      Supplier id
 * @param float  $initial_qty      Initial quantity per batch
 * @param string $notes            Notes for all generated batches
 * @return array Array of generated batch_no strings
 */
function bulk_create_stock_batches($stock_id, $count, $status = 'active',
	$manufacturing_date = null, $expiry_date = null, $supplier_id = null,
	$initial_qty = 0, $notes = null)
{
	$generated = array();

	// Auto-calculate expiry from shelf_life if not provided
	if (!$expiry_date && $manufacturing_date) {
		$item = get_item($stock_id);
		if ($item && $item['shelf_life_days'] > 0) {
			$mfg_ts = strtotime(date2sql($manufacturing_date));
			if ($mfg_ts)
				$expiry_date = sql2date(date('Y-m-d', $mfg_ts + $item['shelf_life_days'] * 86400));
		}
	}

	begin_transaction();

	for ($i = 0; $i < $count; $i++) {
		$batch_no = generate_batch_number($stock_id);

		// Safety: ensure no duplicate
		$attempts = 0;
		while (is_batch_number_duplicate($batch_no, $stock_id) && $attempts < 10) {
			$batch_no = generate_batch_number($stock_id);
			$attempts++;
		}

		if (is_batch_number_duplicate($batch_no, $stock_id))
			$batch_no .= '-' . strtoupper(substr(md5(uniqid('', true)), 0, 4));

		add_stock_batch($batch_no, $stock_id, $status,
			$manufacturing_date, $expiry_date, null, null, null,
			$supplier_id, null, null, null, $initial_qty, null, null, $notes);

		$generated[] = $batch_no;
	}

	commit_transaction();
	return $generated;
}

/**
 * Import batch numbers from a text string (one batch per line or comma-separated).
 *
 * @param string $stock_id     Item code
 * @param string $csv_text     Text with batch numbers
 * @param string $status       Initial status
 * @param string $expiry_date  Shared expiry date (optional)
 * @param int    $supplier_id  Supplier id
 * @param float  $initial_qty  Initial quantity per batch
 * @return array ['imported' => count, 'duplicates' => array, 'errors' => array]
 */
function import_stock_batches($stock_id, $csv_text, $status = 'active',
	$expiry_date = null, $supplier_id = null, $initial_qty = 0)
{
	$result = array('imported' => 0, 'duplicates' => array(), 'errors' => array());

	$lines = preg_split('/[\r\n,]+/', $csv_text);

	begin_transaction();

	foreach ($lines as $line) {
		$batch_no = trim($line);
		if ($batch_no === '')
			continue;

		if (strlen($batch_no) > 100) {
			$result['errors'][] = sprintf(_('Batch "%s" exceeds maximum 100 characters.'), substr($batch_no, 0, 20) . '...');
			continue;
		}

		if (is_batch_number_duplicate($batch_no, $stock_id)) {
			$result['duplicates'][] = $batch_no;
			continue;
		}

		add_stock_batch($batch_no, $stock_id, $status,
			null, $expiry_date, null, null, null,
			$supplier_id, null, null, null, $initial_qty, null, null, null);
		$result['imported']++;
	}

	commit_transaction();
	return $result;
}

//----------------------------------------------------------------------
// Status management
//----------------------------------------------------------------------

/**
 * Update batch status and optionally log a movement.
 *
 * @param int    $batch_id   Batch record id
 * @param string $new_status New status
 * @param string $notes      Notes
 */
function update_batch_status($batch_id, $new_status, $notes = '') {
	$sql = "UPDATE " . TB_PREF . "stock_batches SET status=" . db_escape($new_status)
		. " WHERE id=" . (int)$batch_id;
	db_query($sql, 'could not update batch status');
}

/**
 * Auto-expire batches that are past their expiry date.
 * Changes status from 'active' to 'expired' for all batches with expiry_date < today.
 *
 * @return int Number of batches expired
 */
function auto_expire_batches() {
	$sql = "UPDATE " . TB_PREF . "stock_batches
		SET status='expired'
		WHERE status='active' AND expiry_date IS NOT NULL AND expiry_date < CURDATE()";
	db_query($sql, 'could not auto-expire batches');

	$sql = "SELECT ROW_COUNT()";
	$result = db_query($sql, 'could not get row count');
	$row = db_fetch_row($result);
	return (int)$row[0];
}

//----------------------------------------------------------------------
// Movement logging
//----------------------------------------------------------------------

/**
 * Add a batch movement record.
 *
 * @param int    $batch_id      Batch record id
 * @param int    $trans_type    Transaction type
 * @param int    $trans_no      Transaction number
 * @param string $loc_code      Location code
 * @param int    $wh_loc_id     Bin location id
 * @param float  $quantity      Quantity moved (+inbound, -outbound)
 * @param string $tran_date     Movement date
 * @param string $reference     Reference string
 * @param string $notes         Notes
 * @param int    $stock_move_id FK to stock_moves.trans_id
 * @return int Inserted movement id
 */
function add_batch_movement($batch_id, $trans_type, $trans_no, $loc_code,
	$wh_loc_id = null, $quantity = 0, $tran_date = null, $reference = '',
	$notes = '', $stock_move_id = null)
{
	if ($tran_date === null)
		$tran_date = Today();

	$sql = "INSERT INTO " . TB_PREF . "batch_movements (
		batch_id, trans_type, trans_no, loc_code, wh_loc_id,
		quantity, tran_date, reference, notes,
		stock_move_id, created_by, created_at
	) VALUES (
		" . (int)$batch_id . ",
		" . (int)$trans_type . ",
		" . (int)$trans_no . ",
		" . db_escape($loc_code) . ",
		" . ($wh_loc_id !== null ? (int)$wh_loc_id : 'NULL') . ",
		" . (float)$quantity . ",
		" . db_escape(date2sql($tran_date)) . ",
		" . db_escape($reference) . ",
		" . db_escape($notes) . ",
		" . ($stock_move_id !== null ? (int)$stock_move_id : 'NULL') . ",
		" . (int)$_SESSION['wa_current_user']->user . ",
		NOW()
	)";
	db_query($sql, 'could not add batch movement');
	return db_insert_id();
}

/**
 * Get movement history for a batch.
 *
 * @param int $batch_id Batch record id
 * @return resource DB result
 */
function get_batch_movements($batch_id) {
	$sql = "SELECT bm.*,
			COALESCE(loc.location_name, bm.loc_code) AS location_name
		FROM " . TB_PREF . "batch_movements bm
		LEFT JOIN " . TB_PREF . "locations loc ON bm.loc_code = loc.loc_code
		WHERE bm.batch_id=" . (int)$batch_id
		. " ORDER BY bm.tran_date DESC, bm.id DESC";
	return db_query($sql, 'could not get batch movements');
}

/**
 * Count movements for a batch.
 *
 * @param int $batch_id Batch record id
 * @return int
 */
function count_batch_movements($batch_id) {
	$sql = "SELECT COUNT(*) FROM " . TB_PREF . "batch_movements WHERE batch_id=" . (int)$batch_id;
	$result = db_query($sql, 'could not count batch movements');
	$row = db_fetch_row($result);
	return (int)$row[0];
}

/**
 * Get the current total quantity for a batch across all locations (from movements).
 *
 * @param int    $batch_id Batch record id
 * @param string $loc_code Location code ('' for all locations)
 * @return float Total quantity
 */
function get_batch_quantity_on_hand($batch_id, $loc_code = '') {
	$sql = "SELECT COALESCE(SUM(quantity), 0) FROM " . TB_PREF . "batch_movements
		WHERE batch_id=" . (int)$batch_id;
	if ($loc_code !== '' && $loc_code !== null)
		$sql .= " AND loc_code=" . db_escape($loc_code);
	$result = db_query($sql, 'could not get batch qty on hand');
	$row = db_fetch_row($result);
	return (float)$row[0];
}

/**
 * Get batch quantity breakdown by location.
 *
 * @param int $batch_id Batch record id
 * @return resource DB result with loc_code, location_name, qty
 */
function get_batch_location_breakdown($batch_id) {
	$sql = "SELECT bm.loc_code,
			COALESCE(loc.location_name, bm.loc_code) AS location_name,
			SUM(bm.quantity) AS qty
		FROM " . TB_PREF . "batch_movements bm
		LEFT JOIN " . TB_PREF . "locations loc ON bm.loc_code = loc.loc_code
		WHERE bm.batch_id=" . (int)$batch_id . "
		GROUP BY bm.loc_code
		HAVING qty <> 0
		ORDER BY loc.location_name";
	return db_query($sql, 'could not get batch location breakdown');
}

//----------------------------------------------------------------------
// FEFO / FIFO allocation
//----------------------------------------------------------------------

/**
 * Get batches available for allocation, ordered by FEFO (First Expiry, First Out).
 * Returns only active batches with qty > 0.
 *
 * @param string $stock_id  Item code
 * @param string $loc_code  Location code ('' for all)
 * @param float  $qty_needed Quantity needed (0 = return all)
 * @return array Array of [batch_id, batch_no, expiry_date, qty_available, days_until_expiry]
 */
function get_fefo_batches($stock_id, $loc_code = '', $qty_needed = 0) {
	$where = "b.stock_id=" . db_escape($stock_id) . " AND b.status='active' AND b.inactive=0";
	if ($loc_code !== '' && $loc_code !== null)
		$where .= " AND bm.loc_code=" . db_escape($loc_code);

	$sql = "SELECT b.id AS batch_id, b.batch_no, b.expiry_date,
			SUM(bm.quantity) AS qty_available,
			DATEDIFF(b.expiry_date, CURDATE()) AS days_until_expiry
		FROM " . TB_PREF . "stock_batches b
		INNER JOIN " . TB_PREF . "batch_movements bm ON b.id = bm.batch_id
		WHERE $where
		GROUP BY b.id
		HAVING qty_available > 0
		ORDER BY CASE WHEN b.expiry_date IS NULL THEN 1 ELSE 0 END,
				 b.expiry_date ASC, b.created_at ASC";

	$result = db_query($sql, 'could not get FEFO batches');
	$batches = array();
	$allocated = 0;

	while ($row = db_fetch($result)) {
		$batches[] = $row;
		$allocated += $row['qty_available'];
		if ($qty_needed > 0 && $allocated >= $qty_needed)
			break;
	}

	return $batches;
}

/**
 * Get batches available for allocation, ordered by FIFO (First In, First Out).
 *
 * @param string $stock_id  Item code
 * @param string $loc_code  Location code ('' for all)
 * @param float  $qty_needed Quantity needed (0 = return all)
 * @return array Array of batch rows ordered by creation date
 */
function get_fifo_batches($stock_id, $loc_code = '', $qty_needed = 0) {
	$where = "b.stock_id=" . db_escape($stock_id) . " AND b.status='active' AND b.inactive=0";
	if ($loc_code !== '' && $loc_code !== null)
		$where .= " AND bm.loc_code=" . db_escape($loc_code);

	$sql = "SELECT b.id AS batch_id, b.batch_no, b.expiry_date,
			SUM(bm.quantity) AS qty_available,
			DATEDIFF(b.expiry_date, CURDATE()) AS days_until_expiry
		FROM " . TB_PREF . "stock_batches b
		INNER JOIN " . TB_PREF . "batch_movements bm ON b.id = bm.batch_id
		WHERE $where
		GROUP BY b.id
		HAVING qty_available > 0
		ORDER BY b.created_at ASC";

	$result = db_query($sql, 'could not get FIFO batches');
	$batches = array();
	$allocated = 0;

	while ($row = db_fetch($result)) {
		$batches[] = $row;
		$allocated += $row['qty_available'];
		if ($qty_needed > 0 && $allocated >= $qty_needed)
			break;
	}

	return $batches;
}

/**
 * Allocate batch quantities using the item's configured removal strategy (FEFO/FIFO).
 *
 * @param string $stock_id  Item code
 * @param float  $qty       Quantity to allocate
 * @param string $loc_code  Location code
 * @return array Array of ['batch_id' => id, 'batch_no' => no, 'qty' => allocated_qty]
 */
function allocate_batches($stock_id, $qty, $loc_code = '') {
	$item = get_item($stock_id);
	$strategy = isset($item['removal_strategy']) && $item['removal_strategy']
		? $item['removal_strategy'] : 'fefo';

	if ($strategy === 'fifo')
		$available = get_fifo_batches($stock_id, $loc_code, $qty);
	else
		$available = get_fefo_batches($stock_id, $loc_code, $qty);

	$allocations = array();
	$remaining = $qty;

	foreach ($available as $batch) {
		if ($remaining <= 0)
			break;

		$alloc = min($batch['qty_available'], $remaining);
		$allocations[] = array(
			'batch_id' => $batch['batch_id'],
			'batch_no' => $batch['batch_no'],
			'qty' => $alloc,
			'expiry_date' => $batch['expiry_date'],
		);
		$remaining -= $alloc;
	}

	return $allocations;
}

//----------------------------------------------------------------------
// Merge / Split operations
//----------------------------------------------------------------------

/**
 * Merge multiple batches into a target batch.
 * Moves all remaining quantities from source batches to the target batch.
 *
 * @param int   $target_batch_id  The batch to merge into
 * @param array $source_batch_ids Array of batch IDs to merge from
 * @param string $loc_code        Location code
 * @param string $notes           Merge notes
 * @return array ['merged_qty' => total, 'source_count' => count]
 */
function merge_batches($target_batch_id, $source_batch_ids, $loc_code, $notes = '') {
	$merged_qty = 0;
	$source_count = 0;

	begin_transaction();

	$target = get_stock_batch($target_batch_id);
	if (!$target) {
		commit_transaction();
		return array('merged_qty' => 0, 'source_count' => 0);
	}

	foreach ($source_batch_ids as $source_id) {
		$source_id = (int)$source_id;
		if ($source_id == $target_batch_id)
			continue;

		$source_qty = get_batch_quantity_on_hand($source_id, $loc_code);
		if ($source_qty <= 0)
			continue;

		// Move qty out of source
		add_batch_movement($source_id, 0, 0, $loc_code, null, -$source_qty,
			Today(), '', sprintf(_('Merged into batch #%d'), $target_batch_id));

		// Move qty into target
		add_batch_movement($target_batch_id, 0, 0, $loc_code, null, $source_qty,
			Today(), '', sprintf(_('Merged from batch #%d. %s'), $source_id, $notes));

		// Mark source as consumed if no qty left anywhere
		$total_remaining = get_batch_quantity_on_hand($source_id);
		if ($total_remaining <= 0)
			update_batch_status($source_id, 'consumed');

		$merged_qty += $source_qty;
		$source_count++;
	}

	commit_transaction();
	return array('merged_qty' => $merged_qty, 'source_count' => $source_count);
}

/**
 * Split a quantity from a source batch into a new batch.
 *
 * @param int    $source_batch_id Source batch id
 * @param float  $split_qty       Quantity to split off
 * @param string $new_batch_no    New batch number (null = auto-generate)
 * @param string $loc_code        Location code
 * @param string $notes           Split notes
 * @return int|false New batch id or false on failure
 */
function split_batch($source_batch_id, $split_qty, $new_batch_no = null, $loc_code = '', $notes = '') {
	$source = get_stock_batch($source_batch_id);
	if (!$source)
		return false;

	$available = get_batch_quantity_on_hand($source_batch_id, $loc_code);
	if ($split_qty > $available || $split_qty <= 0)
		return false;

	begin_transaction();

	// Generate batch number if not provided
	if (!$new_batch_no) {
		include_once(dirname(__FILE__) . '/serial_batch_db.inc');
		$new_batch_no = generate_batch_number($source['stock_id']);
		$attempts = 0;
		while (is_batch_number_duplicate($new_batch_no, $source['stock_id']) && $attempts < 10) {
			$new_batch_no = generate_batch_number($source['stock_id']);
			$attempts++;
		}
	}

	// Create new batch with same attributes as source
	$new_id = add_stock_batch(
		$new_batch_no,
		$source['stock_id'],
		$source['status'],
		$source['manufacturing_date'] ? sql2date($source['manufacturing_date']) : null,
		$source['expiry_date'] ? sql2date($source['expiry_date']) : null,
		$source['best_before_date'] ? sql2date($source['best_before_date']) : null,
		$source['retest_date'] ? sql2date($source['retest_date']) : null,
		$source['shelf_life_days'],
		$source['supplier_id'],
		$source['supplier_batch_no'],
		$source['grn_id'],
		$source['work_order_id'],
		$split_qty,
		$source['country_of_origin'],
		$source['certification'],
		sprintf(_('Split from batch %s. %s'), $source['batch_no'], $notes)
	);

	// Move qty out of source
	add_batch_movement($source_batch_id, 0, 0, $loc_code, null, -$split_qty,
		Today(), '', sprintf(_('Split to new batch #%d (%s)'), $new_id, $new_batch_no));

	// Move qty into new batch
	add_batch_movement($new_id, 0, 0, $loc_code, null, $split_qty,
		Today(), '', sprintf(_('Split from batch #%d (%s)'), $source_batch_id, $source['batch_no']));

	commit_transaction();
	return $new_id;
}

//----------------------------------------------------------------------
// Summary / statistics
//----------------------------------------------------------------------

/**
 * Get batch counts grouped by status for an item (or all items).
 *
 * @param string $stock_id Item code ('' for all items)
 * @return array Associative array of status => count
 */
function get_batch_status_summary($stock_id = '') {
	$where = '';
	if ($stock_id !== '' && $stock_id !== null)
		$where = " WHERE stock_id=" . db_escape($stock_id);

	$sql = "SELECT status, COUNT(*) AS cnt FROM " . TB_PREF . "stock_batches" . $where
		. " GROUP BY status ORDER BY FIELD(status, 'active', 'quarantine', 'recalled', 'expired', 'consumed', 'scrapped')";

	$result = db_query($sql, 'could not get batch status summary');
	$summary = array();
	while ($row = db_fetch($result))
		$summary[$row['status']] = (int)$row['cnt'];

	return $summary;
}

/**
 * Get expiry summary — counts of batches in each expiry category.
 *
 * @param string $stock_id Item code ('' for all)
 * @return array ['expired' => int, 'critical' => int (<=30d), 'warning' => int (<=90d), 'ok' => int, 'no_expiry' => int]
 */
function get_batch_expiry_summary($stock_id = '') {
	$where_base = "status='active' AND inactive=0";
	if ($stock_id !== '' && $stock_id !== null)
		$where_base .= " AND stock_id=" . db_escape($stock_id);

	$summary = array('expired' => 0, 'critical' => 0, 'warning' => 0, 'ok' => 0, 'no_expiry' => 0);

	$warning_days = (int)get_company_pref('expiry_warning_days');
	if ($warning_days <= 0)
		$warning_days = 90;

	// Expired
	$sql = "SELECT COUNT(*) FROM " . TB_PREF . "stock_batches WHERE $where_base AND expiry_date IS NOT NULL AND expiry_date < CURDATE()";
	$result = db_query($sql, 'expiry summary');
	$row = db_fetch_row($result);
	$summary['expired'] = (int)$row[0];

	// Critical (<=30 days)
	$sql = "SELECT COUNT(*) FROM " . TB_PREF . "stock_batches WHERE $where_base AND expiry_date IS NOT NULL AND expiry_date >= CURDATE() AND expiry_date <= DATE_ADD(CURDATE(), INTERVAL 30 DAY)";
	$result = db_query($sql, 'expiry summary');
	$row = db_fetch_row($result);
	$summary['critical'] = (int)$row[0];

	// Warning (31 to warning_days)
	$sql = "SELECT COUNT(*) FROM " . TB_PREF . "stock_batches WHERE $where_base AND expiry_date IS NOT NULL AND expiry_date > DATE_ADD(CURDATE(), INTERVAL 30 DAY) AND expiry_date <= DATE_ADD(CURDATE(), INTERVAL $warning_days DAY)";
	$result = db_query($sql, 'expiry summary');
	$row = db_fetch_row($result);
	$summary['warning'] = (int)$row[0];

	// OK (beyond warning_days)
	$sql = "SELECT COUNT(*) FROM " . TB_PREF . "stock_batches WHERE $where_base AND expiry_date IS NOT NULL AND expiry_date > DATE_ADD(CURDATE(), INTERVAL $warning_days DAY)";
	$result = db_query($sql, 'expiry summary');
	$row = db_fetch_row($result);
	$summary['ok'] = (int)$row[0];

	// No expiry date
	$sql = "SELECT COUNT(*) FROM " . TB_PREF . "stock_batches WHERE $where_base AND expiry_date IS NULL";
	$result = db_query($sql, 'expiry summary');
	$row = db_fetch_row($result);
	$summary['no_expiry'] = (int)$row[0];

	return $summary;
}

/**
 * Get batches that are expiring within a given number of days (for dashboard/alerts).
 *
 * @param int    $days     Number of days from today
 * @param string $stock_id Item code ('' for all)
 * @param int    $limit    Max rows
 * @return resource DB result
 */
function get_expiring_batches($days = 90, $stock_id = '', $limit = 50) {
	$where = "b.status='active' AND b.inactive=0 AND b.expiry_date IS NOT NULL
		AND b.expiry_date >= CURDATE() AND b.expiry_date <= DATE_ADD(CURDATE(), INTERVAL " . (int)$days . " DAY)";

	if ($stock_id !== '' && $stock_id !== null)
		$where .= " AND b.stock_id=" . db_escape($stock_id);

	$sql = "SELECT b.*, sm.description AS item_description,
			DATEDIFF(b.expiry_date, CURDATE()) AS days_until_expiry
		FROM " . TB_PREF . "stock_batches b
		LEFT JOIN " . TB_PREF . "stock_master sm ON b.stock_id = sm.stock_id
		WHERE $where
		ORDER BY b.expiry_date ASC
		LIMIT " . (int)$limit;

	return db_query($sql, 'could not get expiring batches');
}

/**
 * Get already-expired batches still marked as active (candidates for auto-expire).
 *
 * @param string $stock_id Item code ('' for all)
 * @param int    $limit    Max rows
 * @return resource DB result
 */
function get_expired_active_batches($stock_id = '', $limit = 50) {
	$where = "b.status='active' AND b.inactive=0 AND b.expiry_date IS NOT NULL AND b.expiry_date < CURDATE()";

	if ($stock_id !== '' && $stock_id !== null)
		$where .= " AND b.stock_id=" . db_escape($stock_id);

	$sql = "SELECT b.*, sm.description AS item_description,
			DATEDIFF(CURDATE(), b.expiry_date) AS days_past_expiry
		FROM " . TB_PREF . "stock_batches b
		LEFT JOIN " . TB_PREF . "stock_master sm ON b.stock_id = sm.stock_id
		WHERE $where
		ORDER BY b.expiry_date ASC
		LIMIT " . (int)$limit;

	return db_query($sql, 'could not get expired active batches');
}

/**
 * Get batch-tracked items list (items with track_by = 'batch' or 'serial_batch').
 *
 * @return resource DB result
 */
function get_batch_tracked_items() {
	$sql = "SELECT stock_id, description, track_by, batch_no_prefix, shelf_life_days
		FROM " . TB_PREF . "stock_master
		WHERE track_by IN ('batch', 'serial_batch')
		ORDER BY description";
	return db_query($sql, 'could not get batch tracked items');
}

/**
 * Calculate the expiry date from a manufacturing date using item's shelf_life_days.
 *
 * @param string $stock_id          Item code
 * @param string $manufacturing_date Manufacturing date (display format)
 * @return string|null Calculated expiry date (display format) or null
 */
function calculate_expiry_from_shelf_life($stock_id, $manufacturing_date) {
	if (!$manufacturing_date)
		return null;

	$item = get_item($stock_id);
	if (!$item || !$item['shelf_life_days'] || $item['shelf_life_days'] <= 0)
		return null;

	$mfg_sql = date2sql($manufacturing_date);
	$mfg_ts = strtotime($mfg_sql);
	if (!$mfg_ts)
		return null;

	$expiry_ts = $mfg_ts + ($item['shelf_life_days'] * 86400);
	return sql2date(date('Y-m-d', $expiry_ts));
}
