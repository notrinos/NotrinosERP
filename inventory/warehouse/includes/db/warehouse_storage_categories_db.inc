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
 * Database functions for warehouse storage categories.
 *
 * Storage categories define the physical capacity and environmental constraints
 * (weight, volume, temperature, humidity, hazmat) that apply to warehouse locations.
 *
 * @package NotrinosERP
 * @subpackage Inventory/Warehouse
 */

/**
 * Add a new storage category.
 *
 * @param string     $category_name   Display name for this category.
 * @param float|null $max_weight      Maximum weight capacity (kg).
 * @param float|null $max_volume      Maximum volume capacity (m³).
 * @param int|null   $max_units       Maximum unit count.
 * @param int        $allow_mixed_items Whether mixed items are allowed (0/1).
 * @param int        $allow_mixed_lots  Whether mixed lots are allowed (0/1).
 * @param float|null $temperature_min Minimum temperature (°C).
 * @param float|null $temperature_max Maximum temperature (°C).
 * @param float|null $humidity_max    Maximum humidity (%).
 * @param int        $is_hazmat       Whether this category is for hazmat (0/1).
 * @return void
 */
function add_storage_category($category_name, $max_weight, $max_volume, $max_units,
	$allow_mixed_items = 1, $allow_mixed_lots = 1,
	$temperature_min = null, $temperature_max = null, $humidity_max = null,
	$is_hazmat = 0) {

	$sql = "INSERT INTO " . TB_PREF . "wh_storage_categories "
		. "(category_name, max_weight, max_volume, max_units, allow_mixed_items, allow_mixed_lots, "
		. "temperature_min, temperature_max, humidity_max, is_hazmat) VALUES ("
		. db_escape($category_name) . ", "
		. ($max_weight !== null && $max_weight !== '' ? db_escape($max_weight) : "NULL") . ", "
		. ($max_volume !== null && $max_volume !== '' ? db_escape($max_volume) : "NULL") . ", "
		. ($max_units !== null && $max_units !== '' ? db_escape($max_units) : "NULL") . ", "
		. db_escape($allow_mixed_items) . ", "
		. db_escape($allow_mixed_lots) . ", "
		. ($temperature_min !== null && $temperature_min !== '' ? db_escape($temperature_min) : "NULL") . ", "
		. ($temperature_max !== null && $temperature_max !== '' ? db_escape($temperature_max) : "NULL") . ", "
		. ($humidity_max !== null && $humidity_max !== '' ? db_escape($humidity_max) : "NULL") . ", "
		. db_escape($is_hazmat) . ")";

	db_query($sql, 'could not add storage category');
}

/**
 * Update an existing storage category.
 *
 * @param int        $id              The category ID.
 * @param string     $category_name   Display name.
 * @param float|null $max_weight      Max weight (kg).
 * @param float|null $max_volume      Max volume (m³).
 * @param int|null   $max_units       Max units.
 * @param int        $allow_mixed_items Allow mixed items.
 * @param int        $allow_mixed_lots  Allow mixed lots.
 * @param float|null $temperature_min Min temperature (°C).
 * @param float|null $temperature_max Max temperature (°C).
 * @param float|null $humidity_max    Max humidity (%).
 * @param int        $is_hazmat       Hazmat flag.
 * @return void
 */
function update_storage_category($id, $category_name, $max_weight, $max_volume, $max_units,
	$allow_mixed_items = 1, $allow_mixed_lots = 1,
	$temperature_min = null, $temperature_max = null, $humidity_max = null,
	$is_hazmat = 0) {

	$sql = "UPDATE " . TB_PREF . "wh_storage_categories SET "
		. "category_name = " . db_escape($category_name) . ", "
		. "max_weight = " . ($max_weight !== null && $max_weight !== '' ? db_escape($max_weight) : "NULL") . ", "
		. "max_volume = " . ($max_volume !== null && $max_volume !== '' ? db_escape($max_volume) : "NULL") . ", "
		. "max_units = " . ($max_units !== null && $max_units !== '' ? db_escape($max_units) : "NULL") . ", "
		. "allow_mixed_items = " . db_escape($allow_mixed_items) . ", "
		. "allow_mixed_lots = " . db_escape($allow_mixed_lots) . ", "
		. "temperature_min = " . ($temperature_min !== null && $temperature_min !== '' ? db_escape($temperature_min) : "NULL") . ", "
		. "temperature_max = " . ($temperature_max !== null && $temperature_max !== '' ? db_escape($temperature_max) : "NULL") . ", "
		. "humidity_max = " . ($humidity_max !== null && $humidity_max !== '' ? db_escape($humidity_max) : "NULL") . ", "
		. "is_hazmat = " . db_escape($is_hazmat)
		. " WHERE id = " . db_escape($id);

	db_query($sql, 'could not update storage category');
}

/**
 * Delete a storage category. Only if not in use.
 *
 * @param int $id The category ID.
 * @return void
 */
function delete_storage_category($id) {
	$sql = "DELETE FROM " . TB_PREF . "wh_storage_categories WHERE id = " . db_escape($id);
	db_query($sql, 'could not delete storage category');
}

/**
 * Check if a storage category can be safely deleted.
 *
 * @param int $id The category ID.
 * @return string|true True if safe, or error message.
 */
function can_delete_storage_category($id) {
	// Check wh_locations referencing this category
	$sql = "SELECT COUNT(*) AS cnt FROM " . TB_PREF . "wh_locations WHERE storage_category_id = " . db_escape($id);
	$result = db_query($sql, 'could not check storage category usage');
	$row = db_fetch($result);
	if ($row['cnt'] > 0) {
		return _('Cannot delete this storage category because it is assigned to warehouse locations.');
	}

	// Check stock_master referencing this category
	$sql = "SELECT COUNT(*) AS cnt FROM " . TB_PREF . "stock_master WHERE storage_category_id = " . db_escape($id);
	$result = db_query($sql, 'could not check stock master usage');
	$row = db_fetch($result);
	if ($row['cnt'] > 0) {
		return _('Cannot delete this storage category because it is assigned to items.');
	}

	return true;
}

/**
 * Get a single storage category.
 *
 * @param int $id The category ID.
 * @return array|false The row or false.
 */
function get_storage_category($id) {
	$sql = "SELECT * FROM " . TB_PREF . "wh_storage_categories WHERE id = " . db_escape($id);
	$result = db_query($sql, 'could not get storage category');
	return db_fetch($result);
}

/**
 * Get all storage categories.
 *
 * @param bool $show_inactive Whether to include inactive.
 * @return resource DB result.
 */
function get_storage_categories($show_inactive = false) {
	$sql = "SELECT * FROM " . TB_PREF . "wh_storage_categories";
	if (!$show_inactive) {
		$sql .= " WHERE inactive = 0";
	}
	$sql .= " ORDER BY category_name";
	return db_query($sql, 'could not get storage categories');
}

/**
 * Get storage categories for use in dropdown selectors.
 *
 * @return array Array of id => category_name.
 */
function get_storage_categories_list() {
	$result = get_storage_categories(false);
	$list = array();
	while ($row = db_fetch($result)) {
		$list[$row['id']] = $row['category_name'];
	}
	return $list;
}

//----------------------------------------------------------------------
// Capacity validation functions (Session 5)
//----------------------------------------------------------------------

/**
 * Validate that a storage category's capacity limits are logical.
 *
 * @param float|null $max_weight Max weight.
 * @param float|null $max_volume Max volume.
 * @param int|null   $max_units  Max units.
 * @param float|null $temp_min   Min temperature.
 * @param float|null $temp_max   Max temperature.
 * @param float|null $humidity   Max humidity.
 * @return array Array of validation error messages (empty if valid).
 */
function validate_storage_category_limits($max_weight, $max_volume, $max_units, $temp_min = null, $temp_max = null, $humidity = null) {
	$errors = array();

	if ($max_weight !== null && $max_weight !== '' && (float)$max_weight < 0) {
		$errors[] = _('Maximum weight must be a positive number.');
	}

	if ($max_volume !== null && $max_volume !== '' && (float)$max_volume < 0) {
		$errors[] = _('Maximum volume must be a positive number.');
	}

	if ($max_units !== null && $max_units !== '' && (int)$max_units < 0) {
		$errors[] = _('Maximum units must be a positive number.');
	}

	if ($temp_min !== null && $temp_min !== '' && $temp_max !== null && $temp_max !== '') {
		if ((float)$temp_min > (float)$temp_max) {
			$errors[] = _('Minimum temperature cannot be greater than maximum temperature.');
		}
	}

	if ($humidity !== null && $humidity !== '') {
		if ((float)$humidity < 0 || (float)$humidity > 100) {
			$errors[] = _('Maximum humidity must be between 0 and 100.');
		}
	}

	return $errors;
}

/**
 * Get locations using a specific storage category.
 *
 * @param int $category_id The storage category ID.
 * @return resource DB result set.
 */
function get_locations_by_storage_category($category_id) {
	$sql = "SELECT wl.loc_id, wl.loc_code, wl.loc_name, wl.warehouse_loc_code, "
		. "lt.type_name, wl.max_weight, wl.max_volume, wl.max_units, "
		. "wl.current_weight, wl.current_volume, wl.current_units "
		. "FROM " . TB_PREF . "wh_locations wl "
		. "LEFT JOIN " . TB_PREF . "wh_location_types lt ON wl.location_type_id = lt.id "
		. "WHERE wl.storage_category_id = " . db_escape($category_id)
		. " ORDER BY wl.warehouse_loc_code, wl.location_path";
	return db_query($sql, 'could not get locations by storage category');
}

/**
 * Get items requiring a specific storage category.
 *
 * @param int $category_id The storage category ID.
 * @return resource DB result set.
 */
function get_items_by_storage_category($category_id) {
	$sql = "SELECT stock_id, description, units, item_weight, item_volume "
		. "FROM " . TB_PREF . "stock_master "
		. "WHERE storage_category_id = " . db_escape($category_id)
		. " AND inactive = 0"
		. " ORDER BY stock_id";
	return db_query($sql, 'could not get items by storage category');
}

/**
 * Get a summary of storage category usage across the warehouse.
 *
 * @return array Array of categories with usage counts and utilization.
 */
function get_storage_category_usage_summary() {
	$sql = "SELECT sc.id, sc.category_name, sc.max_weight, sc.max_volume, sc.max_units, "
		. "sc.is_hazmat, sc.temperature_min, sc.temperature_max, "
		. "COUNT(DISTINCT wl.loc_id) AS location_count, "
		. "COUNT(DISTINCT sm.stock_id) AS item_count, "
		. "SUM(wl.current_weight) AS total_current_weight, "
		. "SUM(wl.max_weight) AS total_max_weight, "
		. "SUM(wl.current_volume) AS total_current_volume, "
		. "SUM(wl.max_volume) AS total_max_volume "
		. "FROM " . TB_PREF . "wh_storage_categories sc "
		. "LEFT JOIN " . TB_PREF . "wh_locations wl ON sc.id = wl.storage_category_id AND wl.is_active = 1 "
		. "LEFT JOIN " . TB_PREF . "stock_master sm ON sc.id = sm.storage_category_id AND sm.inactive = 0 "
		. "WHERE sc.inactive = 0 "
		. "GROUP BY sc.id, sc.category_name, sc.max_weight, sc.max_volume, sc.max_units, "
		. "sc.is_hazmat, sc.temperature_min, sc.temperature_max "
		. "ORDER BY sc.category_name";
	return db_query($sql, 'could not get storage category usage summary');
}
