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
$page_security = 'SA_TAXREP';
// ----------------------------------------------------------------
// $ Revision:	2.0 $
// Creator:	Joe Hunt
// date_:	2005-05-19
// Title:	Tax Report
// ----------------------------------------------------------------
$path_to_root="..";

include_once($path_to_root . "/includes/session.inc");
include_once($path_to_root . "/includes/date_functions.inc");
include_once($path_to_root . "/includes/data_checks.inc");
include_once($path_to_root . "/gl/includes/gl_db.inc");

//------------------------------------------------------------------


print_tax_report();

function getTaxTransactions($from, $to)
{
	$fromdate = date2sql($from);
	$todate = date2sql($to);

	$sql = "SELECT tt.name as taxname, taxrec.*, taxrec.amount*ex_rate AS amount,
	            taxrec.net_amount*ex_rate AS net_amount,
				IF(taxrec.trans_type=".ST_BANKPAYMENT." OR taxrec.trans_type=".ST_BANKDEPOSIT.", 
					IF(gl.person_type_id<>".PT_MISC.", gl.memo_, gl.person_id), 
					IF(ISNULL(supp.supp_name), debt.name, supp.supp_name)) as name,
				branch.br_name
		FROM ".TB_PREF."trans_tax_details taxrec
		LEFT JOIN ".TB_PREF."tax_types tt
			ON taxrec.tax_type_id=tt.id
		LEFT JOIN ".TB_PREF."gl_trans gl 
			ON taxrec.trans_type=gl.type AND taxrec.trans_no=gl.type_no AND gl.amount<>0 AND
			(tt.purchasing_gl_code=gl.account OR tt.sales_gl_code=gl.account)
		LEFT JOIN ".TB_PREF."supp_trans strans
			ON taxrec.trans_no=strans.trans_no AND taxrec.trans_type=strans.type
		LEFT JOIN ".TB_PREF."suppliers as supp ON strans.supplier_id=supp.supplier_id
		LEFT JOIN ".TB_PREF."debtor_trans dtrans
			ON taxrec.trans_no=dtrans.trans_no AND taxrec.trans_type=dtrans.type
		LEFT JOIN ".TB_PREF."debtors_master as debt ON dtrans.debtor_no=debt.debtor_no
		LEFT JOIN ".TB_PREF."cust_branch as branch ON dtrans.branch_code=branch.branch_code
		WHERE (taxrec.amount <> 0 OR taxrec.net_amount <> 0)
			AND !ISNULL(taxrec.reg_type)
			AND taxrec.tran_date >= '$fromdate'
			AND taxrec.tran_date <= '$todate'
		ORDER BY taxrec.trans_type, taxrec.tran_date, taxrec.trans_no, taxrec.ex_rate";

    return db_query($sql,"No transactions were returned");
}

function getTaxTypes()
{
	$sql = "SELECT * FROM ".TB_PREF."tax_types ORDER BY id";
    return db_query($sql,"No transactions were returned");
}

function getTaxInfo($id)
{
	$sql = "SELECT * FROM ".TB_PREF."tax_types WHERE id=$id";
    $result = db_query($sql,"No transactions were returned");
    return db_fetch($result);
}

//----------------------------------------------------------------------------------------------------

function print_tax_report()
{
	global $path_to_root, $systypes_array;

	$from = $_POST['PARAM_0'];
	$to = $_POST['PARAM_1'];
	$summaryOnly = $_POST['PARAM_2'];
	$comments = $_POST['PARAM_3'];
	$orientation = $_POST['PARAM_4'];
	$destination = $_POST['PARAM_5'];

	if ($destination)
		include_once($path_to_root . "/reporting/includes/excel_report.inc");
	else
		include_once($path_to_root . "/reporting/includes/pdf_report.inc");

	$orientation = ($orientation ? 'L' : 'P');
	$dec = user_price_dec();

	$rep = new FrontReport(_('Tax Report'), "TaxReport", user_pagesize(), 9, $orientation);
	if ($summaryOnly == 1)
		$summary = _('Summary Only');
	else
		$summary = _('Detailed Report');

	$res = getTaxTypes();

	$taxes[0] = array('in'=>0, 'out'=>0, 'taxin'=>0, 'taxout'=>0);
	while ($tax=db_fetch($res))
		$taxes[$tax['id']] = array('in'=>0, 'out'=>0, 'taxin'=>0, 'taxout'=>0);

	$params =   array( 	0 => $comments,
						1 => array('text' => _('Period'), 'from' => $from, 'to' => $to),
						2 => array('text' => _('Type'), 'from' => $summary, 'to' => ''));

	$cols = array(0, 80, 130, 180, 270, 350, 400, 430, 480, 485, 520);

	$headers = array(_('Trans Type'), _('Ref'), _('Date'), _('Name'), _('Branch Name'),
		_('Net'), _('Rate'), _('Tax'), '', _('Name'));
	$aligns = array('left', 'left', 'left', 'left', 'left', 'right', 'right', 'right', 'right','left');
    if ($orientation == 'L')
    	recalculate_cols($cols);

	$rep->Font();
	$rep->Info($params, $cols, $headers, $aligns);
	if (!$summaryOnly)
	{
		$rep->NewPage();
	}
	
	$totalnet = 0.0;
	$totaltax = 0.0;
	$transactions = getTaxTransactions($from, $to);

	while ($trans=db_fetch($transactions))
	{
		if (in_array($trans['trans_type'], array(ST_CUSTCREDIT,ST_SUPPINVOICE,ST_JOURNAL))) {
			$trans['net_amount'] *= -1;
			$trans['amount'] *= -1;
		}

		if (!$summaryOnly)
		{
			$rep->TextCol(0, 1, $systypes_array[$trans['trans_type']]);
			if ($trans['memo'] == '')
				$trans['memo'] = get_reference($trans['trans_type'], $trans['trans_no']);
			$rep->TextCol(1, 2,	$trans['memo']);
			$rep->DateCol(2, 3,	$trans['tran_date'], true);
			$rep->TextCol(3, 4,	$trans['name']);
			$rep->TextCol(4, 5,	$trans['br_name']);

			$rep->AmountCol(5, 6, $trans['net_amount'], $dec);
			$rep->AmountCol(6, 7, $trans['rate'], $dec);
			$rep->AmountCol(7, 8, $trans['amount'], $dec);

			$rep->TextCol(9, 10, $trans['taxname']);

			$rep->NewLine();

			if ($rep->row < $rep->bottomMargin + $rep->lineHeight)
			{
				$rep->Line($rep->row - 2);
				$rep->NewPage();
			}
		}
		$tax_type = $trans['tax_type_id'];
		if ($trans['trans_type']==ST_JOURNAL && $trans['reg_type']==TR_INPUT) {
			$taxes[$tax_type]['taxin'] += $trans['amount'];
			$taxes[$tax_type]['in'] += $trans['net_amount'];
		}
		elseif ($trans['trans_type']==ST_JOURNAL && $trans['reg_type']==TR_OUTPUT) {
			$taxes[$tax_type]['taxout'] += $trans['amount'];
			$taxes[$tax_type]['out'] += $trans['net_amount'];
		}
		elseif (in_array($trans['trans_type'], array(ST_BANKDEPOSIT,ST_SALESINVOICE,ST_CUSTCREDIT))) {
			$taxes[$tax_type]['taxout'] += $trans['amount'];
			$taxes[$tax_type]['out'] += $trans['net_amount'];
		} elseif ($trans['reg_type'] !== NULL) {
			$taxes[$tax_type]['taxin'] += $trans['amount'];
			$taxes[$tax_type]['in'] += $trans['net_amount'];
		}
		$totalnet += $trans['net_amount'];
		$totaltax += $trans['amount'];
	}
	
	// Summary
	$cols2 = array(0, 100, 180,	260, 340, 420, 500);
    if ($orientation == 'L')
    	recalculate_cols($cols2);

	$headers2 = array(_('Tax Rate'), _('Outputs'), _('Output Tax'),	_('Inputs'), _('Input Tax'), _('Net Tax'));

	$aligns2 = array('left', 'right', 'right', 'right',	'right', 'right', 'right');

	$rep->Info($params, $cols2, $headers2, $aligns2);

	$rep->headers = $headers2;
	$rep->aligns = $aligns2;
	$rep->NewPage();

	$taxtotal = 0;
	foreach( $taxes as $id=>$sum)
	{
		if ($id)
		{
			$tx = getTaxInfo($id);
			$rep->TextCol(0, 1, $tx['name'] . " " . number_format2($tx['rate'], $dec) . "%");
		} else {
			$rep->TextCol(0, 1, _('Exempt'));
		}
		$rep->AmountCol(1, 2, $sum['out'], $dec);
		$rep->AmountCol(2, 3, $sum['taxout'], $dec);
		$rep->AmountCol(3, 4, $sum['in'], $dec);
		$rep->AmountCol(4, 5, $sum['taxin'], $dec); 
		$rep->AmountCol(5, 6, $sum['taxout']+$sum['taxin'], $dec);
		$taxtotal += $sum['taxout']+$sum['taxin'];
		$rep->NewLine();
	}

	$rep->Font('bold');
	$rep->NewLine();
	$rep->Line($rep->row + $rep->lineHeight);
	$rep->TextCol(3, 5,	_("Total payable or refund"));
	$rep->AmountCol(5, 6, $taxtotal, $dec);
	$rep->Line($rep->row - 5);
	$rep->Font();
	$rep->NewLine();

	hook_tax_report_done();

	$rep->End();
}

