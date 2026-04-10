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
 * Packing Station — Package management and pack confirmation UI.
 *
 * Features:
 * - Package CRUD with status badges and type badges
 * - Ready-for-packing pick operations list
 * - Create pack operation from completed picks
 * - Assign items to packages, seal packages
 * - View package contents
 *
 * @package NotrinosERP
 * @subpackage Inventory/Warehouse
 */

$page_security = 'SA_WAREHOUSE_PACKING';
$path_to_root = '../..';

include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/includes/data_checks.inc');
include_once($path_to_root . '/inventory/warehouse/includes/db/warehouse_packing_db.inc');
include_once($path_to_root . '/inventory/warehouse/includes/db/warehouse_locations_db.inc');

$js = '';
if (user_use_date_picker())
	$js .= get_js_date_picker();

page(_($help_context = 'Packing Station'), false, false, '', $js);

simple_page_mode(true);

// =====================================================================
// Process Actions
// =====================================================================

// --- Create Pack Operation from Pick ---
if (isset($_POST['CreatePackOp'])) {
	$pick_op_id = (int)$_POST['CreatePackOp'];
	$pack_op_id = create_pack_operation($pick_op_id);
	if ($pack_op_id > 0) {
		display_notification(sprintf(_('Pack operation #%d created from pick operation #%d.'), $pack_op_id, $pick_op_id));
		$_POST['view_pack_op'] = $pack_op_id;
	} else {
		display_error(_('Could not create pack operation.'));
	}
	$Ajax->activate('_page_body');
}

// --- Pack a line into a package ---
if (isset($_POST['PackLine'])) {
	$line_id = (int)$_POST['PackLine'];
	$pkg_id = (int)get_post('pack_into_' . $line_id);
	$pack_qty = (float)get_post('pack_qty_' . $line_id);

	if ($pkg_id <= 0) {
		display_error(_('Please select a package to pack into.'));
	} elseif ($pack_qty <= 0) {
		display_error(_('Pack quantity must be greater than zero.'));
	} else {
		if (confirm_pack_line($line_id, $pkg_id, $pack_qty))
			display_notification(_('Item packed successfully.'));
		else
			display_error(_('Could not pack item. Check that the package is open.'));
	}
	$Ajax->activate('_page_body');
}

// --- Complete Pack Operation ---
if (isset($_POST['CompletePack'])) {
	$pack_op_id = (int)$_POST['CompletePack'];
	$result = complete_pack_operation($pack_op_id);
	if ($result['success'])
		display_notification($result['message']);
	else
		display_error($result['message']);
	$Ajax->activate('_page_body');
}

// --- Seal Package ---
if (isset($_POST['SealPackage'])) {
	$pkg_id = (int)$_POST['SealPackage'];
	$result = seal_package($pkg_id);
	if ($result['success'])
		display_notification($result['message']);
	else
		display_error($result['message']);
	$Ajax->activate('_page_body');
}

// --- Unseal Package ---
if (isset($_POST['UnsealPackage'])) {
	$pkg_id = (int)$_POST['UnsealPackage'];
	$result = unseal_package($pkg_id);
	if ($result['success'])
		display_notification($result['message']);
	else
		display_error($result['message']);
	$Ajax->activate('_page_body');
}

// --- Cancel Package ---
if (isset($_POST['CancelPackage'])) {
	$pkg_id = (int)$_POST['CancelPackage'];
	$result = cancel_package($pkg_id);
	if ($result['success'])
		display_notification($result['message']);
	else
		display_error($result['message']);
	$Ajax->activate('_page_body');
}

// --- Add Package ---
if (isset($_POST['ADD_ITEM'])) {
	$pkg_code = trim(get_post('package_code'));
	$pkg_type = get_post('package_type', 'box');
	$weight = get_post('pkg_weight') !== '' ? (float)get_post('pkg_weight') : null;
	$length = get_post('pkg_length') !== '' ? (float)get_post('pkg_length') : null;
	$width = get_post('pkg_width') !== '' ? (float)get_post('pkg_width') : null;
	$height = get_post('pkg_height') !== '' ? (float)get_post('pkg_height') : null;

	if (!empty($pkg_code) && !is_package_code_unique($pkg_code)) {
		display_error(_('Package code already exists.'));
	} else {
		$new_id = add_package($pkg_code, $pkg_type, null, $weight, $length, $width, $height);
		display_notification(sprintf(_('Package #%d created.'), $new_id));
		$Mode = 'RESET';
	}
	$Ajax->activate('_page_body');
}

// --- Update Package ---
if (isset($_POST['UPDATE_ITEM'])) {
	$pkg_id = (int)$_POST['selected_id'];
	$pkg_code = trim(get_post('package_code'));
	$pkg_type = get_post('package_type', 'box');
	$weight = get_post('pkg_weight') !== '' ? (float)get_post('pkg_weight') : null;
	$length = get_post('pkg_length') !== '' ? (float)get_post('pkg_length') : null;
	$width = get_post('pkg_width') !== '' ? (float)get_post('pkg_width') : null;
	$height = get_post('pkg_height') !== '' ? (float)get_post('pkg_height') : null;
	$carrier = trim(get_post('carrier'));
	$tracking = trim(get_post('tracking_number'));

	if (empty($pkg_code)) {
		display_error(_('Package code cannot be empty.'));
	} elseif (!is_package_code_unique($pkg_code, $pkg_id)) {
		display_error(_('Package code already exists.'));
	} else {
		update_package($pkg_id, $pkg_code, $pkg_type, null, $weight, $length, $width, $height, $carrier, $tracking);
		display_notification(_('Package has been updated.'));
		$Mode = 'RESET';
	}
	$Ajax->activate('_page_body');
}

// --- Delete Package ---
if ($Mode == 'Delete') {
	if (can_delete_package($selected_id)) {
		delete_package($selected_id);
		display_notification(_('Package has been deleted.'));
	} else {
		display_error(_('Cannot delete this package — it has contents or is not open.'));
	}
	$Mode = 'RESET';
}

if ($Mode == 'RESET') {
	$selected_id = -1;
	unset($_POST['package_code'], $_POST['package_type'], $_POST['pkg_weight'],
		$_POST['pkg_length'], $_POST['pkg_width'], $_POST['pkg_height'],
		$_POST['carrier'], $_POST['tracking_number']);
}

// =====================================================================
// Filter Bar
// =====================================================================

start_form();

start_table(TABLESTYLE_NOBORDER);
start_row();

// Warehouse filter
$sql_locs = "SELECT loc_code, location_name FROM " . TB_PREF . "locations WHERE wh_enabled = 1 ORDER BY location_name";
echo '<td>' . _('Warehouse:') . '</td><td>';
echo combo_input('filter_warehouse', get_post('filter_warehouse'), $sql_locs, 'loc_code', 'location_name',
	array('spec_option' => _('All Warehouses'), 'spec_id' => '', 'select_submit' => true, 'order' => false));
echo '</td>';

// Status filter
$status_options = array(
	''          => _('All Active'),
	'open'      => _('Open'),
	'sealed'    => _('Sealed'),
	'shipped'   => _('Shipped'),
	'delivered' => _('Delivered'),
	'cancelled' => _('Cancelled'),
);
echo '<td>' . _('Status:') . '</td><td>';
echo array_selector('filter_status', get_post('filter_status'), $status_options, array('select_submit' => true));
echo '</td>';

// Type filter
$type_options = array_merge(array('' => _('All Types')), get_package_types());
echo '<td>' . _('Type:') . '</td><td>';
echo array_selector('filter_type', get_post('filter_type'), $type_options, array('select_submit' => true));
echo '</td>';

end_row();
end_table();

// =====================================================================
// Status Summary Cards
// =====================================================================

$summary = get_package_status_summary();

echo '<div style="display:flex;gap:12px;margin:10px 0;flex-wrap:wrap;">';
foreach ($summary as $status => $count) {
	if ($status === 'cancelled' && $count === 0) continue;
	$color = get_package_status_color($status);
	$statuses = get_package_statuses();
	$label = isset($statuses[$status]) ? $statuses[$status] : $status;
	echo '<div style="background:#fff;border:2px solid ' . $color . ';border-radius:8px;padding:8px 16px;text-align:center;min-width:100px;">'
		. '<div style="font-size:22px;font-weight:bold;color:' . $color . ';">' . $count . '</div>'
		. '<div style="font-size:11px;color:#666;">' . htmlspecialchars($label) . '</div></div>';
}
echo '</div>';

// =====================================================================
// Ready for Packing — Completed Pick Operations
// =====================================================================

$wh_filter = get_post('filter_warehouse');
$picks_ready = get_picks_ready_for_packing($wh_filter);
$num_picks = db_num_rows($picks_ready);

if ($num_picks > 0) {
	display_heading(_('Ready for Packing — Completed Picks'));
	start_table(TABLESTYLE, "width='100%'");
	$th = array(_('#'), _('Pick Op'), _('Wave'), _('Lines'), _('Qty Picked'), _('Priority'), _('Completed'), _(''));
	table_header($th);

	$k = 0;
	$row_num = 0;
	while ($pick = db_fetch($picks_ready)) {
		$row_num++;
		alt_table_row_color($k);

		label_cell($row_num);
		label_cell('#' . $pick['op_id']);
		label_cell($pick['wave_name'] ? htmlspecialchars($pick['wave_name']) : '-');
		label_cell((int)$pick['total_lines']);
		label_cell(number_format2((float)$pick['total_picked_qty'], get_qty_dec('')));
		label_cell((int)$pick['priority']);
		label_cell($pick['completed_at'] ? sql2date(substr($pick['completed_at'], 0, 10)) : '-');

		// Create Pack Op button
		echo '<td>';
		submit('CreatePackOp', $pick['op_id'], false, _('Create Pack Operation'), 'default');
		echo '</td>';

		end_row();
	}
	end_table(1);
}

// =====================================================================
// Pack Operation Detail (when viewing a pack operation)
// =====================================================================

$view_pack_op = get_post('view_pack_op');
if ($view_pack_op > 0) {
	$pack_op = get_wh_operation((int)$view_pack_op);
	if ($pack_op && $pack_op['op_type'] === 'pack') {
		$progress = get_packing_progress((int)$view_pack_op);

		display_heading(sprintf(_('Pack Operation #%d'), (int)$view_pack_op));

		// Progress bar
		$pct = $progress['pct'];
		$bar_color = $pct >= 100 ? '#28a745' : ($pct >= 50 ? '#ffc107' : '#007bff');
		echo '<div style="margin:8px 0;">'
			. '<div style="background:#e9ecef;border-radius:4px;height:20px;width:300px;display:inline-block;">'
			. '<div style="background:' . $bar_color . ';height:100%;border-radius:4px;width:' . min($pct, 100) . '%;">'
			. '</div></div> <strong>' . $pct . '%</strong> ('
			. $progress['packed_lines'] . '/' . $progress['total_lines'] . ' ' . _('lines') . ')</div>';

		// Pack lines table
		$lines = get_wh_operation_lines((int)$view_pack_op);
		start_table(TABLESTYLE, "width='100%'");
		$th = array(_('#'), _('Item'), _('Description'), _('From Bin'), _('Batch/Serial'), _('Planned'), _('Packed'), _('Package'), _(''));
		table_header($th);

		// Get open packages for dropdown
		$open_pkgs = get_packages(array('status' => 'open'));
		$pkg_options = array('' => _('-- Select Package --'));
		while ($opkg = db_fetch($open_pkgs))
			$pkg_options[$opkg['package_id']] = $opkg['package_code'] . ' (' . $opkg['package_type'] . ')';

		$k = 0;
		$line_num = 0;
		while ($line = db_fetch($lines)) {
			$line_num++;
			alt_table_row_color($k);

			label_cell($line_num);
			label_cell($line['stock_id']);
			label_cell(htmlspecialchars($line['item_description']));
			label_cell($line['from_bin_code'] ? htmlspecialchars($line['from_bin_code']) : '-');

			// Batch/Serial info
			$tracking_info = array();
			if (!empty($line['batch_no']))
				$tracking_info[] = _('Batch:') . ' ' . htmlspecialchars($line['batch_no']);
			if (!empty($line['serial_no']))
				$tracking_info[] = _('S/N:') . ' ' . htmlspecialchars($line['serial_no']);
			label_cell(!empty($tracking_info) ? implode('<br>', $tracking_info) : '-');

			label_cell(number_format2((float)$line['qty_planned'], get_qty_dec($line['stock_id'])));
			label_cell(number_format2((float)$line['qty_done'], get_qty_dec($line['stock_id'])));

			// Package assignment
			if ((float)$line['qty_done'] > 0 && $line['package_id']) {
				$pkg = get_package((int)$line['package_id']);
				label_cell($pkg ? htmlspecialchars($pkg['package_code']) : '#' . $line['package_id']);
				label_cell(''); // No action needed — already packed
			} elseif ($pack_op['op_status'] !== 'done') {
				// Package selector + qty + Pack button
				echo '<td>';
				echo array_selector('pack_into_' . $line['line_id'], '', $pkg_options);
				echo '</td><td>';
				$dec = get_qty_dec($line['stock_id']);
				echo '<input type="text" name="pack_qty_' . $line['line_id'] . '" size="6" maxlength="12" '
					. 'value="' . number_format2((float)$line['qty_planned'], $dec) . '"> ';
				submit('PackLine', $line['line_id'], false, _('Pack'), 'default');
				echo '</td>';
			} else {
				label_cell('-');
				label_cell('');
			}

			end_row();
		}
		end_table(1);

		// Action buttons
		if ($pack_op['op_status'] !== 'done' && $pack_op['op_status'] !== 'cancelled') {
			echo '<div style="margin:8px 0;">';
			submit('CompletePack', (int)$view_pack_op, false, _('Complete Pack Operation'), 'default');
			echo '</div>';
		}

		// Link to view page
		echo '<div style="margin:8px 0;">';
		echo '<a href="' . $path_to_root . '/inventory/warehouse/view/view_pack_order.php?op_id='
			. (int)$view_pack_op . '" target="_blank">' . _('Print Pack Order') . '</a>';
		echo '</div>';
	}
}

// =====================================================================
// Packages List
// =====================================================================

display_heading(_('Packages'));

div_start('package_list');

$filters = array();
if (!empty(get_post('filter_status')))
	$filters['status'] = get_post('filter_status');
if (!empty(get_post('filter_type')))
	$filters['package_type'] = get_post('filter_type');
if (!empty(get_post('filter_warehouse')))
	$filters['warehouse_loc_code'] = get_post('filter_warehouse');

$packages = get_packages($filters);

start_table(TABLESTYLE, "width='100%'");
$th = array(_('#'), _('Package Code'), _('Type'), _('Status'), _('Items'), _('Weight (kg)'),
	_('Dimensions (cm)'), _('Carrier'), _('Tracking #'), _('Created'), _(''));
table_header($th);

$k = 0;
$row_num = 0;
while ($pkg = db_fetch($packages)) {
	$row_num++;
	alt_table_row_color($k);

	label_cell($row_num);

	// Package code as link to view
	$link = '<a href="' . $path_to_root . '/inventory/warehouse/view/view_pack_order.php?package_id='
		. (int)$pkg['package_id'] . '" target="_blank">'
		. htmlspecialchars($pkg['package_code']) . '</a>';
	label_cell($link);

	label_cell(package_type_badge($pkg['package_type']));
	label_cell(package_status_badge($pkg['status']));

	// Item count
	$item_count = count_package_contents((int)$pkg['package_id']);
	label_cell($item_count);

	// Weight
	label_cell($pkg['weight'] ? number_format2((float)$pkg['weight'], 2) : '-');

	// Dimensions
	$dims = array();
	if ($pkg['length']) $dims[] = number_format2((float)$pkg['length'], 1);
	if ($pkg['width']) $dims[] = number_format2((float)$pkg['width'], 1);
	if ($pkg['height']) $dims[] = number_format2((float)$pkg['height'], 1);
	label_cell(!empty($dims) ? implode(' x ', $dims) : '-');

	// Carrier & Tracking
	label_cell($pkg['carrier'] ? htmlspecialchars($pkg['carrier']) : '-');
	label_cell($pkg['tracking_number'] ? htmlspecialchars($pkg['tracking_number']) : '-');

	label_cell(sql2date(substr($pkg['created_at'], 0, 10)));

	// Actions cell
	echo '<td nowrap>';
	if ($pkg['status'] === 'open') {
		echo "<button class='ajaxsubmit' type='submit' aspect='default' name='SealPackage' value='" . $pkg['package_id'] . "' title='" . _('Seal') . "'>" . set_icon(ICON_SUBMIT) . "<span>" . _('Seal') . "</span></button>\n";
		echo ' ';
		submit('Edit' . $pkg['package_id'], _('Edit'), true, _('Edit'), false, ICON_EDIT);
		echo ' ';
		submit('Delete' . $pkg['package_id'], _('Delete'), true, _('Delete'), false, ICON_DELETE);
	} elseif ($pkg['status'] === 'sealed') {
		echo "<button class='ajaxsubmit' type='submit' aspect='default' name='UnsealPackage' value='" . $pkg['package_id'] . "' title='" . _('Unseal') . "'>" . set_icon(ICON_SUBMIT) . "<span>" . _('Unseal') . "</span></button>\n";
	}
	if (!in_array($pkg['status'], array('shipped', 'delivered', 'cancelled'))) {
		echo ' ';
		echo "<button class='ajaxsubmit' type='submit' name='CancelPackage' value='" . $pkg['package_id'] . "' title='" . _('Cancel') . "'>" . set_icon(ICON_ESCAPE) . "<span>" . _('Cancel') . "</span></button>\n";
	}
	echo '</td>';

	end_row();

	// Expandable contents row
	if ($item_count > 0) {
		$contents = get_package_contents((int)$pkg['package_id']);
		echo '<tr><td colspan="11" style="padding:4px 20px;background:#f8f9fa;">';
		echo '<table style="width:100%;font-size:12px;"><tr style="background:#e9ecef;">'
			. '<th style="padding:3px 8px;">' . _('Item') . '</th>'
			. '<th style="padding:3px 8px;">' . _('Description') . '</th>'
			. '<th style="padding:3px 8px;">' . _('Qty') . '</th>'
			. '<th style="padding:3px 8px;">' . _('Batch') . '</th>'
			. '<th style="padding:3px 8px;">' . _('Serial') . '</th></tr>';
		while ($content = db_fetch($contents)) {
			echo '<tr>'
				. '<td style="padding:2px 8px;">' . htmlspecialchars($content['stock_id']) . '</td>'
				. '<td style="padding:2px 8px;">' . htmlspecialchars($content['item_description']) . '</td>'
				. '<td style="padding:2px 8px;">' . number_format2((float)$content['qty'], get_qty_dec($content['stock_id'])) . '</td>'
				. '<td style="padding:2px 8px;">' . ($content['batch_no'] ? htmlspecialchars($content['batch_no']) : '-') . '</td>'
				. '<td style="padding:2px 8px;">' . ($content['serial_no'] ? htmlspecialchars($content['serial_no']) : '-') . '</td>'
				. '</tr>';
		}
		echo '</table></td></tr>';
	}
}

if ($row_num === 0) {
	start_row();
	label_cell(_('No packages found.'), "colspan='11' style='text-align:center;padding:20px;color:#999;'");
	end_row();
}

end_table(1);
div_end();

// =====================================================================
// Package Add/Edit Form
// =====================================================================

display_heading($selected_id == -1 ? _('New Package') : _('Edit Package'));

start_table(TABLESTYLE2);

if ($Mode == 'Edit') {
	$pkg = get_package($selected_id);
	if ($pkg) {
		$_POST['package_code'] = $pkg['package_code'];
		$_POST['package_type'] = $pkg['package_type'];
		$_POST['pkg_weight'] = $pkg['weight'] ? $pkg['weight'] : '';
		$_POST['pkg_length'] = $pkg['length'] ? $pkg['length'] : '';
		$_POST['pkg_width'] = $pkg['width'] ? $pkg['width'] : '';
		$_POST['pkg_height'] = $pkg['height'] ? $pkg['height'] : '';
		$_POST['carrier'] = $pkg['carrier'];
		$_POST['tracking_number'] = $pkg['tracking_number'];
	}
}

text_row(_('Package Code:'), 'package_code', get_post('package_code'), 30, 40);
echo '<tr><td class="label">' . _('Package Type:') . '</td><td>';
echo array_selector('package_type', get_post('package_type', 'box'), get_package_types());
echo '</td></tr>';
small_amount_row(_('Weight (kg):'), 'pkg_weight', get_post('pkg_weight'), null, null, 2);
small_amount_row(_('Length (cm):'), 'pkg_length', get_post('pkg_length'), null, null, 1);
small_amount_row(_('Width (cm):'), 'pkg_width', get_post('pkg_width'), null, null, 1);
small_amount_row(_('Height (cm):'), 'pkg_height', get_post('pkg_height'), null, null, 1);

if ($selected_id != -1) {
	text_row(_('Carrier:'), 'carrier', get_post('carrier'), 40, 60);
	text_row(_('Tracking #:'), 'tracking_number', get_post('tracking_number'), 40, 100);
}

end_table(1);

submit_add_or_update_center($selected_id == -1, '', 'both');

// =====================================================================
// Quick Create Package (auto-code)
// =====================================================================

if ($selected_id == -1) {
	echo '<div style="margin:10px 0;padding:10px;background:#f0f8ff;border:1px solid #bee5eb;border-radius:4px;">';
	echo '<strong>' . _('Quick Create:') . '</strong> ';
	echo _('Leave Package Code empty to auto-generate (format: PKG-YYYYMMDD-NNNN).');
	echo '</div>';
}

end_form();
end_page();
