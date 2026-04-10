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
 * Warehouse Routes & Push/Pull Rules — CRUD, resolution, and auto-trigger logic.
 *
 * Tables: 0_wh_routes, 0_wh_route_rules
 *
 * A "route" defines a stock-flow pattern for a warehouse (inbound, outbound,
 * internal, or crossdock). Each route contains ordered "rules" (push or pull)
 * that describe individual operation steps (e.g., Receipt → QC → Putaway).
 *
 * Route resolution: given a warehouse + direction + optional item, find the
 * applicable route (item-specific > warehouse default > global default).
 *
 * Push rule execution: when stock arrives at a trigger location, auto-create
 * the next operation defined by the route's push rules.
 *
 * Session 13 of the Unified Advanced Inventory Implementation Plan.
 *
 * @package NotrinosERP
 * @subpackage Inventory/Warehouse
 */

//----------------------------------------------------------------------
// Route Type & Rule Type Constants
//----------------------------------------------------------------------

/**
 * Get valid route types.
 *
 * @return array Associative array of type_code => display_label
 */
function get_route_types() {
	return array(
		'inbound'   => _('Inbound'),
		'outbound'  => _('Outbound'),
		'internal'  => _('Internal'),
		'crossdock' => _('Cross-Dock'),
	);
}

/**
 * Get a CSS color for a route type.
 *
 * @param string $type Route type code
 * @return string CSS color
 */
function get_route_type_color($type) {
	$colors = array(
		'inbound'   => '#28a745',
		'outbound'  => '#007bff',
		'internal'  => '#ffc107',
		'crossdock' => '#6f42c1',
	);
	return isset($colors[$type]) ? $colors[$type] : '#6c757d';
}

/**
 * Get route type badge HTML.
 *
 * @param string $type Route type code
 * @return string HTML badge
 */
function route_type_badge($type) {
	$types = get_route_types();
	$label = isset($types[$type]) ? $types[$type] : $type;
	$color = get_route_type_color($type);
	return '<span style="display:inline-block;padding:2px 8px;border-radius:3px;'
		. 'background:' . $color . ';color:#fff;font-size:11px;font-weight:bold;">'
		. htmlspecialchars($label, ENT_QUOTES) . '</span>';
}

/**
 * Get valid rule types (push/pull).
 *
 * @return array Associative array of type_code => display_label
 */
function get_rule_types() {
	return array(
		'push' => _('Push'),
		'pull' => _('Pull'),
	);
}

/**
 * Get valid operation types for rules (subset of warehouse operations).
 *
 * @return array Associative array of op_type => display_label
 */
function get_rule_operation_types() {
	return array(
		'receipt'        => _('Receipt'),
		'quality_check'  => _('Quality Check'),
		'putaway'        => _('Putaway'),
		'pick'           => _('Pick'),
		'pack'           => _('Pack'),
		'ship'           => _('Ship'),
		'transfer'       => _('Transfer'),
	);
}

/**
 * Get valid trigger methods for rules.
 *
 * @return array Associative array of method => display_label
 */
function get_trigger_methods() {
	return array(
		'manual'    => _('Manual'),
		'auto'      => _('Automatic'),
		'scheduled' => _('Scheduled'),
	);
}

//----------------------------------------------------------------------
// CRUD: Routes
//----------------------------------------------------------------------

/**
 * Add a new warehouse route.
 *
 * @param string      $route_name          Route display name
 * @param string      $route_type          Route type (inbound/outbound/internal/crossdock)
 * @param string|null $warehouse_loc_code  Warehouse scope (null = global)
 * @param bool        $is_default          Whether this is the default route for the type+warehouse
 * @param int         $sequence            Sort priority (lower = higher priority)
 * @param bool        $active              Active flag
 * @param string      $description         Optional description
 * @return int        Inserted route_id
 */
function add_route($route_name, $route_type, $warehouse_loc_code = null,
	$is_default = false, $sequence = 10, $active = true, $description = '')
{
	// If setting as default, unset any existing default for this type+warehouse
	if ($is_default) {
		unset_default_route($route_type, $warehouse_loc_code);
	}

	$sql = "INSERT INTO " . TB_PREF . "wh_routes (
		route_name, route_type, warehouse_loc_code,
		is_default, sequence, active, description
	) VALUES (
		" . db_escape($route_name) . ",
		" . db_escape($route_type) . ",
		" . ($warehouse_loc_code ? db_escape($warehouse_loc_code) : 'NULL') . ",
		" . ($is_default ? 1 : 0) . ",
		" . (int)$sequence . ",
		" . ($active ? 1 : 0) . ",
		" . db_escape($description) . "
	)";
	db_query($sql, 'could not add warehouse route');
	return db_insert_id();
}

/**
 * Update an existing warehouse route.
 *
 * @param int         $route_id            Route to update
 * @param string      $route_name          Route display name
 * @param string      $route_type          Route type
 * @param string|null $warehouse_loc_code  Warehouse scope
 * @param bool        $is_default          Default flag
 * @param int         $sequence            Sort priority
 * @param bool        $active              Active flag
 * @param string      $description         Description
 */
function update_route($route_id, $route_name, $route_type, $warehouse_loc_code = null,
	$is_default = false, $sequence = 10, $active = true, $description = '')
{
	if ($is_default) {
		unset_default_route($route_type, $warehouse_loc_code, $route_id);
	}

	$sql = "UPDATE " . TB_PREF . "wh_routes SET "
		. "route_name=" . db_escape($route_name) . ","
		. "route_type=" . db_escape($route_type) . ","
		. "warehouse_loc_code=" . ($warehouse_loc_code ? db_escape($warehouse_loc_code) : 'NULL') . ","
		. "is_default=" . ($is_default ? 1 : 0) . ","
		. "sequence=" . (int)$sequence . ","
		. "active=" . ($active ? 1 : 0) . ","
		. "description=" . db_escape($description)
		. " WHERE route_id=" . (int)$route_id;
	db_query($sql, 'could not update warehouse route');
}

/**
 * Get a single route by ID.
 *
 * @param int $route_id Route ID
 * @return array|false Route row or false
 */
function get_route($route_id) {
	$sql = "SELECT r.*, l.location_name AS warehouse_name
		FROM " . TB_PREF . "wh_routes r
		LEFT JOIN " . TB_PREF . "locations l ON r.warehouse_loc_code = l.loc_code
		WHERE r.route_id=" . (int)$route_id;
	$result = db_query($sql, 'could not get route');
	return db_fetch($result);
}

/**
 * Get all routes, optionally filtered by type/warehouse/active.
 *
 * @param string|null $route_type          Filter by type
 * @param string|null $warehouse_loc_code  Filter by warehouse
 * @param bool        $show_inactive       Include inactive routes
 * @return resource   DB result
 */
function get_routes($route_type = null, $warehouse_loc_code = null, $show_inactive = false) {
	$sql = "SELECT r.*, l.location_name AS warehouse_name
		FROM " . TB_PREF . "wh_routes r
		LEFT JOIN " . TB_PREF . "locations l ON r.warehouse_loc_code = l.loc_code
		WHERE 1=1";

	if ($route_type) {
		$sql .= " AND r.route_type=" . db_escape($route_type);
	}
	if ($warehouse_loc_code) {
		$sql .= " AND r.warehouse_loc_code=" . db_escape($warehouse_loc_code);
	}
	if (!$show_inactive) {
		$sql .= " AND r.active=1";
	}
	$sql .= " ORDER BY r.sequence ASC, r.route_name ASC";
	return db_query($sql, 'could not get routes');
}

/**
 * Check if a route can be deleted.
 *
 * Blocks deletion if any warehouse operations reference this route.
 *
 * @param int $route_id Route ID
 * @return true|string True if can delete, or error message
 */
function can_delete_route($route_id) {
	// Check if any operations reference this route
	$sql = "SELECT COUNT(*) AS cnt FROM " . TB_PREF . "wh_operations WHERE route_id=" . (int)$route_id;
	$result = db_query($sql, 'could not check route references');
	$row = db_fetch($result);
	if ($row['cnt'] > 0) {
		return _('Cannot delete this route because it is referenced by existing warehouse operations.');
	}

	// Check if any location defaults to this route
	$sql = "SELECT COUNT(*) AS cnt FROM " . TB_PREF . "locations WHERE default_route_in=" . (int)$route_id
		. " OR default_route_out=" . (int)$route_id;
	$result = db_query($sql, 'could not check default route references');
	$row = db_fetch($result);
	if ($row['cnt'] > 0) {
		return _('Cannot delete this route because it is set as default on one or more inventory locations.');
	}

	return true;
}

/**
 * Delete a route and all its rules.
 *
 * @param int $route_id Route ID
 */
function delete_route($route_id) {
	// Delete rules first
	$sql = "DELETE FROM " . TB_PREF . "wh_route_rules WHERE route_id=" . (int)$route_id;
	db_query($sql, 'could not delete route rules');

	// Delete route
	$sql = "DELETE FROM " . TB_PREF . "wh_routes WHERE route_id=" . (int)$route_id;
	db_query($sql, 'could not delete route');
}

/**
 * Unset default flag for all routes of a given type+warehouse.
 *
 * @param string      $route_type          Route type
 * @param string|null $warehouse_loc_code  Warehouse code
 * @param int|null    $exclude_route_id    Route to exclude (during update)
 */
function unset_default_route($route_type, $warehouse_loc_code = null, $exclude_route_id = null) {
	$sql = "UPDATE " . TB_PREF . "wh_routes SET is_default=0 WHERE route_type=" . db_escape($route_type);
	if ($warehouse_loc_code) {
		$sql .= " AND warehouse_loc_code=" . db_escape($warehouse_loc_code);
	} else {
		$sql .= " AND warehouse_loc_code IS NULL";
	}
	if ($exclude_route_id) {
		$sql .= " AND route_id != " . (int)$exclude_route_id;
	}
	db_query($sql, 'could not unset default route');
}

/**
 * Count routes by type for summary display.
 *
 * @return array Associative array of type => count
 */
function get_route_type_summary() {
	$sql = "SELECT route_type, COUNT(*) AS cnt FROM " . TB_PREF . "wh_routes WHERE active=1 GROUP BY route_type";
	$result = db_query($sql, 'could not get route summary');
	$summary = array();
	while ($row = db_fetch($result)) {
		$summary[$row['route_type']] = (int)$row['cnt'];
	}
	return $summary;
}

//----------------------------------------------------------------------
// CRUD: Route Rules
//----------------------------------------------------------------------

/**
 * Add a rule to a route.
 *
 * @param int         $route_id        Parent route ID
 * @param string      $rule_type       Rule type (push/pull)
 * @param int         $sequence        Sort order within route
 * @param int|null    $from_loc_id     Source location (FK to 0_wh_locations)
 * @param int|null    $to_loc_id       Destination location (FK to 0_wh_locations)
 * @param string      $operation_type  Operation type (receipt/putaway/pick/etc.)
 * @param string      $trigger_method  Trigger method (manual/auto/scheduled)
 * @param int         $delay_days      Delay before execution (days)
 * @param bool        $active          Active flag
 * @return int        Inserted rule_id
 */
function add_route_rule($route_id, $rule_type, $sequence = 10, $from_loc_id = null,
	$to_loc_id = null, $operation_type = 'receipt', $trigger_method = 'manual',
	$delay_days = 0, $active = true)
{
	$sql = "INSERT INTO " . TB_PREF . "wh_route_rules (
		route_id, rule_type, sequence, from_loc_id, to_loc_id,
		operation_type, trigger_method, delay_days, active
	) VALUES (
		" . (int)$route_id . ",
		" . db_escape($rule_type) . ",
		" . (int)$sequence . ",
		" . ($from_loc_id ? (int)$from_loc_id : 'NULL') . ",
		" . ($to_loc_id ? (int)$to_loc_id : 'NULL') . ",
		" . db_escape($operation_type) . ",
		" . db_escape($trigger_method) . ",
		" . (int)$delay_days . ",
		" . ($active ? 1 : 0) . "
	)";
	db_query($sql, 'could not add route rule');
	return db_insert_id();
}

/**
 * Update an existing route rule.
 *
 * @param int         $rule_id         Rule to update
 * @param string      $rule_type       Rule type (push/pull)
 * @param int         $sequence        Sort order
 * @param int|null    $from_loc_id     Source location
 * @param int|null    $to_loc_id       Destination location
 * @param string      $operation_type  Operation type
 * @param string      $trigger_method  Trigger method
 * @param int         $delay_days      Delay days
 * @param bool        $active          Active flag
 */
function update_route_rule($rule_id, $rule_type, $sequence = 10, $from_loc_id = null,
	$to_loc_id = null, $operation_type = 'receipt', $trigger_method = 'manual',
	$delay_days = 0, $active = true)
{
	$sql = "UPDATE " . TB_PREF . "wh_route_rules SET "
		. "rule_type=" . db_escape($rule_type) . ","
		. "sequence=" . (int)$sequence . ","
		. "from_loc_id=" . ($from_loc_id ? (int)$from_loc_id : 'NULL') . ","
		. "to_loc_id=" . ($to_loc_id ? (int)$to_loc_id : 'NULL') . ","
		. "operation_type=" . db_escape($operation_type) . ","
		. "trigger_method=" . db_escape($trigger_method) . ","
		. "delay_days=" . (int)$delay_days . ","
		. "active=" . ($active ? 1 : 0)
		. " WHERE rule_id=" . (int)$rule_id;
	db_query($sql, 'could not update route rule');
}

/**
 * Get a single rule by ID.
 *
 * @param int $rule_id Rule ID
 * @return array|false Rule row or false
 */
function get_route_rule($rule_id) {
	$sql = "SELECT rr.*,
		fl.loc_name AS from_loc_name, fl.loc_code AS from_loc_code,
		tl.loc_name AS to_loc_name, tl.loc_code AS to_loc_code
		FROM " . TB_PREF . "wh_route_rules rr
		LEFT JOIN " . TB_PREF . "wh_locations fl ON rr.from_loc_id = fl.loc_id
		LEFT JOIN " . TB_PREF . "wh_locations tl ON rr.to_loc_id = tl.loc_id
		WHERE rr.rule_id=" . (int)$rule_id;
	$result = db_query($sql, 'could not get route rule');
	return db_fetch($result);
}

/**
 * Get all rules for a route, ordered by sequence.
 *
 * @param int  $route_id       Route ID
 * @param bool $active_only    Only return active rules
 * @return resource DB result
 */
function get_route_rules($route_id, $active_only = false) {
	$sql = "SELECT rr.*,
		fl.loc_name AS from_loc_name, fl.loc_code AS from_loc_code,
		tl.loc_name AS to_loc_name, tl.loc_code AS to_loc_code
		FROM " . TB_PREF . "wh_route_rules rr
		LEFT JOIN " . TB_PREF . "wh_locations fl ON rr.from_loc_id = fl.loc_id
		LEFT JOIN " . TB_PREF . "wh_locations tl ON rr.to_loc_id = tl.loc_id
		WHERE rr.route_id=" . (int)$route_id;

	if ($active_only) {
		$sql .= " AND rr.active=1";
	}
	$sql .= " ORDER BY rr.sequence ASC, rr.rule_id ASC";
	return db_query($sql, 'could not get route rules');
}

/**
 * Count rules for a route.
 *
 * @param int $route_id Route ID
 * @return int Rule count
 */
function count_route_rules($route_id) {
	$sql = "SELECT COUNT(*) AS cnt FROM " . TB_PREF . "wh_route_rules WHERE route_id=" . (int)$route_id;
	$result = db_query($sql, 'could not count rules');
	$row = db_fetch($result);
	return (int)$row['cnt'];
}

/**
 * Delete a single route rule.
 *
 * @param int $rule_id Rule ID
 */
function delete_route_rule($rule_id) {
	$sql = "DELETE FROM " . TB_PREF . "wh_route_rules WHERE rule_id=" . (int)$rule_id;
	db_query($sql, 'could not delete route rule');
}

/**
 * Delete all rules for a route.
 *
 * @param int $route_id Route ID
 */
function delete_route_rules($route_id) {
	$sql = "DELETE FROM " . TB_PREF . "wh_route_rules WHERE route_id=" . (int)$route_id;
	db_query($sql, 'could not delete route rules');
}

//----------------------------------------------------------------------
// Route Resolution
//----------------------------------------------------------------------

/**
 * Resolve the applicable route for a warehouse + direction + optional item.
 *
 * Resolution order (highest priority first):
 * 1. Location's default_route_in/out (if set on 0_locations)
 * 2. Route marked is_default=1 for this type + warehouse
 * 3. Route marked is_default=1 for this type + global (warehouse IS NULL)
 * 4. Fallback: first active route matching type + warehouse, sorted by sequence
 * 5. Fallback: first active global route matching type
 *
 * @param string      $warehouse_loc_code  Warehouse code (0_locations.loc_code)
 * @param string      $route_type          Route type (inbound/outbound/internal)
 * @param string|null $stock_id            Optional item code (for future item-specific routes)
 * @return array|false The route row or false if none found
 */
function resolve_route($warehouse_loc_code, $route_type, $stock_id = null) {
	// 1. Check location's default inbound/outbound route
	$route_col = ($route_type === 'inbound') ? 'default_route_in' : 'default_route_out';
	$sql = "SELECT $route_col FROM " . TB_PREF . "locations WHERE loc_code=" . db_escape($warehouse_loc_code);
	$result = db_query($sql, 'could not get location default route');
	$loc = db_fetch($result);
	if ($loc && $loc[$route_col]) {
		$route = get_route((int)$loc[$route_col]);
		if ($route && $route['active']) {
			return $route;
		}
	}

	// 2. Warehouse-specific default route
	$sql = "SELECT r.*, l.location_name AS warehouse_name
		FROM " . TB_PREF . "wh_routes r
		LEFT JOIN " . TB_PREF . "locations l ON r.warehouse_loc_code = l.loc_code
		WHERE r.route_type=" . db_escape($route_type)
		. " AND r.warehouse_loc_code=" . db_escape($warehouse_loc_code)
		. " AND r.is_default=1 AND r.active=1
		LIMIT 1";
	$result = db_query($sql, 'could not resolve warehouse default route');
	$row = db_fetch($result);
	if ($row) return $row;

	// 3. Global default route
	$sql = "SELECT r.*, l.location_name AS warehouse_name
		FROM " . TB_PREF . "wh_routes r
		LEFT JOIN " . TB_PREF . "locations l ON r.warehouse_loc_code = l.loc_code
		WHERE r.route_type=" . db_escape($route_type)
		. " AND r.warehouse_loc_code IS NULL
		AND r.is_default=1 AND r.active=1
		LIMIT 1";
	$result = db_query($sql, 'could not resolve global default route');
	$row = db_fetch($result);
	if ($row) return $row;

	// 4. First active route for warehouse by sequence
	$sql = "SELECT r.*, l.location_name AS warehouse_name
		FROM " . TB_PREF . "wh_routes r
		LEFT JOIN " . TB_PREF . "locations l ON r.warehouse_loc_code = l.loc_code
		WHERE r.route_type=" . db_escape($route_type)
		. " AND r.warehouse_loc_code=" . db_escape($warehouse_loc_code)
		. " AND r.active=1
		ORDER BY r.sequence ASC
		LIMIT 1";
	$result = db_query($sql, 'could not resolve warehouse route');
	$row = db_fetch($result);
	if ($row) return $row;

	// 5. First active global route by sequence
	$sql = "SELECT r.*, l.location_name AS warehouse_name
		FROM " . TB_PREF . "wh_routes r
		LEFT JOIN " . TB_PREF . "locations l ON r.warehouse_loc_code = l.loc_code
		WHERE r.route_type=" . db_escape($route_type)
		. " AND r.warehouse_loc_code IS NULL
		AND r.active=1
		ORDER BY r.sequence ASC
		LIMIT 1";
	$result = db_query($sql, 'could not resolve global route');
	return db_fetch($result);
}

/**
 * Get the active push rules for a route, ordered by sequence.
 * Used by the push rule engine to determine which operations to auto-create.
 *
 * @param int $route_id Route ID
 * @return array Array of rule rows
 */
function get_push_rules_for_route($route_id) {
	$sql = "SELECT rr.*,
		fl.loc_name AS from_loc_name, fl.loc_code AS from_loc_code,
		tl.loc_name AS to_loc_name, tl.loc_code AS to_loc_code
		FROM " . TB_PREF . "wh_route_rules rr
		LEFT JOIN " . TB_PREF . "wh_locations fl ON rr.from_loc_id = fl.loc_id
		LEFT JOIN " . TB_PREF . "wh_locations tl ON rr.to_loc_id = tl.loc_id
		WHERE rr.route_id=" . (int)$route_id
		. " AND rr.rule_type='push'
		AND rr.active=1
		ORDER BY rr.sequence ASC";
	$result = db_query($sql, 'could not get push rules');
	$rules = array();
	while ($row = db_fetch($result)) {
		$rules[] = $row;
	}
	return $rules;
}

//----------------------------------------------------------------------
// Push Rule Execution
//----------------------------------------------------------------------

/**
 * Execute push rules for a route after stock arrives at a trigger location.
 *
 * When an operation completes at a location, this function checks if the
 * route has any push rules that should auto-create the next operation.
 *
 * @param int         $route_id        Route ID
 * @param int         $completed_op_id The operation that just completed
 * @param int|null    $trigger_loc_id  Location where stock has arrived
 * @param int|null    $source_doc_type Source document type
 * @param int|null    $source_doc_no   Source document number
 * @param array       $items           Array of items: [{stock_id, qty, batch_id, serial_id, unit_cost}]
 * @return array      Result with 'created_ops' (IDs of newly created operations)
 */
function execute_push_rules($route_id, $completed_op_id, $trigger_loc_id = null,
	$source_doc_type = null, $source_doc_no = null, $items = array())
{
	$created_ops = array();
	$push_rules = get_push_rules_for_route($route_id);

	// Load operation functions
	$ops_db_path = dirname(__FILE__) . '/warehouse_operations_db.inc';
	if (file_exists($ops_db_path)) {
		include_once($ops_db_path);
	}

	foreach ($push_rules as $rule) {
		// Check if this rule's trigger location matches where stock arrived
		if ($trigger_loc_id && $rule['from_loc_id'] && (int)$rule['from_loc_id'] !== (int)$trigger_loc_id) {
			continue; // This rule doesn't apply to this location
		}

		// Skip if trigger method is 'manual' (user must start manually)
		if ($rule['trigger_method'] === 'manual') {
			continue;
		}

		$scheduled_date = null;
		if ($rule['delay_days'] > 0) {
			$scheduled_date = date('Y-m-d H:i:s', strtotime('+' . (int)$rule['delay_days'] . ' days'));
		} else {
			$scheduled_date = date('Y-m-d H:i:s');
		}

		$new_status = ($rule['trigger_method'] === 'auto') ? 'ready' : 'draft';

		if (!function_exists('add_wh_operation')) {
			continue;
		}

		// Create the operation
		$new_op_id = add_wh_operation(
			$rule['operation_type'],
			$new_status,
			$source_doc_type,
			$source_doc_no,
			$completed_op_id,
			$rule['from_loc_id'],
			$rule['to_loc_id'],
			$scheduled_date,
			null,     // assigned_to
			5,        // priority
			$route_id,
			sprintf(_('Auto-created by push rule #%d from route #%d'), $rule['rule_id'], $route_id)
		);

		if ($new_op_id) {
			// Link to completed operation
			if (function_exists('link_wh_operations')) {
				link_wh_operations($completed_op_id, $new_op_id);
			}

			// Add operation lines for each item
			if (function_exists('add_wh_operation_line') && !empty($items)) {
				foreach ($items as $item) {
					add_wh_operation_line(
						$new_op_id,
						$item['stock_id'],
						isset($item['qty']) ? $item['qty'] : 0,
						0,
						$rule['from_loc_id'],
						$rule['to_loc_id'],
						isset($item['batch_id']) ? $item['batch_id'] : null,
						isset($item['serial_id']) ? $item['serial_id'] : null,
						null,
						isset($item['unit_cost']) ? $item['unit_cost'] : 0
					);
				}
			}

			$created_ops[] = $new_op_id;
		}
	}

	return $created_ops;
}

//----------------------------------------------------------------------
// Default Route Templates
//----------------------------------------------------------------------

/**
 * Get pre-built route templates that can be installed with one click.
 *
 * @return array Array of templates: [key => {name, type, description, rules:[]}]
 */
function get_route_templates() {
	return array(
		'inbound_1step' => array(
			'name'        => _('1-Step Inbound (Direct Receipt)'),
			'type'        => 'inbound',
			'description' => _('Goods received directly to stock bin. Simplest flow for low-volume warehouses.'),
			'rules'       => array(
				array('rule_type' => 'push', 'sequence' => 10, 'operation_type' => 'receipt', 'trigger_method' => 'auto'),
			),
		),
		'inbound_2step' => array(
			'name'        => _('2-Step Inbound (Receipt + Putaway)'),
			'type'        => 'inbound',
			'description' => _('Goods received to staging area, then put away to stock bins. Standard flow.'),
			'rules'       => array(
				array('rule_type' => 'push', 'sequence' => 10, 'operation_type' => 'receipt', 'trigger_method' => 'auto'),
				array('rule_type' => 'push', 'sequence' => 20, 'operation_type' => 'putaway', 'trigger_method' => 'auto'),
			),
		),
		'inbound_3step' => array(
			'name'        => _('3-Step Inbound (Receipt + QC + Putaway)'),
			'type'        => 'inbound',
			'description' => _('Goods received to staging, quality checked, then put away. Full control flow.'),
			'rules'       => array(
				array('rule_type' => 'push', 'sequence' => 10, 'operation_type' => 'receipt',       'trigger_method' => 'auto'),
				array('rule_type' => 'push', 'sequence' => 20, 'operation_type' => 'quality_check', 'trigger_method' => 'auto'),
				array('rule_type' => 'push', 'sequence' => 30, 'operation_type' => 'putaway',       'trigger_method' => 'auto'),
			),
		),
		'outbound_1step' => array(
			'name'        => _('1-Step Outbound (Direct Ship)'),
			'type'        => 'outbound',
			'description' => _('Pick and ship in one step. For small warehouses or pre-packed items.'),
			'rules'       => array(
				array('rule_type' => 'push', 'sequence' => 10, 'operation_type' => 'ship', 'trigger_method' => 'auto'),
			),
		),
		'outbound_2step' => array(
			'name'        => _('2-Step Outbound (Pick + Ship)'),
			'type'        => 'outbound',
			'description' => _('Goods picked from bins, then shipped from dock. Standard outbound flow.'),
			'rules'       => array(
				array('rule_type' => 'push', 'sequence' => 10, 'operation_type' => 'pick', 'trigger_method' => 'auto'),
				array('rule_type' => 'push', 'sequence' => 20, 'operation_type' => 'ship', 'trigger_method' => 'auto'),
			),
		),
		'outbound_3step' => array(
			'name'        => _('3-Step Outbound (Pick + Pack + Ship)'),
			'type'        => 'outbound',
			'description' => _('Full outbound flow: pick from bins, pack at packing station, ship from dock.'),
			'rules'       => array(
				array('rule_type' => 'push', 'sequence' => 10, 'operation_type' => 'pick', 'trigger_method' => 'auto'),
				array('rule_type' => 'push', 'sequence' => 20, 'operation_type' => 'pack', 'trigger_method' => 'auto'),
				array('rule_type' => 'push', 'sequence' => 30, 'operation_type' => 'ship', 'trigger_method' => 'auto'),
			),
		),
		'crossdock' => array(
			'name'        => _('Cross-Dock (Receipt to Ship)'),
			'type'        => 'crossdock',
			'description' => _('Goods received and immediately shipped without storage. For pre-allocated orders.'),
			'rules'       => array(
				array('rule_type' => 'push', 'sequence' => 10, 'operation_type' => 'receipt', 'trigger_method' => 'auto'),
				array('rule_type' => 'push', 'sequence' => 20, 'operation_type' => 'ship',    'trigger_method' => 'auto'),
			),
		),
		'internal_transfer' => array(
			'name'        => _('Internal Transfer'),
			'type'        => 'internal',
			'description' => _('Move stock between locations within the same warehouse.'),
			'rules'       => array(
				array('rule_type' => 'push', 'sequence' => 10, 'operation_type' => 'transfer', 'trigger_method' => 'manual'),
			),
		),
	);
}

/**
 * Install a route template for a specific warehouse.
 *
 * @param string      $template_key       Template key from get_route_templates()
 * @param string|null $warehouse_loc_code  Warehouse code (null for global)
 * @param bool        $set_as_default      Set as default route for this type+warehouse
 * @return int|false   Created route_id or false on error
 */
function install_route_template($template_key, $warehouse_loc_code = null, $set_as_default = false) {
	$templates = get_route_templates();
	if (!isset($templates[$template_key])) {
		return false;
	}

	$template = $templates[$template_key];

	$route_id = add_route(
		$template['name'],
		$template['type'],
		$warehouse_loc_code,
		$set_as_default,
		10,
		true,
		$template['description']
	);

	if (!$route_id) return false;

	foreach ($template['rules'] as $rule_def) {
		add_route_rule(
			$route_id,
			$rule_def['rule_type'],
			$rule_def['sequence'],
			null, // from_loc_id — user configures via UI
			null, // to_loc_id — user configures via UI
			$rule_def['operation_type'],
			$rule_def['trigger_method'],
			0,    // delay_days
			true
		);
	}

	return $route_id;
}

/**
 * Get routes formatted for combo_input dropdown selector.
 *
 * @param string|null $route_type Filter by type
 * @return string SQL query for combo_input
 */
function get_routes_combo_sql($route_type = null) {
	$sql = "SELECT route_id, CONCAT(route_name, ' [', route_type, ']') AS display_name, "
		. "CASE WHEN active=0 THEN 1 ELSE 0 END AS inactive "
		. "FROM " . TB_PREF . "wh_routes WHERE 1=1";
	if ($route_type) {
		$sql .= " AND route_type=" . db_escape($route_type);
	}
	$sql .= " ORDER BY sequence ASC, route_name ASC";
	return $sql;
}
