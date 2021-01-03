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
// Creator:	Joe Hunt, Chaitanya for the recursive version 2009-02-05.
// date_:	2005-05-19
// Title:	Balance Sheet
// ----------------------------------------------------------------
$path_to_root="..";

include_once($path_to_root . "/includes/session.inc");
include_once($path_to_root . "/includes/date_functions.inc");
include_once($path_to_root . "/includes/data_checks.inc");
include_once($path_to_root . "/gl/includes/gl_db.inc");
include_once($path_to_root . "/admin/db/tags_db.inc");

//----------------------------------------------------------------------------------------------------

function display_type ($type, $typename, $from, $to, $convert, &$dec, &$rep, $dimension, $dimension2, $tags, &$pg, $graphics)
{
	$code_open_balance = 0;
	$code_period_balance = 0;
	$open_balance_total = 0;
	$period_balance_total = 0;
	$totals_arr = array();

	$printtitle = 0; //Flag for printing type name	
	
	//Get Accounts directly under this group/type
	$result = get_gl_accounts(null, null, $type);	
	while ($account=db_fetch($result))
	{
		if ($tags != -1 && is_array($tags) && $tags[0] != false)
		{
			if (!is_record_in_tags($tags, TAG_ACCOUNT, $account['account_code']))
				continue;
		}	
		$prev_balance = get_gl_balance_from_to("", $from, $account["account_code"], $dimension, $dimension2);

		$curr_balance = get_gl_trans_from_to($from, $to, $account["account_code"], $dimension, $dimension2);

		if (!$prev_balance && !$curr_balance)
			continue;
		
		//Print Type Title if it has atleast one non-zero account	
		if (!$printtitle)
		{
			$printtitle = 1;
			$rep->row -= 4;
			$rep->TextCol(0, 5, $typename);
			$rep->row -= 4;
			$rep->Line($rep->row);
			$rep->NewLine();		
		}			

		$rep->TextCol(0, 1,	$account['account_code']);
		$rep->TextCol(1, 2,	$account['account_name']);

		$rep->AmountCol(2, 3, $prev_balance * $convert, $dec);
		$rep->AmountCol(3, 4, $curr_balance * $convert, $dec);
		$rep->AmountCol(4, 5, ($prev_balance + $curr_balance) * $convert, $dec);

		$rep->NewLine();

		$code_open_balance += $prev_balance;
		$code_period_balance += $curr_balance;
	}
		
	//Get Account groups/types under this group/type
	$result = get_account_types(false, false, $type);
	while ($accounttype=db_fetch($result))
	{
		//Print Type Title if has sub types and not previously printed
		if (!$printtitle)
		{
			$printtitle = 1;
			$rep->row -= 4;
			$rep->TextCol(0, 5, $typename);
			$rep->row -= 4;
			$rep->Line($rep->row);
			$rep->NewLine();		
		}

		$totals_arr = display_type($accounttype["id"], $accounttype["name"], $from, $to, $convert, $dec, 
			$rep, $dimension, $dimension2, $tags, $pg, $graphics);
		$open_balance_total += $totals_arr[0];
		$period_balance_total += $totals_arr[1];
	}

	//Display Type Summary if total is != 0 OR head is printed (Needed in case of unused hierarchical COA) 
	if (($code_open_balance + $open_balance_total + $code_period_balance + $period_balance_total) != 0 || $printtitle)
	{
		$rep->row += 6;
		$rep->Line($rep->row);
		$rep->NewLine();
		$rep->TextCol(0, 2,	_('Total') . " " . $typename);
		$rep->AmountCol(2, 3, ($code_open_balance + $open_balance_total) * $convert, $dec);
		$rep->AmountCol(3, 4, ($code_period_balance + $period_balance_total) * $convert, $dec);
		$rep->AmountCol(4, 5, ($code_open_balance + $open_balance_total + $code_period_balance + $period_balance_total) * $convert, $dec);		
		if ($graphics)
		{
			$pg->x[] = $typename;
			$pg->y[] = abs($code_open_balance + $open_balance_total);
			$pg->z[] = abs($code_period_balance + $period_balance_total);
		}
		$rep->NewLine();
	}
	
	$totals_arr[0] = $code_open_balance + $open_balance_total;
	$totals_arr[1] = $code_period_balance + $period_balance_total;
	return $totals_arr;
}

print_balance_sheet();

//----------------------------------------------------------------------------------------------------

function print_balance_sheet()
{
	global $path_to_root, $SysPrefs;

	$dim = get_company_pref('use_dimension');
	$dimension = $dimension2 = 0;

	$from = $_POST['PARAM_0'];
	$to = $_POST['PARAM_1'];
	if ($dim == 2)
	{
		$dimension = $_POST['PARAM_2'];
		$dimension2 = $_POST['PARAM_3'];
		$tags = (isset($_POST['PARAM_4']) ? $_POST['PARAM_4'] : -1);
		$decimals = $_POST['PARAM_5'];
		$graphics = $_POST['PARAM_6'];
		$comments = $_POST['PARAM_7'];
		$orientation = $_POST['PARAM_8'];
		$destination = $_POST['PARAM_9'];
	}
	elseif ($dim == 1)
	{
		$dimension = $_POST['PARAM_2'];
		$tags = (isset($_POST['PARAM_3']) ? $_POST['PARAM_3'] : -1);
		$decimals = $_POST['PARAM_4'];
		$graphics = $_POST['PARAM_5'];
		$comments = $_POST['PARAM_6'];
		$orientation = $_POST['PARAM_7'];
		$destination = $_POST['PARAM_8'];
	}
	else
	{
		$tags = (isset($_POST['PARAM_2']) ? $_POST['PARAM_2'] : -1);
		$decimals = $_POST['PARAM_3'];
		$graphics = $_POST['PARAM_4'];
		$comments = $_POST['PARAM_5'];
		$orientation = $_POST['PARAM_6'];
		$destination = $_POST['PARAM_7'];
	}
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
	if (!$decimals)
		$dec = 0;
	else
		$dec = user_price_dec();

	$cols = array(0, 60, 200, 350, 425,	500);
	//------------0--1---2----3----4----5--

	$headers = array(_('Account'), _('Account Name'), _('Open Balance'), _('Period'),
		_('Close Balance'));

	$aligns = array('left',	'left',	'right', 'right', 'right');

    if ($dim == 2)
    {
    	$params =   array( 	0 => $comments,
    				    1 => array('text' => _('Period'),'from' => $from, 'to' => $to),
                    	2 => array('text' => _('Dimension')." 1",
                            'from' => get_dimension_string($dimension), 'to' => ''),
                    	3 => array('text' => _('Dimension')." 2",
                            'from' => get_dimension_string($dimension2), 'to' => ''),
                        4 => array('text' => _('Tags'), 'from' => get_tag_names($tags), 'to' => ''));
    }
    elseif ($dim == 1)
    {
    	$params =   array( 	0 => $comments,
    				    1 => array('text' => _('Period'),'from' => $from, 'to' => $to),
                    	2 => array('text' => _('Dimension'),
                            'from' => get_dimension_string($dimension), 'to' => ''),
                        3 => array('text' => _('Tags'), 'from' => get_tag_names($tags), 'to' => ''));
    }
    else
    {
    	$params =   array( 	0 => $comments,
    				    1 => array('text' => _('Period'),'from' => $from, 'to' => $to),
    				    2 => array('text' => _('Tags'), 'from' => get_tag_names($tags), 'to' => ''));
    }

	$rep = new FrontReport(_('Balance Sheet'), "BalanceSheet", user_pagesize(), 9, $orientation);
    if ($orientation == 'L')
    	recalculate_cols($cols);
	$rep->Font();
	$rep->Info($params, $cols, $headers, $aligns);
	$rep->NewPage();

	$calc_open = $calc_period = 0.0;
	$equity_open = $equity_period = 0.0;
	$liability_open = $liability_period = 0.0;
	$econvert = $lconvert = 0;
	
	$classresult = get_account_classes(false, 1);
	while ($class = db_fetch($classresult))
	{
		$class_open_total = 0;
		$class_period_total = 0;
		$convert = get_class_type_convert($class["ctype"]); 		
		
		//Print Class Name	
		$rep->Font('bold');
		$rep->TextCol(0, 5, $class["class_name"]);
		$rep->Font();
		$rep->NewLine();
		
		//Get Account groups/types under this group/type with no parents
		$typeresult = get_account_types(false, $class['cid'], -1);
		while ($accounttype=db_fetch($typeresult))
		{
			$classtotal = display_type($accounttype["id"], $accounttype["name"], $from, $to, $convert, $dec, 
				$rep, $dimension, $dimension2, $tags, $pg, $graphics);
			$class_open_total += $classtotal[0];
			$class_period_total += $classtotal[1];			
		}
		
		//Print Class Summary	
		$rep->row += 6;
		$rep->Line($rep->row);
		$rep->NewLine();
		$rep->Font('bold');
		$rep->TextCol(0, 2,	_('Total') . " " . $class["class_name"]);
		$rep->AmountCol(2, 3, $class_open_total * $convert, $dec);
		$rep->AmountCol(3, 4, $class_period_total * $convert, $dec);
		$rep->AmountCol(4, 5, ($class_open_total + $class_period_total) * $convert, $dec);
		$rep->Font();
		$rep->NewLine(2);	

		$calc_open += $class_open_total;
		$calc_period += $class_period_total;
		if ($class['ctype'] == CL_EQUITY)
		{
			$equity_open += $class_open_total;
			$equity_period += $class_period_total;
			$econvert = $convert;
		}
		elseif ($class['ctype'] == CL_LIABILITIES)
		{
			$liability_open += $class_open_total;
			$liability_period += $class_period_total;
			$lconvert = $convert;
		}
	}
	$rep->Font();	
	$rep->TextCol(0, 2,	_('Calculated Return'));
	if ($lconvert == 1)
	{
		$calc_open *= -1;
		$calc_period *= -1;
	}	
	$rep->AmountCol(2, 3, $calc_open, $dec); // never convert
	$rep->AmountCol(3, 4, $calc_period, $dec);
	$rep->AmountCol(4, 5, $calc_open + $calc_period, $dec);
	$rep->NewLine(2);

	$rep->Font('bold');	
	$rep->TextCol(0, 2,	_('Total') . " " . _('Liabilities') . _(' and ') . _('Equities'));
	$topen = $equity_open * $econvert + $liability_open * $lconvert + $calc_open;
	$tperiod = $equity_period * $econvert + $liability_period * $lconvert + $calc_period;
	$tclose = $topen + $tperiod;
	$rep->AmountCol(2, 3, $topen, $dec);
	$rep->AmountCol(3, 4, $tperiod, $dec);
	$rep->AmountCol(4, 5, $tclose, $dec);
	
	$rep->Font();
	$rep->NewLine();
	$rep->Line($rep->row);
	if ($graphics)
	{
		$pg->x[] = _('Calculated Return');
		$pg->y[] = abs($calc_open);
		$pg->z[] = abs($calc_period);
		$pg->title     = $rep->title;
		$pg->axis_x    = _("Group");
		$pg->axis_y    = _("Amount");
		$pg->graphic_1 = $headers[2];
		$pg->graphic_2 = $headers[3];
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
	$rep->End();
}

