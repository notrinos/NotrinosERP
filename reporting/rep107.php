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
$page_security = $_POST['PARAM_0'] == $_POST['PARAM_1'] ? 'SA_SALESTRANSVIEW' : 'SA_SALESBULKREP';
$path_to_root = '..';

include_once($path_to_root.'/includes/session.inc');
include_once($path_to_root.'/includes/date_functions.inc');
include_once($path_to_root.'/includes/data_checks.inc');
include_once($path_to_root.'/sales/includes/sales_db.inc');

//----------------------------------------------------------------------------------------------------

function get_invoice_range($from, $to, $currency=false) {
	global $SysPrefs;

	$ref = ($SysPrefs->print_invoice_no() == 1 ? 'trans_no' : 'reference');

	$sql = "SELECT trans.trans_no, trans.reference";

	// if($currency !== false)
		// $sql .= ", cust.curr_code";

	$sql .= " FROM ".TB_PREF."debtor_trans trans 
			LEFT JOIN ".TB_PREF."voided voided ON trans.type=voided.type AND trans.trans_no=voided.id";

	if ($currency !== false)
		$sql .= " LEFT JOIN ".TB_PREF."debtors_master cust ON trans.debtor_no=cust.debtor_no";

	$sql .= " WHERE trans.type=".ST_SALESINVOICE
		." AND ISNULL(voided.id)"
		." AND trans.trans_no BETWEEN ".db_escape($from)." AND ".db_escape($to);			

	if ($currency !== false)
		$sql .= " AND cust.curr_code=".db_escape($currency);

	$sql .= " ORDER BY trans.tran_date, trans.$ref";

	return db_query($sql, 'Cant retrieve invoice range');
}

print_invoices();

//----------------------------------------------------------------------------------------------------

function print_invoices() {
	global $path_to_root, $SysPrefs;
	
	$show_this_payment = true; // include payments invoiced here in summary

	include_once($path_to_root . '/reporting/includes/pdf_report.inc');

	$from = $_POST['PARAM_0'];
	$to = $_POST['PARAM_1'];
	$currency = $_POST['PARAM_2'];
	$email = $_POST['PARAM_3'];
	$pay_service = $_POST['PARAM_4'];
	$comments = $_POST['PARAM_5'];
	$customer = $_POST['PARAM_6'];
	$orientation = $_POST['PARAM_7'];

	if (!$from || !$to) return;

	$orientation = ($orientation ? 'L' : 'P');
	$dec = user_price_dec();

	$fno = explode('-', $from);
	$tno = explode('-', $to);
	$from = min($fno[0], $tno[0]);
	$to = max($fno[0], $tno[0]);

	//-------------code-Descr-Qty--uom--tax--prc--Disc-Tot--//
	$cols = array(4, 60, 225, 300, 325, 385, 450, 515);

	// $headers in doctext.inc
	$aligns = array('left',	'left',	'right', 'center', 'right', 'right', 'right');

	$params = array('comments' => $comments);

	$cur = get_company_Pref('curr_default');

	if ($email == 0)
		$rep = new FrontReport(_('INVOICE'), 'InvoiceBulk', user_pagesize(), 9, $orientation);
	if ($orientation == 'L')
		recalculate_cols($cols);

	$range = Array();
	if ($currency == ALL_TEXT)
		$range = get_invoice_range($from, $to);
	else
		$range = get_invoice_range($from, $to, $currency);

	while($row = db_fetch($range)) {
		if (!exists_customer_trans(ST_SALESINVOICE, $row['trans_no']))
			continue;
		$sign = 1;
		$myrow = get_customer_trans($row['trans_no'], ST_SALESINVOICE);

		if ($customer && $myrow['debtor_no'] != $customer)
			continue;
		
		// if ($currency != ALL_TEXT && $myrow['curr_code'] != $currency) {
		// 	continue;
		// }
			
		$baccount = get_default_bank_account($myrow['curr_code']);
		$params['bankaccount'] = $baccount['id'];

		$branch = get_branch($myrow['branch_code']);
		$sales_order = get_sales_order_header($myrow['order_'], ST_SALESORDER);
		if ($email == 1) {
			$rep = new FrontReport('', '', user_pagesize(), 9, $orientation);
			$rep->title = _('INVOICE');
			$rep->filename = 'Invoice' . $myrow['reference'] . '.pdf';
		}	
		$rep->currency = $cur;
		$rep->Font();
		$rep->Info($params, $cols, null, $aligns);

		$contacts = get_branch_contacts($branch['branch_code'], 'invoice', $branch['debtor_no'], true);
		$baccount['payment_service'] = $pay_service;
		$rep->SetCommonData($myrow, $branch, $sales_order, $baccount, ST_SALESINVOICE, $contacts);
		$rep->SetHeaderType('Header2');
		$rep->NewPage();
		// calculate summary start row for later use
		$summary_start_row = $rep->bottomMargin + (15 * $rep->lineHeight);

		if ($rep->formData['prepaid']) {
			$result = get_sales_order_invoices($myrow['order_']);
			$prepayments = array();
			while($inv = db_fetch($result)) {
				$prepayments[] = $inv;
				if ($inv['trans_no'] == $row['trans_no'])
				break;
			}

			if (count($prepayments) > ($show_this_payment ? 0 : 1))
				$summary_start_row += (count($prepayments)) * $rep->lineHeight;
			else
				unset($prepayments);
		}

		$result = get_customer_trans_details(ST_SALESINVOICE, $row['trans_no']);
		$SubTotal = 0;
		while ($myrow2=db_fetch($result)) {
			if ($myrow2['quantity'] == 0)
				continue;

			$Net = round2($sign * ((1 - $myrow2['discount_percent']) * $myrow2['unit_price'] * $myrow2['quantity']), user_price_dec());
			$SubTotal += $Net;
			$DisplayPrice = number_format2($myrow2['unit_price'],$dec);
			$DisplayQty = number_format2($sign*$myrow2['quantity'],get_qty_dec($myrow2['stock_id']));
			$DisplayNet = number_format2($Net,$dec);
			if ($myrow2['discount_percent']==0)
				$DisplayDiscount ='';
			else
				$DisplayDiscount = number_format2($myrow2['discount_percent']*100,user_percent_dec()) . '%';
			$c=0;
			$rep->TextCol($c++, $c,	$myrow2['stock_id'], -2);
			$oldrow = $rep->row;
			$rep->TextColLines($c++, $c, $myrow2['StockDescription'], -2);
			if (!empty($SysPrefs->prefs['long_description_invoice']) && !empty($myrow2['StockLongDescription'])) {
				$c--;
				$rep->TextColLines($c++, $c, $myrow2['StockLongDescription'], -2);
			}
			$newrow = $rep->row;
			$rep->row = $oldrow;
			if ($Net != 0.0 || !is_service($myrow2['mb_flag']) || !$SysPrefs->no_zero_lines_amount()) {
				$rep->TextCol($c++, $c,	$DisplayQty, -2);
				$rep->TextCol($c++, $c,	$myrow2['units'], -2);
				$rep->TextCol($c++, $c,	$DisplayPrice, -2);
				$rep->TextCol($c++, $c,	$DisplayDiscount, -2);
				$rep->TextCol($c++, $c,	$DisplayNet, -2);
			}
			$rep->row = $newrow;
			//$rep->NewLine(1);
			if ($rep->row < $summary_start_row)
				$rep->NewPage();
		}

		$memo = get_comments_string(ST_SALESINVOICE, $row['trans_no']);
		if ($memo != '') {
			$rep->NewLine();
			$rep->TextColLines(1, 3, $memo, -2);
		}

		$DisplaySubTot = number_format2($SubTotal,$dec);

		// set to start of summary line:
		$rep->row = $summary_start_row;
		if (isset($prepayments)) {
			// Partial invoices table
			$rep->TextCol(0, 3,_('Prepayments invoiced to this order up to day:'));
			$rep->TextCol(0, 3,	str_pad('', 150, '_'));
			$rep->cols[2] -= 20;
			$rep->aligns[2] = 'right';
			$rep->NewLine(); $c = 0; $tot_pym=0;
			$rep->TextCol(0, 3,	str_pad('', 150, '_'));
			$rep->TextCol($c++, $c, _('Date'));
			$rep->TextCol($c++, $c,	_('Invoice reference'));
			$rep->TextCol($c++, $c,	_('Amount'));

			foreach ($prepayments as $invoice) {
				if ($show_this_payment || ($invoice['reference'] != $myrow['reference'])) {
					$rep->NewLine();
					$c = 0; $tot_pym += $invoice['prep_amount'];
					$rep->TextCol($c++, $c,	sql2date($invoice['tran_date']));
					$rep->TextCol($c++, $c,	$invoice['reference']);
					$rep->TextCol($c++, $c, number_format2($invoice['prep_amount'], $dec));
				}
				if ($invoice['reference']==$myrow['reference']) break;
			}
			$rep->TextCol(0, 3,	str_pad('', 150, '_'));
			$rep->NewLine();
			$rep->TextCol(1, 2,	_('Total payments:'));
			$rep->TextCol(2, 3,	number_format2($tot_pym, $dec));
		}
		$doctype = ST_SALESINVOICE;
		$rep->row = $summary_start_row;
		$rep->cols[2] += 20;
		$rep->cols[3] += 20;
		$rep->aligns[3] = 'left';

		$rep->TextCol(3, 6, _('Sub-total'), -2);
		$rep->TextCol(6, 7,	$DisplaySubTot, -2);
		$rep->NewLine();
		if ($myrow['ov_freight'] != 0.0) {
			$DisplayFreight = number_format2($sign*$myrow['ov_freight'],$dec);
			$rep->TextCol(3, 6, _('Shipping'), -2);
			$rep->TextCol(6, 7,	$DisplayFreight, -2);
			$rep->NewLine();
		}	
		$tax_items = get_trans_tax_details(ST_SALESINVOICE, $row['trans_no']);
		$first = true;
		while ($tax_item = db_fetch($tax_items)) {
			if ($tax_item['amount'] == 0)
				continue;
			$DisplayTax = number_format2($sign*$tax_item['amount'], $dec);

			if ($SysPrefs->suppress_tax_rates() == 1)
				$tax_type_name = $tax_item['tax_type_name'];
			else
				$tax_type_name = $tax_item['tax_type_name'].' ('.$tax_item['rate'].'%) ';

			if ($myrow['tax_included']) {
				if ($SysPrefs->alternative_tax_include_on_docs() == 1) {
					if ($first) {
						$rep->TextCol(3, 6, _('Total Tax Excluded'), -2);
						$rep->TextCol(6, 7,	number_format2($sign*$tax_item['net_amount'], $dec), -2);
						$rep->NewLine();
					}
					$rep->TextCol(3, 6, $tax_type_name, -2);
					$rep->TextCol(6, 7,	$DisplayTax, -2);
					$first = false;
				}
				else
					$rep->TextCol(3, 6, _('Included') . ' ' . $tax_type_name . _('Amount') . ': ' . $DisplayTax, -2);
			}
			else {
				$rep->TextCol(3, 6, $tax_type_name, -2);
				$rep->TextCol(6, 7,	$DisplayTax, -2);
			}
			$rep->NewLine();
		}

		$rep->NewLine();
		$DisplayTotal = number_format2($sign*($myrow['ov_freight'] + $myrow['ov_gst'] + $myrow['ov_amount']+$myrow['ov_freight_tax']),$dec);
		$rep->Font('bold');
		if (!$myrow['prepaid']) $rep->Font('bold');
			$rep->TextCol(3, 6, $rep->formData['prepaid'] ? _('TOTAL ORDER VAT INCL.') : _('TOTAL INVOICE'), - 2);
		$rep->TextCol(6, 7, $DisplayTotal, -2);
		if ($rep->formData['prepaid']) {
			$rep->NewLine();
			$rep->Font('bold');
			$rep->TextCol(3, 6, $rep->formData['prepaid']=='final' ? _('THIS INVOICE') : _('TOTAL INVOICE'), - 2);
			$rep->TextCol(6, 7, number_format2($myrow['prep_amount'], $dec), -2);
		}
		$words = price_in_words($rep->formData['prepaid'] ? $myrow['prep_amount'] : $myrow['Total'], array( 'type' => ST_SALESINVOICE, 'currency' => $myrow['curr_code']));
		if ($words != '') {
			$rep->NewLine(1);
			$rep->TextCol(1, 7, $myrow['curr_code'] . ': ' . $words, - 2);
		}
		$rep->Font();
		if ($email == 1)
			$rep->End($email, sprintf(_("Invoice %s from %s"), $myrow['reference'], get_company_pref('coy_name')));
	}
	if ($email == 0)
		$rep->End();
}
