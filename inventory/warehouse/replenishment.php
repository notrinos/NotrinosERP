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
 * Replenishment Engine — Rules management, suggestions dashboard, and execution.
 *
 * Two-tab interface:
 *   Tab 1: Rules — CRUD for replenishment rules with type-specific fields
 *   Tab 2: Suggestions — Engine evaluation results, manual/auto execution
 *
 * @package NotrinosERP
 * @subpackage Inventory/Warehouse
 */

$page_security = 'SA_WAREHOUSE_REPLENISHMENT';
$path_to_root = '../..';

include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/includes/date_functions.inc');
include_once($path_to_root . '/includes/data_checks.inc');
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/includes/ui/ui_lists.inc');
include_once($path_to_root . '/inventory/warehouse/includes/db/warehouse_replenishment_db.inc');
include_once($path_to_root . '/inventory/warehouse/includes/db/warehouse_transfers_db.inc');
include_once($path_to_root . '/inventory/warehouse/includes/db/warehouse_locations_db.inc');
include_once($path_to_root . '/inventory/warehouse/includes/warehouse_ui.inc');
include_once($path_to_root . '/inventory/warehouse/includes/replenishment_engine.inc');

// ===== PAGE SETUP =====
$js = '';
if (user_use_date_picker())
	$js .= get_js_date_picker();

page(_('Replenishment Engine'), false, false, '', $js);

// ===== TAB MANAGEMENT =====
$active_tab = get_post('active_tab', 'suggestions');
if (isset($_POST['tab_rules'])) {
	$active_tab = 'rules';
	$Ajax->activate('_page_body');
}
if (isset($_POST['tab_suggestions'])) {
	$active_tab = 'suggestions';
	$Ajax->activate('_page_body');
}

// ===== RULE EDIT STATE =====
simple_page_mode(true);

// ===== PROCESS ACTIONS =====

// --- Delete rule ---
if ($Mode == 'Delete') {
	if (can_delete_replenishment_rule($selected_id)) {
		delete_replenishment_rule($selected_id);
		display_notification(_('Replenishment rule has been deleted.'));
	}
	$Mode = 'RESET';
	$Ajax->activate('_page_body');
}

// --- Add or Update rule ---
if ($Mode == 'ADD_ITEM' || $Mode == 'UPDATE_ITEM') {
	$input_error = 0;

	if (strlen(get_post('rule_name')) < 1) {
		$input_error = 1;
		display_error(_('Rule name cannot be empty.'));
	}

	$rule_type = get_post('rule_type');
	$rule_types = get_replenishment_rule_types();
	if (!isset($rule_types[$rule_type])) {
		$input_error = 1;
		display_error(_('Invalid rule type selected.'));
	}

	if ($rule_type == 'min_max' || $rule_type == 'inter_warehouse' || $rule_type == 'pick_face') {
		if (get_post('min_qty') === '' || get_post('max_qty') === '') {
			$input_error = 1;
			display_error(_('Min and Max quantities are required for this rule type.'));
		}
		if ((float)get_post('min_qty') >= (float)get_post('max_qty')) {
			$input_error = 1;
			display_error(_('Min quantity must be less than Max quantity.'));
		}
	}

	if ($rule_type == 'reorder_point' && get_post('reorder_qty') === '') {
		$input_error = 1;
		display_error(_('Reorder quantity is required for Reorder Point rules.'));
	}

	if ($rule_type == 'inter_warehouse' && !get_post('supply_warehouse')) {
		$input_error = 1;
		display_error(_('Supply warehouse is required for Inter-Warehouse rules.'));
	}

	if ($rule_type == 'pick_face' && !get_post('stock_id')) {
		$input_error = 1;
		display_error(_('An item must be selected for Pick-Face rules.'));
	}

	if ($input_error == 0) {
		if ($Mode == 'ADD_ITEM') {
			add_replenishment_rule(
				get_post('rule_name'),
				$rule_type,
				get_post('stock_id') ? get_post('stock_id') : null,
				get_post('category_id') ? get_post('category_id') : null,
				get_post('warehouse_loc_code') ? get_post('warehouse_loc_code') : null,
				get_post('bin_loc_id') ? get_post('bin_loc_id') : null,
				get_post('min_qty') !== '' ? get_post('min_qty') : null,
				get_post('max_qty') !== '' ? get_post('max_qty') : null,
				get_post('reorder_qty') !== '' ? get_post('reorder_qty') : null,
				get_post('safety_stock') !== '' ? get_post('safety_stock') : null,
				get_post('lead_time_days') !== '' ? get_post('lead_time_days') : null,
				get_post('supply_warehouse') ? get_post('supply_warehouse') : null,
				get_post('supply_method', 'purchase'),
				get_post('preferred_supplier') ? get_post('preferred_supplier') : null,
				check_value('auto_execute') ? 1 : 0
			);
			display_notification(_('New replenishment rule has been added.'));
			$Ajax->activate('_page_body');
		} else {
			update_replenishment_rule(
				$selected_id,
				get_post('rule_name'),
				$rule_type,
				get_post('stock_id') ? get_post('stock_id') : null,
				get_post('category_id') ? get_post('category_id') : null,
				get_post('warehouse_loc_code') ? get_post('warehouse_loc_code') : null,
				get_post('bin_loc_id') ? get_post('bin_loc_id') : null,
				get_post('min_qty') !== '' ? get_post('min_qty') : null,
				get_post('max_qty') !== '' ? get_post('max_qty') : null,
				get_post('reorder_qty') !== '' ? get_post('reorder_qty') : null,
				get_post('safety_stock') !== '' ? get_post('safety_stock') : null,
				get_post('lead_time_days') !== '' ? get_post('lead_time_days') : null,
				get_post('supply_warehouse') ? get_post('supply_warehouse') : null,
				get_post('supply_method', 'purchase'),
				get_post('preferred_supplier') ? get_post('preferred_supplier') : null,
				check_value('auto_execute') ? 1 : 0,
				check_value('rule_active') ? 1 : 0
			);
			display_notification(_('Replenishment rule has been updated.'));
			$Ajax->activate('_page_body');
		}
		$Mode = 'RESET';
	}
	$active_tab = 'rules';
}

if ($Mode == 'RESET') {
	$selected_id = -1;
	unset($_POST['rule_name'], $_POST['rule_type'], $_POST['stock_id'],
		$_POST['category_id'], $_POST['warehouse_loc_code'], $_POST['bin_loc_id'],
		$_POST['min_qty'], $_POST['max_qty'], $_POST['reorder_qty'],
		$_POST['safety_stock'], $_POST['lead_time_days'], $_POST['supply_warehouse'],
		$_POST['supply_method'], $_POST['preferred_supplier'], $_POST['auto_execute'],
		$_POST['rule_active']);
}

// --- Toggle auto-execute from list ---
if (isset($_POST['toggle_auto'])) {
	$tid = (int)$_POST['toggle_auto'];
	$rule = get_replenishment_rule($tid);
	if ($rule) {
		toggle_replenishment_rule_auto_execute($tid, $rule['auto_execute'] ? 0 : 1);
		display_notification(_('Auto-execute toggled.'));
		$Ajax->activate('_page_body');
	}
	$active_tab = 'rules';
}

// --- Toggle active from list ---
if (isset($_POST['toggle_active'])) {
	$tid = (int)$_POST['toggle_active'];
	$rule = get_replenishment_rule($tid);
	if ($rule) {
		toggle_replenishment_rule_active($tid, $rule['active'] ? 0 : 1);
		display_notification(_('Rule active status toggled.'));
		$Ajax->activate('_page_body');
	}
	$active_tab = 'rules';
}

// --- Execute single suggestion ---
$exec_results = array();
if (isset($_POST['execute_suggestion'])) {
	$exec_idx = (int)$_POST['execute_suggestion'];
	// Re-evaluate to get the suggestion
	$filter_wh = get_post('filter_warehouse');
	$filter_type = get_post('filter_rule_type');
	$all_suggestions = evaluate_replenishment_rules(
		$filter_wh ? $filter_wh : null,
		$filter_type ? $filter_type : null
	);
	if (isset($all_suggestions[$exec_idx])) {
		$exec = execute_replenishment_suggestion($all_suggestions[$exec_idx]);
		if ($exec['success']) {
			display_notification($exec['message']);
		} else {
			display_error($exec['message']);
		}
	}
	$active_tab = 'suggestions';
	$Ajax->activate('_page_body');
}

// --- Execute all auto suggestions ---
if (isset($_POST['execute_all_auto'])) {
	$filter_wh = get_post('filter_warehouse');
	$filter_type = get_post('filter_rule_type');
	$all_suggestions = evaluate_replenishment_rules(
		$filter_wh ? $filter_wh : null,
		$filter_type ? $filter_type : null
	);
	$auto_results = execute_auto_replenishment($all_suggestions);

	if ($auto_results['executed'] > 0) {
		display_notification(sprintf(_('%d replenishment orders created successfully.'), $auto_results['executed']));
	}
	if ($auto_results['failed'] > 0) {
		display_error(sprintf(_('%d replenishment orders failed.'), $auto_results['failed']));
	}
	if ($auto_results['executed'] == 0 && $auto_results['failed'] == 0) {
		display_warning(_('No auto-execute suggestions found.'));
	}
	foreach ($auto_results['messages'] as $msg) {
		display_notification($msg);
	}
	$active_tab = 'suggestions';
	$Ajax->activate('_page_body');
}

// ===== RENDER PAGE =====
start_form();

// --- Tab navigation ---
echo '<div style="margin-bottom:15px; border-bottom:2px solid #dee2e6; padding-bottom:0;">';
echo '<button type="submit" name="tab_suggestions" value="1" class="ajaxsubmit" style="padding:8px 20px; border:1px solid #dee2e6; border-bottom:none; background:'
	. ($active_tab == 'suggestions' ? '#fff' : '#f8f9fa') . '; font-weight:'
	. ($active_tab == 'suggestions' ? 'bold' : 'normal') . '; cursor:pointer; border-radius:4px 4px 0 0; margin-right:4px; color:#71717b;'
	. ($active_tab == 'suggestions' ? ' margin-bottom:-2px; padding-bottom:10px;' : '')
	. '"><i class="fa fa-tachometer"></i> ' . _('Suggestions Dashboard') . '</button>';
echo '<button type="submit" name="tab_rules" value="1" class="ajaxsubmit" style="padding:8px 20px; border:1px solid #dee2e6; border-bottom:none; color:#71717b; background:'
	. ($active_tab == 'rules' ? '#fff' : '#f8f9fa') . '; font-weight:'
	. ($active_tab == 'rules' ? 'bold' : 'normal') . '; cursor:pointer; border-radius:4px 4px 0 0;'
	. ($active_tab == 'rules' ? ' margin-bottom:-2px; padding-bottom:10px;' : '')
	. '"><i class="fa fa-cogs"></i> ' . _('Replenishment Rules') . '</button>';
echo '</div>';

hidden('active_tab', $active_tab);

// =================================================================
// TAB: SUGGESTIONS DASHBOARD
// =================================================================
if ($active_tab == 'suggestions') {
	display_suggestions_tab();
}

// =================================================================
// TAB: RULES MANAGEMENT
// =================================================================
if ($active_tab == 'rules') {
	display_rules_tab($selected_id, $Mode);
}

end_form();
end_page();

// =================================================================
// FUNCTIONS: Suggestions Dashboard
// =================================================================

function display_suggestions_tab() {
	global $Ajax;

	div_start('suggestions_panel');

	// --- Filters ---
	start_table(TABLESTYLE_NOBORDER);
	start_row();

	// Warehouse filter
	// echo '<td>' . _('Warehouse:') . ' </td><td>';
	$sql = "SELECT loc_code, location_name FROM " . TB_PREF . "locations WHERE inactive=0 ORDER BY location_name";
	echo "<div class='form-row'><select name='filter_warehouse' class='combo ajaxsubmit'>";
	echo "<option value=''>" . _('-- All Warehouses --') . "</option>";
	$result = db_query($sql, 'could not get locations');
	while ($row = db_fetch($result)) {
		$sel = (get_post('filter_warehouse') == $row['loc_code']) ? ' selected' : '';
		echo "<option value='" . htmlspecialchars($row['loc_code'], ENT_QUOTES, 'UTF-8') . "'$sel>"
			. htmlspecialchars($row['location_name'], ENT_QUOTES, 'UTF-8') . "</option>";
	}
	echo "</select></div>";

	// Rule type filter
	echo "<div class='form-row'><select name='filter_rule_type' class='combo ajaxsubmit'>";
	echo "<option value=''>" . _('-- All Types --') . "</option>";
	foreach (get_replenishment_rule_types() as $key => $label) {
		$sel = (get_post('filter_rule_type') == $key) ? ' selected' : '';
		echo "<option value='" . htmlspecialchars($key, ENT_QUOTES, 'UTF-8') . "'$sel>"
			. htmlspecialchars($label, ENT_QUOTES, 'UTF-8') . "</option>";
	}
	echo "</select></div>";

	echo "<div class='form-row'>";
	submit('evaluate_btn', _('Evaluate Rules'), true, _('Run replenishment engine'), '');
	echo "</div><div class='form-row'>";
	submit('execute_all_auto', _('Execute All Auto'), true, _('Execute all auto-execute suggestions'), true);
	echo '</div>';
	end_row();
	end_table(1);

	// echo '<br>';

	// --- Rule Summary Cards ---
	$summary = get_replenishment_rule_summary(true);
	$total_rules = array_sum($summary);

	echo '<div style="display:flex; flex-wrap:wrap; gap:10px; margin-bottom:15px;">';

	echo '<div style="background:#f8f9fa; border:1px solid #dee2e6; border-radius:6px; padding:10px 15px; min-width:120px; text-align:center;">';
	echo '<div style="font-size:22px; font-weight:bold; color:#495057;">' . $total_rules . '</div>';
	echo '<div style="font-size:11px; color:#6c757d;">' . _('Active Rules') . '</div>';
	echo '</div>';

	$rule_types = get_replenishment_rule_types();
	foreach ($rule_types as $code => $label) {
		$cnt = isset($summary[$code]) ? $summary[$code] : 0;
		$color = get_replenishment_rule_type_color($code);
		echo '<div style="background:#f8f9fa; border:1px solid #dee2e6; border-left:3px solid ' . $color . '; border-radius:6px; padding:10px 15px; min-width:100px; text-align:center;">';
		echo '<div style="font-size:18px; font-weight:bold; color:' . $color . ';">' . $cnt . '</div>';
		echo '<div style="font-size:11px; color:#6c757d;">' . $label . '</div>';
		echo '</div>';
	}
	echo '</div>';

	// --- Evaluate and display suggestions ---
	$filter_wh = get_post('filter_warehouse');
	$filter_type = get_post('filter_rule_type');
	$suggestions = evaluate_replenishment_rules(
		$filter_wh ? $filter_wh : null,
		$filter_type ? $filter_type : null
	);

	if (empty($suggestions)) {
		display_note(_('No replenishment actions needed at this time. All stock levels are within configured thresholds.'));
	} else {
		// Count by priority
		$priority_counts = array('critical' => 0, 'high' => 0, 'medium' => 0, 'low' => 0);
		$auto_count = 0;
		foreach ($suggestions as $s) {
			if (isset($priority_counts[$s['priority']])) {
				$priority_counts[$s['priority']]++;
			}
			if ($s['auto_execute']) $auto_count++;
		}

		// Priority summary
		echo '<div style="display:flex; gap:10px; margin-bottom:15px; flex-wrap:wrap;">';
		echo '<div style="padding:6px 12px; border-radius:4px; background:#dc3545; color:#fff; font-size:12px;">'
			. '<i class="fa fa-exclamation-circle"></i> ' . $priority_counts['critical'] . ' ' . _('Critical') . '</div>';
		echo '<div style="padding:6px 12px; border-radius:4px; background:#fd7e14; color:#fff; font-size:12px;">'
			. '<i class="fa fa-exclamation-triangle"></i> ' . $priority_counts['high'] . ' ' . _('High') . '</div>';
		echo '<div style="padding:6px 12px; border-radius:4px; background:#ffc107; color:#212529; font-size:12px;">'
			. '<i class="fa fa-info-circle"></i> ' . $priority_counts['medium'] . ' ' . _('Medium') . '</div>';
		echo '<div style="padding:6px 12px; border-radius:4px; background:#28a745; color:#fff; font-size:12px;">'
			. '<i class="fa fa-check-circle"></i> ' . $priority_counts['low'] . ' ' . _('Low') . '</div>';
		echo '<div style="padding:6px 12px; border-radius:4px; background:#6c757d; color:#fff; font-size:12px;">'
			. '<i class="fa fa-bolt"></i> ' . $auto_count . ' ' . _('Auto-Execute') . '</div>';
		echo '</div>';

		// Suggestions table
		start_table(TABLESTYLE, "width='100%'");
		$th = array(
			_('Priority'), _('Item'), _('Warehouse'), _('Rule'),
			_('On Hand'), _('Incoming'), _('Outgoing'), _('Suggested Qty'),
			_('Supply'), _('Auto'), _('Reason'), _('Action')
		);
		table_header($th);

		$k = 0;
		foreach ($suggestions as $idx => $s) {
			alt_table_row_color($k);

			// Priority badge
			$pcolor = get_priority_color($s['priority']);
			$picon = get_priority_icon($s['priority']);
			echo '<td style="text-align:center;"><span style="display:inline-block; padding:2px 8px; border-radius:3px; background:' . $pcolor . '; color:#fff; font-size:11px;">'
				. '<i class="fa ' . $picon . '"></i> ' . ucfirst($s['priority']) . '</span></td>';

			// Item
			echo '<td><b>' . htmlspecialchars($s['stock_id'], ENT_QUOTES, 'UTF-8') . '</b>';
			if ($s['item_description'])
				echo '<br><small style="color:#6c757d;">' . htmlspecialchars($s['item_description'], ENT_QUOTES, 'UTF-8') . '</small>';
			echo '</td>';

			// Warehouse
			echo '<td>' . htmlspecialchars($s['warehouse_name'], ENT_QUOTES, 'UTF-8') . '</td>';

			// Rule
			$rtcolor = get_replenishment_rule_type_color($s['rule_type']);
			echo '<td><span style="display:inline-block; padding:1px 6px; border-radius:3px; background:' . $rtcolor . '22; color:' . $rtcolor . '; font-size:11px;">'
				. get_replenishment_rule_type_label($s['rule_type']) . '</span>';
			echo '<br><small>' . htmlspecialchars($s['rule_name'], ENT_QUOTES, 'UTF-8') . '</small></td>';

			// Quantities
			echo '<td style="text-align:right;">' . number_format2($s['current_qty'], 1) . '</td>';
			echo '<td style="text-align:right;">' . number_format2($s['incoming_qty'], 1) . '</td>';
			echo '<td style="text-align:right;">' . number_format2($s['outgoing_qty'], 1) . '</td>';

			// Suggested qty (highlighted)
			echo '<td style="text-align:right; font-weight:bold; color:' . $pcolor . ';">' . number_format2($s['suggested_qty'], 1) . '</td>';

			// Supply method
			echo '<td>' . get_replenishment_supply_method_label($s['supply_method']);
			if ($s['supply_warehouse'])
				echo '<br><small>' . _('from') . ' ' . htmlspecialchars($s['supply_warehouse'], ENT_QUOTES, 'UTF-8') . '</small>';
			if ($s['bin_code'])
				echo '<br><small>' . _('bin') . ' ' . htmlspecialchars($s['bin_code'], ENT_QUOTES, 'UTF-8') . '</small>';
			echo '</td>';

			// Auto flag
			echo '<td style="text-align:center;">';
			if ($s['auto_execute'])
				echo '<span style="color:#28a745;"><i class="fa fa-bolt"></i> ' . _('Auto') . '</span>';
			else
				echo '<span style="color:#6c757d;">' . _('Manual') . '</span>';
			echo '</td>';

			// Reason
			echo '<td><small>' . htmlspecialchars($s['reason'], ENT_QUOTES, 'UTF-8') . '</small></td>';

			// Execute button
			echo '<td>';
			echo '<button type="submit" name="execute_suggestion" value="' . $idx . '" class="ajaxsubmit" style="padding:3px 10px; background:#007bff; color:#fff; border:none; border-radius:3px; cursor:pointer; font-size:11px;">'
				. '<i class="fa fa-play"></i> ' . _('Execute') . '</button>';
			echo '</td>';

			end_row();
		}
		end_table();
	}

	div_end();
}

// =================================================================
// FUNCTIONS: Rules Management
// =================================================================

function display_rules_tab($selected_id, $Mode) {
	global $Ajax;

	// --- Show inactive toggle ---
	$show_inactive = check_value('show_inactive_rules');

	div_start('rules_panel');

	// --- Filter row ---
	start_table(TABLESTYLE_NOBORDER);
	start_row();
	
	check_cells(_('Show Inactive:'), 'show_inactive_rules', $show_inactive, true);
	
	end_row();
	end_table(1);

	// --- Rules list ---
	$rules = get_replenishment_rules(null, null, null, $show_inactive);

	start_table(TABLESTYLE, "width='100%'");
	$th = array(
		_('Rule Name'), _('Type'), _('Item / Category'), _('Warehouse'),
		_('Min'), _('Max'), _('Reorder'), _('Safety'),
		_('Supply'), _('Auto'), _('Active'), '', ''
	);
	table_header($th);

	$k = 0;
	while ($rule = db_fetch($rules)) {
		alt_table_row_color($k);

		// Rule name
		echo '<td><b>' . htmlspecialchars($rule['rule_name'], ENT_QUOTES, 'UTF-8') . '</b></td>';

		// Type badge
		$rtcolor = get_replenishment_rule_type_color($rule['rule_type']);
		echo '<td><span style="display:inline-block; padding:2px 8px; border-radius:3px; background:' . $rtcolor . '22; color:' . $rtcolor . '; font-size:11px;">'
			. get_replenishment_rule_type_label($rule['rule_type']) . '</span></td>';

		// Item / Category
		echo '<td>';
		if ($rule['stock_id']) {
			echo htmlspecialchars($rule['stock_id'], ENT_QUOTES, 'UTF-8');
			if ($rule['item_description'])
				echo '<br><small style="color:#6c757d;">' . htmlspecialchars($rule['item_description'], ENT_QUOTES, 'UTF-8') . '</small>';
		} elseif ($rule['category_name']) {
			echo '<i class="fa fa-folder-o"></i> ' . htmlspecialchars($rule['category_name'], ENT_QUOTES, 'UTF-8');
		} else {
			echo '<i style="color:#6c757d;">' . _('All Items') . '</i>';
		}
		echo '</td>';

		// Warehouse
		echo '<td>' . ($rule['warehouse_name'] ? htmlspecialchars($rule['warehouse_name'], ENT_QUOTES, 'UTF-8') : '-') . '</td>';

		// Quantities
		echo '<td style="text-align:right;">' . ($rule['min_qty'] !== null ? number_format2($rule['min_qty'], 1) : '-') . '</td>';
		echo '<td style="text-align:right;">' . ($rule['max_qty'] !== null ? number_format2($rule['max_qty'], 1) : '-') . '</td>';
		echo '<td style="text-align:right;">' . ($rule['reorder_qty'] !== null ? number_format2($rule['reorder_qty'], 1) : '-') . '</td>';
		echo '<td style="text-align:right;">' . ($rule['safety_stock'] !== null ? number_format2($rule['safety_stock'], 1) : '-') . '</td>';

		// Supply
		echo '<td>' . get_replenishment_supply_method_label($rule['supply_method']);
		if ($rule['supply_warehouse_name'])
			echo '<br><small>' . _('from') . ' ' . htmlspecialchars($rule['supply_warehouse_name'], ENT_QUOTES, 'UTF-8') . '</small>';
		echo '</td>';

		// Auto-execute toggle
		echo '<td style="text-align:center;">';
		echo '<button type="submit" name="toggle_auto" value="' . $rule['rule_id'] . '" class="ajaxsubmit" style="background:none; border:none; cursor:pointer; font-size:14px; color:'
			. ($rule['auto_execute'] ? '#28a745' : '#6c757d') . ';" title="' . _('Toggle auto-execute') . '">';
		echo '<i class="fa ' . ($rule['auto_execute'] ? 'fa-bolt' : 'fa-hand-paper-o') . '"></i>';
		echo '</button></td>';

		// Active toggle
		echo '<td style="text-align:center;">';
		echo '<button type="submit" name="toggle_active" value="' . $rule['rule_id'] . '" class="ajaxsubmit" style="background:none; border:none; cursor:pointer; font-size:14px; color:'
			. ($rule['active'] ? '#28a745' : '#dc3545') . ';" title="' . _('Toggle active') . '">';
		echo '<i class="fa ' . ($rule['active'] ? 'fa-check-circle' : 'fa-times-circle') . '"></i>';
		echo '</button></td>';

		// Edit / Delete
		edit_button_cell('Edit' . $rule['rule_id'], _('Edit'));
		delete_button_cell('Delete' . $rule['rule_id'], _('Delete'));

		end_row();
	}
	end_table();

	// --- Add/Edit Form ---
	echo '<br>';
	display_rule_form($selected_id, $Mode);

	div_end();
}

/**
 * Display the replenishment rule add/edit form.
 *
 * @param int    $selected_id Currently selected rule ID (-1 for new)
 * @param string $Mode        Current page mode
 */
function display_rule_form($selected_id, $Mode) {
	// Load data for edit mode
	$editing = ($selected_id > 0 && $Mode == 'Edit');
	$rule = null;
	if ($editing) {
		$rule = get_replenishment_rule($selected_id);
		if ($rule) {
			$_POST['rule_name'] = $rule['rule_name'];
			$_POST['rule_type'] = $rule['rule_type'];
			$_POST['stock_id'] = $rule['stock_id'];
			$_POST['category_id'] = $rule['category_id'];
			$_POST['warehouse_loc_code'] = $rule['warehouse_loc_code'];
			$_POST['bin_loc_id'] = $rule['bin_loc_id'];
			$_POST['min_qty'] = $rule['min_qty'];
			$_POST['max_qty'] = $rule['max_qty'];
			$_POST['reorder_qty'] = $rule['reorder_qty'];
			$_POST['safety_stock'] = $rule['safety_stock'];
			$_POST['lead_time_days'] = $rule['lead_time_days'];
			$_POST['supply_warehouse'] = $rule['supply_warehouse'];
			$_POST['supply_method'] = $rule['supply_method'];
			$_POST['preferred_supplier'] = $rule['preferred_supplier'];
			$_POST['auto_execute'] = $rule['auto_execute'];
			$_POST['rule_active'] = $rule['active'];
		}
	}

	start_outer_table(TABLESTYLE2);

	table_section(1);
	table_section_title(_('Replenishment Rule'));

	text_row(_('Rule Name:'), 'rule_name', get_post('rule_name', ''), 40, 100);

	// Rule type selector with submit for conditional field display
	$types = get_replenishment_rule_types();
	array_selector_row(_('Rule Type:'), 'rule_type', get_post('rule_type', 'min_max'), $types,
		array('select_submit' => true));

	$current_type = get_post('rule_type', 'min_max');

	// Item selector
	echo "<tr><td class='label'>" . _('Item (specific):') . "</td>";
	stock_items_list_cells(null, 'stock_id', get_post('stock_id'), _('-- All Items --'));
	echo "</tr>\n";

	// Category selector
	stock_categories_list_row(_('Or Category (all items):'), 'category_id', get_post('category_id'),
		_('-- None --'));

	// Warehouse
	locations_list_row(_('Warehouse:'), 'warehouse_loc_code', get_post('warehouse_loc_code'));

	table_section(2);
	table_section_title(_('Quantities & Thresholds'));

	// Conditional fields based on rule type
	if (in_array($current_type, array('min_max', 'inter_warehouse', 'pick_face'))) {
		small_amount_row(_('Min Quantity:'), 'min_qty', get_post('min_qty', ''), null, null, 1);
		small_amount_row(_('Max Quantity:'), 'max_qty', get_post('max_qty', ''), null, null, 1);
	}

	if ($current_type == 'reorder_point') {
		small_amount_row(_('Reorder Quantity:'), 'reorder_qty', get_post('reorder_qty', ''), null, null, 1);
	}

	small_amount_row(_('Safety Stock:'), 'safety_stock', get_post('safety_stock', ''), null, null, 1);
	text_row(_('Lead Time (days):'), 'lead_time_days', get_post('lead_time_days', ''), 10, 10);

	table_section_title(_('Supply Configuration'));

	// Supply method
	$methods = get_replenishment_supply_methods();
	array_selector_row(_('Supply Method:'), 'supply_method', get_post('supply_method', 'purchase'), $methods);

	// Supply warehouse (for inter-warehouse or transfer supply)
	if ($current_type == 'inter_warehouse' || get_post('supply_method') == 'transfer') {
		locations_list_row(_('Supply From Warehouse:'), 'supply_warehouse', get_post('supply_warehouse'));
	}

	// Bin for pick-face
	if ($current_type == 'pick_face') {
		text_row(_('Pick-Face Bin ID:'), 'bin_loc_id', get_post('bin_loc_id', ''), 10, 10);
	}

	// Preferred supplier (for purchase supply)
	if (get_post('supply_method', 'purchase') == 'purchase') {
		supplier_list_row(_('Preferred Supplier:'), 'preferred_supplier', get_post('preferred_supplier'),
			false, false, true);
	}

	// Toggles
	check_row(_('Auto-Execute:'), 'auto_execute', get_post('auto_execute', 0));

	if ($editing) {
		check_row(_('Active:'), 'rule_active', get_post('rule_active', 1));
	}

	end_outer_table(1);

	submit_add_or_update_center($selected_id == -1, '', 'both');
}
