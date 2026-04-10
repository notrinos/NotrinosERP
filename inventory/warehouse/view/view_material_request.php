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
 * View Material Request — read-only detail view with status timeline.
 *
 * Accessible as a popup or standalone page via ?request_id=N
 *
 * @package NotrinosERP
 * @subpackage Inventory/Warehouse
 */

$page_security = 'SA_MATERIALREQ_INQUIRY';
$path_to_root = '../../..';

include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/includes/date_functions.inc');
include_once($path_to_root . '/inventory/warehouse/includes/db/warehouse_material_requests_db.inc');

page(_($help_context = 'View Material Request'), true);

$request_id = 0;
if (isset($_GET['request_id']))
	$request_id = (int)$_GET['request_id'];
elseif (isset($_GET['id']))
	$request_id = (int)$_GET['id'];

if ($request_id <= 0) {
	display_error(_('No material request specified.'));
	end_page(true);
	exit;
}

$mr = get_material_request($request_id);
if (!$mr) {
	display_error(sprintf(_('Material Request #%d not found.'), $request_id));
	end_page(true);
	exit;
}

// =====================================================================
// Display header
// =====================================================================

display_heading(sprintf(_('Material Request %s'), format_material_request_no($mr['request_no'])));

// Status timeline
echo '<div style="display:flex;align-items:center;gap:0;margin:12px auto 16px;max-width:700px;padding:8px 16px;background:#f8f9fa;border-radius:6px;">';
$timeline_statuses = array('draft', 'submitted', 'approved', 'ordered', 'fulfilled');
$current_idx = array_search($mr['status'], $timeline_statuses);
if ($mr['status'] === 'cancelled') $current_idx = -1;

foreach ($timeline_statuses as $idx => $ts) {
	$sinfo = get_material_request_statuses();
	$label = $sinfo[$ts];
	$color = get_material_request_status_color($ts);

	$is_active = ($idx <= $current_idx);
	$is_current = ($ts === $mr['status']);

	$bg = $is_active ? $color : '#dee2e6';
	$text_color = $is_active ? '#fff' : '#6c757d';
	$border = $is_current ? '3px solid #333' : '2px solid transparent';
	$font_weight = $is_current ? 'bold' : 'normal';

	echo '<div style="flex:1;text-align:center;padding:8px 4px;background:' . $bg . ';color:' . $text_color
		. ';font-size:11px;font-weight:' . $font_weight . ';border:' . $border . ';';
	if ($idx === 0) echo 'border-radius:4px 0 0 4px;';
	elseif ($idx === count($timeline_statuses) - 1) echo 'border-radius:0 4px 4px 0;';
	echo '">' . $label . '</div>';
}
if ($mr['status'] === 'cancelled') {
	echo '<div style="flex:1;text-align:center;padding:8px 4px;background:#dc3545;color:#fff;font-size:11px;font-weight:bold;border:3px solid #333;border-radius:0 4px 4px 0;">' . _('Cancelled') . '</div>';
}
echo '</div>';

// =====================================================================
// Request details
// =====================================================================

echo '<br>';
start_table(TABLESTYLE2, "width='90%'");

start_row();
label_cells(_('Request Type'), material_request_type_badge($mr['request_type']), "class='tableheader2'");
label_cells(_('Status'), material_request_status_badge($mr['status']), "class='tableheader2'");
end_row();

start_row();
label_cells(_('Warehouse'), $mr['warehouse_name'], "class='tableheader2'");
$req_date = !empty($mr['request_date']) ? sql2date($mr['request_date']) : '-';
label_cells(_('Request Date'), $req_date, "class='tableheader2'");
end_row();

start_row();
$req_by_date = !empty($mr['required_date']) ? sql2date($mr['required_date']) : '-';
label_cells(_('Required Date'), $req_by_date, "class='tableheader2'");
label_cells(_('Reference'), $mr['reference'] ? $mr['reference'] : '-', "class='tableheader2'");
end_row();

start_row();
label_cells(_('Requested By'), $mr['requested_by_name'] ? $mr['requested_by_name'] : '-', "class='tableheader2'");
label_cells(_('Approved By'), $mr['approved_by_name'] ? $mr['approved_by_name'] : '-', "class='tableheader2'");
end_row();

if ($mr['memo']) {
	start_row();
	label_cells(_('Memo'), $mr['memo'], "class='tableheader2'", '', 3);
	end_row();
}

// Linked document
if ($mr['linked_doc_type'] && $mr['linked_doc_no']) {
	$doc_types = array(
		ST_PURCHORDER  => _('Purchase Order'),
		ST_LOCTRANSFER => _('Transfer Order'),
		ST_WORKORDER   => _('Work Order'),
	);
	$doc_label = isset($doc_types[$mr['linked_doc_type']]) ? $doc_types[$mr['linked_doc_type']] : _('Document');
	start_row();
	label_cells(_('Linked Document'), $doc_label . ' #' . $mr['linked_doc_no'], "class='tableheader2'", '', 3);
	end_row();
}

// Fulfillment
$fulfillment_pct = get_material_request_fulfillment_pct($request_id);
$pct_color = '#6c757d';
if ($fulfillment_pct >= 100) $pct_color = '#28a745';
elseif ($fulfillment_pct > 0) $pct_color = '#fd7e14';

start_row();
echo '<td class="tableheader2">' . _('Fulfillment') . '</td>';
echo '<td colspan=3>';
echo '<div style="display:inline-flex;align-items:center;gap:8px;">';
echo '<div style="width:300px;height:18px;background:#e9ecef;border-radius:8px;overflow:hidden;">';
echo '<div style="width:' . $fulfillment_pct . '%;height:100%;background:' . $pct_color . ';border-radius:8px;"></div>';
echo '</div>';
echo '<span style="font-weight:bold;color:' . $pct_color . ';">' . $fulfillment_pct . '%</span>';
echo '</div></td>';
end_row();

// Estimated value
start_row();
$total_value = get_material_request_total_value($request_id);
label_cells(_('Estimated Value'), price_format($total_value), "class='tableheader2'", '', 3);
end_row();

end_table(2);

// =====================================================================
// Line items
// =====================================================================

display_heading2(_('Line Items'));

$lines = get_material_request_lines($request_id);

start_table(TABLESTYLE, "width='90%'");
$th = array(_('Item'), _('Description'), _('Qty Requested'), _('Qty Fulfilled'),
	_('Remaining'), _('Unit'), _('Est. Cost'), _('Required Date'), _('Memo'));
table_header($th);

$k = 0;
$total_cost = 0;
while ($ln = db_fetch($lines)) {
	alt_table_row_color($k);

	label_cell($ln['stock_id']);
	label_cell($ln['item_description']);

	$dec = get_qty_dec($ln['stock_id']);
	qty_cell((float)$ln['qty_requested'], false, $dec);
	qty_cell((float)$ln['qty_fulfilled'], false, $dec);

	$remaining = (float)$ln['qty_requested'] - (float)$ln['qty_fulfilled'];
	if ($remaining < 0) $remaining = 0;

	// Color outstanding
	if ($remaining > 0)
		echo '<td align="right" style="color:#d32f2f;font-weight:bold;">' . number_format2($remaining, $dec) . '</td>';
	else
		echo '<td align="right" style="color:#28a745;font-weight:bold;">0</td>';

	label_cell($ln['item_units']);

	$line_cost = (float)$ln['material_cost'] * (float)$ln['qty_requested'];
	$total_cost += $line_cost;
	amount_cell($line_cost);

	label_cell($ln['required_date'] ? sql2date($ln['required_date']) : '-');
	label_cell($ln['memo'] ? $ln['memo'] : '-');

	end_row();
}

if ($k == 0) {
	label_row('', _('No line items.'), 'colspan=9 align=center');
}

label_row(_('Total Estimated Cost'), price_format($total_cost), 'colspan=6 align=right', 'align=right colspan=3');
end_table(1);

// Outstanding warning
if ($mr['status'] === 'ordered' && $fulfillment_pct < 100) {
	echo '<div style="background:#fff3cd;padding:8px 12px;border-radius:4px;border:1px solid #ffc107;font-size:12px;max-width:90%;margin:0 auto;">';
	echo '<i class="fa fa-exclamation-triangle" style="color:#ffc107;"></i> ';
	echo _('This request has outstanding items awaiting fulfillment from the linked document.');
	echo '</div>';
}

end_page(true, false, false);
