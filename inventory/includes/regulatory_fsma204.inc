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
 * FSMA 204 (Food Safety Modernization Act — Food Traceability Rule) Functions.
 *
 * Implements:
 *   - Critical Tracking Events (CTEs) with Key Data Elements (KDEs)
 *   - Traceability Lot Code (TLC) management
 *   - 24-hour recall response capability
 *   - Rapid customer/batch identification for food products
 *
 * CTE Types (FDA Food Traceability Rule):
 *   - Growing: Harvest of food on FDA Traceability List (FTL)
 *   - Receiving: Receipt of FTL food
 *   - Transforming: Converting food (e.g., cutting, cooking, combining)
 *   - Creating: First point where FTL food is packed for sale
 *   - Shipping: Outbound shipment of FTL food
 *
 * Tables: 0_fsma_tracking_events, 0_stock_batches
 * Dependencies: regulatory_compliance_db.inc
 *
 * @package NotrinosERP
 * @subpackage Inventory/Regulatory/FSMA204
 */

// =====================================================================
// CTE TYPE / KDE CONSTANTS
// =====================================================================

/**
 * Get FSMA 204 Critical Tracking Event types.
 *
 * @return array Associative array of event_type => display_label
 */
function get_fsma_event_types()
{
	return array(
		'growing'      => _('Growing (Harvest)'),
		'receiving'    => _('Receiving'),
		'transforming' => _('Transforming'),
		'creating'     => _('Creating'),
		'shipping'     => _('Shipping'),
	);
}

/**
 * Get label for a CTE event type.
 *
 * @param string $type Event type code
 * @return string Translated label
 */
function get_fsma_event_type_label($type)
{
	$types = get_fsma_event_types();
	return isset($types[$type]) ? $types[$type] : $type;
}

/**
 * Get CSS color for a CTE event type.
 *
 * @param string $type Event type code
 * @return string CSS color
 */
function get_fsma_event_type_color($type)
{
	$colors = array(
		'growing'      => '#198754',
		'receiving'    => '#0d6efd',
		'transforming' => '#fd7e14',
		'creating'     => '#6f42c1',
		'shipping'     => '#dc3545',
	);
	return isset($colors[$type]) ? $colors[$type] : '#6c757d';
}

/**
 * Get the required Key Data Elements per CTE type (per FDA rule).
 *
 * @param string $type CTE type
 * @return array List of required KDE field names
 */
function get_fsma_required_kdes($type)
{
	$base = array('lot_code', 'stock_id', 'event_date', 'quantity', 'unit_of_measure', 'location_description');

	$specific = array(
		'growing'      => array('country_of_origin', 'harvest_date'),
		'receiving'    => array('trading_partner_name', 'trading_partner_type'),
		'transforming' => array(),
		'creating'     => array('pack_date'),
		'shipping'     => array('trading_partner_name', 'ship_date'),
	);

	$extra = isset($specific[$type]) ? $specific[$type] : array();
	return array_merge($base, $extra);
}

// =====================================================================
// CTE CRUD
// =====================================================================

/**
 * Record a FSMA 204 Critical Tracking Event.
 *
 * @param string      $event_type   CTE type (growing|receiving|transforming|creating|shipping)
 * @param string      $stock_id     Item stock_id
 * @param int|null    $batch_id     Batch id
 * @param string|null $lot_code     Traceability Lot Code (TLC)
 * @param array       $kde_data     Key Data Elements (associative)
 * @return int Inserted event id
 */
function add_fsma_tracking_event($event_type, $stock_id, $batch_id, $lot_code, $kde_data = array())
{
	$sql = "INSERT INTO " . TB_PREF . "fsma_tracking_events
		(event_type, stock_id, batch_id, lot_code, location_description, loc_code,
		 event_date, quantity, unit_of_measure,
		 reference_type, reference_id,
		 trading_partner_type, trading_partner_id, trading_partner_name,
		 country_of_origin, harvest_date, cooling_date, pack_date, ship_date,
		 temperature_data, notes, created_by, created_date, custom_data)
		VALUES ("
		. db_escape($event_type) . ", "
		. db_escape($stock_id) . ", "
		. db_escape($batch_id) . ", "
		. db_escape($lot_code) . ", "
		. db_escape(isset($kde_data['location_description']) ? $kde_data['location_description'] : '') . ", "
		. db_escape(isset($kde_data['loc_code']) ? $kde_data['loc_code'] : null) . ", "
		. db_escape(isset($kde_data['event_date']) ? $kde_data['event_date'] : date('Y-m-d H:i:s')) . ", "
		. db_escape(isset($kde_data['quantity']) ? $kde_data['quantity'] : null) . ", "
		. db_escape(isset($kde_data['unit_of_measure']) ? $kde_data['unit_of_measure'] : '') . ", "
		. db_escape(isset($kde_data['reference_type']) ? $kde_data['reference_type'] : null) . ", "
		. db_escape(isset($kde_data['reference_id']) ? $kde_data['reference_id'] : null) . ", "
		. db_escape(isset($kde_data['trading_partner_type']) ? $kde_data['trading_partner_type'] : null) . ", "
		. db_escape(isset($kde_data['trading_partner_id']) ? $kde_data['trading_partner_id'] : null) . ", "
		. db_escape(isset($kde_data['trading_partner_name']) ? $kde_data['trading_partner_name'] : null) . ", "
		. db_escape(isset($kde_data['country_of_origin']) ? $kde_data['country_of_origin'] : null) . ", "
		. db_escape(isset($kde_data['harvest_date']) ? $kde_data['harvest_date'] : null) . ", "
		. db_escape(isset($kde_data['cooling_date']) ? $kde_data['cooling_date'] : null) . ", "
		. db_escape(isset($kde_data['pack_date']) ? $kde_data['pack_date'] : null) . ", "
		. db_escape(isset($kde_data['ship_date']) ? $kde_data['ship_date'] : null) . ", "
		. db_escape(isset($kde_data['temperature_data']) ? json_encode($kde_data['temperature_data']) : null) . ", "
		. db_escape(isset($kde_data['notes']) ? $kde_data['notes'] : '') . ", "
		. (int)$_SESSION['wa_current_user']->user . ", "
		. "NOW(), "
		. db_escape(isset($kde_data['custom_data']) ? json_encode($kde_data['custom_data']) : null)
		. ")";

	db_query($sql, 'could not add FSMA tracking event');
	return db_insert_id();
}

/**
 * Get a single FSMA tracking event.
 *
 * @param int $id Event id
 * @return array|null
 */
function get_fsma_tracking_event($id)
{
	$sql = "SELECT fe.*, sm.description AS item_name, sb.batch_no
		FROM " . TB_PREF . "fsma_tracking_events fe
		LEFT JOIN " . TB_PREF . "stock_master sm ON fe.stock_id = sm.stock_id
		LEFT JOIN " . TB_PREF . "stock_batches sb ON fe.batch_id = sb.id
		WHERE fe.id=" . (int)$id;
	$result = db_query($sql, 'could not get FSMA tracking event');
	return db_fetch($result);
}

/**
 * Get FSMA tracking events with optional filters.
 *
 * @param array $filters Associative array: event_type, stock_id, batch_id, lot_code, date_from, date_to
 * @return resource DB result set
 */
function get_fsma_tracking_events($filters = array())
{
	$sql = "SELECT fe.*, sm.description AS item_name, sb.batch_no
		FROM " . TB_PREF . "fsma_tracking_events fe
		LEFT JOIN " . TB_PREF . "stock_master sm ON fe.stock_id = sm.stock_id
		LEFT JOIN " . TB_PREF . "stock_batches sb ON fe.batch_id = sb.id
		WHERE 1=1";

	if (!empty($filters['event_type']))
		$sql .= " AND fe.event_type=" . db_escape($filters['event_type']);
	if (!empty($filters['stock_id']))
		$sql .= " AND fe.stock_id=" . db_escape($filters['stock_id']);
	if (!empty($filters['batch_id']))
		$sql .= " AND fe.batch_id=" . (int)$filters['batch_id'];
	if (!empty($filters['lot_code']))
		$sql .= " AND fe.lot_code LIKE " . db_escape('%' . $filters['lot_code'] . '%');
	if (!empty($filters['date_from']))
		$sql .= " AND fe.event_date>=" . db_escape($filters['date_from']);
	if (!empty($filters['date_to']))
		$sql .= " AND fe.event_date<=" . db_escape($filters['date_to'] . ' 23:59:59');

	$sql .= " ORDER BY fe.event_date DESC, fe.id DESC";
	return db_query($sql, 'could not get FSMA tracking events');
}

// =====================================================================
// AUTOMATIC CTE GENERATION FROM TRANSACTIONS
// =====================================================================

/**
 * Auto-generate a "receiving" CTE when a GRN is processed for FSMA-regulated food.
 *
 * @param int    $grn_id      GRN batch id
 * @param string $stock_id    Item stock_id
 * @param int    $batch_id    Batch id
 * @param string $lot_code    Lot code (or batch_no)
 * @param double $quantity    Quantity received
 * @param string $unit        Unit of measure
 * @param int    $supplier_id Supplier id
 * @param string $loc_code    Receiving location
 * @return int|false Event id or false if not FSMA-regulated
 */
function auto_fsma_receiving_event($grn_id, $stock_id, $batch_id, $lot_code, $quantity, $unit, $supplier_id, $loc_code)
{
	if (!is_item_regulated($stock_id, REG_FSMA204))
		return false;

	$supplier = get_supplier($supplier_id);
	$supplier_name = $supplier ? $supplier['supp_name'] : '';

	$loc = get_item_location($loc_code);
	$loc_desc = $loc ? $loc['location_name'] : $loc_code;

	return add_fsma_tracking_event('receiving', $stock_id, $batch_id, $lot_code, array(
		'event_date'           => date('Y-m-d H:i:s'),
		'quantity'             => $quantity,
		'unit_of_measure'      => $unit,
		'reference_type'       => 'grn',
		'reference_id'         => $grn_id,
		'trading_partner_type' => 'supplier',
		'trading_partner_id'   => $supplier_id,
		'trading_partner_name' => $supplier_name,
		'loc_code'             => $loc_code,
		'location_description' => $loc_desc,
	));
}

/**
 * Auto-generate a "shipping" CTE when a delivery is processed for FSMA-regulated food.
 *
 * @param int    $delivery_id Delivery trans_no
 * @param string $stock_id    Item stock_id
 * @param int    $batch_id    Batch id
 * @param string $lot_code    Lot code
 * @param double $quantity    Quantity shipped
 * @param string $unit        Unit of measure
 * @param int    $customer_id Customer debtor_no
 * @param string $loc_code    Ship-from location
 * @return int|false Event id or false if not FSMA-regulated
 */
function auto_fsma_shipping_event($delivery_id, $stock_id, $batch_id, $lot_code, $quantity, $unit, $customer_id, $loc_code)
{
	if (!is_item_regulated($stock_id, REG_FSMA204))
		return false;

	$customer = get_customer($customer_id);
	$customer_name = $customer ? $customer['name'] : '';

	$loc = get_item_location($loc_code);
	$loc_desc = $loc ? $loc['location_name'] : $loc_code;

	return add_fsma_tracking_event('shipping', $stock_id, $batch_id, $lot_code, array(
		'event_date'           => date('Y-m-d H:i:s'),
		'quantity'             => $quantity,
		'unit_of_measure'      => $unit,
		'reference_type'       => 'delivery',
		'reference_id'         => $delivery_id,
		'trading_partner_type' => 'customer',
		'trading_partner_id'   => $customer_id,
		'trading_partner_name' => $customer_name,
		'loc_code'             => $loc_code,
		'location_description' => $loc_desc,
		'ship_date'            => date('Y-m-d'),
	));
}

// =====================================================================
// 24-HOUR RECALL CAPABILITY
// =====================================================================

/**
 * Rapid recall: Identify all affected customers and locations for a batch/lot
 * within the FSMA 24-hour response window.
 *
 * @param string $stock_id Item stock_id
 * @param string $lot_code Traceability Lot Code
 * @return array Recall response data:
 *   'affected_events'   => All CTEs for this lot
 *   'shipping_events'   => Shipping CTEs (customers who received product)
 *   'receiving_events'  => Receiving CTEs (where product was received)
 *   'customer_list'     => Unique customers affected
 *   'location_list'     => Unique locations affected
 *   'total_quantity'    => Total quantity shipped
 */
function fsma_rapid_recall_lookup($stock_id, $lot_code)
{
	$result = array(
		'affected_events' => array(),
		'shipping_events' => array(),
		'receiving_events' => array(),
		'customer_list'   => array(),
		'location_list'   => array(),
		'total_quantity'  => 0,
	);

	// Get all tracking events for this lot
	$sql = "SELECT fe.*, sm.description AS item_name
		FROM " . TB_PREF . "fsma_tracking_events fe
		LEFT JOIN " . TB_PREF . "stock_master sm ON fe.stock_id = sm.stock_id
		WHERE fe.stock_id=" . db_escape($stock_id)
		. " AND fe.lot_code=" . db_escape($lot_code)
		. " ORDER BY fe.event_date ASC";

	$qr = db_query($sql, 'could not perform FSMA rapid recall lookup');
	while ($row = db_fetch($qr)) {
		$result['affected_events'][] = $row;

		if ($row['event_type'] === 'shipping') {
			$result['shipping_events'][] = $row;
			$result['total_quantity'] += abs($row['quantity']);

			if ($row['trading_partner_name']) {
				$key = $row['trading_partner_id'] . '_' . $row['trading_partner_type'];
				if (!isset($result['customer_list'][$key])) {
					$result['customer_list'][$key] = array(
						'id'   => $row['trading_partner_id'],
						'name' => $row['trading_partner_name'],
						'type' => $row['trading_partner_type'],
					);
				}
			}
		}

		if ($row['event_type'] === 'receiving') {
			$result['receiving_events'][] = $row;
		}

		if ($row['location_description']) {
			$result['location_list'][$row['loc_code']] = $row['location_description'];
		}
	}

	// Also find from batch_movements if lot matches a batch_no
	$sql = "SELECT DISTINCT sb.id AS batch_id, sb.batch_no
		FROM " . TB_PREF . "stock_batches sb
		WHERE sb.stock_id=" . db_escape($stock_id)
		. " AND sb.batch_no=" . db_escape($lot_code);
	$qr = db_query($sql, 'could not find batches for FSMA recall');
	while ($batch = db_fetch($qr)) {
		// Get customers from serial numbers in this batch
		$sql2 = "SELECT DISTINCT sn.customer_id, dm.name AS customer_name
			FROM " . TB_PREF . "serial_numbers sn
			INNER JOIN " . TB_PREF . "debtors_master dm ON sn.customer_id = dm.debtor_no
			WHERE sn.batch_id=" . (int)$batch['batch_id']
			. " AND sn.customer_id IS NOT NULL";
		$qr2 = db_query($sql2, 'could not get batch customers for recall');
		while ($cust = db_fetch($qr2)) {
			$key = $cust['customer_id'] . '_customer';
			if (!isset($result['customer_list'][$key])) {
				$result['customer_list'][$key] = array(
					'id'   => $cust['customer_id'],
					'name' => $cust['customer_name'],
					'type' => 'customer',
				);
			}
		}
	}

	return $result;
}

/**
 * Get the forward trace for a lot code — where did it go?
 *
 * @param string $stock_id Item
 * @param string $lot_code Lot code
 * @return array Shipping events with customer details
 */
function fsma_forward_trace($stock_id, $lot_code)
{
	$sql = "SELECT fe.*, sm.description AS item_name
		FROM " . TB_PREF . "fsma_tracking_events fe
		LEFT JOIN " . TB_PREF . "stock_master sm ON fe.stock_id = sm.stock_id
		WHERE fe.stock_id=" . db_escape($stock_id)
		. " AND fe.lot_code=" . db_escape($lot_code)
		. " AND fe.event_type = 'shipping'
		ORDER BY fe.event_date ASC";
	$result = db_query($sql, 'could not get forward trace');
	$rows = array();
	while ($row = db_fetch($result))
		$rows[] = $row;
	return $rows;
}

/**
 * Get the reverse trace for a lot code — where did it come from?
 *
 * @param string $stock_id Item
 * @param string $lot_code Lot code
 * @return array Receiving and growing events with supplier details
 */
function fsma_reverse_trace($stock_id, $lot_code)
{
	$sql = "SELECT fe.*, sm.description AS item_name
		FROM " . TB_PREF . "fsma_tracking_events fe
		LEFT JOIN " . TB_PREF . "stock_master sm ON fe.stock_id = sm.stock_id
		WHERE fe.stock_id=" . db_escape($stock_id)
		. " AND fe.lot_code=" . db_escape($lot_code)
		. " AND fe.event_type IN ('growing', 'receiving')
		ORDER BY fe.event_date ASC";
	$result = db_query($sql, 'could not get reverse trace');
	$rows = array();
	while ($row = db_fetch($result))
		$rows[] = $row;
	return $rows;
}

/**
 * Get FSMA compliance statistics.
 *
 * @return array Statistics
 */
function get_fsma_compliance_stats()
{
	$stats = array(
		'total_events'   => 0,
		'events_by_type' => array(),
		'recent_events'  => array(),
		'lots_tracked'   => 0,
	);

	$sql = "SELECT event_type, COUNT(*) AS cnt FROM " . TB_PREF . "fsma_tracking_events
		GROUP BY event_type ORDER BY event_type";
	$result = db_query($sql, 'could not get FSMA stats');
	while ($row = db_fetch($result)) {
		$stats['events_by_type'][$row['event_type']] = (int)$row['cnt'];
		$stats['total_events'] += (int)$row['cnt'];
	}

	$sql = "SELECT COUNT(DISTINCT lot_code) FROM " . TB_PREF . "fsma_tracking_events
		WHERE lot_code IS NOT NULL AND lot_code != ''";
	$result = db_query($sql, 'could not get TLC count');
	$row = db_fetch_row($result);
	$stats['lots_tracked'] = (int)$row[0];

	// Recent events (last 10)
	$sql = "SELECT fe.*, sm.description AS item_name
		FROM " . TB_PREF . "fsma_tracking_events fe
		LEFT JOIN " . TB_PREF . "stock_master sm ON fe.stock_id = sm.stock_id
		ORDER BY fe.event_date DESC LIMIT 10";
	$result = db_query($sql, 'could not get recent FSMA events');
	while ($row = db_fetch($result))
		$stats['recent_events'][] = $row;

	return $stats;
}
