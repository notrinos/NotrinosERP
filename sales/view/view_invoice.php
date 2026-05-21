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

/**
 * Returns parent delivery numbers for a sales invoice.
 *
 * @param int $invoice_no Sales invoice transaction number.
 * @return array
 */
function get_invoice_parent_delivery_numbers($invoice_no)
{
	static $cache = array();

	$invoice_no = (int)$invoice_no;
	if (isset($cache[$invoice_no]))
		return $cache[$invoice_no];

	$sql = "SELECT DISTINCT del.debtor_trans_no"
		. " FROM " . TB_PREF . "debtor_trans_details inv"
		. " INNER JOIN " . TB_PREF . "debtor_trans_details del ON del.id = inv.src_id"
		. " WHERE inv.debtor_trans_type = " . ST_SALESINVOICE
		. " AND inv.debtor_trans_no = " . db_escape($invoice_no)
		. " AND del.debtor_trans_type = " . ST_CUSTDELIVERY
		. " ORDER BY del.debtor_trans_no";
	$result = db_query($sql, 'could not get invoice parent deliveries');
	$cache[$invoice_no] = array();
	while ($row = db_fetch($result))
		$cache[$invoice_no][] = (int)$row['debtor_trans_no'];

	return $cache[$invoice_no];
}

/**
 * Renders tracked serial and batch identifiers for an invoice line.
 *
 * @param int    $invoice_no Sales invoice transaction number.
 * @param string $stock_id   Inventory item code.
 * @param int    $colspan    Table column span.
 * @return void
 */
function display_invoice_line_tracking($invoice_no, $stock_id, $colspan)
{
	$tracking_mode = get_item_tracking_mode($stock_id);
	if ($tracking_mode === 'none')
		return;

	$delivery_numbers = get_invoice_parent_delivery_numbers($invoice_no);
	if (empty($delivery_numbers))
		return;

	$delivery_list = implode(',', array_map('intval', $delivery_numbers));

	if (item_has_serial_tracking($stock_id)) {
		$serial_sql = "SELECT DISTINCT sn.id, sn.serial_no, sn.status, sn.warranty_end"
			. " FROM " . TB_PREF . "serial_movements sm"
			. " INNER JOIN " . TB_PREF . "serial_numbers sn ON sn.id = sm.serial_id"
			. " WHERE sm.trans_type = " . ST_CUSTDELIVERY
			. " AND sm.trans_no IN (" . $delivery_list . ")"
			. " AND sn.stock_id = " . db_escape($stock_id)
			. " AND sm.to_status = 'delivered'"
			. " ORDER BY sn.serial_no";
		$serial_result = db_query($serial_sql, 'could not get invoice serials');
		$serial_parts = array();
		while ($serial = db_fetch($serial_result)) {
			$serial_link = viewer_link(
				htmlspecialchars($serial['serial_no']),
				'inventory/inquiry/serial_lifecycle.php?serial_id=' . (int)$serial['id']
			);
			if (!empty($serial['warranty_end']))
				$serial_link .= ' <span style="color:#888;">(' . _('Warranty until') . ': ' . sql2date($serial['warranty_end']) . ')</span>';
			$serial_parts[] = $serial_link;
		}
		if (!empty($serial_parts)) {
			echo '<tr><td colspan="' . (int)$colspan . '" style="padding:2px 8px 4px 24px; border-left:3px solid #5b9bd5; background:#f7f9fc; font-size:11px;">';
			echo '<b style="color:#5b9bd5;">' . _('Serials:') . '</b> ' . implode(', ', $serial_parts);
			echo '</td></tr>';
		}
	}

	if (item_has_batch_tracking($stock_id)) {
		$batch_sql = "SELECT DISTINCT sb.id, sb.batch_no, ABS(bm.quantity) AS qty, sb.expiry_date"
			. " FROM " . TB_PREF . "batch_movements bm"
			. " INNER JOIN " . TB_PREF . "stock_batches sb ON sb.id = bm.batch_id"
			. " WHERE bm.trans_type = " . ST_CUSTDELIVERY
			. " AND bm.trans_no IN (" . $delivery_list . ")"
			. " AND sb.stock_id = " . db_escape($stock_id)
			. " AND bm.quantity < 0"
			. " ORDER BY sb.batch_no";
		$batch_result = db_query($batch_sql, 'could not get invoice batches');
		$batch_parts = array();
		while ($batch = db_fetch($batch_result)) {
			$batch_link = viewer_link(
				htmlspecialchars($batch['batch_no']),
				'inventory/inquiry/batch_lifecycle.php?batch_id=' . (int)$batch['id']
			) . ' ×' . number_format2((float)$batch['qty'], get_qty_dec($stock_id));
			if (!empty($batch['expiry_date']))
				$batch_link .= ' <span style="color:#888;">(' . _('Exp') . ': ' . sql2date($batch['expiry_date']) . ')</span>';
			$batch_parts[] = $batch_link;
		}
		if (!empty($batch_parts)) {
			echo '<tr><td colspan="' . (int)$colspan . '" style="padding:2px 8px 4px 24px; border-left:3px solid #e6a23c; background:#fdf6ec; font-size:11px;">';
			echo '<b style="color:#e6a23c;">' . _('Batches:') . '</b> ' . implode(', ', $batch_parts);
			echo '</td></tr>';
		}
	}
}

$js = '';
if ($SysPrefs->use_popup_windows)
	$js .= get_js_open_window(900, 600);
page(_($help_context = 'View Sales Invoice'), true, false, '', $js);


$trans_id = 0;
if (isset($_GET['trans_no']))
	$trans_id = (int)$_GET['trans_no'];
elseif (isset($_POST['trans_no']))
	$trans_id = (int)$_POST['trans_no'];

if ($trans_id <= 0) {
	display_error(_('No sales invoice transaction was specified.'));
	end_page(true);
	exit;
}

if (!exists_customer_trans(ST_SALESINVOICE, $trans_id)) {
	display_error(_('Sales invoice not found.'));
	end_page(true);
	exit;
}

// 3 different queries to get the information - what a JOKE !!!!

$myrow = get_customer_trans($trans_id, ST_SALESINVOICE);
if (!$myrow) {
	display_error(_('Sales invoice not found.'));
	end_page(true);
	exit;
}

$paym = get_payment_terms($myrow['payment_terms']);
$branch = get_branch($myrow['branch_code']);
$sales_order = get_sales_order_header($myrow['order_'], ST_SALESORDER);

if (!empty($SysPrefs->prefs['company_logo_on_views']))
	company_logo_on_view();

display_heading(sprintf($myrow['prep_amount'] > 0 ? ($paym['days_before_due']>=0 ? _('FINAL INVOICE #%d') : _('PREPAYMENT INVOICE #%d')) : _('SALES INVOICE #%d'),$trans_id));

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
$th = array(_('Payment Terms'));
table_header($th); 
label_row(null, $paym['terms'], 'nowrap');
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
label_cells(_('Due Date'), sql2date($myrow['due_date']), "class='tableheader2'", 'nowrap');
if ($myrow['prep_amount']==0)
	label_cells(_('Deliveries'), get_customer_trans_view_str(ST_CUSTDELIVERY, get_sales_parent_numbers(ST_SALESINVOICE, $trans_id)), "class='tableheader2'");
label_cells(_('Invoice Date'), sql2date($myrow['tran_date']), "class='tableheader2'", 'nowrap');
end_row();
comments_display_row(ST_SALESINVOICE, $trans_id);
end_table();

end_view_columns(); // outer table


$result = get_customer_trans_details(ST_SALESINVOICE, $trans_id);

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

		display_invoice_line_tracking($trans_id, $myrow2['stock_id'], 7);
	} //end while there are line items to print out

	$display_sub_tot = price_format($sub_total);
	label_row(_('Sub-total'), $display_sub_tot, 'colspan=6 align=right', "nowrap align=right width='15%'");
}
else
	display_note(_('There are no line items on this invoice.'), 1, 2);

/*Print out the invoice text entered */
if ($myrow['ov_freight'] != 0.0) {
	$display_freight = price_format($myrow['ov_freight']);
	label_row(_('Shipping'), $display_freight, 'colspan=6 align=right', 'nowrap align=right');
}

$tax_items = get_trans_tax_details(ST_SALESINVOICE, $trans_id);
display_customer_trans_tax_details($tax_items, 6);

$display_total = price_format($myrow['ov_freight']+$myrow['ov_gst']+$myrow['ov_amount']+$myrow['ov_freight_tax']);

label_row(_('TOTAL INVOICE'), $display_total, 'colspan=6 align=right', 'nowrap align=right');
if ($myrow['prep_amount'])
	label_row(_('PREPAYMENT AMOUNT INVOICED'), '<b>'.price_format($myrow['prep_amount']).'</b>', 'colspan=6 align=right', 'nowrap align=right');
end_table(1);

$voided = is_voided_display(ST_SALESINVOICE, $trans_id, _('This invoice has been voided.'));

if (!$voided)
	display_allocations_to(PT_CUSTOMER, $myrow['debtor_no'], ST_SALESINVOICE, $trans_id, $myrow['Total']);

// Phase 5: RMA quick-link
if (!$voided && $_SESSION['wa_current_user']->can_access_page('SA_SALESRETURN')) {
	echo '<div style="margin-top:8px;">';
	echo '<a href="' . $path_to_root . '/sales/sales_rma_entry.php?New=1&source_type=' . ST_SALESINVOICE . '&source_no=' . (int)$trans_id . '" class="button">' . _('Request Return (RMA)') . '</a>';
	echo '</div>';
}

end_page(true, false, false, ST_SALESINVOICE, $trans_id);
