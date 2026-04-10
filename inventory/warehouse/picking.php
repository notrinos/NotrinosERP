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
 * Picking Operations — Wave Management + Pick List UI.
 *
 * Create, release, start, complete, and cancel picking waves.
 * View pick lists sorted by walking path. Per-line pick confirmation
 * with short-pick handling and alternate bin suggestions.
 *
 * @package NotrinosERP
 * @subpackage Inventory/Warehouse
 */

$page_security = 'SA_WAREHOUSE_PICKING';
$path_to_root = '../..';

include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/includes/data_checks.inc');
include_once($path_to_root . '/inventory/warehouse/includes/db/warehouse_picking_db.inc');
include_once($path_to_root . '/inventory/warehouse/includes/db/warehouse_locations_db.inc');

$js = '';
if (user_use_date_picker())
	$js .= get_js_date_picker();

page(_($help_context = 'Picking Operations'), false, false, '', $js);

simple_page_mode(true);

// =====================================================================
// Process wave lifecycle actions
// =====================================================================

if (isset($_POST['ReleaseWave'])) {
	$wid = (int)$_POST['ReleaseWave'];
	$result = release_picking_wave($wid);
	if ($result['success'])
		display_notification($result['message']);
	else
		display_error($result['message']);
	$Ajax->activate('_page_body');
}

if (isset($_POST['StartWave'])) {
	$wid = (int)$_POST['StartWave'];
	if (start_picking_wave($wid))
		display_notification(sprintf(_('Wave #%d started — pick operations now in progress.'), $wid));
	else
		display_error(_('Could not start wave.'));
	$Ajax->activate('_page_body');
}

if (isset($_POST['CompleteWave'])) {
	$wid = (int)$_POST['CompleteWave'];
	if (complete_picking_wave($wid))
		display_notification(sprintf(_('Wave #%d completed.'), $wid));
	else
		display_error(_('Could not complete wave.'));
	$Ajax->activate('_page_body');
}

if (isset($_POST['CancelWave'])) {
	$wid = (int)$_POST['CancelWave'];
	if (cancel_picking_wave($wid))
		display_notification(sprintf(_('Wave #%d cancelled.'), $wid));
	else
		display_error(_('Could not cancel wave — may already be done.'));
	$Ajax->activate('_page_body');
}

// =====================================================================
// Process pick line confirmation (short pick / full confirm)
// =====================================================================

if (isset($_POST['ConfirmPickLine'])) {
	$line_id = (int)$_POST['ConfirmPickLine'];
	$qty_picked = (float)$_POST['qty_picked_' . $line_id];
	$result = confirm_pick_line($line_id, $qty_picked);
	if ($result['success']) {
		display_notification($result['message']);
		if ($result['shortfall'] > 0 && !empty($result['alternate_bins'])) {
			$alt_msg = _('Alternate bins:') . ' ';
			$parts = array();
			foreach ($result['alternate_bins'] as $ab) {
				$parts[] = $ab['bin_code'] . ' (' . $ab['bin_name'] . ') — ' . number_format2($ab['qty_available'], 2) . ' avail';
			}
			$alt_msg .= implode(', ', $parts);
			display_notification($alt_msg);
		}
	} else {
		display_error($result['message']);
	}
	$Ajax->activate('_page_body');
}

// =====================================================================
// CRUD handlers: Add / Update / Delete wave
// =====================================================================

if ($Mode == 'ADD_ITEM' || $Mode == 'UPDATE_ITEM') {

	$wave_name = $_POST['wave_name'];
	$warehouse_loc_code = $_POST['warehouse_loc_code'];
	$picking_method = $_POST['picking_method'];
	$wave_type = $_POST['wave_type'];
	$assigned_to = !empty($_POST['assigned_to']) ? (int)$_POST['assigned_to'] : null;
	$memo = $_POST['memo'];

	if (strlen($wave_name) == 0) {
		display_error(_('Wave name cannot be empty.'));
	} elseif (empty($warehouse_loc_code)) {
		display_error(_('Please select a warehouse.'));
	} else {
		if ($Mode == 'ADD_ITEM') {
			$new_id = add_picking_wave($wave_name, $warehouse_loc_code, $picking_method, $wave_type, $assigned_to, $memo);
			display_notification(sprintf(_('Picking wave #%d created.'), $new_id));
		} else {
			update_picking_wave($selected_id, $wave_name, $warehouse_loc_code, $picking_method, $wave_type, $assigned_to, $memo);
			display_notification(sprintf(_('Picking wave #%d updated.'), $selected_id));
		}
		$Mode = 'RESET';
	}
}

if ($Mode == 'Delete') {
	$check = can_delete_picking_wave($selected_id);
	if ($check === true) {
		delete_picking_wave($selected_id);
		display_notification(_('Picking wave deleted.'));
	} else {
		display_error($check);
	}
	$Mode = 'RESET';
}

if ($Mode == 'RESET') {
	$selected_id = -1;
	unset($_POST['wave_name']);
	unset($_POST['warehouse_loc_code']);
	unset($_POST['picking_method']);
	unset($_POST['wave_type']);
	unset($_POST['assigned_to']);
	unset($_POST['memo']);
}

// =====================================================================
// Filter form
// =====================================================================

start_form();

start_table(TABLESTYLE_NOBORDER);
start_row();

// Warehouse filter
$sql_locs = "SELECT loc_code, location_name FROM " . TB_PREF . "locations WHERE wh_enabled = 1 ORDER BY location_name";
echo '<td>' . _('Warehouse:') . '</td><td>';
echo combo_input('filter_location', get_post('filter_location'), $sql_locs, 'loc_code', 'location_name',
	array('spec_option' => _('All Warehouses'), 'spec_id' => '', 'select_submit' => true, 'order' => false));
echo '</td>';

// Status filter
$status_options = array(
	'active' => _('All Active'),
	'draft'  => _('Draft'),
	'released' => _('Released'),
	'in_progress' => _('In Progress'),
	'done'   => _('Done'),
	'cancelled' => _('Cancelled'),
);
echo '<td>' . _('Status:') . '</td><td>';
echo array_selector('filter_status', get_post('filter_status', 'active'), $status_options, array('select_submit' => true));
echo '</td>';

// Method filter
$method_options = array_merge(array('' => _('All Methods')), get_picking_methods());
echo '<td>' . _('Method:') . '</td><td>';
echo array_selector('filter_method', get_post('filter_method'), $method_options, array('select_submit' => true));
echo '</td>';

end_row();
end_table();

// =====================================================================
// Summary Cards
// =====================================================================

$filter_loc = get_post('filter_location');
$summary = get_wave_status_summary($filter_loc ?: null);

echo '<div style="display:flex;gap:16px;margin:12px 0;">';

// Draft waves
echo '<div style="flex:1;padding:12px 16px;border-radius:6px;background:#f5f5f5;border:1px solid #bdbdbd;">';
echo '<div style="font-size:24px;font-weight:bold;color:#616161;">' . $summary['draft'] . '</div>';
echo '<div style="font-size:12px;color:#616161;">' . _('Draft Waves') . '</div>';
echo '</div>';

// Released waves
echo '<div style="flex:1;padding:12px 16px;border-radius:6px;background:#e3f2fd;border:1px solid #90caf9;">';
echo '<div style="font-size:24px;font-weight:bold;color:#1565c0;">' . $summary['released'] . '</div>';
echo '<div style="font-size:12px;color:#1565c0;">' . _('Released') . '</div>';
echo '</div>';

// In Progress waves
echo '<div style="flex:1;padding:12px 16px;border-radius:6px;background:#fff3e0;border:1px solid #ffcc80;">';
echo '<div style="font-size:24px;font-weight:bold;color:#e65100;">' . $summary['in_progress'] . '</div>';
echo '<div style="font-size:12px;color:#e65100;">' . _('In Progress') . '</div>';
echo '</div>';

// Done waves
echo '<div style="flex:1;padding:12px 16px;border-radius:6px;background:#e8f5e9;border:1px solid #a5d6a7;">';
echo '<div style="font-size:24px;font-weight:bold;color:#2e7d32;">' . $summary['done'] . '</div>';
echo '<div style="font-size:12px;color:#2e7d32;">' . _('Completed') . '</div>';
echo '</div>';

echo '</div>';

// =====================================================================
// Waves List Table
// =====================================================================

$filters = array();
$filter_status = get_post('filter_status', 'active');

if ($filter_status === 'active')
	$filters['status'] = array('draft', 'released', 'in_progress');
elseif ($filter_status)
	$filters['status'] = $filter_status;

if ($filter_loc)
	$filters['warehouse_loc_code'] = $filter_loc;

$filter_method = get_post('filter_method');
if ($filter_method)
	$filters['picking_method'] = $filter_method;

if ($filter_status === 'cancelled' || $filter_status === 'done')
	$filters['show_inactive'] = true;

$waves = get_picking_waves($filters, 100);

div_start('wave_list');

display_heading2(_('Picking Waves'));

start_table(TABLESTYLE, "width='100%'");
$th = array('#', _('Wave Name'), _('Warehouse'), _('Method'), _('Type'), _('Status'),
	_('Orders'), _('Lines'), _('Progress'), _('Assigned'), _('Created'), _('Actions'));
table_header($th);

$k = 0;
$has_rows = false;
while ($row = db_fetch($waves)) {
	$has_rows = true;
	alt_table_row_color($k);

	// Wave ID
	label_cell($row['wave_id']);

	// Name (bold link)
	echo '<td><b>' . htmlspecialchars($row['wave_name']) . '</b></td>';

	// Warehouse
	label_cell($row['warehouse_name'] ? $row['warehouse_name'] : $row['warehouse_loc_code']);

	// Method badge
	$methods = get_picking_methods();
	$method_label = isset($methods[$row['picking_method']]) ? $methods[$row['picking_method']] : $row['picking_method'];
	echo '<td><span style="background:#455a64;color:#fff;padding:2px 8px;border-radius:10px;font-size:11px;">'
		. htmlspecialchars($method_label) . '</span></td>';

	// Type badge
	$type_color = get_wave_type_color($row['wave_type']);
	$types = get_wave_types();
	$type_label = isset($types[$row['wave_type']]) ? $types[$row['wave_type']] : $row['wave_type'];
	echo '<td><span style="background:' . $type_color . ';color:#fff;padding:2px 8px;border-radius:10px;font-size:11px;">'
		. htmlspecialchars($type_label) . '</span></td>';

	// Status badge
	echo '<td>' . wave_status_badge($row['status']) . '</td>';

	// Orders / Lines
	label_cell($row['total_orders']);
	label_cell($row['total_lines']);

	// Progress bar
	if ($row['status'] !== 'draft') {
		$progress = get_wave_progress($row['wave_id']);
		$pct = $progress['pct_complete'];
		$pct_color = $pct >= 100 ? '#28a745' : ($pct >= 50 ? '#ffc107' : '#dc3545');
		echo '<td>';
		echo '<div style="width:80px;height:16px;background:#eee;border-radius:8px;overflow:hidden;display:inline-block;vertical-align:middle;">';
		echo '<div style="width:' . $pct . '%;height:100%;background:' . $pct_color . ';"></div>';
		echo '</div> ' . $pct . '%';
		echo '</td>';
	} else {
		label_cell('-');
	}

	// Assigned
	if (!empty($row['assigned_to'])) {
		$user_sql = "SELECT real_name FROM " . TB_PREF . "users WHERE id=" . (int)$row['assigned_to'];
		$user_result = db_query($user_sql, 'could not get user');
		$user = db_fetch($user_result);
		label_cell($user ? $user['real_name'] : '#' . $row['assigned_to']);
	} else {
		label_cell('-');
	}

	// Created
	$created = !empty($row['created_date']) ? sql2date(substr($row['created_date'], 0, 10)) : '-';
	label_cell($created);

	// Actions
	echo '<td nowrap>';
	$status = $row['status'];
	$wid = $row['wave_id'];

	if ($status === 'draft') {
		// Edit / Delete / Release
		edit_button_cell("Edit$wid", _('Edit'));
		echo ' ';
		delete_button_cell("Delete$wid", _('Delete'));
		echo ' ';
		echo '<button type="submit" name="ReleaseWave" value="' . $wid . '" class="ajaxsubmit" '
			. 'style="padding:2px 8px;background:#007bff;color:#fff;border:none;border-radius:3px;cursor:pointer;">'
			. _('Release') . '</button>';
	} elseif ($status === 'released') {
		// Start / View Pick List / Cancel
		echo '<button type="submit" name="StartWave" value="' . $wid . '" class="ajaxsubmit" '
			. 'style="padding:2px 8px;background:#1976d2;color:#fff;border:none;border-radius:3px;cursor:pointer;">'
			. _('Start') . '</button> ';
		echo '<a href="' . $path_to_root . '/inventory/warehouse/view/view_pick_list.php?wave_id=' . $wid . '" target="_blank" '
			. 'style="padding:2px 8px;background:#455a64;color:#fff;border-radius:3px;text-decoration:none;font-size:12px;">'
			. _('Pick List') . '</a> ';
		echo '<button type="submit" name="CancelWave" value="' . $wid . '" class="ajaxsubmit" '
			. 'style="padding:2px 8px;background:#d32f2f;color:#fff;border:none;border-radius:3px;cursor:pointer;">'
			. _('Cancel') . '</button>';
	} elseif ($status === 'in_progress') {
		// Complete / View Pick List / Cancel
		echo '<button type="submit" name="CompleteWave" value="' . $wid . '" class="ajaxsubmit" '
			. 'style="padding:2px 8px;background:#388e3c;color:#fff;border:none;border-radius:3px;cursor:pointer;">'
			. _('Complete') . '</button> ';
		echo '<a href="' . $path_to_root . '/inventory/warehouse/view/view_pick_list.php?wave_id=' . $wid . '" target="_blank" '
			. 'style="padding:2px 8px;background:#455a64;color:#fff;border-radius:3px;text-decoration:none;font-size:12px;">'
			. _('Pick List') . '</a> ';
		echo '<button type="submit" name="CancelWave" value="' . $wid . '" class="ajaxsubmit" '
			. 'style="padding:2px 8px;background:#d32f2f;color:#fff;border:none;border-radius:3px;cursor:pointer;">'
			. _('Cancel') . '</button>';
	} else {
		echo '-';
	}
	echo '</td>';

	end_row();

	// Expandable: show pick operations for released/in_progress/done waves
	if (in_array($status, array('released', 'in_progress', 'done'))) {
		$ops = get_wave_pick_operations($wid);
		if (db_num_rows($ops) > 0) {
			echo '<tr class="OddTableRow"><td></td><td colspan="11" style="padding:4px 12px;background:#fafafa;font-size:11px;">';
			echo '<b>' . _('Pick Operations:') . '</b>';
			echo '<table style="width:100%;border-collapse:collapse;font-size:11px;margin-top:4px;">';
			echo '<tr style="background:#eee;"><th>' . _('Op #') . '</th><th>' . _('Status') . '</th><th>' . _('Source') . '</th><th>' . _('Memo') . '</th><th>' . _('Items') . '</th></tr>';
			while ($op = db_fetch($ops)) {
				$op_status_color = get_wh_operation_status_color($op['op_status']);
				echo '<tr>';
				echo '<td style="padding:2px 4px;">' . $op['op_id'] . '</td>';
				echo '<td style="padding:2px 4px;"><span style="background:' . $op_status_color . ';color:#fff;padding:1px 6px;border-radius:8px;font-size:10px;">'
					. strtoupper($op['op_status']) . '</span></td>';
				$source = !empty($op['source_doc_no']) ? 'DN #' . $op['source_doc_no'] : '-';
				echo '<td style="padding:2px 4px;">' . $source . '</td>';
				echo '<td style="padding:2px 4px;">' . htmlspecialchars($op['memo']) . '</td>';
				// Count lines
				$lines_sql = "SELECT COUNT(*) FROM " . TB_PREF . "wh_operation_lines WHERE op_id=" . (int)$op['op_id'];
				$lines_res = db_query($lines_sql, 'count lines');
				$lines_row = db_fetch_row($lines_res);
				echo '<td style="padding:2px 4px;">' . $lines_row[0] . ' lines</td>';
				echo '</tr>';
			}
			echo '</table>';
			echo '</td></tr>';
		}
	}
}

if (!$has_rows) {
	label_row('', _('No picking waves found matching the selected filters.'), 'colspan=12 align=center');
}

end_table(1);
div_end();

// =====================================================================
// Wave Add / Edit Form
// =====================================================================

div_start('wave_form');

$is_editing = ($selected_id != -1 && $Mode == 'Edit');

if ($is_editing) {
	$wave = get_picking_wave($selected_id);
	if ($wave) {
		$_POST['wave_name'] = $wave['wave_name'];
		$_POST['warehouse_loc_code'] = $wave['warehouse_loc_code'];
		$_POST['picking_method'] = $wave['picking_method'];
		$_POST['wave_type'] = $wave['wave_type'];
		$_POST['assigned_to'] = $wave['assigned_to'];
		$_POST['memo'] = $wave['memo'];
	}
}

display_heading2($selected_id == -1 ? _('New Picking Wave') : _('Edit Picking Wave'));

start_table(TABLESTYLE2);

text_row(_('Wave Name:'), 'wave_name', null, 50, 60);

// Warehouse selector (WMS-enabled only)
$sql_wh = "SELECT loc_code, location_name FROM " . TB_PREF . "locations WHERE wh_enabled = 1 ORDER BY location_name";
label_row(_('Warehouse:'), combo_input('warehouse_loc_code', get_post('warehouse_loc_code'), $sql_wh,
	'loc_code', 'location_name', array('order' => false)));

// Picking method
label_row(_('Picking Method:'), array_selector('picking_method', get_post('picking_method', 'single'), get_picking_methods()));

// Wave type
label_row(_('Wave Type:'), array_selector('wave_type', get_post('wave_type', 'standard'), get_wave_types()));

// Assigned to (user selector)
$sql_users = "SELECT id, real_name FROM " . TB_PREF . "users WHERE inactive = 0 ORDER BY real_name";
label_row(_('Assigned To:'), combo_input('assigned_to', get_post('assigned_to'), $sql_users,
	'id', 'real_name', array('spec_option' => _('Unassigned'), 'spec_id' => '', 'order' => false)));

textarea_row(_('Notes:'), 'memo', null, 50, 3);

end_table(1);

submit_add_or_update_center($selected_id == -1, '', 'both');

div_end();

// =====================================================================
// Pick List (inline) when viewing a released/in_progress wave
// =====================================================================

if (isset($_GET['view_wave']) || isset($_POST['ViewPickList'])) {
	$view_wave_id = isset($_GET['view_wave']) ? (int)$_GET['view_wave'] : (int)$_POST['ViewPickList'];
	$pick_lines = generate_pick_list($view_wave_id);

	if (!empty($pick_lines)) {
		div_start('pick_list_view');
		display_heading2(sprintf(_('Pick List — Wave #%d'), $view_wave_id));

		echo '<div style="margin:8px 0;">';
		echo '<a href="' . $path_to_root . '/inventory/warehouse/view/view_pick_list.php?wave_id=' . $view_wave_id . '" target="_blank" '
			. 'style="padding:4px 12px;background:#455a64;color:#fff;border-radius:4px;text-decoration:none;font-size:12px;">'
			. '<i class="fa fa-print"></i> ' . _('Print Pick List') . '</a>';
		echo '</div>';

		start_table(TABLESTYLE, "width='100%'");
		$th = array(_('#'), _('Bin Code'), _('Bin Name'), _('Zone'), _('Pick Seq'),
			_('Item Code'), _('Description'), _('Qty to Pick'), _('Qty Picked'),
			_('Batch'), _('Serial'), _('Order #'), _('Status'), _('Confirm'));
		table_header($th);

		$k = 0;
		$seq = 1;
		foreach ($pick_lines as $pl) {
			alt_table_row_color($k);

			// Sequence number
			label_cell($seq++);

			// Bin code (bold)
			echo '<td><b>' . htmlspecialchars($pl['bin_code'] ?: '-') . '</b></td>';

			// Bin name
			label_cell($pl['bin_name'] ?: '-');

			// Zone type
			label_cell($pl['zone_type'] ?: '-');

			// Pick sequence
			label_cell($pl['pick_sequence'] !== null ? $pl['pick_sequence'] : '-');

			// Item code
			label_cell($pl['stock_id']);

			// Item description
			label_cell($pl['item_description']);

			// Qty to pick
			$planned = (float)$pl['qty_planned'];
			echo '<td align="right"><b>' . number_format2($planned, 2) . '</b></td>';

			// Qty picked
			$done = (float)$pl['qty_done'];
			if ($done >= $planned) {
				echo '<td align="right" style="color:#28a745;font-weight:bold;">' . number_format2($done, 2) . '</td>';
			} elseif ($done > 0) {
				echo '<td align="right" style="color:#e65100;">' . number_format2($done, 2) . '</td>';
			} else {
				echo '<td align="right">' . number_format2($done, 2) . '</td>';
			}

			// Batch
			label_cell(!empty($pl['batch_no']) ? $pl['batch_no'] . (!empty($pl['batch_expiry']) ? ' (exp: ' . sql2date($pl['batch_expiry']) . ')' : '') : '-');

			// Serial
			label_cell(!empty($pl['serial_no']) ? $pl['serial_no'] : '-');

			// Order reference
			$order_ref = !empty($pl['delivery_no']) ? 'DN #' . $pl['delivery_no'] : '-';
			label_cell($order_ref);

			// Status badge
			$op_status_color = get_wh_operation_status_color($pl['op_status']);
			echo '<td><span style="background:' . $op_status_color . ';color:#fff;padding:1px 6px;border-radius:8px;font-size:10px;">'
				. strtoupper($pl['op_status']) . '</span></td>';

			// Confirm pick (only for non-completed lines)
			echo '<td nowrap>';
			if ($done < $planned && $pl['op_status'] !== 'done' && $pl['op_status'] !== 'cancelled') {
				echo '<input type="number" name="qty_picked_' . $pl['line_id'] . '" value="' . number_format2($planned, 2) . '" '
					. 'step="0.01" min="0" max="' . number_format2($planned, 2) . '" style="width:60px;font-size:11px;"> ';
				echo '<button type="submit" name="ConfirmPickLine" value="' . $pl['line_id'] . '" class="ajaxsubmit" '
					. 'style="padding:1px 6px;background:#1976d2;color:#fff;border:none;border-radius:3px;cursor:pointer;font-size:11px;">'
					. _('Pick') . '</button>';
			} else {
				echo '<span style="color:#28a745;">&#10003;</span>';
			}
			echo '</td>';

			end_row();
		}

		end_table(1);
		div_end();
	}
}

end_form();

end_page();
