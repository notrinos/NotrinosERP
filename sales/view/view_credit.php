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

include_once($path_to_root . '/includes/date_functions.inc');
include_once($path_to_root . '/includes/ui.inc');

include_once($path_to_root . '/sales/includes/sales_db.inc');

$js = '';
if ($SysPrefs->use_popup_windows)
	$js .= get_js_open_window(900, 500);
page(_($help_context = 'View Credit Note'), true, false, '', $js);

if (isset($_GET['trans_no']))
	$trans_id = $_GET['trans_no'];
elseif (isset($_POST['trans_no']))
	$trans_id = $_POST['trans_no'];

$myrow = get_customer_trans($trans_id, ST_CUSTCREDIT);

$branch = get_branch($myrow['branch_code']);

display_heading('<font color=red>' . sprintf(_('CREDIT NOTE #%d'), $trans_id). '</font>');
echo '<br>';

start_view_columns();
view_column_start(); // outer table

/*Now the customer charged to details in a sub table*/
start_table(TABLESTYLE, "width='100%'");
$th = array(_('Customer'));
table_header($th);

label_row(null, $myrow['DebtorName'] . '<br>' . nl2br($myrow['address']), 'nowrap');

end_table();
/*end of the small table showing charge to account details */

view_column_next(); // outer table

start_table(TABLESTYLE, "width='100%'");
$th = array(_('Branch'));
table_header($th);

label_row(null, $branch['br_name'] . '<br>' . nl2br($branch['br_address']), 'nowrap');
end_table();

view_column_next(); // outer table

start_table(TABLESTYLE, "width='100%'");
start_row();
label_cells(_('Ref'), $myrow['reference'], "class='tableheader2'");
label_cells(_('Date'), sql2date($myrow['tran_date']), "class='tableheader2'");
label_cells(_('Currency'), $myrow['curr_code'], "class='tableheader2'");
end_row();
start_row();
label_cells(_('Sales Type'), $myrow['sales_type'], "class='tableheader2'");
label_cells(_('Shipping Company'), $myrow['shipper_name'], "class='tableheader2'");
end_row();
comments_display_row(ST_CUSTCREDIT, $trans_id);
end_table();

end_view_columns(); // outer table

$sub_total = 0;

$result = get_customer_trans_details(ST_CUSTCREDIT, $trans_id);

start_table(TABLESTYLE, "width='95%'");

if (db_num_rows($result) > 0) {
	$th = array(_('Item Code'), _('Item Description'), _('Quantity'), _('Unit'), _('Price'), _('Discount %'), _('Total'));
	table_header($th);

	$k = 0;	//row colour counter
	$sub_total = 0;

	while ($myrow2 = db_fetch($result)) {
		if ($myrow2['quantity'] == 0)
			continue;
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
		label_cell($display_discount, 'align=right');
		amount_cell($value);
		end_row();

		// --- Advanced Inventory: show serial/batch tracking info for this credit line ---
		$serial_batch_db_path = $path_to_root . '/inventory/includes/db/serial_batch_db.inc';
		if (file_exists($serial_batch_db_path)) {
			include_once($serial_batch_db_path);
			if (function_exists('get_item_tracking_mode')) {
				$tracking_mode = get_item_tracking_mode($myrow2['stock_id']);
				if ($tracking_mode !== 'none') {
					// Look up tracking data from stock_moves for this credit note line
					$track_sql = "SELECT serial_id, batch_id FROM " . TB_PREF . "stock_moves "
						. "WHERE type=" . ST_CUSTCREDIT
						. " AND trans_no=" . db_escape($trans_id)
						. " AND stock_id=" . db_escape($myrow2['stock_id'])
						. " AND (serial_id IS NOT NULL AND serial_id > 0 OR batch_id IS NOT NULL AND batch_id > 0)"
						. " LIMIT 50";
					$track_result = db_query($track_sql, 'could not get tracking data');
					$serial_ids = array();
					$batch_ids = array();
					while ($tr = db_fetch($track_result)) {
						if (!empty($tr['serial_id'])) $serial_ids[$tr['serial_id']] = $tr['serial_id'];
						if (!empty($tr['batch_id'])) $batch_ids[$tr['batch_id']] = $tr['batch_id'];
					}

					$tracking_parts = array();

					if (!empty($serial_ids)) {
						include_once($path_to_root . '/inventory/includes/db/serial_numbers_db.inc');
						$serial_labels = array();
						foreach ($serial_ids as $sid) {
							$sn_sql = "SELECT serial_no FROM " . TB_PREF . "serial_numbers WHERE id=" . (int)$sid;
							$sn_row = db_fetch(db_query($sn_sql, 'could not get serial'));
							if ($sn_row) $serial_labels[] = htmlspecialchars($sn_row['serial_no']);
						}
						if (!empty($serial_labels)) {
							$tracking_parts[] = '<i class="fa fa-barcode"></i> <b>'._('Serials:').'</b> ' . implode(', ', $serial_labels);
						}
					}

					if (!empty($batch_ids)) {
						include_once($path_to_root . '/inventory/includes/db/stock_batches_db.inc');
						$batch_labels = array();
						foreach ($batch_ids as $bid) {
							$bn_sql = "SELECT batch_no, expiry_date FROM " . TB_PREF . "stock_batches WHERE id=" . (int)$bid;
							$bn_row = db_fetch(db_query($bn_sql, 'could not get batch'));
							if ($bn_row) {
								$bl = htmlspecialchars($bn_row['batch_no']);
								if ($bn_row['expiry_date']) $bl .= ' (exp:' . sql2date($bn_row['expiry_date']) . ')';
								$batch_labels[] = $bl;
							}
						}
						if (!empty($batch_labels)) {
							$tracking_parts[] = '<i class="fa fa-cubes"></i> <b>'._('Batches:').'</b> ' . implode(', ', $batch_labels);
						}
					}

					if (!empty($tracking_parts)) {
						echo '<tr><td colspan="7" style="padding:3px 8px 6px 24px; border-left:3px solid #409eff; background:#ecf5ff; font-size:12px;">';
						echo implode(' &nbsp;|&nbsp; ', $tracking_parts);
						echo '</td></tr>';
					}
				}
			}
		}
		// --- End serial/batch tracking display ---
	}
}
else
	display_note(_('There are no line items on this credit note.'), 1, 2);

$display_sub_tot = price_format($sub_total);
$credit_total = $myrow['ov_freight']+$myrow['ov_gst']+$myrow['ov_amount']+$myrow['ov_freight_tax'];
$display_total = price_format($credit_total);

/*Print out the invoice text entered */
if ($sub_total != 0)
	label_row(_('Sub Total'), $display_sub_tot, 'colspan=6 align=right', "nowrap align=right width='15%'");
if ($myrow['ov_freight'] != 0.0) {
	$display_freight = price_format($myrow['ov_freight']);
	label_row(_('Shipping'), $display_freight, 'colspan=6 align=right', 'nowrap align=right');
}

$tax_items = get_trans_tax_details(ST_CUSTCREDIT, $trans_id);
display_customer_trans_tax_details($tax_items, 6);

label_row('<font color=red>' . _('TOTAL CREDIT') . '</font', '<font color=red>'.$display_total.'</font>', 'colspan=6 align=right', 'nowrap align=right');
end_table(1);

$voided = is_voided_display(ST_CUSTCREDIT, $trans_id, _('This credit note has been voided.'));

if (!$voided)
	display_allocations_from(PT_CUSTOMER, $myrow['debtor_no'], ST_CUSTCREDIT, $trans_id, $credit_total);

/* end of check to see that there was an invoice record to print */

end_page(true, false, false, ST_CUSTCREDIT, $trans_id);
