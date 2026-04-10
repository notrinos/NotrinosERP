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
 * Quality Parameters Management — Define inspection criteria (numeric, text, boolean, list).
 *
 * Features:
 *   - CRUD for quality parameters
 *   - Per-item, per-category, or global scope
 *   - Numeric range validation (min/max), list-type acceptable values
 *   - Required/optional flag, display sequence
 *   - Inactive control
 */
$page_security = 'SA_QC_PARAMETERS';
$path_to_root = '../..';
include_once($path_to_root . '/includes/session.inc');

$_SESSION['page_title'] = _($help_context = 'Quality Parameters');

page($_SESSION['page_title'], false, false, '', '');

include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/includes/date_functions.inc');
include_once($path_to_root . '/inventory/includes/db/quality_inspection_db.inc');
include_once($path_to_root . '/inventory/includes/inventory_db.inc');

simple_page_mode(true);

//----------------------------------------------------------------------
// Handle Delete
//----------------------------------------------------------------------
if ($Mode == 'Delete') {
	if (!can_delete_quality_parameter($selected_id)) {
		display_error(_('Cannot delete this parameter because it has been used in inspections.'));
	} else {
		delete_quality_parameter($selected_id);
		display_notification(_('Quality parameter has been deleted.'));
	}
	$Mode = 'RESET';
}

//----------------------------------------------------------------------
// Handle Add / Update
//----------------------------------------------------------------------
if ($Mode == 'ADD_ITEM' || $Mode == 'UPDATE_ITEM') {

	$input_error = 0;

	if (strlen($_POST['param_name']) == 0) {
		$input_error = 1;
		display_error(_('Parameter name cannot be empty.'));
	}

	if ($_POST['parameter_type'] === 'numeric') {
		if ($_POST['min_value'] !== '' && $_POST['max_value'] !== ''
			&& (float)$_POST['min_value'] > (float)$_POST['max_value'])
		{
			$input_error = 1;
			display_error(_('Minimum value cannot be greater than maximum value.'));
		}
	}

	if ($input_error == 0) {
		$stock_id = !empty($_POST['stock_id_filter']) ? $_POST['stock_id_filter'] : null;
		$category_id = !empty($_POST['category_id_filter']) && (int)$_POST['category_id_filter'] > 0
			? (int)$_POST['category_id_filter'] : null;
		$acceptable_values = null;
		if ($_POST['parameter_type'] === 'list' && !empty($_POST['acceptable_values'])) {
			// Parse comma-separated into JSON array
			$vals = array_map('trim', explode(',', $_POST['acceptable_values']));
			$vals = array_filter($vals, function($v) { return $v !== ''; });
			$acceptable_values = json_encode(array_values($vals));
		}

		if ($Mode == 'ADD_ITEM') {
			add_quality_parameter(
				$_POST['param_name'],
				$_POST['parameter_type'],
				$stock_id,
				$category_id,
				$_POST['min_value'] !== '' ? $_POST['min_value'] : null,
				$_POST['max_value'] !== '' ? $_POST['max_value'] : null,
				!empty($_POST['unit_of_measure']) ? $_POST['unit_of_measure'] : null,
				$acceptable_values,
				check_value('is_required'),
				(int)$_POST['sequence'],
				!empty($_POST['description']) ? $_POST['description'] : null
			);
			display_notification(_('New quality parameter has been added.'));
		} else {
			update_quality_parameter(
				$selected_id,
				$_POST['param_name'],
				$_POST['parameter_type'],
				$stock_id,
				$category_id,
				$_POST['min_value'] !== '' ? $_POST['min_value'] : null,
				$_POST['max_value'] !== '' ? $_POST['max_value'] : null,
				!empty($_POST['unit_of_measure']) ? $_POST['unit_of_measure'] : null,
				$acceptable_values,
				check_value('is_required'),
				(int)$_POST['sequence'],
				!empty($_POST['description']) ? $_POST['description'] : null
			);
			display_notification(_('Quality parameter has been updated.'));
		}
		$Mode = 'RESET';
	}
}

//----------------------------------------------------------------------
// Handle Toggle Inactive
//----------------------------------------------------------------------
if (get_post('toggle_inactive')) {
	$pid = (int)get_post('toggle_inactive');
	$param = get_quality_parameter($pid);
	if ($param)
		set_quality_parameter_inactive($pid, $param['inactive'] ? 0 : 1);
	$Mode = 'RESET';
}

//----------------------------------------------------------------------
// Reset form
//----------------------------------------------------------------------
if ($Mode == 'RESET') {
	$selected_id = -1;
	unset($_POST['param_name'], $_POST['parameter_type'], $_POST['stock_id_filter'],
		$_POST['category_id_filter'], $_POST['min_value'], $_POST['max_value'],
		$_POST['unit_of_measure'], $_POST['acceptable_values'], $_POST['is_required'],
		$_POST['sequence'], $_POST['description']);
}

//----------------------------------------------------------------------
// Display Parameters List
//----------------------------------------------------------------------
start_form();

div_start('param_list');

$show_inactive = check_value('show_inactive');

$result = get_quality_parameters(null, null, null, $show_inactive);

start_table(TABLESTYLE, "width='95%'");

$th = array(
	_('Name'), _('Type'), _('Scope'), _('Min'), _('Max'), _('Unit'),
	_('Required'), _('Seq'), '', ''
);
inactive_control_column($th);
table_header($th);

$k = 0;
while ($row = db_fetch($result)) {
	alt_table_row_color($k);

	label_cell($row['name']);
	label_cell(get_qc_parameter_type_label($row['parameter_type']));

	// Scope
	if ($row['stock_id'])
		label_cell($row['stock_id'] . ' — ' . $row['item_description']);
	elseif ($row['category_id'])
		label_cell(_('Category') . ': ' . $row['category_description']);
	else
		label_cell(_('Global'));

	// Min/Max
	if ($row['parameter_type'] === 'numeric') {
		label_cell($row['min_value'] !== null ? number_format2($row['min_value'], 4) : '—');
		label_cell($row['max_value'] !== null ? number_format2($row['max_value'], 4) : '—');
	} else {
		label_cell('—');
		label_cell('—');
	}

	label_cell($row['unit'] ? $row['unit'] : '—');
	label_cell($row['mandatory'] ? _('Yes') : _('No'));
	label_cell($row['sort_order']);

	// Edit / Delete buttons
	edit_button_cell("Edit" . $row['parameter_id'], _('Edit'));
	delete_button_cell("Delete" . $row['parameter_id'], _('Delete'));

	inactive_control_cell($row['parameter_id'], $row['inactive'],
		'quality_parameters', 'id');

	end_row();
}

inactive_control_row($th);
end_table(1);

div_end();

//----------------------------------------------------------------------
// Display Add/Edit Form
//----------------------------------------------------------------------

start_table(TABLESTYLE2);

if ($Mode == 'Edit') {
	$row = get_quality_parameter($selected_id);
	if ($row) {
		$_POST['param_name'] = $row['name'];
		$_POST['parameter_type'] = $row['parameter_type'];
		$_POST['stock_id_filter'] = $row['stock_id'];
		$_POST['category_id_filter'] = $row['category_id'];
		$_POST['min_value'] = $row['min_value'] !== null ? $row['min_value'] : '';
		$_POST['max_value'] = $row['max_value'] !== null ? $row['max_value'] : '';
		$_POST['unit_of_measure'] = $row['unit'] ? $row['unit'] : '';
		if ($row['acceptable_values']) {
			$decoded = html_entity_decode($row['acceptable_values'], ENT_QUOTES, 'UTF-8');
			$list = json_decode($decoded, true);
			$_POST['acceptable_values'] = is_array($list) ? implode(', ', $list) : '';
		} else {
			$_POST['acceptable_values'] = '';
		}
		$_POST['is_required'] = $row['mandatory'];
		$_POST['sequence'] = $row['sort_order'];
		$_POST['description'] = $row['description'] ? $row['description'] : '';
	}
}

if ($selected_id != -1)
	hidden('selected_id', $selected_id);

text_row_ex(_('Parameter Name') . ':', 'param_name', 50, 100);

$param_types = get_qc_parameter_types();
array_selector_row(_('Type') . ':', 'parameter_type', null, $param_types,
	array('select_submit' => true));

// Show applicable fields based on parameter_type
$ptype = get_post('parameter_type', 'numeric');

if ($ptype === 'numeric') {
	text_row_ex(_('Minimum Value') . ':', 'min_value', 20, 20);
	text_row_ex(_('Maximum Value') . ':', 'max_value', 20, 20);
	text_row_ex(_('Unit of Measure') . ':', 'unit_of_measure', 20, 20);
} elseif ($ptype === 'list') {
	text_row_ex(_('Acceptable Values (comma-separated)') . ':', 'acceptable_values', 60, 255);
}

// Scope fields
echo '<tr><td class="label">' . _('Item (blank = global)') . ':</td><td>';
echo stock_items_list('stock_id_filter', null, _('Global - All Items'), true, array(), false, 'stock');
echo '</td></tr>';
stock_categories_list_row(_('Category (blank = all)') . ':', 'category_id_filter',
	null, _('All Categories'));

check_row(_('Required') . ':', 'is_required', null);
text_row_ex(_('Display Sequence') . ':', 'sequence', 10, 10);
textarea_row(_('Description / Instructions') . ':', 'description', null, 60, 3);

end_table(1);

submit_add_or_update_center($selected_id == -1, '', 'both');

end_form();
end_page();
