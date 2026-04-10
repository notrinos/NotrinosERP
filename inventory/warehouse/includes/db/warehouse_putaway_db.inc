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
 * Database functions for warehouse putaway rules.
 *
 * Putaway rules define the logic for automatically assigning incoming goods
 * to the optimal bin location based on item, category, storage category,
 * and zone criteria with configurable strategies.
 *
 * Strategies: fixed_bin, first_available, add_to_existing,
 *             nearest_available, abc_class, storage_category_match, least_packages
 *
 * Rule priority resolution: item-specific -> category -> warehouse -> default
 *
 * @package NotrinosERP
 * @subpackage Inventory/Warehouse
 */

/**
 * Add a new putaway rule.
 *
 * @param string      $rule_name           Display name for this rule.
 * @param string|null $warehouse_loc_code  Warehouse code (null = all warehouses).
 * @param int         $sequence            Priority sequence (lower = higher priority).
 * @param string|null $stock_id            Specific item code (null = any item).
 * @param int|null    $category_id         Item category ID (null = any category).
 * @param int|null    $storage_category_id Required storage category (null = any).
 * @param int|null    $target_loc_id       Fixed target bin location ID.
 * @param int|null    $target_zone_id      Target zone location ID (find bin within).
 * @param string      $strategy            Putaway strategy name.
 * @param int         $active              Whether the rule is active (0/1).
 * @return void
 */
function add_putaway_rule($rule_name, $warehouse_loc_code, $sequence, $stock_id,
	$category_id, $storage_category_id, $target_loc_id, $target_zone_id,
	$strategy, $active = 1) {

	$sql = "INSERT INTO " . TB_PREF . "wh_putaway_rules "
		. "(rule_name, warehouse_loc_code, sequence, stock_id, category_id, "
		. "storage_category_id, target_loc_id, target_zone_id, strategy, active) VALUES ("
		. db_escape($rule_name) . ", "
		. ($warehouse_loc_code ? db_escape($warehouse_loc_code) : "NULL") . ", "
		. db_escape((int)$sequence) . ", "
		. ($stock_id ? db_escape($stock_id) : "NULL") . ", "
		. ($category_id ? db_escape((int)$category_id) : "NULL") . ", "
		. ($storage_category_id ? db_escape((int)$storage_category_id) : "NULL") . ", "
		. ($target_loc_id ? db_escape((int)$target_loc_id) : "NULL") . ", "
		. ($target_zone_id ? db_escape((int)$target_zone_id) : "NULL") . ", "
		. db_escape($strategy) . ", "
		. db_escape((int)$active) . ")";

	db_query($sql, 'could not add putaway rule');
}

/**
 * Update an existing putaway rule.
 *
 * @param int         $rule_id             The rule ID to update.
 * @param string      $rule_name           Display name.
 * @param string|null $warehouse_loc_code  Warehouse code.
 * @param int         $sequence            Priority sequence.
 * @param string|null $stock_id            Specific item code.
 * @param int|null    $category_id         Item category ID.
 * @param int|null    $storage_category_id Required storage category.
 * @param int|null    $target_loc_id       Fixed target bin ID.
 * @param int|null    $target_zone_id      Target zone ID.
 * @param string      $strategy            Putaway strategy.
 * @param int         $active              Active flag.
 * @return void
 */
function update_putaway_rule($rule_id, $rule_name, $warehouse_loc_code, $sequence,
	$stock_id, $category_id, $storage_category_id, $target_loc_id, $target_zone_id,
	$strategy, $active = 1) {

	$sql = "UPDATE " . TB_PREF . "wh_putaway_rules SET "
		. "rule_name = " . db_escape($rule_name) . ", "
		. "warehouse_loc_code = " . ($warehouse_loc_code ? db_escape($warehouse_loc_code) : "NULL") . ", "
		. "sequence = " . db_escape((int)$sequence) . ", "
		. "stock_id = " . ($stock_id ? db_escape($stock_id) : "NULL") . ", "
		. "category_id = " . ($category_id ? db_escape((int)$category_id) : "NULL") . ", "
		. "storage_category_id = " . ($storage_category_id ? db_escape((int)$storage_category_id) : "NULL") . ", "
		. "target_loc_id = " . ($target_loc_id ? db_escape((int)$target_loc_id) : "NULL") . ", "
		. "target_zone_id = " . ($target_zone_id ? db_escape((int)$target_zone_id) : "NULL") . ", "
		. "strategy = " . db_escape($strategy) . ", "
		. "active = " . db_escape((int)$active)
		. " WHERE rule_id = " . db_escape((int)$rule_id);

	db_query($sql, 'could not update putaway rule');
}

/**
 * Delete a putaway rule.
 *
 * @param int $rule_id The rule ID to delete.
 * @return void
 */
function delete_putaway_rule($rule_id) {
	$sql = "DELETE FROM " . TB_PREF . "wh_putaway_rules WHERE rule_id = " . db_escape((int)$rule_id);
	db_query($sql, 'could not delete putaway rule');
}

/**
 * Check if a putaway rule can be safely deleted.
 *
 * A rule can be deleted if it is not referenced by any item in stock_master.putaway_rule_id.
 *
 * @param int $rule_id The rule ID.
 * @return string|true True if safe, or error message string.
 */
function can_delete_putaway_rule($rule_id) {
	$sql = "SELECT COUNT(*) AS cnt FROM " . TB_PREF . "stock_master "
		. "WHERE putaway_rule_id = " . db_escape((int)$rule_id);
	$result = db_query($sql, 'could not check putaway rule usage');
	$row = db_fetch($result);
	if ($row['cnt'] > 0) {
		return _('Cannot delete this putaway rule because it is assigned to one or more items.');
	}
	return true;
}

/**
 * Get a single putaway rule by ID.
 *
 * @param int $rule_id The rule ID.
 * @return array|false The row or false.
 */
function get_putaway_rule($rule_id) {
	$sql = "SELECT * FROM " . TB_PREF . "wh_putaway_rules WHERE rule_id = " . db_escape((int)$rule_id);
	$result = db_query($sql, 'could not get putaway rule');
	return db_fetch($result);
}

/**
 * Get all putaway rules, ordered by sequence.
 *
 * @param string|null $warehouse_loc_code Filter by warehouse (null = all).
 * @param bool        $show_inactive      Whether to include inactive rules.
 * @return resource DB result.
 */
function get_putaway_rules($warehouse_loc_code = null, $show_inactive = false) {
	$sql = "SELECT r.*, "
		. "l.location_name AS warehouse_name, "
		. "sm.description AS item_description, "
		. "sc.description AS item_category_name, "
		. "stc.category_name AS storage_category_name, "
		. "tl.loc_name AS target_location_name, tl.loc_code AS target_loc_code, "
		. "tz.loc_name AS target_zone_name, tz.loc_code AS target_zone_code "
		. "FROM " . TB_PREF . "wh_putaway_rules r "
		. "LEFT JOIN " . TB_PREF . "locations l ON r.warehouse_loc_code = l.loc_code "
		. "LEFT JOIN " . TB_PREF . "stock_master sm ON r.stock_id = sm.stock_id "
		. "LEFT JOIN " . TB_PREF . "stock_category sc ON r.category_id = sc.category_id "
		. "LEFT JOIN " . TB_PREF . "wh_storage_categories stc ON r.storage_category_id = stc.id "
		. "LEFT JOIN " . TB_PREF . "wh_locations tl ON r.target_loc_id = tl.loc_id "
		. "LEFT JOIN " . TB_PREF . "wh_locations tz ON r.target_zone_id = tz.loc_id ";

	$where = array();
	if ($warehouse_loc_code) {
		$where[] = "(r.warehouse_loc_code = " . db_escape($warehouse_loc_code) . " OR r.warehouse_loc_code IS NULL)";
	}
	if (!$show_inactive) {
		$where[] = "r.active = 1";
	}

	if (!empty($where)) {
		$sql .= "WHERE " . implode(' AND ', $where) . " ";
	}

	$sql .= "ORDER BY r.sequence, r.rule_id";
	return db_query($sql, 'could not get putaway rules');
}

/**
 * Get putaway rules for dropdown/selector use.
 *
 * @param string|null $warehouse_loc_code Filter by warehouse.
 * @return array Array of rule_id => rule_name.
 */
function get_putaway_rules_list($warehouse_loc_code = null) {
	$result = get_putaway_rules($warehouse_loc_code, false);
	$list = array();
	while ($row = db_fetch($result)) {
		$list[$row['rule_id']] = $row['rule_name'] . ' [' . $row['strategy'] . ']';
	}
	return $list;
}

/**
 * Find matching putaway rules for a given item and warehouse.
 *
 * Resolves rules in priority order: item-specific -> category -> warehouse -> default.
 * Returns the first matching active rule or null if no rule matches.
 *
 * @param string $stock_id           The item code.
 * @param string $warehouse_loc_code The warehouse code.
 * @return array|null The matching rule row, or null if no match.
 */
function find_putaway_rule_for_item($stock_id, $warehouse_loc_code) {
	// Get item details for matching
	$item_sql = "SELECT stock_id, category_id, storage_category_id, abc_class "
		. "FROM " . TB_PREF . "stock_master WHERE stock_id = " . db_escape($stock_id);
	$item_result = db_query($item_sql, 'could not get item for putaway');
	$item = db_fetch($item_result);

	if (!$item) {
		return null;
	}

	// Check if item has a specific putaway_rule_id assigned
	$specific_sql = "SELECT putaway_rule_id FROM " . TB_PREF . "stock_master "
		. "WHERE stock_id = " . db_escape($stock_id) . " AND putaway_rule_id IS NOT NULL";
	$specific_result = db_query($specific_sql, 'could not check item putaway rule');
	$specific = db_fetch($specific_result);
	if ($specific && $specific['putaway_rule_id']) {
		$rule = get_putaway_rule($specific['putaway_rule_id']);
		if ($rule && $rule['active']) {
			return $rule;
		}
	}

	// Find all matching rules sorted by sequence
	$sql = "SELECT * FROM " . TB_PREF . "wh_putaway_rules "
		. "WHERE active = 1 "
		. "AND (warehouse_loc_code = " . db_escape($warehouse_loc_code) . " OR warehouse_loc_code IS NULL) "
		. "ORDER BY sequence, rule_id";
	$result = db_query($sql, 'could not find putaway rules');

	while ($rule = db_fetch($result)) {
		// Check item-specific match
		if ($rule['stock_id'] !== null && $rule['stock_id'] !== '' && $rule['stock_id'] != -1) {
			if ($rule['stock_id'] == $stock_id) {
				return $rule;
			}
			continue; // Item-specific rule didn't match this item
		}

		// Check category match
		if ($rule['category_id'] !== null && $rule['category_id'] !== '' && $rule['category_id'] != -1 && $rule['category_id'] != 0) {
			if ($rule['category_id'] != $item['category_id']) {
				continue; // Category mismatch
			}
		}

		// Check storage category match
		if ($rule['storage_category_id'] !== null && $rule['storage_category_id'] !== '' && $rule['storage_category_id'] != -1 && $rule['storage_category_id'] != 0) {
			if ($rule['storage_category_id'] != $item['storage_category_id']) {
				continue; // Storage category mismatch
			}
		}

		// All criteria matched (or were NULL = any)
		return $rule;
	}

	return null; // No matching rule found
}

/**
 * Get bins within a zone ordered for putaway use.
 *
 * Returns storable bin locations within the specified zone (or warehouse)
 * with current capacity utilization for the putaway engine.
 *
 * @param int         $zone_loc_id         The zone (or warehouse root) loc_id.
 * @param string      $warehouse_loc_code  The warehouse code.
 * @param int|null    $storage_category_id Filter by storage category (null = any).
 * @return resource DB result with bin details and utilization.
 */
function get_bins_for_putaway($zone_loc_id, $warehouse_loc_code, $storage_category_id = null) {
	$sql = "SELECT wl.loc_id, wl.loc_code, wl.loc_name, wl.location_path, "
		. "wl.max_weight, wl.max_volume, wl.max_units, "
		. "wl.storage_category_id, wl.pick_sequence, wl.abc_class, "
		. "COALESCE(bs.total_qty, 0) AS current_qty, "
		. "COALESCE(bs.total_weight, 0) AS current_weight, "
		. "COALESCE(bs.total_volume, 0) AS current_volume, "
		. "COALESCE(bs.item_count, 0) AS distinct_items, "
		. "sc.allow_mixed_items, sc.allow_mixed_lots "
		. "FROM " . TB_PREF . "wh_locations wl "
		. "INNER JOIN " . TB_PREF . "wh_location_types lt ON wl.location_type_id = lt.id "
		. "LEFT JOIN " . TB_PREF . "wh_storage_categories sc ON wl.storage_category_id = sc.id "
		. "LEFT JOIN ("
		. "  SELECT wh_loc_id, "
		. "    SUM(qty_on_hand) AS total_qty, "
		. "    COUNT(DISTINCT stock_id) AS item_count, "
		. "    0 AS total_weight, 0 AS total_volume "
		. "  FROM " . TB_PREF . "wh_bin_stock "
		. "  GROUP BY wh_loc_id"
		. ") bs ON wl.loc_id = bs.wh_loc_id "
		. "WHERE wl.warehouse_loc_code = " . db_escape($warehouse_loc_code)
		. " AND lt.can_store = 1 AND wl.is_active = 1 "
		. " AND wl.location_path LIKE (SELECT CONCAT(location_path, '%') FROM " . TB_PREF . "wh_locations WHERE loc_id = " . db_escape((int)$zone_loc_id) . ")";

	if ($storage_category_id) {
		$sql .= " AND wl.storage_category_id = " . db_escape((int)$storage_category_id);
	}

	$sql .= " ORDER BY wl.pick_sequence, wl.loc_code";

	return db_query($sql, 'could not get bins for putaway');
}

/**
 * Get all storable bins in a warehouse with capacity info.
 *
 * Used by strategies that scan the entire warehouse (e.g., first_available).
 *
 * @param string   $warehouse_loc_code The warehouse code.
 * @param int|null $storage_category_id Filter by storage category.
 * @return resource DB result.
 */
function get_all_bins_for_putaway($warehouse_loc_code, $storage_category_id = null) {
	$sql = "SELECT wl.loc_id, wl.loc_code, wl.loc_name, wl.location_path, "
		. "wl.max_weight, wl.max_volume, wl.max_units, "
		. "wl.storage_category_id, wl.pick_sequence, wl.abc_class, "
		. "COALESCE(bs.total_qty, 0) AS current_qty, "
		. "COALESCE(bs.item_count, 0) AS distinct_items "
		. "FROM " . TB_PREF . "wh_locations wl "
		. "INNER JOIN " . TB_PREF . "wh_location_types lt ON wl.location_type_id = lt.id "
		. "LEFT JOIN ("
		. "  SELECT wh_loc_id, "
		. "    SUM(qty_on_hand) AS total_qty, "
		. "    COUNT(DISTINCT stock_id) AS item_count "
		. "  FROM " . TB_PREF . "wh_bin_stock "
		. "  GROUP BY wh_loc_id"
		. ") bs ON wl.loc_id = bs.wh_loc_id "
		. "WHERE wl.warehouse_loc_code = " . db_escape($warehouse_loc_code)
		. " AND lt.can_store = 1 AND wl.is_active = 1";

	if ($storage_category_id) {
		$sql .= " AND wl.storage_category_id = " . db_escape((int)$storage_category_id);
	}

	$sql .= " ORDER BY wl.pick_sequence, wl.loc_code";

	return db_query($sql, 'could not get all bins for putaway');
}

/**
 * Get bins that already contain a specific item (for add_to_existing strategy).
 *
 * @param string $stock_id            The item code.
 * @param string $warehouse_loc_code  The warehouse code.
 * @return resource DB result.
 */
function get_bins_with_existing_item($stock_id, $warehouse_loc_code) {
	$sql = "SELECT wl.loc_id, wl.loc_code, wl.loc_name, wl.location_path, "
		. "wl.max_weight, wl.max_volume, wl.max_units, "
		. "wl.storage_category_id, wl.pick_sequence, "
		. "bs.qty_on_hand AS current_qty, bs.qty_reserved "
		. "FROM " . TB_PREF . "wh_bin_stock bs "
		. "INNER JOIN " . TB_PREF . "wh_locations wl ON bs.wh_loc_id = wl.loc_id "
		. "INNER JOIN " . TB_PREF . "wh_location_types lt ON wl.location_type_id = lt.id "
		. "WHERE bs.stock_id = " . db_escape($stock_id)
		. " AND wl.warehouse_loc_code = " . db_escape($warehouse_loc_code)
		. " AND lt.can_store = 1 AND wl.is_active = 1 "
		. "ORDER BY wl.pick_sequence, wl.loc_code";

	return db_query($sql, 'could not get bins with existing item');
}

/**
 * Get bins matching a specific ABC class within a warehouse.
 *
 * @param string $abc_class           The ABC class letter (A/B/C).
 * @param string $warehouse_loc_code  The warehouse code.
 * @return resource DB result.
 */
function get_bins_by_abc_class($abc_class, $warehouse_loc_code) {
	$sql = "SELECT wl.loc_id, wl.loc_code, wl.loc_name, wl.location_path, "
		. "wl.max_weight, wl.max_volume, wl.max_units, "
		. "wl.storage_category_id, wl.pick_sequence, "
		. "COALESCE(bs.total_qty, 0) AS current_qty, "
		. "COALESCE(bs.item_count, 0) AS distinct_items "
		. "FROM " . TB_PREF . "wh_locations wl "
		. "INNER JOIN " . TB_PREF . "wh_location_types lt ON wl.location_type_id = lt.id "
		. "LEFT JOIN ("
		. "  SELECT wh_loc_id, SUM(qty_on_hand) AS total_qty, COUNT(DISTINCT stock_id) AS item_count "
		. "  FROM " . TB_PREF . "wh_bin_stock GROUP BY wh_loc_id"
		. ") bs ON wl.loc_id = bs.wh_loc_id "
		. "WHERE wl.warehouse_loc_code = " . db_escape($warehouse_loc_code)
		. " AND lt.can_store = 1 AND wl.is_active = 1 "
		. " AND wl.abc_class = " . db_escape($abc_class)
		. " ORDER BY wl.pick_sequence, wl.loc_code";

	return db_query($sql, 'could not get bins by ABC class');
}
