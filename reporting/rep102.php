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
$page_security = 'SA_CUSTPAYMREP';
// ----------------------------------------------------------------
// $ Revision:	2.0 $
// Creator:	Joe Hunt
// date_:	2005-05-19
// Title:	Aged Customer Balances
// ----------------------------------------------------------------
$path_to_root="..";

include_once($path_to_root . "/includes/session.inc");
include_once($path_to_root . "/includes/date_functions.inc");
include_once($path_to_root . "/includes/data_checks.inc");
include_once($path_to_root . "/gl/includes/gl_db.inc");

//----------------------------------------------------------------------------------------------------

print_aged_customer_analysis();

function get_invoices($customer_id, $to, $all=true)
{
	$todate = date2sql($to);
	$PastDueDays1 = get_company_pref('past_due_days');
	$PastDueDays2 = 2 * $PastDueDays1;

	// Revomed allocated from sql
	if ($all)
    	$value = "(ov_amount + ov_gst + ov_freight + ov_freight_tax + ov_discount)";
	else
    	$value = "(ov_amount + ov_gst + ov_freight + ov_freight_tax + ov_discount - alloc)";
	$sign = "IF(`type` IN(".implode(',',  array(ST_CUSTCREDIT,ST_CUSTPAYMENT,ST_BANKDEPOSIT,ST_JOURNAL))."), -1, 1)";
	$due = "IF (type=".ST_SALESINVOICE.", due_date, tran_date)";

	$sql = "SELECT type, reference, tran_date,
		$sign*$value as Balance,
		IF ((TO_DAYS('$todate') - TO_DAYS($due)) > 0,$sign*$value,0) AS Due,
		IF ((TO_DAYS('$todate') - TO_DAYS($due)) > $PastDueDays1,$sign*$value,0) AS Overdue1,
		IF ((TO_DAYS('$todate') - TO_DAYS($due)) > $PastDueDays2,$sign*$value,0) AS Overdue2

		FROM ".TB_PREF."debtor_trans trans

		WHERE type <> ".ST_CUSTDELIVERY."
			AND debtor_no = $customer_id 
			AND tran_date <= '$todate'
			AND ABS(ov_amount + ov_gst + ov_freight + ov_freight_tax + ov_discount) > " . FLOAT_COMP_DELTA;

	if (!$all)
		$sql .= "AND ABS(ov_amount + ov_gst + ov_freight + ov_freight_tax + ov_discount - alloc) > " . FLOAT_COMP_DELTA;
	$sql .= "ORDER BY tran_date";

	return db_query($sql, "The customer transactions could not be retrieved");
}

//----------------------------------------------------------------------------------------------------

function print_aged_customer_analysis()
{
    global $path_to_root, $systypes_array, $SysPrefs;

    $to = $_POST['PARAM_0'];
    $fromcust = $_POST['PARAM_1'];
    $currency = $_POST['PARAM_2'];
    $show_all = $_POST['PARAM_3'];
	$summaryOnly = $_POST['PARAM_4'];
    $no_zeros = $_POST['PARAM_5'];
    $graphics = $_POST['PARAM_6'];
    $comments = $_POST['PARAM_7'];
	$orientation = $_POST['PARAM_8'];
	$destination = $_POST['PARAM_9'];
	if ($destination)
		include_once($path_to_root . "/reporting/includes/excel_report.inc");
	else
		include_once($path_to_root . "/reporting/includes/pdf_report.inc");
	$orientation = ($orientation ? 'L' : 'P');
	if ($graphics)
	{
		include_once($path_to_root . "/reporting/includes/class.graphic.inc");
		$pg = new graph();
	}

	if ($fromcust == ALL_TEXT)
		$from = _('All');
	else
		$from = get_customer_name($fromcust);
    	$dec = user_price_dec();

	if ($summaryOnly == 1)
		$summary = _('Summary Only');
	else
		$summary = _('Detailed Report');
	if ($currency == ALL_TEXT)
	{
		$convert = true;
		$currency = _('Balances in Home Currency');
	}
	else
		$convert = false;

	if ($no_zeros) $nozeros = _('Yes');
	else $nozeros = _('No');
	if ($show_all) $show = _('Yes');
	else $show = _('No');

	$PastDueDays1 = get_company_pref('past_due_days');
	$PastDueDays2 = 2 * $PastDueDays1;
	$nowdue = "1-" . $PastDueDays1 . " " . _('Days');
	$pastdue1 = $PastDueDays1 + 1 . "-" . $PastDueDays2 . " " . _('Days');
	$pastdue2 = _('Over') . " " . $PastDueDays2 . " " . _('Days');

	$cols = array(0, 100, 130, 190,	250, 320, 385, 450,	515);
	$headers = array(_('Customer'),	'',	'',	_('Current'), $nowdue, $pastdue1, $pastdue2,
		_('Total Balance'));

	$aligns = array('left',	'left',	'left',	'right', 'right', 'right', 'right',	'right');

    	$params =   array( 	0 => $comments,
    				1 => array('text' => _('End Date'), 'from' => $to, 'to' => ''),
    				2 => array('text' => _('Customer'),	'from' => $from, 'to' => ''),
    				3 => array('text' => _('Currency'), 'from' => $currency, 'to' => ''),
                    		4 => array('text' => _('Type'),		'from' => $summary,'to' => ''),
                    5 => array('text' => _('Show Also Allocated'), 'from' => $show, 'to' => ''),		
				6 => array('text' => _('Suppress Zeros'), 'from' => $nozeros, 'to' => ''));

	if ($convert)
		$headers[2] = _('Currency');
    $rep = new FrontReport(_('Aged Customer Analysis'), "AgedCustomerAnalysis", user_pagesize(), 9, $orientation);
    if ($orientation == 'L')
    	recalculate_cols($cols);

    $rep->Font();
    $rep->Info($params, $cols, $headers, $aligns);
    $rep->NewPage();

	$total = array(0,0,0,0, 0);

	$sql = "SELECT debtor_no, name, curr_code FROM ".TB_PREF."debtors_master";
	if ($fromcust != ALL_TEXT)
		$sql .= " WHERE debtor_no=".db_escape($fromcust);
	$sql .= " ORDER BY name";
	$result = db_query($sql, "The customers could not be retrieved");

	while ($myrow=db_fetch($result))
	{
		if (!$convert && $currency != $myrow['curr_code'])
			continue;

		if ($convert) $rate = get_exchange_rate_from_home_currency($myrow['curr_code'], $to);
		else $rate = 1.0;
		$custrec = get_customer_details($myrow['debtor_no'], $to, $show_all);
		if (!$custrec)
			continue;
		$custrec['Balance'] *= $rate;
		$custrec['Due'] *= $rate;
		$custrec['Overdue1'] *= $rate;
		$custrec['Overdue2'] *= $rate;
		$str = array($custrec["Balance"] - $custrec["Due"],
			$custrec["Due"]-$custrec["Overdue1"],
			$custrec["Overdue1"]-$custrec["Overdue2"],
			$custrec["Overdue2"],
			$custrec["Balance"]);
		if ($no_zeros && floatcmp(array_sum($str), 0) == 0) continue;

		$rep->fontSize += 2;
		$rep->TextCol(0, 2, $myrow["name"]);
		if ($convert) $rep->TextCol(2, 3,	$myrow['curr_code']);
		$rep->fontSize -= 2;
		$total[0] += ($custrec["Balance"] - $custrec["Due"]);
		$total[1] += ($custrec["Due"]-$custrec["Overdue1"]);
		$total[2] += ($custrec["Overdue1"]-$custrec["Overdue2"]);
		$total[3] += $custrec["Overdue2"];
		$total[4] += $custrec["Balance"];
		for ($i = 0; $i < count($str); $i++)
			$rep->AmountCol($i + 3, $i + 4, $str[$i], $dec);
		$rep->NewLine(1, 2);
		if (!$summaryOnly)
		{
			$res = get_invoices($myrow['debtor_no'], $to, $show_all);
    		if (db_num_rows($res)==0)
				continue;
    		$rep->Line($rep->row + 4);
			while ($trans=db_fetch($res))
			{
				$rep->NewLine(1, 2);
        		$rep->TextCol(0, 1, $systypes_array[$trans['type']], -2);
				$rep->TextCol(1, 2,	$trans['reference'], -2);
				$rep->DateCol(2, 3, $trans['tran_date'], true, -2);

				if ($trans['type'] == ST_CUSTCREDIT || $trans['type'] == ST_CUSTPAYMENT || $trans['type'] == ST_BANKDEPOSIT)
				{
					$trans['Balance'] *= -1;
					$trans['Due'] *= -1;
					$trans['Overdue1'] *= -1;
					$trans['Overdue2'] *= -1;
				}

				foreach ($trans as $i => $value)
					$trans[$i] = (float)$trans[$i] * $rate;
				$str = array($trans["Balance"] - $trans["Due"],
					$trans["Due"]-$trans["Overdue1"],
					$trans["Overdue1"]-$trans["Overdue2"],
					$trans["Overdue2"],
					$trans["Balance"]);
				for ($i = 0; $i < count($str); $i++)
					$rep->AmountCol($i + 3, $i + 4, $str[$i], $dec);
			}
			$rep->Line($rep->row - 8);
			$rep->NewLine(2);
		}
	}
	if ($summaryOnly)
	{
    	$rep->Line($rep->row  + 4);
    	$rep->NewLine();
	}
	$rep->fontSize += 2;
	$rep->TextCol(0, 3, _('Grand Total'));
	$rep->fontSize -= 2;
	for ($i = 0; $i < count($total); $i++)
	{
		$rep->AmountCol($i + 3, $i + 4, $total[$i], $dec);
		if ($graphics && $i < count($total) - 1)
		{
			$pg->y[$i] = abs($total[$i]);
		}
	}
   	$rep->Line($rep->row - 8);
   	if ($graphics)
   	{
		$pg->x = array(_('Current'), $nowdue, $pastdue1, $pastdue2);
		$pg->title     = $rep->title;
		$pg->axis_x    = _("Days");
		$pg->axis_y    = _("Amount");
		$pg->graphic_1 = $to;
		$pg->type      = $graphics;
		$pg->skin      = $SysPrefs->graph_skin;
		$pg->built_in  = false;
		$pg->latin_notation = ($SysPrefs->decseps[user_dec_sep()] != ".");
		$filename = company_path(). "/pdf_files/". random_id().".png";
		$pg->display($filename, true);
		$w = $pg->width / 1.5;
		$h = $pg->height / 1.5;
		$x = ($rep->pageWidth - $w) / 2;
		$rep->NewLine(2);
		if ($rep->row - $h < $rep->bottomMargin)
			$rep->NewPage();
		$rep->AddImage($filename, $x, $rep->row - $h, $w, $h);
	}
	$rep->NewLine();
    $rep->End();
}
