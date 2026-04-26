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
$page_security = 'SA_SALESMANREP';
$path_to_root = '..';

include_once($path_to_root.'/includes/session.inc');
include_once($path_to_root.'/includes/date_functions.inc');
include_once($path_to_root.'/includes/data_checks.inc');
include_once($path_to_root.'/sales/includes/sales_db.inc');
include_once($path_to_root.'/inventory/includes/db/items_category_db.inc');
include_once($path_to_root.'/sales/includes/db/sales_commission_db.inc');

//----------------------------------------------------------------------------------------------------

print_salesman_list();

//----------------------------------------------------------------------------------------------------

function get_salesman_trans($from, $to) {

	$sql = "SELECT DISTINCT trans.*,
			ov_amount+ov_discount AS InvoiceTotal,
			cust.name AS DebtorName,
			cust.curr_code,
			branch.br_name,
			sorder.customer_ref,
			salesman.*
		FROM ".TB_PREF."debtor_trans trans,
			 ".TB_PREF."debtors_master cust,
			 ".TB_PREF."sales_orders sorder,
			 ".TB_PREF."cust_branch branch,
			".TB_PREF."salesman salesman
		WHERE sorder.order_no=trans.order_
			AND sorder.branch_code=branch.branch_code
			AND sorder.trans_type = ".ST_SALESORDER."
			AND sorder.salesman_code=salesman.salesman_code
			AND trans.debtor_no=cust.debtor_no
			AND (trans.type=".ST_SALESINVOICE." OR trans.type=".ST_CUSTCREDIT.")
			AND trans.tran_date>='".date2sql($from)."'
			AND trans.tran_date<='".date2sql($to)."'
		ORDER BY salesman.salesman_code, trans.tran_date";

	return db_query($sql, 'Error getting salesman transaction details');
}

//----------------------------------------------------------------------------------------------------

function print_salesman_list() {
	global $path_to_root;

	// Phase 6: when advanced commissions are enabled, use commission entries
	if (get_company_pref('use_advanced_commissions')) {
		print_salesman_list_advanced();
		return;
	}

	$from = $_POST['PARAM_0'];
	$to = $_POST['PARAM_1'];
	$summary = $_POST['PARAM_2'];
	$comments = $_POST['PARAM_3'];
	$orientation = $_POST['PARAM_4'];
	$destination = $_POST['PARAM_5'];
	if ($destination)
		include_once($path_to_root.'/reporting/includes/excel_report.inc');
	else
		include_once($path_to_root.'/reporting/includes/pdf_report.inc');
	$orientation = ($orientation ? 'L' : 'P');

	$sum = $summary == 0 ? _('No') : _('Yes');

	$dec = user_price_dec();

	$cols = array(0, 60, 150, 220, 325,	385, 450, 515);

	$headers = array(_('Invoice'), _('Customer'), _('Branch'), _('Customer Ref'), _('Inv Date'), _('Total'), _('Provision'));

	$aligns = array('left',	'left',	'left', 'left', 'left', 'right', 'right');

	$headers2 = array(_('Salesman'), ' ', _('Phone'), _('Email'), _('Provision'), _('Break Pt.'), _('Provision').' 2');

	$params = array(0 => $comments,
					1 => array(  'text' => _('Period'), 'from' => $from, 'to' => $to),
					2 => array(  'text' => _('Summary Only'),'from' => $sum,'to' => ''));

	$aligns2 = $aligns;

	$rep = new FrontReport(_('Salesman Listing'), 'SalesmanListing', user_pagesize(), 9, $orientation);
	if ($orientation == 'L')
		recalculate_cols($cols);
	$cols2 = $cols;
	$rep->Font();
	$rep->Info($params, $cols, $headers, $aligns, $cols2, $headers2, $aligns2);

	$rep->NewPage();
	$salesman = 0;
	$subtotal = 0;
	$total = 0;
	$subprov = 0;
	$provtotal = 0;

	$result = get_salesman_trans($from, $to);

	while ($myrow=db_fetch($result)) {
		$rep->NewLine(0, 2, false, $salesman);
		if ($salesman != $myrow['salesman_code']) {
			if ($salesman != 0) {
				$rep->Line($rep->row - 8);
				$rep->NewLine(2);
				$rep->TextCol(0, 3, _('Total'));
				$rep->AmountCol(5, 6, $subtotal, $dec);
				$rep->AmountCol(6, 7, $subprov, $dec);
				$rep->Line($rep->row  - 4);
				$rep->NewLine(2);
			}
			$rep->Font('bold');
			$rep->TextCol(0, 2,	$myrow['salesman_code'].' - '.$myrow['salesman_name']);
			$rep->Font();
			$rep->TextCol(2, 3,	$myrow['salesman_phone']);
			$rep->TextCol(3, 4,	$myrow['salesman_email']);
			$rep->TextCol(4, 5,	number_format2($myrow['provision'], user_percent_dec()).' %');
			$rep->AmountCol(5, 6, $myrow['break_pt'], $dec);
			$rep->TextCol(6, 7,	number_format2($myrow['provision2'], user_percent_dec()).' %');
			$rep->NewLine(2);
			$salesman = $myrow['salesman_code'];
			$total += $subtotal;
			$provtotal += $subprov;
			$subtotal = 0;
			$subprov = 0;
		}
		$rate = $myrow['rate'];
		$amt = $myrow['InvoiceTotal'] * $rate;
		if ($myrow['provision2'] == 0)
			$prov = $myrow['provision'] * $amt / 100;
		else {
			$amt1 = min($amt, max(0, $myrow['break_pt']-$subtotal));
			$amt2 = $amt - $amt1;

			$prov = $amt1*$myrow['provision']/100 + $amt2*$myrow['provision2']/100;
		}
		if (!$summary) {
			$rep->TextCol(0, 1,	$myrow['trans_no']);
			$rep->TextCol(1, 2,	$myrow['DebtorName']);
			$rep->TextCol(2, 3,	$myrow['br_name']);
			$rep->TextCol(3, 4,	$myrow['customer_ref']);
			$rep->DateCol(4, 5,	$myrow['tran_date'], true);
			$rep->AmountCol(5, 6, $amt, $dec);
			$rep->AmountCol(6, 7, $prov, $dec);
			$rep->NewLine();
		}
		$subtotal += $amt;
		$subprov += $prov;
	}
	if ($salesman != 0) {
		$rep->Line($rep->row - 4);
		$rep->NewLine(2);
		$rep->TextCol(0, 3, _('Total'));
		$rep->AmountCol(5, 6, $subtotal, $dec);
		$rep->AmountCol(6, 7, $subprov, $dec);
		$rep->Line($rep->row  - 4);
		$rep->NewLine(2);
		$total += $subtotal;
		$provtotal += $subprov;
	}
	$rep->fontSize += 2;
	$rep->TextCol(0, 3, _('Grand Total'));
	$rep->fontSize -= 2;
	$rep->AmountCol(5, 6, $total, $dec);
	$rep->AmountCol(6, 7, $provtotal, $dec);
	$rep->Line($rep->row  - 4);
	$rep->NewLine();
	$rep->End();
}

// -------------------------------------------------------------------------
// Phase 6: Advanced commission report — reads from 0_sales_commission_entries
// -------------------------------------------------------------------------

/**
 * Print salesman commission report using the advanced commission engine.
 * Replaces the legacy provision-based calculation in rep106.php when
 * use_advanced_commissions = 1.
 */
function print_salesman_list_advanced() {
	global $path_to_root;

	$from    = $_POST['PARAM_0'];
	$to      = $_POST['PARAM_1'];
	$summary = $_POST['PARAM_2'];
	$comments    = $_POST['PARAM_3'];
	$orientation = $_POST['PARAM_4'];
	$destination = $_POST['PARAM_5'];

	if ($destination)
		include_once($path_to_root.'/reporting/includes/excel_report.inc');
	else
		include_once($path_to_root.'/reporting/includes/pdf_report.inc');
	$orientation = ($orientation ? 'L' : 'P');

	$sum = $summary == 0 ? _('No') : _('Yes');
	$dec = user_price_dec();

	$cols    = array(0, 60, 200, 290, 360, 430, 515);
	$headers = array(_('Trans#'), _('Customer'), _('Plan'), _('Date'), _('Base Amt'), _('Rate %'), _('Commission'));
	$aligns  = array('left', 'left', 'left', 'left', 'right', 'right', 'right');

	$headers2 = array(_('Salesman'), '', '', '', '', '', '');
	$aligns2  = $aligns;

	$params = array(
		0 => $comments,
		1 => array('text' => _('Period'), 'from' => $from, 'to' => $to),
		2 => array('text' => _('Summary Only'), 'from' => $sum, 'to' => ''),
	);

	$rep = new FrontReport(_('Salesman Commission Report'), 'SalesmanCommissions',
		user_pagesize(), 9, $orientation);
	if ($orientation == 'L')
		recalculate_cols($cols);
	$rep->Font();
	$rep->Info($params, $cols, $headers, $aligns, $cols, $headers2, $aligns2);
	$rep->NewPage();

	$result = get_commission_entries(0, '', date2sql($from), date2sql($to));

	$cur_salesman = 0;
	$sub_base     = 0;
	$sub_comm     = 0;
	$tot_base     = 0;
	$tot_comm     = 0;

	while ($row = db_fetch($result)) {
		$rep->NewLine(0, 2, false, $cur_salesman);

		if ($cur_salesman != $row['salesman_id']) {
			if ($cur_salesman != 0) {
				$rep->Line($rep->row - 8);
				$rep->NewLine(2);
				$rep->TextCol(0, 4, _('Sub-Total'));
				$rep->AmountCol(4, 5, $sub_base, $dec);
				$rep->TextCol(5, 6, '');
				$rep->AmountCol(6, 7, $sub_comm, $dec);
				$rep->Line($rep->row - 4);
				$rep->NewLine(2);
				$tot_base += $sub_base;
				$tot_comm += $sub_comm;
				$sub_base = $sub_comm = 0;
			}
			$rep->Font('bold');
			$rep->TextCol(0, 3, $row['salesman_id'].' - '.$row['salesman_name']);
			$rep->Font();
			$rep->NewLine(2);
			$cur_salesman = $row['salesman_id'];
		}

		if (!$summary) {
			$type_label = $row['trans_type'] == ST_SALESINVOICE ? _('Inv') : _('Cr');
			$rep->TextCol(0, 1, $type_label.' #'.$row['trans_no']);
			$rep->TextCol(1, 2, $row['customer_name']);
			$rep->TextCol(2, 3, $row['plan_name'] ?? '');
			$rep->DateCol(3, 4, $row['trans_date'], true);
			$rep->AmountCol(4, 5, $row['base_amount'], $dec);
			$rep->TextCol(5, 6, number_format2($row['commission_rate'], user_percent_dec()).' %');
			$rep->AmountCol(6, 7, $row['commission_amount'], $dec);
			$rep->NewLine();
		}

		$sub_base += $row['base_amount'];
		$sub_comm += $row['commission_amount'];
	}

	if ($cur_salesman != 0) {
		$rep->Line($rep->row - 4);
		$rep->NewLine(2);
		$rep->TextCol(0, 4, _('Sub-Total'));
		$rep->AmountCol(4, 5, $sub_base, $dec);
		$rep->TextCol(5, 6, '');
		$rep->AmountCol(6, 7, $sub_comm, $dec);
		$rep->Line($rep->row - 4);
		$rep->NewLine(2);
		$tot_base += $sub_base;
		$tot_comm += $sub_comm;
	}

	$rep->fontSize += 2;
	$rep->TextCol(0, 4, _('Grand Total'));
	$rep->fontSize -= 2;
	$rep->AmountCol(4, 5, $tot_base, $dec);
	$rep->TextCol(5, 6, '');
	$rep->AmountCol(6, 7, $tot_comm, $dec);
	$rep->Line($rep->row - 4);
	$rep->NewLine();
	$rep->End();
}
