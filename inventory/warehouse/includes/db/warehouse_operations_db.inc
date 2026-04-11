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
 * Warehouse Operations CRUD, queries, and status management.
 *
 * Tables: 0_wh_operations, 0_wh_operation_lines
 */

//----------------------------------------------------------------------
// Operation Type & Status Constants
//----------------------------------------------------------------------

/**
 * Get valid operation types.
 *
 * @return array Associative array of type_code => display_label
 */
function get_wh_operation_types() {
	return array(
		'receipt'        => _('Receipt'),
		'quality_check'  => _('Quality Check'),
		'putaway'        => _('Putaway'),
		'pick'           => _('Pick'),
		'pack'           => _('Pack'),
		'ship'           => _('Ship'),
		'transfer'       => _('Transfer'),
		'adjustment'     => _('Adjustment'),
		'scrap'          => _('Scrap'),
		'return'         => _('Return'),
	);
}

/**
 * Get valid operation statuses.
 *
 * @return array Associative array of status_code => display_label
 */
function get_wh_operation_statuses() {
	return array(
		'draft'       => _('Draft'),
		'ready'       => _('Ready'),
		'in_progress' => _('In Progress'),
		'done'        => _('Done'),
		'cancelled'   => _('Cancelled'),
	);
}

/**
 * Get a CSS color for an operation status.
 *
 * @param string $status Status code
 * @return string CSS color
 */
function get_wh_operation_status_color($status) {
	$colors = array(
		'draft'       => '#6c757d',
		'ready'       => '#007bff',
		'in_progress' => '#ffc107',
		'done'        => '#28a745',
		'cancelled'   => '#dc3545',
	);
	return isset($colors[$status]) ? $colors[$status] : '#6c757d';
}

/**
 * Get a CSS color for an operation type.
 *
 * @param string $type Operation type code
 * @return string CSS color
 */
function get_wh_operation_type_color($type) {
	$colors = array(
		'receipt'        => '#28a745',
		'quality_check'  => '#17a2b8',
		'putaway'        => '#007bff',
		'pick'           => '#ffc107',
		'pack'           => '#fd7e14',
		'ship'           => '#6f42c1',
		'transfer'       => '#20c997',
		'adjustment'     => '#6c757d',
		'scrap'          => '#dc3545',
		'return'         => '#e83e8c',
	);
	return isset($colors[$type]) ? $colors[$type] : '#6c757d';
}

//----------------------------------------------------------------------
// CRUD: Operations
//----------------------------------------------------------------------

/**
 * Create a new warehouse operation.
 *
 * @param string   $op_type        Operation type (receipt, putaway, etc.)
 * @param string   $op_status      Initial status (default: 'ready')
 * @param int|null $source_doc_type Source document type (ST_SUPPRECEIVE, etc.)
 * @param int|null $source_doc_no  Source document number
 * @param int|null $parent_op_id   Parent operation (for multi-step chains)
 * @param int|null $from_loc_id    Source bin/location
 * @param int|null $to_loc_id      Destination bin/location
 * @param string|null $scheduled_date SQL datetime
 * @param int|null $assigned_to    Worker user id
 * @param int      $priority       Priority 1-9
 * @param int|null $route_id       Route id
 * @param string   $memo           Notes
 * @return int     The inserted op_id
 */
function add_wh_operation($op_type, $op_status = 'ready', $source_doc_type = null,
	$source_doc_no = null, $parent_op_id = null, $from_loc_id = null,
	$to_loc_id = null, $scheduled_date = null, $assigned_to = null,
	$priority = 5, $route_id = null, $memo = '')
{
	$sql = "INSERT INTO " . TB_PREF . "wh_operations (
		op_type, op_status, source_doc_type, source_doc_no,
		parent_op_id, from_loc_id, to_loc_id,
		scheduled_date, assigned_to, priority, route_id, memo
	) VALUES (
		" . db_escape($op_type) . ",
		" . db_escape($op_status) . ",
		" . ($source_doc_type !== null ? (int)$source_doc_type : 'NULL') . ",
		" . ($source_doc_no !== null ? (int)$source_doc_no : 'NULL') . ",
		" . ($parent_op_id !== null ? (int)$parent_op_id : 'NULL') . ",
		" . ($from_loc_id !== null ? (int)$from_loc_id : 'NULL') . ",
		" . ($to_loc_id !== null ? (int)$to_loc_id : 'NULL') . ",
		" . ($scheduled_date !== null ? db_escape($scheduled_date) : 'NULL') . ",
		" . ($assigned_to !== null ? (int)$assigned_to : 'NULL') . ",
		" . (int)$priority . ",
		" . ($route_id !== null ? (int)$route_id : 'NULL') . ",
		" . db_escape($memo) . "
	)";
	db_query($sql, 'could not add warehouse operation');
	return db_insert_id();
}

/**
 * Add a line item to a warehouse operation.
 *
 * @param int       $op_id       Operation id
 * @param string    $stock_id    Item code
 * @param float     $qty_planned Planned quantity
 * @param float     $qty_done    Done quantity (default 0)
 * @param int|null  $from_loc_id Source bin
 * @param int|null  $to_loc_id   Destination bin
 * @param int|null  $batch_id    Batch FK
 * @param int|null  $serial_id   Serial FK
 * @param int|null  $package_id  Package FK
 * @param float     $unit_cost   Unit cost
 * @return int      The inserted line_id
 */
function add_wh_operation_line($op_id, $stock_id, $qty_planned, $qty_done = 0,
	$from_loc_id = null, $to_loc_id = null, $batch_id = null,
	$serial_id = null, $package_id = null, $unit_cost = 0)
{
	$sql = "INSERT INTO " . TB_PREF . "wh_operation_lines (
		op_id, stock_id, qty_planned, qty_done,
		from_loc_id, to_loc_id, batch_id, serial_id, package_id, unit_cost
	) VALUES (
		" . (int)$op_id . ",
		" . db_escape($stock_id) . ",
		" . (float)$qty_planned . ",
		" . (float)$qty_done . ",
		" . ($from_loc_id !== null ? (int)$from_loc_id : 'NULL') . ",
		" . ($to_loc_id !== null ? (int)$to_loc_id : 'NULL') . ",
		" . ($batch_id !== null ? (int)$batch_id : 'NULL') . ",
		" . ($serial_id !== null ? (int)$serial_id : 'NULL') . ",
		" . ($package_id !== null ? (int)$package_id : 'NULL') . ",
		" . (float)$unit_cost . "
	)";
	db_query($sql, 'could not add warehouse operation line');
	return db_insert_id();
}

/**
 * Update operation status.
 *
 * @param int    $op_id     Operation id
 * @param string $new_status New status
 */
function update_wh_operation_status($op_id, $new_status) {
	$sets = array("op_status=" . db_escape($new_status));

	if ($new_status === 'in_progress')
		$sets[] = "started_at=NOW()";
	elseif ($new_status === 'done')
		$sets[] = "completed_at=NOW()";

	$sql = "UPDATE " . TB_PREF . "wh_operations SET "
		. implode(', ', $sets)
		. " WHERE op_id=" . (int)$op_id;
	db_query($sql, 'could not update operation status');
}

/**
 * Link two operations as parent/child (chained steps).
 *
 * @param int $parent_op_id Parent op id
 * @param int $child_op_id  Child op id
 */
function link_wh_operations($parent_op_id, $child_op_id) {
	$sql = "UPDATE " . TB_PREF . "wh_operations SET next_op_id="
		. (int)$child_op_id . " WHERE op_id=" . (int)$parent_op_id;
	db_query($sql, 'could not link parent operation');
}

/**
 * Update operation line qty_done.
 *
 * @param int   $line_id Line id
 * @param float $qty_done New done quantity
 */
function update_wh_operation_line_done($line_id, $qty_done) {
	$sql = "UPDATE " . TB_PREF . "wh_operation_lines SET qty_done="
		. (float)$qty_done . " WHERE line_id=" . (int)$line_id;
	db_query($sql, 'could not update operation line done qty');
}

/**
 * Complete an operation: set all lines qty_done = qty_planned, status = done.
 *
 * @param int $op_id Operation id
 */
function complete_wh_operation($op_id) {
	$sql = "UPDATE " . TB_PREF . "wh_operation_lines SET qty_done = qty_planned"
		. " WHERE op_id=" . (int)$op_id;
	db_query($sql, 'could not complete operation lines');

	update_wh_operation_status($op_id, 'done');
}

//----------------------------------------------------------------------
// Queries
//----------------------------------------------------------------------

/**
 * Get a single warehouse operation by id.
 *
 * @param int $op_id Operation id
 * @return array|false The operation row
 */
function get_wh_operation($op_id) {
	$sql = "SELECT op.*,
			fl.loc_name AS from_loc_name,
			COALESCE(fl.loc_code, '') AS from_loc_code,
			tl.loc_name AS to_loc_name,
			COALESCE(tl.loc_code, '') AS to_loc_code
		FROM " . TB_PREF . "wh_operations op
		LEFT JOIN " . TB_PREF . "wh_locations fl ON op.from_loc_id = fl.loc_id
		LEFT JOIN " . TB_PREF . "wh_locations tl ON op.to_loc_id = tl.loc_id
		WHERE op.op_id=" . (int)$op_id;
	$result = db_query($sql, 'could not get warehouse operation');
	return db_fetch($result);
}

/**
 * Get operation lines for an operation.
 *
 * @param int $op_id Operation id
 * @return resource DB result
 */
function get_wh_operation_lines($op_id) {
	$sql = "SELECT ol.*,
			sm.description AS item_description,
			sm.units,
			fl.loc_name AS from_bin_name,
			COALESCE(fl.loc_code, '') AS from_bin_code,
			tl.loc_name AS to_bin_name,
			COALESCE(tl.loc_code, '') AS to_bin_code,
			sn.serial_no,
			sb.batch_no
		FROM " . TB_PREF . "wh_operation_lines ol
		LEFT JOIN " . TB_PREF . "stock_master sm ON ol.stock_id = sm.stock_id
		LEFT JOIN " . TB_PREF . "wh_locations fl ON ol.from_loc_id = fl.loc_id
		LEFT JOIN " . TB_PREF . "wh_locations tl ON ol.to_loc_id = tl.loc_id
		LEFT JOIN " . TB_PREF . "serial_numbers sn ON ol.serial_id = sn.id
		LEFT JOIN " . TB_PREF . "stock_batches sb ON ol.batch_id = sb.id
		WHERE ol.op_id=" . (int)$op_id
		. " ORDER BY ol.line_id";
	return db_query($sql, 'could not get operation lines');
}

/**
 * Get operations filtered by various criteria.
 *
 * @param array $filters Associative array of filter criteria:
 *   'op_type', 'op_status', 'source_doc_type', 'source_doc_no',
 *   'warehouse_loc_code', 'from_date', 'to_date', 'assigned_to'
 * @param int   $limit   Max rows (0 = unlimited)
 * @param int   $offset  Row offset
 * @return resource DB result
 */
function get_wh_operations_filtered($filters = array(), $limit = 50, $offset = 0) {
	$where = array("1=1");

	if (!empty($filters['op_type'])) {
		if (is_array($filters['op_type'])) {
			$escaped = array_map('db_escape', $filters['op_type']);
			$where[] = "op.op_type IN (" . implode(',', $escaped) . ")";
		} else {
			$where[] = "op.op_type=" . db_escape($filters['op_type']);
		}
	}

	if (!empty($filters['op_status'])) {
		if (is_array($filters['op_status'])) {
			$escaped = array_map('db_escape', $filters['op_status']);
			$where[] = "op.op_status IN (" . implode(',', $escaped) . ")";
		} else {
			$where[] = "op.op_status=" . db_escape($filters['op_status']);
		}
	}

	if (!empty($filters['source_doc_type']))
		$where[] = "op.source_doc_type=" . db_escape($filters['source_doc_type']);

	if (!empty($filters['source_doc_no']))
		$where[] = "op.source_doc_no=" . db_escape($filters['source_doc_no']);

	if (!empty($filters['assigned_to']))
		$where[] = "op.assigned_to=" . (int)$filters['assigned_to'];

	if (!empty($filters['from_date']))
		$where[] = "op.created_at >= " . db_escape($filters['from_date'] . ' 00:00:00');

	if (!empty($filters['to_date']))
		$where[] = "op.created_at <= " . db_escape($filters['to_date'] . ' 23:59:59');

	// Filter by warehouse: operations where from/to bin belongs to a specific warehouse
	if (!empty($filters['warehouse_loc_code'])) {
		$where[] = "(op.from_loc_id IN (SELECT loc_id FROM " . TB_PREF . "wh_locations WHERE warehouse_loc_code=" . db_escape($filters['warehouse_loc_code']) . ")"
			. " OR op.to_loc_id IN (SELECT loc_id FROM " . TB_PREF . "wh_locations WHERE warehouse_loc_code=" . db_escape($filters['warehouse_loc_code']) . "))";
	}

	// Exclude cancelled by default unless explicitly requested
	if (!isset($filters['include_cancelled']) || !$filters['include_cancelled'])
		$where[] = "op.op_status <> 'cancelled'";

	$sql = "SELECT op.*,
			fl.loc_name AS from_loc_name,
			tl.loc_name AS to_loc_name
		FROM " . TB_PREF . "wh_operations op
		LEFT JOIN " . TB_PREF . "wh_locations fl ON op.from_loc_id = fl.loc_id
		LEFT JOIN " . TB_PREF . "wh_locations tl ON op.to_loc_id = tl.loc_id
		WHERE " . implode(' AND ', $where)
		. " ORDER BY op.priority ASC, op.created_at DESC";

	if ($limit > 0)
		$sql .= " LIMIT " . (int)$offset . ", " . (int)$limit;

	return db_query($sql, 'could not get warehouse operations');
}

/**
 * Count operations matching filters.
 *
 * @param array $filters Same as get_wh_operations_filtered()
 * @return int
 */
function count_wh_operations_filtered($filters = array()) {
	$where = array("1=1");

	if (!empty($filters['op_type'])) {
		if (is_array($filters['op_type'])) {
			$escaped = array_map('db_escape', $filters['op_type']);
			$where[] = "op_type IN (" . implode(',', $escaped) . ")";
		} else {
			$where[] = "op_type=" . db_escape($filters['op_type']);
		}
	}
	if (!empty($filters['op_status'])) {
		if (is_array($filters['op_status'])) {
			$escaped = array_map('db_escape', $filters['op_status']);
			$where[] = "op_status IN (" . implode(',', $escaped) . ")";
		} else {
			$where[] = "op_status=" . db_escape($filters['op_status']);
		}
	}
	if (!empty($filters['source_doc_type']))
		$where[] = "source_doc_type=" . db_escape($filters['source_doc_type']);
	if (!isset($filters['include_cancelled']) || !$filters['include_cancelled'])
		$where[] = "op_status <> 'cancelled'";

	$sql = "SELECT COUNT(*) FROM " . TB_PREF . "wh_operations WHERE " . implode(' AND ', $where);
	$result = db_query($sql, 'could not count operations');
	$row = db_fetch_row($result);
	return (int)$row[0];
}

/**
 * Get operations for a source document (e.g., all ops for a GRN).
 *
 * @param int $doc_type Source document type
 * @param int $doc_no   Source document number
 * @return resource DB result
 */
function get_wh_operations_for_document($doc_type, $doc_no) {
	$sql = "SELECT op.*,
			fl.loc_name AS from_loc_name,
			tl.loc_name AS to_loc_name
		FROM " . TB_PREF . "wh_operations op
		LEFT JOIN " . TB_PREF . "wh_locations fl ON op.from_loc_id = fl.loc_id
		LEFT JOIN " . TB_PREF . "wh_locations tl ON op.to_loc_id = tl.loc_id
		WHERE op.source_doc_type=" . (int)$doc_type
		. " AND op.source_doc_no=" . (int)$doc_no
		. " ORDER BY op.op_id";
	return db_query($sql, 'could not get operations for document');
}

/**
 * Get the operation chain starting from a given operation.
 * Follows next_op_id links.
 *
 * @param int $start_op_id Starting operation id
 * @return array Array of operation rows in chain order
 */
function get_wh_operation_chain($start_op_id) {
	$chain = array();
	$current_id = $start_op_id;
	$safety = 0;

	while ($current_id && $safety < 10) {
		$op = get_wh_operation($current_id);
		if (!$op) break;
		$chain[] = $op;
		$current_id = $op['next_op_id'];
		$safety++;
	}

	return $chain;
}

/**
 * Get pending inbound operations (receipt, quality_check, putaway) that need action.
 *
 * @param string|null $warehouse_loc_code Filter by warehouse (null = all)
 * @param int         $limit             Max rows
 * @return resource DB result
 */
function get_pending_inbound_operations($warehouse_loc_code = null, $limit = 50) {
	$where = array(
		"op.op_type IN ('receipt', 'quality_check', 'putaway')",
		"op.op_status IN ('ready', 'in_progress')"
	);

	if ($warehouse_loc_code) {
		$where[] = "(op.from_loc_id IN (SELECT loc_id FROM " . TB_PREF . "wh_locations WHERE warehouse_loc_code=" . db_escape($warehouse_loc_code) . ")"
			. " OR op.to_loc_id IN (SELECT loc_id FROM " . TB_PREF . "wh_locations WHERE warehouse_loc_code=" . db_escape($warehouse_loc_code) . "))";
	}

	$sql = "SELECT op.*,
			fl.loc_name AS from_loc_name,
			tl.loc_name AS to_loc_name,
			COUNT(ol.line_id) AS line_count,
			SUM(ol.qty_planned) AS total_qty_planned,
			SUM(ol.qty_done) AS total_qty_done
		FROM " . TB_PREF . "wh_operations op
		LEFT JOIN " . TB_PREF . "wh_locations fl ON op.from_loc_id = fl.loc_id
		LEFT JOIN " . TB_PREF . "wh_locations tl ON op.to_loc_id = tl.loc_id
		LEFT JOIN " . TB_PREF . "wh_operation_lines ol ON op.op_id = ol.op_id
		WHERE " . implode(' AND ', $where)
		. " GROUP BY op.op_id"
		. " ORDER BY op.priority ASC, op.created_at ASC"
		. " LIMIT " . (int)$limit;
	return db_query($sql, 'could not get pending inbound operations');
}

/**
 * Get summary counts for inbound operations by status.
 *
 * @param string|null $warehouse_loc_code Filter by warehouse
 * @return array Associative: status => count
 */
function get_inbound_operation_summary($warehouse_loc_code = null) {
	$where = array("op_type IN ('receipt', 'quality_check', 'putaway')");

	if ($warehouse_loc_code) {
		$where[] = "(from_loc_id IN (SELECT loc_id FROM " . TB_PREF . "wh_locations WHERE warehouse_loc_code=" . db_escape($warehouse_loc_code) . ")"
			. " OR to_loc_id IN (SELECT loc_id FROM " . TB_PREF . "wh_locations WHERE warehouse_loc_code=" . db_escape($warehouse_loc_code) . "))";
	}

	$sql = "SELECT op_status, op_type, COUNT(*) AS cnt
		FROM " . TB_PREF . "wh_operations
		WHERE " . implode(' AND ', $where)
		. " GROUP BY op_status, op_type";
	$result = db_query($sql, 'could not get inbound summary');

	$summary = array();
	while ($row = db_fetch($result)) {
		$key = $row['op_type'] . '_' . $row['op_status'];
		$summary[$key] = (int)$row['cnt'];
		if (!isset($summary[$row['op_status']]))
			$summary[$row['op_status']] = 0;
		$summary[$row['op_status']] += (int)$row['cnt'];
	}
	return $summary;
}

/**
 * Cancel a warehouse operation and all its chained descendants.
 *
 * @param int    $op_id  Operation id
 * @param string $reason Cancellation reason
 */
function cancel_wh_operation($op_id, $reason = '') {
	$op = get_wh_operation($op_id);
	if (!$op) return;

	// Can only cancel non-done operations
	if ($op['op_status'] === 'done') return;

	$memo = $op['memo'];
	if ($reason)
		$memo .= ($memo ? "\n" : '') . _('Cancelled: ') . $reason;

	$sql = "UPDATE " . TB_PREF . "wh_operations SET op_status='cancelled', memo="
		. db_escape($memo) . " WHERE op_id=" . (int)$op_id;
	db_query($sql, 'could not cancel operation');

	// Cancel chained next operations
	if ($op['next_op_id'])
		cancel_wh_operation($op['next_op_id'], $reason);
}

/**
 * Delete all operations for a source document (used when voiding).
 *
 * @param int $doc_type Source document type
 * @param int $doc_no   Source document number
 */
function void_wh_operations_for_document($doc_type, $doc_no) {
	// Delete operation lines first (CASCADE should handle it, but be explicit)
	$sql = "DELETE ol FROM " . TB_PREF . "wh_operation_lines ol
		INNER JOIN " . TB_PREF . "wh_operations op ON ol.op_id = op.op_id
		WHERE op.source_doc_type=" . (int)$doc_type
		. " AND op.source_doc_no=" . (int)$doc_no;
	db_query($sql, 'could not void operation lines');

	$sql = "DELETE FROM " . TB_PREF . "wh_operations
		WHERE source_doc_type=" . (int)$doc_type
		. " AND source_doc_no=" . (int)$doc_no;
	db_query($sql, 'could not void operations');
}
