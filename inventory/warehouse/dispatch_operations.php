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
 * Dispatch (Outbound) Operations Dashboard
 *
 * Lists and manages WMS outbound operations (pick, pack, ship).
 * Mirrors receipt_operations.php for outbound workflow.
 *
 * @package NotrinosERP
 * @subpackage Inventory/Warehouse
 */

$page_security = 'SA_DISPATCH_OPERATIONS';
$path_to_root = '../..';

include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/includes/data_checks.inc');
include_once($path_to_root . '/inventory/includes/inventory_db.inc');
include_once($path_to_root . '/inventory/warehouse/includes/db/warehouse_operations_db.inc');
include_once($path_to_root . '/inventory/warehouse/includes/db/warehouse_locations_db.inc');

$js = '';
if (user_use_date_picker())
	$js .= get_js_date_picker();

page(_($help_context = 'Dispatch Operations'), false, false, '', $js);

// =====================================================================
// Process action buttons: Start, Complete, Cancel
// =====================================================================

if (isset($_POST['start_op'])) {
	$op_id = (int)$_POST['start_op'];
	update_wh_operation_status($op_id, 'in_progress');
	display_notification(sprintf(_('Operation #%d started.'), $op_id));
}

if (isset($_POST['complete_op'])) {
	$op_id = (int)$_POST['complete_op'];
	complete_wh_operation($op_id);
	display_notification(sprintf(_('Operation #%d completed.'), $op_id));
}

if (isset($_POST['cancel_op'])) {
	$op_id = (int)$_POST['cancel_op'];
	cancel_wh_operation($op_id);
	display_notification(sprintf(_('Operation #%d cancelled.'), $op_id));
}

// =====================================================================
// Filters
// =====================================================================

start_form();
start_table(TABLESTYLE_NOBORDER);
start_row();

// Warehouse location filter
$sql_locs = "SELECT loc_code, location_name FROM " . TB_PREF . "locations WHERE wh_enabled = 1 ORDER BY location_name";
echo "<div class = 'filter-field'>";
echo combo_input('filter_location', get_post('filter_location'), $sql_locs, 'loc_code', 'location_name',
	array('spec_option' => _('All Warehouses'), 'spec_id' => '', 'select_submit' => true, 'order' => false));
echo '</div>';

// Type filter (pick, pack, ship)
$type_options = array(
	'' => _('All Outbound'),
	'pick' => _('Pick'),
	'pack' => _('Pack'),
	'ship' => _('Ship'),
);
echo "<div class = 'filter-field'>";
echo array_selector('filter_type', get_post('filter_type'), $type_options, array('select_submit' => true));
echo '</div>';

// Status filter
$status_options = array(
	'active' => _('All Active'),
	'draft' => _('Draft'),
	'ready' => _('Ready'),
	'in_progress' => _('In Progress'),
	'done' => _('Done'),
	'cancelled' => _('Cancelled'),
);
echo "<div class = 'filter-field'>";
echo array_selector('filter_status', get_post('filter_status', 'active'), $status_options, array('select_submit' => true));
echo '</div>';
submit_cells('RefreshList', _('Search'), '', _('Refresh list'), 'default');
end_row();
end_table(1);

// =====================================================================
// Summary Cards
// =====================================================================

$filter_loc = get_post('filter_location');

// Count pending pick operations
$pick_filter = array('op_type' => 'pick', 'op_status' => array('draft', 'ready', 'in_progress'));
if ($filter_loc) $pick_filter['warehouse_loc_code'] = $filter_loc;
$pick_count = count_wh_operations_filtered($pick_filter);

// Count pending pack operations
$pack_filter = array('op_type' => 'pack', 'op_status' => array('draft', 'ready', 'in_progress'));
if ($filter_loc) $pack_filter['warehouse_loc_code'] = $filter_loc;
$pack_count = count_wh_operations_filtered($pack_filter);

// Count pending ship operations
$ship_filter = array('op_type' => 'ship', 'op_status' => array('draft', 'ready', 'in_progress'));
if ($filter_loc) $ship_filter['warehouse_loc_code'] = $filter_loc;
$ship_count = count_wh_operations_filtered($ship_filter);

echo '<div style="display:flex;gap:16px;margin:12px 0;">';

// Pick card
echo '<div style="flex:1;padding:12px 16px;border-radius:6px;background:#e3f2fd;border:1px solid #90caf9;">';
echo '<div style="font-size:24px;font-weight:bold;color:#1565c0;">' . $pick_count . '</div>';
echo '<div style="font-size:12px;color:#1565c0;">' . _('Pending Picks') . '</div>';
echo '</div>';

// Pack card
echo '<div style="flex:1;padding:12px 16px;border-radius:6px;background:#fff3e0;border:1px solid #ffcc80;">';
echo '<div style="font-size:24px;font-weight:bold;color:#e65100;">' . $pack_count . '</div>';
echo '<div style="font-size:12px;color:#e65100;">' . _('Pending Packs') . '</div>';
echo '</div>';

// Ship card
echo '<div style="flex:1;padding:12px 16px;border-radius:6px;background:#e8f5e9;border:1px solid #a5d6a7;">';
echo '<div style="font-size:24px;font-weight:bold;color:#2e7d32;">' . $ship_count . '</div>';
echo '<div style="font-size:12px;color:#2e7d32;">' . _('Pending Ships') . '</div>';
echo '</div>';

echo '</div>';

// =====================================================================
// Operations Table
// =====================================================================

$filters = array();
$filter_type = get_post('filter_type');
$filter_status = get_post('filter_status', 'active');

if ($filter_type)
	$filters['op_type'] = $filter_type;
else
	$filters['op_type'] = array('pick', 'pack', 'ship');

if ($filter_status === 'active')
	$filters['op_status'] = array('draft', 'ready', 'in_progress');
else
	$filters['op_status'] = $filter_status;

if ($filter_loc)
	$filters['warehouse_loc_code'] = $filter_loc;

// Source doc type filter: only delivery notes
$filters['source_doc_type'] = ST_CUSTDELIVERY;

$operations = get_wh_operations_filtered($filters, 100);

div_start('operations_list');

display_heading2(_('Outbound Operations'));

start_table(TABLESTYLE, "width='100%'");
$th = array('#', _('Type'), _('Status'), _('Source'), _('From'), _('To'), _('Priority'), _('Assigned'), _('Created'), _('Notes'), _('Actions'));
table_header($th);

$k = 0;
$has_rows = false;
while ($row = db_fetch($operations)) {
	$has_rows = true;
	alt_table_row_color($k);

	// Op ID
	label_cell($row['op_id']);

	// Type badge
	$type_color = get_wh_operation_type_color($row['op_type']);
	echo '<td><span style="background:' . $type_color . ';color:#fff;padding:2px 8px;border-radius:10px;font-size:11px;">'
		. strtoupper($row['op_type']) . '</span></td>';

	// Status badge
	$status_color = get_wh_operation_status_color($row['op_status']);
	echo '<td><span style="background:' . $status_color . ';color:#fff;padding:2px 8px;border-radius:10px;font-size:11px;">'
		. strtoupper($row['op_status']) . '</span></td>';

	// Source document link
	$source_text = 'DN #' . $row['source_doc_no'];
	$source_link = '<a href="' . $path_to_root . '/sales/view/view_dispatch.php?trans_no=' . $row['source_doc_no'] . '">' . $source_text . '</a>';
	label_cell($source_link);

	// From/To locations
	$from_name = isset($row['from_loc_name']) ? $row['from_loc_name'] : '-';
	$to_name = isset($row['to_loc_name']) ? $row['to_loc_name'] : '-';
	label_cell($from_name);
	label_cell($to_name);

	// Priority
	label_cell($row['priority']);

	// Assigned
	$assigned = !empty($row['assigned_to']) ? $row['assigned_to'] : '-';
	label_cell($assigned);

	// Created
	$created = !empty($row['created_at']) ? sql2date(substr($row['created_at'], 0, 10)) : '-';
	label_cell($created);

	// Notes/Memo
	$memo = !empty($row['memo']) ? $row['memo'] : '';
	label_cell($memo);

	// Actions
	echo '<td nowrap>';
	if ($row['op_status'] === 'draft' || $row['op_status'] === 'ready') {
		echo '<button type="submit" name="start_op" value="' . $row['op_id'] . '" class="ajaxsubmit" style="margin:1px;padding:2px 6px;background:#1976d2;color:#fff;border:none;border-radius:3px;cursor:pointer;">'
			. _('Start') . '</button> ';
		echo '<button type="submit" name="cancel_op" value="' . $row['op_id'] . '" class="ajaxsubmit" style="margin:1px;padding:2px 6px;background:#d32f2f;color:#fff;border:none;border-radius:3px;cursor:pointer;">'
			. _('Cancel') . '</button>';
	} elseif ($row['op_status'] === 'in_progress') {
		echo '<button type="submit" name="complete_op" value="' . $row['op_id'] . '" class="ajaxsubmit" style="margin:1px;padding:2px 6px;background:#388e3c;color:#fff;border:none;border-radius:3px;cursor:pointer;">'
			. _('Complete') . '</button> ';
		echo '<button type="submit" name="cancel_op" value="' . $row['op_id'] . '" class="ajaxsubmit" style="margin:1px;padding:2px 6px;background:#d32f2f;color:#fff;border:none;border-radius:3px;cursor:pointer;">'
			. _('Cancel') . '</button>';
	} else {
		label_cell('-');
	}
	echo '</td>';

	end_row();

	// Expandable detail: show operation lines
	$lines = get_wh_operation_lines($row['op_id']);
	if (db_num_rows($lines) > 0) {
		echo '<tr class="OddTableRow"><td></td><td colspan="10" style="padding:4px 12px;background:#fafafa;font-size:11px;">';
		echo '<table style="width:100%;border-collapse:collapse;font-size:11px;">';
		echo '<tr style="background:#eee;"><th>' . _('Item') . '</th><th>' . _('Qty Planned') . '</th><th>' . _('Qty Done') . '</th><th>' . _('From Bin') . '</th><th>' . _('To Bin') . '</th><th>' . _('Batch') . '</th><th>' . _('Serial') . '</th></tr>';
		while ($ln = db_fetch($lines)) {
			echo '<tr>';
			echo '<td style="padding:2px 4px;">' . htmlspecialchars($ln['stock_id']) . '</td>';
			echo '<td style="padding:2px 4px;text-align:right;">' . number_format2((float)$ln['qty_planned'], 2) . '</td>';
			echo '<td style="padding:2px 4px;text-align:right;">' . number_format2((float)$ln['qty_done'], 2) . '</td>';
			$from_bin = isset($ln['from_bin_name']) ? $ln['from_bin_name'] : (isset($ln['from_loc_id']) ? '#' . $ln['from_loc_id'] : '-');
			$to_bin = isset($ln['to_bin_name']) ? $ln['to_bin_name'] : (isset($ln['to_loc_id']) ? '#' . $ln['to_loc_id'] : '-');
			echo '<td style="padding:2px 4px;">' . htmlspecialchars($from_bin) . '</td>';
			echo '<td style="padding:2px 4px;">' . htmlspecialchars($to_bin) . '</td>';
			$batch_info = !empty($ln['batch_no']) ? $ln['batch_no'] : (!empty($ln['batch_id']) ? '#' . $ln['batch_id'] : '-');
			$serial_info = !empty($ln['serial_no']) ? $ln['serial_no'] : (!empty($ln['serial_id']) ? '#' . $ln['serial_id'] : '-');
			echo '<td style="padding:2px 4px;">' . htmlspecialchars($batch_info) . '</td>';
			echo '<td style="padding:2px 4px;">' . htmlspecialchars($serial_info) . '</td>';
			echo '</tr>';
		}
		echo '</table>';
		echo '</td></tr>';
	}
}

if (!$has_rows) {
	label_row('', _('No outbound operations found matching the selected filters.'), 'colspan=11 align=center');
}

end_table(1);
div_end();

end_form();

end_page();
