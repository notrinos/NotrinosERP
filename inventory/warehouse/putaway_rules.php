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
 * Putaway Rules — CRUD page for warehouse putaway rule definitions
 * and a "Test Putaway" tool to preview bin assignment results.
 *
 * Session 7 of the Unified Advanced Inventory Implementation Plan.
 */
$page_security = 'SA_WAREHOUSE_PUTAWAY';
$path_to_root = '../..';
include_once($path_to_root . '/includes/session.inc');

$_SESSION['page_title'] = _($help_context = 'Putaway Rules');

page($_SESSION['page_title']);

include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/inventory/warehouse/includes/db/warehouse_putaway_db.inc');
include_once($path_to_root . '/inventory/warehouse/includes/putaway_engine.inc');
include_once($path_to_root . '/inventory/warehouse/includes/warehouse_ui.inc');

simple_page_mode(true);

//-------------------------------------------------------------------------------------
// Handle ADD / UPDATE
//-------------------------------------------------------------------------------------

if ($Mode == 'ADD_ITEM' || $Mode == 'UPDATE_ITEM') {
	$input_error = 0;

	if (strlen(trim(get_post('rule_name'))) == 0) {
		$input_error = 1;
		display_error(_('The rule name must be entered.'));
		set_focus('rule_name');
	}

	$sequence = get_post('sequence');
	if ($sequence === '' || !is_numeric($sequence) || (int)$sequence < 0) {
		$input_error = 1;
		display_error(_('Sequence must be a positive number.'));
		set_focus('sequence');
	}

	$strategy = get_post('strategy');
	$valid_strategies = array_keys(get_putaway_strategies());
	if (!in_array($strategy, $valid_strategies)) {
		$input_error = 1;
		display_error(_('Please select a valid putaway strategy.'));
		set_focus('strategy');
	}

	// fixed_bin strategy requires target_loc_id
	if ($strategy == 'fixed_bin' && !get_post('target_loc_id')) {
		$input_error = 1;
		display_error(_('Fixed Bin strategy requires a target bin location.'));
		set_focus('target_loc_id');
	}

	if ($input_error != 1) {
		$warehouse_loc_code = get_post('warehouse_loc_code');
		$stock_id = get_post('stock_id');
		$category_id = get_post('category_id');
		$storage_category_id = get_post('storage_category_id');
		$target_loc_id = get_post('target_loc_id');
		$target_zone_id = get_post('target_zone_id');

		// Normalize "none" combo values to null
		$warehouse_loc_code = ($warehouse_loc_code && $warehouse_loc_code !== '' && $warehouse_loc_code !== '-1') ? $warehouse_loc_code : null;
		$stock_id = ($stock_id && $stock_id !== '' && $stock_id !== '-1') ? $stock_id : null;
		$category_id = ($category_id && $category_id !== '' && $category_id != -1 && $category_id != 0) ? (int)$category_id : null;
		$storage_category_id = ($storage_category_id && $storage_category_id !== '' && $storage_category_id != -1 && $storage_category_id != 0) ? (int)$storage_category_id : null;
		$target_loc_id = ($target_loc_id && $target_loc_id !== '' && $target_loc_id != -1 && $target_loc_id != 0) ? (int)$target_loc_id : null;
		$target_zone_id = ($target_zone_id && $target_zone_id !== '' && $target_zone_id != -1 && $target_zone_id != 0) ? (int)$target_zone_id : null;

		if ($selected_id != -1) {
			update_putaway_rule(
				$selected_id,
				get_post('rule_name'),
				$warehouse_loc_code,
				(int)$sequence,
				$stock_id,
				$category_id,
				$storage_category_id,
				$target_loc_id,
				$target_zone_id,
				$strategy,
				check_value('active') ? 1 : 0
			);
			display_notification(_('Putaway rule has been updated.'));
		} else {
			add_putaway_rule(
				get_post('rule_name'),
				$warehouse_loc_code,
				(int)$sequence,
				$stock_id,
				$category_id,
				$storage_category_id,
				$target_loc_id,
				$target_zone_id,
				$strategy,
				1
			);
			display_notification(_('New putaway rule has been added.'));
		}
		$Mode = 'RESET';
	}
}

//-------------------------------------------------------------------------------------
// Handle DELETE
//-------------------------------------------------------------------------------------

if ($Mode == 'Delete') {
	$can = can_delete_putaway_rule($selected_id);
	if ($can === true) {
		delete_putaway_rule($selected_id);
		display_notification(_('Selected putaway rule has been deleted.'));
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
// Handle Test Putaway
//-------------------------------------------------------------------------------------

$test_result = null;
if (isset($_POST['TestPutaway'])) {
	$Ajax->activate('_page_body');
	$test_stock_id = get_post('test_stock_id');
	$test_warehouse = get_post('test_warehouse');
	$test_qty = get_post('test_qty');

	if (!$test_stock_id) {
		display_error(_('Please select an item to test.'));
	} elseif (!$test_warehouse) {
		display_error(_('Please select a warehouse to test.'));
	} elseif (!$test_qty || !is_numeric($test_qty) || (float)$test_qty <= 0) {
		display_error(_('Please enter a valid quantity to test.'));
	} else {
		$test_result = test_putaway($test_stock_id, $test_warehouse, (float)$test_qty);
	}
}

//-------------------------------------------------------------------------------------
// List table
//-------------------------------------------------------------------------------------

$result = get_putaway_rules(null, check_value('show_inactive'));

start_form();
start_table(TABLESTYLE);
$th = array(
	_('#'), _('Rule Name'), _('Warehouse'), _('Sequence'),
	_('Item'), _('Category'), _('Storage Cat.'),
	_('Target'), _('Strategy'), _('Active'), '', ''
);
table_header($th);

$k = 0;
while ($myrow = db_fetch($result)) {
	alt_table_row_color($k);

	label_cell($myrow['rule_id']);
	label_cell($myrow['rule_name']);
	label_cell($myrow['warehouse_name'] ? $myrow['warehouse_name'] : '<em>' . _('All') . '</em>');
	label_cell($myrow['sequence'], 'align=right');

	// Item or Category
	if ($myrow['stock_id']) {
		label_cell($myrow['stock_id'] . ' - ' . $myrow['item_description']);
	} else {
		label_cell('<em>' . _('Any') . '</em>');
	}

	label_cell($myrow['item_category_name'] ? $myrow['item_category_name'] : '<em>' . _('Any') . '</em>');
	label_cell($myrow['storage_category_name'] ? $myrow['storage_category_name'] : '<em>' . _('Any') . '</em>');

	// Target
	if ($myrow['target_loc_id'] && $myrow['target_location_name']) {
		label_cell('<i class="fa fa-cube"></i> ' . $myrow['target_loc_code'] . ' - ' . $myrow['target_location_name']);
	} elseif ($myrow['target_zone_id'] && $myrow['target_zone_name']) {
		label_cell('<i class="fa fa-th-large"></i> ' . $myrow['target_zone_code'] . ' - ' . $myrow['target_zone_name']);
	} else {
		label_cell('<em>' . _('Auto') . '</em>');
	}

	// Strategy
	$strategies = get_putaway_strategies();
	$strategy_label = isset($strategies[$myrow['strategy']]) ? $strategies[$myrow['strategy']] : $myrow['strategy'];
	label_cell($strategy_label);

	// Active
	label_cell($myrow['active'] ? '<span style="color:#28a745;">' . _('Yes') . '</span>'
		: '<span style="color:#dc3545;">' . _('No') . '</span>', 'align=center');

	edit_button_cell("Edit" . $myrow['rule_id'], _('Edit'));
	delete_button_cell("Delete" . $myrow['rule_id'], _('Delete'));
	end_row();
}

end_table();

//-------------------------------------------------------------------------------------
// Add/Edit form
//-------------------------------------------------------------------------------------

echo '<br>';
start_table(TABLESTYLE2);

if ($selected_id != -1) {
	if ($Mode == 'Edit') {
		$myrow = get_putaway_rule($selected_id);
		$_POST['rule_name'] = $myrow['rule_name'];
		$_POST['warehouse_loc_code'] = $myrow['warehouse_loc_code'];
		$_POST['sequence'] = $myrow['sequence'];
		$_POST['stock_id'] = $myrow['stock_id'];
		$_POST['category_id'] = $myrow['category_id'];
		$_POST['storage_category_id'] = $myrow['storage_category_id'];
		$_POST['target_loc_id'] = $myrow['target_loc_id'];
		$_POST['target_zone_id'] = $myrow['target_zone_id'];
		$_POST['strategy'] = $myrow['strategy'];
		$_POST['active'] = $myrow['active'];
	}
	hidden('selected_id', $selected_id);
}

// --- Basic Info ---
echo "<tr><td colspan='2' style='padding-top:10px;'><strong>" . _('Rule Definition') . "</strong></td></tr>\n";
text_row_ex(_('Rule Name:'), 'rule_name', 50, 100);
text_row_ex(_('Sequence (Priority):'), 'sequence', 10, 10, null, null, null, get_post('sequence', '10'));

// --- Scope ---
echo "<tr><td colspan='2' style='padding-top:10px;'><strong>" . _('Scope (Matching Criteria)') . "</strong>"
	. " <small style='color:#777;'>" . _('Leave blank for "any"') . "</small></td></tr>\n";

warehouse_list_row(_('Warehouse:'), 'warehouse_loc_code', get_post('warehouse_loc_code'), _('-- All Warehouses --'));

echo "<tr><td class='label'>" . _('Specific Item:') . "</td><td>";
echo stock_items_list('stock_id', get_post('stock_id'), _('-- Any Item --'), false, array('cells' => false));
echo "</td></tr>\n";

stock_categories_list_row(_('Item Category:'), 'category_id', get_post('category_id'), _('-- Any Category --'));

storage_category_list_row(_('Storage Category:'), 'storage_category_id', get_post('storage_category_id'), _('-- Any --'));

// --- Target ---
echo "<tr><td colspan='2' style='padding-top:10px;'><strong>" . _('Target Location') . "</strong>"
	. " <small style='color:#777;'>" . _('Specify a fixed bin or a zone to search within') . "</small></td></tr>\n";

// Target bin (for fixed_bin strategy)
$wh_code = get_post('warehouse_loc_code');
if ($wh_code) {
	warehouse_bin_list_row(_('Target Bin (Fixed):'), 'target_loc_id', $wh_code, get_post('target_loc_id'), _('-- none --'));
} else {
	// Show all storable bins across all warehouses
	echo "<tr><td class='label'>" . _('Target Bin (Fixed):') . "</td><td>";
	$bin_sql = "SELECT wl.loc_id, CONCAT(wl.loc_code, ' - ', wl.loc_name, ' [', l.location_name, ']') AS display_name, 0 AS inactive
		FROM " . TB_PREF . "wh_locations wl
		INNER JOIN " . TB_PREF . "wh_location_types lt ON wl.location_type_id = lt.id
		INNER JOIN " . TB_PREF . "locations l ON wl.warehouse_loc_code = l.loc_code
		WHERE lt.can_store = 1 AND wl.is_active = 1
		ORDER BY l.location_name, wl.location_path";
	echo combo_input('target_loc_id', get_post('target_loc_id'), $bin_sql, 'loc_id', 'display_name',
		array('spec_option' => _('-- none --'), 'spec_id' => '', 'order' => false, 'async' => false));
	echo "</td></tr>\n";
}

// Target zone (for zone-based strategies)
echo "<tr><td class='label'>" . _('Target Zone:') . "</td><td>";
$zone_sql = "SELECT wl.loc_id, CONCAT(wl.loc_code, ' - ', wl.loc_name) AS display_name, 0 AS inactive
	FROM " . TB_PREF . "wh_locations wl
	INNER JOIN " . TB_PREF . "wh_location_types lt ON wl.location_type_id = lt.id
	WHERE lt.can_store = 0 AND wl.is_active = 1";
if ($wh_code) {
	$zone_sql .= " AND wl.warehouse_loc_code = " . db_escape($wh_code);
}
$zone_sql .= " ORDER BY wl.location_path";
echo combo_input('target_zone_id', get_post('target_zone_id'), $zone_sql, 'loc_id', 'display_name',
	array('spec_option' => _('-- none --'), 'spec_id' => '', 'order' => false, 'async' => false));
echo "</td></tr>\n";

// --- Strategy ---
echo "<tr><td colspan='2' style='padding-top:10px;'><strong>" . _('Strategy') . "</strong></td></tr>\n";
$strategies = get_putaway_strategies();
echo "<tr><td class='label'>" . _('Putaway Strategy:') . "</td><td>";
echo array_selector('strategy', get_post('strategy', 'first_available'), $strategies);
echo "</td></tr>\n";

// Strategy descriptions
echo "<tr><td></td><td><div style='background:#f8f9fa; border:1px solid #dee2e6; border-radius:4px; padding:8px; margin-top:5px; font-size:12px;'>";
echo "<strong>" . _('Strategy Guide:') . "</strong><br>";
echo "<b>" . _('Fixed Bin') . "</b> — " . _('Always route to the specified target bin') . "<br>";
echo "<b>" . _('First Available') . "</b> — " . _('Find the first empty bin in target zone') . "<br>";
echo "<b>" . _('Add to Existing') . "</b> — " . _('Prefer bins already containing the same item') . "<br>";
echo "<b>" . _('Nearest Available') . "</b> — " . _('Find the nearest available bin by pick_sequence') . "<br>";
echo "<b>" . _('ABC Class Match') . "</b> — " . _('Route to bins matching item ABC velocity class') . "<br>";
echo "<b>" . _('Storage Category Match') . "</b> — " . _('Route to bins matching item storage category') . "<br>";
echo "<b>" . _('Least Packages') . "</b> — " . _('Route to bin with fewest distinct items (consolidation)') . "<br>";
echo "</div></td></tr>\n";

if ($selected_id != -1) {
	check_row(_('Active'), 'active', get_post('active', 1));
}

end_table(1);
submit_add_or_update_center($selected_id == -1, '', 'both');

//-------------------------------------------------------------------------------------
// Test Putaway Tool
//-------------------------------------------------------------------------------------

echo '<br>';
echo "<div style='border:2px solid #007bff; border-radius:6px; padding:15px; margin:10px 0; background:#f0f7ff;'>";
echo "<h3 style='margin-top:0; color:#007bff;'><i class='fa fa-flask'></i> " . _('Test Putaway') . "</h3>";
echo "<p style='color:#555; font-size:13px;'>"
	. _('Preview where an item would be placed without actually moving anything.') . "</p>";

start_table(TABLESTYLE2);

echo "<tr><td class='label'>" . _('Item:') . "</td><td>";
echo stock_items_list('test_stock_id', get_post('test_stock_id'), false, false, array('cells' => false));
echo "</td></tr>\n";

warehouse_list_row(_('Warehouse:'), 'test_warehouse', get_post('test_warehouse'));

echo "<tr><td class='label'>" . _('Quantity:') . "</td><td>";
echo "<input type='text' name='test_qty' value='" . get_post('test_qty', '1') . "' size='12'>";
echo "</td></tr>\n";

end_table(1);

echo "<div style='text-align:center; margin:10px 0;'>";
submit('TestPutaway', _('Test Putaway'), true, _('Preview bin assignment'), 'default');
echo "</div>";

// Display test results
if ($test_result !== null) {
	echo "<div style='margin-top:15px; padding:10px; border-radius:4px;";
	if (!empty($test_result['assignments'])) {
		echo " background:#d4edda; border:1px solid #c3e6cb;'>";
		echo "<strong><i class='fa fa-check-circle' style='color:#28a745;'></i> " . _('Putaway Result') . "</strong><br>";
	} else {
		echo " background:#f8d7da; border:1px solid #f5c6cb;'>";
		echo "<strong><i class='fa fa-exclamation-triangle' style='color:#dc3545;'></i> " . _('Putaway Result') . "</strong><br>";
	}

	echo "<div style='margin-top:8px; white-space:pre-line;'>" . htmlspecialchars($test_result['message']) . "</div>";

	// Show matched rule details
	if ($test_result['rule']) {
		$r = $test_result['rule'];
		echo "<div style='margin-top:8px; padding:8px; background:rgba(255,255,255,0.7); border-radius:3px; font-size:12px;'>";
		echo "<strong>" . _('Matched Rule:') . "</strong> " . htmlspecialchars($r['rule_name'])
			. " (ID: " . $r['rule_id'] . ", Seq: " . $r['sequence'] . ")<br>";
		echo "<strong>" . _('Strategy:') . "</strong> " . htmlspecialchars($r['strategy']) . "<br>";
		if ($r['stock_id']) echo "<strong>" . _('Item Filter:') . "</strong> " . htmlspecialchars($r['stock_id']) . "<br>";
		if ($r['category_id']) echo "<strong>" . _('Category Filter:') . "</strong> #" . $r['category_id'] . "<br>";
		if ($r['warehouse_loc_code']) echo "<strong>" . _('Warehouse:') . "</strong> " . htmlspecialchars($r['warehouse_loc_code']) . "<br>";
		echo "</div>";
	}

	// Show assignment table
	if (!empty($test_result['assignments'])) {
		echo "<table style='margin-top:10px; width:100%; border-collapse:collapse;'>";
		echo "<tr style='background:#e2e3e5;'>";
		echo "<th style='padding:5px 8px; text-align:left;'>" . _('Bin Code') . "</th>";
		echo "<th style='padding:5px 8px; text-align:left;'>" . _('Bin Name') . "</th>";
		echo "<th style='padding:5px 8px; text-align:right;'>" . _('Quantity') . "</th>";
		echo "</tr>";

		$total = 0;
		foreach ($test_result['assignments'] as $a) {
			echo "<tr style='border-bottom:1px solid #ccc;'>";
			echo "<td style='padding:4px 8px;'><i class='fa fa-cube'></i> " . htmlspecialchars($a['bin_code']) . "</td>";
			echo "<td style='padding:4px 8px;'>" . htmlspecialchars($a['bin_name']) . "</td>";
			echo "<td style='padding:4px 8px; text-align:right;'>" . number_format2($a['qty'], 4) . "</td>";
			echo "</tr>";
			$total += $a['qty'];
		}

		echo "<tr style='font-weight:bold; border-top:2px solid #666;'>";
		echo "<td style='padding:4px 8px;' colspan='2'>" . _('Total Assigned') . "</td>";
		echo "<td style='padding:4px 8px; text-align:right;'>" . number_format2($total, 4) . "</td>";
		echo "</tr></table>";
	}

	echo "</div>";
}

echo "</div>"; // End test putaway box

end_form();
end_page();
