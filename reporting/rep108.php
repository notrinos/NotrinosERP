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
$page_security = 'SA_CUSTSTATREP';
// ----------------------------------------------------------------
// $ Revision:	2.0 $
// Creator:	Joe Hunt
// date_:	2005-05-19
// Title:	Print Statements
// ----------------------------------------------------------------
$path_to_root="..";

include_once($path_to_root . "/includes/session.inc");
include_once($path_to_root . "/includes/date_functions.inc");
include_once($path_to_root . "/includes/data_checks.inc");
include_once($path_to_root . "/sales/includes/sales_db.inc");
include_once($path_to_root . "/includes/db/crm_contacts_db.inc");

//----------------------------------------------------------------------------------------------------

print_statements();

//----------------------------------------------------------------------------------------------------

function getTransactions($debtorno, $date, $show_also_allocated)
{
    $sql = "SELECT trans.type,
        trans.trans_no,
        trans.order_,
        trans.reference,
        trans.tran_date,
        trans.due_date,
        (ov_amount + ov_gst + ov_freight + ov_freight_tax + ov_discount) AS TotalAmount, alloc AS Allocated,
		((trans.type = ".ST_SALESINVOICE.") AND due_date < '$date') AS OverDue
		FROM ".TB_PREF."debtor_trans trans
		LEFT JOIN ".TB_PREF."voided as v
            ON trans.trans_no=v.id AND trans.type=v.type
        WHERE tran_date <= '$date' AND debtor_no = ".db_escape($debtorno)."
			AND trans.type <> ".ST_CUSTDELIVERY." AND ISNULL(v.date_)
			AND ABS(ov_amount + ov_gst + ov_freight + ov_freight_tax + ov_discount) > ". FLOAT_COMP_DELTA;
	
  	if (!$show_also_allocated)
		$sql .= " AND ABS(ABS(ov_amount + ov_gst + ov_freight +	ov_freight_tax + ov_discount) - alloc) > ". FLOAT_COMP_DELTA;
	$sql .= " ORDER BY tran_date";

    return db_query($sql,"No transactions were returned");
}

//----------------------------------------------------------------------------------------------------

function print_statements()
{
	global $path_to_root, $systypes_array;

	include_once($path_to_root . "/reporting/includes/pdf_report.inc");

	$customer = $_POST['PARAM_0'];
	$currency = $_POST['PARAM_1'];
	$show_also_allocated = $_POST['PARAM_2'];
	$email = $_POST['PARAM_3'];
	$comments = $_POST['PARAM_4'];
	$orientation = $_POST['PARAM_5'];

	$orientation = ($orientation ? 'L' : 'P');
	$dec = user_price_dec();

	$cols = array(4, 100, 130, 190,	250, 320, 385, 450, 515);

	//$headers in doctext.inc

	$aligns = array('left',	'left',	'left',	'left',	'right', 'right', 'right', 'right');

	$params = array('comments' => $comments);

	$cur = get_company_pref('curr_default');
	$PastDueDays1 = get_company_pref('past_due_days');
	$PastDueDays2 = 2 * $PastDueDays1;

    if ($orientation == 'L')
    	recalculate_cols($cols);

	$sql = "SELECT debtor_no, name AS DebtorName, address, tax_id, curr_code, curdate() AS tran_date FROM ".TB_PREF."debtors_master";
	if ($customer != ALL_TEXT)
		$sql .= " WHERE debtor_no = ".db_escape($customer);
	else
		$sql .= " ORDER by name";
	$result = db_query($sql, "The customers could not be retrieved");

	while ($myrow=db_fetch($result))
	{
		if ($currency != ALL_TEXT && $myrow['curr_code'] != $currency) {
			continue;
		}	
		$date = date('Y-m-d');

		$myrow['order_'] = "";

		$TransResult = getTransactions($myrow['debtor_no'], $date, $show_also_allocated);
		$baccount = get_default_bank_account($myrow['curr_code']);
		$params['bankaccount'] = $baccount['id'];
		if (db_num_rows($TransResult) == 0)
			continue;
        if ($email == 0 && !isset($rep))
            $rep = new FrontReport(_('STATEMENT'), "StatementBulk", user_pagesize(), 9, $orientation);
		if ($email == 1)
		{
			$rep = new FrontReport("", "", user_pagesize(), 9, $orientation);
			$rep->title = _('STATEMENT');
			$rep->filename = "Statement" . $myrow['debtor_no'] . ".pdf";
			$rep->Info($params, $cols, null, $aligns);
		}

		$contacts = get_customer_contacts($myrow['debtor_no'], 'invoice');
		$rep->currency = $cur;
		$rep->Font();
		$rep->Info($params, $cols, null, $aligns);

		//= get_branch_contacts($branch['branch_code'], 'invoice', $branch['debtor_no']);
		$rep->SetCommonData($myrow, null, null, $baccount, ST_STATEMENT, $contacts);
		$rep->SetHeaderType('Header2');
		$rep->NewPage();
		$rep->NewLine();
		$doctype = ST_STATEMENT;
		$rep->fontSize += 2;
		$rep->TextCol(0, 8, _("Outstanding Transactions"));
		$rep->fontSize -= 2;
		$rep->NewLine(2);
		while ($myrow2=db_fetch($TransResult))
		{
			$DisplayTotal = number_format2(Abs($myrow2["TotalAmount"]),$dec);
			$DisplayAlloc = number_format2($myrow2["Allocated"],$dec);
			$DisplayNet = number_format2($myrow2["TotalAmount"] - $myrow2["Allocated"],$dec);

			$rep->TextCol(0, 1, $systypes_array[$myrow2['type']], -2);
			$rep->TextCol(1, 2,	$myrow2['reference'], -2);
			$rep->TextCol(2, 3,	sql2date($myrow2['tran_date']), -2);
			if ($myrow2['type'] == ST_SALESINVOICE)
				$rep->TextCol(3, 4,	sql2date($myrow2['due_date']), -2);
			if ($myrow2['type'] == ST_SALESINVOICE || $myrow2['type'] == ST_BANKPAYMENT || 
				($myrow2['type'] == ST_JOURNAL && $myrow2["TotalAmount"] > 0.0))
				$rep->TextCol(4, 5,	$DisplayTotal, -2);
			else
				$rep->TextCol(5, 6,	$DisplayTotal, -2);
			$rep->TextCol(6, 7,	$DisplayAlloc, -2);
			$rep->TextCol(7, 8,	$DisplayNet, -2);
			$rep->NewLine();
			if ($rep->row < $rep->bottomMargin + (10 * $rep->lineHeight))
				$rep->NewPage();
		}
		$nowdue = "1-" . $PastDueDays1 . " " . _("Days");
		$pastdue1 = $PastDueDays1 + 1 . "-" . $PastDueDays2 . " " . _("Days");
		$pastdue2 = _("Over") . " " . $PastDueDays2 . " " . _("Days");
		$CustomerRecord = get_customer_details($myrow['debtor_no'], null, $show_also_allocated);
		$str = array(_("Current"), $nowdue, $pastdue1, $pastdue2, _("Total Balance"));
		$str2 = array(number_format2(($CustomerRecord["Balance"] - $CustomerRecord["Due"]),$dec),
			number_format2(($CustomerRecord["Due"]-$CustomerRecord["Overdue1"]),$dec),
			number_format2(($CustomerRecord["Overdue1"]-$CustomerRecord["Overdue2"]) ,$dec),
			number_format2($CustomerRecord["Overdue2"],$dec),
			number_format2($CustomerRecord["Balance"],$dec));
		$col = array($rep->cols[0], $rep->cols[0] + 110, $rep->cols[0] + 210, $rep->cols[0] + 310,
			$rep->cols[0] + 410, $rep->cols[0] + 510);
		$rep->row = $rep->bottomMargin + (10 * $rep->lineHeight - 6);
		for ($i = 0; $i < 5; $i++)
			$rep->TextWrap($col[$i], $rep->row, $col[$i + 1] - $col[$i], $str[$i], 'right');
		$rep->NewLine();
		for ($i = 0; $i < 5; $i++)
			$rep->TextWrap($col[$i], $rep->row, $col[$i + 1] - $col[$i], $str2[$i], 'right');
		if ($email == 1)
			$rep->End($email, _("Statement") . " " . _("as of") . " " . sql2date($date));

	}

    if (!isset($rep))
        display_notification("No customers with outstanding balances found");
	else if ($email == 0)
		$rep->End();
}

