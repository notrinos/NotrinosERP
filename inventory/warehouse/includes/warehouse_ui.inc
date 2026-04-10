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
 * Reusable UI components for warehouse location management.
 *
 * Provides location selectors, tree rendering, breadcrumb display,
 * and location type selectors for use across all WMS forms.
 *
 * @package NotrinosERP
 * @subpackage Inventory/Warehouse
 */

/**
 * Dropdown selector for WMS-enabled locations (warehouses).
 *
 * @param string      $name             Input name.
 * @param string|null $selected_id      Currently selected loc_code.
 * @param bool        $all_option       Whether to show "All Warehouses" option.
 * @param bool        $submit_on_change Whether to auto-submit on change.
 * @return string HTML.
 */
function warehouse_list($name, $selected_id = null, $all_option = false, $submit_on_change = false) {
	$sql = "SELECT loc_code, location_name, inactive FROM " . TB_PREF . "locations WHERE wh_enabled = 1";

	return combo_input($name, $selected_id, $sql, 'loc_code', 'location_name',
		array(
			'spec_option'  => $all_option === true ? _('All Warehouses') : $all_option,
			'spec_id'      => ALL_TEXT,
			'select_submit' => $submit_on_change
		));
}

/**
 * Warehouse list with table cell wrapping.
 *
 * @param string      $label       Label text.
 * @param string      $name        Input name.
 * @param string|null $selected_id Currently selected.
 * @param bool        $all_option  Show "All" option.
 * @param bool        $submit_on_change Auto-submit.
 * @return void
 */
function warehouse_list_cells($label, $name, $selected_id = null, $all_option = false, $submit_on_change = false) {
	if ($label) echo "<td class='label'>$label</td>\n";
	echo "<td>";
	echo warehouse_list($name, $selected_id, $all_option, $submit_on_change);
	echo "</td>\n";
}

/**
 * Warehouse list as form row.
 *
 * @param string      $label       Label text.
 * @param string      $name        Input name.
 * @param string|null $selected_id Currently selected.
 * @param bool        $all_option  Show "All" option.
 * @param bool        $submit_on_change Auto-submit.
 * @return void
 */
function warehouse_list_row($label, $name, $selected_id = null, $all_option = false, $submit_on_change = false) {
	echo "<tr><td class='label'>$label</td>";
	warehouse_list_cells(null, $name, $selected_id, $all_option, $submit_on_change);
	echo "</tr>\n";
}

/**
 * Dropdown selector for warehouse location types.
 *
 * @param string      $name             Input name.
 * @param string|null $selected_id      Currently selected type ID.
 * @param bool        $submit_on_change Auto-submit.
 * @return string HTML.
 */
function warehouse_location_type_list($name, $selected_id = null, $submit_on_change = false) {
	$sql = "SELECT id, type_name, inactive FROM " . TB_PREF . "wh_location_types WHERE inactive = 0 ORDER BY sort_order";

	return combo_input($name, $selected_id, $sql, 'id', 'type_name',
		array(
			'select_submit' => $submit_on_change,
			'order' => false,
			'async' => false
		));
}

/**
 * Location type selector as form row.
 *
 * @param string      $label       Label text.
 * @param string      $name        Input name.
 * @param string|null $selected_id Currently selected.
 * @param bool        $submit_on_change Auto-submit.
 * @return void
 */
function warehouse_location_type_list_row($label, $name, $selected_id = null, $submit_on_change = false) {
	echo "<tr><td class='label'>$label</td><td>";
	echo warehouse_location_type_list($name, $selected_id, $submit_on_change);
	echo "</td></tr>\n";
}

/**
 * Dropdown selector for storable bin locations within a warehouse.
 *
 * @param string      $name               Input name.
 * @param string      $warehouse_loc_code  Warehouse code to filter by.
 * @param string|null $selected_id         Currently selected loc_id.
 * @param bool        $spec_option         Show a special "select" option.
 * @param bool        $submit_on_change    Auto-submit.
 * @return string HTML.
 */
function warehouse_bin_list($name, $warehouse_loc_code, $selected_id = null, $spec_option = false, $submit_on_change = false) {
	$sql = "SELECT wl.loc_id, CONCAT(wl.loc_code, ' - ', wl.loc_name) AS display_name, 0 AS inactive
			FROM " . TB_PREF . "wh_locations wl
			INNER JOIN " . TB_PREF . "wh_location_types lt ON wl.location_type_id = lt.id
			WHERE wl.warehouse_loc_code = " . db_escape($warehouse_loc_code)
			. " AND lt.can_store = 1 AND wl.is_active = 1
			ORDER BY wl.location_path";

	return combo_input($name, $selected_id, $sql, 'loc_id', 'display_name',
		array(
			'spec_option'  => $spec_option === true ? _('-- select --') : $spec_option,
			'spec_id'      => '',
			'select_submit' => $submit_on_change,
			'order' => false,
			'async' => false
		));
}

/**
 * Bin list as form row.
 *
 * @param string      $label              Label text.
 * @param string      $name               Input name.
 * @param string      $warehouse_loc_code Warehouse code.
 * @param string|null $selected_id        Currently selected.
 * @param bool        $spec_option        Show special option.
 * @param bool        $submit_on_change   Auto-submit.
 * @return void
 */
function warehouse_bin_list_row($label, $name, $warehouse_loc_code, $selected_id = null, $spec_option = false, $submit_on_change = false) {
	echo "<tr><td class='label'>$label</td><td>";
	echo warehouse_bin_list($name, $warehouse_loc_code, $selected_id, $spec_option, $submit_on_change);
	echo "</td></tr>\n";
}

/**
 * Dropdown selector for storage categories.
 *
 * @param string      $name             Input name.
 * @param string|null $selected_id      Currently selected ID.
 * @param bool        $spec_option      Show special "none" option.
 * @param bool        $submit_on_change Auto-submit.
 * @return string HTML.
 */
function storage_category_list($name, $selected_id = null, $spec_option = false, $submit_on_change = false) {
	$sql = "SELECT id, category_name, inactive FROM " . TB_PREF . "wh_storage_categories";

	return combo_input($name, $selected_id, $sql, 'id', 'category_name',
		array(
			'spec_option'  => $spec_option === true ? _('-- none --') : $spec_option,
			'spec_id'      => '',
			'select_submit' => $submit_on_change,
			'async' => false
		));
}

/**
 * Storage category selector as form row.
 *
 * @param string      $label       Label text.
 * @param string      $name        Input name.
 * @param string|null $selected_id Currently selected.
 * @param bool        $spec_option Show special option.
 * @param bool        $submit_on_change Auto-submit.
 * @return void
 */
function storage_category_list_row($label, $name, $selected_id = null, $spec_option = false, $submit_on_change = false) {
	echo "<tr><td class='label'>$label</td><td>";
	echo storage_category_list($name, $selected_id, $spec_option, $submit_on_change);
	echo "</td></tr>\n";
}

/**
 * Render a breadcrumb navigation for a warehouse location.
 *
 * @param array  $breadcrumb Array of location rows from get_warehouse_location_breadcrumb().
 * @param string $base_url   The base URL for breadcrumb links.
 * @return void Outputs HTML breadcrumb directly.
 */
function display_warehouse_breadcrumb($breadcrumb, $base_url) {
	echo "<div class='wh-breadcrumb' style='margin:5px 0; padding:5px 10px; background:#f5f5f5; border-radius:3px; font-size:13px;'>";
	echo "<i class='fa fa-warehouse' style='margin-right:5px;'></i> ";
	$links = array();
	foreach ($breadcrumb as $i => $loc) {
		if ($i < count($breadcrumb) - 1) {
			$links[] = "<a href='" . $base_url . "parent_id=" . $loc['loc_id'] . "'>" . htmlspecialchars($loc['loc_name']) . "</a>";
		} else {
			$links[] = "<strong>" . htmlspecialchars($loc['loc_name']) . "</strong>";
		}
	}
	echo implode(" <span style='color:#999;'>&gt;</span> ", $links);
	echo "</div>\n";
}

/**
 * Render a hierarchical tree display of warehouse locations.
 *
 * @param array  $tree      Nested tree from build_warehouse_location_tree().
 * @param string $base_url  Base URL for navigating into locations.
 * @param int    $depth     Current depth level (for indentation).
 * @return void Outputs HTML directly.
 */
function display_warehouse_location_tree($tree, $base_url, $depth = 0) {
	foreach ($tree as $node) {
		$indent = $depth * 20;
		$has_children = !empty($node['children']);
		$icon = get_location_type_icon($node['type_code']);
		$status_class = $node['is_active'] ? '' : ' text-muted';
		$inactive_label = $node['is_active'] ? '' : ' <span class="badge badge-secondary">' . _('Inactive') . '</span>';

		echo "<tr class='location-tree-row" . $status_class . "'>";
		echo "<td style='padding-left:" . ($indent + 5) . "px;'>";
		if ($has_children) {
			echo "<a href='" . $base_url . "parent_id=" . $node['loc_id'] . "&warehouse=" . urlencode($node['warehouse_loc_code']) . "'>";
			echo "<i class='" . $icon . "' style='margin-right:4px;'></i>";
			echo htmlspecialchars($node['loc_name']);
			echo "</a>";
		} else {
			echo "<i class='" . $icon . "' style='margin-right:4px;'></i>";
			echo htmlspecialchars($node['loc_name']);
		}
		echo $inactive_label;
		echo "</td>";
		echo "<td>" . htmlspecialchars($node['loc_code']) . "</td>";
		echo "<td>" . htmlspecialchars($node['type_name']) . "</td>";
		echo "<td class='center'>" . ($node['can_store'] ? _('Yes') : _('No')) . "</td>";
		echo "<td class='right'>" . ($node['max_weight'] ? number_format2($node['max_weight'], 2) : '-') . "</td>";
		echo "<td class='right'>" . ($node['max_volume'] ? number_format2($node['max_volume'], 4) : '-') . "</td>";
		echo "<td class='center'>" . ($node['child_count'] ?: '0') . "</td>";

		echo "<td class='center'>";
		edit_button_cell("Edit" . $node['loc_id'], _('Edit'));
		delete_button_cell("Delete" . $node['loc_id'], _('Delete'));
		echo "</td>";

		echo "</tr>\n";

		if ($has_children) {
			display_warehouse_location_tree($node['children'], $base_url, $depth + 1);
		}
	}
}

/**
 * Get a Font Awesome icon class for a location type.
 *
 * @param string $type_code The location type code.
 * @return string CSS class for the icon.
 */
function get_location_type_icon($type_code) {
	$icons = array(
		'warehouse'  => 'fa fa-building',
		'zone'       => 'fa fa-th-large',
		'aisle'      => 'fa fa-road',
		'rack'       => 'fa fa-server',
		'shelf'      => 'fa fa-minus',
		'bin'        => 'fa fa-cube',
		'staging'    => 'fa fa-object-group',
		'dock'       => 'fa fa-truck',
		'quality'    => 'fa fa-check-circle',
		'transit'    => 'fa fa-exchange',
		'virtual'    => 'fa fa-cloud',
		'scrap'      => 'fa fa-trash',
		'production' => 'fa fa-industry',
		'packing'    => 'fa fa-gift',
		'returns'    => 'fa fa-undo',
	);
	return isset($icons[$type_code]) ? $icons[$type_code] : 'fa fa-map-marker';
}

/**
 * Display location summary cards (count by type).
 *
 * @param string $warehouse_loc_code The warehouse code.
 * @return void Outputs HTML summary cards.
 */
function display_warehouse_location_summary($warehouse_loc_code) {
	$counts = get_warehouse_location_type_counts($warehouse_loc_code);

	if (empty($counts)) {
		echo "<div style='padding:10px; color:#999; text-align:center;'>";
		echo _('No locations defined yet. Use the form below to add locations.');
		echo "</div>\n";
		return;
	}

	echo "<div style='display:flex; flex-wrap:wrap; gap:10px; margin:10px 0;'>";
	foreach ($counts as $type_code => $info) {
		$icon = get_location_type_icon($type_code);
		echo "<div style='background:#fff; border:1px solid #ddd; border-radius:4px; padding:8px 15px; min-width:100px; text-align:center;'>";
		echo "<div style='font-size:20px; color:#555;'><i class='" . $icon . "'></i></div>";
		echo "<div style='font-size:18px; font-weight:bold;'>" . $info['count'] . "</div>";
		echo "<div style='font-size:11px; color:#777;'>" . htmlspecialchars($info['name']) . "</div>";
		echo "</div>";
	}
	echo "</div>\n";
}

//----------------------------------------------------------------------
// Capacity / Utilization UI components (Session 5)
//----------------------------------------------------------------------

/**
 * Get CSS color for a utilization status.
 *
 * @param string $status One of: ok, warning, critical, full.
 * @return string CSS color code.
 */
function get_utilization_color($status) {
	$colors = array(
		'ok'       => '#28a745',
		'warning'  => '#ffc107',
		'critical' => '#fd7e14',
		'full'     => '#dc3545',
	);
	return isset($colors[$status]) ? $colors[$status] : '#6c757d';
}

/**
 * Render a utilization progress bar.
 *
 * @param float  $percentage  Utilization percentage (0-100+).
 * @param string $status      Status string (ok/warning/critical/full).
 * @param string $label       Optional label text inside the bar.
 * @param int    $height      Bar height in pixels (default 20).
 * @return string HTML string.
 */
function utilization_bar($percentage, $status = 'ok', $label = '', $height = 20) {
	$color = get_utilization_color($status);
	$pct = min($percentage, 100); // visual capping at 100%
	$text_color = ($pct > 50) ? '#fff' : '#333';
	if (!$label) $label = round($percentage, 1) . '%';

	$html = "<div style='background:#e9ecef; border-radius:3px; overflow:hidden; height:{$height}px; position:relative; min-width:60px;'>";
	$html .= "<div style='background:{$color}; height:100%; width:{$pct}%; transition:width 0.3s; border-radius:3px;'></div>";
	$html .= "<span style='position:absolute; top:0; left:0; right:0; text-align:center; line-height:{$height}px; font-size:11px; color:{$text_color}; font-weight:bold;'>{$label}</span>";
	$html .= "</div>";
	return $html;
}

/**
 * Render a compact utilization badge (colored circle + percentage text).
 *
 * @param float  $percentage Utilization percentage.
 * @param string $status     Status string.
 * @return string HTML string.
 */
function utilization_badge($percentage, $status = 'ok') {
	$color = get_utilization_color($status);
	return "<span style='display:inline-flex; align-items:center; gap:4px;'>"
		. "<span style='display:inline-block; width:10px; height:10px; border-radius:50%; background:{$color};'></span>"
		. "<span style='font-weight:bold;'>" . round($percentage, 1) . "%</span>"
		. "</span>";
}

/**
 * Render a bin utilization detail panel for a single bin.
 *
 * Shows weight, volume, and unit utilization bars.
 *
 * @param array $utilization Result from get_bin_utilization().
 * @return void Outputs HTML directly.
 */
function display_bin_utilization_detail($utilization) {
	echo "<div style='padding:8px;'>";

	// Weight
	echo "<div style='margin-bottom:6px;'>";
	echo "<div style='display:flex; justify-content:space-between; font-size:12px; margin-bottom:2px;'>";
	echo "<span><i class='fa fa-balance-scale' style='margin-right:3px;'></i>" . _('Weight') . "</span>";
	if ($utilization['max_weight'] !== null) {
		echo "<span>" . number_format2($utilization['current_weight'], 2) . " / " . number_format2($utilization['max_weight'], 2) . " kg</span>";
	} else {
		echo "<span style='color:#999;'>" . _('No limit') . "</span>";
	}
	echo "</div>";
	if ($utilization['max_weight'] !== null) {
		echo utilization_bar($utilization['weight_pct'], $utilization['status'], '', 16);
	}
	echo "</div>";

	// Volume
	echo "<div style='margin-bottom:6px;'>";
	echo "<div style='display:flex; justify-content:space-between; font-size:12px; margin-bottom:2px;'>";
	echo "<span><i class='fa fa-cube' style='margin-right:3px;'></i>" . _('Volume') . "</span>";
	if ($utilization['max_volume'] !== null) {
		echo "<span>" . number_format2($utilization['current_volume'], 4) . " / " . number_format2($utilization['max_volume'], 4) . " m³</span>";
	} else {
		echo "<span style='color:#999;'>" . _('No limit') . "</span>";
	}
	echo "</div>";
	if ($utilization['max_volume'] !== null) {
		echo utilization_bar($utilization['volume_pct'], $utilization['status'], '', 16);
	}
	echo "</div>";

	// Units
	echo "<div>";
	echo "<div style='display:flex; justify-content:space-between; font-size:12px; margin-bottom:2px;'>";
	echo "<span><i class='fa fa-cubes' style='margin-right:3px;'></i>" . _('Units') . "</span>";
	if ($utilization['max_units'] !== null) {
		echo "<span>" . $utilization['current_units'] . " / " . $utilization['max_units'] . "</span>";
	} else {
		echo "<span style='color:#999;'>" . _('No limit') . "</span>";
	}
	echo "</div>";
	if ($utilization['max_units'] !== null) {
		echo utilization_bar($utilization['units_pct'], $utilization['status'], '', 16);
	}
	echo "</div>";

	echo "</div>";
}

/**
 * Render a warehouse utilization dashboard with zone-level capacity cards.
 *
 * @param string $warehouse_loc_code The warehouse code.
 * @return void Outputs HTML directly.
 */
function display_warehouse_utilization_dashboard($warehouse_loc_code) {
	$zones = get_warehouse_utilization_summary($warehouse_loc_code);

	if (empty($zones)) {
		echo "<div style='padding:15px; color:#999; text-align:center;'>";
		echo _('No zones defined in this warehouse.');
		echo "</div>\n";
		return;
	}

	echo "<div style='display:flex; flex-wrap:wrap; gap:12px; margin:10px 0;'>";

	foreach ($zones as $zone) {
		$util = $zone['utilization'];
		$color = get_utilization_color($util['status']);
		$icon = get_location_type_icon($zone['type_code']);

		echo "<div style='background:#fff; border:1px solid #ddd; border-left:4px solid {$color}; border-radius:4px; padding:12px; min-width:200px; max-width:280px; flex:1;'>";

		// Header
		echo "<div style='display:flex; justify-content:space-between; align-items:center; margin-bottom:8px;'>";
		echo "<div><i class='{$icon}' style='margin-right:4px;'></i><strong>" . htmlspecialchars($zone['loc_name']) . "</strong></div>";
		echo utilization_badge($util['overall_pct'], $util['status']);
		echo "</div>";

		// Overall bar
		echo utilization_bar($util['overall_pct'], $util['status']);

		// Stats
		echo "<div style='display:flex; justify-content:space-between; margin-top:8px; font-size:11px; color:#666;'>";
		echo "<span>" . sprintf(_('Bins: %d'), $util['total_bins']) . "</span>";
		echo "<span>" . sprintf(_('Full: %d'), $util['bins_full']) . "</span>";
		echo "<span>" . sprintf(_('Empty: %d'), $util['bins_empty']) . "</span>";
		echo "</div>";

		echo "</div>";
	}

	echo "</div>\n";
}

/**
 * Get CSS color for an ABC classification class.
 *
 * @param string $class 'A', 'B', or 'C'.
 * @return string CSS color.
 */
function get_abc_class_color($class) {
	$colors = array(
		'A' => '#dc3545', // red — high value
		'B' => '#ffc107', // yellow — medium value
		'C' => '#28a745', // green — low value
	);
	return isset($colors[$class]) ? $colors[$class] : '#6c757d';
}

/**
 * Render a colored ABC class badge.
 *
 * @param string $class 'A', 'B', or 'C'.
 * @return string HTML.
 */
function abc_class_badge($class) {
	if (!$class) return '<span style="color:#999;">-</span>';
	$color = get_abc_class_color($class);
	return "<span style='display:inline-block; padding:2px 8px; border-radius:3px; "
		. "background:{$color}; color:#fff; font-weight:bold; font-size:12px;'>{$class}</span>";
}

/**
 * Render ABC classification summary cards.
 *
 * @param array $summary Result from get_abc_classification_summary().
 * @return void Outputs HTML directly.
 */
function display_abc_summary_cards($summary) {
	$total = $summary['A'] + $summary['B'] + $summary['C'] + $summary['unclassified'];

	echo "<div style='display:flex; flex-wrap:wrap; gap:10px; margin:10px 0;'>";

	// Class A
	$a_pct = ($total > 0) ? round(($summary['A'] / $total) * 100, 1) : 0;
	echo "<div style='background:#fff; border:1px solid #ddd; border-left:4px solid " . get_abc_class_color('A') . "; border-radius:4px; padding:10px 20px; min-width:120px; text-align:center;'>";
	echo "<div style='font-size:24px; font-weight:bold; color:" . get_abc_class_color('A') . ";'>" . $summary['A'] . "</div>";
	echo "<div style='font-size:12px; color:#555;'>" . _('Class A') . " ({$a_pct}%)</div>";
	echo "<div style='font-size:11px; color:#999;'>" . _('High Value') . "</div>";
	echo "</div>";

	// Class B
	$b_pct = ($total > 0) ? round(($summary['B'] / $total) * 100, 1) : 0;
	echo "<div style='background:#fff; border:1px solid #ddd; border-left:4px solid " . get_abc_class_color('B') . "; border-radius:4px; padding:10px 20px; min-width:120px; text-align:center;'>";
	echo "<div style='font-size:24px; font-weight:bold; color:" . get_abc_class_color('B') . ";'>" . $summary['B'] . "</div>";
	echo "<div style='font-size:12px; color:#555;'>" . _('Class B') . " ({$b_pct}%)</div>";
	echo "<div style='font-size:11px; color:#999;'>" . _('Medium Value') . "</div>";
	echo "</div>";

	// Class C
	$c_pct = ($total > 0) ? round(($summary['C'] / $total) * 100, 1) : 0;
	echo "<div style='background:#fff; border:1px solid #ddd; border-left:4px solid " . get_abc_class_color('C') . "; border-radius:4px; padding:10px 20px; min-width:120px; text-align:center;'>";
	echo "<div style='font-size:24px; font-weight:bold; color:" . get_abc_class_color('C') . ";'>" . $summary['C'] . "</div>";
	echo "<div style='font-size:12px; color:#555;'>" . _('Class C') . " ({$c_pct}%)</div>";
	echo "<div style='font-size:11px; color:#999;'>" . _('Low Value') . "</div>";
	echo "</div>";

	// Unclassified
	if ($summary['unclassified'] > 0) {
		echo "<div style='background:#fff; border:1px solid #ddd; border-left:4px solid #6c757d; border-radius:4px; padding:10px 20px; min-width:120px; text-align:center;'>";
		echo "<div style='font-size:24px; font-weight:bold; color:#6c757d;'>" . $summary['unclassified'] . "</div>";
		echo "<div style='font-size:12px; color:#555;'>" . _('Unclassified') . "</div>";
		echo "<div style='font-size:11px; color:#999;'>" . _('Not yet analyzed') . "</div>";
		echo "</div>";
	}

	echo "</div>\n";
}
