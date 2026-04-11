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
 * Quality Inspection Entry & Execution.
 *
 * Features:
 *   - List inspections with filters (item, result, type, date range)
 *   - Summary cards (pending, pass, fail, partial counts)
 *   - Create new inspection (manual or auto-from-GRN)
 *   - Enter readings per parameter with auto-evaluation
 *   - Complete inspection with pass/fail determination
 *   - Links to View Inspection and COA PDF
 */
$page_security = 'SA_QC_INSPECTIONS';
$path_to_root = '..';
include_once($path_to_root . '/includes/session.inc');

$_SESSION['page_title'] = _($help_context = 'Quality Inspections');

$js = '';
if (user_use_date_picker())
	$js .= get_js_date_picker();

page($_SESSION['page_title'], false, false, '', $js);

include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/includes/date_functions.inc');
include_once($path_to_root . '/inventory/includes/db/quality_inspection_db.inc');
include_once($path_to_root . '/inventory/includes/db/serial_batch_db.inc');
include_once($path_to_root . '/inventory/includes/inventory_db.inc');

//----------------------------------------------------------------------
// Handle Auto-Create from GRN
//----------------------------------------------------------------------
if (get_post('auto_create_grn')) {
	$grn_no = (int)get_post('grn_no_input');
	$loc_code = get_post('grn_loc_code');
	if ($grn_no > 0) {
		$created = auto_create_inspections_from_grn($grn_no, $loc_code);
		if (count($created) > 0) {
			display_notification(sprintf(_('%d inspection(s) created from GRN #%d.'), count($created), $grn_no));
		} else {
			display_warning(_('No inspections created. Either no QC-required items in this GRN or inspections already exist.'));
		}
	} else {
		display_error(_('Please enter a valid GRN number.'));
	}
	$Ajax->activate('_page_body');
}

//----------------------------------------------------------------------
// Handle Create Manual Inspection
//----------------------------------------------------------------------
if (get_post('create_inspection')) {
	$input_error = 0;

	if (empty($_POST['new_stock_id'])) {
		$input_error = 1;
		display_error(_('Please select an item.'));
	}
	if (floatval(get_post('new_qty')) <= 0) {
		$input_error = 1;
		display_error(_('Quantity must be greater than zero.'));
	}

	if ($input_error == 0) {
		$insp_id = add_quality_inspection(
			$_POST['new_stock_id'],
			$_POST['new_insp_type'],
			null, null,
			(float)$_POST['new_qty'],
			!empty($_POST['new_loc_code']) ? $_POST['new_loc_code'] : null,
			!empty($_POST['new_batch_id']) ? (int)$_POST['new_batch_id'] : null,
			!empty($_POST['new_serial_id']) ? (int)$_POST['new_serial_id'] : null,
			!empty($_POST['new_notes']) ? $_POST['new_notes'] : null
		);

		if ($insp_id) {
			display_notification(sprintf(_('Quality inspection #%d created.'), $insp_id));
			unset($_POST['new_stock_id'], $_POST['new_qty'], $_POST['new_notes']);
		}
	}
	$Ajax->activate('_page_body');
}

//----------------------------------------------------------------------
// Handle Enter Readings (inspection execution)
//----------------------------------------------------------------------
$enter_mode = get_post('enter_mode') || isset($_GET['enter']);
$active_insp_id = get_post('active_insp_id', isset($_GET['inspection_id']) ? (int)$_GET['inspection_id'] : 0);

if (get_post('save_readings')) {
	$active_insp_id = (int)get_post('active_insp_id');
	$insp = get_quality_inspection($active_insp_id);

	if ($insp) {
		// Get parameters for this item
		$params = get_quality_parameters_for_item($insp['stock_id']);
		$saved = 0;
		while ($param = db_fetch($params)) {
			$field_name = 'reading_' . $param['parameter_id'];
			$reading_val = get_post($field_name, '');

			if ($reading_val === '') continue;

			// Auto-evaluate
			$result = 'pass';
			switch ($param['parameter_type']) {
				case 'numeric':
					$result = evaluate_numeric_reading(
						(float)$reading_val,
						$param['min_value'],
						$param['max_value']
					);
					break;
				case 'boolean':
					$result = ($reading_val === '1' || strtolower($reading_val) === 'pass') ? 'pass' : 'fail';
					break;
				case 'list':
					$result = evaluate_list_reading($reading_val, $param['acceptable_values']);
					break;
				case 'text':
					$result = 'pass'; // Text parameters always pass (informational)
					break;
			}

			// Allow manual override via override field
			$override = get_post('override_' . $param['parameter_id'], '');
			if ($override === 'pass' || $override === 'fail')
				$result = $override;

			$notes = get_post('reading_notes_' . $param['parameter_id'], '');

			record_inspection_reading($active_insp_id, $param['parameter_id'],
				$reading_val, $result, $notes ? $notes : null);
			$saved++;
		}

		if ($saved > 0)
			display_notification(sprintf(_('%d reading(s) saved.'), $saved));
		else
			display_warning(_('No readings entered.'));
	}
	$enter_mode = true;
	$Ajax->activate('_page_body');
}

//----------------------------------------------------------------------
// Handle Complete Inspection
//----------------------------------------------------------------------
if (get_post('complete_inspection')) {
	$active_insp_id = (int)get_post('active_insp_id');
	$force_result = get_post('force_result', '');
	$accepted_qty = (float)get_post('accepted_qty', 0);
	$rejected_qty = (float)get_post('rejected_qty', 0);

	$ret = complete_quality_inspection($active_insp_id, $force_result, $accepted_qty, $rejected_qty);

	if ($ret['success']) {
		display_notification($ret['message']);
		$enter_mode = false;
		$active_insp_id = 0;
	} else {
		display_error($ret['message']);
		$enter_mode = true;
	}
	$Ajax->activate('_page_body');
}

//----------------------------------------------------------------------
// Handle Delete Inspection
//----------------------------------------------------------------------
if (get_post('delete_inspection')) {
	$del_id = (int)get_post('delete_inspection');
	if (can_delete_quality_inspection($del_id)) {
		delete_quality_inspection($del_id);
		display_notification(_('Inspection has been deleted.'));
	} else {
		display_error(_('Only pending inspections can be deleted.'));
	}
	$Ajax->activate('_page_body');
}

//----------------------------------------------------------------------
// Render: Inspection Execution Mode (enter readings)
//----------------------------------------------------------------------
if ($enter_mode && $active_insp_id > 0) {

	$insp = get_quality_inspection($active_insp_id);
	if (!$insp) {
		display_error(_('Inspection not found.'));
		$enter_mode = false;
	}
}

if ($enter_mode && $active_insp_id > 0 && $insp) {

	start_form();

	// Header info
	echo '<div style="margin:10px 0;padding:12px;background:#f0f7ff;border-left:4px solid #17a2b8;border-radius:4px;">';
	echo '<h3 style="margin:0 0 8px 0;">' . sprintf(_('Inspection #%d'), $insp['inspection_id'])
		. ' — ' . qc_result_badge($insp['result']) . '</h3>';
	echo '<table style="border-collapse:collapse;">';
	echo '<tr><td style="padding:2px 15px 2px 0;font-weight:600;">' . _('Item') . ':</td>';
	echo '<td>' . $insp['stock_id'] . ' — ' . $insp['item_description'] . '</td></tr>';
	echo '<tr><td style="padding:2px 15px 2px 0;font-weight:600;">' . _('Type') . ':</td>';
	echo '<td>' . qc_type_badge($insp['inspection_type']) . '</td></tr>';
	echo '<tr><td style="padding:2px 15px 2px 0;font-weight:600;">' . _('Quantity') . ':</td>';
	echo '<td>' . number_format2($insp['inspected_qty'], get_qty_dec($insp['stock_id'])) . ' ' . $insp['units'] . '</td></tr>';
	if ($insp['batch_no'])
		echo '<tr><td style="padding:2px 15px 2px 0;font-weight:600;">' . _('Batch') . ':</td><td>' . $insp['batch_no'] . '</td></tr>';
	if ($insp['serial_no'])
		echo '<tr><td style="padding:2px 15px 2px 0;font-weight:600;">' . _('Serial') . ':</td><td>' . $insp['serial_no'] . '</td></tr>';
	if ($insp['trans_type'] == ST_SUPPRECEIVE)
		echo '<tr><td style="padding:2px 15px 2px 0;font-weight:600;">' . _('GRN') . ':</td><td>#' . $insp['trans_no'] . '</td></tr>';
	echo '<tr><td style="padding:2px 15px 2px 0;font-weight:600;">' . _('Inspector') . ':</td><td>' . $insp['inspector_name'] . '</td></tr>';
	echo '<tr><td style="padding:2px 15px 2px 0;font-weight:600;">' . _('Date') . ':</td><td>' . $insp['inspection_date'] . '</td></tr>';
	echo '</table></div>';

	// Get existing readings
	$existing_readings = array();
	$readings_result = get_inspection_readings($active_insp_id);
	while ($r = db_fetch($readings_result))
		$existing_readings[$r['parameter_id']] = $r;

	// Reading counts
	$counts = count_inspection_readings($active_insp_id);

	// Get parameters for this item
	$params = get_quality_parameters_for_item($insp['stock_id']);
	$param_list = array();
	while ($p = db_fetch($params))
		$param_list[] = $p;

	if (empty($param_list)) {
		display_warning(sprintf(
			_('No quality parameters defined for item %s. Please <a href="%s">add parameters</a> first.'),
			$insp['stock_id'],
			$path_to_root . '/inventory/manage/quality_parameters.php'
		));
	}

	// Summary
	if ($counts['total'] > 0) {
		echo '<div style="margin:10px 0;display:flex;gap:15px;flex-wrap:wrap;">';

		echo '<div style="padding:8px 15px;background:#e8f5e9;border-radius:4px;border-left:3px solid #28a745;">';
		echo '<strong>' . _('Pass') . ':</strong> ' . $counts['pass'] . '</div>';

		echo '<div style="padding:8px 15px;background:#ffebee;border-radius:4px;border-left:3px solid #dc3545;">';
		echo '<strong>' . _('Fail') . ':</strong> ' . $counts['fail'] . '</div>';

		echo '<div style="padding:8px 15px;background:#f3f4f6;border-radius:4px;border-left:3px solid #6c757d;">';
		echo '<strong>' . _('Total') . ':</strong> ' . $counts['total'] . '/' . count($param_list) . '</div>';
		echo '</div>';
	}

	// Readings entry form
	if ($insp['result'] === 'pending' && !empty($param_list)) {

		hidden('active_insp_id', $active_insp_id);
		hidden('enter_mode', 1);

		start_table(TABLESTYLE, "width='95%'");
		$th = array(_('Parameter'), _('Type'), _('Range / Options'), _('Reading'), _('Result'), _('Override'), _('Notes'));
		table_header($th);

		$k = 0;
		foreach ($param_list as $param) {
			alt_table_row_color($k);

			// Parameter name + required indicator
			$name_html = $param['name'];
			if ($param['mandatory'])
				$name_html .= ' <span style="color:red;">*</span>';
			label_cell($name_html);

			label_cell(get_qc_parameter_type_label($param['parameter_type']));

			// Range / options
			if ($param['parameter_type'] === 'numeric') {
				$range = '';
				if ($param['min_value'] !== null)
					$range .= number_format2($param['min_value'], 4);
				$range .= ' — ';
				if ($param['max_value'] !== null)
					$range .= number_format2($param['max_value'], 4);
				if ($param['unit'])
					$range .= ' ' . $param['unit'];
				label_cell($range);
			} elseif ($param['parameter_type'] === 'list') {
				$vals = '';
				if ($param['acceptable_values']) {
					$decoded = html_entity_decode($param['acceptable_values'], ENT_QUOTES, 'UTF-8');
					$list = json_decode($decoded, true);
					if (is_array($list)) $vals = implode(', ', $list);
				}
				label_cell($vals);
			} elseif ($param['parameter_type'] === 'boolean') {
				label_cell(_('Pass / Fail'));
			} else {
				label_cell('—');
			}

			// Reading input
			$field_name = 'reading_' . $param['parameter_id'];
			$existing_val = isset($existing_readings[$param['parameter_id']])
				? $existing_readings[$param['parameter_id']]['reading_value'] : '';

			echo '<td>';
			if ($param['parameter_type'] === 'boolean') {
				// Pass/Fail dropdown
				echo '<select name="' . $field_name . '">';
				echo '<option value="">' . _('Select...') . '</option>';
				echo '<option value="1"' . ($existing_val === '1' ? ' selected' : '') . '>' . _('Pass') . '</option>';
				echo '<option value="0"' . ($existing_val === '0' ? ' selected' : '') . '>' . _('Fail') . '</option>';
				echo '</select>';
			} elseif ($param['parameter_type'] === 'list') {
				// Dropdown from acceptable values
				$decoded = html_entity_decode($param['acceptable_values'], ENT_QUOTES, 'UTF-8');
				$list = json_decode($decoded, true);
				echo '<select name="' . $field_name . '">';
				echo '<option value="">' . _('Select...') . '</option>';
				if (is_array($list)) {
					foreach ($list as $v) {
						$sel = (trim($existing_val) === trim($v)) ? ' selected' : '';
						echo '<option value="' . htmlspecialchars($v) . '"' . $sel . '>'
							. htmlspecialchars($v) . '</option>';
					}
				}
				echo '</select>';
			} else {
				// Text or numeric input
				$type_attr = ($param['parameter_type'] === 'numeric') ? 'number' : 'text';
				echo '<input type="' . $type_attr . '" name="' . $field_name . '" value="'
					. htmlspecialchars($existing_val) . '" size="20"'
					. ($type_attr === 'number' ? ' step="any"' : '') . '>';
			}
			echo '</td>';

			// Result column (from existing reading)
			if (isset($existing_readings[$param['parameter_id']])) {
				$r = $existing_readings[$param['parameter_id']];
				label_cell(qc_result_badge($r['result']));
			} else {
				label_cell('—');
			}

			// Override dropdown
			echo '<td>';
			$override_val = get_post('override_' . $param['parameter_id'], '');
			echo '<select name="override_' . $param['parameter_id'] . '">';
			echo '<option value="">' . _('Auto') . '</option>';
			echo '<option value="pass"' . ($override_val === 'pass' ? ' selected' : '') . '>' . _('Force Pass') . '</option>';
			echo '<option value="fail"' . ($override_val === 'fail' ? ' selected' : '') . '>' . _('Force Fail') . '</option>';
			echo '</select>';
			echo '</td>';

			// Notes
			echo '<td>';
			$note_val = get_post('reading_notes_' . $param['parameter_id'], '');
			echo '<input type="text" name="reading_notes_' . $param['parameter_id'] . '" value="'
				. htmlspecialchars($note_val) . '" size="25">';
			echo '</td>';

			end_row();
		}
		end_table(1);

		// Action buttons
		echo '<div style="text-align:center;margin:10px 0;">';
		submit('save_readings', _('Save Readings'), true, _('Save all entered readings'), 'default');
		echo '&nbsp;&nbsp;';

		// Complete inspection section
		echo '<fieldset style="display:inline-block;padding:8px 15px;border:1px solid #ddd;border-radius:4px;margin:0 10px;">';
		echo '<legend style="font-weight:600;font-size:12px;">' . _('Complete Inspection') . '</legend>';
		echo '<label>' . _('Force Result:') . ' </label>';
		echo '<select name="force_result">';
		echo '<option value="">' . _('Auto-determine') . '</option>';
		echo '<option value="pass">' . _('Pass') . '</option>';
		echo '<option value="fail">' . _('Fail') . '</option>';
		echo '<option value="partial">' . _('Partial') . '</option>';
		echo '</select>';
		echo '&nbsp;';
		echo '<label>' . _('Accepted Qty:') . ' </label>';
		echo '<input type="number" name="accepted_qty" value="0" step="any" size="8" style="width:70px;">';
		echo '&nbsp;';
		echo '<label>' . _('Rejected Qty:') . ' </label>';
		echo '<input type="number" name="rejected_qty" value="0" step="any" size="8" style="width:70px;">';
		echo '&nbsp;&nbsp;';
		submit('complete_inspection', _('Complete Inspection'), true, _('Finalize this inspection'), 'default');
		echo '</fieldset>';
		echo '</div>';

	} elseif ($insp['result'] !== 'pending') {
		// View completed readings
		$readings_result = get_inspection_readings($active_insp_id);
		$has_readings = false;

		start_table(TABLESTYLE, "width='95%'");
		$th = array(_('Parameter'), _('Type'), _('Reading'), _('Result'), _('Notes'));
		table_header($th);

		$k = 0;
		while ($r = db_fetch($readings_result)) {
			$has_readings = true;
			alt_table_row_color($k);
			label_cell($r['parameter_name']);
			label_cell(get_qc_parameter_type_label($r['parameter_type']));
			if ($r['parameter_type'] === 'boolean')
				label_cell($r['reading_value'] == '1' ? _('Pass') : _('Fail'));
			else
				label_cell($r['reading_value']);
			label_cell(qc_result_badge($r['result']));
			label_cell($r['notes'] ? $r['notes'] : '—');
			end_row();
		}

		if (!$has_readings) {
			label_row('', _('No readings recorded.'), 'colspan="5" style="text-align:center;"');
		}

		end_table(1);

		// Completion info
		if ($insp['completion_date']) {
			echo '<div style="margin:10px 0;padding:8px 15px;background:#f8f9fa;border-radius:4px;">';
			echo '<strong>' . _('Completed') . ':</strong> ' . $insp['completion_date'] . ' — ';
			echo '<strong>' . _('Accepted') . ':</strong> ' . number_format2($insp['accepted_qty'], get_qty_dec($insp['stock_id']));
			echo ' | <strong>' . _('Rejected') . ':</strong> ' . number_format2($insp['rejected_qty'], get_qty_dec($insp['stock_id']));
			echo '</div>';
		}
	}

	// Navigation links
	echo '<div style="text-align:center;margin:10px 0;">';
	echo '<a href="' . $_SERVER['PHP_SELF'] . '">' . _('Back to Inspection List') . '</a>';
	echo '&nbsp;&nbsp;|&nbsp;&nbsp;';
	echo '<a href="' . $path_to_root . '/inventory/view/view_quality_inspection.php?inspection_id='
		. $active_insp_id . '" target="_blank">' . _('View / Print') . '</a>';
	if ($insp['result'] !== 'pending') {
		echo '&nbsp;&nbsp;|&nbsp;&nbsp;';
		echo '<a href="' . $path_to_root . '/reporting/rep_quality_coa.php?inspection_id='
			. $active_insp_id . '" target="_blank">' . _('Certificate of Analysis (PDF)') . '</a>';
	}
	echo '</div>';

	end_form();
	end_page();
	exit;
}

//----------------------------------------------------------------------
// Main View: Inspection List with Filters
//----------------------------------------------------------------------

start_form();

// Summary cards
$summary = get_inspection_status_summary();

echo '<div style="display:flex;gap:15px;flex-wrap:wrap;margin:10px 0;">';

$cards = array(
	'pending' => array(_('Pending'), '#ffc107', 'fa-clock-o'),
	'pass'    => array(_('Passed'), '#28a745', 'fa-check-circle'),
	'fail'    => array(_('Failed'), '#dc3545', 'fa-times-circle'),
	'partial' => array(_('Partial'), '#fd7e14', 'fa-exclamation-circle'),
);

foreach ($cards as $key => $card) {
	$count = isset($summary[$key]) ? $summary[$key] : 0;
	echo '<div style="flex:1;min-width:120px;padding:12px 15px;background:#fff;border-radius:6px;'
		. 'border-left:4px solid ' . $card[1] . ';box-shadow:0 1px 3px rgba(0,0,0,.1);">';
	echo '<div style="font-size:22px;font-weight:700;color:' . $card[1] . ';">' . $count . '</div>';
	echo '<div style="font-size:12px;color:#666;">' . $card[0] . '</div>';
	echo '</div>';
}
echo '</div>';

// Filters
start_table(TABLESTYLE_NOBORDER);
start_row();

stock_items_list_cells(_('Item:'), 'filter_stock_id', null, _('All Items'), true);

$result_options = array('' => _('All Results')) + get_qc_result_statuses();
label_cell(_('Result:'));
echo '<td>';
echo array_selector('filter_result', null, $result_options,
	array('select_submit' => true));
echo '</td>';

$type_options = array('' => _('All Types')) + get_qc_inspection_types();
label_cell(_('Type:'));
echo '<td>';
echo array_selector('filter_type', null, $type_options,
	array('select_submit' => true));
echo '</td>';

submit_cells('search_btn', _('Search'), '', _('Apply filters'), 'default');

end_row();
end_table();

// Inspection List
div_start('insp_list');

$filters = array();
if (!empty($_POST['filter_stock_id']))
	$filters['stock_id'] = $_POST['filter_stock_id'];
if (!empty($_POST['filter_result']))
	$filters['result'] = $_POST['filter_result'];
if (!empty($_POST['filter_type']))
	$filters['inspection_type'] = $_POST['filter_type'];

$inspections = get_quality_inspections($filters, 50);

start_table(TABLESTYLE, "width='95%'");

$th = array(
	_('#'), _('Item'), _('Type'), _('Result'), _('Qty'),
	_('Accepted'), _('Rejected'), _('Batch'), _('Serial'),
	_('Source'), _('Inspector'), _('Date'), ''
);
table_header($th);

$k = 0;
while ($row = db_fetch($inspections)) {
	alt_table_row_color($k);

	// ID as link to enter/view mode
	$link = '<a href="' . $_SERVER['PHP_SELF'] . '?enter=1&inspection_id=' . $row['inspection_id'] . '">'
		. '#' . $row['inspection_id'] . '</a>';
	label_cell($link);

	label_cell($row['stock_id'] . ' — ' . $row['item_description']);
	label_cell(qc_type_badge($row['inspection_type']));
	label_cell(qc_result_badge($row['result']));
	qty_cell($row['inspected_qty'], false, get_qty_dec($row['stock_id']));
	qty_cell($row['accepted_qty'], false, get_qty_dec($row['stock_id']));
	qty_cell($row['rejected_qty'], false, get_qty_dec($row['stock_id']));
	label_cell($row['batch_no'] ? $row['batch_no'] : '—');
	label_cell($row['serial_no'] ? $row['serial_no'] : '—');

	// Source document link
	if ($row['trans_type'] == ST_SUPPRECEIVE) {
		label_cell('<a href="' . $path_to_root . '/purchasing/view/view_grn.php?trans_no='
			. $row['trans_no'] . '" target="_blank">GRN #' . $row['trans_no'] . '</a>');
	} else {
		label_cell('—');
	}

	label_cell($row['inspector_name'] ? $row['inspector_name'] : '—');
	label_cell(sql2date(substr($row['inspection_date'], 0, 10)));

	// Action buttons
	echo '<td>';
	echo '<a href="' . $_SERVER['PHP_SELF'] . '?enter=1&inspection_id=' . $row['inspection_id'] . '">'
		. ($row['result'] === 'pending' ? _('Enter Readings') : _('View')) . '</a>';
	if ($row['result'] !== 'pending') {
		echo '&nbsp;|&nbsp;<a href="' . $path_to_root . '/reporting/rep_quality_coa.php?inspection_id='
			. $row['inspection_id'] . '" target="_blank">' . _('COA') . '</a>';
	}
	if (can_delete_quality_inspection($row['inspection_id'])) {
		echo '&nbsp;|&nbsp;';
		echo '<button type="submit" name="delete_inspection" value="' . $row['inspection_id']
			. '" style="background:none;border:none;color:#dc3545;cursor:pointer;padding:0;font-size:inherit;">'
			. _('Delete') . '</button>';
	}
	echo '</td>';

	end_row();
}

end_table(1);

div_end();

//----------------------------------------------------------------------
// Create Inspection Section
//----------------------------------------------------------------------
echo '<fieldset style="margin:10px 0;padding:10px 15px;border:1px solid #ddd;border-radius:4px;">';
echo '<legend style="font-weight:600;">' . _('Create New Inspection') . '</legend>';

start_table(TABLESTYLE2);

echo '<tr><td class="label">' . _('Item') . ':</td><td>';
echo stock_items_list('new_stock_id', null, false, false);
echo '</td></tr>';

$insp_types = get_qc_inspection_types();
array_selector_row(_('Inspection Type:'), 'new_insp_type', null, $insp_types);

text_row_ex(_('Quantity:'), 'new_qty', 15, 15);

locations_list_row(_('Location:'), 'new_loc_code', null, false);

text_row_ex(_('Batch ID:'), 'new_batch_id', 10, 10);
text_row_ex(_('Serial ID:'), 'new_serial_id', 10, 10);
textarea_row(_('Notes:'), 'new_notes', null, 50, 2);

end_table(0);

echo '<div style="text-align:center;margin:8px 0;">';
submit('create_inspection', _('Create Inspection'), true, _('Create a new quality inspection'), 'default');
echo '</div>';

echo '</fieldset>';

//----------------------------------------------------------------------
// Auto-create from GRN Section
//----------------------------------------------------------------------
echo '<fieldset style="margin:10px 0;padding:10px 15px;border:1px solid #ddd;border-radius:4px;">';
echo '<legend style="font-weight:600;">' . _('Auto-Create from GRN') . '</legend>';

start_table(TABLESTYLE2);

text_row_ex(_('GRN Number:'), 'grn_no_input', 10, 10);
locations_list_row(_('Warehouse Location:'), 'grn_loc_code', null, false);

end_table(0);

echo '<div style="text-align:center;margin:8px 0;">';
submit('auto_create_grn', _('Create Inspections from GRN'), true,
	_('Auto-create inspections for QC-required items in this GRN'), 'default');
echo '</div>';

echo '</fieldset>';

end_form();
end_page();
