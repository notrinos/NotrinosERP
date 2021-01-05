<?php
/**********************************************************************
	Copyright (C) FrontAccounting, LLC.
	Released under the terms of the GNU General Public License, GPL, 
	as published by the Free Software Foundation, either version 3 
	of the License, or (at your option) any later version.
	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  
	See the License here <http://www.gnu.org/licenses/gpl-3.0.html>.
***********************************************************************/
$page_security = 'SA_SALESINVOICE';
$path_to_root = '..';
include_once($path_to_root . '/sales/includes/cart_class.inc');
include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/sales/includes/ui/sales_order_ui.inc');
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/reporting/includes/reporting.inc');

$js = '';
if ($SysPrefs->use_popup_windows)
	$js .= get_js_open_window(900, 600);
if (user_use_date_picker())
	$js .= get_js_date_picker();

page(_($help_context = 'Create and Print Recurrent Invoices'), false, false, '', $js);

function create_recurrent_invoices($customer_id, $branch_id, $order_no, $tmpl_no, $date, $from, $to, $memo) {
	global $Refs, $SysPrefs;

	update_last_sent_recurrent_invoice($tmpl_no, $to);

	$doc = new Cart(ST_SALESORDER, array($order_no));
	
	if (!empty($SysPrefs->prefs['dim_on_recurrent_invoice']))
		$doc->trans_type = ST_SALESINVOICE;
	
	get_customer_details_to_order($doc, $customer_id, $branch_id);

	$doc->trans_type = ST_SALESORDER;
	$doc->trans_no = 0;
	$doc->document_date = $date;

	$doc->due_date = get_invoice_duedate($doc->payment, $doc->document_date);

	$doc->reference = $Refs->get_next($doc->trans_type, null, array('customer' => $customer_id, 'branch' => $branch_id,
		'date' => $date));
	$doc->Comments = $memo;

	foreach ($doc->line_items as $line_no=>$item) {
		$line = &$doc->line_items[$line_no];
		$new_price = get_price($line->stock_id, $doc->customer_currency,
			$doc->sales_type, $doc->price_factor, $doc->document_date);
		if ($new_price != 0)	// use template price if no price is currently set for the item.
			$line->price = $new_price;
	}	
	$cart = $doc;
	$cart->trans_type = ST_SALESINVOICE;
	$cart->reference = $Refs->get_next($cart->trans_type);
	$cart->payment_terms['cash_sale'] = false; // no way to register cash payment with recurrent invoice at once
	$invno = $cart->write(1);

	return $invno;
}

function calculate_from($myrow) {
	if ($myrow['last_sent'] == '0000-00-00')
		$from = sql2date($myrow['begin']);
	else
		$from = sql2date($myrow['last_sent']);
	return $from;
}

function calculate_next($myrow) {
	if ($myrow['last_sent'] == '0000-00-00')
		$next = sql2date($myrow['begin']);
	else
		$next = sql2date($myrow['last_sent']);
	$next = add_months($next, $myrow['monthly']);
	$next = add_days($next, $myrow['days']);
	return add_days($next,-1);
}

$id = find_submit('confirmed');
if ($id != -1 && is_date_closed($_POST['trans_date'])) {
	display_error(_('The entered date is out of fiscal year or is closed for further data entry.'));
	set_focus('trans_date');
	$_POST['create'.$id] = 1;	//re-display current page
	$id = -1;
}

if ($id != -1) {
	/*
		whole invoiced time is <begin, end>
		invoices are issued _after_ invoiced period is gone, eg:
		begin 1.1
		end	  31.3
		period:	   invoice ready for issue since:
		1.1-31.1 -	1.2
		1.2-28.2 -	1.3
		1.3-31.3 -	1.4
		In example above, when end is set to 1.4 will generate additional invoice on 1.5 !
	*/

	$Ajax->activate('_page_body');
	$from = get_post('from');
	$to = get_post('to');
	$memo = get_post('memo');
	$date = $_POST['trans_date'];
	$myrow = get_recurrent_invoice($id);

	$invs = array();
	if (recurrent_invoice_ready($id, $date)) {
			begin_transaction();

			if ($myrow['debtor_no'] == 0) {
				$cust = get_cust_branches_from_group($myrow['group_no']);
				while ($row = db_fetch($cust)) {
					$invs[] = create_recurrent_invoices($row['debtor_no'], $row['branch_code'], $myrow['order_no'], $myrow['id'], $date, $from, $to, $memo);
				}
			}
			else
				$invs[] = create_recurrent_invoices($myrow['debtor_no'], $myrow['group_no'], $myrow['order_no'], $myrow['id'], $date, $from, $to, $memo);
			
			commit_transaction();
	}
	if (count($invs) > 0) {
		$min = min($invs);
		$max = max($invs);
	}
	else 
		$min = $max = 0;
	display_notification(sprintf(_('%s recurrent invoice(s) created, # %s - # %s.'), count($invs), $min, $max));
	if (count($invs) > 0) {
		$ar = array('PARAM_0' => $min.'-'.ST_SALESINVOICE,	'PARAM_1' => $max.'-'.ST_SALESINVOICE, 'PARAM_2' => '',
			'PARAM_3' => 0,	'PARAM_4' => 0,	'PARAM_5' => '', 'PARAM_6' => '', 'PARAM_7' => user_def_print_orientation());
		display_note(print_link(sprintf(_('&Print Recurrent Invoices # %s - # %s'), $min, $max), 107, $ar), 0, 1);
		$ar['PARAM_3'] = 1; // email
		display_note(print_link(sprintf(_('&Email Recurrent Invoices # %s - # %s'), $min, $max), 107, $ar), 0, 1);
	}
}

$id = find_submit('create');
if ($id != -1) {
	$Ajax->activate('_page_body');
	$date = Today();
	$myrow = get_recurrent_invoice($id);
	$from = calculate_from($myrow);
	$to = add_months($from, $myrow['monthly']);
	$to = add_days($to, $myrow['days']);

	if (!is_date_in_fiscalyear($date))
		display_error(_('The entered date is out of fiscal year or is closed for further data entry.'));
	elseif (!date1_greater_date2(add_days(Today(), 1), $to))
		display_error(_('Recurrent invoice cannot be generated before last day of covered period.'));
	elseif (check_recurrent_invoice_prices($id))
		display_error(_('Recurrent invoices cannot be generated because some items have no price defined in customer currency.'));
	elseif (!check_sales_order_type($myrow['order_no']))
		display_error(_('Recurrent invoices cannot be generated because selected sales order template uses prepayment sales terms. Change payment terms and try again.'));
	else {
		$count = recurrent_invoice_count($id);

		$_POST['trans_date'] = $to;
		start_form();
		start_table(TABLESTYLE, 'width=50%');
		label_row(_('Description:'), $myrow['description']);
		label_row(_('Template:'), get_customer_trans_view_str(ST_SALESORDER, $myrow['order_no']));
		label_row(_('Number of invoices:'), $count);
		date_row(_('Invoice date:'), 'trans_date');
		$newto = add_months($to, $myrow['monthly']);
		$newto = add_days($newto, $myrow['days']);
		text_row(_('Invoice notice:'), 'memo', sprintf(_('Recurrent Invoice covers period %s - %s.'), $to,	 add_days($newto, -1)), 100, 100);
		//text_row(_('Invoice notice:'), 'memo', sprintf(_('Recurrent Invoice covers period %s - %s.'), //$from, add_days($to, -1)), 100, 100);
		end_table();
		hidden('from', $from, true);
		hidden('to', $to, true);
		br();
		submit_center_first('confirmed'.$id, _('Create'), _('Create recurrent invoices'), false, ICON_OK);
		submit_center_last('cancel', _('Cancel'), _('Return to recurrent invoices'), false, ICON_ESCAPE);
		submit_js_confirm('do_create'.$id, sprintf(_("You are about to issue %s invoices.\n Do you want to continue?"), $count));
		end_form();

		display_footer_exit();
	}
}
else {
	$result = get_recurrent_invoices(Today());

	start_form();
	start_table(TABLESTYLE, 'width=70%');
	$th = array(_('Description'), _('Template No'),_('Customer'),_('Branch').'/'._('Group'),_('Days'),_('Monthly'),_('Begin'),_('End'),_('Next invoice'),'');
	table_header($th);
	$k = 0;
	$due = false;
	while ($myrow = db_fetch($result)) {
		if ($myrow['overdue']) {
			start_row("class='overduebg'");
			$due = true;
		}
		else
			alt_table_row_color($k);

		label_cell($myrow['description']);
		label_cell(get_customer_trans_view_str(ST_SALESORDER, $myrow['order_no']), "nowrap align='right'");
		if ($myrow['debtor_no'] == 0) {
			label_cell('');

			label_cell(get_sales_group_name($myrow['group_no']));
		}
		else {
			label_cell(get_customer_name($myrow['debtor_no']));
			label_cell(get_branch_name($myrow['group_no']));
		}
		label_cell($myrow['days']);
		label_cell($myrow['monthly']);
		label_cell(sql2date($myrow['begin']),  "align='center'");
		label_cell(sql2date($myrow['end']),	 "align='center'");
		label_cell(calculate_next($myrow),	"align='center'");
		if ($myrow['overdue']) {
			$count = recurrent_invoice_count($myrow['id']);
			if ($count)
				button_cell('create'.$myrow['id'], sprintf(_('Create %s Invoice(s)'), $count), '', ICON_DOC, 'process');
			else
				label_cell('');
		}
		else
			label_cell('');
		end_row();
	}
	end_table();
	end_form();
	if ($due)
		display_note(_('Marked items are due.'), 1, 0, "class='overduefg'");
	else
		display_note(_('No recurrent invoices are due.'), 1, 0);
	br();
}

end_page();