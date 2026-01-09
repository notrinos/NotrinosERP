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
include_once($path_to_root . '/admin/db/voiding_db.inc');
include_once($path_to_root . '/includes/types.inc');

//--------------------------------------------------------------------------------------

function get_supplier_trans_view_str($type, $trans_no, $label='', $icon=false, $class='', $id='') {
	$viewer = 'purchasing/view/';
	if ($type == ST_PURCHORDER)
		$viewer .= 'view_po.php';
	elseif ($type == ST_SUPPINVOICE)
		$viewer .= 'view_supp_invoice.php';
	elseif ($type == ST_SUPPCREDIT)
		$viewer .= 'view_supp_credit.php';
	elseif ($type == ST_SUPPAYMENT)
		$viewer .= 'view_supp_payment.php';
	elseif ($type == ST_SUPPRECEIVE)
		$viewer .= 'view_grn.php';
	else
		return null;
	$viewer .= '?trans_no='.$trans_no;

	if ($label == '')
		$label = $trans_no;

	return viewer_link($label, $viewer, $class, $id,  $icon);
}

//--------------------------------------------------------------------------------------

function get_gl_view_str($type, $trans_no, $label='', $force=false, $class='', $id='') {
	if (!$force && !user_show_gl_info())
		return '';

	$icon = false;
	if ($label == ''){
		$label = _('GL');
		$icon = ICON_GL;
	}
	$url = ($type == ST_WORKORDER) ? 'manufacturing/view/wo_costs_view.php?trans_no='.$trans_no : 'gl/view/gl_trans_view.php?type_id='.$type.'&trans_no='.$trans_no;

	return viewer_link($label, $url, $class, $id, $icon);
}

//--------------------------------------------------------------------------------------

function get_gl_view_str_cell($type, $trans_no, $label='') {
	$str = get_gl_view_str($type, $trans_no, $label);
	if ($str != '')
		return '<td>'.$str.'</td>';
}

//--------------------------------------------------------------------------------------

function get_customer_trans_view_str($type, $trans_no, $label='', $icon=false, $class='', $id='') {
	$viewer = 'sales/view/';

	if ($type == ST_SALESINVOICE)
		$viewer .= 'view_invoice.php';
	elseif ($type == ST_CUSTCREDIT)
		$viewer .= 'view_credit.php';
	elseif ($type == ST_CUSTPAYMENT)
		$viewer .= 'view_receipt.php';
	elseif ($type == ST_CUSTDELIVERY)
		$viewer .= 'view_dispatch.php';
	elseif ($type == ST_SALESORDER || $type == ST_SALESQUOTE)
		$viewer .= 'view_sales_order.php';
	else
		return null;

	if(!is_array($trans_no)) $trans_no = array($trans_no);

	$lbl = $label;
	$preview_str = '';

	foreach($trans_no as $trans) {
		if ($label == '')
			$lbl = $trans;
		if($preview_str!='') $preview_str .= ',';

		$preview_str .= viewer_link($lbl, $viewer.'?trans_no='.$trans.'&trans_type='.$type, $class, $id, $icon);
	}
	return $preview_str;
}

//--------------------------------------------------------------------------------------

function get_banking_trans_view_str($type, $trans_no, $label='', $icon=false, $class='', $id='') {
	if ($label == '')
		$label = $trans_no;

	if ($type == ST_BANKTRANSFER)
		$viewer = 'bank_transfer_view.php';
	elseif ($type == ST_BANKPAYMENT)
		$viewer = 'gl_payment_view.php';
	elseif ($type == ST_BANKDEPOSIT)
		$viewer = 'gl_deposit_view.php';
	else
		return null;

	return viewer_link($label, 'gl/view/'.$viewer.'?trans_no='.$trans_no, $class, $id,  $icon);
}

//--------------------------------------------------------------------------------------

function get_inventory_trans_view_str($type, $trans_no, $label='', $icon=false, $class='', $id='') {
	$viewer = 'inventory/view/';

	if ($type == ST_INVADJUST)
		$viewer .= 'view_adjustment.php';
	elseif ($type == ST_LOCTRANSFER)
		$viewer .= 'view_transfer.php';
	else
		return null;
	$viewer .= '?trans_no='.$trans_no;

	if ($label == '')
		$label = $trans_no;

	return viewer_link($label, $viewer, $class, $id,  $icon);
}

//--------------------------------------------------------------------------------------

function get_manufacturing_trans_view_str($type, $trans_no, $label='', $icon=false, $class='', $id='') {
	$viewer = 'manufacturing/view/';

	if ($type == ST_MANUISSUE)
		$viewer .= 'wo_issue_view.php';
	elseif ($type == ST_MANURECEIVE)
		$viewer .= 'wo_production_view.php';
	elseif ($type == ST_WORKORDER)
		$viewer .= 'work_order_view.php';
	else
		return null;

	$viewer .= '?trans_no='.$trans_no;

	if ($label == '')
		$label = $trans_no;

	return viewer_link($label, $viewer, $class, $id,  $icon);
}

//--------------------------------------------------------------------------------------

function get_dimensions_trans_view_str($type, $trans_no, $label='', $icon=false, $class='', $id='') {
	if ($type == ST_DIMENSION)
		$viewer = 'dimensions/view/view_dimension.php?trans_no='.$trans_no;
	else
		return null;

	if ($label == '')
		$label = $trans_no;

	return viewer_link($label, $viewer, $class, $id,  $icon);
}
/*
	Journal entry or cost update postings link
*/
function get_journal_trans_view_str($type, $trans_no, $label='', $icon=false, $class='', $id='') {
	if ($type == ST_JOURNAL || $type == ST_COSTUPDATE)
		$viewer = 'gl/view/gl_trans_view.php?type_id='.$type.'&trans_no='.$trans_no;
	else
		return null;

	if ($label == '')
		$label = $trans_no;

	return viewer_link($label, $viewer, $class, $id,  $icon);
}

//--------------------------------------------------------------------------------------

function get_package_view_str($pkg, $label='', $icon=false, $class='', $id='') {
	if ($label == '') {
		$label = _('Info');
		// $icon = ICON_GL;
	}
	return viewer_link($label, 'admin/view/view_package.php?id='.$pkg, $class, $id, $icon);
}

//--------------------------------------------------------------------------------------

function get_trans_view_str($type, $trans_no, $label='', $icon=false, $class='', $id='') {
	$view_str = get_customer_trans_view_str($type, $trans_no, $label, $icon, $class, $id);
	if ($view_str != null)
		return $view_str;

	$view_str = get_supplier_trans_view_str($type, $trans_no, $label, $icon, $class, $id);
	if ($view_str != null)
		return $view_str;

	$view_str = get_banking_trans_view_str($type, $trans_no, $label, $icon, $class, $id);
	if ($view_str != null)
		return $view_str;

	$view_str = get_inventory_trans_view_str($type, $trans_no, $label, $icon, $class, $id);
	if ($view_str != null)
		return $view_str;

	$view_str = get_manufacturing_trans_view_str($type, $trans_no, $label, $icon, $class, $id);
	if ($view_str != null)
		return $view_str;

	$view_str = get_dimensions_trans_view_str($type, $trans_no, $label, $icon, $class, $id);
	if ($view_str != null)
		return $view_str;

	$view_str = get_journal_trans_view_str($type, $trans_no, $label, $icon, $class, $id);
	if ($view_str != null)
		return $view_str;

	return null;
}

/*
	Helper for ui drawing functions.
	Checks whether any of input function parameters has changed or page is called with GET method (first page display).
	$name - context
*/
function check_ui_refresh($name=null) {
	$bt = debug_backtrace();
	if (!$name)
		$name = $bt[1]['function'];
	$old = @$_SESSION['ui_context'][$name];
	$new = $_SESSION['ui_context'][$name] = $bt[1]['args'];
	return ($new != $old);
}

//--------------------------------------------------------------------------------------
// Displays currency exchange rate for given date.
// When there is no exrate for today, 
// gets it form ECB and stores in local database.
//
function exchange_rate_display($from_currency, $to_currency, $date_, $force_edit=false) {
	global $Ajax, $SysPrefs;

	$readonly = false;

	if ($from_currency != $to_currency) {
		$rate = input_num('_ex_rate');
		if (check_ui_refresh() || !$rate) { // readonly or ui context changed
			$comp_currency = get_company_currency();

			if (!isset($from_currency))
				$from_currency = $comp_currency;
			if (!isset($to_currency))
				$to_currency = $comp_currency;
			if ($from_currency == $comp_currency)
				$currency = $to_currency;
			else
				$currency = $from_currency;

			$rate = get_date_exchange_rate($currency, $date_); // try local

			if ($rate)
				$readonly = true; // if we have already local exrate keep it unchanged 

			if (!$rate) {	// retry from remote service
				$row = get_currency($currency);

				if ($row['auto_update']) {// autoupdate means use remote service & store exrate on first transaction.
				
					$rate = retrieve_exrate($currency, $date_);
					if (!$rate)
						display_warning(sprintf(_('Cannot retrieve exchange rate for currency %s. Please adjust approximate rate if needed.'), $currency));
					elseif ($SysPrefs->xr_provider_authoritative) {
						// if the remote exrate is considered authoritative we can store the rate here,
						// otherwise exrate will be stored during transaction write
						$readonly = true;
						add_new_exchange_rate($currency, $date_, $rate);
					}
				}
			}
			if (!$rate)	// get and edit latest available
				$rate = get_exchange_rate_from_home_currency($currency, $date_);
			
			if ($from_currency != $comp_currency)
				$rate = 1 / ($rate / get_exchange_rate_from_home_currency($to_currency, $date_));
			$Ajax->activate('_ex_rate_span');
		}

		$rate = number_format2($rate, user_exrate_dec());

		if ($force_edit || !$readonly)
			$ctrl = "<input type=\"text\" name=\"_ex_rate\" size=\"8\" maxlength=\"8\" value=\"$rate\">";
		else
			$ctrl = "<span id=\"_ex_rate\">$rate</span>";

		label_row(_("Exchange Rate:"), $span = "<span style='vertical-align:top;' id='_ex_rate_span'>$ctrl $from_currency = 1 $to_currency</span>" );

		$Ajax->addUpdate('_ex_rate_span', '_ex_rate_span', $span);
	}
}

//--------------------------------------------------------------------------------------

function is_voided_display($type, $id, $label) {
	$void_entry = get_voided_entry($type, $id);

	if ($void_entry == null)
		return false;

	start_table(TABLESTYLE, "width='50%'");
	echo '<tr><td align=center><font color=red>'.$label.'</font><br>';
	echo "<font color=red>"._("Date Voided:")." ".sql2date($void_entry["date_"]).'</font><br>';
	if (strlen($void_entry['memo_']) > 0)
		echo '<center><font color=red>'._('Memo:').' '.$void_entry['memo_'].'</font></center><br>';
	echo '</td></tr>';
	end_table(1);

	return true;
}

//--------------------------------------------------------------------------------------

function comments_display_row($type, $id) {
	$comments = get_comments($type, $id);
	if ($comments and db_num_rows($comments)) {
		echo '<tr><td colspan=15>';
		while ($comment = db_fetch($comments)) {
			echo nl2br($comment['memo_']) . '<br>';
		}
		echo '</td></tr>';
	}
}

//--------------------------------------------------------------------------------------

function get_comments_string($type, $type_no) {
	$str_return = '';
	$result = get_comments($type, $type_no);
	while ($comment = db_fetch($result)) {
		if (strlen($str_return))
			$str_return = $str_return." \n";
		$str_return = $str_return.$comment['memo_'];
	}
	return $str_return;
}

//--------------------------------------------------------------------------------------

function view_stock_status($stock_id, $description=null, $echo=true) {
	global $path_to_root;
	if ($description)
		$preview_str = "<a target='_blank' href='$path_to_root/inventory/inquiry/stock_status.php?stock_id=$stock_id&popup=1' onclick=\"javascript:openWindow(this.href,this.target); return false;\" >".(user_show_codes() ? $stock_id.' - ' : '').$description."</a>";
	else
		$preview_str = "<a target='_blank' href='$path_to_root/inventory/inquiry/stock_status.php?stock_id=$stock_id&popup=1' onclick=\"javascript:openWindow(this.href,this.target); return false;\" >$stock_id</a>";
	if($echo)
		echo $preview_str;
	return $preview_str;
}

function view_stock_status_cell($stock_id, $description=null) {
	echo '<td>';
	view_stock_status($stock_id, $description);
	echo '</td>';
}

//--------------------------------------------------------------------------------------

function display_debit_or_credit_cells($value, $bold=false) {
	$value = round2($value, user_price_dec());
	if ($value >= 0) {
		amount_cell($value, $bold);
		label_cell('');
	}
	elseif ($value < 0) {
		label_cell('');
		amount_cell(abs($value), $bold);
	}
}

//--------------------------------------------------------------------------------------

function display_customer_trans_tax_details($tax_items, $columns) {
	global $SysPrefs;

	$first = true;
	while ($tax_item = db_fetch($tax_items)) {
		if (!$tax_item['amount'])
			continue;

		$tax = number_format2($tax_item['amount'],user_price_dec());
		if ($SysPrefs->suppress_tax_rates() == 1)
			$tax_type_name = $tax_item['tax_type_name'];
		else
			$tax_type_name = $tax_item['tax_type_name'].' ('.$tax_item['rate'].'%) ';
		if ($tax_item['included_in_price']) {
			if ($SysPrefs->alternative_tax_include_on_docs() == 1) {
				if ($first)
					label_row(_('Total Tax Excluded'), number_format2($tax_item['net_amount'], user_price_dec()), 'colspan='.$columns.' align=right', 'align=right');
				label_row($tax_type_name, $tax, 'colspan='.$columns.' align=right', 'align=right');
				$first = false;
			}
			else
				label_row(_('Included').' '.$tax_type_name.': '.$tax, '', 'colspan='.$columns.' align=right', 'align=right');
		}			
		else
			label_row($tax_type_name, $tax, 'colspan='.$columns.' align=right', 'align=right');
	}
}

//--------------------------------------------------------------------------------------

function display_supp_trans_tax_details($tax_items, $columns) {
	while ($tax_item = db_fetch($tax_items)) {
		if (!$tax_item['amount'])
			continue;

		$tax = number_format2(abs($tax_item['amount']),user_price_dec());
		if ($tax_item['included_in_price'])
			label_row(_('Included').' '.$tax_item['tax_type_name'].' ('.$tax_item['rate'].'%) '. ': '.$tax, '', 'colspan='.$columns.' align=right', 'align=right');
		else
			label_row($tax_item['tax_type_name'].' ('.$tax_item['rate'] . '%)', $tax, 'colspan='.$columns.' align=right', 'align=right');
	}
}

//--------------------------------------------------------------------------------------

function display_edit_tax_items($taxes, $columns, $tax_included, $rightspan=0, $editable=false) {
	$total = 0;

	foreach ($taxes as $taxitem) {
		$amount = isset($taxitem['Override']) ? $taxitem['Override'] : $taxitem['Value'];
		if ($taxitem['Value'] != 0) {
			if ($editable) {
				if (!isset($_POST['mantax['.$taxitem['tax_type_id'].']']))
					$_POST['mantax['.$taxitem['tax_type_id'].']'] = price_format($amount);
				start_row();
				if ($tax_included) {
					$colspan = $columns-1;
					label_cell(_('Included') . ' ' . $taxitem['tax_type_name'].':',
						"colspan={$colspan} align='right'");
					amount_cells(null, 'mantax['.$taxitem['tax_type_id'].']',
						null, 'colspan='.$columns.' align=right', '<td></td>', user_price_dec());
				}
				else {
					label_cell($taxitem['tax_type_name'], 'colspan='.$columns." align='right'");
					amount_cells(null, 'mantax['.$taxitem['tax_type_id'].']', null, 'colspan='.$columns.' align=right', null, user_price_dec());
					$total +=  round2($amount, user_price_dec());
				}
				if ($rightspan)
					label_cell('', "colspan ='".$rightspan."'");
				end_row();
			}
			else {
				$value = number_format2($taxitem['Value'],user_price_dec());
				if ($tax_included)
					label_row(_('Included').' '.$taxitem['tax_type_name'].' '.$value , '', 'colspan='.$columns.' align=right', 'align=right', $rightspan);
				else {
					label_row($taxitem['tax_type_name'], $value, 'colspan='.$columns.' align=right', 'align=right', $rightspan);
					$total +=  round2($taxitem['Value'], user_price_dec());
				}
			}
		}
	}
	return $total;
}

//--------------------------------------------------------------------------------------

function display_footer_exit($space=2) {
	if ($space)
		br($space);
	end_page(false, false, true);
	exit;
}

//--------------------------------------------------------------------------------------

function display_allocations($alloc_result, $total, $title, $show_summary=true) {
	global $systypes_array;

	if (!$alloc_result || db_num_rows($alloc_result) == 0)
		return;

	display_heading2($title);

	start_table(TABLESTYLE, "width='80%'");

	$th = array( _('Type'), _('Number'), _('Date'), _('Total Amount'), _('Left to Allocate'), _('This Allocation'));
	table_header($th);
	$k = $total_allocated = 0;

	while ($alloc_row = db_fetch($alloc_result)) {
		alt_table_row_color($k);

		label_cell($systypes_array[$alloc_row['type']]);
		label_cell(get_trans_view_str($alloc_row['type'],$alloc_row['trans_no']));
		label_cell(sql2date($alloc_row['tran_date']));
		$alloc_row['Total'] = round2($alloc_row['Total'], user_price_dec());
		$alloc_row['amt'] = round2($alloc_row['amt'], user_price_dec());
		if ($alloc_row['type'] == ST_SUPPAYMENT || $alloc_row['type'] == ST_BANKPAYMENT || $alloc_row['type'] == ST_SUPPCREDIT)
			$alloc_row['Total'] = -$alloc_row['Total'];
		amount_cell($alloc_row['Total']);
		amount_cell($alloc_row['Total'] - $alloc_row['alloc']);
		amount_cell($alloc_row['amt']);
		end_row();

		$total_allocated += $alloc_row['amt'];
	}
	start_row();
	label_cell(_('Total Allocated:'), 'align=right colspan=5');
	amount_cell($total_allocated);
	end_row();
	if ($show_summary) {
		start_row();
		label_cell(_('Left to Allocate:'), 'align=right colspan=5');
		$total = round2($total, user_price_dec());
		amount_cell($total - $total_allocated);
		end_row();
	}
	end_table(1);
}

//--------------------------------------------------------------------------------------

function display_allocations_from($person_type, $person_id, $type, $type_no, $total) {
	switch ($person_type) {
		case PT_CUSTOMER :
			$alloc_result = get_allocatable_to_cust_transactions($person_id, $type_no, $type);
			display_allocations($alloc_result, $total, _('Allocations'));
			return;
		case PT_SUPPLIER :
			$alloc_result = get_allocatable_to_supp_transactions($person_id, $type_no, $type);
			display_allocations($alloc_result, $total, _('Allocations'));
			return;
	}
}

//--------------------------------------------------------------------------------------

function display_allocations_to($person_type, $person_id, $type, $type_no, $total) {
	switch ($person_type) {
		case PT_CUSTOMER :
			$alloc_result = get_allocatable_from_cust_transactions($person_id, $type_no, $type);
			display_allocations($alloc_result, $total, $type == ST_SALESORDER ? _('Pre-Payments') : _('Payments'), false);
			return;
		case PT_SUPPLIER :
			$alloc_result = get_allocatable_from_supp_transactions($person_id, $type_no, $type);
			display_allocations($alloc_result, $total, $type == ST_PURCHORDER ? _('Pre-Payments') : _('Payments'), false);
			return;
	}
}

//--------------------------------------------------------------------------------------
//
//	Expands selected quick entry $id into GL posings and adds to cart.
//		returns calculated amount posted to bank GL account.
//
function display_quick_entries(&$cart, $id, $base, $type, $descr='') {
	$bank_amount = 0;

	if (!isset($id) || $id == null || $id == '') {
		display_error( _('No Quick Entries are defined.'));
		set_focus('totamount');
	}
	else {
		if ($type == QE_DEPOSIT)
			$base = -$base;
		if ($type != QE_SUPPINV && $type != QE_JOURNAL)	// only one quick entry on journal/bank transaction
			$cart->clear_items();

		// as for now tax_group is passed only in QE_SUPPINV
		$qe_lines = quickentry_calculate($base, $id, @$cart->tax_group_id, $cart->tran_date);

		if ($qe_lines === 0) {
			display_error( _('No Quick Entry lines are defined.'));
			set_focus('totamount');
			return 0;
		}
		elseif ($qe_lines === -1) {
			display_error(_('Cannot post to GL account used by more than one tax type.'));
			return 0;
		}
		foreach($qe_lines as $qe_line) {
			if ($descr != '')
				$qe_line['descr'] .= ': '.$descr;
			if ($type != QE_SUPPINV)
				$cart->add_gl_item($qe_line['code'], $qe_line['dim1'], $qe_line['dim2'], $qe_line['amount'], $qe_line['descr']);
			else {
				$acc_name = get_gl_account_name($qe_line['code']);
				$cart->add_gl_codes_to_trans($qe_line['code'], $acc_name, $qe_line['dim1'], $qe_line['dim2'], $qe_line['amount'], $qe_line['descr']);
			}
		}
	}
	return $bank_amount;
}

//--------------------------------------------------------------------------------------

function quickentry_calculate($base, $id, $taxgroup=null, $date=null) {
	$gl_entries = array();
	$qe = get_quick_entry($id);
	$type = $qe['type'];
	$total = $base;
	// quick entry made on account balance is special case.
	if ($qe['bal_type'] == 1) {// if this is quick entry based on balance - calculate it
		// Note, that this is ugly hack overriding standard field usage 
		// just to make the feature available between major FA releases!
		$gl_code = $qe['base_desc'];
		$monthly = $qe['base_amount'] == 1.0;

		if (!isset($date))
			$date = Today();

		if ($monthly) // marked as monthly
			$begin = begin_month($date);
		else {
			if (is_account_balancesheet($gl_code)) // total
				$begin = '';
			else
				$begin = begin_fiscalyear(); // from fiscalyear begin
		}
		$base = get_gl_trans_from_to($begin, $date, $gl_code);

	}

	$result = get_quick_entry_lines($id);
	if (db_num_rows($result) == 0)
		return 0;

	// first calculate total rate in case more than one tax is used
	$totrate = 0;
	while ($row = db_fetch($result)) {
		$qe_lines[] = $row;

		switch (strtolower($row['action'])) {
			case 't': // post taxes calculated on base amount
			case 't+': // ditto & increase base amount
			case 't-': // ditto & reduce base amount
				if (substr($row['action'], 0, 1) != 'T') 
					$totrate += get_tax_type_rate($row['dest_id']);
		}
	}
	$first = true;
	$taxbase = 0;
	foreach($qe_lines as $qe_line) {
		$gl_code = $qe_line['dest_id'];
		switch (strtolower($qe_line['action'])) {
			case '=': // post current base amount to GL account
				$part = $base;
				break;
			case 'a': // post amount to GL account and reduce base
				$part = $qe_line['amount'];
				break;
			case 'a+': // post amount to GL account and increase base
				$part = $qe_line['amount']; $base += $part;
				break;
			case 'a-': // post amount to GL account and reduce base
				$part = $qe_line['amount']; $base -= $part;
				break;
			case '%':	// store acc*amount% to GL account
				$part = round2($base * $qe_line['amount'] / 100, user_price_dec());
				break;
			case '%+':	// ditto & increase base amount
				$part = round2($base * $qe_line['amount'] / 100, user_price_dec());
				$base += $part;
				break;
			case '%-':	// ditto & reduce base amount
				$part = round2($base * $qe_line['amount'] / 100, user_price_dec());
				$base -= $part;
				break;
			case 't': // post taxes calculated on base amount
			case 't+': // ditto & increase base amount
			case 't-': // ditto & reduce base amount
				if ($first) {
					$taxbase = $base/($totrate+100);
					$first = false;
				}

				if (substr($qe_line['action'],0,1) != 'T') 
					$part = $taxbase;
				else
					$part = $base/100;

				$item_tax = get_tax_type($qe_line['dest_id']);
				//if ($type == QE_SUPPINV && substr($qe_line['action'],0,1) != 'T')
				if ($type == QE_SUPPINV) {
					$rates = 0;
					$res = get_tax_group_rates($taxgroup);
					while ($row = db_fetch($res))
						$rates += $row['rate'];
					if ($rates == 0)
						continue 2;
				}
				$tax = round2($part * $item_tax['rate'],  user_price_dec());
				if ($tax == 0) continue 2;
				$gl_code = ($type == QE_DEPOSIT || ($type == QE_JOURNAL && $base < 0)) ? $item_tax['sales_gl_code'] : $item_tax['purchasing_gl_code'];
				if (!is_tax_gl_unique($gl_code))
					return -1; // more than one taxtype used with this GL account

				$part = $tax;
				if (strpos($qe_line['action'], '+'))
					$base += $tax;
				elseif (strpos($qe_line['action'], '-'))
					$base -= $tax;
		}
		$gl_entries[] = array(
			'code' => $gl_code, 
			'name' => get_gl_account_name($gl_code),
			'dim1' => $qe_line['dimension_id'], 
			'dim2' => $qe_line['dimension2_id'], 
			'amount' => $part, 
			'descr' => $qe_line['memo']
		);
	}
	return $gl_entries;
}

//--------------------------------------------------------------------------------------
//
//	Simple English version of number to words conversion.
//
function _number_to_words($number) {
	$Bn = floor($number / 1000000000); /* Billions (giga) */ 
	$number -= $Bn * 1000000000; 
	$Gn = floor($number / 1000000);  /* Millions (mega) */ 
	$number -= $Gn * 1000000; 
	$kn = floor($number / 1000);     /* Thousands (kilo) */ 
	$number -= $kn * 1000; 
	$Hn = floor($number / 100);      /* Hundreds (hecto) */ 
	$number -= $Hn * 100; 
	$Dn = floor($number / 10);       /* Tens (deca) */ 
	$n = $number % 10;               /* Ones */

	$res = ''; 

	if ($Bn) 
		$res .= _number_to_words($Bn) . ' Billion'; 
	if ($Gn) 
		$res .= (empty($res) ? '' : ' ') . _number_to_words($Gn) . ' Million'; 
	if ($kn) 
		$res .= (empty($res) ? '' : ' ') . _number_to_words($kn) . ' Thousand'; 
	if ($Hn) 
		$res .= (empty($res) ? '' : ' ') . _number_to_words($Hn) . ' Hundred'; 

	$ones = array('', 'One', 'Two', 'Three', 'Four', 'Five', 'Six', 
		'Seven', 'Eight', 'Nine', 'Ten', 'Eleven', 'Twelve', 'Thirteen', 
		'Fourteen', 'Fifteen', 'Sixteen', 'Seventeen', 'Eighteen', 
		'Nineteen'); 
	$tens = array('', '', 'Twenty', 'Thirty', 'Forty', 'Fifty', 'Sixty', 
		'Seventy', 'Eighty', 'Ninety'); 

	if ($Dn || $n) {
		if (!empty($res)) 
			$res .= ' and '; 
		if ($Dn < 2) 
			$res .= $ones[$Dn * 10 + $n]; 
		else {
			$res .= $tens[$Dn]; 
			if ($n) 
				$res .= '-' . $ones[$n]; 
		} 
	} 

	if (empty($res)) 
		$res = 'zero'; 
	return $res; 
} 

function price_in_words($amount, $document=0) {
	// use local price_in_words() if the hook is defined
	$price = hook_price_in_words($amount, $document);
	if ($price) 
		return $price;

	// Only usefor Remittance and Receipts as default
	if (!($document == ST_SUPPAYMENT || $document == ST_CUSTPAYMENT || $document == ST_CHEQUE))
		return '';
	if ($amount < 0 || $amount > 999999999999)
		return '';
	$dec = user_price_dec();
	if ($dec > 0) {
		$divisor = pow(10, $dec);
		$frac = round2($amount - floor($amount), $dec) * $divisor;
		$frac = sprintf("%0{$dec}d", round2($frac, 0));
		$and = _('and');
		$frac = " $and $frac/$divisor";
	}
	else
		$frac = '';
	return _number_to_words(intval($amount)) . $frac;
}

function get_js_open_window($width, $height) {
	$js ="function openWindow(url, title)\n"
		. "{\n"
		. " var left = (screen.width - $width) / 2;\n"
		. " var top = (screen.height - $height) / 2;\n"
		. " return window.open(url, title, 'width=$width,height=$height,left='+left+',top='+top+',screenX='+left+',screenY='+top+',status=no,scrollbars=yes');\n"
		. "}\n";
	$js .= get_js_lookup_window();
	return $js;
}

function get_js_lookup_window() {
	$js = "function lookupWindow(url, title) {
		var u = url.split('#');
		if (u.length == 2) {
			var element = document.getElementById(u[1]);
			var options = element.options;
			url = u[0] + '&' + u[1] + '=' + options[element.selectedIndex].value;
		}
		openWindow(url, title);
	}";
	return $js;
}

function get_js_select_combo_item() {
	$js = "function selectComboItem(doc, client_id, value){
		var element = doc.getElementById(client_id);
		if (typeof(element) != 'undefined' && element != null && element.tagName === 'SELECT' ){
			var options = element.options;
			for (var i = 0, optionsLength = options.length; i < optionsLength; i++) {
				if (options[i].value == value) {
					element.selectedIndex = i;
					element.onchange();
				}
			}
		} else {			
			var stock_element = doc.getElementsByName('stock_id');
			if( stock_element.length > 0) {
				stock_element[0].value = value;	
				var stock_id = doc.getElementById('_stock_id_edit'); 
				stock_id.value=value;
				stock_id.onblur();		
			}
		}			
		window.close();
	}";
	return $js;
}
/* for space search option */
function get_js_set_combo_item() {
	$js = "function setComboItem(doc, client_id, value, text){
		var element = doc.getElementById(client_id);
		var search = doc.getElementById('_'+client_id+'_edit');
		if(typeof(element) != 'undefined' && element != null && element.tagName === 'SELECT') {
			var options = element.options;
			options.length = 0;
			var option = doc.createElement('option');
			option.value = value;
			option.text = text;
			element.add(option, 0);
			element.selectedIndex = 0;
			search.value = '';
			element.onchange();
		} else {
			var stock_element = doc.getElementsByName('stock_id');
			if( stock_element.length > 0) {
				stock_element[0].value = value;	
				var stock_id = doc.getElementById('_stock_id_edit'); 
				stock_id.value=value;
				stock_id.onblur();					
			}
		}
		window.close();
	}";
	return $js;
}

/*
  Setting focus on element $name in $form.
  If $form<0 $name is element id.
*/
function set_focus($name, $form_no=0) {
	global $Ajax;
	$Ajax->addFocus(true, $name);
	$_POST['_focus'] = $name;
}
//
//	Set default focus on first field $name if not set yet
//	Returns unique name if $name=null
//	
function default_focus($name=null, $form_no=0) {
	static $next; 
	if ($name==null) 
		$name = uniqid('_el',true);
	if (!isset($_POST['_focus']))
		set_focus($name);

	return $name;
}
/*
	Reset focus to next control element (e.g. link).
*/
function reset_focus() {
	unset($_POST['_focus']);
}

function get_js_date_picker() {
	global $SysPrefs, $tmonths;

	$fpath = user_js_cache().'/'.'date_picker.js';

	if (!file_exists($fpath) || $SysPrefs->go_debug) {

		$date_system = $SysPrefs->date_system;
		$dateseps = $SysPrefs->dateseps;
		$how = user_date_format();				// 0 = us/ca, 1 = eu, au, nz, 2 = jp, sw
		$sep = $dateseps[user_date_sep()];		// date separator
		$wstart = (($date_system == 1 || $date_system == 2 || $date_system == 3) ? 6 : ($how == 0 || $how == 3 ? 0 : 1));	// weekstart (sun = 0, mon = 1)
		$months = array(_('January'),_('February'),_('March'),_('April'),_('May'),_('June'),_('July'),_('August'),_('September'),_('October'),_('November'),_('December'));
		$wdays = array(_('Su'),_('Mo'),_('Tu'),_('We'),_('Th'),_('Fr'),_('Sa'));
		$wno = _('W'); // week no

		if ($date_system == 1)
			list($cyear, $cmon, $cday) = gregorian_to_jalali(date('Y'), date('n'), date('j'));
		elseif ($date_system == 2)
			list($cyear, $cmon, $cday) = gregorian_to_islamic(date('Y'), date('n'), date('j'));

		$js = "
			function positionInfo(object) {
				var p_elm = object;
				this.getElementLeft = getElementLeft;
				function getElementLeft() {
					var x = 0;
					var elm;
					if(typeof(p_elm) == 'object'){
						elm = p_elm;
					}
					else {
						elm = document.getElementById(p_elm);
					}
					while (elm != null) {
						x+= elm.offsetLeft;
						elm = elm.offsetParent;
					}
					return parseInt(x);
				}
				this.getElementWidth = getElementWidth;
				function getElementWidth(){
					var elm;
					if(typeof(p_elm) == 'object'){
						elm = p_elm;
					}
					else {
						elm = document.getElementById(p_elm);
					}
					return parseInt(elm.offsetWidth);
				}
				this.getElementRight = getElementRight;
				function getElementRight(){
					return getElementLeft(p_elm) + getElementWidth(p_elm);
				}
				this.getElementTop = getElementTop;
				function getElementTop() {
					var y = 0;
					var elm;
					if(typeof(p_elm) == 'object'){
						elm = p_elm;
					}
					else {
						elm = document.getElementById(p_elm);
					}
					while (elm != null) {
						y+= elm.offsetTop;
						elm = elm.offsetParent;
					}
					return parseInt(y);
				}
				this.getElementHeight = getElementHeight;
				function getElementHeight(){
					var elm;
					if(typeof(p_elm) == 'object'){
						elm = p_elm;
					}
					else {
						elm = document.getElementById(p_elm);
					}
					return parseInt(elm.offsetHeight);
				}
				this.getElementBottom = getElementBottom;
				function getElementBottom(){
					return getElementTop(p_elm) + getElementHeight(p_elm);
				}
			}
			function CC() {
				var calendarId = 'CC';
				var currentYear = 0;
				var currentMonth = 0;
				var currentDay = 0;
				var selectedYear = 0;
				var selectedMonth = 0;
				var selectedDay = 0;
				var months = ['$months[0]','$months[1]','$months[2]','$months[3]','$months[4]','$months[5]','$months[6]','$months[7]','$months[8]','$months[9]','$months[10]','$months[11]'];
				var wdays = ['$wdays[0]', '$wdays[1]', '$wdays[2]', '$wdays[3]', '$wdays[4]', '$wdays[5]', '$wdays[6]'];
				var tmonths = ['$tmonths[0]','$tmonths[1]','$tmonths[2]','$tmonths[3]','$tmonths[4]','$tmonths[5]','$tmonths[6]','$tmonths[7]','$tmonths[8]','$tmonths[9]','$tmonths[10]','$tmonths[11]','$tmonths[12]'];
				var dateField = null;
				function getProperty(p_property){
					var p_elm = calendarId;
					var elm = null;
					if(typeof(p_elm) == 'object'){
						elm = p_elm;
					}
					else {
						elm = document.getElementById(p_elm);
					}
					if (elm != null){
						if(elm.style){
							elm = elm.style;
							if(elm[p_property]){
								return elm[p_property];
							}
							else {
								return null;
							}
						}
						else {
							return null;
						}
					}
				}
				function setElementProperty(p_property, p_value, p_elmId){
					var p_elm = p_elmId;
					var elm = null;
					if(typeof(p_elm) == 'object'){
						elm = p_elm;
					}
					else {
						elm = document.getElementById(p_elm);
					}
					if((elm != null) && (elm.style != null)){
					elm = elm.style;
					elm[ p_property ] = p_value;
				}
			}
			function setProperty(p_property, p_value) {
				setElementProperty(p_property, p_value, calendarId);
			}
			function getDaysInMonth(year, month) {
";
		if ($date_system == 1)
			$js .= "
				return [31,31,31,31,31,31,30,30,30,30,30,(((((((year - ((year > 0) ? 474 : 473)) % 2820) + 474) + 38) * 682) % 2816) < 682 ? 30 : 29)][month-1];
";
		elseif ($date_system == 2)
			$js .= "
				return [30,29,30,29,30,29,30,29,30,29,30,(((((11 * year) + 14) % 30) < 11) ? 30 : 29)][month-1];
";
		else
			$js .= "
				return [31,((!(year % 4 ) && ( (year % 100 ) || !( year % 400 ) ))?29:28),31,30,31,30,31,31,30,31,30,31][month-1];
";
		$js .= "
			}
			function getDayOfWeek(year, month, day) {
";
		if ($date_system == 1 || $date_system == 2) {
			$js .= "
				function mod(a, b) {
					return a - (b * Math.floor(a / b));
				}
				function jwday(j) {
					return mod(Math.floor((j + 1.5)), 7);
				}
";
			if ($date_system == 1)
				$js .= "
				var epbase, epyear, t;
				epbase = year - ((year >= 0) ? 474 : 473);
				epyear = 474 + mod(epbase, 2820);
				t = day + ((month <= 7) ? ((month - 1) * 31) : (((month - 1) * 30) + 6)) +
				Math.floor(((epyear * 682) - 110) / 2816) + (epyear - 1) * 365 +
				Math.floor(epbase / 2820) * 1029983 + (1948320.5 - 1);
";
			elseif ($date_system == 2)
				$js .= "
				var t;
				t = Math.floor((11 * year + 3) / 30) + 354 * year + 30 * month -
				Math.floor((month - 1) / 2) + day + 1948440 - 385;
";
			$js .= "
				return jwday(t);
";
		}
		else
			$js .= "
				var date = new Date(year,month-1,day)
				return date.getDay();
";
		$js .= "
			}
			this.clearDate = clearDate;
			function clearDate() {
				dateField.value = '';
				hide();
			}
			this.getWeek = getWeek;
			function getWeek(year, month, day) {
";
		if ($how == 0 || $how == 3)
			$js .= "
				day++;";
		$js .= "
				var date = new Date(year,month-1,day);
				var D = date.getDay();
				if(D == 0) D = 7;
				date.setDate(date.getDate() + (4 - D));
				var YN = date.getFullYear();
				var ZBDoCY = Math.floor((date.getTime() - new Date(YN, 0, 1, -6)) / 86400000);
				var WN = 1 + Math.floor(ZBDoCY / 7);
				return WN;
			}
			this.setDate = setDate;
			function setDate(year, month, day) {
				if (dateField) {
";
		if ($how < 3)
			$js .= "
					if (month < 10) {month = '0' + month;}
					if (day < 10) {day = '0' + day;}
";
		else
			$js .= "
					month = tmonths[month];
";
		if ($how == 0 || $how == 3)
			$js .= "
					var dateString = month+'$sep'+day+'$sep'+year;
";
		elseif ($how == 1 || $how == 4)
			$js .= "
					var dateString = day+'$sep'+month+'$sep'+year;
";
		elseif ($how == 2 || $how == 5)
			$js .= "
					var dateString = year+'$sep'+month+'$sep'+day;
";
		$js .= "
					dateField.value = dateString;
					setFocus(dateField.name);
					if(dateField.getAttribute('aspect')=='cdate')
						setElementProperty('color', (dateField.value==user.date ? 'black':'red'), dateField);
					if (dateField.className=='searchbox')
						dateField.onblur();
					hide();
				}
				return;
			}
			this.changeMonth = changeMonth;
			function changeMonth(change) {
				currentMonth += change;
				currentDay = 0;
				if(currentMonth > 12) {
					currentMonth = 1;
					currentYear++;
				}
				else if(currentMonth < 1) {
					currentMonth = 12;
					currentYear--;
				}
				calendar = document.getElementById(calendarId);
				calendar.innerHTML = calendarDrawTable();
			}
			this.changeYear = changeYear;
			function changeYear(change) {
				currentYear += change;
				currentDay = 0;
				calendar = document.getElementById(calendarId);
				calendar.innerHTML = calendarDrawTable();
			}
			function getCurrentYear() {
				var year = new Date().getYear();
				if(year < 1900) year += 1900;
				return year;
			}
			function getCurrentMonth() {
				return new Date().getMonth() + 1;
			}
			function getCurrentDay() {
				return new Date().getDate();
			}
			function calendarDrawTable() {
				var dayOfMonth = 1;
				var wstart = $wstart;
				var wno = '&nbsp;$wno&nbsp;';
				var validDay = 0;
				var startDayOfWeek = getDayOfWeek(currentYear, currentMonth, dayOfMonth);
				var daysInMonth = getDaysInMonth(currentYear, currentMonth);
				var css_class = null; //CSS class for each day
				var months_list = \"<select class = 'cc_months_list' onchange='javascript:changeCCMonth(parseInt(this.value));'>\";
					months.forEach(function(item, index){
						if(index == currentMonth-1)
							months_list += \"<option selected value=\" + (index - currentMonth + 1) + \">\" + item + \"</option>\";
						else
							months_list += \"<option value=\" + (index - currentMonth + 1) + \">\" + item + \"</option>\";
					});
					months_list += \"</select>\";
				var table = \"<table cellspacing='0' cellpadding='0' border='0'>\";
				table += \"<tr class='header'>\";
				table += \"  <td colspan='2' class='previous'><a href='javascript:changeCCMonth(-1);'>&#9664;</a><br><a href='javascript:changeCCYear(-1);'>&#9664;</a></td>\";
				table += \"  <td colspan='4' class='title'>\" + months_list + \"<br>\" + currentYear + \"</td>\";
				table += \"  <td colspan='2' class='next'><a href='javascript:changeCCMonth(1);'>&#9654;</a><br><a href='javascript:changeCCYear(1);'>&#9654;</a></td>\";
				table += \"</tr>\";
				table += \"<tr>\";
				table += \"<th class='weekno' title='"._('Week')."'>\"+wno+\"</th>\";
				for (var n=0; n<7; n++)
					table += \"<th>\" + wdays[(wstart+n)%7]+\"</th>\";
				table += \"</tr>\";
				for(var week=0; week < 6; week++) {
					table += \"<tr>\";
					for(var n=0; n < 7; n++) {
						dayOfWeek = (wstart+n)%7;
						if(week == 0 && startDayOfWeek == dayOfWeek) {
							validDay = 1;
						}
						else if (validDay == 1 && dayOfMonth > daysInMonth) {
							validDay = 0;
						}
						if (n==0) {
							if (dayOfMonth > daysInMonth)
								table += \"<td class='empty'>&nbsp;</td>\";
							else
								table += \"<td class='weekno'>\"+getWeek(currentYear, currentMonth, dayOfMonth)+\"</td>\";
						}
						if(validDay) {
							if (dayOfMonth == selectedDay && currentYear == selectedYear && currentMonth == selectedMonth) {
								css_class = 'current';
";
		if ($date_system == 1 || $date_system == 2 || $date_system == 3)
			$js .= "
							}
							else if (dayOfWeek == 5) {
";
		else
			$js .= "
							} else if (dayOfWeek == 0 || dayOfWeek == 6) {
";
		$js .= "
								css_class = 'weekend';
							}
							else {
								css_class = 'weekday';
							}
							table += \"<td><a class='\"+css_class+\"' href=\\\"javascript:setCCDate(\"+currentYear+\",\"+currentMonth+\",\"+dayOfMonth+\")\\\">\"+dayOfMonth+\"</a></td>\";
							dayOfMonth++;
						}
						else {
							table += \"<td class='empty'>&nbsp;</td>\";
						}
					}
					table += \"</tr>\";
				}
				
				table += \"</table>\";
				return table;
			}
			this.show = show;
			function show(field) {
				can_hide = 0;
				if (dateField == field) {
					return;
				}
				else {
					dateField = field;
				}
				if(dateField) {
					try {
						var dateString = new String(dateField.value);
						var dateParts = dateString.split('$sep');
";
		if ($how == 0)
			$js .= "
						selectedMonth = parseInt(dateParts[0],10);
						selectedDay = parseInt(dateParts[1],10);
						selectedYear = parseInt(dateParts[2],10);
";
		elseif ($how == 1)
			$js .= "
						selectedDay = parseInt(dateParts[0],10);
						selectedMonth = parseInt(dateParts[1],10);
						selectedYear = parseInt(dateParts[2],10);
";
		elseif ($how == 2)
			$js .= "
						selectedYear = parseInt(dateParts[0],10);
						selectedMonth = parseInt(dateParts[1],10);
						selectedDay = parseInt(dateParts[2],10);
";
		elseif ($how == 3)
			$js .= "
						selectedDay = parseInt(dateParts[1],10);
						selectedMonth = parseInt(tmonths.indexOf(dateParts[0]),10);
						selectedYear = parseInt(dateParts[2],10);
";
		elseif ($how == 4)
			$js .= "
						selectedDay = parseInt(dateParts[0],10);
						selectedMonth = parseInt(tmonths.indexOf(dateParts[1]),10);
						selectedYear = parseInt(dateParts[2],10);
";
		else
			$js .= "
						selectedYear = parseInt(dateParts[0],10);
						selectedMonth = parseInt(tmonths.indexOf(dateParts[1]),10);
						selectedDay = parseInt(dateParts[2],10);
";
		$js .= "
					}
					catch(e) {}
				}
				if (!(selectedYear && selectedMonth && selectedDay)) {
";
		if ($date_system == 1 || $date_system == 2) {
			$js .= "
					selectedMonth = $cmon;
					selectedDay = $cday;
					selectedYear = $cyear;
";
		}
		else {
			$js .= "
					selectedMonth = getCurrentMonth();
					selectedDay = getCurrentDay();
					selectedYear = getCurrentYear();
";
		}
		$js .= "
				}
				currentMonth = selectedMonth;
				currentDay = selectedDay;
				currentYear = selectedYear;
				if(document.getElementById){
					calendar = document.getElementById(calendarId);
					calendar.innerHTML = calendarDrawTable(currentYear, currentMonth);
					var fieldPos = new positionInfo(dateField);
					var calendarPos = new positionInfo(calendarId);
					var x = fieldPos.getElementLeft();
					var y = fieldPos.getElementBottom();
					setProperty('left', x + 'px');
					setProperty('top', y + 'px');
					setProperty('display', 'block');
					if (document.all) {
						setElementProperty('left', x + 'px', 'CCIframe');
						setElementProperty('top', y + 'px', 'CCIframe');
						setElementProperty('width', calendarPos.getElementWidth() + 'px', 'CCIframe');
						setElementProperty('height', calendarPos.getElementHeight() + 'px', 'CCIframe');
						setElementProperty('display', 'block', 'CCIframe');
					}
				}
			}
			this.hide = hide;
			function hide() {
				if(dateField) {
					setProperty('display', 'none');
					setElementProperty('display', 'none', 'CCIframe');
					dateField = null;
				}
			}
			this.visible = visible;
			function visible() {
				return dateField
			}
			this.can_hide = can_hide;
			var can_hide = 0;
		}
		var cC = new CC();
		function date_picker(textField) {
			cC.show(textField);
		}
		function hideCC() {
			if (cC.visible()) {
				cC.hide();
			}
		}
		function setCCDate(year, month, day) {
			cC.setDate(year, month, day);
		}
		function changeCCYear(change) {
			cC.changeYear(change);
		}
		function changeCCMonth(change) {
			cC.changeMonth(change);
		}
		window.addEventListener('click', function(e) {
			if (document.getElementById('CC').contains(e.target)){}
			else {
				if (cC.visible()) {
				cC.hide();
			}
		}
	});

	document.write(\"<iframe id='CCIframe' src='javascript:false;' frameBorder='0' scrolling='no'></iframe>\");
	document.write(\"<div id='CC'></div>\");";

		cache_js_file($fpath, $js);
	}
	add_js_ufile($fpath);

	return '';
}

function alert($msg) {
	echo "\n<script type=\"text/javascript\">\n"
		. "<!--\n"
		. "alert('$msg');\n"
		. "-->\n"
		. "</script>\n";
}

if (!function_exists('_vd')) {
	function _vd($mixed, $title = '', $exit = false) {
		$str = (!empty($title) ? ($title .':') : '') .'<pre>';
		$str .= print_r($mixed, true); //var_dump($mixed);
		$str .= "</pre>\n";
		display_notification('<table><tr><td>'.$str.'</td></tr></table>');
		if ($exit)
			exit;
	}
}

function _vl($mixed, $title = '', $exit = false) {
	error_log((!empty($title) ? ($title.':') : '') . print_r($mixed, true));
	if ($exit)
		exit;
}

function display_backtrace($cond=true, $msg='') {
	if ($cond) {
		if ($msg) 
			$str = "<center><span class='headingtext'>".$msg."</span></center>\n";
		else
			$str = '';
		$str .= get_backtrace(true);
		display_error($str);
	}
}

//
// FIXME: $payment_services array will be moved to bank_accounts in 2.4.x
//
if (!isset($payment_services)) {
	$payment_services = array(
		'PayPal' => "https://www.paypal.com/xclick?business=<company_email>&item_name=<comment>&amount=<amount>&currency_code=<currency>",
		'XUMM XRP' => "https://xumm.app/detect/request:{{account}}?amount=<amount>"
	);
}
/*
*	Payment link generation. Options provided during invoice generation:
*	company_email, comment, amount, currency
*/
function payment_link($name, $options) {
	global $payment_services;

	$link = @$payment_services[$name];

	if (!$link) return null;

	$patterns = array();
	foreach ($options as $id => $option)
		$patterns['<'.$id.'>'] = urlencode($options[$id]);

	if (substr($name, 0, 4) == 'XUMM') {
		// Get the account number for 'XRPL Account' from 'bank_accounts' table
		$sql = "SELECT bank_account_number FROM ".TB_PREF."bank_accounts WHERE bank_name = '$name'";
        $result = db_query($sql, "could not retrieve $name bank account number");
		$row = db_fetch($result);
		if ($row == false) {
			display_error(sprintf(_("Could not retrieve %s bank account number"), $name));
			return false;
		}
		$account = $row['bank_account_number'];

		// Replace the {{account}} placeholder in the XUMM URL with the extracted account value
		$link = str_replace('{{account}}', urlencode($account), $link);

		// Extract XRP exchange rate
		$curr_code = substr($name, -3);
		$date = date('Y-m-d'); // Get the current date in 'YYYY-MM-DD' format
		$sql = "SELECT rate_buy FROM ".TB_PREF."exchange_rates WHERE curr_code = '$curr_code' AND date_ = '$date'";
		$result = db_query($sql, "could not retrieve exchange rate for $curr_code - $date");
		$row = db_fetch($result);
		if ($row == false) {
			display_error(sprintf(_("Could not retrieve exchange rate for %s - %s"), $curr_code, $date));
			return false;
		}
		$exchange_rate = $row['rate_buy'];
		if ($exchange_rate == 0) $exchange_rate = 1;

		// Divide the amount by the exchange rate
		$options['amount'] /= $exchange_rate;

		// Replace the <amount> placeholder in the XUMM URL with the modified amount value
		$link = str_replace('<amount>', urlencode($options['amount']), $link);
	}

	return strtr($link, $patterns);
}

function trans_editor_link($type, $trans_no) {
	global $path_to_root;

	$editor_url = array(
		ST_JOURNAL => "/gl/gl_journal.php?ModifyGL=Yes&trans_no=%d&trans_type=%d",
		ST_BANKPAYMENT => "/gl/gl_bank.php?ModifyPayment=Yes&trans_no=%d&trans_type=%d",
		ST_BANKDEPOSIT => "/gl/gl_bank.php?ModifyDeposit=Yes&trans_no=%d&trans_type=%d",
		ST_BANKTRANSFER => "/gl/bank_transfer.php?ModifyTransfer=Yes&trans_no=%d&trans_type=%d",
		ST_SALESINVOICE => "/sales/customer_invoice.php?ModifyInvoice=%d",
		ST_CUSTCREDIT => "/sales/credit_note_entry.php?ModifyCredit=%d", // only for free hand credit notes (debtors_trans.order_==0)
		ST_CUSTPAYMENT => "/sales/customer_payments.php?trans_no=%d",
		ST_CUSTDELIVERY => "/sales/customer_delivery.php?ModifyDelivery=%d",
//   16=> Location Transfer,
//   17=> Inventory Adjustment,
		ST_SUPPINVOICE => "/purchasing/supplier_invoice.php?ModifyInvoice=%d",
		ST_SUPPCREDIT =>  "/purchasing/supplier_credit.php?ModifyCredit=%d",
//   21=> Supplier Credit Note,
//   22=> Supplier Payment,
//   25=> Purchase Order Delivery,
		ST_PURCHORDER => "/purchasing/po_entry_items.php?ModifyOrderNumber=%d",
//   28=> Work Order Issue,
//   29=> Work Order Production",
		ST_WORKORDER => "/manufacturing/work_order_entry.php?trans_no=%d",
		ST_SALESORDER => "/sales/sales_order_entry.php?ModifyOrderNumber=%d",
		ST_SALESQUOTE => "/sales/sales_order_entry.php?ModifyQuotationNumber=%d",
	//   35=> Cost Update,
	);

	return !isset($editor_url[$type]) ? '--' :
		(is_closed_trans($type, $trans_no) ? set_icon(ICON_CLOSED, _('Closed')) :
			pager_link(_('Edit'), sprintf($editor_url[$type], $trans_no, $type), ICON_EDIT));
}

function company_logo_on_view() {
	global $SysPrefs;

	$logo = company_path().'/images/'.$SysPrefs->prefs['coy_logo'];
	$nl = 1;

	if (!empty($SysPrefs->prefs['company_logo_on_views']) && $SysPrefs->prefs['coy_logo'] != '' && file_exists($logo))
		$text = "<img style='max-width: 250px;margin-left: 35px;' src='$logo'>";
	else
		$text = "<center><h1>".$SysPrefs->prefs['coy_name']."</h1></center>";

	echo "<table width='100%'><tr><td width='50%'>".$text."</td><td><table width='100%'><tr><td><h5>".$SysPrefs->prefs['coy_name']."</h5></td></tr><tr><td>".get_company_pref('postal_address').'</td></tr><tr><td>'.$SysPrefs->prefs['phone'].'</td></tr><tr><td>'.$SysPrefs->prefs['email']."</td></tr></table></td></tr></table>";
	echo br();
}
