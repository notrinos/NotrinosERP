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
 * View Transfer Order — read-only detail view with status timeline.
 *
 * Accessible as a popup or standalone page via ?transfer_id=N
 *
 * @package NotrinosERP
 * @subpackage Inventory/Warehouse
 */

$page_security = 'SA_TRANSFERORDERS';
$path_to_root = '../../..';

include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/includes/date_functions.inc');
include_once($path_to_root . '/inventory/includes/inventory_db.inc');
include_once($path_to_root . '/inventory/warehouse/includes/db/warehouse_transfers_db.inc');

page(_($help_context = 'View Transfer Order'), true);

$transfer_id = 0;
if (isset($_GET['transfer_id']))
	$transfer_id = (int)$_GET['transfer_id'];
elseif (isset($_GET['id']))
	$transfer_id = (int)$_GET['id'];

if ($transfer_id <= 0) {
	display_error(_('No transfer order specified.'));
	end_page(true);
	exit;
}

$order = get_transfer_order($transfer_id);
if (!$order) {
	display_error(sprintf(_('Transfer Order #%d not found.'), $transfer_id));
	end_page(true);
	exit;
}

// =====================================================================
// Display header
// =====================================================================

display_heading(sprintf(_('Transfer Order #%d'), $transfer_id));

// Status timeline
echo '<div style="display:flex;align-items:center;gap:0;margin:12px auto 16px;max-width:700px;padding:8px 16px;background:#f8f9fa;border-radius:6px;">';
$timeline_statuses = array('draft', 'approved', 'shipped', 'in_transit', 'received');
$current_idx = array_search($order['status'], $timeline_statuses);
if ($order['status'] === 'cancelled') $current_idx = -1;

foreach ($timeline_statuses as $idx => $ts) {
	$sinfo = get_transfer_order_statuses();
	$label = $sinfo[$ts];
	$color = get_transfer_order_status_color($ts);

	$is_active = ($idx <= $current_idx);
	$is_current = ($ts === $order['status']);

	$bg = $is_active ? $color : '#dee2e6';
	$text_color = $is_active ? '#fff' : '#6c757d';
	$border = $is_current ? '3px solid #333' : '2px solid transparent';
	$font_weight = $is_current ? 'bold' : 'normal';

	echo '<div style="flex:1;text-align:center;padding:8px 4px;background:'.$bg.';color:'.$text_color.';font-size:11px;font-weight:'.$font_weight.';border:'.$border.';';
	if ($idx === 0) echo 'border-radius:4px 0 0 4px;';
	elseif ($idx === count($timeline_statuses) - 1) echo 'border-radius:0 4px 4px 0;';
	echo '">'.$label.'</div>';
}
if ($order['status'] === 'cancelled') {
	echo '<div style="flex:1;text-align:center;padding:8px 4px;background:#dc3545;color:#fff;font-size:11px;font-weight:bold;border:3px solid #333;border-radius:0 4px 4px 0;">'._('Cancelled').'</div>';
}
echo '</div>';

// =====================================================================
// Order details
// =====================================================================

echo '<br>';
start_table(TABLESTYLE2, "width='90%'");

start_row();
label_cells(_('Transfer Type'), ucfirst($order['transfer_type']), "class='tableheader2'");
label_cells(_('Status'), transfer_order_status_badge($order['status']), "class='tableheader2'");
end_row();

start_row();
label_cells(_('From Location'), $order['from_loc_name'], "class='tableheader2'");
label_cells(_('To Location'), $order['to_loc_name'], "class='tableheader2'");
end_row();

start_row();
$req_date = !empty($order['request_date']) ? sql2date($order['request_date']) : '-';
$exp_date = !empty($order['expected_date']) ? sql2date($order['expected_date']) : '-';
label_cells(_('Request Date'), $req_date, "class='tableheader2'");
label_cells(_('Expected Date'), $exp_date, "class='tableheader2'");
end_row();

start_row();
$ship_date = !empty($order['ship_date']) ? sql2date($order['ship_date']) : '-';
$recv_date = !empty($order['receive_date']) ? sql2date($order['receive_date']) : '-';
label_cells(_('Ship Date'), $ship_date, "class='tableheader2'");
label_cells(_('Receive Date'), $recv_date, "class='tableheader2'");
end_row();

start_row();
label_cells(_('Reference'), $order['reference'], "class='tableheader2'");
label_cells(_('Memo'), $order['memo'], "class='tableheader2'");
end_row();

// Audit info
start_row();
$created_by = !empty($order['requested_by_name']) ? $order['requested_by_name'] : ('#'.$order['requested_by']);
$shipped_by = !empty($order['shipped_by']) ? ('#'.$order['shipped_by']) : '-';
label_cells(_('Requested By'), $created_by, "class='tableheader2'");
label_cells(_('Shipped By'), $shipped_by, "class='tableheader2'");
end_row();

end_table(2);

// =====================================================================
// Line items
// =====================================================================

display_heading2(_('Line Items'));

$lines = get_transfer_order_lines($transfer_id);

start_table(TABLESTYLE, "width='90%'");
$th = array(_('Item'), _('Description'), _('Qty Requested'), _('Qty Shipped'),
	_('Qty Received'), _('Outstanding'), _('Unit Cost'), _('Total Cost'));
table_header($th);

$k = 0;
$total_cost = 0;
$total_outstanding = 0;
while ($ln = db_fetch($lines)) {
	alt_table_row_color($k);

	label_cell($ln['stock_id']);
	label_cell($ln['item_description']);

	$dec = get_qty_dec($ln['stock_id']);
	qty_cell((float)$ln['qty_requested'], false, $dec);
	qty_cell((float)$ln['qty_shipped'], false, $dec);
	qty_cell((float)$ln['qty_received'], false, $dec);

	$outstanding = (float)$ln['qty_requested'] - (float)$ln['qty_received'];
	if ($outstanding < 0) $outstanding = 0;
	$total_outstanding += $outstanding;

	// Color outstanding if > 0
	if ($outstanding > 0)
		echo '<td align="right" style="color:#d32f2f;font-weight:bold;">'.number_format2($outstanding, $dec).'</td>';
	else
		qty_cell(0, false, $dec);

	amount_decimal_cell((float)$ln['unit_cost']);
	$line_total = (float)$ln['unit_cost'] * (float)$ln['qty_requested'];
	$total_cost += $line_total;
	amount_cell($line_total);

	end_row();
}

label_row(_('Total'), number_format2($total_cost, user_price_dec()), 'colspan=7 align=right', 'align=right');
end_table(1);

// Show outstanding warning
if ($total_outstanding > 0 && in_array($order['status'], array('shipped', 'in_transit'))) {
	echo '<div style="background:#fff3cd;padding:8px 12px;border-radius:4px;border:1px solid #ffc107;font-size:12px;">';
	echo '<i class="fa fa-exclamation-triangle" style="color:#ffc107;"></i> ';
	echo _('This order has outstanding items awaiting receipt.');
	echo '</div>';
}

end_page(true, false, false);
