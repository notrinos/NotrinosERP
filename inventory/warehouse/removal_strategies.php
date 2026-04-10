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
 * Removal Strategies — CRUD page for warehouse removal strategy rules
 * and a "Test Removal" tool to preview bin selection results.
 *
 * Session 8 of the Unified Advanced Inventory Implementation Plan.
 */
$page_security = 'SA_WAREHOUSE_REMOVAL';
$path_to_root = '../..';
include_once($path_to_root . '/includes/session.inc');

$_SESSION['page_title'] = _($help_context = 'Removal Strategies');

page($_SESSION['page_title']);

include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/inventory/warehouse/includes/removal_engine.inc');
include_once($path_to_root . '/inventory/warehouse/includes/warehouse_ui.inc');

simple_page_mode(true);

//-------------------------------------------------------------------------------------
// Handle ADD / UPDATE
//-------------------------------------------------------------------------------------

if ($Mode == 'ADD_ITEM' || $Mode == 'UPDATE_ITEM') {
	$input_error = 0;

	$strategy = get_post('strategy');
	$valid_strategies = array_keys(get_removal_strategies());
	if (!in_array($strategy, $valid_strategies)) {
		$input_error = 1;
		display_error(_('Please select a valid removal strategy.'));
		set_focus('strategy');
	}

	$sequence = get_post('sequence');
	if ($sequence === '' || !is_numeric($sequence) || (int)$sequence < 0) {
		$input_error = 1;
		display_error(_('Sequence must be a positive number.'));
		set_focus('sequence');
	}

	if ($input_error != 1) {
		$warehouse_loc_code = get_post('warehouse_loc_code');
		$stock_id = get_post('stock_id');
		$category_id = get_post('category_id');

		if ($selected_id != -1) {
			update_removal_strategy_rule(
				$selected_id,
				$warehouse_loc_code ? $warehouse_loc_code : null,
				$stock_id ? $stock_id : null,
				$category_id ? (int)$category_id : null,
				$strategy,
				(int)$sequence,
				check_value('active') ? 1 : 0
			);
			display_notification(_('Removal strategy rule has been updated.'));
		} else {
			add_removal_strategy_rule(
				$warehouse_loc_code ? $warehouse_loc_code : null,
				$stock_id ? $stock_id : null,
				$category_id ? (int)$category_id : null,
				$strategy,
				(int)$sequence,
				1
			);
			display_notification(_('New removal strategy rule has been added.'));
		}
		$Mode = 'RESET';
	}
}

//-------------------------------------------------------------------------------------
// Handle DELETE
//-------------------------------------------------------------------------------------

if ($Mode == 'Delete') {
	$can = can_delete_removal_strategy_rule($selected_id);
	if ($can === true) {
		delete_removal_strategy_rule($selected_id);
		display_notification(_('Selected removal strategy rule has been deleted.'));
	} else {
		display_error($can);
	}
	$Mode = 'RESET';
}

if ($Mode == 'RESET') {
	$selected_id = -1;
	$sav = get_post('show_inactive');
	unset($_POST);
	$_POST['show_inactive'] = $sav;
}

//-------------------------------------------------------------------------------------
// Handle Test Removal
//-------------------------------------------------------------------------------------

$test_result = null;
if (isset($_POST['TestRemoval'])) {
	$Ajax->activate('_page_body');
	$test_stock_id = get_post('test_stock_id');
	$test_warehouse = get_post('test_warehouse');
	$test_qty = get_post('test_qty');
	$test_strategy = get_post('test_strategy');

	if (!$test_stock_id) {
		display_error(_('Please select an item to test.'));
	} elseif (!$test_warehouse) {
		display_error(_('Please select a warehouse to test.'));
	} elseif (!$test_qty || !is_numeric($test_qty) || (float)$test_qty <= 0) {
		display_error(_('Please enter a valid quantity to test.'));
	} else {
		$force = ($test_strategy && $test_strategy !== 'auto') ? $test_strategy : null;
		$test_result = test_removal($test_stock_id, $test_warehouse, (float)$test_qty, $force);
	}
}

//-------------------------------------------------------------------------------------
// Strategy hierarchy info box
//-------------------------------------------------------------------------------------

echo "<div style='background:#e8f4fd; border:1px solid #b8daff; border-radius:6px; padding:12px 15px; margin-bottom:15px;'>";
echo "<strong><i class='fa fa-info-circle' style='color:#0c5460;'></i> " . _('Strategy Resolution Hierarchy') . "</strong>";
echo "<div style='margin-top:6px; font-size:13px; color:#0c5460;'>";
echo "<ol style='margin:5px 0 0 20px; padding:0;'>";
echo "<li>" . _('Item-specific override (Items → Edit Item → Removal Strategy)') . "</li>";
echo "<li>" . _('Rules below, matched by priority sequence (item → category → warehouse → global)') . "</li>";
echo "<li>" . _('Warehouse default (Locations setup → Removal Strategy column)') . "</li>";
echo "<li>" . _('System fallback: FIFO') . "</li>";
echo "</ol></div></div>";

//-------------------------------------------------------------------------------------
// List table
//-------------------------------------------------------------------------------------

$result = get_removal_strategy_rules(check_value('show_inactive'));

start_form();
start_table(TABLESTYLE);
$th = array(
	_('#'), _('Sequence'), _('Warehouse'), _('Item'),
	_('Category'), _('Strategy'), _('Active'), '', ''
);
inactive_control_column($th);
table_header($th);

$k = 0;
while ($myrow = db_fetch($result)) {
	alt_table_row_color($k);

	label_cell($myrow['id']);
	label_cell($myrow['sequence'], 'align=right');
	label_cell($myrow['warehouse_name'] ? $myrow['warehouse_name'] : '<em>' . _('All Warehouses') . '</em>');

	// Item
	if ($myrow['stock_id']) {
		label_cell($myrow['stock_id'] . ' - ' . $myrow['item_description']);
	} else {
		label_cell('<em>' . _('All Items') . '</em>');
	}

	// Category
	label_cell($myrow['category_name'] ? $myrow['category_name'] : '<em>' . _('All Categories') . '</em>');

	// Strategy with color badge
	$strategies = get_removal_strategies();
	$strategy_label = isset($strategies[$myrow['strategy']]) ? $strategies[$myrow['strategy']] : $myrow['strategy'];
	$strategy_colors = array(
		'fifo'           => '#007bff',
		'fefo'           => '#28a745',
		'lifo'           => '#6f42c1',
		'closest'        => '#fd7e14',
		'least_packages' => '#17a2b8',
	);
	$color = isset($strategy_colors[$myrow['strategy']]) ? $strategy_colors[$myrow['strategy']] : '#6c757d';
	label_cell('<span style="background:' . $color . '; color:#fff; padding:2px 8px; border-radius:3px; font-size:12px;">'
		. $strategy_label . '</span>');

	// Active
	label_cell($myrow['active'] ? '<span style="color:#28a745;">' . _('Yes') . '</span>'
		: '<span style="color:#dc3545;">' . _('No') . '</span>', 'align=center');

	edit_button_cell("Edit" . $myrow['id'], _('Edit'));
	delete_button_cell("Delete" . $myrow['id'], _('Delete'));
	inactive_control_cell($myrow['id'], $myrow['active'], 'wh_removal_strategies', 'id');
	end_row();
}

inactive_control_row($th);
end_table();

//-------------------------------------------------------------------------------------
// Add/Edit form
//-------------------------------------------------------------------------------------

echo '<br>';
start_table(TABLESTYLE2);

if ($selected_id != -1) {
	if ($Mode == 'Edit') {
		$myrow = get_removal_strategy_rule($selected_id);
		$_POST['warehouse_loc_code'] = $myrow['warehouse_loc_code'];
		$_POST['stock_id'] = $myrow['stock_id'];
		$_POST['category_id'] = $myrow['category_id'];
		$_POST['strategy'] = $myrow['strategy'];
		$_POST['sequence'] = $myrow['sequence'];
		$_POST['active'] = $myrow['active'];
	}
	hidden('selected_id', $selected_id);
}

// --- Strategy ---
echo "<tr><td colspan='2' style='padding-top:10px;'><strong>" . _('Strategy') . "</strong></td></tr>\n";
$strategies = get_removal_strategies();
echo "<tr><td class='label'>" . _('Removal Strategy:') . "</td><td>";
echo array_selector('strategy', get_post('strategy', 'fifo'), $strategies);
echo "</td></tr>\n";

// Strategy descriptions
echo "<tr><td></td><td><div style='background:#f8f9fa; border:1px solid #dee2e6; border-radius:4px; padding:8px; margin-top:5px; font-size:12px;'>";
echo "<strong>" . _('Strategy Guide:') . "</strong><br>";
echo "<b>" . _('FIFO') . "</b> — " . _('First In, First Out: pick from oldest receipt first') . "<br>";
echo "<b>" . _('FEFO') . "</b> — " . _('First Expired, First Out: pick earliest expiry date first (uses batch expiry data)') . "<br>";
echo "<b>" . _('LIFO') . "</b> — " . _('Last In, First Out: pick from newest receipt first') . "<br>";
echo "<b>" . _('Closest') . "</b> — " . _('Pick from the bin nearest to shipping (lowest pick_sequence)') . "<br>";
echo "<b>" . _('Least Packages') . "</b> — " . _('Pick from bins with fewest distinct items (frees up bins)') . "<br>";
echo "</div></td></tr>\n";

text_row_ex(_('Sequence (Priority):'), 'sequence', 10, 10, null, null, null, get_post('sequence', '10'));

// --- Scope ---
echo "<tr><td colspan='2' style='padding-top:10px;'><strong>" . _('Scope (Matching Criteria)') . "</strong>"
	. " <small style='color:#777;'>" . _('Leave blank for "any"') . "</small></td></tr>\n";

warehouse_list_row(_('Warehouse:'), 'warehouse_loc_code', get_post('warehouse_loc_code'), _('-- All Warehouses --'));

echo "<tr><td class='label'>" . _('Specific Item:') . "</td><td>";
echo stock_items_list('stock_id', get_post('stock_id'), _('-- All Items --'), false, array('cells' => false));
echo "</td></tr>\n";

stock_categories_list_row(_('Item Category:'), 'category_id', get_post('category_id'), _('-- All Categories --'));

if ($selected_id != -1) {
	check_row(_('Active'), 'active', get_post('active', 1));
}

end_table(1);
submit_add_or_update_center($selected_id == -1, '', 'both');

//-------------------------------------------------------------------------------------
// Test Removal Tool
//-------------------------------------------------------------------------------------

echo '<br>';
echo "<div style='border:2px solid #28a745; border-radius:6px; padding:15px; margin:10px 0; background:#f0fff4;'>";
echo "<h3 style='margin-top:0; color:#28a745;'><i class='fa fa-flask'></i> " . _('Test Removal') . "</h3>";
echo "<p style='color:#555; font-size:13px;'>"
	. _('Preview which bins/batches would be picked when fulfilling a demand, without actually moving anything.') . "</p>";

start_table(TABLESTYLE2);

echo "<tr><td class='label'>" . _('Item:') . "</td><td>";
echo stock_items_list('test_stock_id', get_post('test_stock_id'), false, false, array('cells' => false));
echo "</td></tr>\n";

warehouse_list_row(_('Warehouse:'), 'test_warehouse', get_post('test_warehouse'));

echo "<tr><td class='label'>" . _('Quantity:') . "</td><td>";
echo "<input type='text' name='test_qty' value='" . htmlspecialchars(get_post('test_qty', '1')) . "' size='12'>";
echo "</td></tr>\n";

// Optional: force strategy override
$test_strategies = array_merge(array('auto' => _('-- Auto (use hierarchy) --')), get_removal_strategies());
echo "<tr><td class='label'>" . _('Force Strategy:') . "</td><td>";
echo array_selector('test_strategy', get_post('test_strategy', 'auto'), $test_strategies);
echo "</td></tr>\n";

end_table(1);

echo "<div style='text-align:center; margin:10px 0;'>";
submit('TestRemoval', _('Test Removal'), true, _('Preview bin selection'), 'default');
echo "</div>";

// Display test results
if ($test_result !== null) {
	echo "<div style='margin-top:15px; padding:10px; border-radius:4px;";
	if (!empty($test_result['allocations'])) {
		if ($test_result['shortfall'] > 0) {
			echo " background:#fff3cd; border:1px solid #ffc107;'>";
			echo "<strong><i class='fa fa-exclamation-triangle' style='color:#856404;'></i> "
				. _('Partial Allocation') . "</strong><br>";
		} else {
			echo " background:#d4edda; border:1px solid #c3e6cb;'>";
			echo "<strong><i class='fa fa-check-circle' style='color:#28a745;'></i> "
				. _('Removal Result') . "</strong><br>";
		}
	} else {
		echo " background:#f8d7da; border:1px solid #f5c6cb;'>";
		echo "<strong><i class='fa fa-exclamation-triangle' style='color:#dc3545;'></i> "
			. _('No Stock Available') . "</strong><br>";
	}

	// Resolution info
	if ($test_result['resolution']) {
		$res = $test_result['resolution'];
		echo "<div style='margin-top:8px; padding:8px; background:rgba(255,255,255,0.7); border-radius:3px; font-size:12px;'>";
		echo "<strong>" . _('Resolved Strategy:') . "</strong> "
			. '<span style="font-weight:bold; text-transform:uppercase;">' . htmlspecialchars($res['strategy']) . '</span>';
		echo " — <em>" . htmlspecialchars($res['source']) . "</em>";
		if ($res['rule_id']) {
			echo " (Rule #" . $res['rule_id'] . ")";
		}
		echo "<br>";
		echo "<strong>" . _('Total Available:') . "</strong> "
			. number_format2($test_result['total_available'], 4);
		echo " | <strong>" . _('Requested:') . "</strong> "
			. number_format2((float)get_post('test_qty'), 4);
		echo " | <strong>" . _('To Pick:') . "</strong> "
			. number_format2($test_result['total_to_pick'], 4);
		if ($test_result['shortfall'] > 0) {
			echo " | <strong style='color:#dc3545;'>" . _('Shortfall:') . "</strong> "
				. number_format2($test_result['shortfall'], 4);
		}
		echo "</div>";
	}

	// Show message
	echo "<div style='margin-top:8px; white-space:pre-line;'>" . htmlspecialchars($test_result['message']) . "</div>";

	// Show allocation table
	if (!empty($test_result['allocations'])) {
		echo "<table style='margin-top:10px; width:100%; border-collapse:collapse;'>";
		echo "<tr style='background:#e2e3e5;'>";
		echo "<th style='padding:5px 8px; text-align:left;'>" . _('#') . "</th>";
		echo "<th style='padding:5px 8px; text-align:left;'>" . _('Bin Code') . "</th>";
		echo "<th style='padding:5px 8px; text-align:left;'>" . _('Bin Name') . "</th>";
		echo "<th style='padding:5px 8px; text-align:left;'>" . _('Batch') . "</th>";
		echo "<th style='padding:5px 8px; text-align:left;'>" . _('Expiry Date') . "</th>";
		echo "<th style='padding:5px 8px; text-align:right;'>" . _('Available') . "</th>";
		echo "<th style='padding:5px 8px; text-align:right;'>" . _('To Pick') . "</th>";
		echo "<th style='padding:5px 8px; text-align:right;'>" . _('Pick Seq') . "</th>";
		echo "</tr>";

		$total = 0;
		$n = 0;
		foreach ($test_result['allocations'] as $a) {
			$n++;
			$row_bg = ($n % 2 == 0) ? ' background:#f8f9fa;' : '';
			echo "<tr style='border-bottom:1px solid #dee2e6;$row_bg'>";
			echo "<td style='padding:4px 8px;'>" . $n . "</td>";
			echo "<td style='padding:4px 8px;'><i class='fa fa-cube'></i> " . htmlspecialchars($a['bin_code']) . "</td>";
			echo "<td style='padding:4px 8px;'>" . htmlspecialchars($a['bin_name']) . "</td>";

			// Batch info
			if ($a['batch_no']) {
				echo "<td style='padding:4px 8px;'>" . htmlspecialchars($a['batch_no']) . "</td>";
			} else {
				echo "<td style='padding:4px 8px; color:#999;'>—</td>";
			}

			// Expiry date with color coding
			if ($a['expiry_date']) {
				$today = date('Y-m-d');
				$exp_color = '#28a745';
				if ($a['expiry_date'] < $today) {
					$exp_color = '#dc3545';
				} elseif ($a['expiry_date'] <= date('Y-m-d', strtotime('+30 days'))) {
					$exp_color = '#fd7e14';
				} elseif ($a['expiry_date'] <= date('Y-m-d', strtotime('+90 days'))) {
					$exp_color = '#ffc107';
				}
				echo "<td style='padding:4px 8px; color:$exp_color; font-weight:bold;'>" . $a['expiry_date'] . "</td>";
			} else {
				echo "<td style='padding:4px 8px; color:#999;'>—</td>";
			}

			echo "<td style='padding:4px 8px; text-align:right;'>" . number_format2($a['qty_available'], 4) . "</td>";
			echo "<td style='padding:4px 8px; text-align:right; font-weight:bold;'>" . number_format2($a['qty_to_pick'], 4) . "</td>";
			echo "<td style='padding:4px 8px; text-align:right;'>" . ($a['pick_sequence'] !== null ? $a['pick_sequence'] : '—') . "</td>";
			echo "</tr>";
			$total += $a['qty_to_pick'];
		}

		echo "<tr style='font-weight:bold; border-top:2px solid #666;'>";
		echo "<td style='padding:4px 8px;' colspan='6'>" . _('Total to Pick') . "</td>";
		echo "<td style='padding:4px 8px; text-align:right;'>" . number_format2($total, 4) . "</td>";
		echo "<td></td>";
		echo "</tr></table>";
	}

	echo "</div>";
}

echo "</div>"; // End test removal box

end_form();
end_page();
