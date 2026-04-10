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
 * Warehouse Shipping DB layer — Shipment CRUD, ship confirmation,
 * carrier tracking, and shipping workflow.
 *
 * A shipment groups one or more sealed packages for dispatch.
 * Shipment lifecycle: draft → confirmed → shipped → delivered
 *
 * Uses: 0_wh_operations (op_type='ship'), 0_wh_packages
 *
 * Note: This module does NOT create a separate shipments table.
 * Instead, it uses the wh_operations infrastructure with op_type='ship'
 * and links to packages via wh_operation_lines.package_id.
 *
 * @package NotrinosERP
 * @subpackage Inventory/Warehouse
 */

include_once(dirname(__FILE__) . '/warehouse_operations_db.inc');
include_once(dirname(__FILE__) . '/warehouse_packing_db.inc');

// =====================================================================
// Shipment Status Helpers
// =====================================================================

/**
 * Get valid shipment statuses (mapped to operation statuses).
 *
 * @return array Associative array of status_code => display_label.
 */
function get_shipment_statuses() {
	return array(
		'draft'       => _('Draft'),
		'ready'       => _('Confirmed'),
		'in_progress' => _('Shipping'),
		'done'        => _('Shipped'),
		'cancelled'   => _('Cancelled'),
	);
}

/**
 * Get CSS color for a shipment status.
 *
 * @param string $status Status code.
 * @return string CSS color hex code.
 */
function get_shipment_status_color($status) {
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
 * Render a colored shipment status badge.
 *
 * @param string $status Status code.
 * @return string HTML span with background color.
 */
function shipment_status_badge($status) {
	$statuses = get_shipment_statuses();
	$label = isset($statuses[$status]) ? $statuses[$status] : $status;
	$color = get_shipment_status_color($status);
	return '<span style="background:' . $color . ';color:#fff;padding:2px 8px;border-radius:10px;font-size:11px;">'
		. htmlspecialchars($label) . '</span>';
}

// =====================================================================
// Shipment CRUD (using wh_operations with op_type='ship')
// =====================================================================

/**
 * Create a new shipment (ship operation).
 *
 * @param string   $warehouse_loc_code Warehouse location code (for lookup).
 * @param string   $carrier            Carrier name.
 * @param string   $tracking_number    Master tracking number.
 * @param string   $ship_date          Planned ship date (SQL date).
 * @param int|null $assigned_to        Assigned worker user ID.
 * @param string   $memo               Notes/special instructions.
 * @return int The new shipment operation ID.
 */
function add_shipment($warehouse_loc_code, $carrier = '', $tracking_number = '',
	$ship_date = null, $assigned_to = null, $memo = '')
{
	// Store carrier/tracking in memo field as structured data
	$ship_memo = $memo;
	if (!empty($carrier) || !empty($tracking_number)) {
		$meta = array();
		if (!empty($carrier))
			$meta[] = 'Carrier: ' . $carrier;
		if (!empty($tracking_number))
			$meta[] = 'Tracking: ' . $tracking_number;
		$ship_memo = implode(' | ', $meta) . (!empty($memo) ? ' | ' . $memo : '');
	}

	// Store carrier & tracking in custom_data JSON-like field
	$custom = array(
		'carrier'         => $carrier,
		'tracking_number' => $tracking_number,
		'ship_date'       => $ship_date ? $ship_date : date('Y-m-d'),
		'warehouse'       => $warehouse_loc_code,
	);

	$op_id = add_wh_operation(
		'ship',        // op_type
		'draft',       // op_status
		null,          // source_doc_type
		null,          // source_doc_no
		null,          // parent_op_id
		null,          // from_loc_id
		null,          // to_loc_id
		$ship_date,    // scheduled_date
		$assigned_to,
		5,             // default priority
		null,          // route_id
		$ship_memo
	);

	// Store structured data in custom_data
	$sql = "UPDATE " . TB_PREF . "wh_operations SET custom_data="
		. db_escape(json_encode($custom))
		. " WHERE op_id=" . (int)$op_id;
	db_query($sql, 'could not set shipment custom data');

	return $op_id;
}

/**
 * Update shipment details.
 *
 * @param int      $shipment_id  Shipment operation ID.
 * @param string   $carrier      Carrier name.
 * @param string   $tracking     Master tracking number.
 * @param string   $ship_date    Ship date.
 * @param int|null $assigned_to  Assigned worker.
 * @param string   $memo         Notes.
 */
function update_shipment($shipment_id, $carrier, $tracking, $ship_date = null,
	$assigned_to = null, $memo = '')
{
	$custom = array(
		'carrier'         => $carrier,
		'tracking_number' => $tracking,
		'ship_date'       => $ship_date ? $ship_date : date('Y-m-d'),
	);

	// Get existing custom_data to preserve warehouse
	$op = get_wh_operation($shipment_id);
	if ($op && !empty($op['custom_data'])) {
		$existing = json_decode($op['custom_data'], true);
		if (is_array($existing)) {
			$custom['warehouse'] = isset($existing['warehouse']) ? $existing['warehouse'] : '';
		}
	}

	$ship_memo = $memo;
	if (!empty($carrier) || !empty($tracking)) {
		$meta = array();
		if (!empty($carrier))
			$meta[] = 'Carrier: ' . $carrier;
		if (!empty($tracking))
			$meta[] = 'Tracking: ' . $tracking;
		$ship_memo = implode(' | ', $meta) . (!empty($memo) ? ' | ' . $memo : '');
	}

	$sql = "UPDATE " . TB_PREF . "wh_operations SET "
		. "memo=" . db_escape($ship_memo) . ", "
		. "custom_data=" . db_escape(json_encode($custom)) . ", "
		. "scheduled_date=" . ($ship_date ? db_escape($ship_date) : 'NULL') . ", "
		. "assigned_to=" . ($assigned_to ? (int)$assigned_to : 'NULL')
		. " WHERE op_id=" . (int)$shipment_id;
	db_query($sql, 'could not update shipment');
}

/**
 * Get a shipment with parsed custom data.
 *
 * @param int $shipment_id Shipment operation ID.
 * @return array|false Shipment data with carrier/tracking fields.
 */
function get_shipment($shipment_id) {
	$op = get_wh_operation($shipment_id);
	if (!$op || $op['op_type'] !== 'ship')
		return false;

	// Parse custom_data
	$op['carrier'] = '';
	$op['tracking_number'] = '';
	$op['ship_date'] = $op['scheduled_date'] ? substr($op['scheduled_date'], 0, 10) : '';
	$op['warehouse'] = '';

	if (!empty($op['custom_data'])) {
		$custom = json_decode($op['custom_data'], true);
		if (is_array($custom)) {
			$op['carrier'] = isset($custom['carrier']) ? $custom['carrier'] : '';
			$op['tracking_number'] = isset($custom['tracking_number']) ? $custom['tracking_number'] : '';
			$op['ship_date'] = isset($custom['ship_date']) ? $custom['ship_date'] : $op['ship_date'];
			$op['warehouse'] = isset($custom['warehouse']) ? $custom['warehouse'] : '';
		}
	}

	return $op;
}

/**
 * Get shipments with optional filters.
 *
 * @param array $filters Associative filter array: 'status', 'carrier',
 *                       'warehouse_loc_code', 'from_date', 'to_date'.
 * @param int   $limit   Max rows.
 * @param int   $offset  Offset.
 * @return resource DB result.
 */
function get_shipments($filters = array(), $limit = 50, $offset = 0) {
	$op_filters = array(
		'op_type' => 'ship',
	);

	if (!empty($filters['status']))
		$op_filters['op_status'] = $filters['status'];
	else
		$op_filters['op_status'] = array('draft', 'ready', 'in_progress', 'done');

	if (!empty($filters['warehouse_loc_code']))
		$op_filters['warehouse_loc_code'] = $filters['warehouse_loc_code'];

	if (!empty($filters['from_date']))
		$op_filters['from_date'] = $filters['from_date'];

	if (!empty($filters['to_date']))
		$op_filters['to_date'] = $filters['to_date'];

	if (!empty($filters['show_inactive']))
		$op_filters['include_cancelled'] = true;

	return get_wh_operations_filtered($op_filters, $limit, $offset);
}

/**
 * Count shipments matching filters.
 *
 * @param array $filters Same filter keys as get_shipments().
 * @return int Count.
 */
function count_shipments($filters = array()) {
	$where = array("op_type='ship'");

	if (!empty($filters['status']))
		$where[] = "op_status=" . db_escape($filters['status']);
	else
		$where[] = "op_status IN ('draft','ready','in_progress','done')";

	$sql = "SELECT COUNT(*) AS cnt FROM " . TB_PREF . "wh_operations WHERE " . implode(' AND ', $where);
	$result = db_query($sql, 'could not count shipments');
	$row = db_fetch($result);
	return (int)$row['cnt'];
}

// =====================================================================
// Shipment Package Management
// =====================================================================

/**
 * Add a sealed package to a shipment.
 *
 * Creates an operation line referencing the package.
 *
 * @param int $shipment_id Shipment operation ID.
 * @param int $package_id  Package ID to add.
 * @return array Result: ['success' => bool, 'message' => string].
 */
function add_package_to_shipment($shipment_id, $package_id) {
	$op = get_wh_operation($shipment_id);
	if (!$op || $op['op_type'] !== 'ship')
		return array('success' => false, 'message' => _('Invalid shipment.'));

	if (!in_array($op['op_status'], array('draft', 'ready')))
		return array('success' => false, 'message' => _('Can only add packages to draft or confirmed shipments.'));

	$pkg = get_package($package_id);
	if (!$pkg)
		return array('success' => false, 'message' => _('Package not found.'));

	if ($pkg['status'] !== 'sealed')
		return array('success' => false, 'message' => _('Only sealed packages can be added to a shipment.'));

	// Check if package is already in this shipment
	$sql = "SELECT COUNT(*) AS cnt FROM " . TB_PREF . "wh_operation_lines "
		. "WHERE op_id=" . (int)$shipment_id . " AND package_id=" . (int)$package_id;
	$result = db_query($sql, 'could not check shipment package');
	$row = db_fetch($result);
	if ((int)$row['cnt'] > 0)
		return array('success' => false, 'message' => _('Package is already in this shipment.'));

	// Check if package is in another active shipment
	$sql = "SELECT op.op_id FROM " . TB_PREF . "wh_operation_lines ol "
		. "INNER JOIN " . TB_PREF . "wh_operations op ON ol.op_id = op.op_id "
		. "WHERE op.op_type='ship' AND op.op_status NOT IN ('cancelled','done') "
		. "AND ol.package_id=" . (int)$package_id
		. " AND op.op_id <> " . (int)$shipment_id;
	$result = db_query($sql, 'could not check package in other shipment');
	$existing = db_fetch($result);
	if ($existing)
		return array('success' => false, 'message' => sprintf(
			_('Package is already assigned to shipment #%d.'), (int)$existing['op_id']
		));

	// Add a summary line for the package (aggregate package contents)
	$total_qty = get_package_total_qty($package_id);
	add_wh_operation_line(
		$shipment_id,
		'',               // no specific stock_id for package-level line
		$total_qty,       // qty_planned
		0,                // qty_done
		$pkg['current_loc_id'], // from_loc_id
		null,             // to_loc_id
		null,             // batch_id
		null,             // serial_id
		$package_id,      // package_id
		0                 // unit_cost
	);

	return array('success' => true, 'message' => sprintf(
		_('Package %s added to shipment.'), $pkg['package_code']
	));
}

/**
 * Remove a package from a shipment.
 *
 * @param int $shipment_id Shipment operation ID.
 * @param int $package_id  Package ID to remove.
 * @return array Result: ['success' => bool, 'message' => string].
 */
function remove_package_from_shipment($shipment_id, $package_id) {
	$op = get_wh_operation($shipment_id);
	if (!$op || $op['op_type'] !== 'ship')
		return array('success' => false, 'message' => _('Invalid shipment.'));

	if (!in_array($op['op_status'], array('draft', 'ready')))
		return array('success' => false, 'message' => _('Can only remove packages from draft or confirmed shipments.'));

	$sql = "DELETE FROM " . TB_PREF . "wh_operation_lines "
		. "WHERE op_id=" . (int)$shipment_id . " AND package_id=" . (int)$package_id;
	db_query($sql, 'could not remove package from shipment');

	return array('success' => true, 'message' => _('Package removed from shipment.'));
}

/**
 * Get packages in a shipment.
 *
 * @param int $shipment_id Shipment operation ID.
 * @return resource DB result.
 */
function get_shipment_packages($shipment_id) {
	$sql = "SELECT ol.line_id, ol.package_id, ol.qty_planned, ol.qty_done,
			p.package_code, p.package_type, p.status AS package_status,
			p.weight, p.carrier, p.tracking_number,
			wl.loc_name AS location_name, wl.loc_code AS location_code
		FROM " . TB_PREF . "wh_operation_lines ol
		INNER JOIN " . TB_PREF . "wh_packages p ON ol.package_id = p.package_id
		LEFT JOIN " . TB_PREF . "wh_locations wl ON p.current_loc_id = wl.loc_id
		WHERE ol.op_id=" . (int)$shipment_id
		. " ORDER BY ol.line_id";
	return db_query($sql, 'could not get shipment packages');
}

/**
 * Count packages in a shipment.
 *
 * @param int $shipment_id Shipment operation ID.
 * @return int Package count.
 */
function count_shipment_packages($shipment_id) {
	$sql = "SELECT COUNT(*) AS cnt FROM " . TB_PREF . "wh_operation_lines "
		. "WHERE op_id=" . (int)$shipment_id . " AND package_id IS NOT NULL";
	$result = db_query($sql, 'could not count shipment packages');
	$row = db_fetch($result);
	return (int)$row['cnt'];
}

// =====================================================================
// Shipment Lifecycle
// =====================================================================

/**
 * Confirm a shipment — mark as ready for dispatch.
 *
 * @param int $shipment_id Shipment operation ID.
 * @return array Result: ['success' => bool, 'message' => string].
 */
function confirm_shipment($shipment_id) {
	$op = get_wh_operation($shipment_id);
	if (!$op || $op['op_type'] !== 'ship')
		return array('success' => false, 'message' => _('Invalid shipment.'));

	if ($op['op_status'] !== 'draft')
		return array('success' => false, 'message' => _('Only draft shipments can be confirmed.'));

	$pkg_count = count_shipment_packages($shipment_id);
	if ($pkg_count === 0)
		return array('success' => false, 'message' => _('Cannot confirm an empty shipment. Add packages first.'));

	update_wh_operation_status($shipment_id, 'ready');

	return array('success' => true, 'message' => sprintf(
		_('Shipment #%d confirmed with %d package(s).'), $shipment_id, $pkg_count
	));
}

/**
 * Ship a shipment — update all packages to shipped status, record ship date.
 *
 * @param int    $shipment_id    Shipment operation ID.
 * @param string $carrier        Carrier name (override).
 * @param string $tracking       Master tracking number (override).
 * @return array Result: ['success' => bool, 'message' => string].
 */
function ship_shipment($shipment_id, $carrier = '', $tracking = '') {
	$op = get_wh_operation($shipment_id);
	if (!$op || $op['op_type'] !== 'ship')
		return array('success' => false, 'message' => _('Invalid shipment.'));

	if (!in_array($op['op_status'], array('ready', 'in_progress')))
		return array('success' => false, 'message' => _('Only confirmed shipments can be shipped.'));

	begin_transaction();

	// Update all packages in this shipment to shipped
	$packages = get_shipment_packages($shipment_id);
	$pkg_count = 0;
	while ($pkg_row = db_fetch($packages)) {
		$pkg_carrier = !empty($carrier) ? $carrier : '';
		$pkg_tracking = !empty($tracking) ? $tracking : '';
		ship_package((int)$pkg_row['package_id'], $pkg_carrier, $pkg_tracking);
		$pkg_count++;
	}

	// Update carrier/tracking if provided
	if (!empty($carrier) || !empty($tracking)) {
		$custom_str = $op['custom_data'];
		$custom = !empty($custom_str) ? json_decode($custom_str, true) : array();
		if (!is_array($custom)) $custom = array();
		if (!empty($carrier))
			$custom['carrier'] = $carrier;
		if (!empty($tracking))
			$custom['tracking_number'] = $tracking;
		$custom['shipped_at'] = date('Y-m-d H:i:s');

		$sql = "UPDATE " . TB_PREF . "wh_operations SET custom_data="
			. db_escape(json_encode($custom))
			. " WHERE op_id=" . (int)$shipment_id;
		db_query($sql, 'could not update shipment custom data');
	}

	// Complete all operation lines
	$sql = "UPDATE " . TB_PREF . "wh_operation_lines SET qty_done = qty_planned "
		. "WHERE op_id=" . (int)$shipment_id;
	db_query($sql, 'could not complete shipment lines');

	// Mark shipment as done
	update_wh_operation_status($shipment_id, 'done');

	commit_transaction();

	return array('success' => true, 'message' => sprintf(
		_('Shipment #%d shipped with %d package(s).'), $shipment_id, $pkg_count
	));
}

/**
 * Cancel a shipment — remove package associations.
 *
 * @param int $shipment_id Shipment operation ID.
 * @return array Result: ['success' => bool, 'message' => string].
 */
function cancel_shipment($shipment_id) {
	$op = get_wh_operation($shipment_id);
	if (!$op || $op['op_type'] !== 'ship')
		return array('success' => false, 'message' => _('Invalid shipment.'));

	if ($op['op_status'] === 'done')
		return array('success' => false, 'message' => _('Cannot cancel a completed shipment.'));

	begin_transaction();

	// Remove operation lines (package references)
	$sql = "DELETE FROM " . TB_PREF . "wh_operation_lines WHERE op_id=" . (int)$shipment_id;
	db_query($sql, 'could not remove shipment lines');

	// Cancel the operation
	update_wh_operation_status($shipment_id, 'cancelled');

	commit_transaction();

	return array('success' => true, 'message' => sprintf(_('Shipment #%d cancelled.'), $shipment_id));
}

// =====================================================================
// Summary & Statistics
// =====================================================================

/**
 * Get shipment status summary counts.
 *
 * @return array Associative array of status => count.
 */
function get_shipment_status_summary() {
	$summary = array(
		'draft'       => 0,
		'ready'       => 0,
		'in_progress' => 0,
		'done'        => 0,
		'cancelled'   => 0,
	);

	$sql = "SELECT op_status, COUNT(*) AS cnt FROM " . TB_PREF . "wh_operations "
		. "WHERE op_type='ship' GROUP BY op_status";
	$result = db_query($sql, 'could not get shipment status summary');
	while ($row = db_fetch($result)) {
		if (isset($summary[$row['op_status']]))
			$summary[$row['op_status']] = (int)$row['cnt'];
	}

	return $summary;
}

/**
 * Get available sealed packages not yet assigned to any active shipment.
 *
 * @param string $warehouse_loc_code Filter by warehouse. Empty for all.
 * @return resource DB result.
 */
function get_packages_available_for_shipping($warehouse_loc_code = '') {
	$where = array(
		"p.status = 'sealed'",
	);

	// Exclude packages already in active shipments
	$where[] = "p.package_id NOT IN (
		SELECT ol.package_id FROM " . TB_PREF . "wh_operation_lines ol
		INNER JOIN " . TB_PREF . "wh_operations op ON ol.op_id = op.op_id
		WHERE op.op_type='ship' AND op.op_status NOT IN ('cancelled','done')
		AND ol.package_id IS NOT NULL
	)";

	if (!empty($warehouse_loc_code)) {
		$where[] = "p.current_loc_id IN (
			SELECT loc_id FROM " . TB_PREF . "wh_locations
			WHERE warehouse_loc_code=" . db_escape($warehouse_loc_code)
		. ")";
	}

	$sql = "SELECT p.*, wl.loc_name AS location_name,
			(SELECT COUNT(*) FROM " . TB_PREF . "wh_package_contents pc WHERE pc.package_id = p.package_id) AS item_count,
			(SELECT COALESCE(SUM(pc2.qty), 0) FROM " . TB_PREF . "wh_package_contents pc2 WHERE pc2.package_id = p.package_id) AS total_qty
		FROM " . TB_PREF . "wh_packages p
		LEFT JOIN " . TB_PREF . "wh_locations wl ON p.current_loc_id = wl.loc_id
		WHERE " . implode(' AND ', $where)
		. " ORDER BY p.created_at ASC";
	return db_query($sql, 'could not get packages available for shipping');
}
