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
$page_security = 'SA_SUPPTRANSVIEW';
$path_to_root = '../..';
include($path_to_root.'/purchasing/includes/po_class.inc');

include($path_to_root.'/includes/session.inc');

$js = '';
if ($SysPrefs->use_popup_windows)
	$js .= get_js_open_window(900, 500);
page(_($help_context = 'View Purchase Order Delivery'), true, false, '', $js);

include($path_to_root.'/purchasing/includes/purchasing_ui.inc');

if (!isset($_GET['trans_no']))
	die ('<BR>'._('This page must be called with a Purchase Order Delivery number to review.'));

$purchase_order = new purch_order;
read_grn($_GET['trans_no'], $purchase_order);

display_heading(_('Purchase Order Delivery').' #'.$_GET['trans_no']);
echo '<BR>';
display_grn_summary($purchase_order);

display_heading2(_('Line Details'));

// Pre-load tracking data from grn_items for this GRN
$grn_tracking = array();
$tracking_sql = "SELECT gi.item_code, gi.serial_numbers_json, gi.batch_id, gi.expiry_date,
	gi.manufacturing_date, gi.inspection_status,
	sb.batch_no AS batch_code
	FROM " . TB_PREF . "grn_items gi
	LEFT JOIN " . TB_PREF . "stock_batches sb ON gi.batch_id = sb.id
	WHERE gi.grn_batch_id = " . db_escape($_GET['trans_no']);
$tracking_result = db_query($tracking_sql, 'could not get tracking data');
while ($trow = db_fetch($tracking_result)) {
	$grn_tracking[$trow['item_code']] = $trow;
}

start_table(TABLESTYLE, "width='90%'");
$th = array(_('Item Code'), _('Item Description'), _('Required by'), _('Quantity'), _('Unit'), _('Price'), _('Line Total'), _('Quantity Invoiced'));

table_header($th);

$total = 0;
$k = 0;  //row colour counter
$overdue_items = false;

foreach ($purchase_order->line_items as $stock_item) {

	$line_total = $stock_item->qty_received * $stock_item->price;

	// if overdue and outstanding quantities, then highlight as so
	if (date1_greater_date2($purchase_order->orig_order_date, $stock_item->req_del_date)) {
		start_row("class='overduebg'");
		$overdue_items = true;
	}
	else
		alt_table_row_color($k);

	label_cell($stock_item->stock_id);
	label_cell($stock_item->item_description);
	label_cell($stock_item->req_del_date, 'nowrap align=right');
	$dec = get_qty_dec($stock_item->stock_id);
	qty_cell($stock_item->qty_received, false, $dec);
	label_cell($stock_item->units);
	amount_decimal_cell($stock_item->price);
	amount_cell($line_total);
	qty_cell($stock_item->qty_inv, false, $dec);
	end_row();

	// --- Advanced Inventory: display serial/batch tracking info ---
	if (isset($grn_tracking[$stock_item->stock_id])) {
		$track = $grn_tracking[$stock_item->stock_id];
		$has_tracking = false;
		$tracking_parts = array();

		// Serial numbers (db_escape stores HTML-encoded data, decode before JSON parse)
		if (!empty($track['serial_numbers_json'])) {
			$raw_json = html_entity_decode($track['serial_numbers_json'], ENT_QUOTES, 'UTF-8');
			$serials = json_decode($raw_json, true);
			if (!empty($serials)) {
				$has_tracking = true;
				$tracking_parts[] = '<b>' . _('Serials:') . '</b> ' . htmlspecialchars(implode(', ', $serials));
			}
		}

		// Batch info
		if (!empty($track['batch_id']) && !empty($track['batch_code'])) {
			$has_tracking = true;
			$batch_info = '<b>' . _('Batch:') . '</b> ' . htmlspecialchars($track['batch_code']);
			if (!empty($track['expiry_date']) && $track['expiry_date'] !== '0000-00-00')
				$batch_info .= ' &nbsp; <b>' . _('Expiry:') . '</b> ' . sql2date($track['expiry_date']);
			if (!empty($track['manufacturing_date']) && $track['manufacturing_date'] !== '0000-00-00')
				$batch_info .= ' &nbsp; <b>' . _('Mfg:') . '</b> ' . sql2date($track['manufacturing_date']);
			$tracking_parts[] = $batch_info;
		}

		// Inspection status
		if (!empty($track['inspection_status']) && $track['inspection_status'] !== 'none') {
			$has_tracking = true;
			$status_colors = array('pending' => '#ffc107', 'pass' => '#28a745', 'fail' => '#dc3545');
			$scolor = isset($status_colors[$track['inspection_status']]) ? $status_colors[$track['inspection_status']] : '#6c757d';
			$tracking_parts[] = '<b>' . _('QC:') . '</b> <span style="color:' . $scolor . '; font-weight:bold;">'
				. ucfirst($track['inspection_status']) . '</span>';
		}

		if ($has_tracking) {
			echo '<tr><td></td>';
			echo '<td colspan="7" style="padding:3px 8px; background:#f0f4f8; border-left:3px solid #5b9bd5; font-size:11px;">';
			echo implode(' &nbsp;|&nbsp; ', $tracking_parts);
			echo '</td></tr>';
		}
	}
	// --- End tracking display ---

	$total += $line_total;
}

$display_sub_tot = number_format2($total,user_price_dec());
label_row(_('Sub Total'), $display_sub_tot, 'align=right colspan=6', 'nowrap align=right', 1);

$taxes = $purchase_order->get_taxes();
$tax_total = display_edit_tax_items($taxes, 6, $purchase_order->tax_included, 1);

$display_total = price_format(($total + $tax_total));

start_row();
label_cells(_('Amount Total'), $display_total, "colspan=6 align='right'","align='right'");
label_cell('');
end_row();

end_table(1);

if ($overdue_items)
	display_note(_('Marked items were delivered overdue.'), 0, 0, "class='overduefg'");

is_voided_display(ST_SUPPRECEIVE, $_GET['trans_no'], _('This delivery has been voided.'));

end_page(true, false, false, ST_SUPPRECEIVE, $_GET['trans_no']);
