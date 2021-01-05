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
// Title:	Annual expense breakdown
// ----------------------------------------------------------------
$path_to_root='..';

include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/includes/date_functions.inc');
include_once($path_to_root . '/includes/data_checks.inc');
include_once($path_to_root . '/gl/includes/gl_db.inc');
include_once($path_to_root . '/admin/db/tags_db.inc');

//----------------------------------------------------------------------------------------------------

print_annual_expense_breakdown();

//----------------------------------------------------------------------------------------------------

function getPeriods($yr, $mo, $account, $dimension, $dimension2, $thousands) {
	$date13 = date('Y-m-d',mktime(0,0,0,$mo+1,1,$yr));
	$date12 = date('Y-m-d',mktime(0,0,0,$mo,1,$yr));
	$date11 = date('Y-m-d',mktime(0,0,0,$mo-1,1,$yr));
	$date10 = date('Y-m-d',mktime(0,0,0,$mo-2,1,$yr));
	$date09 = date('Y-m-d',mktime(0,0,0,$mo-3,1,$yr));
	$date08 = date('Y-m-d',mktime(0,0,0,$mo-4,1,$yr));
	$date07 = date('Y-m-d',mktime(0,0,0,$mo-5,1,$yr));
	$date06 = date('Y-m-d',mktime(0,0,0,$mo-6,1,$yr));
	$date05 = date('Y-m-d',mktime(0,0,0,$mo-7,1,$yr));
	$date04 = date('Y-m-d',mktime(0,0,0,$mo-8,1,$yr));
	$date03 = date('Y-m-d',mktime(0,0,0,$mo-9,1,$yr));
	$date02 = date('Y-m-d',mktime(0,0,0,$mo-10,1,$yr));
	$date01 = date('Y-m-d',mktime(0,0,0,$mo-11,1,$yr));

	$sql = "SELECT SUM(CASE WHEN tran_date >= '$date01' AND tran_date < '$date02' THEN amount / $thousands ELSE 0 END) AS per01,
		SUM(CASE WHEN tran_date >= '$date02' AND tran_date < '$date03' THEN amount / $thousands ELSE 0 END) AS per02,
		SUM(CASE WHEN tran_date >= '$date03' AND tran_date < '$date04' THEN amount / $thousands ELSE 0 END) AS per03,
		SUM(CASE WHEN tran_date >= '$date04' AND tran_date < '$date05' THEN amount / $thousands ELSE 0 END) AS per04,
		SUM(CASE WHEN tran_date >= '$date05' AND tran_date < '$date06' THEN amount / $thousands ELSE 0 END) AS per05,
		SUM(CASE WHEN tran_date >= '$date06' AND tran_date < '$date07' THEN amount / $thousands ELSE 0 END) AS per06,
		SUM(CASE WHEN tran_date >= '$date07' AND tran_date < '$date08' THEN amount / $thousands ELSE 0 END) AS per07,
		SUM(CASE WHEN tran_date >= '$date08' AND tran_date < '$date09' THEN amount / $thousands ELSE 0 END) AS per08,
		SUM(CASE WHEN tran_date >= '$date09' AND tran_date < '$date10' THEN amount / $thousands ELSE 0 END) AS per09,
		SUM(CASE WHEN tran_date >= '$date10' AND tran_date < '$date11' THEN amount / $thousands ELSE 0 END) AS per10,
		SUM(CASE WHEN tran_date >= '$date11' AND tran_date < '$date12' THEN amount / $thousands ELSE 0 END) AS per11,
		SUM(CASE WHEN tran_date >= '$date12' AND tran_date < '$date13' THEN amount / $thousands ELSE 0 END) AS per12,
		SUM(CASE WHEN tran_date >= '$date01' AND tran_date < '$date13' THEN amount / $thousands ELSE 0 END) AS pertotal
		FROM ".TB_PREF."gl_trans
		WHERE account='$account'";
	if ($dimension != 0)
		$sql .= " AND dimension_id = ".($dimension<0?0:db_escape($dimension));
	if ($dimension2 != 0)
		$sql .= " AND dimension2_id = ".($dimension2<0?0:db_escape($dimension2));

	$result = db_query($sql, 'Transactions for account $account could not be calculated');

	return db_fetch($result);
}

//----------------------------------------------------------------------------------------------------

function display_type ($type, $typename, $yr, $mo, $convert, &$dec, &$rep, $dimension, $dimension2, $tags, $thousands) {
	$ctotal = array(1 => 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0);
	$total = array(1 => 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0);
	$totals_arr = array();

	$printtitle = 0; //Flag for printing type name	

	//Get Accounts directly under this group/type
	$result = get_gl_accounts(null, null, $type);	
	while ($account=db_fetch($result)) {
		if ($tags != -1 && is_array($tags) && $tags[0] != false) {
			if (!is_record_in_tags($tags, TAG_ACCOUNT, $account['account_code']))
				continue;
		}	
		$bal = getPeriods($yr, $mo, $account['account_code'], $dimension, $dimension2, $thousands);
		if (!$bal['per01'] && !$bal['per02'] && !$bal['per03'] && !$bal['per04'] &&	!$bal['per05'] && !$bal['per06'] && !$bal['per07'] && !$bal['per08'] && !$bal['per09'] && !$bal['per10'] && !$bal['per11'] && !$bal['per12'])
			continue;
	
		//Print Type Title if it has at least one non-zero account	
		if (!$printtitle) {
			$printtitle = 1;
			$rep->row -= 4;
			$rep->TextCol(0, 5, $typename);
			$rep->row -= 4;
			$rep->Line($rep->row);
			$rep->NewLine();
		}			

		$balance = array(1 => $bal['per01'], $bal['per02'], $bal['per03'], $bal['per04'], $bal['per05'], $bal['per06'], $bal['per07'], $bal['per08'], $bal['per09'], $bal['per10'], $bal['per11'], $bal['per12'], $bal['pertotal']);
		$rep->TextCol(0, 1,	$account['account_code']);
		$rep->TextCol(1, 2,	$account['account_name']);

		for ($i = 1; $i <= 13; $i++) {
			$rep->AmountCol($i + 1, $i + 2, $balance[$i] * $convert, $dec);
			$ctotal[$i] += $balance[$i];
		}

		$rep->NewLine();
	}
		
	//Get Account groups/types under this group/type
	$result = get_account_types(false, false, $type);
	while ($accounttype=db_fetch($result)) {
		//Print Type Title if has sub types and not previously printed
		if (!$printtitle) {
			$printtitle = 1;
			$rep->row -= 4;
			$rep->TextCol(0, 5, $typename);
			$rep->row -= 4;
			$rep->Line($rep->row);
			$rep->NewLine();
		}

		$totals_arr = display_type($accounttype['id'], $accounttype['name'], $yr, $mo, $convert, $dec, $rep, $dimension, $dimension2, $tags, $thousands);
		for ($i = 1; $i <= 13; $i++) {
			$total[$i] += $totals_arr[$i];
		}
	}

	//Display Type Summary if total is != 0 OR head is printed (Needed in case of unused hierarchical COA) 
	if ($printtitle) {
		$rep->row += 6;
		$rep->Line($rep->row);
		$rep->NewLine();
		$rep->TextCol(0, 2,	_('Total') . ' ' . $typename);
		for ($i = 1; $i <= 13; $i++)
			$rep->AmountCol($i + 1, $i + 2, ($total[$i] + $ctotal[$i]) * $convert, $dec);
		$rep->NewLine();
	}
	for ($i = 1; $i <= 13; $i++)
		$totals_arr[$i] = $total[$i] + $ctotal[$i];	
	return $totals_arr;
}

//----------------------------------------------------------------------------------------------------

function print_annual_expense_breakdown() {
	global $path_to_root, $SysPrefs;

	$dim = get_company_pref('use_dimension');
	$dimension = $dimension2 = 0;
	$thousands = 0;

	if ($dim == 2) {
		$year = $_POST['PARAM_0'];
		$dimension = $_POST['PARAM_1'];
		$dimension2 = $_POST['PARAM_2'];
		$tags = (isset($_POST['PARAM_3']) ? $_POST['PARAM_3'] : -1);
		$comments = $_POST['PARAM_4'];
		$orientation = $_POST['PARAM_5'];
		$thousands = $_POST['PARAM_6'];
		$destination = $_POST['PARAM_7'];
	}
	elseif ($dim == 1) {
		$year = $_POST['PARAM_0'];
		$dimension = $_POST['PARAM_1'];
		$tags = (isset($_POST['PARAM_2']) ? $_POST['PARAM_2'] : -1);
		$comments = $_POST['PARAM_3'];
		$orientation = $_POST['PARAM_4'];
		$thousands = $_POST['PARAM_5'];
		$destination = $_POST['PARAM_6'];
	}
	else {
		$year = $_POST['PARAM_0'];
		$tags = (isset($_POST['PARAM_1']) ? $_POST['PARAM_1'] : -1);
		$comments = $_POST['PARAM_2'];
		$orientation = $_POST['PARAM_3'];
		$thousands = $_POST['PARAM_4'];
		$destination = $_POST['PARAM_5'];
	}
	if ($destination)
		include_once($path_to_root . '/reporting/includes/excel_report.inc');
	else
		include_once($path_to_root . '/reporting/includes/pdf_report.inc');

	$orientation = ($orientation ? 'L' : 'P');
	if ($thousands) {
		$dec = 1;
		$thousands = 1000;
		$amts_thousands = _('Amounts in thousands');
		$fontSize = 9;
	}
	else {
		$dec = 2;
		$thousands = 1;
		$amts_thousands = '';
		$fontSize = ($orientation == 'P' ? 7 : 8);
	}

	$cols = array(0, 34, 130, 162, 194, 226, 258, 290, 322, 354, 386, 418, 450, 482, 514, 546);
	//------------0--1---2----3----4----5----6----7----8----10---11---12---13---14---15---16-

	// from now
	$sql = "SELECT begin, end, YEAR(end) AS yr, MONTH(end) AS mo FROM ".TB_PREF."fiscal_year WHERE id=".db_escape($year);
	$result = db_query($sql, 'could not get fiscal year');
	$row = db_fetch($result);
	
	$year = sql2date($row['begin']).' - '.sql2date($row['end']);
	$yr = $row['yr'];
	$mo = $row['mo'];
	$da = 1;
	if ($SysPrefs->date_system == 1)
		list($yr, $mo, $da) = jalali_to_gregorian($yr, $mo, $da);
	elseif ($SysPrefs->date_system == 2)
		list($yr, $mo, $da) = islamic_to_gregorian($yr, $mo, $da);
	$per12 = strftime('%b',mktime(0,0,0,$mo,$da,$yr));
	$per11 = strftime('%b',mktime(0,0,0,$mo-1,$da,$yr));
	$per10 = strftime('%b',mktime(0,0,0,$mo-2,$da,$yr));
	$per09 = strftime('%b',mktime(0,0,0,$mo-3,$da,$yr));
	$per08 = strftime('%b',mktime(0,0,0,$mo-4,$da,$yr));
	$per07 = strftime('%b',mktime(0,0,0,$mo-5,$da,$yr));
	$per06 = strftime('%b',mktime(0,0,0,$mo-6,$da,$yr));
	$per05 = strftime('%b',mktime(0,0,0,$mo-7,$da,$yr));
	$per04 = strftime('%b',mktime(0,0,0,$mo-8,$da,$yr));
	$per03 = strftime('%b',mktime(0,0,0,$mo-9,$da,$yr));
	$per02 = strftime('%b',mktime(0,0,0,$mo-10,$da,$yr));
	$per01 = strftime('%b',mktime(0,0,0,$mo-11,$da,$yr));

	$headers = array(_('Account'), _('Account Name'), $per01, $per02, $per03, $per04, $per05, $per06, $per07, $per08, $per09, $per10, $per11, $per12, _('Total'));

	$aligns = array('left',	'left',	'right', 'right', 'right',	'right', 'right', 'right', 'right', 'right', 'right',	'right', 'right', 'right', 'right');

	if ($dim == 2) {
		$params =   array( 	0 => $comments,
						1 => array('text' => _('Year'),
							'from' => $year, 'to' => ''),
						2 => array('text' => _('Dimension').' 1',
							'from' => get_dimension_string($dimension), 'to' => ''),
						3 => array('text' => _('Dimension').' 2',
							'from' => get_dimension_string($dimension2), 'to' => ''),
						4 => array('text' => _('Tags'), 'from' => get_tag_names($tags), 'to' => ''),	
						5 => array('text' => _('Info'), 'from' => $amts_thousands, 'to' => ''));
	}
	elseif ($dim == 1) {
		$params =   array( 	0 => $comments,
						1 => array('text' => _('Year'),
							'from' => $year, 'to' => ''),
						2 => array('text' => _('Dimension'),
							'from' => get_dimension_string($dimension), 'to' => ''),
						3 => array('text' => _('Tags'), 'from' => get_tag_names($tags), 'to' => ''),	
						4 => array('text' => _('Info'), 'from' => $amts_thousands, 'to' => ''));
	}
	else {
		$params =   array( 	0 => $comments,
						1 => array('text' => _('Year'),
							'from' => $year, 'to' => ''),
						2 => array('text' => _('Tags'), 'from' => get_tag_names($tags), 'to' => ''),	
						3 => array('text' => _('Info'), 'from' => $amts_thousands, 'to' => ''));
	}

	$rep = new FrontReport(_('Annual Expense Breakdown'), 'AnnualBreakDown', user_pagesize(), $fontSize,
										$orientation);
	if ($orientation == 'L')
		recalculate_cols($cols);
	elseif (user_pagesize() == 'A4') {
		// Portrait, so adjust columns for A4, 16.7 pts narrower than Letter
		for ($i = 2; $i < sizeof($cols); $i++)
			$cols[$i] -= 17;
	}

	$rep->Font();
	$rep->Info($params, $cols, $headers, $aligns);
	$rep->NewPage();

	$sales = Array(1 => 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0);
	
	$classresult = get_account_classes(false, 0);
	while ($class = db_fetch($classresult)) {
		$ctotal = Array(1 => 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0);
		$convert = get_class_type_convert($class['ctype']); 		
		
		//Print Class Name	
		$rep->Font('bold');
		$rep->TextCol(0, 5, $class['class_name']);
		$rep->Font();
		$rep->NewLine();
		
		//Get Account groups/types under this group/type with no parents
		$typeresult = get_account_types(false, $class['cid'], -1);
		while ($accounttype=db_fetch($typeresult)) {
			$classtotal = display_type($accounttype['id'], $accounttype['name'], $yr, $mo, $convert, $dec, $rep, $dimension,	$dimension2, $tags, $thousands);
			for ($i = 1; $i <= 13; $i++)
				$ctotal[$i] += $classtotal[$i];
		}
		
		//Print Class Summary	
		$rep->row += 6;
		$rep->Line($rep->row);
		$rep->NewLine();
		$rep->Font('bold');
		$rep->TextCol(0, 2,	_('Total') . ' ' . $class['class_name']);
		for ($i = 1; $i <= 13; $i++) {
			$rep->AmountCol($i + 1, $i + 2, $ctotal[$i] * $convert, $dec);
			$sales[$i] += $ctotal[$i];
		}
		$rep->Font();
		$rep->NewLine(2);
	}
	$rep->Font('bold');	
	$rep->TextCol(0, 2,	_('Calculated Return'));
	for ($i = 1; $i <= 13; $i++)
		$rep->AmountCol($i + 1, $i + 2, $sales[$i] * -1, $dec);
	$rep->Font();
	$rep->NewLine();
	$rep->Line($rep->row);
	$rep->NewLine(2);
	$rep->End();
}