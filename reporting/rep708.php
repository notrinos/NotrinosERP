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
$page_security = 'SA_GLANALYTIC';
// ----------------------------------------------------------------
// $ Revision:	2.0 $
// Creator:	Joe Hunt
// date_:	2005-05-19
// Title:	Trial Balance
// ----------------------------------------------------------------
$path_to_root="..";

include_once($path_to_root . "/includes/session.inc");
include_once($path_to_root . "/admin/db/fiscalyears_db.inc");
include_once($path_to_root . "/includes/ui.inc");
include_once($path_to_root . "/includes/date_functions.inc");
include_once($path_to_root . "/includes/data_checks.inc");
include_once($path_to_root . "/gl/includes/gl_db.inc");

$pdeb = $pcre = $cdeb = $ccre = $tdeb = $tcre = $pbal = $cbal = $tbal = 0;

//----------------------------------------------------------------------------------------------------

function display_type ($type, $typename, &$dec, &$rep, $from, $to, $zero, $balances, $dimension, $dimension2)
{
	global $pdeb, $pcre, $cdeb, $ccre, $tdeb, $tcre, $pbal, $cbal, $tbal, $SysPrefs;
	
	$printtitle = 0; //Flag for printing type name	
	
	//Get Accounts directly under this group/type
	$accounts = get_gl_accounts(null, null, $type);	
	
	$begin = get_fiscalyear_begin_for_date($from);
	if (date1_greater_date2($begin, $from))
		$begin = $from;
	$begin = add_days($begin, -1);
	while ($account=db_fetch($accounts))
	{
		//Print Type Title if it has atleast one non-zero account	
		if (!$printtitle)
		{	
			$rep->row -= 4;
			$rep->TextCol(0, 8, _("Group")." - ".$type ." - ".$typename);	
			$printtitle = 1;
			$rep->row -= 4;
			$rep->Line($rep->row);
			$rep->NewLine();			
		}
		
		// FA doesn't really clear the closed year, therefore the brought forward balance includes all the transactions from the past, even though the balance is null.
		// If we want to remove the balanced part for the past years, this option removes the common part from from the prev and tot figures.
		if (@$SysPrefs->clear_trial_balance_opening)
		{
			$open = get_balance($account["account_code"], $dimension, $dimension2, $begin,  $begin, false, true);
			$offset = min($open['debit'], $open['credit']);
		} else
			$offset = 0;

		$prev = get_balance($account["account_code"], $dimension, $dimension2, $begin, $from, false, false);
		$curr = get_balance($account["account_code"], $dimension, $dimension2, $from, $to, true, true);
		$tot = get_balance($account["account_code"], $dimension, $dimension2, $begin, $to, false, true);

		if ($zero == 0 && !$prev['balance'] && !$curr['balance'] && !$tot['balance'])
			continue;
		$rep->TextCol(0, 1, $account['account_code']);
		$rep->TextCol(1, 2,	$account['account_name']);
		if ($balances != 0)
		{
			if ($prev['balance'] >= 0.0)
				$rep->AmountCol(2, 3, $prev['balance'], $dec);
			else
				$rep->AmountCol(3, 4, abs($prev['balance']), $dec);
			if ($curr['balance'] >= 0.0)
				$rep->AmountCol(4, 5, $curr['balance'], $dec);
			else
				$rep->AmountCol(5, 6, abs($curr['balance']), $dec);
			if ($tot['balance'] >= 0.0)
				$rep->AmountCol(6, 7, $tot['balance'], $dec);
			else
				$rep->AmountCol(7, 8, abs($tot['balance']), $dec);
		}
		else
		{
			$rep->AmountCol(2, 3, $prev['debit']-$offset, $dec);
			$rep->AmountCol(3, 4, $prev['credit']-$offset, $dec);
			$rep->AmountCol(4, 5, $curr['debit'], $dec);
			$rep->AmountCol(5, 6, $curr['credit'], $dec);
			$rep->AmountCol(6, 7, $tot['debit']-$offset, $dec);
			$rep->AmountCol(7, 8, $tot['credit']-$offset, $dec);
			$pdeb += $prev['debit']-$offset;
			$pcre += $prev['credit']-$offset;
			$cdeb += $curr['debit'];
			$ccre += $curr['credit'];
			$tdeb += $tot['debit']-$offset;
			$tcre += $tot['credit']-$offset;
		}	

		$pbal += $prev['balance'];
		$cbal += $curr['balance'];
		$tbal += $tot['balance'];
		$rep->NewLine();

		if ($rep->row < $rep->bottomMargin + $rep->lineHeight)
		{
			$rep->Line($rep->row - 2);
			$rep->NewPage();
		}
	}
		
	//Get Account groups/types under this group/type
	$result = get_account_types(false, false, $type);
	while ($accounttype=db_fetch($result))
	{
		//Print Type Title if has sub types and not previously printed
		if (!$printtitle)
		{
			$rep->row -= 4;
			$rep->TextCol(0, 8, _("Group")." - ".$type ." - ".$typename);	
			$printtitle = 1;
			$rep->row -= 4;
			$rep->Line($rep->row);
			$rep->NewLine();		
		}
		display_type($accounttype["id"], $accounttype["name"].' ('.$typename.')', $dec, $rep, $from, $to, $zero, $balances, $dimension, $dimension2);
	}
}

//----------------------------------------------------------------------------------------------------

print_trial_balance();

//----------------------------------------------------------------------------------------------------

function print_trial_balance()
{
	global $path_to_root;
	global $pdeb, $pcre, $cdeb, $ccre, $tdeb, $tcre, $pbal, $cbal, $tbal;

	$dim = get_company_pref('use_dimension');
	$dimension = $dimension2 = 0;

	$from = $_POST['PARAM_0'];
	$to = $_POST['PARAM_1'];
	$zero = $_POST['PARAM_2'];
	$balances = $_POST['PARAM_3'];
	if ($dim == 2)
	{
		$dimension = $_POST['PARAM_4'];
		$dimension2 = $_POST['PARAM_5'];
		$comments = $_POST['PARAM_6'];
		$orientation = $_POST['PARAM_7'];
		$destination = $_POST['PARAM_8'];
	}
	else if ($dim == 1)
	{
		$dimension = $_POST['PARAM_4'];
		$comments = $_POST['PARAM_5'];
		$orientation = $_POST['PARAM_6'];
		$destination = $_POST['PARAM_7'];
	}
	else
	{
		$comments = $_POST['PARAM_4'];
		$orientation = $_POST['PARAM_5'];
		$destination = $_POST['PARAM_6'];
	}
	if ($destination)
		include_once($path_to_root . "/reporting/includes/excel_report.inc");
	else
		include_once($path_to_root . "/reporting/includes/pdf_report.inc");
	$orientation = ($orientation ? 'L' : 'P');
	$dec = user_price_dec();

	$cols2 = array(0, 50, 190, 310, 430, 530);
	//-------------0--1---2----3----4----5--

	$headers2 = array('', '', _('Brought Forward'),	_('This Period'), _('Balance'));

	$aligns2 = array('left', 'left', 'left', 'left', 'left');

	$cols = array(0, 50, 150, 210, 270,	330, 390, 450, 510,	570);
	//------------0--1---2----3----4----5----6----7----8--

	$headers = array(_('Account'), _('Account Name'), _('Debit'), _('Credit'), _('Debit'),
		_('Credit'), _('Debit'), _('Credit'));

	$aligns = array('left',	'left',	'right', 'right', 'right', 'right',	'right', 'right');

    if ($dim == 2)
    {
    	$params =   array( 	0 => $comments,
    				    1 => array('text' => _('Period'),'from' => $from, 'to' => $to),
                    	2 => array('text' => _('Dimension')." 1",
                            'from' => get_dimension_string($dimension), 'to' => ''),
                    	3 => array('text' => _('Dimension')." 2",
                            'from' => get_dimension_string($dimension2), 'to' => ''));
    }
    elseif ($dim == 1)
    {
    	$params =   array( 	0 => $comments,
    				    1 => array('text' => _('Period'),'from' => $from, 'to' => $to),
                    	2 => array('text' => _('Dimension'),
                            'from' => get_dimension_string($dimension), 'to' => ''));
    }
    else
    {
    	$params =   array( 	0 => $comments,
    				    1 => array('text' => _('Period'),'from' => $from, 'to' => $to));
    }

	$rep = new FrontReport(_('Trial Balance'), "TrialBalance", user_pagesize(), 9, $orientation);
    if ($orientation == 'L')
    {
    	recalculate_cols($cols);
    	recalculate_cols($cols2);
	}

	$rep->Font();
	$rep->Info($params, $cols, $headers, $aligns, $cols2, $headers2, $aligns2);
	$rep->NewPage();

	$classresult = get_account_classes(false);
	while ($class = db_fetch($classresult))
	{
		$rep->Font('bold');
		$rep->TextCol(0, 1, $class['cid']);
		$rep->TextCol(1, 4, $class['class_name']);
		$rep->Font();
		$rep->NewLine();

		//Get Account groups/types under this group/type with no parents
		$typeresult = get_account_types(false, $class['cid'], -1);
		while ($accounttype=db_fetch($typeresult))
		{
			display_type($accounttype["id"], $accounttype["name"], $dec, $rep, $from, $to,	$zero, $balances, $dimension, $dimension2);
		}
		$rep->NewLine();
	}
	$rep->Line($rep->row);
	$rep->NewLine();
	$rep->Font('bold');

	if ($balances == 0)
	{
		$rep->TextCol(0, 2, _("Total"));
		$rep->AmountCol(2, 3, $pdeb, $dec);
		$rep->AmountCol(3, 4, $pcre, $dec);
		$rep->AmountCol(4, 5, $cdeb, $dec);
		$rep->AmountCol(5, 6, $ccre, $dec);
		$rep->AmountCol(6, 7, $tdeb, $dec);
		$rep->AmountCol(7, 8, $tcre, $dec);
		$rep->NewLine();
	}	
	$rep->TextCol(0, 2, _("Ending Balance"));

	if ($pbal >= 0.0)
		$rep->AmountCol(2, 3, $pbal, $dec);
	else
		$rep->AmountCol(3, 4, abs($pbal), $dec);
	if ($cbal >= 0.0)
		$rep->AmountCol(4, 5, $cbal, $dec);
	else
		$rep->AmountCol(5, 6, abs($cbal), $dec);
	if ($tbal >= 0.0)
		$rep->AmountCol(6, 7, $tbal, $dec);
	else
		$rep->AmountCol(7, 8, abs($tbal), $dec);
	$rep->NewLine();

	$rep->Line($rep->row + 10);
	if (($pbal = round2($pbal, $dec)) != 0.0 && $dimension == 0 && $dimension2 == 0)
	{
		$rep->NewLine(2);
		$rep->Font();
		$rep->TextCol(0, 8, _("The Opening Balance is not in balance, probably due to a non closed Previous Fiscalyear."));
	}	
	$rep->End();
}

