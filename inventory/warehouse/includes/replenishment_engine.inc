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
 * Replenishment Engine — Rule evaluation, suggestion generation, and auto-execution.
 *
 * Scans active replenishment rules, checks stock level triggers against current
 * on-hand/incoming/outgoing quantities, and generates replenishment suggestions.
 * Suggestions can be auto-executed (creating Material Requests or Transfer Orders)
 * or presented to the user for manual confirmation.
 *
 * Rule types supported:
 *   - min_max: Trigger when on-hand falls below min_qty, replenish up to max_qty
 *   - reorder_point: Trigger when on-hand - outgoing + incoming <= reorder_qty
 *   - mto: Generate demand when open sales orders exist for the item
 *   - inter_warehouse: Transfer from supply_warehouse when stock is low
 *   - pick_face: Refill pick-face bin from bulk storage when below min_qty
 *   - forecast: Based on average daily consumption over configurable period
 *
 * @package NotrinosERP
 * @subpackage Inventory/Warehouse
 */

/**
 * Evaluate all active replenishment rules and generate suggestions.
 *
 * Each suggestion is an associative array with:
 *   - rule_id, rule_name, rule_type
 *   - stock_id, item_description, units
 *   - warehouse_loc_code, warehouse_name
 *   - current_qty, incoming_qty, outgoing_qty, available_qty
 *   - suggested_qty (how much to order/transfer)
 *   - supply_method (purchase|transfer|manufacture)
 *   - supply_warehouse (for transfer type)
 *   - bin_loc_id, bin_code (for pick_face type)
 *   - source_bin_id, source_bin_code (for pick_face type)
 *   - auto_execute (0|1)
 *   - priority (low|medium|high|critical)
 *   - reason (human-readable trigger explanation)
 *
 * @param string|null $warehouse_loc_code Filter by warehouse (null = all)
 * @param string|null $rule_type         Filter by rule type (null = all)
 * @return array List of suggestion arrays
 */
function evaluate_replenishment_rules($warehouse_loc_code = null, $rule_type = null) {
	$suggestions = array();

	// Get all active rules matching filters
	$rules = get_replenishment_rules($rule_type, $warehouse_loc_code, null, false);

	while ($rule = db_fetch($rules)) {
		$rule_suggestions = array();

		switch ($rule['rule_type']) {
			case 'min_max':
				$rule_suggestions = evaluate_min_max_rule($rule);
				break;
			case 'reorder_point':
				$rule_suggestions = evaluate_reorder_point_rule($rule);
				break;
			case 'mto':
				$rule_suggestions = evaluate_mto_rule($rule);
				break;
			case 'inter_warehouse':
				$rule_suggestions = evaluate_inter_warehouse_rule($rule);
				break;
			case 'pick_face':
				$rule_suggestions = evaluate_pick_face_rule($rule);
				break;
			case 'forecast':
				$rule_suggestions = evaluate_forecast_rule($rule);
				break;
		}

		$suggestions = array_merge($suggestions, $rule_suggestions);
	}

	// Sort by priority (critical first)
	usort($suggestions, 'compare_suggestion_priority');

	return $suggestions;
}

/**
 * Evaluate a Min/Max rule.
 *
 * Trigger: on_hand < min_qty
 * Quantity: max_qty - on_hand
 *
 * If the rule targets a specific item, check that item only.
 * If the rule targets a category, check all items in that category at the warehouse.
 *
 * @param array $rule Rule row from DB
 * @return array List of suggestions
 */
function evaluate_min_max_rule($rule) {
	$suggestions = array();
	$items = get_rule_target_items($rule);

	foreach ($items as $item) {
		if (!$rule['warehouse_loc_code']) continue;

		$on_hand = get_replenishment_on_hand_qty($item['stock_id'], $rule['warehouse_loc_code']);
		$incoming = get_replenishment_incoming_qty($item['stock_id'], $rule['warehouse_loc_code']);

		$effective_stock = $on_hand + $incoming;

		if ($effective_stock < (float)$rule['min_qty']) {
			$suggested_qty = (float)$rule['max_qty'] - $on_hand;

			if ($suggested_qty > 0) {
				$priority = calculate_priority($on_hand, (float)$rule['min_qty'], (float)$rule['safety_stock']);

				$suggestions[] = build_suggestion($rule, $item, array(
					'current_qty' => $on_hand,
					'incoming_qty' => $incoming,
					'outgoing_qty' => 0,
					'suggested_qty' => $suggested_qty,
					'priority' => $priority,
					'reason' => sprintf(_('Stock %.1f is below minimum %.1f (max %.1f)'),
						$on_hand, $rule['min_qty'], $rule['max_qty']),
				));
			}
		}
	}

	return $suggestions;
}

/**
 * Evaluate a Reorder Point rule.
 *
 * Trigger: on_hand - outgoing + incoming <= reorder_qty + safety_stock
 * Quantity: reorder_qty (fixed order quantity)
 *
 * @param array $rule Rule row from DB
 * @return array List of suggestions
 */
function evaluate_reorder_point_rule($rule) {
	$suggestions = array();
	$items = get_rule_target_items($rule);

	foreach ($items as $item) {
		if (!$rule['warehouse_loc_code']) continue;

		$on_hand = get_replenishment_on_hand_qty($item['stock_id'], $rule['warehouse_loc_code']);
		$incoming = get_replenishment_incoming_qty($item['stock_id'], $rule['warehouse_loc_code']);
		$outgoing = get_replenishment_outgoing_qty($item['stock_id'], $rule['warehouse_loc_code']);

		$available = $on_hand - $outgoing + $incoming;
		$safety = (float)$rule['safety_stock'];
		$reorder_point = (float)$rule['reorder_qty'] + $safety;

		if ($available <= $reorder_point) {
			$suggested_qty = (float)$rule['reorder_qty'];
			if ($rule['max_qty'] > 0) {
				// Cap at max if set
				$suggested_qty = min($suggested_qty, (float)$rule['max_qty'] - $on_hand);
			}

			if ($suggested_qty > 0) {
				$priority = calculate_priority($available, $reorder_point, $safety);

				$suggestions[] = build_suggestion($rule, $item, array(
					'current_qty' => $on_hand,
					'incoming_qty' => $incoming,
					'outgoing_qty' => $outgoing,
					'suggested_qty' => $suggested_qty,
					'priority' => $priority,
					'reason' => sprintf(_('Available %.1f at/below reorder point %.1f (safety: %.1f)'),
						$available, $reorder_point, $safety),
				));
			}
		}
	}

	return $suggestions;
}

/**
 * Evaluate a Make-to-Order rule.
 *
 * Trigger: Open sales orders exist for the item that cannot be fulfilled from stock.
 * Quantity: unfulfilled demand - on_hand - incoming
 *
 * @param array $rule Rule row from DB
 * @return array List of suggestions
 */
function evaluate_mto_rule($rule) {
	$suggestions = array();
	$items = get_rule_target_items($rule);

	foreach ($items as $item) {
		if (!$rule['warehouse_loc_code']) continue;

		$on_hand = get_replenishment_on_hand_qty($item['stock_id'], $rule['warehouse_loc_code']);
		$incoming = get_replenishment_incoming_qty($item['stock_id'], $rule['warehouse_loc_code']);
		$outgoing = get_replenishment_outgoing_qty($item['stock_id'], $rule['warehouse_loc_code']);

		$shortfall = $outgoing - $on_hand - $incoming;

		if ($shortfall > 0) {
			$priority = $shortfall > $outgoing * 0.5 ? 'high' : 'medium';

			$suggestions[] = build_suggestion($rule, $item, array(
				'current_qty' => $on_hand,
				'incoming_qty' => $incoming,
				'outgoing_qty' => $outgoing,
				'suggested_qty' => $shortfall,
				'priority' => $priority,
				'reason' => sprintf(_('Open demand %.1f exceeds available stock %.1f + incoming %.1f'),
					$outgoing, $on_hand, $incoming),
			));
		}
	}

	return $suggestions;
}

/**
 * Evaluate an Inter-Warehouse rule.
 *
 * Same trigger as min_max but supply comes from another warehouse via transfer.
 *
 * @param array $rule Rule row from DB
 * @return array List of suggestions
 */
function evaluate_inter_warehouse_rule($rule) {
	$suggestions = array();
	$items = get_rule_target_items($rule);

	foreach ($items as $item) {
		if (!$rule['warehouse_loc_code'] || !$rule['supply_warehouse']) continue;

		$on_hand = get_replenishment_on_hand_qty($item['stock_id'], $rule['warehouse_loc_code']);
		$incoming = get_replenishment_incoming_qty($item['stock_id'], $rule['warehouse_loc_code']);

		$effective_stock = $on_hand + $incoming;

		if ($effective_stock < (float)$rule['min_qty']) {
			// Check supply warehouse has enough stock
			$supply_stock = get_replenishment_on_hand_qty($item['stock_id'], $rule['supply_warehouse']);

			$suggested_qty = (float)$rule['max_qty'] - $on_hand;
			// Don't request more than supply warehouse has
			$suggested_qty = min($suggested_qty, $supply_stock);

			if ($suggested_qty > 0) {
				$priority = calculate_priority($on_hand, (float)$rule['min_qty'], (float)$rule['safety_stock']);

				$s = build_suggestion($rule, $item, array(
					'current_qty' => $on_hand,
					'incoming_qty' => $incoming,
					'outgoing_qty' => 0,
					'suggested_qty' => $suggested_qty,
					'priority' => $priority,
					'reason' => sprintf(_('Stock %.1f below min %.1f; transfer from %s (has %.1f)'),
						$on_hand, $rule['min_qty'], $rule['supply_warehouse'], $supply_stock),
				));
				$s['supply_method'] = 'transfer';
				$suggestions[] = $s;
			}
		}
	}

	return $suggestions;
}

/**
 * Evaluate a Pick-Face Refill rule.
 *
 * Trigger: bin stock at pick-face bin < min_qty
 * Quantity: max_qty - current_bin_qty
 * Source: bulk storage bin with highest stock in same warehouse
 *
 * @param array $rule Rule row from DB
 * @return array List of suggestions
 */
function evaluate_pick_face_rule($rule) {
	$suggestions = array();

	if (!$rule['stock_id'] || !$rule['bin_loc_id']) return $suggestions;

	$bin_qty = get_replenishment_bin_qty($rule['stock_id'], $rule['bin_loc_id']);
	$warehouse_loc_code = $rule['warehouse_loc_code'];

	if ($bin_qty < (float)$rule['min_qty']) {
		$suggested_qty = (float)$rule['max_qty'] - $bin_qty;

		// Find bulk source bin
		$source_bin = null;
		if ($warehouse_loc_code) {
			$source_bin = find_bulk_source_bin($rule['stock_id'], $warehouse_loc_code, $rule['bin_loc_id']);
		}

		if ($suggested_qty > 0) {
			if ($source_bin) {
				$suggested_qty = min($suggested_qty, (float)$source_bin['total_qty']);
			}

			if ($suggested_qty > 0) {
				$priority = $bin_qty <= 0 ? 'critical' : ($bin_qty < (float)$rule['min_qty'] * 0.3 ? 'high' : 'medium');

				$s = build_suggestion($rule, array(
					'stock_id' => $rule['stock_id'],
					'description' => $rule['item_description'],
					'units' => '',
				), array(
					'current_qty' => $bin_qty,
					'incoming_qty' => 0,
					'outgoing_qty' => 0,
					'suggested_qty' => $suggested_qty,
					'priority' => $priority,
					'reason' => sprintf(_('Pick-face bin %s has %.1f, below minimum %.1f'),
						$rule['bin_code'], $bin_qty, $rule['min_qty']),
				));

				// Add pick-face specific info
				$s['bin_loc_id'] = $rule['bin_loc_id'];
				$s['bin_code'] = $rule['bin_code'];
				if ($source_bin) {
					$s['source_bin_id'] = $source_bin['wh_loc_id'];
					$s['source_bin_code'] = $source_bin['location_code'];
				}
				$s['supply_method'] = 'transfer';
				$suggestions[] = $s;
			}
		}
	}

	return $suggestions;
}

/**
 * Evaluate a Forecast-Based rule.
 *
 * Estimates average daily consumption over the last 30 days.
 * Trigger: projected stock in lead_time_days <= safety_stock
 * Quantity: average_daily * lead_time_days + safety_stock - current
 *
 * @param array $rule Rule row from DB
 * @return array List of suggestions
 */
function evaluate_forecast_rule($rule) {
	$suggestions = array();
	$items = get_rule_target_items($rule);

	foreach ($items as $item) {
		if (!$rule['warehouse_loc_code']) continue;

		$on_hand = get_replenishment_on_hand_qty($item['stock_id'], $rule['warehouse_loc_code']);
		$incoming = get_replenishment_incoming_qty($item['stock_id'], $rule['warehouse_loc_code']);

		// Calculate average daily consumption (outbound moves over last 30 days)
		$avg_daily = get_average_daily_consumption($item['stock_id'], $rule['warehouse_loc_code'], 30);

		$lead_time = max(1, (int)$rule['lead_time_days']);
		$safety = (float)$rule['safety_stock'];

		// Projected stock after lead time
		$projected = $on_hand + $incoming - ($avg_daily * $lead_time);

		if ($projected <= $safety) {
			$suggested_qty = ($avg_daily * $lead_time) + $safety - $on_hand;

			if ($rule['max_qty'] > 0) {
				$suggested_qty = min($suggested_qty, (float)$rule['max_qty'] - $on_hand);
			}

			if ($suggested_qty > 0) {
				$priority = $projected <= 0 ? 'critical' : ($projected < $safety * 0.5 ? 'high' : 'medium');

				$suggestions[] = build_suggestion($rule, $item, array(
					'current_qty' => $on_hand,
					'incoming_qty' => $incoming,
					'outgoing_qty' => round($avg_daily * $lead_time, 2),
					'suggested_qty' => round($suggested_qty, 2),
					'priority' => $priority,
					'reason' => sprintf(_('Forecast: avg daily use %.2f, projected in %d days: %.1f (safety: %.1f)'),
						$avg_daily, $lead_time, $projected, $safety),
				));
			}
		}
	}

	return $suggestions;
}

/**
 * Get average daily consumption for an item at a location over N days.
 *
 * Calculates from negative stock moves (outbound) in the period.
 *
 * @param string $stock_id  Item code
 * @param string $loc_code  Location code
 * @param int    $days      Number of days to look back
 * @return float Average daily outbound quantity (positive number)
 */
function get_average_daily_consumption($stock_id, $loc_code, $days = 30) {
	$sql = "SELECT COALESCE(SUM(ABS(qty)), 0) AS total_consumed
		FROM " . TB_PREF . "stock_moves
		WHERE stock_id=" . db_escape($stock_id)
		. " AND loc_code=" . db_escape($loc_code)
		. " AND qty < 0"
		. " AND tran_date >= DATE_SUB(CURDATE(), INTERVAL " . (int)$days . " DAY)";

	$result = db_query($sql, 'could not get consumption');
	$row = db_fetch($result);

	$total = (float)$row['total_consumed'];
	return $days > 0 ? round($total / $days, 4) : 0;
}

//----------------------------------------------------------------------
// Helper Functions
//----------------------------------------------------------------------

/**
 * Get target items for a rule (item-specific or category-wide).
 *
 * @param array $rule Rule row
 * @return array List of arrays with stock_id, description, category_id, units
 */
function get_rule_target_items($rule) {
	$items = array();

	if ($rule['stock_id']) {
		// Item-specific rule
		$sql = "SELECT stock_id, description, category_id, units
			FROM " . TB_PREF . "stock_master
			WHERE stock_id=" . db_escape($rule['stock_id'])
			. " AND mb_flag <> 'D' AND inactive=0";
		$result = db_query($sql, 'could not get rule target item');
		$row = db_fetch($result);
		if ($row) $items[] = $row;
	} elseif ($rule['category_id']) {
		// Category-wide rule — get all active items in the category
		$sql = "SELECT stock_id, description, category_id, units
			FROM " . TB_PREF . "stock_master
			WHERE category_id=" . (int)$rule['category_id']
			. " AND mb_flag <> 'D' AND inactive=0
			ORDER BY description";
		$result = db_query($sql, 'could not get category items');
		while ($row = db_fetch($result)) {
			$items[] = $row;
		}
	} else {
		// Global rule — check all stocked items at the warehouse
		if ($rule['warehouse_loc_code']) {
			$result = get_stocked_items_at_location($rule['warehouse_loc_code']);
			while ($row = db_fetch($result)) {
				$items[] = $row;
			}
		}
	}

	return $items;
}

/**
 * Build a standardized suggestion array.
 *
 * @param array $rule     Rule row
 * @param array $item     Item row (stock_id, description, units)
 * @param array $overrides Additional fields
 * @return array Suggestion
 */
function build_suggestion($rule, $item, $overrides) {
	$suggestion = array(
		'rule_id'             => $rule['rule_id'],
		'rule_name'           => $rule['rule_name'],
		'rule_type'           => $rule['rule_type'],
		'stock_id'            => $item['stock_id'],
		'item_description'    => $item['description'],
		'units'               => isset($item['units']) ? $item['units'] : '',
		'warehouse_loc_code'  => $rule['warehouse_loc_code'],
		'warehouse_name'      => isset($rule['warehouse_name']) ? $rule['warehouse_name'] : $rule['warehouse_loc_code'],
		'supply_method'       => $rule['supply_method'],
		'supply_warehouse'    => $rule['supply_warehouse'],
		'preferred_supplier'  => $rule['preferred_supplier'],
		'auto_execute'        => (int)$rule['auto_execute'],
		'current_qty'         => 0,
		'incoming_qty'        => 0,
		'outgoing_qty'        => 0,
		'suggested_qty'       => 0,
		'priority'            => 'medium',
		'reason'              => '',
		'bin_loc_id'          => null,
		'bin_code'            => null,
		'source_bin_id'       => null,
		'source_bin_code'     => null,
	);

	return array_merge($suggestion, $overrides);
}

/**
 * Calculate priority based on current stock vs thresholds.
 *
 * @param float      $current   Current stock level
 * @param float      $threshold Trigger threshold (min_qty or reorder_point)
 * @param float|null $safety    Safety stock level
 * @return string Priority code: critical|high|medium|low
 */
function calculate_priority($current, $threshold, $safety = null) {
	if ($current <= 0) return 'critical';
	if ($safety !== null && $current <= (float)$safety) return 'high';
	if ($current < $threshold * 0.5) return 'high';
	if ($current < $threshold) return 'medium';
	return 'low';
}

/**
 * Compare two suggestions by priority for sorting.
 *
 * @param array $a First suggestion
 * @param array $b Second suggestion
 * @return int Sort order (critical first)
 */
function compare_suggestion_priority($a, $b) {
	$order = array('critical' => 0, 'high' => 1, 'medium' => 2, 'low' => 3);
	$pa = isset($order[$a['priority']]) ? $order[$a['priority']] : 2;
	$pb = isset($order[$b['priority']]) ? $order[$b['priority']] : 2;
	return $pa - $pb;
}

/**
 * Get CSS color for a priority level.
 *
 * @param string $priority Priority code
 * @return string Hex color
 */
function get_priority_color($priority) {
	$colors = array(
		'critical' => '#dc3545',
		'high'     => '#fd7e14',
		'medium'   => '#ffc107',
		'low'      => '#28a745',
	);
	return isset($colors[$priority]) ? $colors[$priority] : '#6c757d';
}

/**
 * Get Font Awesome icon for a priority level.
 *
 * @param string $priority Priority code
 * @return string FA icon class
 */
function get_priority_icon($priority) {
	$icons = array(
		'critical' => 'fa-exclamation-circle',
		'high'     => 'fa-exclamation-triangle',
		'medium'   => 'fa-info-circle',
		'low'      => 'fa-check-circle',
	);
	return isset($icons[$priority]) ? $icons[$priority] : 'fa-question-circle';
}

//----------------------------------------------------------------------
// Execution Functions
//----------------------------------------------------------------------

/**
 * Execute a single replenishment suggestion — create Material Request or Transfer Order.
 *
 * @param array $suggestion Suggestion array from evaluate_replenishment_rules()
 * @return array Result with 'success', 'message', 'doc_type', 'doc_no'
 */
function execute_replenishment_suggestion($suggestion) {
	$result = array(
		'success' => false,
		'message' => '',
		'doc_type' => '',
		'doc_no' => 0,
	);

	if ($suggestion['suggested_qty'] <= 0) {
		$result['message'] = _('No quantity to replenish.');
		return $result;
	}

	begin_transaction();

	$supply_method = $suggestion['supply_method'];

	if ($supply_method == 'transfer' || $suggestion['rule_type'] == 'pick_face') {
		$result = execute_transfer_suggestion($suggestion);
	} else {
		// For purchase and manufacture, create a Material Request
		$result = execute_material_request_suggestion($suggestion);
	}

	if ($result['success']) {
		commit_transaction();
	} else {
		cancel_transaction();
	}

	return $result;
}

/**
 * Execute a transfer-type suggestion — create a Transfer Order.
 *
 * @param array $suggestion Suggestion array
 * @return array Execution result
 */
function execute_transfer_suggestion($suggestion) {
	$result = array('success' => false, 'message' => '', 'doc_type' => 'transfer', 'doc_no' => 0);

	$from_loc = $suggestion['supply_warehouse'];
	$to_loc = $suggestion['warehouse_loc_code'];

	if ($suggestion['rule_type'] == 'pick_face') {
		// Pick-face is internal within same warehouse
		$from_loc = $to_loc;
	}

	if (!$from_loc || !$to_loc) {
		$result['message'] = _('Missing source or destination warehouse.');
		return $result;
	}

	$reference = sprintf('REPL-%06d', $suggestion['rule_id']);
	$memo = sprintf(_('Auto-generated by replenishment rule: %s'), $suggestion['rule_name']);
	$request_date = date('Y-m-d');

	$transfer_id = add_transfer_order(
		$from_loc, $to_loc, $request_date,
		'standard', $reference, $memo
	);

	if (!$transfer_id) {
		$result['message'] = _('Failed to create transfer order.');
		return $result;
	}

	$from_bin = isset($suggestion['source_bin_id']) ? $suggestion['source_bin_id'] : null;
	$to_bin = isset($suggestion['bin_loc_id']) ? $suggestion['bin_loc_id'] : null;

	add_transfer_order_line(
		$transfer_id,
		$suggestion['stock_id'],
		$suggestion['suggested_qty'],
		$from_bin,
		$to_bin
	);

	$result['success'] = true;
	$result['doc_no'] = $transfer_id;
	$result['message'] = sprintf(_('Transfer Order #%d created: %s x %.1f from %s to %s'),
		$transfer_id, $suggestion['stock_id'], $suggestion['suggested_qty'],
		$from_loc, $to_loc);

	return $result;
}

/**
 * Execute a material request suggestion (for purchase or manufacture supply methods).
 *
 * @param array $suggestion Suggestion array
 * @return array Execution result
 */
function execute_material_request_suggestion($suggestion) {
	$result = array('success' => false, 'message' => '', 'doc_type' => 'material_request', 'doc_no' => 0);

	$request_type = $suggestion['supply_method'] == 'manufacture' ? 'manufacturing' : 'purchase';
	$warehouse_loc_code = $suggestion['warehouse_loc_code'];

	if (!$warehouse_loc_code) {
		$result['message'] = _('Missing warehouse location.');
		return $result;
	}

	// Get next request_no
	$sql = "SELECT COALESCE(MAX(request_no), 0) + 1 AS next_no
		FROM " . TB_PREF . "wh_material_requests";
	$r = db_query($sql, 'could not get next MR no');
	$row = db_fetch($r);
	$request_no = (int)$row['next_no'];

	$reference = sprintf('REPL-%06d', $suggestion['rule_id']);
	$memo = sprintf(_('Auto-generated by replenishment rule: %s'), $suggestion['rule_name']);

	// Insert MR header
	$sql = "INSERT INTO " . TB_PREF . "wh_material_requests (
		request_no, request_type, status, warehouse_loc_code,
		requested_by, request_date, reference, memo, created_at
	) VALUES ("
		. (int)$request_no . ","
		. db_escape($request_type) . ","
		. "'draft',"
		. db_escape($warehouse_loc_code) . ","
		. (int)$_SESSION['wa_current_user']->user . ","
		. db_escape(date('Y-m-d')) . ","
		. db_escape($reference) . ","
		. db_escape($memo) . ","
		. "NOW()"
		. ")";

	db_query($sql, 'could not add material request');
	$request_id = db_insert_id();

	if (!$request_id) {
		$result['message'] = _('Failed to create material request.');
		return $result;
	}

	// Insert MR line
	$sql = "INSERT INTO " . TB_PREF . "wh_material_request_lines (
		request_id, stock_id, qty_requested, memo
	) VALUES ("
		. (int)$request_id . ","
		. db_escape($suggestion['stock_id']) . ","
		. (float)$suggestion['suggested_qty'] . ","
		. db_escape($suggestion['reason'])
		. ")";

	db_query($sql, 'could not add material request line');

	$result['success'] = true;
	$result['doc_no'] = $request_id;
	$result['message'] = sprintf(_('Material Request #%d created: %s x %.1f (%s)'),
		$request_no, $suggestion['stock_id'], $suggestion['suggested_qty'],
		get_replenishment_supply_method_label($suggestion['supply_method']));

	return $result;
}

/**
 * Execute all auto-execute suggestions.
 *
 * @param array $suggestions List of suggestions from evaluate_replenishment_rules()
 * @return array Results with 'executed' count, 'failed' count, 'messages' array
 */
function execute_auto_replenishment($suggestions) {
	$results = array('executed' => 0, 'failed' => 0, 'messages' => array());

	foreach ($suggestions as $suggestion) {
		if (!$suggestion['auto_execute']) continue;

		$exec = execute_replenishment_suggestion($suggestion);

		if ($exec['success']) {
			$results['executed']++;
			$results['messages'][] = $exec['message'];
		} else {
			$results['failed']++;
			$results['messages'][] = _('FAILED: ') . $exec['message'];
		}
	}

	return $results;
}
