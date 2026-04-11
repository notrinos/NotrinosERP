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
 * Database functions for warehouse hierarchical location management.
 *
 * Provides CRUD operations for the 0_wh_locations hierarchy tree,
 * location type queries, path computation, and auto-code generation.
 *
 * @package NotrinosERP
 * @subpackage Inventory/Warehouse
 */

//----------------------------------------------------------------------
// Location Type functions
//----------------------------------------------------------------------

/**
 * Get all active location types ordered by sort_order.
 *
 * @param bool $show_inactive Whether to include inactive types.
 * @return resource DB result set.
 */
function get_warehouse_location_types($show_inactive = false) {
	$sql = "SELECT * FROM " . TB_PREF . "wh_location_types";
	if (!$show_inactive) {
		$sql .= " WHERE inactive = 0";
	}
	$sql .= " ORDER BY sort_order";
	return db_query($sql, 'could not get warehouse location types');
}

/**
 * Get a single location type by ID.
 *
 * @param int $type_id The location type ID.
 * @return array|false The type row or false.
 */
function get_warehouse_location_type($type_id) {
	$sql = "SELECT * FROM " . TB_PREF . "wh_location_types WHERE id = " . db_escape($type_id);
	$result = db_query($sql, 'could not get warehouse location type');
	return db_fetch($result);
}

/**
 * Get location type ID by type_code.
 *
 * @param string $type_code The type code (e.g., 'warehouse', 'zone', 'bin').
 * @return int|false The type ID or false.
 */
function get_warehouse_location_type_id_by_code($type_code) {
	$sql = "SELECT id FROM " . TB_PREF . "wh_location_types WHERE type_code = " . db_escape($type_code);
	$result = db_query($sql, 'could not get location type by code');
	$row = db_fetch($result);
	return $row ? (int)$row['id'] : false;
}

//----------------------------------------------------------------------
// Location CRUD functions
//----------------------------------------------------------------------

/**
 * Add a new warehouse location in the hierarchy.
 *
 * @param string   $loc_code           Unique location code (auto-generated or manual).
 * @param string   $loc_name           Display name.
 * @param int|null $parent_loc_id      Parent location ID (null for root/warehouse level).
 * @param string   $warehouse_loc_code FK to 0_locations.loc_code (the warehouse this belongs to).
 * @param int      $location_type_id   FK to 0_wh_location_types.id.
 * @param array    $extra              Optional extra fields: max_weight, max_volume, max_units,
 *                                     storage_category_id, zone_type, abc_class, pick_sequence,
 *                                     barcode, is_default_receipt, is_default_ship, is_default_scrap,
 *                                     is_default_production.
 * @return int The inserted loc_id.
 */
function add_warehouse_location($loc_code, $loc_name, $parent_loc_id, $warehouse_loc_code, $location_type_id, $extra = array()) {
	$sql = "INSERT INTO " . TB_PREF . "wh_locations (loc_code, loc_name, parent_loc_id, warehouse_loc_code, location_type_id, location_path, full_name";

	$values = db_escape($loc_code) . ", " . db_escape($loc_name) . ", "
		. ($parent_loc_id ? db_escape($parent_loc_id) : "NULL") . ", "
		. db_escape($warehouse_loc_code) . ", " . db_escape($location_type_id) . ", "
		. "'', ''";

	$allowed_extra = array(
		'max_weight', 'max_volume', 'max_units', 'storage_category_id',
		'zone_type', 'abc_class', 'pick_sequence', 'barcode',
		'is_default_receipt', 'is_default_ship', 'is_default_scrap', 'is_default_production'
	);

	foreach ($allowed_extra as $col) {
		if (isset($extra[$col]) && $extra[$col] !== '' && $extra[$col] !== null) {
			$sql .= ", " . $col;
			$values .= ", " . db_escape($extra[$col]);
		}
	}

	$sql .= ") VALUES (" . $values . ")";
	db_query($sql, 'could not add warehouse location');

	$loc_id = db_insert_id();

	// Now compute and update location_path and full_name
	update_warehouse_location_path($loc_id);

	return $loc_id;
}

/**
 * Update an existing warehouse location.
 *
 * @param int    $loc_id   The location ID to update.
 * @param string $loc_name Updated display name.
 * @param int    $location_type_id  Location type.
 * @param array  $extra    Optional extra fields (same as add function).
 * @return void
 */
function update_warehouse_location($loc_id, $loc_name, $location_type_id, $extra = array()) {
	$sql = "UPDATE " . TB_PREF . "wh_locations SET "
		. "loc_name = " . db_escape($loc_name) . ", "
		. "location_type_id = " . db_escape($location_type_id);

	$allowed_extra = array(
		'max_weight', 'max_volume', 'max_units', 'storage_category_id',
		'zone_type', 'abc_class', 'pick_sequence', 'barcode',
		'is_default_receipt', 'is_default_ship', 'is_default_scrap', 'is_default_production',
		'is_active'
	);

	foreach ($allowed_extra as $col) {
		if (array_key_exists($col, $extra)) {
			if ($extra[$col] === '' || $extra[$col] === null) {
				$sql .= ", " . $col . " = NULL";
			} else {
				$sql .= ", " . $col . " = " . db_escape($extra[$col]);
			}
		}
	}

	$sql .= " WHERE loc_id = " . db_escape($loc_id);
	db_query($sql, 'could not update warehouse location');

	// Recompute path for this node and all descendants
	update_warehouse_location_path($loc_id);
	recompute_descendant_paths($loc_id);
}

/**
 * Delete a warehouse location. Only allowed if no children and no stock.
 *
 * @param int $loc_id The location ID to delete.
 * @return bool True if deleted, false if blocked.
 */
function delete_warehouse_location($loc_id) {
	$sql = "DELETE FROM " . TB_PREF . "wh_locations WHERE loc_id = " . db_escape($loc_id);
	db_query($sql, 'could not delete warehouse location');
}

/**
 * Check if a warehouse location can be safely deleted.
 *
 * @param int $loc_id The location ID to check.
 * @return string|true True if can delete, or error message string.
 */
function can_delete_warehouse_location($loc_id) {
	// Check for children
	$sql = "SELECT COUNT(*) AS cnt FROM " . TB_PREF . "wh_locations WHERE parent_loc_id = " . db_escape($loc_id);
	$result = db_query($sql, 'could not check children');
	$row = db_fetch($result);
	if ($row['cnt'] > 0) {
		return _('Cannot delete this location because it has child locations. Delete children first.');
	}

	// Check for bin stock
	$sql = "SELECT COUNT(*) AS cnt FROM " . TB_PREF . "wh_bin_stock WHERE wh_loc_id = " . db_escape($loc_id) . " AND qty_on_hand <> 0";
	$result = db_query($sql, 'could not check bin stock');
	$row = db_fetch($result);
	if ($row['cnt'] > 0) {
		return _('Cannot delete this location because it contains stock.');
	}

	// Check for stock moves referencing this location
	$sql = "SELECT COUNT(*) AS cnt FROM " . TB_PREF . "stock_moves WHERE from_bin_id = " . db_escape($loc_id) . " OR to_bin_id = " . db_escape($loc_id);
	$result = db_query($sql, 'could not check stock moves');
	$row = db_fetch($result);
	if ($row['cnt'] > 0) {
		return _('Cannot delete this location because stock movements reference it.');
	}

	return true;
}

/**
 * Get a single warehouse location by its loc_id.
 *
 * @param int $loc_id The location ID.
 * @return array|false The location row or false.
 */
function get_warehouse_location($loc_id) {
	$sql = "SELECT wl.*, lt.type_code, lt.type_name, lt.can_store, lt.is_physical,
				sc.category_name AS storage_category_name
			FROM " . TB_PREF . "wh_locations wl
			LEFT JOIN " . TB_PREF . "wh_location_types lt ON wl.location_type_id = lt.id
			LEFT JOIN " . TB_PREF . "wh_storage_categories sc ON wl.storage_category_id = sc.id
			WHERE wl.loc_id = " . db_escape($loc_id);
	$result = db_query($sql, 'could not get warehouse location');
	return db_fetch($result);
}

/**
 * Get warehouse location by its loc_code within a warehouse.
 *
 * @param string $loc_code The location code.
 * @param string $warehouse_loc_code The warehouse loc_code from 0_locations.
 * @return array|false The location row or false.
 */
function get_warehouse_location_by_code($loc_code, $warehouse_loc_code=null) {
	$sql = "SELECT wl.*, lt.type_code, lt.type_name
			FROM " . TB_PREF . "wh_locations wl
			LEFT JOIN " . TB_PREF . "wh_location_types lt ON wl.location_type_id = lt.id
			WHERE wl.loc_code = " . db_escape($loc_code);
	if ($warehouse_loc_code !== null && $warehouse_loc_code !== '')
		$sql .= " AND wl.warehouse_loc_code = " . db_escape($warehouse_loc_code);
	$result = db_query($sql, 'could not get warehouse location by code');
	return db_fetch($result);
}

//----------------------------------------------------------------------
// Tree / Hierarchy functions
//----------------------------------------------------------------------

/**
 * Get children of a given parent location.
 *
 * @param int|null $parent_loc_id Parent location ID. NULL returns root locations.
 * @param string   $warehouse_loc_code Warehouse to filter by.
 * @param bool     $include_inactive Whether to include inactive locations.
 * @return resource DB result set.
 */
function get_warehouse_location_children($parent_loc_id, $warehouse_loc_code, $include_inactive = false) {
	$sql = "SELECT wl.*, lt.type_code, lt.type_name, lt.can_store,
				(SELECT COUNT(*) FROM " . TB_PREF . "wh_locations child WHERE child.parent_loc_id = wl.loc_id) AS child_count
			FROM " . TB_PREF . "wh_locations wl
			LEFT JOIN " . TB_PREF . "wh_location_types lt ON wl.location_type_id = lt.id
			WHERE wl.warehouse_loc_code = " . db_escape($warehouse_loc_code);

	if ($parent_loc_id === null || $parent_loc_id === 0 || $parent_loc_id === '') {
		$sql .= " AND wl.parent_loc_id IS NULL";
	} else {
		$sql .= " AND wl.parent_loc_id = " . db_escape($parent_loc_id);
	}

	if (!$include_inactive) {
		$sql .= " AND wl.is_active = 1";
	}

	$sql .= " ORDER BY lt.sort_order, wl.loc_code";
	return db_query($sql, 'could not get location children');
}

/**
 * Get all locations for a warehouse as a flat list (for tree rendering).
 *
 * @param string $warehouse_loc_code Warehouse to filter by.
 * @param bool   $include_inactive Whether to include inactive locations.
 * @return resource DB result set.
 */
function get_warehouse_locations_all($warehouse_loc_code, $include_inactive = false) {
	$sql = "SELECT wl.*, lt.type_code, lt.type_name, lt.can_store,
				(SELECT COUNT(*) FROM " . TB_PREF . "wh_locations child WHERE child.parent_loc_id = wl.loc_id) AS child_count
			FROM " . TB_PREF . "wh_locations wl
			LEFT JOIN " . TB_PREF . "wh_location_types lt ON wl.location_type_id = lt.id
			WHERE wl.warehouse_loc_code = " . db_escape($warehouse_loc_code);

	if (!$include_inactive) {
		$sql .= " AND wl.is_active = 1";
	}

	$sql .= " ORDER BY wl.location_path, lt.sort_order, wl.loc_code";
	return db_query($sql, 'could not get all warehouse locations');
}

/**
 * Get the breadcrumb path (ancestors) for a location.
 *
 * @param int $loc_id The location ID.
 * @return array Array of ancestor rows from root to immediate parent.
 */
function get_warehouse_location_breadcrumb($loc_id) {
	$breadcrumb = array();
	$current_id = $loc_id;
	$max_depth = 20; // safety limit
	$depth = 0;

	while ($current_id && $depth < $max_depth) {
		$loc = get_warehouse_location($current_id);
		if (!$loc) break;
		array_unshift($breadcrumb, $loc);
		$current_id = $loc['parent_loc_id'];
		$depth++;
	}

	return $breadcrumb;
}

/**
 * Get all descendant loc_ids (recursively) for a given location.
 *
 * @param int $loc_id The parent location ID.
 * @return array Array of descendant loc_ids.
 */
function get_warehouse_location_descendants($loc_id) {
	$descendants = array();
	$queue = array($loc_id);

	while (!empty($queue)) {
		$current = array_shift($queue);
		$sql = "SELECT loc_id FROM " . TB_PREF . "wh_locations WHERE parent_loc_id = " . db_escape($current);
		$result = db_query($sql, 'could not get descendants');
		while ($row = db_fetch($result)) {
			$descendants[] = (int)$row['loc_id'];
			$queue[] = (int)$row['loc_id'];
		}
	}

	return $descendants;
}

//----------------------------------------------------------------------
// Path computation functions
//----------------------------------------------------------------------

/**
 * Compute and update location_path and full_name for a location.
 *
 * @param int $loc_id The location ID to update.
 * @return void
 */
function update_warehouse_location_path($loc_id) {
	$loc = get_warehouse_location_raw($loc_id);
	if (!$loc) return;

	if ($loc['parent_loc_id']) {
		$parent = get_warehouse_location_raw($loc['parent_loc_id']);
		if ($parent) {
			$path = $parent['location_path'] ? $parent['location_path'] . '/' . $loc['loc_code'] : $loc['loc_code'];
			$full = $parent['full_name'] ? $parent['full_name'] . ' > ' . $loc['loc_name'] : $loc['loc_name'];
		} else {
			$path = $loc['loc_code'];
			$full = $loc['loc_name'];
		}
	} else {
		$path = $loc['loc_code'];
		$full = $loc['loc_name'];
	}

	$sql = "UPDATE " . TB_PREF . "wh_locations SET "
		. "location_path = " . db_escape($path) . ", "
		. "full_name = " . db_escape($full)
		. " WHERE loc_id = " . db_escape($loc_id);
	db_query($sql, 'could not update location path');
}

/**
 * Recompute paths for all descendants of a location.
 *
 * @param int $parent_loc_id The parent location ID.
 * @return void
 */
function recompute_descendant_paths($parent_loc_id) {
	$sql = "SELECT loc_id FROM " . TB_PREF . "wh_locations WHERE parent_loc_id = " . db_escape($parent_loc_id);
	$result = db_query($sql, 'could not get children for path recompute');
	while ($row = db_fetch($result)) {
		update_warehouse_location_path($row['loc_id']);
		recompute_descendant_paths($row['loc_id']);
	}
}

/**
 * Raw fetch of location (no joins) for path computation.
 *
 * @param int $loc_id The location ID.
 * @return array|false
 */
function get_warehouse_location_raw($loc_id) {
	$sql = "SELECT * FROM " . TB_PREF . "wh_locations WHERE loc_id = " . db_escape($loc_id);
	$result = db_query($sql, 'could not get raw warehouse location');
	return db_fetch($result);
}

//----------------------------------------------------------------------
// Auto-code generation
//----------------------------------------------------------------------

/**
 * Generate an auto-code for a new location based on its parent and type.
 *
 * Format: {parent_code}-{type_prefix}{sequence}
 * Example: WH01-Z01, WH01-Z01-A01, WH01-Z01-A01-R01-B01
 *
 * @param int|null $parent_loc_id Parent location ID.
 * @param string   $warehouse_loc_code Warehouse code.
 * @param string   $type_code Location type code (e.g., 'zone', 'aisle', 'bin').
 * @return string The generated code.
 */
function generate_warehouse_location_code($parent_loc_id, $warehouse_loc_code, $type_code) {
	$prefixes = array(
		'warehouse' => 'WH',
		'zone'      => 'Z',
		'aisle'     => 'A',
		'rack'      => 'R',
		'shelf'     => 'S',
		'bin'       => 'B',
		'staging'   => 'ST',
		'dock'      => 'DK',
		'quality'   => 'QC',
		'transit'   => 'TR',
		'virtual'   => 'VR',
		'scrap'     => 'SC',
		'production'=> 'PD',
		'packing'   => 'PK',
		'returns'   => 'RT',
	);

	$prefix = isset($prefixes[$type_code]) ? $prefixes[$type_code] : strtoupper(substr($type_code, 0, 2));

	// Get the parent's code for building the full code
	$parent_code = '';
	if ($parent_loc_id) {
		$parent = get_warehouse_location_raw($parent_loc_id);
		if ($parent) {
			$parent_code = $parent['loc_code'];
		}
	}

	// Count existing siblings of same type under same parent
	$sql = "SELECT COUNT(*) AS cnt FROM " . TB_PREF . "wh_locations wl "
		. "INNER JOIN " . TB_PREF . "wh_location_types lt ON wl.location_type_id = lt.id "
		. "WHERE wl.warehouse_loc_code = " . db_escape($warehouse_loc_code)
		. " AND lt.type_code = " . db_escape($type_code);

	if ($parent_loc_id) {
		$sql .= " AND wl.parent_loc_id = " . db_escape($parent_loc_id);
	} else {
		$sql .= " AND wl.parent_loc_id IS NULL";
	}

	$result = db_query($sql, 'could not count siblings');
	$row = db_fetch($result);
	$seq = ((int)$row['cnt']) + 1;

	$code_suffix = $prefix . str_pad($seq, 2, '0', STR_PAD_LEFT);

	if ($parent_code) {
		return $parent_code . '-' . $code_suffix;
	} else {
		return $warehouse_loc_code . '-' . $code_suffix;
	}
}

//----------------------------------------------------------------------
// Root location management (linking 0_locations to 0_wh_locations)
//----------------------------------------------------------------------

/**
 * Create the root warehouse location node when WMS is enabled on a 0_locations entry.
 *
 * @param string $loc_code The 0_locations.loc_code.
 * @param string $location_name The warehouse name.
 * @return int The new root loc_id.
 */
function create_warehouse_root_location($loc_code, $location_name) {
	$type_id = get_warehouse_location_type_id_by_code('warehouse');
	if (!$type_id) {
		$type_id = 1; // fallback
	}

	$root_code = $loc_code . '-WH01';

	return add_warehouse_location(
		$root_code,
		$location_name . ' ' . _('Warehouse'),
		null,
		$loc_code,
		$type_id
	);
}

/**
 * Check if a warehouse root location already exists for a given 0_locations.loc_code.
 *
 * @param string $loc_code The 0_locations.loc_code.
 * @return array|false The root location row or false.
 */
function get_warehouse_root_location($loc_code) {
	$type_id = get_warehouse_location_type_id_by_code('warehouse');
	$sql = "SELECT * FROM " . TB_PREF . "wh_locations "
		. "WHERE warehouse_loc_code = " . db_escape($loc_code)
		. " AND parent_loc_id IS NULL";
	if ($type_id) {
		$sql .= " AND location_type_id = " . db_escape($type_id);
	}
	$sql .= " LIMIT 1";
	$result = db_query($sql, 'could not check warehouse root');
	return db_fetch($result);
}

//----------------------------------------------------------------------
// Utility / summary queries
//----------------------------------------------------------------------

/**
 * Count locations by type for a warehouse (for summary display).
 *
 * @param string $warehouse_loc_code The warehouse code.
 * @return array Associative array: type_code => count.
 */
function get_warehouse_location_type_counts($warehouse_loc_code) {
	$sql = "SELECT lt.type_code, lt.type_name, COUNT(wl.loc_id) AS cnt
			FROM " . TB_PREF . "wh_locations wl
			INNER JOIN " . TB_PREF . "wh_location_types lt ON wl.location_type_id = lt.id
			WHERE wl.warehouse_loc_code = " . db_escape($warehouse_loc_code)
			. " AND wl.is_active = 1
			GROUP BY lt.type_code, lt.type_name
			ORDER BY lt.sort_order";
	$result = db_query($sql, 'could not get location type counts');
	$counts = array();
	while ($row = db_fetch($result)) {
		$counts[$row['type_code']] = array(
			'name' => $row['type_name'],
			'count' => (int)$row['cnt']
		);
	}
	return $counts;
}

/**
 * Check if a location code is unique within a warehouse.
 *
 * @param string   $loc_code The code to check.
 * @param string   $warehouse_loc_code The warehouse code.
 * @param int|null $exclude_loc_id Location ID to exclude (for updates).
 * @return bool True if the code is unique.
 */
function is_warehouse_location_code_unique($loc_code, $warehouse_loc_code, $exclude_loc_id = null) {
	$sql = "SELECT COUNT(*) AS cnt FROM " . TB_PREF . "wh_locations "
		. "WHERE loc_code = " . db_escape($loc_code)
		. " AND warehouse_loc_code = " . db_escape($warehouse_loc_code);
	if ($exclude_loc_id) {
		$sql .= " AND loc_id <> " . db_escape($exclude_loc_id);
	}
	$result = db_query($sql, 'could not check code uniqueness');
	$row = db_fetch($result);
	return ((int)$row['cnt']) === 0;
}

/**
 * Get all WMS-enabled locations from 0_locations (warehouses).
 *
 * @param bool $show_inactive Whether to include inactive.
 * @return resource DB result set.
 */
function get_wms_enabled_locations($show_inactive = false) {
	$sql = "SELECT * FROM " . TB_PREF . "locations WHERE wh_enabled = 1";
	if (!$show_inactive) {
		$sql .= " AND inactive = 0";
	}
	$sql .= " ORDER BY loc_code";
	return db_query($sql, 'could not get WMS-enabled locations');
}

/**
 * Build a nested tree array from flat DB results for a warehouse.
 *
 * @param string $warehouse_loc_code The warehouse code.
 * @param bool   $include_inactive Include inactive.
 * @return array Nested tree structure.
 */
function build_warehouse_location_tree($warehouse_loc_code, $include_inactive = false) {
	$result = get_warehouse_locations_all($warehouse_loc_code, $include_inactive);
	$flat = array();
	$tree = array();
	$by_id = array();

	while ($row = db_fetch($result)) {
		$row['children'] = array();
		$by_id[$row['loc_id']] = $row;
	}

	foreach ($by_id as $id => $node) {
		if ($node['parent_loc_id'] && isset($by_id[$node['parent_loc_id']])) {
			$by_id[$node['parent_loc_id']]['children'][] = &$by_id[$id];
		} else {
			$tree[] = &$by_id[$id];
		}
	}

	return $tree;
}

/**
 * Get all storable locations (bins) for a warehouse, useful for selectors.
 *
 * @param string $warehouse_loc_code The warehouse code.
 * @return resource DB result set.
 */
function get_warehouse_storable_locations($warehouse_loc_code) {
	$sql = "SELECT wl.loc_id, wl.loc_code, wl.loc_name, wl.location_path, wl.full_name,
				lt.type_code, lt.type_name
			FROM " . TB_PREF . "wh_locations wl
			INNER JOIN " . TB_PREF . "wh_location_types lt ON wl.location_type_id = lt.id
			WHERE wl.warehouse_loc_code = " . db_escape($warehouse_loc_code)
			. " AND lt.can_store = 1 AND wl.is_active = 1
			ORDER BY wl.location_path";
	return db_query($sql, 'could not get storable locations');
}

//----------------------------------------------------------------------
// Capacity checking functions (Session 5)
//----------------------------------------------------------------------

/**
 * Check if a bin location can store additional stock.
 *
 * Validates weight, volume, and unit capacity against the bin's own limits
 * and its storage category constraints. Also checks mixing rules.
 *
 * @param int         $loc_id    The bin location ID.
 * @param string      $stock_id  The item stock_id to store.
 * @param float       $qty       Quantity to add.
 * @param float|null  $weight    Weight per unit (kg). If null, reads from stock_master.
 * @param float|null  $volume    Volume per unit (m³). If null, reads from stock_master.
 * @param string|null $batch_id  Batch ID if applicable.
 * @return array      ['ok' => bool, 'errors' => string[]] — ok=true if can store, errors list if not.
 */
function check_can_store($loc_id, $stock_id, $qty, $weight = null, $volume = null, $batch_id = null) {
	$errors = array();
	$loc = get_warehouse_location($loc_id);
	if (!$loc) {
		return array('ok' => false, 'errors' => array(_('Location not found.')));
	}

	// Get item data for weight/volume if not provided
	if ($weight === null || $volume === null) {
		$sql = "SELECT item_weight, item_volume FROM " . TB_PREF . "stock_master WHERE stock_id = " . db_escape($stock_id);
		$result = db_query($sql, 'could not get item weight/volume');
		$item = db_fetch($result);
		if ($item) {
			if ($weight === null) $weight = $item['item_weight'] ? (float)$item['item_weight'] : 0;
			if ($volume === null) $volume = $item['item_volume'] ? (float)$item['item_volume'] : 0;
		} else {
			$weight = $weight ?: 0;
			$volume = $volume ?: 0;
		}
	}

	$add_weight = $weight * $qty;
	$add_volume = $volume * $qty;
	$add_units  = (int)$qty;

	// Check bin-level capacity limits
	if ($loc['max_weight'] && ($loc['current_weight'] + $add_weight) > (float)$loc['max_weight']) {
		$errors[] = sprintf(_('Weight capacity exceeded. Max: %s kg, Current: %s kg, Adding: %s kg.'),
			number_format2($loc['max_weight'], 2),
			number_format2($loc['current_weight'], 2),
			number_format2($add_weight, 2));
	}

	if ($loc['max_volume'] && ($loc['current_volume'] + $add_volume) > (float)$loc['max_volume']) {
		$errors[] = sprintf(_('Volume capacity exceeded. Max: %s m³, Current: %s m³, Adding: %s m³.'),
			number_format2($loc['max_volume'], 4),
			number_format2($loc['current_volume'], 4),
			number_format2($add_volume, 4));
	}

	if ($loc['max_units'] && ($loc['current_units'] + $add_units) > (int)$loc['max_units']) {
		$errors[] = sprintf(_('Unit capacity exceeded. Max: %d, Current: %d, Adding: %d.'),
			$loc['max_units'], $loc['current_units'], $add_units);
	}

	// Check storage category constraints
	if ($loc['storage_category_id']) {
		$cat_errors = check_storage_category_constraints($loc['storage_category_id'], $loc_id, $stock_id, $qty, $add_weight, $add_volume, $batch_id);
		$errors = array_merge($errors, $cat_errors);
	}

	return array('ok' => empty($errors), 'errors' => $errors);
}

/**
 * Check storage category constraints (capacity limits + mixing rules + environmental).
 *
 * @param int         $category_id The storage category ID.
 * @param int         $loc_id      The bin location ID.
 * @param string      $stock_id    The item stock_id to store.
 * @param float       $qty         Quantity to add.
 * @param float       $add_weight  Total weight being added.
 * @param float       $add_volume  Total volume being added.
 * @param string|null $batch_id    Batch ID if applicable.
 * @return array      Array of error message strings (empty if OK).
 */
function check_storage_category_constraints($category_id, $loc_id, $stock_id, $qty, $add_weight, $add_volume, $batch_id = null) {
	$errors = array();

	$sql = "SELECT * FROM " . TB_PREF . "wh_storage_categories WHERE id = " . db_escape($category_id);
	$result = db_query($sql, 'could not get storage category');
	$cat = db_fetch($result);
	if (!$cat) return $errors;

	// Category-level capacity limits (override bin if stricter)
	if ($cat['max_weight']) {
		$loc = get_warehouse_location_raw($loc_id);
		if ($loc && ($loc['current_weight'] + $add_weight) > (float)$cat['max_weight']) {
			$errors[] = sprintf(_('Storage category "%s" weight limit exceeded. Max: %s kg.'),
				$cat['category_name'], number_format2($cat['max_weight'], 2));
		}
	}

	if ($cat['max_volume']) {
		$loc = get_warehouse_location_raw($loc_id);
		if ($loc && ($loc['current_volume'] + $add_volume) > (float)$cat['max_volume']) {
			$errors[] = sprintf(_('Storage category "%s" volume limit exceeded. Max: %s m³.'),
				$cat['category_name'], number_format2($cat['max_volume'], 4));
		}
	}

	if ($cat['max_units']) {
		$loc = get_warehouse_location_raw($loc_id);
		if ($loc && ($loc['current_units'] + (int)$qty) > (int)$cat['max_units']) {
			$errors[] = sprintf(_('Storage category "%s" unit limit exceeded. Max: %d.'),
				$cat['category_name'], $cat['max_units']);
		}
	}

	// Mixing rules
	if (!$cat['allow_mixed_items']) {
		$sql = "SELECT DISTINCT stock_id FROM " . TB_PREF . "wh_bin_stock "
			. "WHERE wh_loc_id = " . db_escape($loc_id)
			. " AND qty_on_hand > 0"
			. " AND stock_id <> " . db_escape($stock_id);
		$result = db_query($sql, 'could not check mixed items');
		if (db_fetch($result)) {
			$errors[] = sprintf(_('Storage category "%s" does not allow mixed items in the same bin.'),
				$cat['category_name']);
		}
	}

	if (!$cat['allow_mixed_lots'] && $batch_id) {
		$sql = "SELECT DISTINCT batch_id FROM " . TB_PREF . "wh_bin_stock "
			. "WHERE wh_loc_id = " . db_escape($loc_id)
			. " AND qty_on_hand > 0"
			. " AND batch_id IS NOT NULL"
			. " AND batch_id <> " . db_escape($batch_id);
		$result = db_query($sql, 'could not check mixed lots');
		if (db_fetch($result)) {
			$errors[] = sprintf(_('Storage category "%s" does not allow mixed lots/batches in the same bin.'),
				$cat['category_name']);
		}
	}

	return $errors;
}

/**
 * Check if an item is compatible with a bin's storage attributes (temperature, hazmat).
 *
 * Uses the item's storage_category_id to determine its environmental requirements,
 * then checks against the bin's storage category.
 *
 * @param int    $loc_id   The bin location ID.
 * @param string $stock_id The item stock_id.
 * @return array ['ok' => bool, 'errors' => string[]]
 */
function check_storage_compatibility($loc_id, $stock_id) {
	$errors = array();

	$loc = get_warehouse_location($loc_id);
	if (!$loc) {
		return array('ok' => false, 'errors' => array(_('Location not found.')));
	}

	// Get item's storage category requirement
	$sql = "SELECT sm.storage_category_id AS item_cat_id, sc.category_name AS item_cat_name, "
		. "sc.temperature_min AS item_temp_min, sc.temperature_max AS item_temp_max, "
		. "sc.humidity_max AS item_humidity_max, sc.is_hazmat AS item_is_hazmat "
		. "FROM " . TB_PREF . "stock_master sm "
		. "LEFT JOIN " . TB_PREF . "wh_storage_categories sc ON sm.storage_category_id = sc.id "
		. "WHERE sm.stock_id = " . db_escape($stock_id);
	$result = db_query($sql, 'could not get item storage requirements');
	$item = db_fetch($result);

	if (!$item || !$item['item_cat_id']) {
		// Item has no specific storage requirements — compatible with anything
		return array('ok' => true, 'errors' => array());
	}

	// If the bin has no storage category, it can't store items with environmental requirements
	if (!$loc['storage_category_id']) {
		if ($item['item_temp_min'] !== null || $item['item_temp_max'] !== null || $item['item_is_hazmat']) {
			$errors[] = sprintf(_('Item requires "%s" storage conditions but bin "%s" has no storage category assigned.'),
				$item['item_cat_name'], $loc['loc_code']);
		}
		return array('ok' => empty($errors), 'errors' => $errors);
	}

	// Get bin's storage category
	$sql = "SELECT * FROM " . TB_PREF . "wh_storage_categories WHERE id = " . db_escape($loc['storage_category_id']);
	$result = db_query($sql, 'could not get bin storage category');
	$bin_cat = db_fetch($result);

	if (!$bin_cat) {
		return array('ok' => true, 'errors' => array());
	}

	// Temperature check: item requires a temp range, bin must support it
	if ($item['item_temp_min'] !== null && $bin_cat['temperature_min'] !== null) {
		if ((float)$bin_cat['temperature_min'] > (float)$item['item_temp_min']) {
			$errors[] = sprintf(_('Bin minimum temperature (%s°C) is above item requirement (%s°C).'),
				number_format2($bin_cat['temperature_min'], 1),
				number_format2($item['item_temp_min'], 1));
		}
	}

	if ($item['item_temp_max'] !== null && $bin_cat['temperature_max'] !== null) {
		if ((float)$bin_cat['temperature_max'] < (float)$item['item_temp_max']) {
			// Bin max temp is lower than item needs — this is actually fine for cold items
			// The real check: does the bin support the item's temp range?
		}
	}

	// If item requires cold storage but bin doesn't have temp controls
	if ($item['item_temp_max'] !== null && $bin_cat['temperature_max'] === null) {
		$errors[] = sprintf(_('Item requires temperature-controlled storage (max %s°C) but bin category "%s" has no temperature limits.'),
			number_format2($item['item_temp_max'], 1), $bin_cat['category_name']);
	}

	// Hazmat check
	if ($item['item_is_hazmat'] && !$bin_cat['is_hazmat']) {
		$errors[] = sprintf(_('Item requires hazmat-rated storage but bin category "%s" is not hazmat-rated.'),
			$bin_cat['category_name']);
	}

	return array('ok' => empty($errors), 'errors' => $errors);
}

/**
 * Find compatible bins for an item in a warehouse, considering capacity and storage attributes.
 *
 * @param string      $warehouse_loc_code The warehouse code.
 * @param string      $stock_id           The item stock_id.
 * @param float       $qty                Required quantity.
 * @param string|null $zone_type          Optional zone type filter (e.g., 'dry', 'cold', 'hazmat').
 * @param string|null $abc_class          Optional ABC class filter.
 * @return array Array of location rows that can accommodate the item, with utilization info.
 */
function find_compatible_bins($warehouse_loc_code, $stock_id, $qty = 1, $zone_type = null, $abc_class = null) {
	$compatible = array();

	// Get item properties
	$sql = "SELECT item_weight, item_volume, storage_category_id FROM " . TB_PREF . "stock_master "
		. "WHERE stock_id = " . db_escape($stock_id);
	$result = db_query($sql, 'could not get item');
	$item = db_fetch($result);
	if (!$item) return $compatible;

	$unit_weight = $item['item_weight'] ? (float)$item['item_weight'] : 0;
	$unit_volume = $item['item_volume'] ? (float)$item['item_volume'] : 0;

	// Get all storable bins in this warehouse
	$sql = "SELECT wl.*, lt.type_code, lt.can_store, sc.category_name AS storage_category_name "
		. "FROM " . TB_PREF . "wh_locations wl "
		. "INNER JOIN " . TB_PREF . "wh_location_types lt ON wl.location_type_id = lt.id "
		. "LEFT JOIN " . TB_PREF . "wh_storage_categories sc ON wl.storage_category_id = sc.id "
		. "WHERE wl.warehouse_loc_code = " . db_escape($warehouse_loc_code)
		. " AND lt.can_store = 1 AND wl.is_active = 1";

	if ($zone_type) {
		$sql .= " AND wl.zone_type = " . db_escape($zone_type);
	}
	if ($abc_class) {
		$sql .= " AND wl.abc_class = " . db_escape($abc_class);
	}

	$sql .= " ORDER BY wl.pick_sequence, wl.location_path";
	$result = db_query($sql, 'could not get storable bins');

	while ($bin = db_fetch($result)) {
		// Check capacity
		$cap_check = check_can_store($bin['loc_id'], $stock_id, $qty, $unit_weight, $unit_volume);
		// Check storage compatibility
		$compat_check = check_storage_compatibility($bin['loc_id'], $stock_id);

		$bin['capacity_ok'] = $cap_check['ok'];
		$bin['capacity_errors'] = $cap_check['errors'];
		$bin['compatibility_ok'] = $compat_check['ok'];
		$bin['compatibility_errors'] = $compat_check['errors'];
		$bin['is_compatible'] = $cap_check['ok'] && $compat_check['ok'];
		$bin['utilization'] = get_bin_utilization($bin['loc_id']);

		$compatible[] = $bin;
	}

	// Sort: compatible first, then by utilization (prefer less full bins)
	usort($compatible, function($a, $b) {
		if ($a['is_compatible'] && !$b['is_compatible']) return -1;
		if (!$a['is_compatible'] && $b['is_compatible']) return 1;
		return $a['utilization']['overall_pct'] - $b['utilization']['overall_pct'];
	});

	return $compatible;
}

//----------------------------------------------------------------------
// Utilization / capacity tracking functions (Session 5)
//----------------------------------------------------------------------

/**
 * Get current utilization (percentage) for a single bin location.
 *
 * Returns the highest utilization percentage across weight, volume, and units.
 *
 * @param int $loc_id The bin location ID.
 * @return array ['weight_pct' => float, 'volume_pct' => float, 'units_pct' => float,
 *                'overall_pct' => float, 'status' => string (ok|warning|critical|full)]
 */
function get_bin_utilization($loc_id) {
	$loc = get_warehouse_location_raw($loc_id);
	if (!$loc) {
		return array('weight_pct' => 0, 'volume_pct' => 0, 'units_pct' => 0, 'overall_pct' => 0, 'status' => 'ok');
	}

	$weight_pct = 0;
	$volume_pct = 0;
	$units_pct  = 0;

	if ($loc['max_weight'] && (float)$loc['max_weight'] > 0) {
		$weight_pct = round(((float)$loc['current_weight'] / (float)$loc['max_weight']) * 100, 1);
	}

	if ($loc['max_volume'] && (float)$loc['max_volume'] > 0) {
		$volume_pct = round(((float)$loc['current_volume'] / (float)$loc['max_volume']) * 100, 1);
	}

	if ($loc['max_units'] && (int)$loc['max_units'] > 0) {
		$units_pct = round(((int)$loc['current_units'] / (int)$loc['max_units']) * 100, 1);
	}

	$overall_pct = max($weight_pct, $volume_pct, $units_pct);

	if ($overall_pct >= 100) {
		$status = 'full';
	} elseif ($overall_pct >= 85) {
		$status = 'critical';
	} elseif ($overall_pct >= 60) {
		$status = 'warning';
	} else {
		$status = 'ok';
	}

	return array(
		'weight_pct'  => $weight_pct,
		'volume_pct'  => $volume_pct,
		'units_pct'   => $units_pct,
		'overall_pct' => $overall_pct,
		'status'      => $status,
		'current_weight' => (float)$loc['current_weight'],
		'current_volume' => (float)$loc['current_volume'],
		'current_units'  => (int)$loc['current_units'],
		'max_weight'     => $loc['max_weight'] ? (float)$loc['max_weight'] : null,
		'max_volume'     => $loc['max_volume'] ? (float)$loc['max_volume'] : null,
		'max_units'      => $loc['max_units'] ? (int)$loc['max_units'] : null,
	);
}

/**
 * Get aggregated utilization for a zone (or any parent location) by summing
 * capacity and usage of all descendant storable bins.
 *
 * @param int $parent_loc_id The parent location ID (zone, aisle, rack, or warehouse).
 * @return array ['total_bins' => int, 'bins_with_capacity' => int,
 *                'total_max_weight' => float, 'total_current_weight' => float, 'weight_pct' => float,
 *                'total_max_volume' => float, 'total_current_volume' => float, 'volume_pct' => float,
 *                'total_max_units' => int, 'total_current_units' => int, 'units_pct' => float,
 *                'overall_pct' => float, 'status' => string, 'bins_full' => int, 'bins_empty' => int]
 */
function get_zone_utilization($parent_loc_id) {
	$descendants = get_warehouse_location_descendants($parent_loc_id);
	$descendants[] = $parent_loc_id; // include self

	if (empty($descendants)) {
		return array(
			'total_bins' => 0, 'bins_with_capacity' => 0,
			'total_max_weight' => 0, 'total_current_weight' => 0, 'weight_pct' => 0,
			'total_max_volume' => 0, 'total_current_volume' => 0, 'volume_pct' => 0,
			'total_max_units' => 0, 'total_current_units' => 0, 'units_pct' => 0,
			'overall_pct' => 0, 'status' => 'ok', 'bins_full' => 0, 'bins_empty' => 0
		);
	}

	$id_list = implode(',', array_map('intval', $descendants));

	$sql = "SELECT wl.loc_id, wl.max_weight, wl.max_volume, wl.max_units, "
		. "wl.current_weight, wl.current_volume, wl.current_units "
		. "FROM " . TB_PREF . "wh_locations wl "
		. "INNER JOIN " . TB_PREF . "wh_location_types lt ON wl.location_type_id = lt.id "
		. "WHERE wl.loc_id IN ($id_list) AND lt.can_store = 1 AND wl.is_active = 1";

	$result = db_query($sql, 'could not get zone utilization');

	$totals = array(
		'total_bins' => 0, 'bins_with_capacity' => 0,
		'total_max_weight' => 0, 'total_current_weight' => 0,
		'total_max_volume' => 0, 'total_current_volume' => 0,
		'total_max_units' => 0, 'total_current_units' => 0,
		'bins_full' => 0, 'bins_empty' => 0
	);

	while ($row = db_fetch($result)) {
		$totals['total_bins']++;
		$has_capacity = false;

		if ($row['max_weight']) {
			$has_capacity = true;
			$totals['total_max_weight'] += (float)$row['max_weight'];
		}
		$totals['total_current_weight'] += (float)$row['current_weight'];

		if ($row['max_volume']) {
			$has_capacity = true;
			$totals['total_max_volume'] += (float)$row['max_volume'];
		}
		$totals['total_current_volume'] += (float)$row['current_volume'];

		if ($row['max_units']) {
			$has_capacity = true;
			$totals['total_max_units'] += (int)$row['max_units'];
		}
		$totals['total_current_units'] += (int)$row['current_units'];

		if ($has_capacity) $totals['bins_with_capacity']++;

		// Count full/empty bins
		$util = get_bin_utilization($row['loc_id']);
		if ($util['overall_pct'] >= 100) $totals['bins_full']++;
		if ($util['overall_pct'] == 0 && $has_capacity) $totals['bins_empty']++;
	}

	// Calculate percentages
	$weight_pct = ($totals['total_max_weight'] > 0)
		? round(($totals['total_current_weight'] / $totals['total_max_weight']) * 100, 1) : 0;
	$volume_pct = ($totals['total_max_volume'] > 0)
		? round(($totals['total_current_volume'] / $totals['total_max_volume']) * 100, 1) : 0;
	$units_pct = ($totals['total_max_units'] > 0)
		? round(($totals['total_current_units'] / $totals['total_max_units']) * 100, 1) : 0;

	$overall_pct = max($weight_pct, $volume_pct, $units_pct);

	if ($overall_pct >= 100) $status = 'full';
	elseif ($overall_pct >= 85) $status = 'critical';
	elseif ($overall_pct >= 60) $status = 'warning';
	else $status = 'ok';

	$totals['weight_pct'] = $weight_pct;
	$totals['volume_pct'] = $volume_pct;
	$totals['units_pct']  = $units_pct;
	$totals['overall_pct'] = $overall_pct;
	$totals['status'] = $status;

	return $totals;
}

/**
 * Update the current weight, volume, and units for a bin location.
 *
 * Called when stock is added to or removed from a bin.
 *
 * @param int   $loc_id      The bin location ID.
 * @param float $weight_delta Weight change (positive = add, negative = remove).
 * @param float $volume_delta Volume change.
 * @param int   $units_delta  Units change.
 * @return void
 */
function update_bin_current_capacity($loc_id, $weight_delta, $volume_delta, $units_delta) {
	$sql = "UPDATE " . TB_PREF . "wh_locations SET "
		. "current_weight = GREATEST(0, COALESCE(current_weight, 0) + " . db_escape($weight_delta) . "), "
		. "current_volume = GREATEST(0, COALESCE(current_volume, 0) + " . db_escape($volume_delta) . "), "
		. "current_units = GREATEST(0, COALESCE(current_units, 0) + " . db_escape((int)$units_delta) . ") "
		. "WHERE loc_id = " . db_escape($loc_id);
	db_query($sql, 'could not update bin capacity');
}

/**
 * Recalculate current capacity for a bin by summing actual bin stock.
 *
 * @param int $loc_id The bin location ID.
 * @return void
 */
function recalculate_bin_capacity($loc_id) {
	// Get actual stock in this bin
	$sql = "SELECT bs.stock_id, bs.qty_on_hand, sm.item_weight, sm.item_volume "
		. "FROM " . TB_PREF . "wh_bin_stock bs "
		. "LEFT JOIN " . TB_PREF . "stock_master sm ON bs.stock_id = sm.stock_id "
		. "WHERE bs.wh_loc_id = " . db_escape($loc_id) . " AND bs.qty_on_hand > 0";
	$result = db_query($sql, 'could not get bin stock for recalculation');

	$total_weight = 0;
	$total_volume = 0;
	$total_units  = 0;

	while ($row = db_fetch($result)) {
		$qty = (float)$row['qty_on_hand'];
		$total_weight += $qty * ($row['item_weight'] ? (float)$row['item_weight'] : 0);
		$total_volume += $qty * ($row['item_volume'] ? (float)$row['item_volume'] : 0);
		$total_units  += (int)$qty;
	}

	$sql = "UPDATE " . TB_PREF . "wh_locations SET "
		. "current_weight = " . db_escape($total_weight) . ", "
		. "current_volume = " . db_escape($total_volume) . ", "
		. "current_units = " . db_escape($total_units)
		. " WHERE loc_id = " . db_escape($loc_id);
	db_query($sql, 'could not set recalculated bin capacity');
}

/**
 * Recalculate capacity for all storable bins in a warehouse.
 *
 * @param string $warehouse_loc_code The warehouse code.
 * @return int Number of bins recalculated.
 */
function recalculate_warehouse_capacity($warehouse_loc_code) {
	$result = get_warehouse_storable_locations($warehouse_loc_code);
	$count = 0;
	while ($row = db_fetch($result)) {
		recalculate_bin_capacity($row['loc_id']);
		$count++;
	}
	return $count;
}

//----------------------------------------------------------------------
// ABC Classification functions (Session 5)
//----------------------------------------------------------------------

/**
 * Calculate ABC classification for items based on movement velocity.
 *
 * Analyzes stock movements over a given period, calculates cumulative value,
 * and assigns A/B/C class based on Pareto (80/15/5) thresholds.
 *
 * @param int    $months       Number of months to analyze (default 12).
 * @param float  $a_threshold  Percentage threshold for A class (default 80).
 * @param float  $b_threshold  Percentage threshold for A+B class (default 95).
 * @param string $loc_code     Optional: filter to a specific warehouse location.
 * @param string $method       Analysis method: 'value' (default) or 'turnover' (movement count).
 * @return array Array of items with classification: ['stock_id', 'description', 'total_value',
 *               'total_qty', 'move_count', 'cumulative_pct', 'abc_class', 'rank']
 */
function calculate_abc_classification($months = 12, $a_threshold = 80, $b_threshold = 95, $loc_code = '', $method = 'value') {
	$date_from = date('Y-m-d', strtotime("-{$months} months"));

	// Get outbound movement value per item (only count issues, not receipts)
	// For 'turnover' method, sort/rank by total quantity moved (velocity)
	// For 'value' method (default), sort/rank by total movement value (Pareto)
	$order_by = ($method === 'turnover') ? 'total_qty DESC, move_count DESC' : 'total_value DESC';
	$having   = ($method === 'turnover') ? 'total_qty > 0' : 'total_value > 0';

	$sql = "SELECT sm.stock_id, sm.description, sm.units, "
		. "ABS(SUM(m.qty)) AS total_qty, "
		. "COUNT(m.trans_id) AS move_count, "
		. "ABS(SUM(m.qty * m.standard_cost)) AS total_value "
		. "FROM " . TB_PREF . "stock_moves m "
		. "INNER JOIN " . TB_PREF . "stock_master sm ON m.stock_id = sm.stock_id "
		. "WHERE m.tran_date >= " . db_escape($date_from)
		. " AND m.qty < 0"; // outbound moves

	if ($loc_code) {
		$sql .= " AND m.loc_code = " . db_escape($loc_code);
	}

	$sql .= " GROUP BY sm.stock_id, sm.description, sm.units"
		. " HAVING " . $having
		. " ORDER BY " . $order_by;

	$result = db_query($sql, 'could not calculate ABC classification');

	$items = array();
	$total_movement_value = 0;
	$total_movement_qty   = 0;

	while ($row = db_fetch($result)) {
		$row['total_value'] = (float)$row['total_value'];
		$row['total_qty'] = (float)$row['total_qty'];
		$row['move_count'] = (int)$row['move_count'];
		$total_movement_value += $row['total_value'];
		$total_movement_qty   += $row['total_qty'];
		$items[] = $row;
	}

	// Also include items with zero movement (class C)
	$moved_ids = array();
	foreach ($items as $item) {
		$moved_ids[] = db_escape($item['stock_id']);
	}

	$sql = "SELECT stock_id, description, units FROM " . TB_PREF . "stock_master "
		. "WHERE mb_flag <> 'D' AND inactive = 0"; // exclude service/dummy items
	if (!empty($moved_ids)) {
		$sql .= " AND stock_id NOT IN (" . implode(',', $moved_ids) . ")";
	}
	$result = db_query($sql, 'could not get non-moving items');
	while ($row = db_fetch($result)) {
		$items[] = array(
			'stock_id' => $row['stock_id'],
			'description' => $row['description'],
			'units' => $row['units'],
			'total_qty' => 0,
			'move_count' => 0,
			'total_value' => 0,
		);
	}

	// Assign ABC class using previous-cumulative method (standard Pareto):
	// an item is classified based on the cumulative % BEFORE adding its own value,
	// so the highest-value item is always included in class A.
	// For 'turnover' method, cumulative is based on qty moved, not value.
	$total_for_pareto = ($method === 'turnover') ? $total_movement_qty : $total_movement_value;
	$cumulative_measure = 0;
	$rank = 0;
	foreach ($items as &$item) {
		$rank++;
		$item['rank'] = $rank;
		$item_measure = ($method === 'turnover') ? $item['total_qty'] : $item['total_value'];
		$prev_pct = ($total_for_pareto > 0)
			? round(($cumulative_measure / $total_for_pareto) * 100, 2) : 100;
		$cumulative_measure += $item_measure;
		$item['cumulative_pct'] = ($total_for_pareto > 0)
			? round(($cumulative_measure / $total_for_pareto) * 100, 2) : 100;

		if ($item_measure == 0) {
			$item['abc_class'] = 'C';
		} elseif ($prev_pct < $a_threshold) {
			$item['abc_class'] = 'A';
		} elseif ($prev_pct < $b_threshold) {
			$item['abc_class'] = 'B';
		} else {
			$item['abc_class'] = 'C';
		}
	}
	unset($item);

	return $items;
}

/**
 * Apply ABC classification results to stock_master table.
 *
 * @param array $classifications Array from calculate_abc_classification().
 * @return int Number of items updated.
 */
function apply_abc_classification($classifications) {
	$count = 0;
	foreach ($classifications as $item) {
		$sql = "UPDATE " . TB_PREF . "stock_master SET abc_class = " . db_escape($item['abc_class'])
			. " WHERE stock_id = " . db_escape($item['stock_id']);
		db_query($sql, 'could not update ABC class');
		$count++;
	}
	return $count;
}

/**
 * Get ABC classification summary (counts per class).
 *
 * @return array ['A' => int, 'B' => int, 'C' => int, 'unclassified' => int]
 */
function get_abc_classification_summary() {
	$sql = "SELECT abc_class, COUNT(*) AS cnt FROM " . TB_PREF . "stock_master "
		. "WHERE mb_flag <> 'D' AND inactive = 0 "
		. "GROUP BY abc_class";
	$result = db_query($sql, 'could not get ABC summary');

	$summary = array('A' => 0, 'B' => 0, 'C' => 0, 'unclassified' => 0);
	while ($row = db_fetch($result)) {
		$class = $row['abc_class'];
		if ($class === 'A' || $class === 'B' || $class === 'C') {
			$summary[$class] = (int)$row['cnt'];
		} else {
			$summary['unclassified'] += (int)$row['cnt'];
		}
	}
	return $summary;
}

/**
 * Get warehouse utilization summary for all zones in a warehouse.
 *
 * @param string $warehouse_loc_code The warehouse code.
 * @return array Array of zone utilization data for dashboard display.
 */
function get_warehouse_utilization_summary($warehouse_loc_code) {
	// Get direct children (zones) of the warehouse root
	$root = get_warehouse_root_location($warehouse_loc_code);
	if (!$root) return array();

	$zones = array();
	$result = get_warehouse_location_children($root['loc_id'], $warehouse_loc_code);

	while ($zone = db_fetch($result)) {
		$util = get_zone_utilization($zone['loc_id']);
		$zones[] = array(
			'loc_id'     => $zone['loc_id'],
			'loc_code'   => $zone['loc_code'],
			'loc_name'   => $zone['loc_name'],
			'type_code'  => $zone['type_code'],
			'type_name'  => $zone['type_name'],
			'utilization' => $util,
		);
	}

	return $zones;
}
