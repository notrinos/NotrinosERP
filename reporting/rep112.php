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
// Title:	Receipts
// ----------------------------------------------------------------
$path_to_root='..';

include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/includes/date_functions.inc');
include_once($path_to_root . '/includes/data_checks.inc');

//----------------------------------------------------------------------------------------------------

print_receipts();

//----------------------------------------------------------------------------------------------------

function get_receipt($type, $trans_no) {
	$sql = "SELECT trans.*,
				(trans.ov_amount + trans.ov_gst + trans.ov_freight + trans.ov_freight_tax) AS Total,
				trans.ov_discount, 
				debtor.name AS DebtorName,
				debtor.debtor_ref,
				debtor.curr_code,
				debtor.payment_terms,
				debtor.tax_id AS tax_id,
				debtor.address
				FROM ".TB_PREF."debtor_trans trans,"
					.TB_PREF."debtors_master debtor
				WHERE trans.debtor_no = debtor.debtor_no
				AND trans.type = ".db_escape($type)."
				AND trans.trans_no = ".db_escape($trans_no);
	$result = db_query($sql, 'The remittance cannot be retrieved');
	if (db_num_rows($result) == 0)
		return false;
	return db_fetch($result);
}

function print_receipts() {
	global $path_to_root, $systypes_array;

	include_once($path_to_root . '/reporting/includes/pdf_report.inc');

	$from = $_POST['PARAM_0'];
	$to = $_POST['PARAM_1'];
	$currency = $_POST['PARAM_2'];
	$email = $_POST['PARAM_3'];
	$comments = $_POST['PARAM_4'];
	$orientation = $_POST['PARAM_5'];

	if (!$from || !$to) return;

	$orientation = ($orientation ? 'L' : 'P');
	$dec = user_price_dec();

	$fno = explode('-', $from);
	$tno = explode('-', $to);
	$from = min($fno[0], $tno[0]);
	$to = max($fno[0], $tno[0]);

	$cols = array(4, 85, 150, 225, 275, 360, 450, 515);

	// $headers in doctext.inc
	$aligns = array('left',	'left',	'left', 'left', 'right', 'right', 'right');

	$params = array('comments' => $comments);

	$cur = get_company_Pref('curr_default');

	if ($email == 0)
		$rep = new FrontReport(_('RECEIPT'), 'ReceiptBulk', user_pagesize(), 9, $orientation);
	if ($orientation == 'L')
		recalculate_cols($cols);

	for ($i = $from; $i <= $to; $i++) {
		if ($fno[0] == $tno[0])
			$types = array($fno[1]);
		else
			$types = array(ST_BANKDEPOSIT, ST_CUSTPAYMENT);
		foreach ($types as $j) {
			$myrow = get_receipt($j, $i);
			if (!$myrow)
				continue;
			if ($currency != ALL_TEXT && $myrow['curr_code'] != $currency)
				continue;
			
			$res = get_bank_trans($j, $i);
			$baccount = db_fetch($res);
			$params['bankaccount'] = $baccount['bank_act'];

			if ($email == 1) {
				$rep = new FrontReport('', '', user_pagesize(), 9, $orientation);
				$rep->title = _('RECEIPT');
				$rep->filename = 'Receipt' . $i . '.pdf';
			}
			$rep->currency = $cur;
			$rep->Font();
			$rep->Info($params, $cols, null, $aligns);

			$contacts = get_branch_contacts($myrow['branch_code'], 'invoice', $myrow['debtor_no']);
			$rep->SetCommonData($myrow, null, $myrow, $baccount, ST_CUSTPAYMENT, $contacts);
			$rep->SetHeaderType('Header2');
			$rep->NewPage();
			$result = get_allocatable_to_cust_transactions($myrow['debtor_no'], $myrow['trans_no'], $myrow['type']);

			$doctype = ST_CUSTPAYMENT;

			$total_allocated = 0;
			$rep->TextCol(0, 4,	_('As advance / full / part / payment towards:'), -2);
			$rep->NewLine(2);

			while ($myrow2=db_fetch($result)) {
				$rep->TextCol(0, 1,	$systypes_array[$myrow2['type']], -2);
				$rep->TextCol(1, 2,	$myrow2['reference'], -2);
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
			if ($memo != '') {
				$rep->NewLine();
				$rep->TextColLines(1, 5, $memo, -2);
			}

			$rep->row = $rep->bottomMargin + (16 * $rep->lineHeight);

			$rep->TextCol(3, 6, _('Total Allocated'), -2);
			$rep->AmountCol(6, 7, $total_allocated, $dec, -2);
			$rep->NewLine();
			$rep->TextCol(3, 6, _('Left to Allocate'), -2);
			$rep->AmountCol(6, 7, $myrow['Total'] + $myrow['ov_discount'] - $total_allocated, $dec, -2);
			if (floatcmp($myrow['ov_discount'], 0)) {
				$rep->NewLine();
				$rep->TextCol(3, 6, _('Discount'), - 2);
				$rep->AmountCol(6, 7, -$myrow['ov_discount'], $dec, -2);
			}	
			$rep->NewLine();
			$rep->Font('bold');
			$rep->TextCol(3, 6, _('TOTAL RECEIPT'), - 2);
			$rep->AmountCol(6, 7, $myrow['Total'], $dec, -2);

			$words = price_in_words($myrow['Total'], ST_CUSTPAYMENT);
			if ($words != '') {
				$rep->NewLine(1);
				$rep->TextCol(0, 7, $myrow['curr_code'] . ': ' . $words, - 2);
			}
			$rep->Font();
			$rep->NewLine();
			$rep->TextCol(6, 7, _('Received / Sign'), - 2);
			$rep->NewLine();
			$rep->TextCol(0, 2, _('By Cash / Cheque* / Draft No.'), - 2);
			$rep->TextCol(2, 4, '______________________________', - 2);
			$rep->TextCol(4, 5, _('Dated'), - 2);
			$rep->TextCol(5, 6, '__________________', - 2);
			$rep->NewLine(1);
			$rep->TextCol(0, 2, _('Drawn on Bank'), - 2);
			$rep->TextCol(2, 4, '______________________________', - 2);
			$rep->TextCol(4, 5, _('Branch'), - 2);
			$rep->TextCol(5, 6, '__________________', - 2);
			$rep->TextCol(6, 7, '__________________');
			if ($email == 1)
				$rep->End($email);
		}
	}
	if ($email == 0)
		$rep->End();
}