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
$page_security = $_POST['PARAM_0'] == $_POST['PARAM_1'] ?
	'SA_SALESTRANSVIEW' : 'SA_SALESBULKREP';
// ----------------------------------------------------------------
// $ Revision:	2.0 $
// Creator:	Joe Hunt
// date_:	2005-05-19
// Title:	Print Credit Notes
// ----------------------------------------------------------------
$path_to_root="..";

include_once($path_to_root . "/includes/session.inc");
include_once($path_to_root . "/includes/date_functions.inc");
include_once($path_to_root . "/includes/data_checks.inc");
include_once($path_to_root . "/sales/includes/sales_db.inc");

//----------------------------------------------------------------------------------------------------

print_credits();

//----------------------------------------------------------------------------------------------------

function print_credits()
{
	global $path_to_root, $SysPrefs;
	
	include_once($path_to_root . "/reporting/includes/pdf_report.inc");

	$from = $_POST['PARAM_0'];
	$to = $_POST['PARAM_1'];
	$currency = $_POST['PARAM_2'];
	$email = $_POST['PARAM_3'];
	$paylink = $_POST['PARAM_4'];
	$comments = $_POST['PARAM_5'];
	$orientation = $_POST['PARAM_6'];

	if (!$from || !$to) return;

	$orientation = ($orientation ? 'L' : 'P');
	$dec = user_price_dec();

 	$fno = explode("-", $from);
	$tno = explode("-", $to);
	$from = min($fno[0], $tno[0]);
	$to = max($fno[0], $tno[0]);

	$cols = array(4, 60, 225, 300, 325, 385, 450, 515);

	// $headers in doctext.inc
	$aligns = array('left',	'left',	'right', 'left', 'right', 'right', 'right');

	$params = array('comments' => $comments);

	$cur = get_company_Pref('curr_default');

	if ($email == 0)
		$rep = new FrontReport(_('CREDIT NOTE'), "InvoiceBulk", user_pagesize(), 9, $orientation);
    if ($orientation == 'L')
    	recalculate_cols($cols);

	for ($i = $from; $i <= $to; $i++)
	{
		if (!exists_customer_trans(ST_CUSTCREDIT, $i))
			continue;
		$sign = -1;
		$myrow = get_customer_trans($i, ST_CUSTCREDIT);
		if ($currency != ALL_TEXT && $myrow['curr_code'] != $currency) {
			continue;
		}
		$baccount = get_default_bank_account($myrow['curr_code']);
		$params['bankaccount'] = $baccount['id'];

		$branch = get_branch($myrow["branch_code"]);
		$branch['disable_branch'] = $paylink; // helper
		$sales_order = null;
		if ($email == 1)
		{
			$rep = new FrontReport("", "", user_pagesize(), 9, $orientation);
			$rep->title = _('CREDIT NOTE');
			$rep->filename = "CreditNote" . $myrow['reference'] . ".pdf";
		}
		$rep->currency = $cur;
		$rep->Font();
		$rep->Info($params, $cols, null, $aligns);

		$contacts = get_branch_contacts($branch['branch_code'], 'invoice', $branch['debtor_no'], true);
		$rep->SetCommonData($myrow, $branch, $sales_order, $baccount, ST_CUSTCREDIT, $contacts);
		$rep->SetHeaderType('Header2');
		$rep->NewPage();

		$result = get_customer_trans_details(ST_CUSTCREDIT, $i);
		$SubTotal = 0;
		while ($myrow2=db_fetch($result))
		{
			if ($myrow2["quantity"] == 0)
				continue;

			$Net = round2($sign * ((1 - $myrow2["discount_percent"]) * $myrow2["unit_price"] * $myrow2["quantity"]),
			   user_price_dec());
			$SubTotal += $Net;
			$DisplayPrice = number_format2($myrow2["unit_price"],$dec);
			$DisplayQty = number_format2($sign*$myrow2["quantity"],get_qty_dec($myrow2['stock_id']));
			$DisplayNet = number_format2($Net,$dec);
			if ($myrow2["discount_percent"]==0)
				$DisplayDiscount ="";
			else
				$DisplayDiscount = number_format2($myrow2["discount_percent"]*100,user_percent_dec()) . "%";
			$rep->TextCol(0, 1,	$myrow2['stock_id'], -2);
			$oldrow = $rep->row;
			$rep->TextColLines(1, 2, $myrow2['StockDescription'], -2);
			$newrow = $rep->row;
			$rep->row = $oldrow;
			$rep->TextCol(2, 3,	$DisplayQty, -2);
			$rep->TextCol(3, 4,	$myrow2['units'], -2);
			$rep->TextCol(4, 5,	$DisplayPrice, -2);
			$rep->TextCol(5, 6,	$DisplayDiscount, -2);
			$rep->TextCol(6, 7,	$DisplayNet, -2);
			$rep->row = $newrow;
			if ($rep->row < $rep->bottomMargin + (15 * $rep->lineHeight))
				$rep->NewPage();
		}

		$memo = get_comments_string(ST_CUSTCREDIT, $i);
		if ($memo != "")
		{
			$rep->NewLine();
			$rep->TextColLines(1, 3, $memo, -2);
		}

		$DisplaySubTot = number_format2($SubTotal,$dec);

		$rep->row = $rep->bottomMargin + (15 * $rep->lineHeight);
		$doctype = ST_CUSTCREDIT;

		$rep->TextCol(3, 6, _("Sub-total"), -2);
		$rep->TextCol(6, 7,	$DisplaySubTot, -2);
		$rep->NewLine();
		if ($myrow['ov_freight'] != 0.0)
		{
			$DisplayFreight = number_format2($sign*$myrow["ov_freight"],$dec);
			$rep->TextCol(3, 6, _("Shipping"), -2);
			$rep->TextCol(6, 7,	$DisplayFreight, -2);
			$rep->NewLine();
		}
		$tax_items = get_trans_tax_details(ST_CUSTCREDIT, $i);
		$first = true;
		while ($tax_item = db_fetch($tax_items))
		{
			if ($tax_item['amount'] == 0)
				continue;
			$DisplayTax = number_format2($sign*$tax_item['amount'], $dec);

			if ($SysPrefs->suppress_tax_rates() == 1)
				$tax_type_name = $tax_item['tax_type_name'];
			else
				$tax_type_name = $tax_item['tax_type_name']." (".$tax_item['rate']."%) ";

			if ($myrow['tax_included'])
			{
				if ($SysPrefs->alternative_tax_include_on_docs() == 1)
				{
					if ($first)
					{
						$rep->TextCol(3, 6, _("Total Tax Excluded"), -2);
						$rep->TextCol(6, 7,	number_format2($sign*$tax_item['net_amount'], $dec), -2);
						$rep->NewLine();
					}
					$rep->TextCol(3, 6, $tax_type_name, -2);
					$rep->TextCol(6, 7,	$DisplayTax, -2);
					$first = false;
				}
				else
					$rep->TextCol(3, 7, _("Included") . " " . $tax_type_name . _("Amount") . ": " . $DisplayTax, -2);
			}
			else
			{
				$rep->TextCol(3, 6, $tax_type_name, -2);
				$rep->TextCol(6, 7,	$DisplayTax, -2);
			}
			$rep->NewLine();
		}
		$rep->NewLine();
		$DisplayTotal = number_format2($sign*($myrow["ov_freight"] + $myrow["ov_gst"] +
			$myrow["ov_amount"]+$myrow["ov_freight_tax"]),$dec);
		$rep->Font('bold');
		$rep->TextCol(3, 6, _("TOTAL CREDIT"), - 2);
		$rep->TextCol(6, 7, $DisplayTotal, -2);
		$words = price_in_words($myrow['Total'], ST_CUSTCREDIT);
		if ($words != "")
		{
			$rep->NewLine(1);
			$rep->TextCol(1, 7, $myrow['curr_code'] . ": " . $words, - 2);
		}	
		$rep->Font();
		if ($email == 1)
		{
			$myrow['dimension_id'] = $paylink; // helper for pmt link
			$rep->End($email);
		}
	}
	if ($email == 0)
		$rep->End();
}

