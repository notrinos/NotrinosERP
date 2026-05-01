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
 * Warehouse Routes — Route builder UI with push/pull rules.
 *
 * Allows creating and managing stock flow routes (inbound/outbound/internal/crossdock)
 * with ordered push/pull rules. Includes one-click template installation and
 * route resolution testing.
 *
 * Session 13 of the Unified Advanced Inventory Implementation Plan.
 */
$page_security = 'SA_WAREHOUSE_ROUTES';
$path_to_root = '../..';
include_once($path_to_root . '/includes/session.inc');

$_SESSION['page_title'] = _($help_context = 'Warehouse Routes');

page($_SESSION['page_title']);

include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/inventory/warehouse/includes/db/warehouse_routes_db.inc');
include_once($path_to_root . '/inventory/warehouse/includes/warehouse_ui.inc');

simple_page_mode(true);

//-------------------------------------------------------------------------------------
// Handle ROUTE ADD / UPDATE
//-------------------------------------------------------------------------------------

if ($Mode == 'ADD_ITEM' || $Mode == 'UPDATE_ITEM') {
	$input_error = 0;

	if (strlen(trim(get_post('route_name'))) == 0) {
		$input_error = 1;
		display_error(_('The route name must be entered.'));
		set_focus('route_name');
	}

	$route_type = get_post('route_type');
	$valid_types = array_keys(get_route_types());
	if (!in_array($route_type, $valid_types)) {
		$input_error = 1;
		display_error(_('Please select a valid route type.'));
		set_focus('route_type');
	}

	$sequence = get_post('sequence');
	if ($sequence === '' || !is_numeric($sequence) || (int)$sequence < 0) {
		$input_error = 1;
		display_error(_('Sequence must be a non-negative number.'));
		set_focus('sequence');
	}

	if ($input_error != 1) {
		$warehouse_loc_code = get_post('warehouse_loc_code');
		$warehouse_loc_code = ($warehouse_loc_code && $warehouse_loc_code !== '' && $warehouse_loc_code !== ALL_TEXT) ? $warehouse_loc_code : null;
		$is_default = check_value('is_default') ? true : false;
		$active = check_value('active') ? true : false;
		$description = get_post('description');

		if ($selected_id != -1) {
			update_route(
				$selected_id,
				get_post('route_name'),
				$route_type,
				$warehouse_loc_code,
				$is_default,
				(int)$sequence,
				$active,
				$description
			);
			display_notification(_('Route has been updated.'));
		} else {
			$new_id = add_route(
				get_post('route_name'),
				$route_type,
				$warehouse_loc_code,
				$is_default,
				(int)$sequence,
				true,
				$description
			);
			display_notification(_('New route has been added.'));
		}
		$Mode = 'RESET';
	}
}

//-------------------------------------------------------------------------------------
// Handle ROUTE DELETE
//-------------------------------------------------------------------------------------

if ($Mode == 'Delete') {
	$can = can_delete_route($selected_id);
	if ($can === true) {
		delete_route($selected_id);
		display_notification(_('Selected route and its rules have been deleted.'));
	} else {
		display_error($can);
	}
	$Mode = 'RESET';
}

//-------------------------------------------------------------------------------------
// Handle RULE ADD
//-------------------------------------------------------------------------------------

if (isset($_POST['AddRule']) && get_post('edit_route_id')) {
	$edit_route_id = (int)get_post('edit_route_id');
	$input_error = 0;

	$rule_type = get_post('rule_type');
	if (!in_array($rule_type, array_keys(get_rule_types()))) {
		$input_error = 1;
		display_error(_('Please select a valid rule type.'));
	}

	$operation_type = get_post('rule_operation_type');
	if (!in_array($operation_type, array_keys(get_rule_operation_types()))) {
		$input_error = 1;
		display_error(_('Please select a valid operation type.'));
	}

	$trigger_method = get_post('rule_trigger_method');
	if (!in_array($trigger_method, array_keys(get_trigger_methods()))) {
		$input_error = 1;
		display_error(_('Please select a valid trigger method.'));
	}

	$rule_sequence = get_post('rule_sequence');
	if ($rule_sequence === '' || !is_numeric($rule_sequence)) {
		$input_error = 1;
		display_error(_('Rule sequence must be a number.'));
	}

	if ($input_error != 1) {
		$from_loc_id = get_post('rule_from_loc_id');
		$from_loc_id = ($from_loc_id && $from_loc_id !== '' && $from_loc_id != 0 && $from_loc_id != -1) ? (int)$from_loc_id : null;
		$to_loc_id = get_post('rule_to_loc_id');
		$to_loc_id = ($to_loc_id && $to_loc_id !== '' && $to_loc_id != 0 && $to_loc_id != -1) ? (int)$to_loc_id : null;
		$delay_days = get_post('rule_delay_days');
		$delay_days = is_numeric($delay_days) ? (int)$delay_days : 0;

		add_route_rule(
			$edit_route_id,
			$rule_type,
			(int)$rule_sequence,
			$from_loc_id,
			$to_loc_id,
			$operation_type,
			$trigger_method,
			$delay_days,
			true
		);
		display_notification(_('Rule has been added to the route.'));
	}
	$Ajax->activate('_page_body');
}

//-------------------------------------------------------------------------------------
// Handle RULE DELETE
//-------------------------------------------------------------------------------------

if (isset($_POST['DeleteRule'])) {
	$rule_id = (int)$_POST['DeleteRule'];
	delete_route_rule($rule_id);
	display_notification(_('Rule has been deleted.'));
	$Ajax->activate('_page_body');
}

//-------------------------------------------------------------------------------------
// Handle TEMPLATE INSTALL
//-------------------------------------------------------------------------------------

if (isset($_POST['InstallTemplate'])) {
	$template_key = get_post('template_key');
	$template_warehouse = get_post('template_warehouse');
	$template_warehouse = ($template_warehouse && $template_warehouse !== '' && $template_warehouse !== ALL_TEXT) ? $template_warehouse : null;
	$template_default = check_value('template_default');

	$route_id = install_route_template($template_key, $template_warehouse, $template_default);
	if ($route_id) {
		display_notification(sprintf(_('Route template installed successfully (Route #%d).'), $route_id));
	} else {
		display_error(_('Failed to install route template. Invalid template selected.'));
	}
	$Ajax->activate('_page_body');
}

//-------------------------------------------------------------------------------------
// Handle TEST ROUTE RESOLUTION
//-------------------------------------------------------------------------------------

$test_result = null;
if (isset($_POST['TestResolve'])) {
	$test_warehouse = get_post('test_warehouse');
	$test_type = get_post('test_type');

	if (!$test_warehouse) {
		display_error(_('Please select a warehouse to test.'));
	} elseif (!$test_type) {
		display_error(_('Please select a route type to test.'));
	} else {
		$test_result = resolve_route($test_warehouse, $test_type);
	}
	$Ajax->activate('_page_body');
}

//-------------------------------------------------------------------------------------
// RESET
//-------------------------------------------------------------------------------------

if ($Mode == 'RESET') {
	$selected_id = -1;
	$sav = get_post('show_inactive');
	unset($_POST);
	$_POST['show_inactive'] = $sav;
}

//============================================================================================
// DISPLAY
//============================================================================================

start_form();

//-------------------------------------------------------------------------------------
// Summary Cards
//-------------------------------------------------------------------------------------

$type_summary = get_route_type_summary();
$total_routes = 0;
foreach ($type_summary as $cnt) $total_routes += $cnt;

echo '<div style="display:flex;flex-wrap:wrap;gap:10px;margin-bottom:15px;">';

// Total routes
echo '<div style="flex:1;min-width:140px;padding:12px 16px;border-radius:6px;background:#f8f9fa;border-left:4px solid #6c757d;">';
echo '<div style="font-size:24px;font-weight:bold;color:#6c757d;">' . $total_routes . '</div>';
echo '<div style="font-size:12px;color:#888;">' . _('Total Routes') . '</div></div>';

$route_types = get_route_types();
foreach ($route_types as $type_code => $type_label) {
	$cnt = isset($type_summary[$type_code]) ? $type_summary[$type_code] : 0;
	$color = get_route_type_color($type_code);
	echo '<div style="flex:1;min-width:140px;padding:12px 16px;border-radius:6px;background:#f8f9fa;border-left:4px solid ' . $color . ';">';
	echo '<div style="font-size:24px;font-weight:bold;color:' . $color . ';">' . $cnt . '</div>';
	echo '<div style="font-size:12px;color:#888;">' . $type_label . '</div></div>';
}

echo '</div>';

//-------------------------------------------------------------------------------------
// Routes List Table
//-------------------------------------------------------------------------------------

$filter_type = get_post('filter_type');
$filter_warehouse = get_post('filter_warehouse');
$show_inactive = check_value('show_inactive');

// Filter bar
start_table(TABLESTYLE_NOBORDER);
start_row();

$filter_types = array('' => _('All Route Types')) + get_route_types();
echo '<td>'.array_selector('filter_type', $filter_type, $filter_types, array('select_submit' => true)).'</td>';

warehouse_list_cells(null, 'filter_warehouse', $filter_warehouse, _('All Warehouses'), true);

check_cells(_('Show inactive:'), 'show_inactive', $show_inactive, true);
submit_cells('search', _('Search'), '', _('Search'), 'default');

end_row();
end_table();

echo '<br>';

$result = get_routes(
	$filter_type ? $filter_type : null,
	($filter_warehouse && $filter_warehouse !== ALL_TEXT) ? $filter_warehouse : null,
	$show_inactive
);

start_table(TABLESTYLE);
$th = array(
	_('#'), _('Route Name'), _('Type'), _('Warehouse'), _('Default'),
	_('Sequence'), _('Rules'), _('Active'), '', ''
);
inactive_control_column($th);
table_header($th);

$k = 0;
while ($myrow = db_fetch($result)) {
	alt_table_row_color($k);

	label_cell($myrow['route_id']);
	label_cell('<strong>' . htmlspecialchars($myrow['route_name'], ENT_QUOTES) . '</strong>'
		. ($myrow['description'] ? '<br><small style="color:#888;">' . htmlspecialchars($myrow['description'], ENT_QUOTES) . '</small>' : ''));
	label_cell(route_type_badge($myrow['route_type']));
	label_cell($myrow['warehouse_name'] ? htmlspecialchars($myrow['warehouse_name'], ENT_QUOTES) : '<em>' . _('Global') . '</em>');
	label_cell($myrow['is_default']
		? '<span style="color:#28a745;font-weight:bold;"><i class="fa fa-star"></i> ' . _('Yes') . '</span>'
		: '<span style="color:#999;">' . _('No') . '</span>', 'align=center');
	label_cell($myrow['sequence'], 'align=right');

	// Rule count
	$rule_count = count_route_rules($myrow['route_id']);
	label_cell($rule_count, 'align=center');

	// Active
	label_cell($myrow['active']
		? '<span style="color:#28a745;">' . _('Yes') . '</span>'
		: '<span style="color:#dc3545;">' . _('No') . '</span>', 'align=center');

	edit_button_cell("Edit" . $myrow['route_id'], _('Edit'));
	delete_button_cell("Delete" . $myrow['route_id'], _('Delete'));

	end_row();
}

end_table();

//-------------------------------------------------------------------------------------
// Route Add/Edit Form
//-------------------------------------------------------------------------------------

echo '<br>';
start_table(TABLESTYLE2);

if ($selected_id != -1) {
	if ($Mode == 'Edit') {
		$myrow = get_route($selected_id);
		$_POST['route_name'] = $myrow['route_name'];
		$_POST['route_type'] = $myrow['route_type'];
		$_POST['warehouse_loc_code'] = $myrow['warehouse_loc_code'];
		$_POST['is_default'] = $myrow['is_default'];
		$_POST['sequence'] = $myrow['sequence'];
		$_POST['active'] = $myrow['active'];
		$_POST['description'] = $myrow['description'];
		$_POST['edit_route_id'] = $selected_id;
	}
	hidden('selected_id', $selected_id);
	hidden('edit_route_id', $selected_id);
}

echo "<tr><td colspan='2' style='padding-top:10px;'><strong>"
	. ($selected_id != -1 ? _('Edit Route') : _('New Route'))
	. "</strong></td></tr>\n";

text_row_ex(_('Route Name:'), 'route_name', 60, 100);

// Route type
echo "<tr><td class='label'>" . _('Route Type:') . "</td><td>";
echo array_selector('route_type', get_post('route_type', 'inbound'), get_route_types());
echo "</td></tr>\n";

// Warehouse scope
warehouse_list_row(_('Warehouse:'), 'warehouse_loc_code', get_post('warehouse_loc_code'), _('-- Global (All Warehouses) --'));

check_row(_('Default Route:'), 'is_default', get_post('is_default', 0));

text_row_ex(_('Sequence (Priority):'), 'sequence', 10, 10, null, null, null, get_post('sequence', '10'));

// Description
echo "<tr><td class='label'>" . _('Description:') . "</td><td>";
echo "<textarea name='description' rows='2' cols='60'>" . htmlspecialchars(get_post('description', ''), ENT_QUOTES) . "</textarea>";
echo "</td></tr>\n";

if ($selected_id != -1) {
	check_row(_('Active'), 'active', get_post('active', 1));
}

end_table(1);
submit_add_or_update_center($selected_id == -1, '', 'both');

//-------------------------------------------------------------------------------------
// Rules Section (shown when editing a route)
//-------------------------------------------------------------------------------------

if ($selected_id != -1) {
	$edit_route_id = $selected_id;

	echo '<br>';
	echo "<div style='border:2px solid #17a2b8;border-radius:6px;padding:15px;margin:10px 0;background:#f0fcff;'>";
	echo "<h3 style='margin-top:0;color:#17a2b8;'><i class='fa fa-list-ol'></i> "
		. sprintf(_('Rules for Route #%d'), $edit_route_id) . "</h3>";
	echo "<p style='color:#555;font-size:13px;'>"
		. _('Rules define the operation steps in this route. They are executed in sequence order.') . "</p>";

	// Current rules table
	$rules_result = get_route_rules($edit_route_id);
	$has_rules = false;

	start_table(TABLESTYLE);
	$rth = array(_('Seq'), _('Rule Type'), _('Operation'), _('From Location'), _('To Location'),
		_('Trigger'), _('Delay (Days)'), _('Active'), '');
	table_header($rth);

	$k = 0;
	while ($rule_row = db_fetch($rules_result)) {
		$has_rules = true;
		alt_table_row_color($k);

		label_cell($rule_row['sequence'], 'align=center');

		// Rule type badge
		$rt_color = ($rule_row['rule_type'] === 'push') ? '#28a745' : '#007bff';
		label_cell('<span style="display:inline-block;padding:2px 8px;border-radius:3px;background:'
			. $rt_color . ';color:#fff;font-size:11px;font-weight:bold;">'
			. htmlspecialchars(strtoupper($rule_row['rule_type']), ENT_QUOTES) . '</span>');

		// Operation type
		$op_types = get_rule_operation_types();
		$op_label = isset($op_types[$rule_row['operation_type']]) ? $op_types[$rule_row['operation_type']] : $rule_row['operation_type'];
		label_cell($op_label);

		// From location
		label_cell($rule_row['from_loc_name']
			? htmlspecialchars($rule_row['from_loc_code'] . ' - ' . $rule_row['from_loc_name'], ENT_QUOTES)
			: '<em>' . _('Any / Auto') . '</em>');

		// To location
		label_cell($rule_row['to_loc_name']
			? htmlspecialchars($rule_row['to_loc_code'] . ' - ' . $rule_row['to_loc_name'], ENT_QUOTES)
			: '<em>' . _('Any / Auto') . '</em>');

		// Trigger
		$triggers = get_trigger_methods();
		$trigger_label = isset($triggers[$rule_row['trigger_method']]) ? $triggers[$rule_row['trigger_method']] : $rule_row['trigger_method'];
		$trigger_color = '#6c757d';
		if ($rule_row['trigger_method'] === 'auto') $trigger_color = '#28a745';
		elseif ($rule_row['trigger_method'] === 'scheduled') $trigger_color = '#ffc107';
		label_cell('<span style="color:' . $trigger_color . ';font-weight:bold;">' . $trigger_label . '</span>');

		// Delay
		label_cell($rule_row['delay_days'] > 0 ? $rule_row['delay_days'] . ' ' . _('days') : '-', 'align=center');

		// Active
		label_cell($rule_row['active']
			? '<span style="color:#28a745;">' . _('Yes') . '</span>'
			: '<span style="color:#dc3545;">' . _('No') . '</span>', 'align=center');

		// Delete button
		echo '<td>';
		echo '<button type="submit" name="DeleteRule" value="' . $rule_row['rule_id'] . '" class="btn-small"'
			. ' onclick="return confirm(\'' . _('Are you sure you want to delete this rule?') . '\');"'
			. ' style="background:#dc3545;color:#fff;border:none;padding:3px 8px;border-radius:3px;cursor:pointer;">'
			. '<i class="fa fa-trash"></i></button>';
		echo '</td>';

		end_row();
	}

	if (!$has_rules) {
		echo '<tr><td colspan="9" align="center" style="padding:15px;color:#888;"><em>'
			. _('No rules defined yet. Add rules below or install a template.') . '</em></td></tr>';
	}

	end_table();

	// Add Rule form
	echo '<br>';
	echo "<div style='background:#fff;border:1px solid #dee2e6;border-radius:4px;padding:12px;margin-top:10px;'>";
	echo "<strong><i class='fa fa-plus-circle'></i> " . _('Add Rule') . "</strong>";

	start_table(TABLESTYLE2);

	// Rule type
	echo "<tr><td class='label'>" . _('Rule Type:') . "</td><td>";
	echo array_selector('rule_type', get_post('rule_type', 'push'), get_rule_types());
	echo "</td></tr>\n";

	// Sequence
	text_row_ex(_('Sequence:'), 'rule_sequence', 10, 10, null, null, null, get_post('rule_sequence', '10'));

	// Operation type
	echo "<tr><td class='label'>" . _('Operation Type:') . "</td><td>";
	echo array_selector('rule_operation_type', get_post('rule_operation_type', 'receipt'), get_rule_operation_types());
	echo "</td></tr>\n";

	// From location (warehouse bin/zone selector)
	$route_data = get_route($edit_route_id);
	$wh_code = $route_data ? $route_data['warehouse_loc_code'] : null;

	echo "<tr><td class='label'>" . _('From Location:') . "</td><td>";
	if ($wh_code) {
		$loc_sql = "SELECT wl.loc_id, CONCAT(wl.loc_code, ' - ', wl.loc_name) AS display_name, 0 AS inactive
			FROM " . TB_PREF . "wh_locations wl
			WHERE wl.warehouse_loc_code = " . db_escape($wh_code) . " AND wl.is_active = 1
			ORDER BY wl.location_path";
	} else {
		$loc_sql = "SELECT wl.loc_id, CONCAT(wl.loc_code, ' - ', wl.loc_name, ' [', COALESCE(l.location_name,''), ']') AS display_name, 0 AS inactive
			FROM " . TB_PREF . "wh_locations wl
			LEFT JOIN " . TB_PREF . "locations l ON wl.warehouse_loc_code = l.loc_code
			WHERE wl.is_active = 1
			ORDER BY wl.location_path";
	}
	echo combo_input('rule_from_loc_id', get_post('rule_from_loc_id'), $loc_sql, 'loc_id', 'display_name',
		array('spec_option' => _('-- Any / Auto --'), 'spec_id' => '', 'order' => false, 'async' => false));
	echo "</td></tr>\n";

	// To location
	echo "<tr><td class='label'>" . _('To Location:') . "</td><td>";
	echo combo_input('rule_to_loc_id', get_post('rule_to_loc_id'), $loc_sql, 'loc_id', 'display_name',
		array('spec_option' => _('-- Any / Auto --'), 'spec_id' => '', 'order' => false, 'async' => false));
	echo "</td></tr>\n";

	// Trigger method
	echo "<tr><td class='label'>" . _('Trigger Method:') . "</td><td>";
	echo array_selector('rule_trigger_method', get_post('rule_trigger_method', 'auto'), get_trigger_methods());
	echo " <small style='color:#888;'>" . _('Auto = auto-create when previous step completes; Manual = user starts; Scheduled = delay-based') . "</small>";
	echo "</td></tr>\n";

	// Delay days
	text_row_ex(_('Delay (Days):'), 'rule_delay_days', 10, 10, null, null, null, get_post('rule_delay_days', '0'));

	end_table(0);

	echo "<div style='text-align:center;padding:10px;'>";
	submit('AddRule', _('Add Rule'), true, _('Add this rule to the route'), 'default');
	echo "</div>";
	echo "</div>"; // end add rule form div

	echo "</div>"; // end rules section div
}

//-------------------------------------------------------------------------------------
// Template Installation Section
//-------------------------------------------------------------------------------------

echo '<br>';
echo "<div style='border:2px solid #6f42c1;border-radius:6px;padding:15px;margin:10px 0;background:#f8f0ff;'>";
echo "<h3 style='margin-top:0;color:#6f42c1;'><i class='fa fa-magic'></i> " . _('Install Route Template') . "</h3>";
echo "<p style='color:#555;font-size:13px;'>"
	. _('Create a pre-configured route with one click. You can customize rules after installation.') . "</p>";

$templates = get_route_templates();

start_table(TABLESTYLE2);

// Template selector
echo "<tr><td class='label'>" . _('Template:') . "</td><td>";
$template_options = array();
foreach ($templates as $key => $tpl) {
	$template_options[$key] = $tpl['name'] . ' (' . $tpl['type'] . ')';
}
echo array_selector('template_key', get_post('template_key', 'inbound_2step'), $template_options);
echo "</td></tr>\n";

// Template descriptions
echo "<tr><td></td><td><div style='background:#f8f9fa;border:1px solid #dee2e6;border-radius:4px;padding:8px;margin-top:5px;font-size:12px;'>";
foreach ($templates as $key => $tpl) {
	$step_count = count($tpl['rules']);
	$steps = array();
	foreach ($tpl['rules'] as $r) {
		$op_types = get_rule_operation_types();
		$steps[] = isset($op_types[$r['operation_type']]) ? $op_types[$r['operation_type']] : $r['operation_type'];
	}
	echo "<div style='margin-bottom:4px;'>";
	echo "<strong>" . htmlspecialchars($tpl['name'], ENT_QUOTES) . "</strong>: ";
	echo htmlspecialchars($tpl['description'], ENT_QUOTES);
	echo " <em>(" . implode(' → ', $steps) . ")</em>";
	echo "</div>";
}
echo "</div></td></tr>\n";

// Warehouse
warehouse_list_row(_('For Warehouse:'), 'template_warehouse', get_post('template_warehouse'), _('-- Global --'));

check_row(_('Set as Default:'), 'template_default', get_post('template_default', 0));

end_table(0);

echo "<div style='text-align:center;padding:10px;'>";
submit('InstallTemplate', _('Install Template'), true, _('Create the route from this template'), 'default');
echo "</div>";
echo "</div>";

//-------------------------------------------------------------------------------------
// Test Route Resolution
//-------------------------------------------------------------------------------------

echo '<br>';
echo "<div style='border:2px solid #007bff;border-radius:6px;padding:15px;margin:10px 0;background:#f0f7ff;'>";
echo "<h3 style='margin-top:0;color:#007bff;'><i class='fa fa-search'></i> " . _('Test Route Resolution') . "</h3>";
echo "<p style='color:#555;font-size:13px;'>"
	. _('Preview which route would be selected for a given warehouse and direction.') . "</p>";

start_table(TABLESTYLE2);

warehouse_list_row(_('Warehouse:'), 'test_warehouse', get_post('test_warehouse'));

echo "<tr><td class='label'>" . _('Direction:') . "</td><td>";
echo array_selector('test_type', get_post('test_type', 'inbound'), get_route_types());
echo "</td></tr>\n";

end_table(0);

echo "<div style='text-align:center;padding:10px;'>";
submit('TestResolve', _('Test Resolution'), true, _('Find which route applies'), 'default');
echo "</div>";

// Display test result
if ($test_result !== null) {
	echo "<div style='background:#d4edda;border:1px solid #c3e6cb;border-radius:4px;padding:12px;margin-top:10px;'>";
	echo "<strong><i class='fa fa-check-circle' style='color:#28a745;'></i> " . _('Resolved Route:') . "</strong><br>";
	echo "<table style='margin-top:8px;'>";
	echo "<tr><td style='padding-right:15px;'><strong>" . _('Route:') . "</strong></td><td>#" . $test_result['route_id'] . " — " . htmlspecialchars($test_result['route_name'], ENT_QUOTES) . "</td></tr>";
	echo "<tr><td><strong>" . _('Type:') . "</strong></td><td>" . route_type_badge($test_result['route_type']) . "</td></tr>";
	echo "<tr><td><strong>" . _('Warehouse:') . "</strong></td><td>" . ($test_result['warehouse_name'] ? htmlspecialchars($test_result['warehouse_name'], ENT_QUOTES) : _('Global')) . "</td></tr>";
	echo "<tr><td><strong>" . _('Default:') . "</strong></td><td>" . ($test_result['is_default'] ? _('Yes') : _('No')) . "</td></tr>";

	// Show rules
	$resolved_rules = get_route_rules($test_result['route_id'], true);
	$rule_steps = array();
	while ($rr = db_fetch($resolved_rules)) {
		$op_types = get_rule_operation_types();
		$step_label = isset($op_types[$rr['operation_type']]) ? $op_types[$rr['operation_type']] : $rr['operation_type'];
		$rule_steps[] = $step_label;
	}
	echo "<tr><td><strong>" . _('Steps:') . "</strong></td><td>" . implode(' → ', $rule_steps) . "</td></tr>";
	echo "</table></div>";
} elseif (isset($_POST['TestResolve'])) {
	echo "<div style='background:#f8d7da;border:1px solid #f5c6cb;border-radius:4px;padding:12px;margin-top:10px;'>";
	echo "<i class='fa fa-exclamation-triangle' style='color:#dc3545;'></i> "
		. _('No route found for the selected warehouse and direction. Create one or install a template.');
	echo "</div>";
}

echo "</div>";

end_form();
end_page();
