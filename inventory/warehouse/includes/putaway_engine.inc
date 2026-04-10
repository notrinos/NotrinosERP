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
 * Putaway Strategy Engine for warehouse bin assignment.
 *
 * Determines the optimal bin location(s) for incoming goods using
 * configurable rules and strategies. Supports capacity-aware splitting
 * across multiple bins when a single bin cannot hold the full quantity.
 *
 * Strategies:
 *   - fixed_bin:              Route to a designated fixed bin
 *   - first_available:        Find the first empty bin in target zone/warehouse
 *   - add_to_existing:        Place in a bin that already contains the same item
 *   - nearest_available:      Find the nearest available bin by pick_sequence
 *   - abc_class:              Match item ABC class to bin ABC zone
 *   - storage_category_match: Match item storage category to bin storage category
 *   - least_packages:         Find the bin with the fewest distinct items
 *
 * @package NotrinosERP
 * @subpackage Inventory/Warehouse
 */

include_once(dirname(__FILE__) . '/db/warehouse_putaway_db.inc');

/**
 * Execute the putaway engine for a given item and warehouse.
 *
 * Finds the matching rule and applies the appropriate strategy to determine
 * which bin(s) should receive the incoming quantity. Returns an array of
 * bin assignments, potentially split across multiple bins for capacity.
 *
 * @param string     $stock_id            The item code.
 * @param string     $warehouse_loc_code  The warehouse code.
 * @param float      $quantity            Quantity to put away.
 * @param float|null $item_weight         Weight per unit (kg). Null = not checked.
 * @param float|null $item_volume         Volume per unit (m³). Null = not checked.
 * @return array Array of assignments, each: ['bin_loc_id'=>int, 'bin_code'=>string,
 *               'bin_name'=>string, 'qty'=>float, 'rule_name'=>string, 'strategy'=>string]
 *               Empty array if no suitable bin found.
 */
function execute_putaway($stock_id, $warehouse_loc_code, $quantity, $item_weight = null, $item_volume = null) {
	// Find matching rule
	$rule = find_putaway_rule_for_item($stock_id, $warehouse_loc_code);

	if (!$rule) {
		return array(); // No rule matches
	}

	$strategy = $rule['strategy'];
	$assignments = array();

	switch ($strategy) {
		case 'fixed_bin':
			$assignments = putaway_strategy_fixed_bin($rule, $stock_id, $quantity, $item_weight, $item_volume);
			break;

		case 'first_available':
			$assignments = putaway_strategy_first_available($rule, $stock_id, $warehouse_loc_code, $quantity, $item_weight, $item_volume);
			break;

		case 'add_to_existing':
			$assignments = putaway_strategy_add_to_existing($rule, $stock_id, $warehouse_loc_code, $quantity, $item_weight, $item_volume);
			break;

		case 'nearest_available':
			$assignments = putaway_strategy_nearest_available($rule, $stock_id, $warehouse_loc_code, $quantity, $item_weight, $item_volume);
			break;

		case 'abc_class':
			$assignments = putaway_strategy_abc_class($rule, $stock_id, $warehouse_loc_code, $quantity, $item_weight, $item_volume);
			break;

		case 'storage_category_match':
			$assignments = putaway_strategy_storage_category_match($rule, $stock_id, $warehouse_loc_code, $quantity, $item_weight, $item_volume);
			break;

		case 'least_packages':
			$assignments = putaway_strategy_least_packages($rule, $stock_id, $warehouse_loc_code, $quantity, $item_weight, $item_volume);
			break;

		default:
			// Unknown strategy — fall back to first_available
			$assignments = putaway_strategy_first_available($rule, $stock_id, $warehouse_loc_code, $quantity, $item_weight, $item_volume);
			break;
	}

	// Stamp rule info on each assignment
	foreach ($assignments as &$assignment) {
		$assignment['rule_id'] = $rule['rule_id'];
		$assignment['rule_name'] = $rule['rule_name'];
		$assignment['strategy'] = $strategy;
	}
	unset($assignment);

	return $assignments;
}

/**
 * Test putaway: shows where an item WOULD go without actually moving anything.
 *
 * @param string $stock_id            The item code.
 * @param string $warehouse_loc_code  The warehouse code.
 * @param float  $quantity            Test quantity.
 * @return array Result with 'rule' (matched rule or null), 'assignments' (bin list),
 *               and 'message' (human-readable explanation).
 */
function test_putaway($stock_id, $warehouse_loc_code, $quantity) {
	$result = array(
		'rule' => null,
		'assignments' => array(),
		'message' => '',
	);

	// Find the item
	$item_sql = "SELECT stock_id, description, item_weight, item_volume, abc_class, "
		. "category_id, storage_category_id, putaway_rule_id "
		. "FROM " . TB_PREF . "stock_master WHERE stock_id = " . db_escape($stock_id);
	$item_result = db_query($item_sql, 'could not get item for putaway test');
	$item = db_fetch($item_result);

	if (!$item) {
		$result['message'] = _('Item not found.');
		return $result;
	}

	$rule = find_putaway_rule_for_item($stock_id, $warehouse_loc_code);
	$result['rule'] = $rule;

	if (!$rule) {
		$result['message'] = sprintf(_('No matching putaway rule found for item %s in warehouse %s.'), $stock_id, $warehouse_loc_code);
		return $result;
	}

	$item_weight = $item['item_weight'] ? (float)$item['item_weight'] : null;
	$item_volume = $item['item_volume'] ? (float)$item['item_volume'] : null;

	$assignments = execute_putaway($stock_id, $warehouse_loc_code, $quantity, $item_weight, $item_volume);
	$result['assignments'] = $assignments;

	if (empty($assignments)) {
		$result['message'] = sprintf(
			_('Rule "%s" (strategy: %s) matched, but no suitable bin found with available capacity.'),
			$rule['rule_name'], $rule['strategy']
		);
	} else {
		$total_assigned = 0;
		$bin_list = array();
		foreach ($assignments as $a) {
			$total_assigned += $a['qty'];
			$bin_list[] = $a['bin_code'] . ' (' . $a['bin_name'] . ') × ' . round($a['qty'], 4);
		}
		$msg = sprintf(
			_('Rule "%s" (strategy: %s) → %d bin(s): %s'),
			$rule['rule_name'],
			$rule['strategy'],
			count($assignments),
			implode(', ', $bin_list)
		);
		if ($total_assigned < $quantity) {
			$msg .= "\n" . sprintf(
				_('Warning: Only %.4f of %.4f units could be assigned (insufficient capacity).'),
				$total_assigned, $quantity
			);
		}
		$result['message'] = $msg;
	}

	return $result;
}

/**
 * Get list of available putaway strategies for dropdowns.
 *
 * @return array Associative array of strategy_key => display name.
 */
function get_putaway_strategies() {
	return array(
		'fixed_bin'              => _('Fixed Bin'),
		'first_available'        => _('First Available'),
		'add_to_existing'        => _('Add to Existing Stock'),
		'nearest_available'      => _('Nearest Available'),
		'abc_class'              => _('ABC Class Match'),
		'storage_category_match' => _('Storage Category Match'),
		'least_packages'         => _('Least Packages (fewest items)'),
	);
}

// =====================================================================
// Strategy implementations
// =====================================================================

/**
 * Fixed Bin strategy: Route directly to a designated bin.
 *
 * If quantity exceeds the bin's capacity, only the capacity portion is assigned.
 *
 * @param array       $rule              The putaway rule row.
 * @param string      $stock_id          The item code.
 * @param float       $quantity          Quantity to put away.
 * @param float|null  $item_weight       Weight per unit.
 * @param float|null  $item_volume       Volume per unit.
 * @return array Array of bin assignments.
 */
function putaway_strategy_fixed_bin($rule, $stock_id, $quantity, $item_weight, $item_volume) {
	if (!$rule['target_loc_id']) {
		return array();
	}

	$sql = "SELECT wl.loc_id, wl.loc_code, wl.loc_name, wl.max_weight, wl.max_volume, wl.max_units, "
		. "COALESCE(bs.total_qty, 0) AS current_qty "
		. "FROM " . TB_PREF . "wh_locations wl "
		. "LEFT JOIN ("
		. "  SELECT wh_loc_id, SUM(qty_on_hand) AS total_qty FROM " . TB_PREF . "wh_bin_stock GROUP BY wh_loc_id"
		. ") bs ON wl.loc_id = bs.wh_loc_id "
		. "WHERE wl.loc_id = " . db_escape((int)$rule['target_loc_id'])
		. " AND wl.is_active = 1";

	$result = db_query($sql, 'could not get fixed bin');
	$bin = db_fetch($result);

	if (!$bin) {
		return array();
	}

	$available_qty = calculate_available_capacity($bin, $quantity, $item_weight, $item_volume);
	if ($available_qty <= 0) {
		return array();
	}

	return array(array(
		'bin_loc_id' => (int)$bin['loc_id'],
		'bin_code'   => $bin['loc_code'],
		'bin_name'   => $bin['loc_name'],
		'qty'        => min($quantity, $available_qty),
	));
}

/**
 * First Available strategy: Find the first empty or available bin.
 *
 * Scans bins in the target zone (or entire warehouse) ordered by pick_sequence.
 * Splits across multiple bins if needed.
 *
 * @param array       $rule              The putaway rule row.
 * @param string      $stock_id          The item code.
 * @param string      $warehouse_loc_code Warehouse code.
 * @param float       $quantity          Quantity to put away.
 * @param float|null  $item_weight       Weight per unit.
 * @param float|null  $item_volume       Volume per unit.
 * @return array Array of bin assignments.
 */
function putaway_strategy_first_available($rule, $stock_id, $warehouse_loc_code, $quantity, $item_weight, $item_volume) {
	if ($rule['target_zone_id']) {
		$bins_result = get_bins_for_putaway($rule['target_zone_id'], $warehouse_loc_code, $rule['storage_category_id']);
	} else {
		$bins_result = get_all_bins_for_putaway($warehouse_loc_code, $rule['storage_category_id']);
	}

	return allocate_to_bins($bins_result, $quantity, $item_weight, $item_volume, 'empty_first');
}

/**
 * Add to Existing strategy: Prefer bins that already contain the same item.
 *
 * Falls back to first available if no existing bins have capacity.
 *
 * @param array       $rule              The putaway rule row.
 * @param string      $stock_id          The item code.
 * @param string      $warehouse_loc_code Warehouse code.
 * @param float       $quantity          Quantity to put away.
 * @param float|null  $item_weight       Weight per unit.
 * @param float|null  $item_volume       Volume per unit.
 * @return array Array of bin assignments.
 */
function putaway_strategy_add_to_existing($rule, $stock_id, $warehouse_loc_code, $quantity, $item_weight, $item_volume) {
	// First try bins with existing stock of this item
	$existing_result = get_bins_with_existing_item($stock_id, $warehouse_loc_code);
	$assignments = allocate_to_bins($existing_result, $quantity, $item_weight, $item_volume, 'any');

	$assigned_qty = 0;
	foreach ($assignments as $a) {
		$assigned_qty += $a['qty'];
	}

	// If not fully allocated, fall back to first available for remainder
	$remaining = $quantity - $assigned_qty;
	if ($remaining > 0) {
		if ($rule['target_zone_id']) {
			$bins_result = get_bins_for_putaway($rule['target_zone_id'], $warehouse_loc_code, $rule['storage_category_id']);
		} else {
			$bins_result = get_all_bins_for_putaway($warehouse_loc_code, $rule['storage_category_id']);
		}
		$extra = allocate_to_bins($bins_result, $remaining, $item_weight, $item_volume, 'empty_first');
		$assignments = array_merge($assignments, $extra);
	}

	return $assignments;
}

/**
 * Nearest Available strategy: Find the nearest bin by pick_sequence.
 *
 * Prefers bins with the lowest pick_sequence value, which represents
 * physical proximity to the receiving area.
 *
 * @param array       $rule              The putaway rule row.
 * @param string      $stock_id          The item code.
 * @param string      $warehouse_loc_code Warehouse code.
 * @param float       $quantity          Quantity to put away.
 * @param float|null  $item_weight       Weight per unit.
 * @param float|null  $item_volume       Volume per unit.
 * @return array Array of bin assignments.
 */
function putaway_strategy_nearest_available($rule, $stock_id, $warehouse_loc_code, $quantity, $item_weight, $item_volume) {
	// Same as first_available but bins are ordered by pick_sequence (default ordering)
	if ($rule['target_zone_id']) {
		$bins_result = get_bins_for_putaway($rule['target_zone_id'], $warehouse_loc_code, $rule['storage_category_id']);
	} else {
		$bins_result = get_all_bins_for_putaway($warehouse_loc_code, $rule['storage_category_id']);
	}

	return allocate_to_bins($bins_result, $quantity, $item_weight, $item_volume, 'any');
}

/**
 * ABC Class strategy: Route to bins matching the item's ABC velocity class.
 *
 * A-class items go to A-class bins (fastest access), B to B, C to C.
 * Falls back to first available if no matching ABC bins are found.
 *
 * @param array       $rule              The putaway rule row.
 * @param string      $stock_id          The item code.
 * @param string      $warehouse_loc_code Warehouse code.
 * @param float       $quantity          Quantity to put away.
 * @param float|null  $item_weight       Weight per unit.
 * @param float|null  $item_volume       Volume per unit.
 * @return array Array of bin assignments.
 */
function putaway_strategy_abc_class($rule, $stock_id, $warehouse_loc_code, $quantity, $item_weight, $item_volume) {
	// Get item's ABC class
	$item_sql = "SELECT abc_class FROM " . TB_PREF . "stock_master WHERE stock_id = " . db_escape($stock_id);
	$item_result = db_query($item_sql, 'could not get item abc class');
	$item = db_fetch($item_result);

	$abc_class = $item && $item['abc_class'] ? $item['abc_class'] : 'C';

	$bins_result = get_bins_by_abc_class($abc_class, $warehouse_loc_code);
	$assignments = allocate_to_bins($bins_result, $quantity, $item_weight, $item_volume, 'any');

	// Fallback if no matching ABC bins
	$assigned_qty = 0;
	foreach ($assignments as $a) {
		$assigned_qty += $a['qty'];
	}

	if ($assigned_qty < $quantity) {
		$remaining = $quantity - $assigned_qty;
		$all_bins = get_all_bins_for_putaway($warehouse_loc_code, null);
		$extra = allocate_to_bins($all_bins, $remaining, $item_weight, $item_volume, 'empty_first');
		$assignments = array_merge($assignments, $extra);
	}

	return $assignments;
}

/**
 * Storage Category Match strategy: Route to bins matching item's storage category.
 *
 * Falls back to first available if no matching category bins are found.
 *
 * @param array       $rule              The putaway rule row.
 * @param string      $stock_id          The item code.
 * @param string      $warehouse_loc_code Warehouse code.
 * @param float       $quantity          Quantity to put away.
 * @param float|null  $item_weight       Weight per unit.
 * @param float|null  $item_volume       Volume per unit.
 * @return array Array of bin assignments.
 */
function putaway_strategy_storage_category_match($rule, $stock_id, $warehouse_loc_code, $quantity, $item_weight, $item_volume) {
	// Get item's storage category
	$item_sql = "SELECT storage_category_id FROM " . TB_PREF . "stock_master WHERE stock_id = " . db_escape($stock_id);
	$item_result = db_query($item_sql, 'could not get item storage category');
	$item = db_fetch($item_result);

	$storage_cat = $item && $item['storage_category_id'] ? (int)$item['storage_category_id'] : null;

	if ($storage_cat) {
		$bins_result = get_all_bins_for_putaway($warehouse_loc_code, $storage_cat);
		$assignments = allocate_to_bins($bins_result, $quantity, $item_weight, $item_volume, 'any');

		$assigned_qty = 0;
		foreach ($assignments as $a) {
			$assigned_qty += $a['qty'];
		}

		if ($assigned_qty >= $quantity) {
			return $assignments;
		}

		// Fallback: any available bin
		$remaining = $quantity - $assigned_qty;
		$all_bins = get_all_bins_for_putaway($warehouse_loc_code, null);
		$extra = allocate_to_bins($all_bins, $remaining, $item_weight, $item_volume, 'empty_first');
		return array_merge($assignments, $extra);
	}

	// No storage category on item — use any available bin
	$bins_result = get_all_bins_for_putaway($warehouse_loc_code, null);
	return allocate_to_bins($bins_result, $quantity, $item_weight, $item_volume, 'empty_first');
}

/**
 * Least Packages strategy: Route to the bin with the fewest distinct items.
 *
 * Consolidation-focused strategy to minimize bin fragmentation.
 *
 * @param array       $rule              The putaway rule row.
 * @param string      $stock_id          The item code.
 * @param string      $warehouse_loc_code Warehouse code.
 * @param float       $quantity          Quantity to put away.
 * @param float|null  $item_weight       Weight per unit.
 * @param float|null  $item_volume       Volume per unit.
 * @return array Array of bin assignments.
 */
function putaway_strategy_least_packages($rule, $stock_id, $warehouse_loc_code, $quantity, $item_weight, $item_volume) {
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
		. " AND lt.can_store = 1 AND wl.is_active = 1 ";

	if ($rule['storage_category_id']) {
		$sql .= " AND wl.storage_category_id = " . db_escape((int)$rule['storage_category_id']);
	}

	// Order by fewest distinct items first, then by pick_sequence
	$sql .= " ORDER BY COALESCE(bs.item_count, 0), wl.pick_sequence, wl.loc_code";

	$bins_result = db_query($sql, 'could not get bins for least packages');
	return allocate_to_bins($bins_result, $quantity, $item_weight, $item_volume, 'any');
}

// =====================================================================
// Capacity helpers
// =====================================================================

/**
 * Calculate how many units a bin can accept based on its capacity limits.
 *
 * Checks max_units, max_weight, and max_volume constraints. Returns the
 * maximum number of units that can fit, or PHP_INT_MAX if no limit applies.
 *
 * @param array      $bin          Bin row with max_weight, max_volume, max_units, current_qty.
 * @param float      $quantity     Quantity being considered.
 * @param float|null $item_weight  Weight per unit (kg).
 * @param float|null $item_volume  Volume per unit (m³).
 * @return float Maximum units that can be accepted.
 */
function calculate_available_capacity($bin, $quantity, $item_weight = null, $item_volume = null) {
	$max_available = PHP_INT_MAX;

	// Unit limit check
	if ($bin['max_units'] !== null && $bin['max_units'] !== '' && (int)$bin['max_units'] > 0) {
		$remaining_units = (int)$bin['max_units'] - (float)$bin['current_qty'];
		$max_available = min($max_available, max(0, $remaining_units));
	}

	// Weight limit check
	if ($item_weight && $item_weight > 0 && $bin['max_weight'] !== null && $bin['max_weight'] !== '' && (float)$bin['max_weight'] > 0) {
		$current_weight = isset($bin['current_weight']) ? (float)$bin['current_weight'] : 0;
		$remaining_weight = (float)$bin['max_weight'] - $current_weight;
		$units_by_weight = floor($remaining_weight / $item_weight);
		$max_available = min($max_available, max(0, $units_by_weight));
	}

	// Volume limit check
	if ($item_volume && $item_volume > 0 && $bin['max_volume'] !== null && $bin['max_volume'] !== '' && (float)$bin['max_volume'] > 0) {
		$current_volume = isset($bin['current_volume']) ? (float)$bin['current_volume'] : 0;
		$remaining_volume = (float)$bin['max_volume'] - $current_volume;
		$units_by_volume = floor($remaining_volume / $item_volume);
		$max_available = min($max_available, max(0, $units_by_volume));
	}

	return $max_available;
}

/**
 * Allocate quantity across bins from a result set.
 *
 * Iterates through bins, assigning quantity up to capacity of each bin.
 * Supports two modes:
 *   'empty_first' — prefer bins with zero current stock
 *   'any'         — assign in order regardless of current stock
 *
 * @param resource    $bins_result  DB result of candidate bins.
 * @param float       $quantity     Total quantity to allocate.
 * @param float|null  $item_weight  Weight per unit.
 * @param float|null  $item_volume  Volume per unit.
 * @param string      $preference   'empty_first' or 'any'.
 * @return array Array of assignments.
 */
function allocate_to_bins($bins_result, $quantity, $item_weight, $item_volume, $preference = 'any') {
	$bins = array();
	while ($row = db_fetch($bins_result)) {
		$bins[] = $row;
	}

	// Sort empty bins first if preferred
	if ($preference === 'empty_first') {
		usort($bins, function($a, $b) {
			$a_empty = ((float)$a['current_qty'] == 0) ? 0 : 1;
			$b_empty = ((float)$b['current_qty'] == 0) ? 0 : 1;
			if ($a_empty !== $b_empty) {
				return $a_empty - $b_empty;
			}
			// Secondary sort by pick_sequence
			$a_seq = isset($a['pick_sequence']) ? (int)$a['pick_sequence'] : 999;
			$b_seq = isset($b['pick_sequence']) ? (int)$b['pick_sequence'] : 999;
			return $a_seq - $b_seq;
		});
	}

	$assignments = array();
	$remaining = $quantity;

	foreach ($bins as $bin) {
		if ($remaining <= 0) {
			break;
		}

		$available = calculate_available_capacity($bin, $remaining, $item_weight, $item_volume);
		if ($available <= 0) {
			continue;
		}

		$assign_qty = min($remaining, $available);

		// Check if this bin is already in our assignments (shouldn't happen, but safety)
		$already_assigned = false;
		foreach ($assignments as &$existing) {
			if ($existing['bin_loc_id'] == (int)$bin['loc_id']) {
				$existing['qty'] += $assign_qty;
				$already_assigned = true;
				break;
			}
		}
		unset($existing);

		if (!$already_assigned) {
			$assignments[] = array(
				'bin_loc_id' => (int)$bin['loc_id'],
				'bin_code'   => $bin['loc_code'],
				'bin_name'   => $bin['loc_name'],
				'qty'        => $assign_qty,
			);
		}

		$remaining -= $assign_qty;
	}

	return $assignments;
}
