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
$page_security = 'SA_SUPPLIERANALYTIC';

// ----------------------------------------------------------------
// $ Revision:    2.0 $
// Creator:    @boxygen, Joe Hunt
// date_:    2018-12-20
// Title:    Supplier Trial Balances
// ----------------------------------------------------------------
$path_to_root="..";

include_once($path_to_root . "/includes/session.inc");
include_once($path_to_root . "/includes/date_functions.inc");
include_once($path_to_root . "/includes/data_checks.inc");
include_once($path_to_root . "/gl/includes/gl_db.inc");

//----------------------------------------------------------------------------------------------------

print_supplier_balances();

function get_open_balance($supplier_id, $to)
{
    if($to)
        $to = date2sql($to);

    $sql = "SELECT SUM(IF(t.type = ".ST_SUPPINVOICE." OR (t.type IN (".ST_JOURNAL." , ".ST_BANKDEPOSIT.") AND t.ov_amount>0),
        -abs(t.ov_amount + t.ov_gst + t.ov_discount), 0)) AS charges,";

 	$sql .= "SUM(IF(t.type != ".ST_SUPPINVOICE." AND NOT(t.type IN (".ST_JOURNAL." , ".ST_BANKDEPOSIT.") AND t.ov_amount>0),
        abs(t.ov_amount + t.ov_gst + t.ov_discount) * -1, 0)) AS credits,";

    $sql .= "SUM(IF(t.type != ".ST_SUPPINVOICE." AND NOT(t.type IN (".ST_JOURNAL." , ".ST_BANKDEPOSIT.")), 
    	t.alloc * -1, t.alloc)) AS Allocated,";

    $sql .= "SUM(IF(t.type = ".ST_SUPPINVOICE.", 1, -1) *
        (abs(t.ov_amount + t.ov_gst + t.ov_discount) - abs(t.alloc))) AS OutStanding
        FROM ".TB_PREF."supp_trans t
        WHERE t.supplier_id = ".db_escape($supplier_id);
    if ($to)
        $sql .= " AND t.tran_date < '$to'";
    $sql .= " GROUP BY supplier_id";

    $result = db_query($sql,"No transactions were returned");
    return db_fetch($result);
}

function getTransactions($supplier_id, $from, $to)
{
    $from = date2sql($from);
    $to = date2sql($to);
	//memo added by faisal
    $sql = "SELECT ".TB_PREF."supp_trans.*, comments.memo_,
        (".TB_PREF."supp_trans.ov_amount + ".TB_PREF."supp_trans.ov_gst + ".TB_PREF."supp_trans.ov_discount)
        AS TotalAmount, ".TB_PREF."supp_trans.alloc AS Allocated,
        ((".TB_PREF."supp_trans.type = ".ST_SUPPINVOICE.")
        AND ".TB_PREF."supp_trans.due_date < '$to') AS OverDue
        FROM ".TB_PREF."supp_trans
        LEFT JOIN ".TB_PREF."comments comments ON ".TB_PREF."supp_trans.type=comments.type AND ".TB_PREF."supp_trans.trans_no=comments.id
        WHERE ".TB_PREF."supp_trans.tran_date >= '$from' AND ".TB_PREF."supp_trans.tran_date <= '$to'
        AND ".TB_PREF."supp_trans.supplier_id = '$supplier_id' AND ".TB_PREF."supp_trans.ov_amount!=0
        ORDER BY ".TB_PREF."supp_trans.tran_date";

    return db_query($sql,"No transactions were returned");
}

//----------------------------------------------------------------------------------------------------

function print_supplier_balances()
{
    global $path_to_root, $systypes_array;

    $from = $_POST['PARAM_0'];
    $to = $_POST['PARAM_1'];
    $fromsupp = $_POST['PARAM_2'];
    $currency = $_POST['PARAM_3'];
    $no_zeros = $_POST['PARAM_4'];
    $comments = $_POST['PARAM_5'];
    $orientation = $_POST['PARAM_6'];
    $destination = $_POST['PARAM_7'];
    if ($destination)
        include_once($path_to_root . "/reporting/includes/excel_report.inc");
    else
        include_once($path_to_root . "/reporting/includes/pdf_report.inc");

    $orientation = ($orientation ? 'L' : 'P');
    if ($fromsupp == ALL_TEXT)
        $supp = _('All');
    else
        $supp = get_supplier_name($fromsupp);
    $dec = user_price_dec();

    if ($currency == ALL_TEXT)
    {
        $convert = true;
        $currency = _('Balances in Home currency');
    }
    else
        $convert = false;

    if ($no_zeros) $nozeros = _('Yes');
    else $nozeros = _('No');

    $cols = array(0, 100, 130, 190, 250, 320, 385, 450, 515);

    $headers = array(_('Name'), '', '', _('Open Balance'), _('Debit'),
        _('Credit'), '', _('Balance'));

    $aligns = array('left', 'left', 'left', 'right', 'right', 'right', 'right', 'right');

    $params =   array( 	0 => $comments,
                		1 => array('text' => _('Period'), 'from' => $from, 'to' => $to),
                		2 => array('text' => _('Supplier'), 'from' => $supp, 'to' => ''),
                		3 => array(  'text' => _('Currency'),'from' => $currency, 'to' => ''),
            			4 => array('text' => _('Suppress Zeros'), 'from' => $nozeros, 'to' => ''));

    $rep = new FrontReport(_('Supplier Trial Balance'), "SupplierTB", user_pagesize(), 9, $orientation);
    if ($orientation == 'L')
        recalculate_cols($cols);

    $rep->Font();
    $rep->Info($params, $cols, $headers, $aligns);
    $rep->NewPage();

    $total = array();
    $grandtotal = array(0,0,0,0);

    $sql = "SELECT supplier_id, supp_name AS name, curr_code FROM ".TB_PREF."suppliers";
    if ($fromsupp != ALL_TEXT)
        $sql .= " WHERE supplier_id=".db_escape($fromsupp);
    $sql .= " ORDER BY supp_name";
    $result = db_query($sql, "The customers could not be retrieved");

	$tot_cur_cr = $tot_cur_db = 0;
    while ($myrow=db_fetch($result))
    {
        if (!$convert && $currency != $myrow['curr_code'])
            continue;
        $accumulate = 0;
        $rate = $convert ? get_exchange_rate_from_home_currency($myrow['curr_code'], Today()) : 1;
        $bal = get_open_balance($myrow['supplier_id'], $from);
        $init[0] = $init[1] = 0.0;
        $init[0] = round2(abs($bal['charges']*$rate), $dec);
        $init[1] = round2(Abs($bal['credits']*$rate), $dec);
        $init[2] = round2($bal['Allocated']*$rate, $dec);

        $init[3] = $init[1] - $init[0];
        $accumulate += $init[3];

        $res = getTransactions($myrow['supplier_id'], $from, $to);

        $total = array(0,0,0,0);
        for ($i = 0; $i < 4; $i++)
        {
            $total[$i] += $init[$i];
            $grandtotal[$i] += $init[$i];
        }

        if (db_num_rows($res) == 0 && !$no_zeros) 
        {
            $rep->TextCol(0, 2, $myrow['name']);
            $rep->AmountCol(3, 4, $init[3], $dec);
            $rep->AmountCol(7, 8, $init[3], $dec);
            //$rep->Line($rep->row  - 2);
        	$rep->NewLine();

            continue;
        }
        $curr_db = $curr_cr = 0;
        while ($trans=db_fetch($res))
        {
            //if ($no_zeros && floatcmp(abs($trans['TotalAmount']), $trans['Allocated']) == 0) continue;
            $item[0] = $item[1] = 0.0;
            if ($trans['TotalAmount'] > 0.0)
            {
                $item[0] = round2(abs($trans['TotalAmount']) * $rate, $dec);
                $curr_cr += $item[0];
                $tot_cur_cr += $item[0];

                $accumulate -= $item[0];
            }
            else
            {
                $item[1] = round2(abs($trans['TotalAmount']) * $rate, $dec);
                $curr_db += $item[1];
                $tot_cur_db += $item[1];
                $accumulate += $item[1];
            }
            $item[2] = round2($trans['Allocated'] * $rate, $dec);
            if ($trans['TotalAmount'] > 0.0)
                $item[3] = $item[2] - $item[0];
            else
                $item[3] = ($item[2] - $item[1]) * -1;

            for ($i = 0; $i < 4; $i++)
            {
                $total[$i] += $item[$i];
                $grandtotal[$i] += $item[$i];
            }
            $total[3] = $total[1] - $total[0];
        }
		if ($no_zeros && $total[3] == 0.0 && $curr_db == 0.0 && $curr_cr == 0.0) continue;
        $rep->TextCol(0, 2, $myrow['name']);
        $rep->AmountCol(3, 4, $total[3] + $curr_cr - $curr_db, $dec);
        $rep->AmountCol(4, 5, $curr_db, $dec);
        $rep->AmountCol(5, 6, $curr_cr, $dec);
        $rep->AmountCol(7, 8, $total[3], $dec);
        for ($i = 2; $i < 4; $i++)
        {
            $total[$i] = 0.0;
        }
        //$rep->Line($rep->row  - 2);
        $rep->NewLine();
    }
    $rep->Line($rep->row + 4); // added line by Joe
    $rep->NewLine();
    $rep->fontSize += 2;
    $rep->TextCol(0, 3,    _('Grand Total'));
    $rep->fontSize -= 2;

    $grandtotal[3] = $grandtotal[1] - $grandtotal[0];

	$rep->AmountCol(3, 4,$grandtotal[3] - $tot_cur_db + $tot_cur_cr, $dec);

	$rep->AmountCol(4, 5,$tot_cur_db, $dec);
	$rep->AmountCol(5, 6,$tot_cur_cr, $dec);

    $rep->AmountCol(7, 8,$grandtotal[3], $dec);
    $rep->Line($rep->row - 6, 1);
    $rep->End();
}
