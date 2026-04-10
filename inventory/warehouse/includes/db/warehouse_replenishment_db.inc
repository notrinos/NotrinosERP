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
 * Database functions for warehouse replenishment rules.
 *
 * CRUD operations on 0_wh_replenishment_rules table.
 * Used by the replenishment engine to evaluate triggers and generate suggestions.
 *
 * @package NotrinosERP
 * @subpackage Inventory/Warehouse
 */

//----------------------------------------------------------------------
// Rule Type & Supply Method Constants
//----------------------------------------------------------------------

/**
 * Get valid replenishment rule types.
 *
 * @return array rule_type => display label
 */
function get_replenishment_rule_types() {
	return array(
		'min_max'         => _('Min / Max'),
		'reorder_point'   => _('Reorder Point'),
		'mto'             => _('Make to Order'),
		'inter_warehouse' => _('Inter-Warehouse'),
		'pick_face'       => _('Pick-Face Refill'),
		'forecast'        => _('Forecast-Based'),
	);
}

/**
 * Get a display label for a rule type.
 *
 * @param string $type Rule type code
 * @return string Translated label
 */
function get_replenishment_rule_type_label($type) {
	$types = get_replenishment_rule_types();
	return isset($types[$type]) ? $types[$type] : $type;
}

/**
 * Get CSS color for a rule type.
 *
 * @param string $type Rule type code
 * @return string Hex color
 */
function get_replenishment_rule_type_color($type) {
	$colors = array(
		'min_max'         => '#007bff',
		'reorder_point'   => '#17a2b8',
		'mto'             => '#6f42c1',
		'inter_warehouse' => '#fd7e14',
		'pick_face'       => '#28a745',
		'forecast'        => '#20c997',
	);
	return isset($colors[$type]) ? $colors[$type] : '#6c757d';
}

/**
 * Get valid supply methods.
 *
 * @return array method_code => display label
 */
function get_replenishment_supply_methods() {
	return array(
		'purchase'    => _('Purchase Order'),
		'transfer'    => _('Transfer Order'),
		'manufacture' => _('Work Order'),
	);
}

/**
 * Get display label for a supply method.
 *
 * @param string $method Supply method code
 * @return string Translated label
 */
function get_replenishment_supply_method_label($method) {
	$methods = get_replenishment_supply_methods();
	return isset($methods[$method]) ? $methods[$method] : $method;
}

//----------------------------------------------------------------------
// CRUD: Replenishment Rules
//----------------------------------------------------------------------

/**
 * Add a new replenishment rule.
 *
 * @param string      $rule_name         Rule display name
 * @param string      $rule_type         min_max|reorder_point|mto|inter_warehouse|pick_face|forecast
 * @param string|null $stock_id          Specific item code (or null for category-wide)
 * @param int|null    $category_id       Item category (or null)
 * @param string|null $warehouse_loc_code Warehouse location code
 * @param int|null    $bin_loc_id        Pick-face bin (wh_locations.loc_id)
 * @param float|null  $min_qty           Minimum quantity threshold
 * @param float|null  $max_qty           Maximum quantity target
 * @param float|null  $reorder_qty       Fixed reorder quantity
 * @param float|null  $safety_stock      Safety stock level
 * @param int|null    $lead_time_days    Lead time in days
 * @param string|null $supply_warehouse  Source warehouse loc_code for transfers
 * @param string      $supply_method     purchase|transfer|manufacture
 * @param int|null    $preferred_supplier Preferred supplier ID
 * @param int         $auto_execute      0=suggest only, 1=auto-create
 * @return int Inserted rule_id
 */
function add_replenishment_rule($rule_name, $rule_type, $stock_id = null,
	$category_id = null, $warehouse_loc_code = null, $bin_loc_id = null,
	$min_qty = null, $max_qty = null, $reorder_qty = null, $safety_stock = null,
	$lead_time_days = null, $supply_warehouse = null, $supply_method = 'purchase',
	$preferred_supplier = null, $auto_execute = 0)
{
	$sql = "INSERT INTO " . TB_PREF . "wh_replenishment_rules (
		rule_name, rule_type, stock_id, category_id, warehouse_loc_code,
		bin_loc_id, min_qty, max_qty, reorder_qty, safety_stock,
		lead_time_days, supply_warehouse, supply_method,
		preferred_supplier, auto_execute, active
	) VALUES ("
		. db_escape($rule_name) . ","
		. db_escape($rule_type) . ","
		. ($stock_id ? db_escape($stock_id) : 'NULL') . ","
		. ($category_id ? (int)$category_id : 'NULL') . ","
		. ($warehouse_loc_code ? db_escape($warehouse_loc_code) : 'NULL') . ","
		. ($bin_loc_id ? (int)$bin_loc_id : 'NULL') . ","
		. ($min_qty !== null && $min_qty !== '' ? (float)$min_qty : 'NULL') . ","
		. ($max_qty !== null && $max_qty !== '' ? (float)$max_qty : 'NULL') . ","
		. ($reorder_qty !== null && $reorder_qty !== '' ? (float)$reorder_qty : 'NULL') . ","
		. ($safety_stock !== null && $safety_stock !== '' ? (float)$safety_stock : 'NULL') . ","
		. ($lead_time_days !== null && $lead_time_days !== '' ? (int)$lead_time_days : 'NULL') . ","
		. ($supply_warehouse ? db_escape($supply_warehouse) : 'NULL') . ","
		. db_escape($supply_method) . ","
		. ($preferred_supplier ? (int)$preferred_supplier : 'NULL') . ","
		. (int)$auto_execute . ","
		. "1"
		. ")";

	db_query($sql, 'could not add replenishment rule');
	return db_insert_id();
}

/**
 * Update an existing replenishment rule.
 *
 * @param int         $rule_id           Rule ID to update
 * @param string      $rule_name         Rule display name
 * @param string      $rule_type         Rule type code
 * @param string|null $stock_id          Item code
 * @param int|null    $category_id       Item category
 * @param string|null $warehouse_loc_code Warehouse location code
 * @param int|null    $bin_loc_id        Pick-face bin
 * @param float|null  $min_qty           Minimum qty
 * @param float|null  $max_qty           Maximum qty
 * @param float|null  $reorder_qty       Reorder qty
 * @param float|null  $safety_stock      Safety stock
 * @param int|null    $lead_time_days    Lead time
 * @param string|null $supply_warehouse  Supply warehouse
 * @param string      $supply_method     Supply method
 * @param int|null    $preferred_supplier Preferred supplier
 * @param int         $auto_execute      Auto-execute toggle
 * @param int         $active            Active toggle
 */
function update_replenishment_rule($rule_id, $rule_name, $rule_type,
	$stock_id = null, $category_id = null, $warehouse_loc_code = null,
	$bin_loc_id = null, $min_qty = null, $max_qty = null, $reorder_qty = null,
	$safety_stock = null, $lead_time_days = null, $supply_warehouse = null,
	$supply_method = 'purchase', $preferred_supplier = null,
	$auto_execute = 0, $active = 1)
{
	$sql = "UPDATE " . TB_PREF . "wh_replenishment_rules SET "
		. "rule_name=" . db_escape($rule_name) . ","
		. "rule_type=" . db_escape($rule_type) . ","
		. "stock_id=" . ($stock_id ? db_escape($stock_id) : 'NULL') . ","
		. "category_id=" . ($category_id ? (int)$category_id : 'NULL') . ","
		. "warehouse_loc_code=" . ($warehouse_loc_code ? db_escape($warehouse_loc_code) : 'NULL') . ","
		. "bin_loc_id=" . ($bin_loc_id ? (int)$bin_loc_id : 'NULL') . ","
		. "min_qty=" . ($min_qty !== null && $min_qty !== '' ? (float)$min_qty : 'NULL') . ","
		. "max_qty=" . ($max_qty !== null && $max_qty !== '' ? (float)$max_qty : 'NULL') . ","
		. "reorder_qty=" . ($reorder_qty !== null && $reorder_qty !== '' ? (float)$reorder_qty : 'NULL') . ","
		. "safety_stock=" . ($safety_stock !== null && $safety_stock !== '' ? (float)$safety_stock : 'NULL') . ","
		. "lead_time_days=" . ($lead_time_days !== null && $lead_time_days !== '' ? (int)$lead_time_days : 'NULL') . ","
		. "supply_warehouse=" . ($supply_warehouse ? db_escape($supply_warehouse) : 'NULL') . ","
		. "supply_method=" . db_escape($supply_method) . ","
		. "preferred_supplier=" . ($preferred_supplier ? (int)$preferred_supplier : 'NULL') . ","
		. "auto_execute=" . (int)$auto_execute . ","
		. "active=" . (int)$active
		. " WHERE rule_id=" . (int)$rule_id;

	db_query($sql, 'could not update replenishment rule');
}

/**
 * Get a single replenishment rule by ID.
 *
 * @param int $rule_id Rule ID
 * @return array|false Rule row or false
 */
function get_replenishment_rule($rule_id) {
	$sql = "SELECT r.*,
		sm.description AS item_description, sm.units AS item_units,
		sc.description AS category_name,
		l.location_name AS warehouse_name,
		wl.loc_name AS bin_name, wl.loc_code AS bin_code,
		sl.location_name AS supply_warehouse_name,
		sup.supp_name AS supplier_name
		FROM " . TB_PREF . "wh_replenishment_rules r
		LEFT JOIN " . TB_PREF . "stock_master sm ON r.stock_id = sm.stock_id
		LEFT JOIN " . TB_PREF . "stock_category sc ON r.category_id = sc.category_id
		LEFT JOIN " . TB_PREF . "locations l ON r.warehouse_loc_code = l.loc_code
		LEFT JOIN " . TB_PREF . "wh_locations wl ON r.bin_loc_id = wl.loc_id
		LEFT JOIN " . TB_PREF . "locations sl ON r.supply_warehouse = sl.loc_code
		LEFT JOIN " . TB_PREF . "suppliers sup ON r.preferred_supplier = sup.supplier_id
		WHERE r.rule_id=" . (int)$rule_id;

	$result = db_query($sql, 'could not get replenishment rule');
	return db_fetch($result);
}

/**
 * Get replenishment rules with optional filters.
 *
 * @param string|null $rule_type         Filter by rule type
 * @param string|null $warehouse_loc_code Filter by warehouse
 * @param string|null $stock_id          Filter by item
 * @param bool        $show_inactive     Include inactive rules
 * @return resource DB result
 */
function get_replenishment_rules($rule_type = null, $warehouse_loc_code = null,
	$stock_id = null, $show_inactive = false)
{
	$sql = "SELECT r.*,
		sm.description AS item_description,
		sc.description AS category_name,
		l.location_name AS warehouse_name,
		wl.loc_name AS bin_name, wl.loc_code AS bin_code,
		sl.location_name AS supply_warehouse_name,
		sup.supp_name AS supplier_name
		FROM " . TB_PREF . "wh_replenishment_rules r
		LEFT JOIN " . TB_PREF . "stock_master sm ON r.stock_id = sm.stock_id
		LEFT JOIN " . TB_PREF . "stock_category sc ON r.category_id = sc.category_id
		LEFT JOIN " . TB_PREF . "locations l ON r.warehouse_loc_code = l.loc_code
		LEFT JOIN " . TB_PREF . "wh_locations wl ON r.bin_loc_id = wl.loc_id
		LEFT JOIN " . TB_PREF . "locations sl ON r.supply_warehouse = sl.loc_code
		LEFT JOIN " . TB_PREF . "suppliers sup ON r.preferred_supplier = sup.supplier_id
		WHERE 1=1";

	if ($rule_type)
		$sql .= " AND r.rule_type=" . db_escape($rule_type);
	if ($warehouse_loc_code)
		$sql .= " AND r.warehouse_loc_code=" . db_escape($warehouse_loc_code);
	if ($stock_id)
		$sql .= " AND r.stock_id=" . db_escape($stock_id);
	if (!$show_inactive)
		$sql .= " AND r.active=1";

	$sql .= " ORDER BY r.rule_name";
	return db_query($sql, 'could not get replenishment rules');
}

/**
 * Delete a replenishment rule.
 *
 * @param int $rule_id Rule ID
 */
function delete_replenishment_rule($rule_id) {
	$sql = "DELETE FROM " . TB_PREF . "wh_replenishment_rules WHERE rule_id=" . (int)$rule_id;
	db_query($sql, 'could not delete replenishment rule');
}

/**
 * Check if a replenishment rule can be deleted.
 * Rules can always be deleted (they are configuration, not transactional).
 *
 * @param int $rule_id Rule ID
 * @return bool Always true
 */
function can_delete_replenishment_rule($rule_id) {
	return true;
}

/**
 * Toggle a rule's active state.
 *
 * @param int $rule_id Rule ID
 * @param int $active  1=active, 0=inactive
 */
function toggle_replenishment_rule_active($rule_id, $active) {
	$sql = "UPDATE " . TB_PREF . "wh_replenishment_rules SET active=" . (int)$active
		. " WHERE rule_id=" . (int)$rule_id;
	db_query($sql, 'could not toggle replenishment rule active');
}

/**
 * Toggle a rule's auto_execute flag.
 *
 * @param int $rule_id      Rule ID
 * @param int $auto_execute 1=auto, 0=suggest
 */
function toggle_replenishment_rule_auto_execute($rule_id, $auto_execute) {
	$sql = "UPDATE " . TB_PREF . "wh_replenishment_rules SET auto_execute=" . (int)$auto_execute
		. " WHERE rule_id=" . (int)$rule_id;
	db_query($sql, 'could not toggle replenishment auto-execute');
}

/**
 * Get active rules applicable to a specific item at a specific warehouse.
 *
 * Returns item-specific rules first, then category-level rules, then global rules.
 *
 * @param string      $stock_id          Item code
 * @param int         $category_id       Item category
 * @param string|null $warehouse_loc_code Warehouse filter
 * @return resource DB result
 */
function get_applicable_replenishment_rules($stock_id, $category_id, $warehouse_loc_code = null) {
	$sql = "SELECT r.* FROM " . TB_PREF . "wh_replenishment_rules r
		WHERE r.active = 1
		AND (
			r.stock_id = " . db_escape($stock_id) . "
			OR r.category_id = " . (int)$category_id . "
			OR (r.stock_id IS NULL AND r.category_id IS NULL)
		)";

	if ($warehouse_loc_code) {
		$sql .= " AND (r.warehouse_loc_code = " . db_escape($warehouse_loc_code)
			. " OR r.warehouse_loc_code IS NULL)";
	}

	// Item-specific first, then category, then global
	$sql .= " ORDER BY
		CASE WHEN r.stock_id IS NOT NULL THEN 0
		     WHEN r.category_id IS NOT NULL THEN 1
		     ELSE 2 END,
		CASE WHEN r.warehouse_loc_code IS NOT NULL THEN 0 ELSE 1 END";

	return db_query($sql, 'could not get applicable replenishment rules');
}

/**
 * Get current on-hand stock quantity for an item at a warehouse.
 *
 * Uses the standard stock_moves table aggregation.
 *
 * @param string $stock_id  Item code
 * @param string $loc_code  Location code
 * @return float Current on-hand quantity
 */
function get_replenishment_on_hand_qty($stock_id, $loc_code) {
	$sql = "SELECT COALESCE(SUM(qty), 0) AS on_hand
		FROM " . TB_PREF . "stock_moves
		WHERE stock_id=" . db_escape($stock_id)
		. " AND loc_code=" . db_escape($loc_code);

	$result = db_query($sql, 'could not get on-hand qty');
	$row = db_fetch($result);
	return (float)$row['on_hand'];
}

/**
 * Get incoming (on-order) quantity for an item at a warehouse.
 *
 * Sums qty ordered minus qty received from open purchase order lines.
 *
 * @param string $stock_id  Item code
 * @param string $loc_code  Location code
 * @return float Incoming quantity
 */
function get_replenishment_incoming_qty($stock_id, $loc_code) {
	$sql = "SELECT COALESCE(SUM(pod.quantity_ordered - pod.quantity_received), 0) AS incoming
		FROM " . TB_PREF . "purch_order_details pod
		JOIN " . TB_PREF . "purch_orders po ON pod.order_no = po.order_no
		WHERE pod.item_code=" . db_escape($stock_id)
		. " AND po.into_stock_location=" . db_escape($loc_code)
		. " AND pod.quantity_ordered > pod.quantity_received";

	$result = db_query($sql, 'could not get incoming qty');
	$row = db_fetch($result);
	return (float)$row['incoming'];
}

/**
 * Get outgoing (allocated/committed) quantity for an item at a warehouse.
 *
 * Sums open sales order demand (ordered minus sent).
 *
 * @param string $stock_id  Item code
 * @param string $loc_code  Location code
 * @return float Outgoing (committed) quantity
 */
function get_replenishment_outgoing_qty($stock_id, $loc_code) {
	$sql = "SELECT COALESCE(SUM(sod.quantity - sod.qty_sent), 0) AS outgoing
		FROM " . TB_PREF . "sales_order_details sod
		JOIN " . TB_PREF . "sales_orders so ON sod.order_no = so.order_no AND sod.trans_type = so.trans_type
		WHERE sod.stk_code=" . db_escape($stock_id)
		. " AND so.from_stk_loc=" . db_escape($loc_code)
		. " AND so.trans_type=30"
		. " AND sod.quantity > sod.qty_sent";

	$result = db_query($sql, 'could not get outgoing qty');
	$row = db_fetch($result);
	return (float)$row['outgoing'];
}

/**
 * Get bin-level stock quantity for a specific item at a specific bin.
 *
 * Used for pick-face replenishment checks.
 *
 * @param string $stock_id  Item code
 * @param int    $bin_loc_id Bin location ID (wh_locations.loc_id)
 * @return float Quantity at bin
 */
function get_replenishment_bin_qty($stock_id, $bin_loc_id) {
	$sql = "SELECT COALESCE(SUM(qty_on_hand), 0) AS bin_qty
		FROM " . TB_PREF . "wh_bin_stock
		WHERE stock_id=" . db_escape($stock_id)
		. " AND wh_loc_id=" . (int)$bin_loc_id;

	$result = db_query($sql, 'could not get bin stock qty');
	$row = db_fetch($result);
	return (float)$row['bin_qty'];
}

/**
 * Find the best bulk storage bin for an item in a given warehouse (for pick-face source).
 *
 * Returns the bin with the highest quantity of the item that is NOT the target pick-face bin.
 *
 * @param string $stock_id     Item code
 * @param string $loc_code     Warehouse location code
 * @param int    $exclude_bin  Bin to exclude (the pick-face bin itself)
 * @return array|false Bin info (wh_loc_id, location_code, qty_on_hand) or false
 */
function find_bulk_source_bin($stock_id, $loc_code, $exclude_bin) {
	$sql = "SELECT bs.wh_loc_id, wl.loc_code AS location_code, wl.loc_name AS location_name,
		SUM(bs.qty_on_hand) AS total_qty
		FROM " . TB_PREF . "wh_bin_stock bs
		JOIN " . TB_PREF . "wh_locations wl ON bs.wh_loc_id = wl.loc_id
		WHERE bs.stock_id=" . db_escape($stock_id)
		. " AND wl.warehouse_loc_code=" . db_escape($loc_code)
		. " AND bs.wh_loc_id <> " . (int)$exclude_bin
		. " AND bs.qty_on_hand > 0
		GROUP BY bs.wh_loc_id, wl.loc_code, wl.loc_name
		ORDER BY total_qty DESC
		LIMIT 1";

	$result = db_query($sql, 'could not find bulk source bin');
	return db_fetch($result);
}

/**
 * Get a summary count of rules by type.
 *
 * @param bool $active_only Only count active rules
 * @return array rule_type => count
 */
function get_replenishment_rule_summary($active_only = true) {
	$sql = "SELECT rule_type, COUNT(*) AS cnt
		FROM " . TB_PREF . "wh_replenishment_rules";
	if ($active_only)
		$sql .= " WHERE active=1";
	$sql .= " GROUP BY rule_type";

	$result = db_query($sql, 'could not get rule summary');
	$summary = array();
	while ($row = db_fetch($result)) {
		$summary[$row['rule_type']] = (int)$row['cnt'];
	}
	return $summary;
}

/**
 * Get all items with stock at a warehouse (for engine scanning).
 *
 * Returns items that have any stock moves at the given location.
 *
 * @param string $loc_code Location code
 * @return resource DB result with stock_id, description, category_id, units
 */
function get_stocked_items_at_location($loc_code) {
	$sql = "SELECT DISTINCT sm.stock_id, sm2.description, sm2.category_id, sm2.units
		FROM " . TB_PREF . "stock_moves sm
		JOIN " . TB_PREF . "stock_master sm2 ON sm.stock_id = sm2.stock_id
		WHERE sm.loc_code=" . db_escape($loc_code)
		. " AND sm2.mb_flag <> 'D'"
		. " ORDER BY sm2.description";

	return db_query($sql, 'could not get stocked items');
}

/**
 * Get all active pick-face replenishment rules.
 *
 * @return resource DB result
 */
function get_pick_face_replenishment_rules() {
	$sql = "SELECT r.*,
		sm.description AS item_description,
		wl.loc_code AS bin_code, wl.loc_name AS bin_name,
		wl.warehouse_loc_code
		FROM " . TB_PREF . "wh_replenishment_rules r
		LEFT JOIN " . TB_PREF . "stock_master sm ON r.stock_id = sm.stock_id
		LEFT JOIN " . TB_PREF . "wh_locations wl ON r.bin_loc_id = wl.loc_id
		WHERE r.rule_type='pick_face' AND r.active=1
		ORDER BY r.rule_name";

	return db_query($sql, 'could not get pick-face rules');
}
