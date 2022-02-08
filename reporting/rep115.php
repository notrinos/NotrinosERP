<?php
/**********************************************************************
	Copyright (C) FrontAccounting Team.
	Released under the terms of the GNU General Public License, GPL,
	as published by the Free Software Foundation, either version 3
	of the License, or (at your option) any later version.
	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
	See the License here <http://www.gnu.org/licenses/gpl-3.0.html>.
***********************************************************************/
$page_security = 'SA_CUSTPAYMREP';
$path_to_root = '..';

include_once($path_to_root.'/includes/session.inc');
include_once($path_to_root.'/includes/date_functions.inc');
include_once($path_to_root.'/includes/data_checks.inc');
include_once($path_to_root.'/gl/includes/gl_db.inc');
include_once($path_to_root.'/sales/includes/db/customers_db.inc');

//----------------------------------------------------------------------------------------------------

print_customer_balances();

function get_open_balance($debtorno, $to) {
	if ($to)
		$to = date2sql($to);

	$sql = "SELECT SUM(IF(t.type = ".ST_SALESINVOICE." OR (t.type IN (".ST_JOURNAL." , ".ST_BANKPAYMENT.") AND t.ov_amount>0),
		-abs(t.ov_amount + t.ov_gst + t.ov_freight + t.ov_freight_tax + t.ov_discount), 0)) AS charges,";
	$sql .= "SUM(IF(t.type != ".ST_SALESINVOICE." AND NOT(t.type IN (".ST_JOURNAL." , ".ST_BANKPAYMENT.") AND t.ov_amount>0),
		abs(t.ov_amount + t.ov_gst + t.ov_freight + t.ov_freight_tax + t.ov_discount) * -1, 0)) AS credits,";
	$sql .= "SUM(IF(t.type != ".ST_SALESINVOICE." AND NOT(t.type IN (".ST_JOURNAL." , ".ST_BANKPAYMENT.")), 
		t.alloc * -1, t.alloc)) AS Allocated,";
	$sql .= "SUM(IF(t.type = ".ST_SALESINVOICE.", 1, -1) *
		(abs(t.ov_amount + t.ov_gst + t.ov_freight + t.ov_freight_tax + t.ov_discount) - abs(t.alloc))) AS OutStanding
		FROM ".TB_PREF."debtor_trans t
		WHERE t.debtor_no = ".db_escape($debtorno)
		." AND t.type <> ".ST_CUSTDELIVERY;
	if ($to)
		$sql .= " AND t.tran_date < '$to'";
	$sql .= " GROUP BY debtor_no";

	$result = db_query($sql, 'No transactions were returned');
	return db_fetch($result);
}

function get_transactions($debtorno, $from, $to, $only_rec) {
	$from = date2sql($from);
	$to = date2sql($to);

	$allocated_from =
		"(SELECT trans_type_from as trans_type, trans_no_from as trans_no, date_alloc, sum(amt) amount
		FROM ".TB_PREF."cust_allocations alloc
		WHERE person_id=".db_escape($debtorno)."
		AND date_alloc <= '$to'
		GROUP BY trans_type_from, trans_no_from) alloc_from";
	$allocated_to =
		"(SELECT trans_type_to as trans_type, trans_no_to as trans_no, date_alloc, sum(amt) amount
		FROM ".TB_PREF."cust_allocations alloc
		WHERE person_id=".db_escape($debtorno)."
		AND date_alloc <= '$to'
		GROUP BY trans_type_to, trans_no_to) alloc_to";

	$sql = "SELECT trans.*, comments.memo_,
		(trans.ov_amount + trans.ov_gst + trans.ov_freight + trans.ov_freight_tax + trans.ov_discount) AS TotalAmount,
		IFNULL(alloc_from.amount, alloc_to.amount) AS Allocated,
		((trans.type = ".ST_SALESINVOICE.")    AND trans.due_date < '$to') AS OverDue
		FROM ".TB_PREF."debtor_trans trans
		LEFT JOIN ".TB_PREF."voided voided ON trans.type=voided.type AND trans.trans_no=voided.id
		LEFT JOIN ".TB_PREF."comments comments ON trans.type=comments.type AND trans.trans_no=comments.id
		LEFT JOIN $allocated_from ON alloc_from.trans_type = trans.type AND alloc_from.trans_no = trans.trans_no
		LEFT JOIN $allocated_to ON alloc_to.trans_type = trans.type AND alloc_to.trans_no = trans.trans_no
		WHERE trans.tran_date >= '$from'
		AND trans.tran_date <= '$to'
		AND trans.debtor_no = ".db_escape($debtorno);
	$sql .= " AND trans.type <> ".ST_CUSTDELIVERY;
	$sql .= " AND ISNULL(voided.id)
		ORDER BY trans.tran_date ";
	
	return db_query($sql, 'No transactions were returned');
}

function get_customer_reference ($order_number) {

	$sql = "SELECT customer_ref FROM ".TB_PREF."sales_orders WHERE order_no = ".db_escape($order_number)." AND trans_type=".ST_SALESORDER;

	$result = db_query($sql, 'No Transcation were returned');

	$val = db_fetch($result);

	return $val['customer_ref'];
}

//----------------------------------------------------------------------------------------------------

function print_customer_balances() {
	global $path_to_root, $systypes_array;

	$from = $_POST['PARAM_0'];
	$to = $_POST['PARAM_1'];
	$fromcust = $_POST['PARAM_2'];
	$area = $_POST['PARAM_3'];
	$folk = $_POST['PARAM_4'];
	$currency = $_POST['PARAM_5'];
	$no_zeros = $_POST['PARAM_6'];
	$comments = $_POST['PARAM_7'];
	$orientation = $_POST['PARAM_8'];
	$destination = $_POST['PARAM_9'];
	if ($destination)
		include_once($path_to_root.'/reporting/includes/excel_report.inc');
	else
		include_once($path_to_root.'/reporting/includes/pdf_report.inc');

	$orientation = ($orientation ? 'L' : 'P');
	$cust = $fromcust == ALL_TEXT ? _('All') : get_customer_name($fromcust);
	$dec = user_price_dec();

	if ($area == ALL_NUMERIC)
		$area = 0;

	$sarea = $area == 0 ? _('All Areas') : get_area_name($area);

	if ($folk == ALL_NUMERIC)
		$folk = 0;

	$salesfolk = $folk == 0 ? _('All Sales Man') : get_salesman_name($folk);

	if ($currency == ALL_TEXT) {
		$convert = true;
		$currency = _('Balances in Home Currency');
	}
	else
		$convert = false;

	$nozeros = $no_zeros ? _('Yes') : _('No');

	$cols = array(0, 180, 260, 340, 420, 515);

	$headers = array(_('Name'), _('Open Balance'), _('Debit'), _('Credit'), _('Balance'));

	$aligns = array('left', 'right', 'right', 'right', 'right');

	$params = array(0 => $comments,
					1 => array('text' => _('Period'), 'from' => $from,   'to' => $to),
					2 => array('text' => _('Customer'), 'from' => $cust, 'to' => ''),
					3 => array('text' => _('Sales Areas'), 'from' => $sarea, 		'to' => ''),
					4 => array('text' => _('Sales Folk'), 'from' => $salesfolk, 	'to' => ''),
					5 => array('text' => _('Currency'), 'from' => $currency, 'to' => ''),
					6 => array('text' => _('Suppress Zeros'), 'from' => $nozeros, 'to' => ''));

	$rep = new FrontReport(_('Customer Trial Balance'), 'CustomerTB', user_pagesize(), 9, $orientation);
	if ($orientation == 'L')
		recalculate_cols($cols);
	$rep->Font();
	$rep->Info($params, $cols, $headers, $aligns);
	$rep->NewPage();

	$grandtotal = array(0, 0, 0, 0);

	$sql = "SELECT ".TB_PREF."debtors_master.debtor_no, name, curr_code FROM ".TB_PREF."debtors_master
		INNER JOIN ".TB_PREF."cust_branch
		ON ".TB_PREF."debtors_master.debtor_no=".TB_PREF."cust_branch.debtor_no
		INNER JOIN ".TB_PREF."areas
		ON ".TB_PREF."cust_branch.area = ".TB_PREF."areas.area_code
		INNER JOIN ".TB_PREF."salesman
		ON ".TB_PREF."cust_branch.salesman=".TB_PREF."salesman.salesman_code";
	if ($fromcust != ALL_TEXT )
		$sql .= " WHERE ".TB_PREF."debtors_master.debtor_no=".db_escape($fromcust);
	elseif ($area != 0) {
		if ($folk != 0)
			$sql .= " WHERE ".TB_PREF."salesman.salesman_code=".db_escape($folk)."
				AND ".TB_PREF."areas.area_code=".db_escape($area);
		else
			$sql .= " WHERE ".TB_PREF."areas.area_code=".db_escape($area);
	}
	elseif ($folk != 0 )
		$sql .= " WHERE ".TB_PREF."salesman.salesman_code=".db_escape($folk);

	$sql .= " GROUP BY ".TB_PREF."debtors_master.debtor_no ORDER BY name";

	$result = db_query($sql, 'The customers could not be retrieved');

	$tot_cur_cr = 0;
	$tot_cur_db = 0;
	while ($myrow = db_fetch($result)) {
		if (!$convert && $currency != $myrow['curr_code'])
			continue;

		$accumulate = 0;
		$rate = $convert ? get_exchange_rate_from_home_currency($myrow['curr_code'], Today()) : 1;
		$bal = get_open_balance($myrow['debtor_no'], $from, $convert);
		$init = array();
		$bal['charges'] = isset($bal['charges']) ? $bal['charges'] : 0;
		$bal['credits'] = isset($bal['credits']) ? $bal['credits'] : 0;
		$bal['Allocated'] = isset($bal['Allocated']) ? $bal['Allocated'] : 0;
		$bal['OutStanding'] = isset($bal['OutStanding']) ? $bal['OutStanding'] : 0;
		$init[0] = round2(abs($bal['charges'] * $rate), $dec);
		$init[1] = round2(Abs($bal['credits'] * $rate), $dec);
		$init[2] = round2($bal['Allocated'] * $rate, $dec);
		$init[3] = $init[0] - $init[1];
		$accumulate += $init[3];

		$res = get_transactions($myrow['debtor_no'], $from, $to, false);

		$total = array(0, 0, 0, 0);
		for ($i = 0; $i < 4; $i++) {
			$total[$i] += $init[$i];
			$grandtotal[$i] += $init[$i];
		}

		if (db_num_rows($res) == 0 && !$no_zeros) {
			$rep->TextCol(0, 1, $myrow['name']);
			$rep->AmountCol(1, 2, $init[3], $dec);
			$rep->AmountCol(4, 5, $init[3], $dec);
			$rep->NewLine(1);
			continue;
		}
		$curr_cr = 0;
		$curr_db = 0;
		while ($trans = db_fetch($res)) {
			$item[0] = $item[1] = 0.0;
			if ($trans['type'] == ST_CUSTCREDIT || $trans['type'] == ST_CUSTPAYMENT || $trans['type'] == ST_BANKDEPOSIT)
				$trans['TotalAmount'] *= -1;
			if ($trans['TotalAmount'] > 0.0) {
				$item[0] = round2(abs($trans['TotalAmount']) * $rate, $dec);
				$accumulate += $item[0];
				$curr_db += $item[0];
				$tot_cur_db += $item[0];
				$item[2] = round2($trans['Allocated'] * $rate, $dec);
			}
			else {
				$item[1] = round2(Abs($trans['TotalAmount']) * $rate, $dec);
				$accumulate -= $item[1];
				$curr_cr += $item[1];
				$tot_cur_cr +=$item[1];
				$item[2] = round2($trans['Allocated'] * $rate, $dec) * -1;
			}

			if ($trans['type'] == ST_JOURNAL || $trans['type'] == ST_SALESINVOICE || $trans['type'] == ST_BANKPAYMENT)
				$item[3] = $item[0] - $item[2];
			else
				$item[3] = -$item[1] - $item[2];

			for ($i = 0; $i < 4; $i++) {
				$total[$i] += $item[$i];
				$grandtotal[$i] += $item[$i];
			}
			$total[3] = $total[0] - $total[1];
		}

		if ($no_zeros && $total[3] == 0.0 && $curr_db == 0.0 && $curr_cr == 0.0)
			continue;

		$rep->TextCol(0, 1, $myrow['name']);
		$rep->AmountCol(1, 2, $total[3] + $curr_cr - $curr_db, $dec);
		$rep->AmountCol(2, 3, $curr_db, $dec);
		$rep->AmountCol(3, 4, $curr_cr, $dec);
		$rep->AmountCol(4, 5, $total[3], $dec);
		$rep->NewLine(1);
	}
	$rep->Line($rep->row + 4);
	$rep->NewLine();
	$rep->fontSize += 2;
	$rep->TextCol(0, 1, _('Grand Total'));
	$rep->fontSize -= 2;
	$grandtotal[3] = $grandtotal[0] - $grandtotal[1];

	$rep->AmountCol(1, 2, $grandtotal[3] - $tot_cur_db + $tot_cur_cr, $dec);
	$rep->AmountCol(2, 3, $tot_cur_db, $dec);
	$rep->AmountCol(3, 4, $tot_cur_cr, $dec);
	$rep->AmountCol(4, 5, $grandtotal[3], $dec);
	$rep->Line($rep->row - 6, 1);
	$rep->NewLine();
	$rep->End();
}
