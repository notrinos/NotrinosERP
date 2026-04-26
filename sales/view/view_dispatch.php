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
$page_security = 'SA_SALESTRANSVIEW';
$path_to_root = '../..';
include_once($path_to_root . '/includes/session.inc');

include_once($path_to_root . '/sales/includes/sales_ui.inc');

include_once($path_to_root . '/sales/includes/sales_db.inc');
include_once($path_to_root . '/inventory/includes/db/serial_batch_db.inc');

$js = '';
if ($SysPrefs->use_popup_windows)
	$js .= get_js_open_window(900, 600);
page(_($help_context = 'View Sales Dispatch'), true, false, '', $js);


if (isset($_GET['trans_no']))
	$trans_id = $_GET['trans_no'];
elseif (isset($_POST['trans_no']))
	$trans_id = $_POST['trans_no'];

// 3 different queries to get the information - what a JOKE !!!!

$myrow = get_customer_trans($trans_id, ST_CUSTDELIVERY);

$branch = get_branch($myrow['branch_code']);

$sales_order = get_sales_order_header($myrow['order_'], ST_SALESORDER);

display_heading(sprintf(_('DISPATCH NOTE #%d'),$trans_id));

echo '<br>';
start_view_columns();
view_column_start(); // outer table

/*Now the customer charged to details in a sub table*/
start_table(TABLESTYLE, "width='100%'");
$th = array(_('Charge To'));
table_header($th);

label_row(null, $myrow['DebtorName'] . '<br>' . nl2br($myrow['address']), 'nowrap');

end_table();

/*end of the small table showing charge to account details */

view_column_next(); // outer table

/*end of the main table showing the company name and charge to details */

start_table(TABLESTYLE, "width='100%'");
$th = array(_('Charge Branch'));
table_header($th);

label_row(null, $branch['br_name'] . '<br>' . nl2br($branch['br_address']), 'nowrap');
end_table();

view_column_next(); // outer table

start_table(TABLESTYLE, "width='100%'");
$th = array(_('Delivered To'));
table_header($th);

label_row(null, $sales_order['deliver_to'] . '<br>' . nl2br($sales_order['delivery_address']),
	'nowrap');
end_table();

view_column_next(); // outer table

start_table(TABLESTYLE, "width='100%'");
start_row();
label_cells(_('Reference'), $myrow['reference'], "class='tableheader2'");
label_cells(_('Currency'), $sales_order['curr_code'], "class='tableheader2'");
label_cells(_('Our Order No'), get_customer_trans_view_str(ST_SALESORDER,$sales_order['order_no']), "class='tableheader2'");
end_row();
start_row();
label_cells(_('Customer Order Ref.'), $sales_order['customer_ref'], "class='tableheader2'");
label_cells(_('Shipping Company'), $myrow['shipper_name'], "class='tableheader2'");
label_cells(_('Sales Type'), $myrow['sales_type'], "class='tableheader2'");
end_row();
start_row();
label_cells(_('Dispatch Date'), sql2date($myrow['tran_date']), "class='tableheader2'", 'nowrap');
label_cells(_('Due Date'), sql2date($myrow["due_date"]), "class='tableheader2'", 'nowrap');
end_row();
comments_display_row(ST_CUSTDELIVERY, $trans_id);
end_table();

end_view_columns(); // outer table


$result = get_customer_trans_details(ST_CUSTDELIVERY, $trans_id);

start_table(TABLESTYLE, "width='95%'");

if (db_num_rows($result) > 0) {
	$th = array(_('Item Code'), _('Item Description'), _('Quantity'), _('Unit'), _('Price'), _('Discount %'), _('Total'));
	table_header($th);

	$k = 0;	//row colour counter
	$sub_total = 0;
	while ($myrow2 = db_fetch($result)) {
		if($myrow2['quantity']==0) continue;
		alt_table_row_color($k);

		$value = round2(((1 - $myrow2['discount_percent']) * $myrow2['unit_price'] * $myrow2['quantity']),
		   user_price_dec());
		$sub_total += $value;

		if ($myrow2['discount_percent'] == 0)
			$display_discount = '';
		else
			$display_discount = percent_format($myrow2['discount_percent']*100) . '%';

		label_cell($myrow2['stock_id']);
		label_cell($myrow2['StockDescription']);
		qty_cell($myrow2['quantity'], false, get_qty_dec($myrow2['stock_id']));
		label_cell($myrow2['units'], 'align=right');
		amount_cell($myrow2['unit_price']);
		label_cell($display_discount, 'nowrap align=right');
		amount_cell($value);
		end_row();

		// --- Show serial/batch tracking info per line ---
		$tracking_mode = get_item_tracking_mode($myrow2['stock_id']);
		if ($tracking_mode !== 'none') {
			$colspan_track = 7;

			// Query serial movements for this delivery + item
			if (item_has_serial_tracking($myrow2['stock_id'])) {
				$sn_sql = "SELECT sn.serial_no, sn.status, sn.warranty_start, sn.warranty_end "
					. "FROM " . TB_PREF . "serial_movements sm "
					. "INNER JOIN " . TB_PREF . "serial_numbers sn ON sm.serial_id = sn.id "
					. "WHERE sm.trans_type = " . ST_CUSTDELIVERY
					. " AND sm.trans_no = " . (int)$trans_id
					. " AND sn.stock_id = " . db_escape($myrow2['stock_id'])
					. " AND sm.to_status = 'delivered'";
				$sn_result = db_query($sn_sql, 'could not get delivery serials');
				$serials_found = array();
				while ($sn_row = db_fetch($sn_result)) {
					$serials_found[] = $sn_row;
				}

				if (!empty($serials_found)) {
					echo '<tr><td colspan="'.$colspan_track.'" style="padding:2px 8px 4px 24px; border-left:3px solid #5b9bd5; background:#f7f9fc; font-size:11px;">';
					echo '<b style="color:#5b9bd5;"><i class="fa fa-barcode"></i> '._('Serials:').'</b> ';
					$sn_parts = array();
					foreach ($serials_found as $sf) {
						$txt = htmlspecialchars($sf['serial_no']);
						if (!empty($sf['warranty_end'])) {
							$txt .= ' <span style="color:#888;">('._('Warranty until').': '.sql2date($sf['warranty_end']).')</span>';
						}
						$sn_parts[] = $txt;
					}
					echo implode(', ', $sn_parts);
					echo '</td></tr>';
				}
			}

			// Query batch movements for this delivery + item
			if (item_has_batch_tracking($myrow2['stock_id'])) {
				$bt_sql = "SELECT bm.batch_id, sb.batch_no, ABS(bm.quantity) as qty, sb.expiry_date "
					. "FROM " . TB_PREF . "batch_movements bm "
					. "INNER JOIN " . TB_PREF . "stock_batches sb ON bm.batch_id = sb.id "
					. "WHERE bm.trans_type = " . ST_CUSTDELIVERY
					. " AND bm.trans_no = " . (int)$trans_id
					. " AND sb.stock_id = " . db_escape($myrow2['stock_id'])
					. " AND bm.quantity < 0";
				$bt_result = db_query($bt_sql, 'could not get delivery batches');
				$batches_found = array();
				while ($bt_row = db_fetch($bt_result)) {
					$batches_found[] = $bt_row;
				}

				if (!empty($batches_found)) {
					echo '<tr><td colspan="'.$colspan_track.'" style="padding:2px 8px 4px 24px; border-left:3px solid #e6a23c; background:#fdf6ec; font-size:11px;">';
					echo '<b style="color:#e6a23c;"><i class="fa fa-cubes"></i> '._('Batches:').'</b> ';
					$bt_parts = array();
					foreach ($batches_found as $bf) {
						$txt = htmlspecialchars($bf['batch_no']) . ' ×' . number_format2((float)$bf['qty'], get_qty_dec($myrow2['stock_id']));
						if (!empty($bf['expiry_date'])) {
							$txt .= ' <span style="color:#888;">('._('Exp').': '.sql2date($bf['expiry_date']).')</span>';
						}
						$bt_parts[] = $txt;
					}
					echo implode(', ', $bt_parts);
					echo '</td></tr>';
				}
			}
		}
		// --- End tracking info ---
	}
	$display_sub_tot = price_format($sub_total);
	label_row(_('Sub-total'), $display_sub_tot, 'colspan=6 align=right', "nowrap align=right width='15%'");
}
else
	display_note(_('There are no line items on this dispatch.'), 1, 2);
if ($myrow['ov_freight'] != 0.0) {
	$display_freight = price_format($myrow['ov_freight']);
	label_row(_('Shipping'), $display_freight, 'colspan=6 align=right', 'nowrap align=right');
}

$tax_items = get_trans_tax_details(ST_CUSTDELIVERY, $trans_id);
display_customer_trans_tax_details($tax_items, 6);

$display_total = price_format($myrow['ov_freight']+$myrow['ov_amount']+$myrow['ov_freight_tax']+$myrow['ov_gst']);

label_row(_('TOTAL VALUE'), $display_total, 'colspan=6 align=right', 'nowrap align=right');
end_table(1);

is_voided_display(ST_CUSTDELIVERY, $trans_id, _('This dispatch has been voided.'));

// Phase 5: RMA quick-link
if ($_SESSION['wa_current_user']->can_access_page('SA_SALESRETURN')) {
	echo '<div style="margin-top:8px;">';
	echo '<a href="' . $path_to_root . '/sales/sales_rma_entry.php?New=1&source_type=' . ST_CUSTDELIVERY . '&source_no=' . (int)$trans_id . '" class="button">' . _('Request Return (RMA)') . '</a>';
	echo '</div>';
}

end_page(true, false, false, ST_CUSTDELIVERY, $trans_id);
