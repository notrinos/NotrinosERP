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
 * Scrap Entry â€” Enter scrap with reason codes, serial/batch awareness,
 * bin selection, GL posting, and approval workflow for high-value scraps.
 *
 * @package NotrinosERP
 * @subpackage Inventory/Warehouse
 */

$page_security = 'SA_WAREHOUSE_SCRAP';
$path_to_root = '../..';

include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/includes/date_functions.inc');
include_once($path_to_root . '/includes/data_checks.inc');
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/includes/ui/ui_lists.inc');
include_once($path_to_root . '/includes/db/inventory_db.inc');
include_once($path_to_root . '/gl/includes/db/gl_db_trans.inc');
include_once($path_to_root . '/inventory/warehouse/includes/db/warehouse_scrap_db.inc');
include_once($path_to_root . '/inventory/warehouse/includes/db/warehouse_locations_db.inc');
include_once($path_to_root . '/inventory/warehouse/includes/warehouse_ui.inc');
include_once($path_to_root . '/inventory/includes/db/serial_numbers_db.inc');
include_once($path_to_root . '/inventory/includes/db/stock_batches_db.inc');
include_once($path_to_root . '/inventory/includes/db/serial_batch_db.inc');

$js = '';
if (user_use_date_picker())
	$js .= get_js_date_picker();

page(_($help_context = 'Scrap Entry'), false, false, '', $js);

// =====================================================================
// HANDLE ACTIONS
// =====================================================================

// --- Add scrap entry ---
if (isset($_POST['ADD_SCRAP'])) {
	$input_error = 0;

	if (empty($_POST['stock_id'])) {
		display_error(_('You must select an item.'));
		$input_error = 1;
	}

	$qty = input_num('qty', 0);
	if ($qty <= 0) {
		display_error(_('Quantity must be greater than zero.'));
		$input_error = 1;
	}

	if (empty($_POST['reason_code'])) {
		display_error(_('You must select a reason code.'));
		$input_error = 1;
	}

	if (empty($_POST['warehouse'])) {
		display_error(_('You must select a warehouse location.'));
		$input_error = 1;
	}

	if ($input_error == 0) {
		$stock_id = $_POST['stock_id'];
		$loc_code = $_POST['warehouse'];
		$date_ = $_POST['scrap_date'];
		$reason_code = $_POST['reason_code'];
		$reason_detail = get_post('reason_detail');
		$from_bin_id = get_post('from_bin_id') ? (int)get_post('from_bin_id') : null;
		$scrap_loc_id = get_post('scrap_loc_id') ? (int)get_post('scrap_loc_id') : null;
		$serial_id = get_post('serial_id') ? (int)get_post('serial_id') : null;
		$batch_id = get_post('batch_id') ? (int)get_post('batch_id') : null;
		$memo = get_post('memo');

		$scrap_id = add_scrap_entry($stock_id, $qty, $loc_code, $date_,
			$reason_code, $reason_detail, $from_bin_id, $scrap_loc_id,
			$serial_id, $batch_id, $memo);

		if ($scrap_id) {
			$entry = get_scrap_entry($scrap_id);
			$needs_approval = scrap_needs_approval($entry['total_cost']);

			display_notification(sprintf(_('Scrap entry #%s has been created. Cost: %s'),
				$scrap_id, price_format($entry['total_cost'])));

			if ($needs_approval) {
				display_warning(_('This scrap entry exceeds the approval threshold and requires manager approval.'));
			}

			// Reset form
			unset($_POST['stock_id'], $_POST['qty'], $_POST['reason_code'],
				$_POST['reason_detail'], $_POST['from_bin_id'],
				$_POST['scrap_loc_id'], $_POST['serial_id'],
				$_POST['batch_id'], $_POST['memo']);
		} else {
			display_error(_('Failed to create scrap entry.'));
		}
	}
	$Ajax->activate('_page_body');
}

// --- Approve scrap entry ---
if (isset($_POST['APPROVE_SCRAP'])) {
	$scrap_id = (int)get_post('approve_scrap_id');
	if ($scrap_id > 0) {
		approve_scrap_entry($scrap_id);
		display_notification(sprintf(_('Scrap entry #%s has been approved.'), $scrap_id));
	}
	$Ajax->activate('_page_body');
}

// =====================================================================
// RENDER PAGE
// =====================================================================

start_form();

// =====================================================================
// Summary Cards
// =====================================================================

$summary = get_scrap_summary(get_post('filter_warehouse'));
$pending_result = get_scrap_pending_approval();
$pending_count = db_num_rows($pending_result);

echo "<div style='display:flex;gap:12px;flex-wrap:wrap;margin-bottom:16px;'>";

// Total scrap entries
echo "<div style='flex:1;min-width:150px;padding:12px 16px;background:#f8f9fa;border-left:4px solid #6c757d;border-radius:4px;'>";
echo "<div style='font-size:11px;color:#6c757d;text-transform:uppercase;'>" . _('Total Entries') . "</div>";
echo "<div style='font-size:22px;font-weight:600;color:#343a40;'>" . number_format($summary['total_entries']) . "</div>";
echo "</div>";

// Total quantity scrapped
echo "<div style='flex:1;min-width:150px;padding:12px 16px;background:#f8f9fa;border-left:4px solid #dc3545;border-radius:4px;'>";
echo "<div style='font-size:11px;color:#6c757d;text-transform:uppercase;'>" . _('Total Qty Scrapped') . "</div>";
echo "<div style='font-size:22px;font-weight:600;color:#dc3545;'>" . number_format2($summary['total_qty'], user_qty_dec()) . "</div>";
echo "</div>";

// Total cost
echo "<div style='flex:1;min-width:150px;padding:12px 16px;background:#f8f9fa;border-left:4px solid #fd7e14;border-radius:4px;'>";
echo "<div style='font-size:11px;color:#6c757d;text-transform:uppercase;'>" . _('Total Cost') . "</div>";
echo "<div style='font-size:22px;font-weight:600;color:#fd7e14;'>" . price_format($summary['total_cost']) . "</div>";
echo "</div>";

// Pending approval
if ($pending_count > 0) {
	echo "<div style='flex:1;min-width:150px;padding:12px 16px;background:#fff3cd;border-left:4px solid #ffc107;border-radius:4px;'>";
	echo "<div style='font-size:11px;color:#856404;text-transform:uppercase;'>" . _('Pending Approval') . "</div>";
	echo "<div style='font-size:22px;font-weight:600;color:#856404;'>" . $pending_count . "</div>";
	echo "</div>";
}

echo "</div>";

// =====================================================================
// Reason Breakdown (compact)
// =====================================================================

if (!empty($summary['by_reason'])) {
	echo "<div style='display:flex;gap:8px;flex-wrap:wrap;margin-bottom:16px;'>";
	foreach ($summary['by_reason'] as $r) {
		$color = get_scrap_reason_color($r['reason_code']);
		$label = get_scrap_reason_label($r['reason_code']);
		echo "<span style='display:inline-block;padding:4px 10px;border-radius:12px;font-size:11px;"
			. "background:" . $color . "20;color:" . $color . ";font-weight:600;'>"
			. $label . ": " . (int)$r['cnt'] . " (" . price_format($r['cost']) . ")"
			. "</span>";
	}
	echo "</div>";
}

// =====================================================================
// Pending Approval Table
// =====================================================================

if ($pending_count > 0) {
	echo "<h3 style='color:#856404;'>" . _('Scrap Entries Pending Approval') . "</h3>";
	start_table(TABLESTYLE, "width='80%'");
	$th = array(_('#'), _('Date'), _('Item'), _('Qty'), _('Cost'), _('Reason'), _('Action'));
	table_header($th);

	// Re-query since we consumed the result counting
	$pending_result2 = get_scrap_pending_approval();
	$k = 0;
	while ($row = db_fetch($pending_result2)) {
		alt_table_row_color($k);
		label_cell($row['scrap_id']);
		label_cell(sql2date($row['scrap_date']));
		label_cell($row['stock_id'] . ' â€” ' . $row['description']);
		qty_cell($row['qty']);
		amount_cell($row['total_cost']);
		$reason_label = get_scrap_reason_label($row['reason_code']);
		$reason_color = get_scrap_reason_color($row['reason_code']);
		label_cell("<span style='color:" . $reason_color . ";font-weight:600;'>" . $reason_label . "</span>");
		// Approve button
		echo "<td>";
		hidden('approve_scrap_id', $row['scrap_id']);
		submit('APPROVE_SCRAP', _('Approve'), true, _('Approve this scrap entry'), 'default');
		echo "</td>";
		end_row();
	}
	end_table(1);
}

// =====================================================================
// Filter Bar
// =====================================================================

echo "<h3>" . _('Scrap History') . "</h3>";

start_table(TABLESTYLE2);
start_row();
locations_list_cells(_('Warehouse:'), 'filter_warehouse', get_post('filter_warehouse'), true, false, false);

$reason_options = array_merge(array('' => _('-- All Reasons --')), get_scrap_reason_codes());
echo "<td class='label'>" . _('Reason:') . "</td><td>";
echo array_selector('filter_reason', get_post('filter_reason'), $reason_options);
echo "</td>";

date_cells(_('From:'), 'filter_date_from', '', null, 0, 0, 1001);
date_cells(_('To:'), 'filter_date_to', '', null, 0, 0, 1001);
submit_cells('Search', _('Search'), '', _('Search scrap entries'), 'default');
end_row();
end_table(1);

// =====================================================================
// Scrap History Table
// =====================================================================

$filters = array();
if (get_post('filter_warehouse')) $filters['loc_code'] = get_post('filter_warehouse');
if (get_post('filter_reason')) $filters['reason_code'] = get_post('filter_reason');
if (get_post('filter_date_from')) $filters['date_from'] = get_post('filter_date_from');
if (get_post('filter_date_to')) $filters['date_to'] = get_post('filter_date_to');

$result = get_scrap_entries($filters);

div_start('scrap_list');
start_table(TABLESTYLE, "width='95%'");
$th = array(_('#'), _('Date'), _('Item'), _('Qty'), _('Unit Cost'), _('Total Cost'),
	_('Reason'), _('From Bin'), _('Serial'), _('Batch'), _('Approved'), _('Memo'));
table_header($th);

$total_cost = 0;
$k = 0;
while ($row = db_fetch($result)) {
	alt_table_row_color($k);
	label_cell($row['scrap_id']);
	label_cell(sql2date($row['scrap_date']));
	label_cell($row['stock_id'] . ' â€” ' . $row['description']);
	qty_cell($row['qty']);
	amount_cell($row['unit_cost']);
	amount_cell($row['total_cost']);

	$reason_label = get_scrap_reason_label($row['reason_code']);
	$reason_color = get_scrap_reason_color($row['reason_code']);
	label_cell("<span style='color:" . $reason_color . ";font-weight:600;'>" . $reason_label . "</span>");

	label_cell($row['from_loc_name'] ? $row['from_loc_name'] : 'â€”');
	label_cell($row['serial_no'] ? $row['serial_no'] : 'â€”');
	label_cell($row['batch_no'] ? $row['batch_no'] : 'â€”');
	label_cell($row['approved_by'] ? _('Yes') : ($row['total_cost'] > 0 && scrap_needs_approval($row['total_cost']) ? "<span style='color:#dc3545;'>" . _('Pending') . "</span>" : 'â€”'));
	label_cell($row['memo'] ? $row['memo'] : '');

	$total_cost += $row['total_cost'];
	end_row();
}

// Totals row
if ($total_cost > 0) {
	start_row("class='inquirybg'");
	label_cell('<b>' . _('Total') . '</b>', "colspan=5 align='right'");
	amount_cell($total_cost);
	label_cell('', "colspan=6");
	end_row();
}

end_table(1);
div_end();

// =====================================================================
// New Scrap Entry Form
// =====================================================================

echo "<h3>" . _('New Scrap Entry') . "</h3>";

start_table(TABLESTYLE2);

// Item
echo "<tr>";
stock_items_list_cells(_('Item:'), 'stock_id', get_post('stock_id'), false, true);
echo "</tr>";

// Quantity
small_amount_row(_('Quantity:'), 'qty', get_post('qty', ''), null, null, user_qty_dec());

// Warehouse
locations_list_row(_('Warehouse:'), 'warehouse', get_post('warehouse'), false, false, false);

// Source Bin (optional)
$wh_code = get_post('warehouse');
if ($wh_code) {
	warehouse_bin_list_row(_('Source Bin:'), 'from_bin_id', $wh_code, get_post('from_bin_id'), _('-- Select --'));
}

// Scrap Destination Bin (optional â€” usually a scrap zone)
if ($wh_code) {
	// Show bins filtered â€” for scrap zone
	warehouse_bin_list_row(_('Scrap Bin:'), 'scrap_loc_id', $wh_code, get_post('scrap_loc_id'), _('-- None --'));
}

// Reason code
$reason_options = get_scrap_reason_codes();
echo "<tr><td class='label'>" . _('Reason Code:') . "</td><td>";
echo array_selector('reason_code', get_post('reason_code'), $reason_options);
echo "</td></tr>";

// Reason detail (free text)
text_row(_('Reason Detail:'), 'reason_detail', get_post('reason_detail'), 60, 255);

// Date
date_row(_('Scrap Date:'), 'scrap_date', '', null, 0, 0, 0);

// Serial number (optional â€” only shown for serial-tracked items)
$stock_id = get_post('stock_id');
if ($stock_id) {
	$tracking_mode = get_item_tracking_mode($stock_id);
	if ($tracking_mode === 'serial' || $tracking_mode === 'both') {
		// Serial selector â€” show available serials for this item
		$serial_sql = "SELECT id, serial_no FROM " . TB_PREF . "serial_numbers"
			. " WHERE stock_id = " . db_escape($stock_id)
			. " AND status IN ('available', 'returned', 'quarantine')"
			. " ORDER BY serial_no";
		echo "<tr><td class='label'>" . _('Serial Number:') . "</td><td>";
		echo combo_input('serial_id', get_post('serial_id'), $serial_sql, 'id', 'serial_no',
			array('spec_option' => _('-- None --'), 'spec_id' => '', 'order' => false));
		echo "</td></tr>";
	}
	if ($tracking_mode === 'batch' || $tracking_mode === 'both') {
		// Batch selector â€” show active batches for this item
		$batch_sql = "SELECT id, batch_no FROM " . TB_PREF . "stock_batches"
			. " WHERE stock_id = " . db_escape($stock_id)
			. " AND status IN ('active', 'quarantine')"
			. " ORDER BY batch_no";
		echo "<tr><td class='label'>" . _('Batch / Lot:') . "</td><td>";
		echo combo_input('batch_id', get_post('batch_id'), $batch_sql, 'id', 'batch_no',
			array('spec_option' => _('-- None --'), 'spec_id' => '', 'order' => false));
		echo "</td></tr>";
	}
}

// Memo
textarea_row(_('Memo:'), 'memo', get_post('memo'), 60, 3);

end_table(1);

submit_center('ADD_SCRAP', _('Create Scrap Entry'), true, _('Create a new scrap entry'), 'default');

end_form();
end_page();
