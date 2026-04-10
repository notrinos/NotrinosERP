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
 * Removal Strategy Engine for warehouse bin stock picking.
 *
 * Determines which bin stock to pick first when fulfilling demand.
 * Supports configurable strategies with a priority hierarchy:
 *   1. Item-specific override (stock_master.removal_strategy)
 *   2. Removal strategy rules (0_wh_removal_strategies, matched by sequence)
 *   3. Warehouse default (0_locations.removal_strategy)
 *   4. System fallback: FIFO
 *
 * Strategies:
 *   - fifo:            First In, First Out (oldest receipt date)
 *   - fefo:            First Expired, First Out (earliest expiry from stock_batches)
 *   - lifo:            Last In, First Out (newest receipt date)
 *   - closest:         Closest bin by pick_sequence (lowest pick_sequence first)
 *   - least_packages:  Bin with fewest distinct items (consolidation-friendly)
 *
 * @package NotrinosERP
 * @subpackage Inventory/Warehouse
 */

include_once(dirname(__FILE__) . '/db/warehouse_locations_db.inc');

// =====================================================================
// Strategy resolution (hierarchy)
// =====================================================================

/**
 * Resolve the effective removal strategy for a given item in a warehouse.
 *
 * Hierarchy:
 *   1. stock_master.removal_strategy (item-specific override)
 *   2. 0_wh_removal_strategies rules (item → category → warehouse → global, by sequence)
 *   3. 0_locations.removal_strategy (warehouse default)
 *   4. 'fifo' (system fallback)
 *
 * @param string $stock_id           The item code.
 * @param string $warehouse_loc_code The warehouse location code.
 * @return array ['strategy' => string, 'source' => string, 'rule_id' => int|null]
 */
function resolve_removal_strategy($stock_id, $warehouse_loc_code) {
	// 1. Check item-specific override
	$sql = "SELECT removal_strategy, category_id FROM " . TB_PREF . "stock_master "
		. "WHERE stock_id = " . db_escape($stock_id);
	$result = db_query($sql, 'could not get item removal strategy');
	$item = db_fetch($result);

	if ($item && !empty($item['removal_strategy'])) {
		return array(
			'strategy' => $item['removal_strategy'],
			'source'   => _('Item override'),
			'rule_id'  => null,
		);
	}

	$category_id = $item ? (int)$item['category_id'] : null;

	// 2. Check removal strategy rules (ordered by sequence)
	$sql = "SELECT id, strategy, warehouse_loc_code, stock_id, category_id, sequence "
		. "FROM " . TB_PREF . "wh_removal_strategies "
		. "WHERE active = 1 "
		. "ORDER BY sequence ASC, id ASC";
	$result = db_query($sql, 'could not get removal strategy rules');

	while ($rule = db_fetch($result)) {
		// Match specificity: item > category > warehouse > global
		$matches = true;

		// If rule specifies a stock_id, it must match
		if ($rule['stock_id'] !== null && $rule['stock_id'] !== '') {
			if ($rule['stock_id'] !== $stock_id) {
				$matches = false;
			}
		}

		// If rule specifies a warehouse, it must match
		if ($matches && $rule['warehouse_loc_code'] !== null && $rule['warehouse_loc_code'] !== '') {
			if ($rule['warehouse_loc_code'] !== $warehouse_loc_code) {
				$matches = false;
			}
		}

		// If rule specifies a category, it must match
		if ($matches && $rule['category_id'] !== null && $rule['category_id'] !== '') {
			if ((int)$rule['category_id'] !== $category_id) {
				$matches = false;
			}
		}

		if ($matches) {
			$source_parts = array();
			if ($rule['stock_id']) $source_parts[] = _('Item') . ': ' . $rule['stock_id'];
			if ($rule['category_id']) $source_parts[] = _('Category') . ': #' . $rule['category_id'];
			if ($rule['warehouse_loc_code']) $source_parts[] = _('Warehouse') . ': ' . $rule['warehouse_loc_code'];
			$source = _('Rule') . ' #' . $rule['id'];
			if (!empty($source_parts)) {
				$source .= ' (' . implode(', ', $source_parts) . ')';
			} else {
				$source .= ' (' . _('Global default') . ')';
			}

			return array(
				'strategy' => $rule['strategy'],
				'source'   => $source,
				'rule_id'  => (int)$rule['id'],
			);
		}
	}

	// 3. Check warehouse default
	$sql = "SELECT removal_strategy FROM " . TB_PREF . "locations "
		. "WHERE loc_code = " . db_escape($warehouse_loc_code);
	$result = db_query($sql, 'could not get warehouse removal strategy');
	$warehouse = db_fetch($result);

	if ($warehouse && !empty($warehouse['removal_strategy'])) {
		return array(
			'strategy' => $warehouse['removal_strategy'],
			'source'   => _('Warehouse default'),
			'rule_id'  => null,
		);
	}

	// 4. System fallback
	return array(
		'strategy' => 'fifo',
		'source'   => _('System default (FIFO)'),
		'rule_id'  => null,
	);
}

// =====================================================================
// Main removal execution
// =====================================================================

/**
 * Execute the removal engine: determine which bin stock to pick for a given item and quantity.
 *
 * Returns an array of bin allocations ordered according to the resolved strategy.
 * Supports multi-bin allocation when a single bin doesn't have enough.
 *
 * @param string      $stock_id           The item code.
 * @param string      $warehouse_loc_code The warehouse location code.
 * @param float       $quantity           Desired quantity to pick.
 * @param string|null $force_strategy     Force a specific strategy (bypass hierarchy).
 * @return array Array of allocations: ['bin_loc_id', 'bin_code', 'bin_name', 'stock_id',
 *               'batch_id', 'batch_no', 'expiry_date', 'qty_available', 'qty_to_pick',
 *               'pick_sequence', 'strategy', 'source']
 */
function execute_removal($stock_id, $warehouse_loc_code, $quantity, $force_strategy = null) {
	if ($force_strategy) {
		$resolution = array(
			'strategy' => $force_strategy,
			'source'   => _('Forced') . ': ' . $force_strategy,
			'rule_id'  => null,
		);
	} else {
		$resolution = resolve_removal_strategy($stock_id, $warehouse_loc_code);
	}

	$strategy = $resolution['strategy'];
	$source = $resolution['source'];

	// Get bin stock sorted by strategy
	switch ($strategy) {
		case 'fifo':
			$bins = get_bin_stock_fifo($stock_id, $warehouse_loc_code);
			break;

		case 'fefo':
			$bins = get_bin_stock_fefo($stock_id, $warehouse_loc_code);
			break;

		case 'lifo':
			$bins = get_bin_stock_lifo($stock_id, $warehouse_loc_code);
			break;

		case 'closest':
			$bins = get_bin_stock_closest($stock_id, $warehouse_loc_code);
			break;

		case 'least_packages':
			$bins = get_bin_stock_least_packages($stock_id, $warehouse_loc_code);
			break;

		default:
			// Unknown strategy — fallback to FIFO
			$bins = get_bin_stock_fifo($stock_id, $warehouse_loc_code);
			$strategy = 'fifo';
			$source = _('Fallback (FIFO)');
			break;
	}

	// Allocate across bins
	return allocate_removal($bins, $quantity, $strategy, $source);
}

/**
 * Test removal: preview which bins would be picked without actually moving anything.
 *
 * @param string $stock_id           The item code.
 * @param string $warehouse_loc_code The warehouse location code.
 * @param float  $quantity           Desired quantity.
 * @param string|null $force_strategy Force a specific strategy.
 * @return array ['resolution' => [...], 'allocations' => [...], 'message' => string,
 *               'total_available' => float, 'total_to_pick' => float, 'shortfall' => float]
 */
function test_removal($stock_id, $warehouse_loc_code, $quantity, $force_strategy = null) {
	$result = array(
		'resolution'      => null,
		'allocations'     => array(),
		'message'         => '',
		'total_available' => 0,
		'total_to_pick'   => 0,
		'shortfall'       => 0,
	);

	// Validate item exists
	$item_sql = "SELECT stock_id, description, removal_strategy, category_id, track_by "
		. "FROM " . TB_PREF . "stock_master WHERE stock_id = " . db_escape($stock_id);
	$item_result = db_query($item_sql, 'could not get item for removal test');
	$item = db_fetch($item_result);

	if (!$item) {
		$result['message'] = _('Item not found.');
		return $result;
	}

	// Resolve strategy
	if ($force_strategy) {
		$resolution = array(
			'strategy' => $force_strategy,
			'source'   => _('Forced') . ': ' . $force_strategy,
			'rule_id'  => null,
		);
	} else {
		$resolution = resolve_removal_strategy($stock_id, $warehouse_loc_code);
	}
	$result['resolution'] = $resolution;

	// Get total available
	$total_sql = "SELECT COALESCE(SUM(bs.qty_available), 0) AS total "
		. "FROM " . TB_PREF . "wh_bin_stock bs "
		. "INNER JOIN " . TB_PREF . "wh_locations wl ON bs.wh_loc_id = wl.loc_id "
		. "WHERE bs.stock_id = " . db_escape($stock_id)
		. " AND wl.warehouse_loc_code = " . db_escape($warehouse_loc_code)
		. " AND bs.stock_status = 'available'"
		. " AND bs.qty_available > 0";
	$total_result = db_query($total_sql, 'could not get total available');
	$total_row = db_fetch($total_result);
	$result['total_available'] = (float)$total_row['total'];

	// Execute removal
	$allocations = execute_removal($stock_id, $warehouse_loc_code, $quantity, $force_strategy);
	$result['allocations'] = $allocations;

	$total_pick = 0;
	foreach ($allocations as $a) {
		$total_pick += $a['qty_to_pick'];
	}
	$result['total_to_pick'] = $total_pick;
	$result['shortfall'] = max(0, $quantity - $total_pick);

	// Build message
	if (empty($allocations)) {
		$result['message'] = sprintf(
			_('No available bin stock found for item %s in this warehouse. Strategy: %s (%s).'),
			$stock_id, strtoupper($resolution['strategy']), $resolution['source']
		);
	} else {
		$bin_list = array();
		foreach ($allocations as $a) {
			$entry = $a['bin_code'] . ' (' . $a['bin_name'] . ')';
			if ($a['batch_no']) {
				$entry .= ' [' . _('Batch') . ': ' . $a['batch_no'];
				if ($a['expiry_date']) {
					$entry .= ', ' . _('Exp') . ': ' . $a['expiry_date'];
				}
				$entry .= ']';
			}
			$entry .= ' × ' . round($a['qty_to_pick'], 4);
			$bin_list[] = $entry;
		}

		$msg = sprintf(
			_('Strategy: %s (%s) → %d allocation(s): %s'),
			strtoupper($resolution['strategy']),
			$resolution['source'],
			count($allocations),
			implode('; ', $bin_list)
		);

		if ($result['shortfall'] > 0) {
			$msg .= "\n" . sprintf(
				_('Warning: Only %.4f of %.4f units available (shortfall: %.4f).'),
				$total_pick, $quantity, $result['shortfall']
			);
		}

		$result['message'] = $msg;
	}

	return $result;
}

// =====================================================================
// Strategy-specific bin stock queries
// =====================================================================

/**
 * FIFO: Get bin stock ordered by oldest receipt (updated_at or id as proxy).
 *
 * @param string $stock_id           Item code.
 * @param string $warehouse_loc_code Warehouse code.
 * @return array Array of bin stock rows.
 */
function get_bin_stock_fifo($stock_id, $warehouse_loc_code) {
	$sql = "SELECT bs.id, bs.wh_loc_id, bs.stock_id, bs.qty_on_hand, bs.qty_available, "
		. "bs.batch_id, bs.serial_id, bs.expiry_date, bs.stock_status, bs.updated_at, "
		. "wl.loc_code AS bin_code, wl.loc_name AS bin_name, wl.pick_sequence, "
		. "wl.location_path, "
		. "b.batch_no, b.manufacturing_date, b.expiry_date AS batch_expiry_date "
		. "FROM " . TB_PREF . "wh_bin_stock bs "
		. "INNER JOIN " . TB_PREF . "wh_locations wl ON bs.wh_loc_id = wl.loc_id "
		. "LEFT JOIN " . TB_PREF . "stock_batches b ON bs.batch_id = b.id "
		. "WHERE bs.stock_id = " . db_escape($stock_id)
		. " AND wl.warehouse_loc_code = " . db_escape($warehouse_loc_code)
		. " AND bs.stock_status = 'available'"
		. " AND bs.qty_available > 0"
		. " ORDER BY bs.id ASC, wl.pick_sequence ASC"; // id ASC = oldest first

	$result = db_query($sql, 'could not get FIFO bin stock');
	$bins = array();
	while ($row = db_fetch($result)) {
		$bins[] = $row;
	}
	return $bins;
}

/**
 * FEFO: Get bin stock ordered by earliest expiry date.
 *
 * Integrates with stock_batches: uses batch expiry_date first, then bin-level expiry_date.
 * Items without expiry dates are picked last.
 *
 * @param string $stock_id           Item code.
 * @param string $warehouse_loc_code Warehouse code.
 * @return array Array of bin stock rows.
 */
function get_bin_stock_fefo($stock_id, $warehouse_loc_code) {
	$sql = "SELECT bs.id, bs.wh_loc_id, bs.stock_id, bs.qty_on_hand, bs.qty_available, "
		. "bs.batch_id, bs.serial_id, bs.expiry_date, bs.stock_status, bs.updated_at, "
		. "wl.loc_code AS bin_code, wl.loc_name AS bin_name, wl.pick_sequence, "
		. "wl.location_path, "
		. "b.batch_no, b.manufacturing_date, b.expiry_date AS batch_expiry_date, "
		// Effective expiry: prefer batch expiry, fall back to bin-level expiry
		. "COALESCE(b.expiry_date, bs.expiry_date) AS effective_expiry "
		. "FROM " . TB_PREF . "wh_bin_stock bs "
		. "INNER JOIN " . TB_PREF . "wh_locations wl ON bs.wh_loc_id = wl.loc_id "
		. "LEFT JOIN " . TB_PREF . "stock_batches b ON bs.batch_id = b.id "
		. "WHERE bs.stock_id = " . db_escape($stock_id)
		. " AND wl.warehouse_loc_code = " . db_escape($warehouse_loc_code)
		. " AND bs.stock_status = 'available'"
		. " AND bs.qty_available > 0"
		// Exclude already expired batches in quarantine status
		. " ORDER BY "
		// Sort NULLs (no expiry) last, then by earliest expiry
		. "CASE WHEN COALESCE(b.expiry_date, bs.expiry_date) IS NULL THEN 1 ELSE 0 END ASC, "
		. "COALESCE(b.expiry_date, bs.expiry_date) ASC, "
		. "wl.pick_sequence ASC, bs.id ASC";

	$result = db_query($sql, 'could not get FEFO bin stock');
	$bins = array();
	while ($row = db_fetch($result)) {
		$bins[] = $row;
	}
	return $bins;
}

/**
 * LIFO: Get bin stock ordered by newest receipt (most recent first).
 *
 * @param string $stock_id           Item code.
 * @param string $warehouse_loc_code Warehouse code.
 * @return array Array of bin stock rows.
 */
function get_bin_stock_lifo($stock_id, $warehouse_loc_code) {
	$sql = "SELECT bs.id, bs.wh_loc_id, bs.stock_id, bs.qty_on_hand, bs.qty_available, "
		. "bs.batch_id, bs.serial_id, bs.expiry_date, bs.stock_status, bs.updated_at, "
		. "wl.loc_code AS bin_code, wl.loc_name AS bin_name, wl.pick_sequence, "
		. "wl.location_path, "
		. "b.batch_no, b.manufacturing_date, b.expiry_date AS batch_expiry_date "
		. "FROM " . TB_PREF . "wh_bin_stock bs "
		. "INNER JOIN " . TB_PREF . "wh_locations wl ON bs.wh_loc_id = wl.loc_id "
		. "LEFT JOIN " . TB_PREF . "stock_batches b ON bs.batch_id = b.id "
		. "WHERE bs.stock_id = " . db_escape($stock_id)
		. " AND wl.warehouse_loc_code = " . db_escape($warehouse_loc_code)
		. " AND bs.stock_status = 'available'"
		. " AND bs.qty_available > 0"
		. " ORDER BY bs.id DESC, wl.pick_sequence ASC"; // id DESC = newest first

	$result = db_query($sql, 'could not get LIFO bin stock');
	$bins = array();
	while ($row = db_fetch($result)) {
		$bins[] = $row;
	}
	return $bins;
}

/**
 * Closest: Get bin stock ordered by pick_sequence (lowest = closest to shipping area).
 *
 * @param string $stock_id           Item code.
 * @param string $warehouse_loc_code Warehouse code.
 * @return array Array of bin stock rows.
 */
function get_bin_stock_closest($stock_id, $warehouse_loc_code) {
	$sql = "SELECT bs.id, bs.wh_loc_id, bs.stock_id, bs.qty_on_hand, bs.qty_available, "
		. "bs.batch_id, bs.serial_id, bs.expiry_date, bs.stock_status, bs.updated_at, "
		. "wl.loc_code AS bin_code, wl.loc_name AS bin_name, wl.pick_sequence, "
		. "wl.location_path, "
		. "b.batch_no, b.manufacturing_date, b.expiry_date AS batch_expiry_date "
		. "FROM " . TB_PREF . "wh_bin_stock bs "
		. "INNER JOIN " . TB_PREF . "wh_locations wl ON bs.wh_loc_id = wl.loc_id "
		. "LEFT JOIN " . TB_PREF . "stock_batches b ON bs.batch_id = b.id "
		. "WHERE bs.stock_id = " . db_escape($stock_id)
		. " AND wl.warehouse_loc_code = " . db_escape($warehouse_loc_code)
		. " AND bs.stock_status = 'available'"
		. " AND bs.qty_available > 0"
		. " ORDER BY wl.pick_sequence ASC, wl.loc_code ASC, bs.id ASC";

	$result = db_query($sql, 'could not get Closest bin stock');
	$bins = array();
	while ($row = db_fetch($result)) {
		$bins[] = $row;
	}
	return $bins;
}

/**
 * Least Packages: Get bin stock ordered by bins with fewest distinct items.
 *
 * Picks from the most consolidated bins first (fewest different items),
 * which helps free up bins for other uses.
 *
 * @param string $stock_id           Item code.
 * @param string $warehouse_loc_code Warehouse code.
 * @return array Array of bin stock rows.
 */
function get_bin_stock_least_packages($stock_id, $warehouse_loc_code) {
	$sql = "SELECT bs.id, bs.wh_loc_id, bs.stock_id, bs.qty_on_hand, bs.qty_available, "
		. "bs.batch_id, bs.serial_id, bs.expiry_date, bs.stock_status, bs.updated_at, "
		. "wl.loc_code AS bin_code, wl.loc_name AS bin_name, wl.pick_sequence, "
		. "wl.location_path, "
		. "b.batch_no, b.manufacturing_date, b.expiry_date AS batch_expiry_date, "
		. "COALESCE(pkg.item_count, 0) AS bin_item_count "
		. "FROM " . TB_PREF . "wh_bin_stock bs "
		. "INNER JOIN " . TB_PREF . "wh_locations wl ON bs.wh_loc_id = wl.loc_id "
		. "LEFT JOIN " . TB_PREF . "stock_batches b ON bs.batch_id = b.id "
		. "LEFT JOIN ("
		. "  SELECT wh_loc_id, COUNT(DISTINCT stock_id) AS item_count "
		. "  FROM " . TB_PREF . "wh_bin_stock "
		. "  WHERE qty_on_hand > 0 "
		. "  GROUP BY wh_loc_id"
		. ") pkg ON bs.wh_loc_id = pkg.wh_loc_id "
		. "WHERE bs.stock_id = " . db_escape($stock_id)
		. " AND wl.warehouse_loc_code = " . db_escape($warehouse_loc_code)
		. " AND bs.stock_status = 'available'"
		. " AND bs.qty_available > 0"
		. " ORDER BY COALESCE(pkg.item_count, 0) ASC, wl.pick_sequence ASC, bs.id ASC";

	$result = db_query($sql, 'could not get Least Packages bin stock');
	$bins = array();
	while ($row = db_fetch($result)) {
		$bins[] = $row;
	}
	return $bins;
}

// =====================================================================
// Allocation helper
// =====================================================================

/**
 * Allocate desired quantity across bin stock rows.
 *
 * Iterates through pre-sorted bin stock rows and allocates up to
 * qty_available from each bin until the full quantity is satisfied.
 *
 * @param array  $bins     Array of bin stock rows (pre-sorted by strategy).
 * @param float  $quantity Desired quantity to pick.
 * @param string $strategy The strategy name.
 * @param string $source   The source description.
 * @return array Array of allocations.
 */
function allocate_removal($bins, $quantity, $strategy, $source) {
	$allocations = array();
	$remaining = $quantity;

	foreach ($bins as $bin) {
		if ($remaining <= 0) {
			break;
		}

		$available = (float)$bin['qty_available'];
		if ($available <= 0) {
			continue;
		}

		$pick_qty = min($remaining, $available);

		$allocations[] = array(
			'bin_loc_id'    => (int)$bin['wh_loc_id'],
			'bin_code'      => $bin['bin_code'],
			'bin_name'      => $bin['bin_name'],
			'stock_id'      => $bin['stock_id'],
			'batch_id'      => $bin['batch_id'] ? (int)$bin['batch_id'] : null,
			'batch_no'      => isset($bin['batch_no']) ? $bin['batch_no'] : null,
			'serial_id'     => $bin['serial_id'] ? (int)$bin['serial_id'] : null,
			'expiry_date'   => isset($bin['batch_expiry_date']) ? $bin['batch_expiry_date']
							   : (isset($bin['expiry_date']) ? $bin['expiry_date'] : null),
			'qty_available' => $available,
			'qty_to_pick'   => $pick_qty,
			'pick_sequence' => isset($bin['pick_sequence']) ? (int)$bin['pick_sequence'] : null,
			'location_path' => isset($bin['location_path']) ? $bin['location_path'] : '',
			'strategy'      => $strategy,
			'source'        => $source,
		);

		$remaining -= $pick_qty;
	}

	return $allocations;
}

// =====================================================================
// Removal strategy CRUD for 0_wh_removal_strategies table
// =====================================================================

/**
 * Get list of available removal strategies for dropdowns.
 *
 * @return array Associative array of strategy_key => display name.
 */
function get_removal_strategies() {
	return array(
		'fifo'           => _('FIFO (First In, First Out)'),
		'fefo'           => _('FEFO (First Expired, First Out)'),
		'lifo'           => _('LIFO (Last In, First Out)'),
		'closest'        => _('Closest (by pick sequence)'),
		'least_packages' => _('Least Packages (fewest items in bin)'),
	);
}

/**
 * Add a new removal strategy rule.
 *
 * @param string|null $warehouse_loc_code Warehouse code (null = all warehouses).
 * @param string|null $stock_id           Item code (null = all items).
 * @param int|null    $category_id        Category ID (null = all categories).
 * @param string      $strategy           Strategy name.
 * @param int         $sequence           Priority sequence.
 * @param int         $active             Active flag.
 * @return int The inserted rule ID.
 */
function add_removal_strategy_rule($warehouse_loc_code, $stock_id, $category_id, $strategy, $sequence = 10, $active = 1) {
	$sql = "INSERT INTO " . TB_PREF . "wh_removal_strategies "
		. "(warehouse_loc_code, stock_id, category_id, strategy, sequence, active) VALUES ("
		. ($warehouse_loc_code ? db_escape($warehouse_loc_code) : "NULL") . ", "
		. ($stock_id ? db_escape($stock_id) : "NULL") . ", "
		. ($category_id ? db_escape((int)$category_id) : "NULL") . ", "
		. db_escape($strategy) . ", "
		. db_escape((int)$sequence) . ", "
		. db_escape((int)$active)
		. ")";
	db_query($sql, 'could not add removal strategy rule');
	return db_insert_id();
}

/**
 * Update an existing removal strategy rule.
 *
 * @param int         $id                 Rule ID.
 * @param string|null $warehouse_loc_code Warehouse code.
 * @param string|null $stock_id           Item code.
 * @param int|null    $category_id        Category ID.
 * @param string      $strategy           Strategy name.
 * @param int         $sequence           Priority sequence.
 * @param int         $active             Active flag.
 */
function update_removal_strategy_rule($id, $warehouse_loc_code, $stock_id, $category_id, $strategy, $sequence, $active) {
	$sql = "UPDATE " . TB_PREF . "wh_removal_strategies SET "
		. "warehouse_loc_code = " . ($warehouse_loc_code ? db_escape($warehouse_loc_code) : "NULL") . ", "
		. "stock_id = " . ($stock_id ? db_escape($stock_id) : "NULL") . ", "
		. "category_id = " . ($category_id ? db_escape((int)$category_id) : "NULL") . ", "
		. "strategy = " . db_escape($strategy) . ", "
		. "sequence = " . db_escape((int)$sequence) . ", "
		. "active = " . db_escape((int)$active)
		. " WHERE id = " . db_escape((int)$id);
	db_query($sql, 'could not update removal strategy rule');
}

/**
 * Delete a removal strategy rule.
 *
 * @param int $id Rule ID.
 */
function delete_removal_strategy_rule($id) {
	$sql = "DELETE FROM " . TB_PREF . "wh_removal_strategies WHERE id = " . db_escape((int)$id);
	db_query($sql, 'could not delete removal strategy rule');
}

/**
 * Get a single removal strategy rule.
 *
 * @param int $id Rule ID.
 * @return array|false The rule row or false.
 */
function get_removal_strategy_rule($id) {
	$sql = "SELECT * FROM " . TB_PREF . "wh_removal_strategies WHERE id = " . db_escape((int)$id);
	$result = db_query($sql, 'could not get removal strategy rule');
	return db_fetch($result);
}

/**
 * Get all removal strategy rules with joined names.
 *
 * @param bool $show_inactive Whether to include inactive rules.
 * @return resource DB result set.
 */
function get_removal_strategy_rules($show_inactive = false) {
	$sql = "SELECT rs.*, "
		. "l.location_name AS warehouse_name, "
		. "sm.description AS item_description, "
		. "sc.description AS category_name "
		. "FROM " . TB_PREF . "wh_removal_strategies rs "
		. "LEFT JOIN " . TB_PREF . "locations l ON rs.warehouse_loc_code = l.loc_code "
		. "LEFT JOIN " . TB_PREF . "stock_master sm ON rs.stock_id = sm.stock_id "
		. "LEFT JOIN " . TB_PREF . "stock_category sc ON rs.category_id = sc.category_id ";

	if (!$show_inactive) {
		$sql .= "WHERE rs.active = 1 ";
	}

	$sql .= "ORDER BY rs.sequence ASC, rs.id ASC";
	return db_query($sql, 'could not get removal strategy rules');
}

/**
 * Check whether a removal strategy rule can be deleted.
 *
 * @param int $id Rule ID.
 * @return bool|string True if can delete, or error message.
 */
function can_delete_removal_strategy_rule($id) {
	// Rules have no dependencies — can always be deleted
	return true;
}
