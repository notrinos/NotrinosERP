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
 * Warehouse Packing DB layer — Package CRUD, packing station operations,
 * item assignment, seal/unseal lifecycle.
 *
 * Tables: 0_wh_packages, 0_wh_package_contents, 0_wh_operations (op_type='pack')
 *
 * Package lifecycle: open → sealed → shipped → delivered
 *
 * @package NotrinosERP
 * @subpackage Inventory/Warehouse
 */

include_once(dirname(__FILE__) . '/warehouse_operations_db.inc');

// =====================================================================
// Package Status & Type Helpers
// =====================================================================

/**
 * Get valid package statuses.
 *
 * @return array Associative array of status_code => display_label.
 */
function get_package_statuses() {
	return array(
		'open'      => _('Open'),
		'sealed'    => _('Sealed'),
		'shipped'   => _('Shipped'),
		'delivered' => _('Delivered'),
		'cancelled' => _('Cancelled'),
	);
}

/**
 * Get CSS color for a package status.
 *
 * @param string $status Status code.
 * @return string CSS color hex code.
 */
function get_package_status_color($status) {
	$colors = array(
		'open'      => '#007bff',
		'sealed'    => '#28a745',
		'shipped'   => '#6f42c1',
		'delivered' => '#20c997',
		'cancelled' => '#dc3545',
	);
	return isset($colors[$status]) ? $colors[$status] : '#6c757d';
}

/**
 * Render a colored package status badge.
 *
 * @param string $status Status code.
 * @return string HTML span with background color.
 */
function package_status_badge($status) {
	$statuses = get_package_statuses();
	$label = isset($statuses[$status]) ? $statuses[$status] : $status;
	$color = get_package_status_color($status);
	return '<span style="background:' . $color . ';color:#fff;padding:2px 8px;border-radius:10px;font-size:11px;">'
		. htmlspecialchars($label) . '</span>';
}

/**
 * Get valid package types.
 *
 * @return array Associative array of type_code => display_label.
 */
function get_package_types() {
	return array(
		'box'       => _('Box'),
		'pallet'    => _('Pallet'),
		'envelope'  => _('Envelope'),
		'crate'     => _('Crate'),
		'tube'      => _('Tube'),
		'bag'       => _('Bag'),
		'drum'      => _('Drum'),
		'custom'    => _('Custom'),
	);
}

/**
 * Get CSS color for a package type.
 *
 * @param string $type Package type code.
 * @return string CSS color hex code.
 */
function get_package_type_color($type) {
	$colors = array(
		'box'       => '#007bff',
		'pallet'    => '#fd7e14',
		'envelope'  => '#17a2b8',
		'crate'     => '#6c757d',
		'tube'      => '#20c997',
		'bag'       => '#e83e8c',
		'drum'      => '#795548',
		'custom'    => '#6f42c1',
	);
	return isset($colors[$type]) ? $colors[$type] : '#6c757d';
}

/**
 * Render a colored package type badge.
 *
 * @param string $type Package type code.
 * @return string HTML span with background color.
 */
function package_type_badge($type) {
	$types = get_package_types();
	$label = isset($types[$type]) ? $types[$type] : $type;
	$color = get_package_type_color($type);
	return '<span style="background:' . $color . ';color:#fff;padding:2px 8px;border-radius:10px;font-size:11px;">'
		. htmlspecialchars($label) . '</span>';
}

// =====================================================================
// CRUD: Packages
// =====================================================================

/**
 * Generate a unique package code.
 *
 * Format: PKG-{YYYYMMDD}-{SEQ:4} (e.g., PKG-20260409-0001)
 *
 * @return string Generated package code.
 */
function generate_package_code() {
	$date_part = date('Ymd');
	$prefix = 'PKG-' . $date_part . '-';

	$sql = "SELECT MAX(CAST(SUBSTRING(package_code, " . (strlen($prefix) + 1) . ") AS UNSIGNED)) AS max_seq "
		. "FROM " . TB_PREF . "wh_packages "
		. "WHERE package_code LIKE " . db_escape($prefix . '%');
	$result = db_query($sql, 'could not get max package sequence');
	$row = db_fetch($result);
	$next_seq = ($row && $row['max_seq']) ? (int)$row['max_seq'] + 1 : 1;

	return $prefix . str_pad($next_seq, 4, '0', STR_PAD_LEFT);
}

/**
 * Create a new package.
 *
 * @param string   $package_code  Unique package code (auto-generated if empty).
 * @param string   $package_type  Package type (box/pallet/envelope/etc.).
 * @param int|null $current_loc_id Current bin location.
 * @param float|null $weight      Package weight (kg).
 * @param float|null $length      Package length (cm).
 * @param float|null $width       Package width (cm).
 * @param float|null $height      Package height (cm).
 * @param string   $carrier       Carrier name.
 * @param string   $tracking_number Tracking number.
 * @param string   $shipping_label Shipping label reference.
 * @return int The inserted package_id.
 */
function add_package($package_code = '', $package_type = 'box', $current_loc_id = null,
	$weight = null, $length = null, $width = null, $height = null,
	$carrier = '', $tracking_number = '', $shipping_label = '')
{
	if (empty($package_code))
		$package_code = generate_package_code();

	$sql = "INSERT INTO " . TB_PREF . "wh_packages (
		package_code, package_type, status, current_loc_id,
		weight, length, width, height,
		carrier, tracking_number, shipping_label
	) VALUES (
		" . db_escape($package_code) . ",
		" . db_escape($package_type) . ",
		'open',
		" . ($current_loc_id !== null ? (int)$current_loc_id : 'NULL') . ",
		" . ($weight !== null ? (float)$weight : 'NULL') . ",
		" . ($length !== null ? (float)$length : 'NULL') . ",
		" . ($width !== null ? (float)$width : 'NULL') . ",
		" . ($height !== null ? (float)$height : 'NULL') . ",
		" . db_escape($carrier) . ",
		" . db_escape($tracking_number) . ",
		" . db_escape($shipping_label) . "
	)";
	db_query($sql, 'could not add package');
	return db_insert_id();
}

/**
 * Update an existing package.
 *
 * @param int      $package_id    Package ID.
 * @param string   $package_code  Package code.
 * @param string   $package_type  Package type.
 * @param int|null $current_loc_id Current bin location.
 * @param float|null $weight      Package weight (kg).
 * @param float|null $length      Package length (cm).
 * @param float|null $width       Package width (cm).
 * @param float|null $height      Package height (cm).
 * @param string   $carrier       Carrier name.
 * @param string   $tracking_number Tracking number.
 * @param string   $shipping_label Shipping label reference.
 */
function update_package($package_id, $package_code, $package_type, $current_loc_id = null,
	$weight = null, $length = null, $width = null, $height = null,
	$carrier = '', $tracking_number = '', $shipping_label = '')
{
	$sql = "UPDATE " . TB_PREF . "wh_packages SET "
		. "package_code=" . db_escape($package_code) . ", "
		. "package_type=" . db_escape($package_type) . ", "
		. "current_loc_id=" . ($current_loc_id !== null ? (int)$current_loc_id : 'NULL') . ", "
		. "weight=" . ($weight !== null ? (float)$weight : 'NULL') . ", "
		. "length=" . ($length !== null ? (float)$length : 'NULL') . ", "
		. "width=" . ($width !== null ? (float)$width : 'NULL') . ", "
		. "height=" . ($height !== null ? (float)$height : 'NULL') . ", "
		. "carrier=" . db_escape($carrier) . ", "
		. "tracking_number=" . db_escape($tracking_number) . ", "
		. "shipping_label=" . db_escape($shipping_label)
		. " WHERE package_id=" . (int)$package_id;
	db_query($sql, 'could not update package');
}

/**
 * Get a single package by ID.
 *
 * @param int $package_id Package ID.
 * @return array|false The package row or false.
 */
function get_package($package_id) {
	$sql = "SELECT p.*, wl.loc_name AS location_name, wl.loc_code AS location_code "
		. "FROM " . TB_PREF . "wh_packages p "
		. "LEFT JOIN " . TB_PREF . "wh_locations wl ON p.current_loc_id = wl.loc_id "
		. "WHERE p.package_id=" . (int)$package_id;
	$result = db_query($sql, 'could not get package');
	return db_fetch($result);
}

/**
 * Get a single package by code.
 *
 * @param string $package_code Package code.
 * @return array|false The package row or false.
 */
function get_package_by_code($package_code) {
	$sql = "SELECT p.*, wl.loc_name AS location_name, wl.loc_code AS location_code "
		. "FROM " . TB_PREF . "wh_packages p "
		. "LEFT JOIN " . TB_PREF . "wh_locations wl ON p.current_loc_id = wl.loc_id "
		. "WHERE p.package_code=" . db_escape($package_code);
	$result = db_query($sql, 'could not get package by code');
	return db_fetch($result);
}

/**
 * Get packages with optional filters.
 *
 * @param array $filters Associative filter array: 'status', 'package_type',
 *                       'carrier', 'code_like', 'tracking_like', 'show_inactive',
 *                       'warehouse_loc_code'.
 * @param int   $limit   Max rows.
 * @param int   $offset  Offset.
 * @return resource DB result.
 */
function get_packages($filters = array(), $limit = 50, $offset = 0) {
	$where = array("1=1");

	if (!empty($filters['status'])) {
		if (is_array($filters['status'])) {
			$escaped = array_map('db_escape', $filters['status']);
			$where[] = "p.status IN (" . implode(',', $escaped) . ")";
		} else {
			$where[] = "p.status=" . db_escape($filters['status']);
		}
	}

	if (!empty($filters['package_type']))
		$where[] = "p.package_type=" . db_escape($filters['package_type']);

	if (!empty($filters['carrier']))
		$where[] = "p.carrier=" . db_escape($filters['carrier']);

	if (!empty($filters['code_like']))
		$where[] = "p.package_code LIKE " . db_escape('%' . $filters['code_like'] . '%');

	if (!empty($filters['tracking_like']))
		$where[] = "p.tracking_number LIKE " . db_escape('%' . $filters['tracking_like'] . '%');

	if (empty($filters['show_inactive']))
		$where[] = "p.status NOT IN ('cancelled')";

	if (!empty($filters['warehouse_loc_code']))
		$where[] = "p.current_loc_id IN (SELECT loc_id FROM " . TB_PREF . "wh_locations WHERE warehouse_loc_code=" . db_escape($filters['warehouse_loc_code']) . ")";

	$sql = "SELECT p.*, wl.loc_name AS location_name, wl.loc_code AS location_code "
		. "FROM " . TB_PREF . "wh_packages p "
		. "LEFT JOIN " . TB_PREF . "wh_locations wl ON p.current_loc_id = wl.loc_id "
		. "WHERE " . implode(' AND ', $where)
		. " ORDER BY FIELD(p.status, 'open', 'sealed', 'shipped', 'delivered', 'cancelled'), p.created_at DESC";

	if ($limit > 0)
		$sql .= " LIMIT " . (int)$offset . ", " . (int)$limit;

	return db_query($sql, 'could not get packages');
}

/**
 * Count packages matching filters.
 *
 * @param array $filters Same filter keys as get_packages().
 * @return int Count.
 */
function count_packages($filters = array()) {
	$where = array("1=1");

	if (!empty($filters['status'])) {
		if (is_array($filters['status'])) {
			$escaped = array_map('db_escape', $filters['status']);
			$where[] = "status IN (" . implode(',', $escaped) . ")";
		} else {
			$where[] = "status=" . db_escape($filters['status']);
		}
	}

	if (!empty($filters['package_type']))
		$where[] = "package_type=" . db_escape($filters['package_type']);

	if (empty($filters['show_inactive']))
		$where[] = "status NOT IN ('cancelled')";

	$sql = "SELECT COUNT(*) AS cnt FROM " . TB_PREF . "wh_packages WHERE " . implode(' AND ', $where);
	$result = db_query($sql, 'could not count packages');
	$row = db_fetch($result);
	return (int)$row['cnt'];
}

/**
 * Check if a package can be deleted.
 *
 * Only open packages with no contents can be deleted.
 *
 * @param int $package_id Package ID.
 * @return bool True if deletable.
 */
function can_delete_package($package_id) {
	$pkg = get_package($package_id);
	if (!$pkg || $pkg['status'] !== 'open')
		return false;

	$sql = "SELECT COUNT(*) AS cnt FROM " . TB_PREF . "wh_package_contents WHERE package_id=" . (int)$package_id;
	$result = db_query($sql, 'could not check package contents');
	$row = db_fetch($result);
	return (int)$row['cnt'] === 0;
}

/**
 * Delete a package (only if empty and open).
 *
 * @param int $package_id Package ID.
 * @return bool True if deleted.
 */
function delete_package($package_id) {
	if (!can_delete_package($package_id))
		return false;

	$sql = "DELETE FROM " . TB_PREF . "wh_packages WHERE package_id=" . (int)$package_id;
	db_query($sql, 'could not delete package');
	return true;
}

/**
 * Check if a package code is unique.
 *
 * @param string $code Package code to check.
 * @param int    $exclude_id Package ID to exclude (for edits).
 * @return bool True if unique.
 */
function is_package_code_unique($code, $exclude_id = 0) {
	$sql = "SELECT COUNT(*) AS cnt FROM " . TB_PREF . "wh_packages "
		. "WHERE package_code=" . db_escape($code);
	if ($exclude_id > 0)
		$sql .= " AND package_id <> " . (int)$exclude_id;
	$result = db_query($sql, 'could not check package code');
	$row = db_fetch($result);
	return (int)$row['cnt'] === 0;
}

// =====================================================================
// Package Contents Management
// =====================================================================

/**
 * Add an item to a package.
 *
 * @param int      $package_id Package ID.
 * @param string   $stock_id   Item code.
 * @param float    $qty        Quantity.
 * @param int|null $batch_id   Batch FK.
 * @param int|null $serial_id  Serial FK.
 * @return int The inserted content line id.
 */
function add_package_content($package_id, $stock_id, $qty, $batch_id = null, $serial_id = null) {
	// Check for existing line with same item+batch+serial
	$sql_check = "SELECT id, qty FROM " . TB_PREF . "wh_package_contents "
		. "WHERE package_id=" . (int)$package_id
		. " AND stock_id=" . db_escape($stock_id)
		. " AND " . ($batch_id !== null ? "batch_id=" . (int)$batch_id : "batch_id IS NULL")
		. " AND " . ($serial_id !== null ? "serial_id=" . (int)$serial_id : "serial_id IS NULL");
	$result_check = db_query($sql_check, 'could not check package contents');
	$existing = db_fetch($result_check);

	if ($existing) {
		// Update quantity if same item/batch/serial already in package
		$new_qty = (float)$existing['qty'] + (float)$qty;
		$sql = "UPDATE " . TB_PREF . "wh_package_contents SET qty=" . $new_qty
			. " WHERE id=" . (int)$existing['id'];
		db_query($sql, 'could not update package content');
		return (int)$existing['id'];
	}

	$sql = "INSERT INTO " . TB_PREF . "wh_package_contents (
		package_id, stock_id, qty, batch_id, serial_id
	) VALUES (
		" . (int)$package_id . ",
		" . db_escape($stock_id) . ",
		" . (float)$qty . ",
		" . ($batch_id !== null ? (int)$batch_id : 'NULL') . ",
		" . ($serial_id !== null ? (int)$serial_id : 'NULL') . "
	)";
	db_query($sql, 'could not add package content');
	return db_insert_id();
}

/**
 * Remove an item from a package.
 *
 * @param int $content_id Content line ID.
 * @return bool Success.
 */
function remove_package_content($content_id) {
	$sql = "DELETE FROM " . TB_PREF . "wh_package_contents WHERE id=" . (int)$content_id;
	db_query($sql, 'could not remove package content');
	return true;
}

/**
 * Update quantity of a content line.
 *
 * @param int   $content_id Content line ID.
 * @param float $qty        New quantity.
 */
function update_package_content_qty($content_id, $qty) {
	$sql = "UPDATE " . TB_PREF . "wh_package_contents SET qty=" . (float)$qty
		. " WHERE id=" . (int)$content_id;
	db_query($sql, 'could not update package content qty');
}

/**
 * Get all contents of a package.
 *
 * @param int $package_id Package ID.
 * @return resource DB result.
 */
function get_package_contents($package_id) {
	$sql = "SELECT pc.*, sm.description AS item_description, sm.units,
			sn.serial_no, sb.batch_no
		FROM " . TB_PREF . "wh_package_contents pc
		LEFT JOIN " . TB_PREF . "stock_master sm ON pc.stock_id = sm.stock_id
		LEFT JOIN " . TB_PREF . "serial_numbers sn ON pc.serial_id = sn.id
		LEFT JOIN " . TB_PREF . "stock_batches sb ON pc.batch_id = sb.id
		WHERE pc.package_id=" . (int)$package_id
		. " ORDER BY pc.id";
	return db_query($sql, 'could not get package contents');
}

/**
 * Count items in a package (distinct line count).
 *
 * @param int $package_id Package ID.
 * @return int Line count.
 */
function count_package_contents($package_id) {
	$sql = "SELECT COUNT(*) AS cnt FROM " . TB_PREF . "wh_package_contents "
		. "WHERE package_id=" . (int)$package_id;
	$result = db_query($sql, 'could not count package contents');
	$row = db_fetch($result);
	return (int)$row['cnt'];
}

/**
 * Get total quantity across all items in a package.
 *
 * @param int $package_id Package ID.
 * @return float Total quantity.
 */
function get_package_total_qty($package_id) {
	$sql = "SELECT COALESCE(SUM(qty), 0) AS total FROM " . TB_PREF . "wh_package_contents "
		. "WHERE package_id=" . (int)$package_id;
	$result = db_query($sql, 'could not get package total qty');
	$row = db_fetch($result);
	return (float)$row['total'];
}

// =====================================================================
// Package Lifecycle
// =====================================================================

/**
 * Seal a package — mark it as packed and ready for shipping.
 *
 * Only open packages with at least one content line can be sealed.
 *
 * @param int $package_id Package ID.
 * @return array Result: ['success' => bool, 'message' => string].
 */
function seal_package($package_id) {
	$pkg = get_package($package_id);
	if (!$pkg)
		return array('success' => false, 'message' => _('Package not found.'));

	if ($pkg['status'] !== 'open')
		return array('success' => false, 'message' => _('Only open packages can be sealed.'));

	$cnt = count_package_contents($package_id);
	if ($cnt === 0)
		return array('success' => false, 'message' => _('Cannot seal an empty package. Add items first.'));

	$sql = "UPDATE " . TB_PREF . "wh_packages SET status='sealed' "
		. "WHERE package_id=" . (int)$package_id;
	db_query($sql, 'could not seal package');

	return array('success' => true, 'message' => sprintf(_('Package %s has been sealed.'), $pkg['package_code']));
}

/**
 * Unseal a package — reopen a sealed package for modifications.
 *
 * @param int $package_id Package ID.
 * @return array Result: ['success' => bool, 'message' => string].
 */
function unseal_package($package_id) {
	$pkg = get_package($package_id);
	if (!$pkg)
		return array('success' => false, 'message' => _('Package not found.'));

	if ($pkg['status'] !== 'sealed')
		return array('success' => false, 'message' => _('Only sealed packages can be unsealed.'));

	$sql = "UPDATE " . TB_PREF . "wh_packages SET status='open' "
		. "WHERE package_id=" . (int)$package_id;
	db_query($sql, 'could not unseal package');

	return array('success' => true, 'message' => sprintf(_('Package %s has been reopened.'), $pkg['package_code']));
}

/**
 * Mark a package as shipped.
 *
 * @param int    $package_id      Package ID.
 * @param string $carrier         Carrier name.
 * @param string $tracking_number Tracking number.
 * @return array Result: ['success' => bool, 'message' => string].
 */
function ship_package($package_id, $carrier = '', $tracking_number = '') {
	$pkg = get_package($package_id);
	if (!$pkg)
		return array('success' => false, 'message' => _('Package not found.'));

	if ($pkg['status'] !== 'sealed')
		return array('success' => false, 'message' => _('Only sealed packages can be shipped.'));

	$sets = array("status='shipped'");
	if (!empty($carrier))
		$sets[] = "carrier=" . db_escape($carrier);
	if (!empty($tracking_number))
		$sets[] = "tracking_number=" . db_escape($tracking_number);

	$sql = "UPDATE " . TB_PREF . "wh_packages SET " . implode(', ', $sets)
		. " WHERE package_id=" . (int)$package_id;
	db_query($sql, 'could not ship package');

	return array('success' => true, 'message' => sprintf(_('Package %s has been shipped.'), $pkg['package_code']));
}

/**
 * Mark a package as delivered.
 *
 * @param int $package_id Package ID.
 * @return array Result: ['success' => bool, 'message' => string].
 */
function deliver_package($package_id) {
	$pkg = get_package($package_id);
	if (!$pkg)
		return array('success' => false, 'message' => _('Package not found.'));

	if ($pkg['status'] !== 'shipped')
		return array('success' => false, 'message' => _('Only shipped packages can be marked as delivered.'));

	$sql = "UPDATE " . TB_PREF . "wh_packages SET status='delivered' "
		. "WHERE package_id=" . (int)$package_id;
	db_query($sql, 'could not deliver package');

	return array('success' => true, 'message' => sprintf(_('Package %s has been delivered.'), $pkg['package_code']));
}

/**
 * Cancel a package — contents are removed, status set to cancelled.
 *
 * @param int $package_id Package ID.
 * @return array Result: ['success' => bool, 'message' => string].
 */
function cancel_package($package_id) {
	$pkg = get_package($package_id);
	if (!$pkg)
		return array('success' => false, 'message' => _('Package not found.'));

	if (in_array($pkg['status'], array('shipped', 'delivered')))
		return array('success' => false, 'message' => _('Cannot cancel a shipped or delivered package.'));

	begin_transaction();

	// Remove all contents
	$sql = "DELETE FROM " . TB_PREF . "wh_package_contents WHERE package_id=" . (int)$package_id;
	db_query($sql, 'could not remove package contents');

	// Cancel the package
	$sql = "UPDATE " . TB_PREF . "wh_packages SET status='cancelled' "
		. "WHERE package_id=" . (int)$package_id;
	db_query($sql, 'could not cancel package');

	commit_transaction();

	return array('success' => true, 'message' => sprintf(_('Package %s has been cancelled.'), $pkg['package_code']));
}

// =====================================================================
// Packing Station: Delivery-based Packing
// =====================================================================

/**
 * Get completed pick operations that are ready for packing.
 *
 * Returns pick operations that are 'done' and don't yet have
 * a linked pack operation.
 *
 * @param string $warehouse_loc_code Filter by warehouse. Empty for all.
 * @return resource DB result.
 */
function get_picks_ready_for_packing($warehouse_loc_code = '') {
	$where = array(
		"op.op_type='pick'",
		"op.op_status='done'",
	);

	// Exclude picks that already have a linked pack operation
	$where[] = "NOT EXISTS (
		SELECT 1 FROM " . TB_PREF . "wh_operations child
		WHERE child.parent_op_id = op.op_id AND child.op_type = 'pack'
		AND child.op_status <> 'cancelled'
	)";

	if (!empty($warehouse_loc_code)) {
		$where[] = "op.from_loc_id IN (
			SELECT loc_id FROM " . TB_PREF . "wh_locations
			WHERE warehouse_loc_code=" . db_escape($warehouse_loc_code)
		. ")";
	}

	$sql = "SELECT op.*,
			pw.wave_name,
			COALESCE(SUM(ol.qty_done), 0) AS total_picked_qty,
			COUNT(ol.line_id) AS total_lines
		FROM " . TB_PREF . "wh_operations op
		LEFT JOIN " . TB_PREF . "wh_picking_waves pw ON op.wave_id = pw.wave_id
		LEFT JOIN " . TB_PREF . "wh_operation_lines ol ON op.op_id = ol.op_id
		WHERE " . implode(' AND ', $where)
		. " GROUP BY op.op_id"
		. " ORDER BY op.priority ASC, op.completed_at ASC";

	return db_query($sql, 'could not get picks ready for packing');
}

/**
 * Get picked items (operation lines) for a pick operation.
 *
 * @param int $pick_op_id Pick operation ID.
 * @return resource DB result with item details.
 */
function get_picked_items_for_packing($pick_op_id) {
	$sql = "SELECT ol.*, sm.description AS item_description, sm.units,
			sm.track_by, sm.has_serial_no, sm.has_batch_no,
			fl.loc_name AS from_bin_name, fl.loc_code AS from_bin_code,
			sn.serial_no, sb.batch_no
		FROM " . TB_PREF . "wh_operation_lines ol
		LEFT JOIN " . TB_PREF . "stock_master sm ON ol.stock_id = sm.stock_id
		LEFT JOIN " . TB_PREF . "wh_locations fl ON ol.from_loc_id = fl.loc_id
		LEFT JOIN " . TB_PREF . "serial_numbers sn ON ol.serial_id = sn.id
		LEFT JOIN " . TB_PREF . "stock_batches sb ON ol.batch_id = sb.id
		WHERE ol.op_id=" . (int)$pick_op_id
		. " ORDER BY ol.line_id";
	return db_query($sql, 'could not get picked items for packing');
}

/**
 * Create a pack operation linked to a pick operation.
 *
 * Creates a warehouse operation of type 'pack' with the same lines
 * as the completed pick operation.
 *
 * @param int      $pick_op_id  Source pick operation ID.
 * @param int|null $assigned_to Assigned packer user ID.
 * @param string   $memo        Notes.
 * @return int The new pack operation ID.
 */
function create_pack_operation($pick_op_id, $assigned_to = null, $memo = '') {
	$pick_op = get_wh_operation($pick_op_id);
	if (!$pick_op)
		return 0;

	begin_transaction();

	$pack_op_id = add_wh_operation(
		'pack',           // op_type
		'ready',          // op_status
		$pick_op['source_doc_type'],
		$pick_op['source_doc_no'],
		$pick_op_id,      // parent_op_id — link to the pick
		null,             // from_loc_id
		null,             // to_loc_id
		null,             // scheduled_date
		$assigned_to,
		$pick_op['priority'],
		$pick_op['route_id'],
		$memo
	);

	// Link pick → pack
	link_wh_operations($pick_op_id, $pack_op_id);

	// Copy picked lines to pack operation
	$lines = get_wh_operation_lines($pick_op_id);
	while ($line = db_fetch($lines)) {
		if ((float)$line['qty_done'] > 0) {
			add_wh_operation_line(
				$pack_op_id,
				$line['stock_id'],
				(float)$line['qty_done'],  // qty_planned = pick's qty_done
				0,                          // qty_done starts at 0
				$line['from_loc_id'],
				$line['to_loc_id'],
				$line['batch_id'] ? (int)$line['batch_id'] : null,
				$line['serial_id'] ? (int)$line['serial_id'] : null,
				null,                       // package_id — assigned during packing
				(float)$line['unit_cost']
			);
		}
	}

	commit_transaction();

	return $pack_op_id;
}

/**
 * Confirm packing of a line into a package.
 *
 * Sets the package_id on the operation line and marks qty_done.
 * Also adds the item to the package contents.
 *
 * @param int   $line_id    Operation line ID.
 * @param int   $package_id Package to pack into.
 * @param float $qty        Quantity packed.
 * @return bool Success.
 */
function confirm_pack_line($line_id, $package_id, $qty) {
	// Get the line details
	$sql = "SELECT * FROM " . TB_PREF . "wh_operation_lines WHERE line_id=" . (int)$line_id;
	$result = db_query($sql, 'could not get operation line');
	$line = db_fetch($result);
	if (!$line)
		return false;

	// Verify package is open
	$pkg = get_package($package_id);
	if (!$pkg || $pkg['status'] !== 'open')
		return false;

	$packed_qty = min((float)$qty, (float)$line['qty_planned']);

	begin_transaction();

	// Update operation line: set qty_done and package_id
	$sql = "UPDATE " . TB_PREF . "wh_operation_lines SET "
		. "qty_done=" . $packed_qty . ", "
		. "package_id=" . (int)$package_id
		. " WHERE line_id=" . (int)$line_id;
	db_query($sql, 'could not confirm pack line');

	// Add to package contents
	add_package_content(
		$package_id,
		$line['stock_id'],
		$packed_qty,
		$line['batch_id'] ? (int)$line['batch_id'] : null,
		$line['serial_id'] ? (int)$line['serial_id'] : null
	);

	commit_transaction();

	return true;
}

/**
 * Complete a pack operation — all lines must be packed.
 *
 * @param int $pack_op_id Pack operation ID.
 * @return array Result: ['success' => bool, 'message' => string].
 */
function complete_pack_operation($pack_op_id) {
	$op = get_wh_operation($pack_op_id);
	if (!$op || $op['op_type'] !== 'pack')
		return array('success' => false, 'message' => _('Invalid pack operation.'));

	if ($op['op_status'] === 'done')
		return array('success' => false, 'message' => _('Pack operation already completed.'));

	// Check all lines are packed
	$sql = "SELECT COUNT(*) AS total, "
		. "SUM(CASE WHEN qty_done > 0 THEN 1 ELSE 0 END) AS packed "
		. "FROM " . TB_PREF . "wh_operation_lines WHERE op_id=" . (int)$pack_op_id;
	$result = db_query($sql, 'could not check pack lines');
	$row = db_fetch($result);

	if ((int)$row['total'] === 0)
		return array('success' => false, 'message' => _('No lines to pack.'));

	if ((int)$row['packed'] < (int)$row['total'])
		return array('success' => false, 'message' => sprintf(
			_('%d of %d lines are packed. Pack all lines before completing.'),
			(int)$row['packed'], (int)$row['total']
		));

	update_wh_operation_status($pack_op_id, 'done');

	return array('success' => true, 'message' => _('Pack operation completed successfully.'));
}

// =====================================================================
// Summary & Statistics
// =====================================================================

/**
 * Get package status summary counts.
 *
 * @return array Associative array of status => count.
 */
function get_package_status_summary() {
	$summary = array();
	foreach (get_package_statuses() as $status => $label)
		$summary[$status] = 0;

	$sql = "SELECT status, COUNT(*) AS cnt FROM " . TB_PREF . "wh_packages GROUP BY status";
	$result = db_query($sql, 'could not get package status summary');
	while ($row = db_fetch($result))
		$summary[$row['status']] = (int)$row['cnt'];

	return $summary;
}

/**
 * Get packages associated with a delivery note.
 *
 * Finds packages linked via pack operations that reference a delivery.
 *
 * @param int $delivery_no Delivery note number.
 * @return resource DB result.
 */
function get_packages_for_delivery($delivery_no) {
	$sql = "SELECT DISTINCT p.*
		FROM " . TB_PREF . "wh_packages p
		INNER JOIN " . TB_PREF . "wh_package_contents pc ON p.package_id = pc.package_id
		INNER JOIN " . TB_PREF . "wh_operation_lines ol ON ol.package_id = p.package_id
		INNER JOIN " . TB_PREF . "wh_operations op ON ol.op_id = op.op_id
		WHERE op.op_type = 'pack'
			AND op.source_doc_type = " . ST_CUSTDELIVERY . "
			AND op.source_doc_no = " . (int)$delivery_no
		. " ORDER BY p.package_id";
	return db_query($sql, 'could not get packages for delivery');
}

/**
 * Get packing progress for a pack operation.
 *
 * @param int $pack_op_id Pack operation ID.
 * @return array Progress data: total_lines, packed_lines, total_qty, packed_qty, pct.
 */
function get_packing_progress($pack_op_id) {
	$sql = "SELECT COUNT(*) AS total_lines,
			SUM(CASE WHEN qty_done > 0 THEN 1 ELSE 0 END) AS packed_lines,
			COALESCE(SUM(qty_planned), 0) AS total_qty,
			COALESCE(SUM(qty_done), 0) AS packed_qty
		FROM " . TB_PREF . "wh_operation_lines
		WHERE op_id=" . (int)$pack_op_id;
	$result = db_query($sql, 'could not get packing progress');
	$row = db_fetch($result);

	$total = (int)$row['total_lines'];
	$packed = (int)$row['packed_lines'];
	$pct = $total > 0 ? round(($packed / $total) * 100, 1) : 0;

	return array(
		'total_lines' => $total,
		'packed_lines' => $packed,
		'total_qty'   => (float)$row['total_qty'],
		'packed_qty'  => (float)$row['packed_qty'],
		'pct'         => $pct,
	);
}
