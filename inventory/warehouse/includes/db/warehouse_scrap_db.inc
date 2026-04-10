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
 * Warehouse Scrap — CRUD, stock move creation, GL posting, serial/batch status updates.
 *
 * Scrap entries remove inventory (negative stock move) and create a GL journal
 * debiting the adjustment (scrap) account and crediting the inventory account.
 *
 * Tables: 0_wh_scrap, 0_stock_moves, 0_gl_trans
 *
 * @package NotrinosERP
 * @subpackage Inventory/Warehouse
 */

/**
 * Get standard scrap reason codes with labels.
 *
 * @return array reason_code => label
 */
function get_scrap_reason_codes() {
	return array(
		'damaged'       => _('Damaged'),
		'expired'       => _('Expired'),
		'qc_fail'       => _('QC Failure'),
		'obsolete'      => _('Obsolete'),
		'production'    => _('Production Waste'),
		'contaminated'  => _('Contaminated'),
		'customer_return' => _('Customer Return (Unsalvageable)'),
		'other'         => _('Other'),
	);
}

/**
 * Get CSS color for a scrap reason code.
 *
 * @param string $reason_code Reason code
 * @return string Hex color
 */
function get_scrap_reason_color($reason_code) {
	$colors = array(
		'damaged'       => '#dc3545',
		'expired'       => '#fd7e14',
		'qc_fail'       => '#e83e8c',
		'obsolete'      => '#6c757d',
		'production'    => '#17a2b8',
		'contaminated'  => '#6610f2',
		'customer_return' => '#ffc107',
		'other'         => '#495057',
	);
	return isset($colors[$reason_code]) ? $colors[$reason_code] : '#6c757d';
}

/**
 * Get a display label for a reason code.
 *
 * @param string $reason_code Reason code
 * @return string Display label
 */
function get_scrap_reason_label($reason_code) {
	$reasons = get_scrap_reason_codes();
	return isset($reasons[$reason_code]) ? $reasons[$reason_code] : $reason_code;
}

/**
 * Create a scrap entry with stock move and GL posting.
 *
 * @param string      $stock_id      Item code
 * @param float       $qty           Quantity to scrap (positive)
 * @param string      $loc_code      Warehouse location code (0_locations.loc_code)
 * @param string      $date_         Scrap date (display format)
 * @param string      $reason_code   Reason code
 * @param string|null $reason_detail Free-text reason detail
 * @param int|null    $from_bin_id   Source bin (wh_locations.loc_id)
 * @param int|null    $scrap_loc_id  Scrap destination bin (wh_locations.loc_id)
 * @param int|null    $serial_id     Serial number FK
 * @param int|null    $batch_id      Batch FK
 * @param string|null $memo          Memo text
 * @return int|false  scrap_id on success, false on failure
 */
function add_scrap_entry($stock_id, $qty, $loc_code, $date_, $reason_code, $reason_detail = null,
	$from_bin_id = null, $scrap_loc_id = null, $serial_id = null, $batch_id = null, $memo = null) {

	global $Refs, $path_to_root;

	if ($qty <= 0) return false;

	begin_transaction();

	// Get item cost
	$item_cost = get_standard_cost($stock_id);
	$unit_cost = round2($item_cost, user_price_dec());
	$total_cost = round2($unit_cost * $qty, user_price_dec());

	// Create inventory adjustment (negative stock move)
	$adj_id = get_next_trans_no(ST_INVADJUST);
	$ref = $Refs->get_next(ST_INVADJUST, null, $date_);

	// Update average material cost
	update_average_material_cost(null, $stock_id, $unit_cost, -$qty, $date_);

	// Build extended params for bin/serial/batch tracking
	$extended = array();
	if ($from_bin_id) $extended['from_bin_id'] = $from_bin_id;
	if ($scrap_loc_id) $extended['to_bin_id'] = $scrap_loc_id;
	if ($serial_id) $extended['serial_id'] = $serial_id;
	if ($batch_id) $extended['batch_id'] = $batch_id;
	$extended['stock_status'] = 'quarantine';

	$stock_move_id = add_stock_move_extended(
		ST_INVADJUST, $stock_id, $adj_id, $loc_code, $date_, $ref,
		-$qty, $unit_cost, 0, $extended
	);

	// GL posting: debit adjustment (scrap) account, credit inventory account
	$stock_gl_codes = get_stock_gl_code($stock_id);
	$gl_memo = $memo ? $memo : sprintf(_('Scrap: %s x%s — %s'), $stock_id, $qty, get_scrap_reason_label($reason_code));

	if ($unit_cost > 0) {
		// Debit adjustment/scrap account (expense)
		add_gl_trans_std_cost(ST_INVADJUST, $adj_id, $date_,
			$stock_gl_codes['adjustment_account'],
			$stock_gl_codes['dimension_id'], $stock_gl_codes['dimension2_id'],
			$gl_memo, $total_cost);

		// Credit inventory account (reduce inventory value)
		add_gl_trans_std_cost(ST_INVADJUST, $adj_id, $date_,
			$stock_gl_codes['inventory_account'],
			$stock_gl_codes['dimension_id'], $stock_gl_codes['dimension2_id'],
			$gl_memo, -$total_cost);
	}

	$Refs->save(ST_INVADJUST, $adj_id, $ref);
	add_comments(ST_INVADJUST, $adj_id, $date_, $gl_memo);
	add_audit_trail(ST_INVADJUST, $adj_id, $date_);

	// Insert scrap record
	$sql = "INSERT INTO " . TB_PREF . "wh_scrap (
		scrap_date, stock_id, qty, from_loc_id, scrap_loc_id,
		reason_code, reason_detail, serial_id, batch_id,
		unit_cost, total_cost, stock_move_id, memo
	) VALUES ("
		. "'" . date2sql($date_) . "', "
		. db_escape($stock_id) . ", "
		. db_escape($qty) . ", "
		. ($from_bin_id ? (int)$from_bin_id : 'NULL') . ", "
		. ($scrap_loc_id ? (int)$scrap_loc_id : 'NULL') . ", "
		. db_escape($reason_code) . ", "
		. ($reason_detail ? db_escape($reason_detail) : 'NULL') . ", "
		. ($serial_id ? (int)$serial_id : 'NULL') . ", "
		. ($batch_id ? (int)$batch_id : 'NULL') . ", "
		. db_escape($unit_cost) . ", "
		. db_escape($total_cost) . ", "
		. ($stock_move_id ? (int)$stock_move_id : 'NULL') . ", "
		. ($memo ? db_escape($memo) : 'NULL')
		. ")";
	db_query($sql, 'Could not insert scrap record');
	$scrap_id = db_insert_id();

	// Update bin stock (remove from source bin)
	if ($from_bin_id) {
		$wh_ops_path = dirname(__FILE__) . '/../../includes/warehouse_operations.inc';
		if (file_exists($wh_ops_path)) {
			include_once($wh_ops_path);
		} else {
			$wh_ops_path2 = dirname(__FILE__) . '/../warehouse_operations.inc';
			if (file_exists($wh_ops_path2))
				include_once($wh_ops_path2);
		}
		if (function_exists('update_bin_stock')) {
			update_bin_stock($from_bin_id, $stock_id, -$qty, $batch_id, $serial_id);
		}
	}

	// Update serial status to 'scrapped'
	if ($serial_id) {
		include_once($path_to_root . '/inventory/includes/db/serial_numbers_db.inc');
		if (function_exists('update_serial_status')) {
			update_serial_status($serial_id, 'scrapped', ST_INVADJUST, $adj_id,
				$loc_code, null, $date_, $ref, _('Scrapped: ') . get_scrap_reason_label($reason_code), $stock_move_id);
		}
	}

	// Update batch status to 'scrapped' if entire batch is scrapped
	if ($batch_id) {
		include_once($path_to_root . '/inventory/includes/db/stock_batches_db.inc');
		if (function_exists('update_batch_status')) {
			// Log movement
			if (function_exists('add_batch_movement')) {
				add_batch_movement($batch_id, 'adjust', ST_INVADJUST, $adj_id,
					$loc_code, null, -$qty, _('Scrapped: ') . get_scrap_reason_label($reason_code));
			}
		}
	}

	// Create warehouse operation record
	include_once(dirname(__FILE__) . '/warehouse_operations_db.inc');
	if (function_exists('add_wh_operation')) {
		$op_id = add_wh_operation('scrap', 'done', ST_INVADJUST, $adj_id,
			null, $from_bin_id, $scrap_loc_id, null, null, 5, null, $gl_memo);
		if ($op_id && function_exists('add_wh_operation_line')) {
			add_wh_operation_line($op_id, $stock_id, $qty, $qty,
				$from_bin_id, $scrap_loc_id, $batch_id, $serial_id, null, $unit_cost);
		}
	}

	commit_transaction();
	return $scrap_id;
}

/**
 * Get a single scrap record by ID.
 *
 * @param int $scrap_id Scrap ID
 * @return array|false Row data or false
 */
function get_scrap_entry($scrap_id) {
	$sql = "SELECT s.*, sm.description, sm.units,
		fl.loc_name AS from_loc_name, sl.loc_name AS scrap_loc_name
		FROM " . TB_PREF . "wh_scrap s
		LEFT JOIN " . TB_PREF . "stock_master sm ON sm.stock_id = s.stock_id
		LEFT JOIN " . TB_PREF . "wh_locations fl ON fl.loc_id = s.from_loc_id
		LEFT JOIN " . TB_PREF . "wh_locations sl ON sl.loc_id = s.scrap_loc_id
		WHERE s.scrap_id = " . (int)$scrap_id;
	$result = db_query($sql, 'Could not get scrap entry');
	return db_fetch($result);
}

/**
 * Get scrap entries with optional filters.
 *
 * @param array $filters Associative array of filter params:
 *   'stock_id'    => string  Item code filter
 *   'reason_code' => string  Reason code filter
 *   'date_from'   => string  Start date (display format)
 *   'date_to'     => string  End date (display format)
 *   'loc_code'    => string  Warehouse location code (0_locations)
 *   'offset'      => int     Pagination offset
 *   'limit'       => int     Pagination limit
 * @return resource DB result
 */
function get_scrap_entries($filters = array()) {
	$sql = "SELECT s.*, sm.description, sm.units,
		fl.loc_name AS from_loc_name, sl.loc_name AS scrap_loc_name,
		sn.serial_no, sb.batch_no
		FROM " . TB_PREF . "wh_scrap s
		LEFT JOIN " . TB_PREF . "stock_master sm ON sm.stock_id = s.stock_id
		LEFT JOIN " . TB_PREF . "wh_locations fl ON fl.loc_id = s.from_loc_id
		LEFT JOIN " . TB_PREF . "wh_locations sl ON sl.loc_id = s.scrap_loc_id
		LEFT JOIN " . TB_PREF . "serial_numbers sn ON sn.id = s.serial_id
		LEFT JOIN " . TB_PREF . "stock_batches sb ON sb.id = s.batch_id
		WHERE 1=1";

	if (!empty($filters['stock_id']))
		$sql .= " AND s.stock_id = " . db_escape($filters['stock_id']);
	if (!empty($filters['reason_code']))
		$sql .= " AND s.reason_code = " . db_escape($filters['reason_code']);
	if (!empty($filters['date_from']))
		$sql .= " AND s.scrap_date >= '" . date2sql($filters['date_from']) . "'";
	if (!empty($filters['date_to']))
		$sql .= " AND s.scrap_date <= '" . date2sql($filters['date_to']) . "'";
	if (!empty($filters['loc_code'])) {
		// Match from_loc_id by looking up bins belonging to this warehouse
		$sql .= " AND s.from_loc_id IN (SELECT loc_id FROM " . TB_PREF
			. "wh_locations WHERE warehouse_loc_code = " . db_escape($filters['loc_code']) . ")";
	}

	$sql .= " ORDER BY s.scrap_date DESC, s.scrap_id DESC";

	if (isset($filters['limit'])) {
		$offset = isset($filters['offset']) ? (int)$filters['offset'] : 0;
		$sql .= " LIMIT " . $offset . ", " . (int)$filters['limit'];
	}

	return db_query($sql, 'Could not get scrap entries');
}

/**
 * Count scrap entries matching filters.
 *
 * @param array $filters Same as get_scrap_entries()
 * @return int Count
 */
function count_scrap_entries($filters = array()) {
	$sql = "SELECT COUNT(*) FROM " . TB_PREF . "wh_scrap s WHERE 1=1";

	if (!empty($filters['stock_id']))
		$sql .= " AND s.stock_id = " . db_escape($filters['stock_id']);
	if (!empty($filters['reason_code']))
		$sql .= " AND s.reason_code = " . db_escape($filters['reason_code']);
	if (!empty($filters['date_from']))
		$sql .= " AND s.scrap_date >= '" . date2sql($filters['date_from']) . "'";
	if (!empty($filters['date_to']))
		$sql .= " AND s.scrap_date <= '" . date2sql($filters['date_to']) . "'";

	$result = db_query($sql, 'Could not count scrap entries');
	$row = db_fetch_row($result);
	return (int)$row[0];
}

/**
 * Get scrap summary statistics.
 *
 * @param string|null $loc_code Warehouse location code filter
 * @return array Summary with total_entries, total_qty, total_cost, by_reason breakdown
 */
function get_scrap_summary($loc_code = null) {
	$where = "1=1";
	if ($loc_code) {
		$where = "s.from_loc_id IN (SELECT loc_id FROM " . TB_PREF
			. "wh_locations WHERE warehouse_loc_code = " . db_escape($loc_code) . ")";
	}

	// Overall totals
	$sql = "SELECT COUNT(*) AS total_entries,
		COALESCE(SUM(qty), 0) AS total_qty,
		COALESCE(SUM(total_cost), 0) AS total_cost
		FROM " . TB_PREF . "wh_scrap s WHERE " . $where;
	$result = db_query($sql, 'Could not get scrap summary');
	$summary = db_fetch($result);

	// Breakdown by reason
	$sql = "SELECT reason_code, COUNT(*) AS cnt,
		COALESCE(SUM(qty), 0) AS qty,
		COALESCE(SUM(total_cost), 0) AS cost
		FROM " . TB_PREF . "wh_scrap s
		WHERE " . $where
		. " GROUP BY reason_code ORDER BY cost DESC";
	$result = db_query($sql, 'Could not get scrap reason summary');
	$by_reason = array();
	while ($row = db_fetch($result)) {
		$by_reason[] = $row;
	}
	$summary['by_reason'] = $by_reason;

	return $summary;
}

/**
 * Check if a scrap entry needs approval based on threshold value.
 *
 * The threshold is stored in company preferences as 'scrap_approval_threshold'.
 * If not set, no approval is required.
 *
 * @param float $total_cost Total cost of the scrap entry
 * @return bool True if approval is required
 */
function scrap_needs_approval($total_cost) {
	$threshold = get_company_pref('scrap_approval_threshold');
	if (!$threshold || $threshold <= 0) return false;
	return ($total_cost > $threshold);
}

/**
 * Approve a scrap entry.
 *
 * @param int $scrap_id Scrap ID
 * @return bool Success
 */
function approve_scrap_entry($scrap_id) {
	global $current_user;
	$user_id = $current_user ? $current_user : 0;

	$sql = "UPDATE " . TB_PREF . "wh_scrap SET approved_by = " . (int)$user_id
		. " WHERE scrap_id = " . (int)$scrap_id;
	db_query($sql, 'Could not approve scrap entry');
	return true;
}

/**
 * Get scrap entries pending approval (above threshold, not yet approved).
 *
 * @return resource DB result
 */
function get_scrap_pending_approval() {
	$threshold = get_company_pref('scrap_approval_threshold');
	if (!$threshold || $threshold <= 0) {
		// No threshold set — no entries need approval
		$sql = "SELECT * FROM " . TB_PREF . "wh_scrap WHERE 1=0";
		return db_query($sql, 'no-op');
	}

	$sql = "SELECT s.*, sm.description, sm.units,
		fl.loc_name AS from_loc_name
		FROM " . TB_PREF . "wh_scrap s
		LEFT JOIN " . TB_PREF . "stock_master sm ON sm.stock_id = s.stock_id
		LEFT JOIN " . TB_PREF . "wh_locations fl ON fl.loc_id = s.from_loc_id
		WHERE s.total_cost > " . db_escape($threshold)
		. " AND s.approved_by IS NULL
		ORDER BY s.total_cost DESC";
	return db_query($sql, 'Could not get pending scrap entries');
}

/**
 * Delete a scrap entry. Only deletes the scrap record — does NOT reverse the stock move or GL.
 * Reversals should be done via a separate void/reversal process.
 *
 * @param int $scrap_id Scrap ID
 * @return bool Success
 */
function delete_scrap_entry($scrap_id) {
	$sql = "DELETE FROM " . TB_PREF . "wh_scrap WHERE scrap_id = " . (int)$scrap_id;
	db_query($sql, 'Could not delete scrap entry');
	return true;
}
