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
 * Storage Categories — CRUD page for warehouse storage category definitions.
 *
 * Defines capacity limits, environmental constraints (temperature, humidity),
 * and mixing rules for warehouse bins.
 */
$page_security = 'SA_WAREHOUSE_STORAGE';
$path_to_root = '../..';
include_once($path_to_root . '/includes/session.inc');

$_SESSION['page_title'] = _($help_context = 'Storage Categories');

page($_SESSION['page_title']);

include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/inventory/warehouse/includes/db/warehouse_storage_categories_db.inc');

simple_page_mode(true);

//-------------------------------------------------------------------------------------
// Handle ADD / UPDATE
//-------------------------------------------------------------------------------------

if ($Mode == 'ADD_ITEM' || $Mode == 'UPDATE_ITEM') {
	$input_error = 0;

	if (strlen(get_post('category_name')) == 0) {
		$input_error = 1;
		display_error(_('The category name must be entered.'));
		set_focus('category_name');
	}

	// Validate numeric fields
	$max_weight = get_post('max_weight');
	$max_volume = get_post('max_volume');
	$max_units = get_post('max_units');
	$temp_min = get_post('temperature_min');
	$temp_max = get_post('temperature_max');
	$humidity_max = get_post('humidity_max');

	if ($max_weight !== '' && !is_numeric($max_weight)) {
		$input_error = 1;
		display_error(_('Max weight must be a number.'));
		set_focus('max_weight');
	}
	if ($max_volume !== '' && !is_numeric($max_volume)) {
		$input_error = 1;
		display_error(_('Max volume must be a number.'));
		set_focus('max_volume');
	}
	if ($max_units !== '' && (!is_numeric($max_units) || (int)$max_units < 0)) {
		$input_error = 1;
		display_error(_('Max units must be a positive integer.'));
		set_focus('max_units');
	}
	if ($temp_min !== '' && !is_numeric($temp_min)) {
		$input_error = 1;
		display_error(_('Temperature minimum must be a number.'));
		set_focus('temperature_min');
	}
	if ($temp_max !== '' && !is_numeric($temp_max)) {
		$input_error = 1;
		display_error(_('Temperature maximum must be a number.'));
		set_focus('temperature_max');
	}
	if ($temp_min !== '' && $temp_max !== '' && is_numeric($temp_min) && is_numeric($temp_max) && (float)$temp_min > (float)$temp_max) {
		$input_error = 1;
		display_error(_('Temperature minimum cannot be greater than maximum.'));
		set_focus('temperature_min');
	}

	if ($input_error != 1) {
		if ($selected_id != -1) {
			update_storage_category(
				$selected_id,
				get_post('category_name'),
				$max_weight !== '' ? $max_weight : null,
				$max_volume !== '' ? $max_volume : null,
				$max_units !== '' ? $max_units : null,
				check_value('allow_mixed_items') ? 1 : 0,
				check_value('allow_mixed_lots') ? 1 : 0,
				$temp_min !== '' ? $temp_min : null,
				$temp_max !== '' ? $temp_max : null,
				$humidity_max !== '' ? $humidity_max : null,
				check_value('is_hazmat') ? 1 : 0
			);
			display_notification(_('Storage category has been updated.'));
		} else {
			add_storage_category(
				get_post('category_name'),
				$max_weight !== '' ? $max_weight : null,
				$max_volume !== '' ? $max_volume : null,
				$max_units !== '' ? $max_units : null,
				check_value('allow_mixed_items') ? 1 : 0,
				check_value('allow_mixed_lots') ? 1 : 0,
				$temp_min !== '' ? $temp_min : null,
				$temp_max !== '' ? $temp_max : null,
				$humidity_max !== '' ? $humidity_max : null,
				check_value('is_hazmat') ? 1 : 0
			);
			display_notification(_('New storage category has been added.'));
		}
		$Mode = 'RESET';
	}
}

//-------------------------------------------------------------------------------------
// Handle DELETE
//-------------------------------------------------------------------------------------

if ($Mode == 'Delete') {
	$can = can_delete_storage_category($selected_id);
	if ($can === true) {
		delete_storage_category($selected_id);
		display_notification(_('Selected storage category has been deleted.'));
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
// List table
//-------------------------------------------------------------------------------------

$result = get_storage_categories(check_value('show_inactive'));

start_form();
start_table(TABLESTYLE);
$th = array(
	_('Category Name'), _('Max Weight (kg)'), _('Max Volume (m³)'), _('Max Units'),
	_('Mixed Items'), _('Mixed Lots'), _('Temp Min (°C)'), _('Temp Max (°C)'),
	_('Humidity Max (%)'), _('Hazmat'), '', ''
);
inactive_control_column($th);
table_header($th);

$k = 0;
while ($myrow = db_fetch($result)) {
	alt_table_row_color($k);

	label_cell($myrow['category_name']);
	label_cell($myrow['max_weight'] !== null ? number_format2($myrow['max_weight'], 2) : '-', 'align=right');
	label_cell($myrow['max_volume'] !== null ? number_format2($myrow['max_volume'], 4) : '-', 'align=right');
	label_cell($myrow['max_units'] !== null ? $myrow['max_units'] : '-', 'align=right');
	label_cell($myrow['allow_mixed_items'] ? _('Yes') : _('No'), 'align=center');
	label_cell($myrow['allow_mixed_lots'] ? _('Yes') : _('No'), 'align=center');
	label_cell($myrow['temperature_min'] !== null ? $myrow['temperature_min'] . '°' : '-', 'align=right');
	label_cell($myrow['temperature_max'] !== null ? $myrow['temperature_max'] . '°' : '-', 'align=right');
	label_cell($myrow['humidity_max'] !== null ? $myrow['humidity_max'] . '%' : '-', 'align=right');
	label_cell($myrow['is_hazmat'] ? '<span style="color:red;">' . _('Yes') . '</span>' : _('No'), 'align=center');

	inactive_control_cell($myrow['id'], $myrow['inactive'], 'wh_storage_categories', 'id');
	edit_button_cell("Edit" . $myrow['id'], _('Edit'));
	delete_button_cell("Delete" . $myrow['id'], _('Delete'));
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
		$myrow = get_storage_category($selected_id);
		$_POST['category_name'] = $myrow['category_name'];
		$_POST['max_weight'] = $myrow['max_weight'];
		$_POST['max_volume'] = $myrow['max_volume'];
		$_POST['max_units'] = $myrow['max_units'];
		$_POST['allow_mixed_items'] = $myrow['allow_mixed_items'];
		$_POST['allow_mixed_lots'] = $myrow['allow_mixed_lots'];
		$_POST['temperature_min'] = $myrow['temperature_min'];
		$_POST['temperature_max'] = $myrow['temperature_max'];
		$_POST['humidity_max'] = $myrow['humidity_max'];
		$_POST['is_hazmat'] = $myrow['is_hazmat'];
	}
	hidden('selected_id', $selected_id);
}

text_row_ex(_('Category Name:'), 'category_name', 50, 60);

echo "<tr><td colspan='2' style='padding-top:10px;'><strong>" . _('Capacity Limits') . "</strong></td></tr>\n";
small_amount_row(_('Max Weight (kg):'), 'max_weight', get_post('max_weight', ''), null, null, 2);
small_amount_row(_('Max Volume (m³):'), 'max_volume', get_post('max_volume', ''), null, null, 4);
text_row_ex(_('Max Units:'), 'max_units', 10, 10);

echo "<tr><td colspan='2' style='padding-top:10px;'><strong>" . _('Mixing Rules') . "</strong></td></tr>\n";
check_row(_('Allow Mixed Items'), 'allow_mixed_items', get_post('allow_mixed_items', 1));
check_row(_('Allow Mixed Lots/Batches'), 'allow_mixed_lots', get_post('allow_mixed_lots', 1));

echo "<tr><td colspan='2' style='padding-top:10px;'><strong>" . _('Environmental Constraints') . "</strong></td></tr>\n";
small_amount_row(_('Temperature Min (°C):'), 'temperature_min', get_post('temperature_min', ''), null, null, 2);
small_amount_row(_('Temperature Max (°C):'), 'temperature_max', get_post('temperature_max', ''), null, null, 2);
small_amount_row(_('Humidity Max (%):'), 'humidity_max', get_post('humidity_max', ''), null, null, 2);
check_row(_('Hazardous Materials'), 'is_hazmat', get_post('is_hazmat', 0));

end_table(1);
submit_add_or_update_center($selected_id == -1, '', 'both');

end_form();
end_page();
