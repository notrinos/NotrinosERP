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
 * Warehouse Picking Operations — Wave/Pick CRUD, pick generation, path optimization.
 *
 * Tables: 0_wh_picking_waves, 0_wh_operations (op_type='pick'), 0_wh_operation_lines
 *
 * Picking methods:
 *   - single:  Pick one order at a time
 *   - batch:   Combine multiple orders into one trip
 *   - wave:    Group by zone/aisle, consolidate at packing
 *   - cluster: Pick multiple orders simultaneously, sort during pick
 *
 * Wave lifecycle: draft → released (pick ops generated) → in_progress → done → cancelled
 *
 * @package NotrinosERP
 * @subpackage Inventory/Warehouse
 */

include_once(dirname(__FILE__) . '/warehouse_operations_db.inc');
include_once(dirname(dirname(__FILE__)) . '/removal_engine.inc');

// =====================================================================
// Wave status/type helpers
// =====================================================================

/**
 * Get valid wave statuses.
 *
 * @return array Associative array of status_code => display_label.
 */
function get_wave_statuses() {
	return array(
		'draft'       => _('Draft'),
		'released'    => _('Released'),
		'in_progress' => _('In Progress'),
		'done'        => _('Done'),
		'cancelled'   => _('Cancelled'),
	);
}

/**
 * Get CSS color for a wave status.
 *
 * @param string $status Status code.
 * @return string CSS color hex code.
 */
function get_wave_status_color($status) {
	$colors = array(
		'draft'       => '#6c757d',
		'released'    => '#007bff',
		'in_progress' => '#ffc107',
		'done'        => '#28a745',
		'cancelled'   => '#dc3545',
	);
	return isset($colors[$status]) ? $colors[$status] : '#6c757d';
}

/**
 * Render a colored wave status badge.
 *
 * @param string $status Status code.
 * @return string HTML span with background color.
 */
function wave_status_badge($status) {
	$statuses = get_wave_statuses();
	$label = isset($statuses[$status]) ? $statuses[$status] : $status;
	$color = get_wave_status_color($status);
	return '<span style="background:' . $color . ';color:#fff;padding:2px 8px;border-radius:10px;font-size:11px;">'
		. htmlspecialchars($label) . '</span>';
}

/**
 * Get valid wave types.
 *
 * @return array Associative array of type_code => display_label.
 */
function get_wave_types() {
	return array(
		'standard'  => _('Standard'),
		'rush'      => _('Rush'),
		'backorder' => _('Backorder'),
	);
}

/**
 * Get CSS color for a wave type.
 *
 * @param string $type Type code.
 * @return string CSS color hex code.
 */
function get_wave_type_color($type) {
	$colors = array(
		'standard'  => '#007bff',
		'rush'      => '#dc3545',
		'backorder' => '#fd7e14',
	);
	return isset($colors[$type]) ? $colors[$type] : '#6c757d';
}

/**
 * Get valid picking methods.
 *
 * @return array Associative array of method_code => display_label.
 */
function get_picking_methods() {
	return array(
		'single'  => _('Single Order'),
		'batch'   => _('Batch Pick'),
		'wave'    => _('Wave Pick'),
		'cluster' => _('Cluster Pick'),
	);
}

// =====================================================================
// CRUD: Picking Waves
// =====================================================================

/**
 * Create a new picking wave.
 *
 * @param string $wave_name          Wave display name.
 * @param string $warehouse_loc_code Warehouse location code.
 * @param string $picking_method     Picking method (single/batch/wave/cluster).
 * @param string $wave_type          Wave type (standard/rush/backorder).
 * @param int|null $assigned_to      Assigned worker user ID.
 * @param string $memo               Notes.
 * @return int The inserted wave_id.
 */
function add_picking_wave($wave_name, $warehouse_loc_code, $picking_method = 'single',
	$wave_type = 'standard', $assigned_to = null, $memo = '')
{
	$sql = "INSERT INTO " . TB_PREF . "wh_picking_waves (
		wave_name, warehouse_loc_code, picking_method, wave_type,
		status, assigned_to, memo
	) VALUES (
		" . db_escape($wave_name) . ",
		" . db_escape($warehouse_loc_code) . ",
		" . db_escape($picking_method) . ",
		" . db_escape($wave_type) . ",
		'draft',
		" . ($assigned_to ? (int)$assigned_to : 'NULL') . ",
		" . db_escape($memo) . "
	)";
	db_query($sql, 'could not add picking wave');
	return db_insert_id();
}

/**
 * Update an existing picking wave.
 *
 * @param int    $wave_id            Wave ID.
 * @param string $wave_name          Wave display name.
 * @param string $warehouse_loc_code Warehouse location code.
 * @param string $picking_method     Picking method.
 * @param string $wave_type          Wave type.
 * @param int|null $assigned_to      Assigned worker user ID.
 * @param string $memo               Notes.
 */
function update_picking_wave($wave_id, $wave_name, $warehouse_loc_code, $picking_method,
	$wave_type, $assigned_to = null, $memo = '')
{
	$sql = "UPDATE " . TB_PREF . "wh_picking_waves SET "
		. "wave_name=" . db_escape($wave_name) . ", "
		. "warehouse_loc_code=" . db_escape($warehouse_loc_code) . ", "
		. "picking_method=" . db_escape($picking_method) . ", "
		. "wave_type=" . db_escape($wave_type) . ", "
		. "assigned_to=" . ($assigned_to ? (int)$assigned_to : 'NULL') . ", "
		. "memo=" . db_escape($memo)
		. " WHERE wave_id=" . (int)$wave_id;
	db_query($sql, 'could not update picking wave');
}

/**
 * Get a single picking wave by ID.
 *
 * @param int $wave_id Wave ID.
 * @return array|false The wave row or false.
 */
function get_picking_wave($wave_id) {
	$sql = "SELECT pw.*, l.location_name AS warehouse_name "
		. "FROM " . TB_PREF . "wh_picking_waves pw "
		. "LEFT JOIN " . TB_PREF . "locations l ON pw.warehouse_loc_code = l.loc_code "
		. "WHERE pw.wave_id=" . (int)$wave_id;
	$result = db_query($sql, 'could not get picking wave');
	return db_fetch($result);
}

/**
 * Get picking waves with optional filters.
 *
 * @param array $filters Associative filter array: 'status', 'warehouse_loc_code',
 *                       'picking_method', 'wave_type', 'show_inactive'.
 * @param int   $limit  Max rows.
 * @param int   $offset Offset.
 * @return resource DB result.
 */
function get_picking_waves($filters = array(), $limit = 50, $offset = 0) {
	$where = array("1=1");

	if (!empty($filters['status'])) {
		if (is_array($filters['status'])) {
			$escaped = array_map('db_escape', $filters['status']);
			$where[] = "pw.status IN (" . implode(',', $escaped) . ")";
		} else {
			$where[] = "pw.status=" . db_escape($filters['status']);
		}
	}

	if (!empty($filters['warehouse_loc_code']))
		$where[] = "pw.warehouse_loc_code=" . db_escape($filters['warehouse_loc_code']);

	if (!empty($filters['picking_method']))
		$where[] = "pw.picking_method=" . db_escape($filters['picking_method']);

	if (!empty($filters['wave_type']))
		$where[] = "pw.wave_type=" . db_escape($filters['wave_type']);

	if (empty($filters['show_inactive']))
		$where[] = "pw.status NOT IN ('cancelled')";

	$sql = "SELECT pw.*, l.location_name AS warehouse_name "
		. "FROM " . TB_PREF . "wh_picking_waves pw "
		. "LEFT JOIN " . TB_PREF . "locations l ON pw.warehouse_loc_code = l.loc_code "
		. "WHERE " . implode(' AND ', $where)
		. " ORDER BY FIELD(pw.status, 'in_progress', 'released', 'draft', 'done', 'cancelled'), pw.created_date DESC";

	if ($limit > 0)
		$sql .= " LIMIT " . (int)$offset . ", " . (int)$limit;

	return db_query($sql, 'could not get picking waves');
}

/**
 * Count picking waves matching filters.
 *
 * @param array $filters Same filter keys as get_picking_waves().
 * @return int Count.
 */
function count_picking_waves($filters = array()) {
	$where = array("1=1");

	if (!empty($filters['status'])) {
		if (is_array($filters['status'])) {
			$escaped = array_map('db_escape', $filters['status']);
			$where[] = "status IN (" . implode(',', $escaped) . ")";
		} else {
			$where[] = "status=" . db_escape($filters['status']);
		}
	}
	if (!empty($filters['warehouse_loc_code']))
		$where[] = "warehouse_loc_code=" . db_escape($filters['warehouse_loc_code']);
	if (empty($filters['show_inactive']))
		$where[] = "status NOT IN ('cancelled')";

	$sql = "SELECT COUNT(*) FROM " . TB_PREF . "wh_picking_waves WHERE " . implode(' AND ', $where);
	$result = db_query($sql, 'could not count picking waves');
	$row = db_fetch_row($result);
	return (int)$row[0];
}

/**
 * Check whether a picking wave can be deleted.
 *
 * Can only delete draft waves that have no pick operations.
 *
 * @param int $wave_id Wave ID.
 * @return bool|string True if deletable, or error reason string.
 */
function can_delete_picking_wave($wave_id) {
	$wave = get_picking_wave($wave_id);
	if (!$wave)
		return _('Wave not found.');

	if ($wave['status'] !== 'draft')
		return _('Only draft waves can be deleted.');

	// Check for linked pick operations
	$sql = "SELECT COUNT(*) FROM " . TB_PREF . "wh_operations WHERE wave_id=" . (int)$wave_id;
	$result = db_query($sql, 'could not check wave operations');
	$row = db_fetch_row($result);
	if ((int)$row[0] > 0)
		return _('Cannot delete — this wave has pick operations. Cancel the wave instead.');

	return true;
}

/**
 * Delete a picking wave. Only draft waves with no operations can be deleted.
 *
 * @param int $wave_id Wave ID.
 * @return bool True on success.
 */
function delete_picking_wave($wave_id) {
	$check = can_delete_picking_wave($wave_id);
	if ($check !== true)
		return false;

	$sql = "DELETE FROM " . TB_PREF . "wh_picking_waves WHERE wave_id=" . (int)$wave_id;
	db_query($sql, 'could not delete picking wave');
	return true;
}

// =====================================================================
// Wave lifecycle actions
// =====================================================================

/**
 * Release a wave: generate pick operations from pending delivery orders.
 *
 * For each delivery order line that needs picking, applies the removal engine
 * to determine source bins, creates pick operations with path-optimized lines.
 *
 * @param int $wave_id Wave ID.
 * @return array Result with 'success', 'message', 'operations_created', 'lines_created'.
 */
function release_picking_wave($wave_id) {
	$wave = get_picking_wave($wave_id);
	if (!$wave)
		return array('success' => false, 'message' => _('Wave not found.'));

	if ($wave['status'] !== 'draft')
		return array('success' => false, 'message' => _('Only draft waves can be released.'));

	$warehouse_loc_code = $wave['warehouse_loc_code'];

	// Get pending delivery orders at this warehouse that need picking
	$deliveries = get_pending_deliveries_for_picking($warehouse_loc_code);
	if (empty($deliveries))
		return array('success' => false, 'message' => _('No pending deliveries found for picking at this warehouse.'));

	begin_transaction();

	$operations_created = 0;
	$total_lines = 0;
	$total_orders = 0;
	$all_pick_lines = array();

	foreach ($deliveries as $delivery) {
		$order_lines = get_delivery_lines_for_picking($delivery['trans_no']);
		if (empty($order_lines))
			continue;

		$total_orders++;

		foreach ($order_lines as $line) {
			$stock_id = $line['stock_id'];
			$qty_needed = (float)$line['quantity'];

			if ($qty_needed <= 0)
				continue;

			// Apply removal engine to determine source bins
			$allocations = execute_removal($stock_id, $warehouse_loc_code, $qty_needed);

			foreach ($allocations as $alloc) {
				$all_pick_lines[] = array(
					'delivery_no'   => $delivery['trans_no'],
					'stock_id'      => $stock_id,
					'qty_to_pick'   => $alloc['qty_to_pick'],
					'from_bin_id'   => $alloc['bin_loc_id'],
					'bin_code'      => $alloc['bin_code'],
					'bin_name'      => $alloc['bin_name'],
					'batch_id'      => $alloc['batch_id'],
					'serial_id'     => $alloc['serial_id'],
					'pick_sequence' => $alloc['pick_sequence'],
				);
			}
		}
	}

	if (empty($all_pick_lines)) {
		commit_transaction();
		return array('success' => false, 'message' => _('No items to pick — no bin stock available for the pending deliveries.'));
	}

	// Sort all pick lines by pick_sequence for walking path optimization
	usort($all_pick_lines, function($a, $b) {
		$seq_a = $a['pick_sequence'] !== null ? $a['pick_sequence'] : 999999;
		$seq_b = $b['pick_sequence'] !== null ? $b['pick_sequence'] : 999999;
		if ($seq_a === $seq_b)
			return strcmp($a['bin_code'], $b['bin_code']);
		return $seq_a - $seq_b;
	});

	// Group pick lines by delivery for single-order picking, or create one operation for batch/wave
	$method = $wave['picking_method'];

	if ($method === 'single') {
		// One pick operation per delivery
		$grouped = array();
		foreach ($all_pick_lines as $pl) {
			$grouped[$pl['delivery_no']][] = $pl;
		}

		foreach ($grouped as $delivery_no => $lines) {
			// Sort lines by pick sequence within this order
			usort($lines, function($a, $b) {
				$seq_a = $a['pick_sequence'] !== null ? $a['pick_sequence'] : 999999;
				$seq_b = $b['pick_sequence'] !== null ? $b['pick_sequence'] : 999999;
				if ($seq_a === $seq_b) return strcmp($a['bin_code'], $b['bin_code']);
				return $seq_a - $seq_b;
			});

			$op_id = add_wh_operation(
				'pick', 'ready', ST_CUSTDELIVERY, $delivery_no,
				null, null, null, null, $wave['assigned_to'],
				($wave['wave_type'] === 'rush' ? 1 : 5), null,
				sprintf(_('Wave #%d — Single pick for DN #%d'), $wave_id, $delivery_no)
			);

			// Link operation to wave
			update_operation_wave($op_id, $wave_id);

			foreach ($lines as $pl) {
				add_wh_operation_line(
					$op_id, $pl['stock_id'], $pl['qty_to_pick'], 0,
					$pl['from_bin_id'], null, $pl['batch_id'], $pl['serial_id']
				);
				$total_lines++;
			}

			$operations_created++;
		}

	} else {
		// batch/wave/cluster: One combined pick operation for all deliveries
		$op_id = add_wh_operation(
			'pick', 'ready', null, null,
			null, null, null, null, $wave['assigned_to'],
			($wave['wave_type'] === 'rush' ? 1 : 5), null,
			sprintf(_('Wave #%d — %s pick (%d orders)'), $wave_id, ucfirst($method), $total_orders)
		);

		update_operation_wave($op_id, $wave_id);

		foreach ($all_pick_lines as $pl) {
			add_wh_operation_line(
				$op_id, $pl['stock_id'], $pl['qty_to_pick'], 0,
				$pl['from_bin_id'], null, $pl['batch_id'], $pl['serial_id']
			);
			$total_lines++;
		}

		$operations_created++;
	}

	// Update wave status and counts
	$sql = "UPDATE " . TB_PREF . "wh_picking_waves SET "
		. "status='released', "
		. "released_date=NOW(), "
		. "total_orders=" . (int)$total_orders . ", "
		. "total_lines=" . (int)$total_lines
		. " WHERE wave_id=" . (int)$wave_id;
	db_query($sql, 'could not update wave to released');

	commit_transaction();

	return array(
		'success' => true,
		'message' => sprintf(
			_('Wave released: %d pick operation(s) created with %d line(s) across %d order(s).'),
			$operations_created, $total_lines, $total_orders
		),
		'operations_created' => $operations_created,
		'lines_created' => $total_lines,
	);
}

/**
 * Complete a wave: mark all pick operations as done, update wave status.
 *
 * @param int $wave_id Wave ID.
 * @return bool True on success.
 */
function complete_picking_wave($wave_id) {
	$wave = get_picking_wave($wave_id);
	if (!$wave)
		return false;

	if (!in_array($wave['status'], array('released', 'in_progress')))
		return false;

	begin_transaction();

	// Complete all non-cancelled pick operations in this wave
	$sql = "SELECT op_id FROM " . TB_PREF . "wh_operations "
		. "WHERE wave_id=" . (int)$wave_id . " AND op_status <> 'cancelled' AND op_status <> 'done'";
	$result = db_query($sql, 'could not get wave operations');
	while ($row = db_fetch($result)) {
		complete_wh_operation($row['op_id']);
	}

	// Update wave status
	$sql = "UPDATE " . TB_PREF . "wh_picking_waves SET "
		. "status='done', completed_date=NOW() "
		. "WHERE wave_id=" . (int)$wave_id;
	db_query($sql, 'could not complete wave');

	commit_transaction();
	return true;
}

/**
 * Cancel a wave: cancel all pick operations and mark wave as cancelled.
 *
 * @param int $wave_id Wave ID.
 * @return bool True on success.
 */
function cancel_picking_wave($wave_id) {
	$wave = get_picking_wave($wave_id);
	if (!$wave)
		return false;

	if ($wave['status'] === 'done' || $wave['status'] === 'cancelled')
		return false;

	begin_transaction();

	// Cancel all non-completed operations in this wave
	$sql = "SELECT op_id FROM " . TB_PREF . "wh_operations "
		. "WHERE wave_id=" . (int)$wave_id . " AND op_status <> 'done' AND op_status <> 'cancelled'";
	$result = db_query($sql, 'could not get wave operations');
	while ($row = db_fetch($result)) {
		cancel_wh_operation($row['op_id']);
	}

	$sql = "UPDATE " . TB_PREF . "wh_picking_waves SET status='cancelled' "
		. "WHERE wave_id=" . (int)$wave_id;
	db_query($sql, 'could not cancel wave');

	commit_transaction();
	return true;
}

/**
 * Start a wave: transition from released to in_progress.
 *
 * @param int $wave_id Wave ID.
 * @return bool True on success.
 */
function start_picking_wave($wave_id) {
	$wave = get_picking_wave($wave_id);
	if (!$wave || $wave['status'] !== 'released')
		return false;

	$sql = "UPDATE " . TB_PREF . "wh_picking_waves SET status='in_progress' "
		. "WHERE wave_id=" . (int)$wave_id;
	db_query($sql, 'could not start wave');

	// Start all ready operations in this wave
	$sql = "SELECT op_id FROM " . TB_PREF . "wh_operations "
		. "WHERE wave_id=" . (int)$wave_id . " AND op_status='ready'";
	$result = db_query($sql, 'could not get wave operations');
	while ($row = db_fetch($result)) {
		update_wh_operation_status($row['op_id'], 'in_progress');
	}

	return true;
}

// =====================================================================
// Operation-wave linking helper
// =====================================================================

/**
 * Link an operation to a wave by setting wave_id on the operation.
 *
 * @param int $op_id   Operation ID.
 * @param int $wave_id Wave ID.
 */
function update_operation_wave($op_id, $wave_id) {
	$sql = "UPDATE " . TB_PREF . "wh_operations SET wave_id=" . (int)$wave_id
		. " WHERE op_id=" . (int)$op_id;
	db_query($sql, 'could not set wave_id on operation');
}

// =====================================================================
// Delivery queries for picking
// =====================================================================

/**
 * Get pending delivery orders that need picking at a warehouse.
 *
 * A delivery is "pending picking" if it has been created (type 13 = ST_CUSTDELIVERY)
 * and no completed pick operation exists for it yet.
 *
 * @param string $warehouse_loc_code Warehouse location code.
 * @return array Array of delivery rows.
 */
function get_pending_deliveries_for_picking($warehouse_loc_code) {
	$sql = "SELECT dt.trans_no, dt.reference, dt.tran_date, dt.debtor_no, dt.branch_code, "
		. "d.name AS customer_name "
		. "FROM " . TB_PREF . "debtor_trans dt "
		. "INNER JOIN " . TB_PREF . "debtors_master d ON dt.debtor_no = d.debtor_no "
		. "WHERE dt.type = " . ST_CUSTDELIVERY
		. " AND dt.trans_no NOT IN ("
		. "  SELECT DISTINCT source_doc_no FROM " . TB_PREF . "wh_operations "
		. "  WHERE source_doc_type = " . ST_CUSTDELIVERY
		. "    AND op_type = 'pick' AND op_status IN ('ready', 'in_progress', 'done')"
		. ")"
		. " ORDER BY dt.tran_date ASC, dt.trans_no ASC";
	$result = db_query($sql, 'could not get pending deliveries');
	$rows = array();
	while ($row = db_fetch($result))
		$rows[] = $row;
	return $rows;
}

/**
 * Get line items for a delivery order that need picking.
 *
 * @param int $delivery_no Delivery (dispatch) number.
 * @return array Array of line item rows.
 */
function get_delivery_lines_for_picking($delivery_no) {
	$sql = "SELECT dtd.id, dtd.stock_id, dtd.quantity, dtd.unit_price, "
		. "sm.description AS item_description, sm.units, sm.track_by "
		. "FROM " . TB_PREF . "debtor_trans_details dtd "
		. "INNER JOIN " . TB_PREF . "stock_master sm ON dtd.stock_id = sm.stock_id "
		. "WHERE dtd.debtor_trans_no = " . (int)$delivery_no
		. " AND dtd.debtor_trans_type = " . ST_CUSTDELIVERY
		. " AND dtd.quantity > 0 "
		. " AND sm.mb_flag <> 'D'"
		. " ORDER BY dtd.id";
	$result = db_query($sql, 'could not get delivery lines for picking');
	$rows = array();
	while ($row = db_fetch($result))
		$rows[] = $row;
	return $rows;
}

// =====================================================================
// Wave pick operations queries
// =====================================================================

/**
 * Get pick operations belonging to a wave.
 *
 * @param int $wave_id Wave ID.
 * @return resource DB result.
 */
function get_wave_pick_operations($wave_id) {
	$sql = "SELECT op.*, "
		. "fl.loc_name AS from_loc_name, "
		. "tl.loc_name AS to_loc_name "
		. "FROM " . TB_PREF . "wh_operations op "
		. "LEFT JOIN " . TB_PREF . "wh_locations fl ON op.from_loc_id = fl.loc_id "
		. "LEFT JOIN " . TB_PREF . "wh_locations tl ON op.to_loc_id = tl.loc_id "
		. "WHERE op.wave_id=" . (int)$wave_id
		. " ORDER BY op.op_id ASC";
	return db_query($sql, 'could not get wave pick operations');
}

/**
 * Get wave progress: count operations by status.
 *
 * @param int $wave_id Wave ID.
 * @return array ['total', 'done', 'in_progress', 'ready', 'cancelled', 'pct_complete'].
 */
function get_wave_progress($wave_id) {
	$sql = "SELECT "
		. "COUNT(*) AS total, "
		. "SUM(CASE WHEN op_status='done' THEN 1 ELSE 0 END) AS done, "
		. "SUM(CASE WHEN op_status='in_progress' THEN 1 ELSE 0 END) AS in_progress, "
		. "SUM(CASE WHEN op_status='ready' THEN 1 ELSE 0 END) AS ready, "
		. "SUM(CASE WHEN op_status='cancelled' THEN 1 ELSE 0 END) AS cancelled "
		. "FROM " . TB_PREF . "wh_operations "
		. "WHERE wave_id=" . (int)$wave_id;
	$result = db_query($sql, 'could not get wave progress');
	$row = db_fetch($result);
	if (!$row)
		return array('total' => 0, 'done' => 0, 'in_progress' => 0, 'ready' => 0, 'cancelled' => 0, 'pct_complete' => 0);

	$total = (int)$row['total'];
	$done = (int)$row['done'];
	$active = $total - (int)$row['cancelled'];
	$pct = $active > 0 ? round(($done / $active) * 100) : 0;

	return array(
		'total' => $total,
		'done' => $done,
		'in_progress' => (int)$row['in_progress'],
		'ready' => (int)$row['ready'],
		'cancelled' => (int)$row['cancelled'],
		'pct_complete' => $pct,
	);
}

/**
 * Get line-level pick progress for a wave (sum of all pick operation lines).
 *
 * @param int $wave_id Wave ID.
 * @return array ['total_planned', 'total_done', 'pct_lines_complete'].
 */
function get_wave_lines_progress($wave_id) {
	$sql = "SELECT "
		. "COALESCE(SUM(ol.qty_planned), 0) AS total_planned, "
		. "COALESCE(SUM(ol.qty_done), 0) AS total_done "
		. "FROM " . TB_PREF . "wh_operation_lines ol "
		. "INNER JOIN " . TB_PREF . "wh_operations op ON ol.op_id = op.op_id "
		. "WHERE op.wave_id=" . (int)$wave_id
		. " AND op.op_status <> 'cancelled'";
	$result = db_query($sql, 'could not get wave lines progress');
	$row = db_fetch($result);
	$planned = (float)$row['total_planned'];
	$done = (float)$row['total_done'];
	$pct = $planned > 0 ? round(($done / $planned) * 100) : 0;

	return array(
		'total_planned' => $planned,
		'total_done' => $done,
		'pct_lines_complete' => $pct,
	);
}

// =====================================================================
// Pick list generation (path-optimized)
// =====================================================================

/**
 * Generate a consolidated, path-optimized pick list for a wave.
 *
 * Returns all pick operation lines, sorted by pick_sequence for efficient
 * warehouse walking. Includes bin location, item, quantity, order reference,
 * and batch/serial info.
 *
 * @param int $wave_id Wave ID.
 * @return array Array of pick list line arrays, sorted by pick_sequence.
 */
function generate_pick_list($wave_id) {
	$sql = "SELECT ol.line_id, ol.op_id, ol.stock_id, ol.qty_planned, ol.qty_done, "
		. "ol.from_loc_id, ol.to_loc_id, ol.batch_id, ol.serial_id, "
		. "sm.description AS item_description, sm.units, "
		. "wl.loc_code AS bin_code, wl.loc_name AS bin_name, wl.pick_sequence, "
		. "wl.zone_type, wl.location_path, "
		. "op.source_doc_no AS delivery_no, op.op_status, op.memo, "
		. "sn.serial_no, sb.batch_no, sb.expiry_date AS batch_expiry "
		. "FROM " . TB_PREF . "wh_operation_lines ol "
		. "INNER JOIN " . TB_PREF . "wh_operations op ON ol.op_id = op.op_id "
		. "INNER JOIN " . TB_PREF . "stock_master sm ON ol.stock_id = sm.stock_id "
		. "LEFT JOIN " . TB_PREF . "wh_locations wl ON ol.from_loc_id = wl.loc_id "
		. "LEFT JOIN " . TB_PREF . "serial_numbers sn ON ol.serial_id = sn.id "
		. "LEFT JOIN " . TB_PREF . "stock_batches sb ON ol.batch_id = sb.id "
		. "WHERE op.wave_id=" . (int)$wave_id
		. " AND op.op_status <> 'cancelled'"
		. " ORDER BY COALESCE(wl.pick_sequence, 999999) ASC, wl.loc_code ASC, ol.stock_id ASC, ol.line_id ASC";
	$result = db_query($sql, 'could not generate pick list');
	$lines = array();
	while ($row = db_fetch($result))
		$lines[] = $row;
	return $lines;
}

// =====================================================================
// Short-pick handling
// =====================================================================

/**
 * Confirm a pick line (full or partial).
 *
 * If qty_picked < qty_planned, this is a "short pick". The function records
 * the actual picked quantity and returns shortfall info.
 *
 * @param int   $line_id    Operation line ID.
 * @param float $qty_picked Actual quantity picked.
 * @return array Result: ['success', 'shortfall', 'alternate_bins'].
 */
function confirm_pick_line($line_id, $qty_picked) {
	$sql = "SELECT ol.*, op.wave_id, op.source_doc_no, "
		. "wl.warehouse_loc_code, wl.loc_code AS bin_code "
		. "FROM " . TB_PREF . "wh_operation_lines ol "
		. "INNER JOIN " . TB_PREF . "wh_operations op ON ol.op_id = op.op_id "
		. "LEFT JOIN " . TB_PREF . "wh_locations wl ON ol.from_loc_id = wl.loc_id "
		. "WHERE ol.line_id=" . (int)$line_id;
	$result = db_query($sql, 'could not get pick line');
	$line = db_fetch($result);

	if (!$line)
		return array('success' => false, 'message' => _('Pick line not found.'));

	$planned = (float)$line['qty_planned'];
	$picked = min((float)$qty_picked, $planned);
	$shortfall = $planned - $picked;

	// Update the line's qty_done
	update_wh_operation_line_done($line_id, $picked);

	$alternate_bins = array();

	// If short pick, suggest alternate bins with available stock
	if ($shortfall > 0 && !empty($line['warehouse_loc_code'])) {
		$alt_sql = "SELECT bs.wh_loc_id, bs.qty_available, "
			. "wl.loc_code AS bin_code, wl.loc_name AS bin_name, wl.pick_sequence "
			. "FROM " . TB_PREF . "wh_bin_stock bs "
			. "INNER JOIN " . TB_PREF . "wh_locations wl ON bs.wh_loc_id = wl.loc_id "
			. "WHERE bs.stock_id = " . db_escape($line['stock_id'])
			. " AND wl.warehouse_loc_code = " . db_escape($line['warehouse_loc_code'])
			. " AND bs.wh_loc_id <> " . (int)$line['from_loc_id']
			. " AND bs.stock_status = 'available'"
			. " AND bs.qty_available > 0"
			. " ORDER BY wl.pick_sequence ASC"
			. " LIMIT 5";
		$alt_result = db_query($alt_sql, 'could not find alternate bins');
		while ($alt = db_fetch($alt_result)) {
			$alternate_bins[] = array(
				'bin_loc_id'   => (int)$alt['wh_loc_id'],
				'bin_code'     => $alt['bin_code'],
				'bin_name'     => $alt['bin_name'],
				'qty_available'=> (float)$alt['qty_available'],
				'pick_sequence'=> (int)$alt['pick_sequence'],
			);
		}
	}

	return array(
		'success'        => true,
		'qty_picked'     => $picked,
		'shortfall'      => $shortfall,
		'alternate_bins' => $alternate_bins,
		'message'        => $shortfall > 0
			? sprintf(_('Short pick: %.4f of %.4f picked. %.4f remaining.'), $picked, $planned, $shortfall)
				. (!empty($alternate_bins) ? ' ' . sprintf(_('%d alternate bin(s) available.'), count($alternate_bins)) : '')
			: sprintf(_('Pick confirmed: %.4f picked.'), $picked),
	);
}

// =====================================================================
// Wave summary for status display
// =====================================================================

/**
 * Get count summary of waves by status.
 *
 * @param string|null $warehouse_loc_code Optional warehouse filter.
 * @return array ['draft', 'released', 'in_progress', 'done', 'total_active'].
 */
function get_wave_status_summary($warehouse_loc_code = null) {
	$where = "1=1";
	if ($warehouse_loc_code)
		$where .= " AND warehouse_loc_code=" . db_escape($warehouse_loc_code);

	$sql = "SELECT "
		. "SUM(CASE WHEN status='draft' THEN 1 ELSE 0 END) AS draft, "
		. "SUM(CASE WHEN status='released' THEN 1 ELSE 0 END) AS released, "
		. "SUM(CASE WHEN status='in_progress' THEN 1 ELSE 0 END) AS in_progress, "
		. "SUM(CASE WHEN status='done' THEN 1 ELSE 0 END) AS done "
		. "FROM " . TB_PREF . "wh_picking_waves "
		. "WHERE " . $where;
	$result = db_query($sql, 'could not get wave status summary');
	$row = db_fetch($result);
	if (!$row)
		return array('draft' => 0, 'released' => 0, 'in_progress' => 0, 'done' => 0, 'total_active' => 0);

	$draft = (int)$row['draft'];
	$released = (int)$row['released'];
	$in_progress = (int)$row['in_progress'];

	return array(
		'draft'        => $draft,
		'released'     => $released,
		'in_progress'  => $in_progress,
		'done'         => (int)$row['done'],
		'total_active' => $draft + $released + $in_progress,
	);
}
