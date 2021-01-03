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
$page_security = 'SA_ASSETSANALYTIC';
// ----------------------------------------------------------------
// $ Revision:	2.0 $
// Creator:	Joe Hunt
// date_:	2015-12-01
// Title:	Fixed Assets Valuation
// ----------------------------------------------------------------
$path_to_root="..";

include_once($path_to_root . "/includes/session.inc");
include_once($path_to_root . "/includes/date_functions.inc");
include_once($path_to_root . "/includes/data_checks.inc");
include_once($path_to_root . "/gl/includes/gl_db.inc");
include_once($path_to_root . "/inventory/includes/db/items_category_db.inc");
include_once($path_to_root . "/fixed_assets/includes/fixed_assets_db.inc");
include_once($path_to_root . "/fixed_assets/includes/fa_classes_db.inc");

function find_last_location($stock_id, $end_date)
{
	$end_date = date2sql($end_date);
	$sql = "SELECT loc_code FROM ".TB_PREF."stock_moves WHERE stock_id = ".db_escape($stock_id)." AND
		tran_date <= '$end_date' ORDER BY tran_date DESC LIMIT 1";
	$res = db_query($sql,"No stock moves were returned");
	$row = db_fetch_row($res);
	return $row[0];
}

//----------------------------------------------------------------------------------------------------

print_fixed_assets_valuation_report();

//----------------------------------------------------------------------------------------------------

function print_fixed_assets_valuation_report()
{
    global $path_to_root, $SysPrefs;

	$date = $_POST['PARAM_0'];
    $class = $_POST['PARAM_1'];
    $location = $_POST['PARAM_2'];
    $detail = $_POST['PARAM_3'];
    $comments = $_POST['PARAM_4'];
	$orientation = $_POST['PARAM_5'];
	$destination = $_POST['PARAM_6'];
	if ($destination)
		include_once($path_to_root . "/reporting/includes/excel_report.inc");
	else
		include_once($path_to_root . "/reporting/includes/pdf_report.inc");
	$detail = !$detail;
    $dec = user_price_dec();

	$orientation = ($orientation ? 'L' : 'P');
	if ($class == ALL_NUMERIC)
		$class = 0;
	if ($class== 0)
		$cln = _('All');
	else
		$cln = get_fixed_asset_classname($class);

	if ($location == ALL_TEXT)
		$location = 'all';
	if ($location == 'all')
		$loc = _('All');
	else
		$loc = get_location_name($location);

	$cols = array(0, 75, 225, 250, 350, 450,	515);

	$headers = array(_('Class'), '', _('UOM'),  _('Initial'), _('Depreciations'), _('Current'));

	$aligns = array('left',	'left',	'left', 'right', 'right', 'right', 'right');

    $params =   array( 	0 => $comments,
    					1 => array('text' => _('End Date'), 'from' => $date, 		'to' => ''),
    				    2 => array('text' => _('Class'), 'from' => $cln, 'to' => ''),
    				    3 => array('text' => _('Location'), 'from' => $loc, 'to' => ''));

    $rep = new FrontReport(_('Fixed Assets Valuation Report'), "FixedAssetsValReport", user_pagesize(), 9, $orientation);
    if ($orientation == 'L')
    	recalculate_cols($cols);
    $rep->Font();
    $rep->Info($params, $cols, $headers, $aligns);
    $rep->NewPage();

	//$res = getTransactions($category, $location, $date);
	$sql = get_sql_for_fixed_assets(false);
	$res = db_query($sql,"No transactions were returned");
	
	$total = $grandtotal = 0.0;
	$catt = '';
	while ($trans=db_fetch($res))
	{
		$loc = find_last_location($trans['stock_id'], $date);
		if ($location != 'all' && $location != $loc)
			continue;
		$purchase = get_fixed_asset_purchase($trans['stock_id']);
		$d = sql2date($purchase['tran_date']);
		if (date1_greater_date2($d, $date))
			continue;
		if ($class != 0 && $cln != $trans['description'])
			continue;
		if ($catt != $trans['description'])
		{
			if ($catt != '')
			{
				if ($detail)
				{
					$rep->NewLine(2, 3);
					$rep->TextCol(0, 4, _('Total'));
				}
				$rep->AmountCol(5, 6, $total, $dec);
				if ($detail)
				{
					$rep->Line($rep->row - 2);
					$rep->NewLine();
				}
				$rep->NewLine();
				$total = 0.0;
			}
			$rep->TextCol(0, 2, $trans['description']);
			$catt = $trans['description'];
			if ($detail)
				$rep->NewLine();
		}
		$UnitCost = $trans['purchase_cost'];
		$Depreciation = $trans['purchase_cost'] - $trans['material_cost'];
		$Balance = $trans['material_cost'];
		if ($detail)
		{
			$rep->NewLine();
			$rep->TextCol(0, 1, $trans['stock_id']);
			$rep->TextCol(1, 2, $trans['name']);
			$rep->TextCol(2, 3, $trans['units']);
			$rep->AmountCol(3, 4, $UnitCost, $dec);
			$rep->AmountCol(4, 5, $Depreciation, $dec);
			$rep->AmountCol(5, 6, $Balance, $dec);
		}
		$total += $Balance;
		$grandtotal += $Balance;
	}
	if ($detail)
	{
		$rep->NewLine(2, 3);
		$rep->TextCol(0, 4, _('Total'));
	}
	$rep->Amountcol(5, 6, $total, $dec);
	if ($detail)
	{
		$rep->Line($rep->row - 2);
		$rep->NewLine();
	}
	$rep->NewLine(2, 1);
	$rep->TextCol(0, 4, _('Grand Total'));
	$rep->AmountCol(5, 6, $grandtotal, $dec);
	$rep->Line($rep->row  - 4);
	$rep->NewLine();
    $rep->End();
}

