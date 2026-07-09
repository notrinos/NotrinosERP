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
 * Warehouse Locations — hierarchical location management page.
 *
 * Manages the location tree: Warehouse → Zone → Aisle → Rack → Shelf → Bin.
 * Allows creating, editing, deleting locations at any level.
 */
$page_security = 'SA_WAREHOUSE_SETUP';
$path_to_root = '../..';
include_once($path_to_root . '/includes/session.inc');

$_SESSION['page_title'] = _($help_context = 'Warehouse Locations');

page($_SESSION['page_title']);

include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/inventory/warehouse/includes/db/warehouse_locations_db.inc');
include_once($path_to_root . '/inventory/warehouse/includes/db/warehouse_storage_categories_db.inc');
include_once($path_to_root . '/inventory/warehouse/includes/warehouse_ui.inc');

/**
 * Check whether submitted warehouse location fields contain scalar values only.
 *
 * @param array $field_names Expected scalar field names.
 * @return bool
 */
function warehouse_location_post_fields_are_scalar($field_names) {
	foreach ($field_names as $field_name) {
		if (isset($_POST[$field_name]) && is_array($_POST[$field_name])) {
			return false;
		}
	}

	return true;
}

/**
 * Check whether a value is a non-negative numeric string.
 *
 * @param mixed $value Submitted value.
 * @return bool
 */
function warehouse_location_is_non_negative_number($value) {
	if (!is_scalar($value) || !is_numeric($value)) {
		return false;
	}

	$number = (float)$value;
	return is_finite($number) && $number >= 0 && $number <= 99999999999.9999;
}

/**
 * Check whether a value is a non-negative integer string.
 *
 * @param mixed $value Submitted value.
 * @return bool
 */
function warehouse_location_is_non_negative_integer($value, $max_value = 2147483647) {
	if (!is_scalar($value) || !preg_match('/^\d+$/', trim((string)$value))) {
		return false;
	}

	$value = trim((string)$value);
	if (strlen($value) > 10) {
		return false;
	}

	return (int)$value <= $max_value;
}

/**
 * Check whether a submitted location ID is a positive integer.
 *
 * @param mixed $value Submitted value.
 * @return bool
 */
function warehouse_location_is_positive_integer($value) {
	return warehouse_location_is_non_negative_integer($value) && (int)$value > 0;
}

//-------------------------------------------------------------------------------------
// Determine current warehouse and parent context
//-------------------------------------------------------------------------------------

$warehouse = get_post('warehouse', '');
$parent_id = get_post('parent_id', '');
if (isset($_GET['warehouse'])) $warehouse = $_GET['warehouse'];
if (isset($_GET['parent_id'])) $parent_id = $_GET['parent_id'];

// Mode tracking — use standard simple_page_mode for proper ADD_ITEM/UPDATE_ITEM/Edit/Delete handling
simple_page_mode(true);

//-------------------------------------------------------------------------------------
// Handle bulk create
//-------------------------------------------------------------------------------------

if (isset($_POST['bulk_create']) && $_POST['bulk_create'] != '') {
	$input_error = 0;

	$bulk_type = get_post('bulk_type_id');
	$bulk_count_value = trim(get_post('bulk_count', ''));
	$bulk_parent = get_post('bulk_parent_id', '');

	if (!warehouse_location_post_fields_are_scalar(array('bulk_type_id', 'bulk_count', 'bulk_parent_id', 'warehouse'))) {
		$input_error = 1;
		display_error(_('One or more submitted fields contained an invalid value. Please review the form and try again.'));
	}
	if ($input_error == 0 && !warehouse_location_is_positive_integer($bulk_type)) {
		$input_error = 1;
		display_error(_('Please select a location type for bulk creation.'));
	}
	if ($input_error == 0 && !warehouse_location_is_non_negative_integer($bulk_count_value, 200)) {
		$input_error = 1;
		display_error(_('Bulk count must be between 1 and 200.'));
	}
	$bulk_count = (int)$bulk_count_value;
	if ($input_error == 0 && $bulk_count < 1) {
		$input_error = 1;
		display_error(_('Bulk count must be between 1 and 200.'));
	}
	if ($input_error == 0 && !$warehouse) {
		$input_error = 1;
		display_error(_('Please select a warehouse first.'));
	}
	if ($input_error == 0 && $bulk_parent !== '' && !warehouse_location_is_positive_integer($bulk_parent)) {
		$input_error = 1;
		display_error(_('Parent location must be a valid warehouse location.'));
	}
	if ($input_error == 0 && $bulk_parent !== '' && !warehouse_location_belongs_to_warehouse((int)$bulk_parent, $warehouse)) {
		$input_error = 1;
		display_error(_('Parent location does not belong to the selected warehouse.'));
	}

	if ($input_error == 0) {
		$type = get_warehouse_location_type((int)$bulk_type, false);
		if (!$type) {
			$input_error = 1;
			display_error(_('Please select a valid active location type for bulk creation.'));
		}
	}

	if ($input_error == 0) {
		$type_code = $type['type_code'];
		begin_transaction();
		for ($i = 0; $i < $bulk_count; $i++) {
			$auto_code = generate_warehouse_location_code(
				$bulk_parent ? $bulk_parent : null,
				$warehouse,
				$type_code
			);
			if (!is_warehouse_location_code_globally_unique($auto_code)) {
				cancel_transaction();
				$input_error = 1;
				display_error(sprintf(_('Generated location code "%s" already exists. Please retry after reviewing existing locations.'), $auto_code));
				break;
			}
			add_warehouse_location(
				$auto_code,
				$type['type_name'] . ' ' . ($i + 1),
				$bulk_parent ? $bulk_parent : null,
				$warehouse,
				$bulk_type
			);
		}
		if ($input_error == 0) {
			commit_transaction();
			display_notification(sprintf(_('%d locations created successfully.'), $bulk_count));
			$Mode = 'RESET';
		}
	}
}

//-------------------------------------------------------------------------------------
// Handle ADD / UPDATE
//-------------------------------------------------------------------------------------

if ($Mode == 'ADD_ITEM' || $Mode == 'UPDATE_ITEM') {
	$input_error = 0;
	$location_code = trim(get_post('loc_code'));
	$location_name = trim(get_post('loc_name'));
	$location_type_id = get_post('location_type_id');
	$max_weight = trim(get_post('max_weight', ''));
	$max_volume = trim(get_post('max_volume', ''));
	$max_units = trim(get_post('max_units', ''));
	$storage_category_id = get_post('storage_category_id');
	$zone_type = get_post('zone_type', '');
	$abc_class = get_post('abc_class', '');
	$pick_sequence = trim(get_post('pick_sequence', ''));
	$barcode = trim(get_post('barcode', ''));
	$allowed_zone_types = array('', 'staging', 'quality', 'packing', 'dock', 'ambient', 'cold', 'frozen', 'hazmat', 'highvalue', 'bulk', 'picking');
	$allowed_abc_classes = array('', 'A', 'B', 'C');
	$location_type = false;

	if (!warehouse_location_post_fields_are_scalar(array(
		'loc_code', 'loc_name', 'location_type_id', 'max_weight', 'max_volume', 'max_units',
		'storage_category_id', 'zone_type', 'abc_class', 'pick_sequence', 'barcode',
		'warehouse', 'parent_id', 'selected_id', 'is_default_receipt', 'is_default_ship',
		'is_default_scrap', 'is_default_production'
	))) {
		$input_error = 1;
		display_error(_('One or more submitted fields contained an invalid value. Please review the form and try again.'));
	}

	if ($input_error == 0 && strlen($location_code) == 0) {
		$input_error = 1;
		display_error(_('The location code must be entered.'));
		set_focus('loc_code');
	} elseif ($input_error == 0 && strlen($location_code) > 30) {
		$input_error = 1;
		display_error(_('The location code must be 30 characters or less.'));
		set_focus('loc_code');
	} elseif ($input_error == 0 && strlen($location_name) == 0) {
		$input_error = 1;
		display_error(_('The location name must be entered.'));
		set_focus('loc_name');
	} elseif ($input_error == 0 && strlen($location_name) > 100) {
		$input_error = 1;
		display_error(_('The location name must be 100 characters or less.'));
		set_focus('loc_name');
	} elseif ($input_error == 0 && !warehouse_location_is_non_negative_integer($location_type_id)) {
		$input_error = 1;
		display_error(_('Please select a location type.'));
		set_focus('location_type_id');
	} elseif ($input_error == 0 && !$warehouse) {
		$input_error = 1;
		display_error(_('Please select a warehouse first.'));
	}

	if ($input_error == 0) {
		$location_type = get_warehouse_location_type((int)$location_type_id, false);
		if (!$location_type) {
			$input_error = 1;
			display_error(_('Please select a valid active location type.'));
			set_focus('location_type_id');
		}
	}
	if ($input_error == 0 && $selected_id != -1 && !warehouse_location_is_positive_integer($selected_id)) {
		$input_error = 1;
		display_error(_('Selected location must be a valid warehouse location.'));
	}
	if ($input_error == 0 && $selected_id != -1 && !warehouse_location_belongs_to_warehouse((int)$selected_id, $warehouse)) {
		$input_error = 1;
		display_error(_('Selected location does not belong to the selected warehouse.'));
	}
	if ($input_error == 0 && $selected_id == -1 && $parent_id !== '' && !warehouse_location_is_positive_integer($parent_id)) {
		$input_error = 1;
		display_error(_('Parent location must be a valid warehouse location.'));
	}
	if ($input_error == 0 && $selected_id == -1 && $parent_id !== '' && !warehouse_location_belongs_to_warehouse((int)$parent_id, $warehouse)) {
		$input_error = 1;
		display_error(_('Parent location does not belong to the selected warehouse.'));
	}
	if ($input_error == 0 && $max_weight !== '' && !warehouse_location_is_non_negative_number($max_weight)) {
		$input_error = 1;
		display_error(_('Max weight must be a non-negative number.'));
		set_focus('max_weight');
	}
	if ($input_error == 0 && $max_volume !== '' && !warehouse_location_is_non_negative_number($max_volume)) {
		$input_error = 1;
		display_error(_('Max volume must be a non-negative number.'));
		set_focus('max_volume');
	}
	if ($input_error == 0 && $max_units !== '' && !warehouse_location_is_non_negative_integer($max_units)) {
		$input_error = 1;
		display_error(_('Max units must be a non-negative integer.'));
		set_focus('max_units');
	}
	if ($input_error == 0 && $pick_sequence !== '' && !warehouse_location_is_non_negative_integer($pick_sequence)) {
		$input_error = 1;
		display_error(_('Pick sequence must be a non-negative integer.'));
		set_focus('pick_sequence');
	}
	if ($input_error == 0
		&& $storage_category_id !== ''
		&& $storage_category_id != -1
		&& $storage_category_id != 0
		&& !warehouse_location_is_non_negative_integer($storage_category_id)) {
		$input_error = 1;
		display_error(_('Storage category must be a valid selection.'));
		set_focus('storage_category_id');
	}
	if ($input_error == 0
		&& $storage_category_id !== ''
		&& $storage_category_id != -1
		&& $storage_category_id != 0) {
		$storage_category = get_storage_category((int)$storage_category_id);
		if (!$storage_category || (isset($storage_category['inactive']) && $storage_category['inactive'])) {
			$input_error = 1;
			display_error(_('Storage category must be an active valid selection.'));
			set_focus('storage_category_id');
		}
	}
	if ($input_error == 0 && !in_array($zone_type, $allowed_zone_types, true)) {
		$input_error = 1;
		display_error(_('Zone type must be a valid selection.'));
		set_focus('zone_type');
	}
	if ($input_error == 0 && !in_array($abc_class, $allowed_abc_classes, true)) {
		$input_error = 1;
		display_error(_('ABC class must be a valid selection.'));
		set_focus('abc_class');
	}
	if ($input_error == 0 && strlen($barcode) > 50) {
		$input_error = 1;
		display_error(_('Barcode must be 50 characters or less.'));
		set_focus('barcode');
	}

	// Validate code uniqueness
	if ($input_error == 0) {
		$exclude_id = ($selected_id != -1) ? $selected_id : null;
		if (!is_warehouse_location_code_globally_unique($location_code, $exclude_id)) {
			$input_error = 1;
			display_error(_('This location code already exists. Location codes must be unique across warehouses.'));
			set_focus('loc_code');
		}
	}

	if ($input_error == 0) {
		$extra = array();
		if ($max_weight !== '') $extra['max_weight'] = $max_weight;
		if ($max_volume !== '') $extra['max_volume'] = $max_volume;
		if ($max_units !== '') $extra['max_units'] = (int)$max_units;
		if ($storage_category_id !== '' && $storage_category_id != -1 && $storage_category_id != 0) {
			$extra['storage_category_id'] = (int)$storage_category_id;
		}
		if ($zone_type !== '') $extra['zone_type'] = $zone_type;
		if ($abc_class !== '') $extra['abc_class'] = $abc_class;
		if ($pick_sequence !== '') $extra['pick_sequence'] = (int)$pick_sequence;
		if ($barcode !== '') $extra['barcode'] = $barcode;
		$extra['is_default_receipt'] = check_value('is_default_receipt') ? 1 : 0;
		$extra['is_default_ship'] = check_value('is_default_ship') ? 1 : 0;
		$extra['is_default_scrap'] = check_value('is_default_scrap') ? 1 : 0;
		$extra['is_default_production'] = check_value('is_default_production') ? 1 : 0;

		if ($selected_id != -1) {
			// Update
			update_warehouse_location(
				$selected_id,
				$location_name,
				(int)$location_type_id,
				$extra
			);
			display_notification(_('Location has been updated.'));
		} else {
			// Add
			$effective_parent = $parent_id ? $parent_id : null;
			add_warehouse_location(
				$location_code,
				$location_name,
				$effective_parent,
				$warehouse,
				(int)$location_type_id,
				$extra
			);
			display_notification(_('New location has been added.'));
		}

		$Mode = 'RESET';
	}
}

//-------------------------------------------------------------------------------------
// Handle DELETE
//-------------------------------------------------------------------------------------

if ($Mode == 'Delete' && $selected_id != -1) {
	if (!warehouse_location_is_positive_integer($selected_id)) {
		display_error(_('Selected location must be a valid warehouse location.'));
	} elseif ($warehouse && !warehouse_location_belongs_to_warehouse((int)$selected_id, $warehouse)) {
		display_error(_('Selected location does not belong to the selected warehouse.'));
	} else {
		$can = can_delete_warehouse_location($selected_id);
		if ($can === true) {
			delete_warehouse_location($selected_id);
			display_notification(_('Location has been deleted.'));
		} else {
			display_error($can);
		}
	}
	$selected_id = -1;
	$Mode = 'RESET';
}

if ($Mode == 'RESET') {
	$selected_id = -1;
	$sav_wh = $warehouse;
	$sav_parent = $parent_id;
	unset($_POST);
	$_POST['warehouse'] = $warehouse = $sav_wh;
	$_POST['parent_id'] = $parent_id = $sav_parent;
}

//-------------------------------------------------------------------------------------
// Start form
//-------------------------------------------------------------------------------------

start_form();

//-------------------------------------------------------------------------------------
// Warehouse selector
//-------------------------------------------------------------------------------------

$wms_locations = get_wms_enabled_locations();
$has_wms = false;
$warehouses = array();
while ($row = db_fetch($wms_locations)) {
	$has_wms = true;
	$warehouses[$row['loc_code']] = $row['location_name'];
}

if (!$has_wms) {
	display_note(_('No warehouses have WMS enabled. Go to Inventory Locations and enable WMS on a location first.'));
	end_form();
	end_page();
	exit;
}

// Default to first warehouse if none selected
if (!$warehouse && !empty($warehouses)) {
	reset($warehouses);
	$warehouse = key($warehouses);
}

// Handle warehouse change before validating parent/selected context from the previous warehouse.
if (list_updated('warehouse')) {
	$warehouse = get_post('warehouse');
	$parent_id = '';
	$selected_id = -1;
	$_POST['parent_id'] = '';
	$Ajax->activate('_page_body');
}

if ($parent_id !== '') {
	if (!warehouse_location_is_positive_integer($parent_id) || !warehouse_location_belongs_to_warehouse((int)$parent_id, $warehouse)) {
		display_error(_('Parent location does not belong to the selected warehouse.'));
		$parent_id = '';
		$_POST['parent_id'] = '';
	}
}

if ($selected_id != -1 && (!warehouse_location_is_positive_integer($selected_id) || !warehouse_location_belongs_to_warehouse((int)$selected_id, $warehouse))) {
	display_error(_('Selected location does not belong to the selected warehouse.'));
	$selected_id = -1;
	$Mode = 'RESET';
}

echo "<div style='margin-bottom:10px;'>";
echo "<table class='tablestyle_noborder'><tr>";
warehouse_list_cells(_('Warehouse:'), 'warehouse', $warehouse, false, true);
echo "</tr></table>";
echo "</div>";
echo "<script>
(function() {
	var select = document.getElementById('warehouse');
	if (!select || select.getAttribute('data-wh-submit-bound') === '1') return;
	select.setAttribute('data-wh-submit-bound', '1');
	var submitting = false;
	var submitWarehouse = function() {
		if (submitting) return;
		submitting = true;
		if (window.JsHttpRequest && select.form) {
			JsHttpRequest.request('_warehouse_update', select.form);
		} else {
			submitting = false;
		}
	};
	select.addEventListener('change', submitWarehouse);
	if (window.jQuery) {
		window.jQuery(select).on('select2:select', submitWarehouse);
	}
})();
</script>";

//-------------------------------------------------------------------------------------
// Breadcrumb navigation
//-------------------------------------------------------------------------------------

if ($parent_id) {
	$breadcrumb = get_warehouse_location_breadcrumb($parent_id);
	$base_url = $_SERVER['PHP_SELF'] . '?warehouse=' . urlencode($warehouse) . '&';

	// Add "root" link
	echo "<div class='wh-breadcrumb' style='margin:5px 0; padding:5px 10px; background:#f5f5f5; border-radius:3px; font-size:13px;'>";
	echo "<i class='fa fa-building' style='margin-right:5px;'></i> ";
	echo "<a href='" . $_SERVER['PHP_SELF'] . "?warehouse=" . urlencode($warehouse) . "'>" . htmlspecialchars($warehouses[$warehouse]) . "</a>";
	foreach ($breadcrumb as $i => $loc) {
		echo " <span style='color:#999;'>&gt;</span> ";
		if ($i < count($breadcrumb) - 1) {
			echo "<a href='" . $base_url . "parent_id=" . $loc['loc_id'] . "'>" . htmlspecialchars($loc['loc_name']) . "</a>";
		} else {
			echo "<strong>" . htmlspecialchars($loc['loc_name']) . "</strong>";
		}
	}
	echo "</div>\n";
}

//-------------------------------------------------------------------------------------
// Summary cards
//-------------------------------------------------------------------------------------

display_warehouse_location_summary($warehouse);

//-------------------------------------------------------------------------------------
// Location list table
//-------------------------------------------------------------------------------------

$children = get_warehouse_location_children(
	$parent_id ? $parent_id : null,
	$warehouse,
	check_value('show_inactive')
);

// Zone utilization dashboard (only when viewing top-level zones)
if (!$parent_id || $parent_id == '') {
	echo "<div class='section-header' style='margin:10px 0 5px;'><strong><i class='fa fa-tachometer' style='margin-right:4px;'></i>" . _('Capacity Overview') . "</strong></div>";
	display_warehouse_utilization_dashboard($warehouse);
}

start_table(TABLESTYLE);
$th = array(_('Location Name'), _('Code'), _('Type'), _('Can Store'), _('Max Weight (kg)'), _('Max Volume (m³)'), _('Utilization'), _('Children'), '', '');
table_header($th);

$k = 0;
$has_rows = false;
while ($myrow = db_fetch($children)) {
	$has_rows = true;
	alt_table_row_color($k);

	$has_sub = ($myrow['child_count'] > 0);
	$icon = get_location_type_icon($myrow['type_code']);
	$inactive_label = $myrow['is_active'] ? '' : ' <span style="color:red; font-size:10px;">(' . _('inactive') . ')</span>';

	// Name column - non-storable locations remain navigable so their first child can be created.
	echo "<td>";
	echo "<i class='" . $icon . "' style='margin-right:4px; color:#555;'></i>";
	if ($has_sub || !$myrow['can_store']) {
		echo "<a href='" . $_SERVER['PHP_SELF'] . "?warehouse=" . urlencode($warehouse) . "&parent_id=" . $myrow['loc_id'] . "'>";
		echo htmlspecialchars($myrow['loc_name']);
		echo "</a>";
	} else {
		echo htmlspecialchars($myrow['loc_name']);
	}
	echo $inactive_label;
	echo "</td>";

	label_cell($myrow['loc_code']);
	label_cell($myrow['type_name']);
	label_cell($myrow['can_store'] ? _('Yes') : _('No'), 'align=center');
	label_cell($myrow['max_weight'] ? number_format2($myrow['max_weight'], 2) : '-', 'align=right');
	label_cell($myrow['max_volume'] ? number_format2($myrow['max_volume'], 4) : '-', 'align=right');

	// Utilization column
	if ($myrow['can_store']) {
		$util = get_bin_utilization($myrow['loc_id']);
		echo '<td style="min-width:80px;">';
		if ($util['max_weight'] !== null || $util['max_volume'] !== null || $util['max_units'] !== null) {
			echo utilization_bar($util['overall_pct'], $util['status'], '', 16);
		} else {
			echo '<span style="color:#999;">-</span>';
		}
		echo '</td>';
	} elseif ($myrow['child_count'] > 0) {
		$zone_util = get_zone_utilization($myrow['loc_id']);
		echo '<td style="min-width:80px;">';
		if ($zone_util['bins_with_capacity'] > 0) {
			echo utilization_bar($zone_util['overall_pct'], $zone_util['status'], '', 16);
		} else {
			echo '<span style="color:#999;">-</span>';
		}
		echo '</td>';
	} else {
		label_cell('-', 'align=center');
	}

	label_cell($myrow['child_count'], 'align=center');

	edit_button_cell("Edit" . $myrow['loc_id'], _('Edit'));
	delete_button_cell("Delete" . $myrow['loc_id'], _('Delete'));
	end_row();
}

if (!$has_rows) {
	echo "<tr><td colspan='10' class='center'>" . _('No locations at this level. Use the form below to add.') . "</td></tr>\n";
}

end_table();

//-------------------------------------------------------------------------------------
// Add/Edit form
//-------------------------------------------------------------------------------------

echo '<br>';
start_outer_table();

// If editing, load existing data
if ($selected_id != -1 && $Mode == 'Edit') {
	$loc = get_warehouse_location($selected_id);
	if ($loc) {
		$_POST['loc_code'] = $loc['loc_code'];
		$_POST['loc_name'] = $loc['loc_name'];
		$_POST['location_type_id'] = $loc['location_type_id'];
		$_POST['max_weight'] = $loc['max_weight'];
		$_POST['max_volume'] = $loc['max_volume'];
		$_POST['max_units'] = $loc['max_units'];
		$_POST['storage_category_id'] = $loc['storage_category_id'];
		$_POST['zone_type'] = $loc['zone_type'];
		$_POST['abc_class'] = $loc['abc_class'];
		$_POST['pick_sequence'] = $loc['pick_sequence'];
		$_POST['barcode'] = $loc['barcode'];
		$_POST['is_default_receipt'] = $loc['is_default_receipt'];
		$_POST['is_default_ship'] = $loc['is_default_ship'];
		$_POST['is_default_scrap'] = $loc['is_default_scrap'];
		$_POST['is_default_production'] = $loc['is_default_production'];
	}
	hidden('selected_id', $selected_id);
}

hidden('parent_id', $parent_id);

table_section(1);
// Location code
if ($selected_id != -1) {
	hidden('loc_code', get_post('loc_code'));
	label_row(_('Location Code:'), get_post('loc_code'));
} else {
	// Auto-generate button
	$auto_code = '';
	if ($warehouse && get_post('location_type_id')) {
		$type = get_warehouse_location_type(get_post('location_type_id'));
		if ($type) {
			$auto_code = generate_warehouse_location_code(
				$parent_id ? $parent_id : null,
				$warehouse,
				$type['type_code']
			);
		}
	}
	text_row_ex(_('Location Code:'), 'loc_code', 30, 30);
	if ($auto_code) {
		echo "<tr><td></td><td><small style='color:#888;'>" . _('Suggested:') . " <strong>" . htmlspecialchars($auto_code) . "</strong>";
		echo " <button type='button' onclick=\"document.getElementById('loc_code').value='" . htmlspecialchars($auto_code) . "';\" class='btn btn-xs btn-default'>" . _('Use') . "</button>";
		echo "</small></td></tr>\n";
	}
}

text_row_ex(_('Location Name:'), 'loc_name', 50, 100);

warehouse_location_type_list_row(_('Location Type:'), 'location_type_id', get_post('location_type_id'), true);

// Capacity fields
table_section_title(_('Capacity Limits'));
small_amount_row(_('Max Weight (kg):'), 'max_weight', get_post('max_weight', ''), null, null, 2);
small_amount_row(_('Max Volume (m³):'), 'max_volume', get_post('max_volume', ''), null, null, 4);
text_row_ex(_('Max Units:'), 'max_units', 10, 10);

// Storage category
storage_category_list_row(_('Storage Category:'), 'storage_category_id', get_post('storage_category_id'), true);

table_section(2);

// Zone/Classification
table_section_title(_('Classification'));

$zone_types = array(
	'' => _('-- none --'),
	'staging' => _('Staging'),
	'quality' => _('Quality'),
	'packing' => _('Packing'),
	'dock' => _('Dock'),
	'ambient' => _('Ambient'),
	'cold' => _('Cold Storage'),
	'frozen' => _('Frozen'),
	'hazmat' => _('Hazardous Materials'),
	'highvalue' => _('High Value'),
	'bulk' => _('Bulk Storage'),
	'picking' => _('Picking'),
);
array_selector_row(_('Zone Type:'), 'zone_type', get_post('zone_type', ''), $zone_types);

$abc_classes = array('' => _('-- none --'), 'A' => 'A - ' . _('High'), 'B' => 'B - ' . _('Medium'), 'C' => 'C - ' . _('Low'));
array_selector_row(_('ABC Class:'), 'abc_class', get_post('abc_class', ''), $abc_classes);

text_row_ex(_('Pick Sequence:'), 'pick_sequence', 10, 10);
text_row_ex(_('Barcode:'), 'barcode', 50, 50);

// Default flags
table_section_title(_('Default Flags'));
check_row(_('Default Receipt Location'), 'is_default_receipt', get_post('is_default_receipt', 0));
check_row(_('Default Shipping Location'), 'is_default_ship', get_post('is_default_ship', 0));
check_row(_('Default Scrap Location'), 'is_default_scrap', get_post('is_default_scrap', 0));
check_row(_('Default Production Location'), 'is_default_production', get_post('is_default_production', 0));

end_outer_table(1);

submit_add_or_update_center($selected_id == -1, '', 'both');

//-------------------------------------------------------------------------------------
// Bulk create section
//-------------------------------------------------------------------------------------

start_fieldset(_('Bulk Create Locations'));

start_table(TABLESTYLE2);
warehouse_location_type_list_row(_('Location Type:'), 'bulk_type_id', get_post('bulk_type_id'));
text_row_ex(_('Number to Create:'), 'bulk_count', 10, 10);

// Parent for bulk create: default to current browsing parent
if ($parent_id) {
	$parent_loc = get_warehouse_location($parent_id);
	$parent_label = $parent_loc ? $parent_loc['full_name'] : $parent_id;
	label_row(_('Parent Location:'), htmlspecialchars($parent_label));
	hidden('bulk_parent_id', $parent_id);
} else {
	label_row(_('Parent Location:'), _('(Root level)'));
	hidden('bulk_parent_id', '');
}

end_table(1);
submit_center('bulk_create', _('Bulk Create'), true, '', 'default');
end_fieldset();
end_form();

end_page();
