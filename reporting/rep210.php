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
	'SA_SUPPTRANSVIEW' : 'SA_SUPPBULKREP';
// ----------------------------------------------------------------
// $ Revision:	2.0 $
// Creator:	Joe Hunt
// date_:	2005-05-19
// Title:	Purchase Remittance
// ----------------------------------------------------------------
$path_to_root="..";

include_once($path_to_root . "/includes/session.inc");
include_once($path_to_root . "/includes/date_functions.inc");
include_once($path_to_root . "/includes/data_checks.inc");
include_once($path_to_root . "/includes/db/crm_contacts_db.inc");

//----------------------------------------------------------------------------------------------------

print_remittances();

//----------------------------------------------------------------------------------------------------
function get_remittance($type, $trans_no)
{
   	$sql = "SELECT trans.*, 
   		(trans.ov_amount+trans.ov_gst) AS Total,
   		trans.ov_discount,
   		supplier.supp_name,  supplier.supp_account_no, 
   		supplier.curr_code, supplier.payment_terms, supplier.gst_no AS tax_id, 
   		supplier.address
		FROM "
			.TB_PREF."supp_trans trans,"
			.TB_PREF."suppliers supplier
		WHERE trans.supplier_id = supplier.supplier_id
		AND trans.type = ".db_escape($type)."
		AND trans.trans_no = ".db_escape($trans_no);
   	$result = db_query($sql, "The remittance cannot be retrieved");
   	if (db_num_rows($result) == 0)
   		return false;
    return db_fetch($result);
}

function print_remittances()
{
	global $path_to_root, $systypes_array;

	include_once($path_to_root . "/reporting/includes/pdf_report.inc");

	$from = $_POST['PARAM_0'];
	$to = $_POST['PARAM_1'];
	$currency = $_POST['PARAM_2'];
	$email = $_POST['PARAM_3'];
	$comments = $_POST['PARAM_4'];
	$orientation = $_POST['PARAM_5'];

	if (!$from || !$to) return;

	$orientation = ($orientation ? 'L' : 'P');
	$dec = user_price_dec();

 	$fno = explode("-", $from);
	$tno = explode("-", $to);
	$from = min($fno[0], $tno[0]);
	$to = max($fno[0], $tno[0]);

	$cols = array(4, 85, 150, 225, 275, 360, 450, 515);

	// $headers in doctext.inc
	$aligns = array('left',	'left',	'left', 'left', 'right', 'right', 'right');

	$params = array('comments' => $comments);

	$cur = get_company_Pref('curr_default');

	if ($email == 0)
		$rep = new FrontReport(_('REMITTANCE'), "RemittanceBulk", user_pagesize(), 9, $orientation);
    if ($orientation == 'L')
    	recalculate_cols($cols);

	for ($i = $from; $i <= $to; $i++)
	{
		if ($fno[0] == $tno[0])
			$types = array($fno[1]);
		else
			$types = array(ST_BANKPAYMENT, ST_SUPPAYMENT, ST_SUPPCREDIT);
		foreach ($types as $j)
		{
			$myrow = get_remittance($j, $i);
			if (!$myrow)
				continue;
			if ($currency != ALL_TEXT && $myrow['curr_code'] != $currency) {
				continue;
			}
			$res = get_bank_trans($j, $i);
			$baccount = db_fetch($res);
			$params['bankaccount'] = $baccount['bank_act'];

			if ($email == 1)
			{
				$rep = new FrontReport("", "", user_pagesize(), 9, $orientation);
				$rep->title = _('REMITTANCE');
				$rep->filename = "Remittance" . $i . ".pdf";
			}
			$rep->currency = $cur;
			$rep->Font();
			$rep->Info($params, $cols, null, $aligns);

			$contacts = get_supplier_contacts($myrow['supplier_id'], 'invoice');
			$rep->SetCommonData($myrow, null, $myrow, $baccount, ST_SUPPAYMENT, $contacts);
			$rep->SetHeaderType('Header2');
			$rep->NewPage();
			$result = get_allocatable_to_supp_transactions($myrow['supplier_id'], $myrow['trans_no'], $myrow['type']);

			$doctype = ST_SUPPAYMENT;

			$total_allocated = 0;
			$rep->TextCol(0, 4,	_("As advance / full / part / payment towards:"), -2);
			$rep->NewLine(2);

			while ($myrow2=db_fetch($result))
			{
				$rep->TextCol(0, 1,	$systypes_array[$myrow2['type']], -2);
				$rep->TextCol(1, 2,	$myrow2['supp_reference'], -2);
				$rep->TextCol(2, 3,	sql2date($myrow2['tran_date']), -2);
				$rep->TextCol(3, 4,	sql2date($myrow2['due_date']), -2);
				$rep->AmountCol(4, 5, $myrow2['Total'], $dec, -2);
				$rep->AmountCol(5, 6, $myrow2['Total'] - $myrow2['alloc'], $dec, -2);
				$rep->AmountCol(6, 7, $myrow2['amt'], $dec, -2);

				$total_allocated += $myrow2['amt'];
				$rep->NewLine(1);
				if ($rep->row < $rep->bottomMargin + (15 * $rep->lineHeight))
					$rep->NewPage();
			}

			$memo = get_comments_string($j, $i);
			if ($memo != "")
			{
				$rep->NewLine();
				$rep->TextColLines(1, 5, $memo, -2);
			}
			$rep->row = $rep->bottomMargin + (16 * $rep->lineHeight);

			$rep->TextCol(3, 6, _("Total Allocated"), -2);
			$rep->AmountCol(6, 7, $total_allocated, $dec, -2);
			$rep->NewLine();
			$rep->TextCol(3, 6, _("Left to Allocate"), -2);
			$myrow['Total'] *= -1;
			$myrow['ov_discount'] *= -1;
			$rep->AmountCol(6, 7, $myrow['Total'] + $myrow['ov_discount'] - $total_allocated, $dec, -2);
			if (floatcmp($myrow['ov_discount'], 0))
			{
				$rep->NewLine();
				$rep->TextCol(3, 6, _("Discount"), - 2);
				$rep->AmountCol(6, 7, -$myrow['ov_discount'], $dec, -2);
			}	

			$rep->NewLine();
			$rep->Font('bold');
			$rep->TextCol(3, 6, _("TOTAL REMITTANCE"), - 2);
			$rep->AmountCol(6, 7, $myrow['Total'], $dec, -2);

			$words = price_in_words($myrow['Total'], ST_SUPPAYMENT);
			if ($words != "")
			{
				$rep->NewLine(2);
				$rep->TextCol(1, 7, $myrow['curr_code'] . ": " . $words, - 2);
			}
			$rep->Font();
			if ($email == 1)
			{
				$myrow['DebtorName'] = $myrow['supp_name'];
				$rep->End($email);
			}
		}
	}
	if ($email == 0)
		$rep->End();
}

