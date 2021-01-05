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
$path_to_root = '../..';

include_once($path_to_root.'/includes/session.inc');

include_once($path_to_root.'/includes/date_functions.inc');
include_once($path_to_root.'/includes/ui.inc');
include_once($path_to_root.'/includes/data_checks.inc');

include_once($path_to_root.'/gl/includes/gl_db.inc');

$js = '';
if (user_use_date_picker())
	$js = get_js_date_picker();

page(_($help_context = 'Profit & Loss Drilldown'), false, false, '', $js);

$compare_types = array(
	_('Accumulated'),
	_('Period Y-1'),
	_('Budget')
);

//----------------------------------------------------------------------------------------------------
// Ajax updates

if (get_post('Show')) 
	$Ajax->activate('pl_tbl');
if (isset($_GET['TransFromDate']))
	$_POST['TransFromDate'] = $_GET['TransFromDate'];	
if (isset($_GET['TransToDate']))
	$_POST['TransToDate'] = $_GET['TransToDate'];
if (isset($_GET['Compare']))
	$_POST['Compare'] = $_GET['Compare'];
if (isset($_GET['Dimension']))
	$_POST['Dimension'] = $_GET['Dimension'];
if (isset($_GET['Dimension2']))
	$_POST['Dimension2'] = $_GET['Dimension2'];
if (isset($_GET['AccGrp']))
	$_POST['AccGrp'] = $_GET['AccGrp'];

//----------------------------------------------------------------------------------------------------

function display_type ($type, $typename, $from, $to, $begin, $end, $compare, $convert, $dimension=0, $dimension2=0, $drilldown, $path_to_root) {
	global $levelptr, $k;
		
	$code_per_balance = 0;
	$code_acc_balance = 0;
	$per_balance_total = 0;
	$acc_balance_total = 0;
	unset($totals_arr);
	$totals_arr = array();
	
	//Get Accounts directly under this group/type
	$result = get_gl_accounts(null, null, $type);	

	while ($account=db_fetch($result)) {
		$per_balance = get_gl_trans_from_to($from, $to, $account['account_code'], $dimension, $dimension2);

		if ($compare == 2)
			$acc_balance = get_budget_trans_from_to($begin, $end, $account['account_code'], $dimension, $dimension2);
		else
			$acc_balance = get_gl_trans_from_to($begin, $end, $account['account_code'], $dimension, $dimension2);
		if (!$per_balance && !$acc_balance)
			continue;
		
		if ($drilldown && $levelptr == 0) {
			$url = "<a href='".$path_to_root."/gl/inquiry/gl_account_inquiry.php?TransFromDate=".$from."&TransToDate=".$to."&Dimension=".$dimension."&Dimension2=".$dimension2."&account=".$account['account_code']."'>".$account['account_code']." ". $account['account_name'].'</a>';
				
			start_row("class='stockmankobg'");
			label_cell($url);
			amount_cell($per_balance * $convert);
			amount_cell($acc_balance * $convert);
			amount_cell(Achieve($per_balance, $acc_balance));
			end_row();
		}
			
		$code_per_balance += $per_balance;
		$code_acc_balance += $acc_balance;
	}

	$levelptr = 1;
	
	//Get Account groups/types under this group/type
	$result = get_account_types(false, false, $type);
	while ($accounttype=db_fetch($result)) {	
		$totals_arr = display_type($accounttype['id'], $accounttype['name'], $from, $to, $begin, $end, $compare, $convert, $dimension, $dimension2, $drilldown, $path_to_root);
		$per_balance_total += $totals_arr[0];
		$acc_balance_total += $totals_arr[1];
	}

	//Display Type Summary if total is != 0 
	if (($code_per_balance + $per_balance_total + $code_acc_balance + $acc_balance_total) != 0) {
		if ($drilldown && $type == $_POST['AccGrp']) {
			start_row("class='inquirybg' style='font-weight:bold'");
			label_cell(_('Total').' '.$typename);
			amount_cell(($code_per_balance + $per_balance_total) * $convert);
			amount_cell(($code_acc_balance + $acc_balance_total) * $convert);
			amount_cell(Achieve(($code_per_balance + $per_balance_total), ($code_acc_balance + $acc_balance_total)));
			end_row();
		}
		//START Patch#1 : Display  only direct child types
		$acctype1 = get_account_type($type);
		$parent1 = $acctype1['parent'];
		if ($drilldown && $parent1 == $_POST['AccGrp'])
		//END Patch#2		
		//elseif ($drilldown && $type != $_POST['AccGrp'])
		{
			$url = "<a href='".$path_to_root."/gl/inquiry/profit_loss.php?TransFromDate=".$from."&TransToDate=".$to."&Compare=".$compare."&Dimension=".$dimension."&Dimension2=".$dimension2."&AccGrp=".$type."'>".$type." ".$typename.'</a>';
				
			alt_table_row_color($k);
			label_cell($url);
			amount_cell(($code_per_balance + $per_balance_total) * $convert);
			amount_cell(($code_acc_balance + $acc_balance_total) * $convert);
			amount_cell(Achieve(($code_per_balance + $per_balance_total), ($code_acc_balance + $acc_balance_total)));
			end_row();
		}
	}
	
	$totals_arr[0] = $code_per_balance + $per_balance_total;
	$totals_arr[1] = $code_acc_balance + $acc_balance_total;
	return $totals_arr;
}	
	
function Achieve($d1, $d2) {
	if ($d1 == 0 && $d2 == 0)
		return 0;
	elseif ($d2 == 0)
		return 999;
	$ret = ($d1 / $d2 * 100.0);
	if ($ret > 999)
		$ret = 999;
	return $ret;
}

function inquiry_controls() {
	global $compare_types;

	$dim = get_company_pref('use_dimension');
	start_table(TABLESTYLE_NOBORDER);
	
	$date = today();
	if (!isset($_POST['TransToDate']))
		$_POST['TransToDate'] = end_month($date);
	if (!isset($_POST['TransFromDate']))
		$_POST['TransFromDate'] = add_days(end_month($date), -user_transaction_days());
	date_cells(_('From:'), 'TransFromDate');
	date_cells(_('To:'), 'TransToDate');
	
	echo '<td>'._('Compare to').":</td>\n";
	echo '<td>';
	echo array_selector('Compare', null, $compare_types);
	echo "</td>\n";	

	if ($dim >= 1)
		dimensions_list_cells(_('Dimension').' 1:', 'Dimension', null, true, ' ', false, 1);
	if ($dim > 1)
		dimensions_list_cells(_('Dimension').' 2:', 'Dimension2', null, true, ' ', false, 2);
	
	submit_cells('Show',_('Show'), '', '', 'default');
	end_table();

	hidden('AccGrp');
}

//----------------------------------------------------------------------------------------------------

function display_profit_and_loss($compare) {
	global $path_to_root, $compare_types;

	if (!isset($_POST['Dimension']))
		$_POST['Dimension'] = 0;
	if (!isset($_POST['Dimension2']))
		$_POST['Dimension2'] = 0;
	$dimension = $_POST['Dimension'];
	$dimension2 = $_POST['Dimension2'];

	$from = $_POST['TransFromDate'];
	$to = $_POST['TransToDate'];
	
	if (isset($_POST['AccGrp']) && (strlen($_POST['AccGrp']) > 0))
		$drilldown = 1; // Deeper Level
	else
		$drilldown = 0; // Root level
	
	if ($compare == 0 || $compare == 2) {
		$end = $to;
		if ($compare == 2)
			$begin = $from;
		else
			$begin = begin_fiscalyear();
	}
	elseif ($compare == 1) {
		$begin = add_months($from, -12);
		$end = add_months($to, -12);
	}
	
	div_start('pl_tbl');

	start_table(TABLESTYLE, "width='50%'");

	$tableheader =  "<tr>
		<td class='tableheader'>"._("Group/Account Name")."</td>
		<td class='tableheader'>"._("Period") . "</td>
		<td class='tableheader'>".$compare_types[$compare]."</td>
		<td class='tableheader'>"._("Achieved %")."</td>
	</tr>";	
	
	if (!$drilldown) {//Root Level
		$salesper = 0.0;
		$salesacc = 0.0;	
	
		//Get classes for PL
		$classresult = get_account_classes(false, 0);
		while ($class = db_fetch($classresult)) {
			$class_per_total = 0;
			$class_acc_total = 0;
			$convert = get_class_type_convert($class['ctype']); 		
			
			//Print Class Name	
			table_section_title($class['class_name'],4);	
			echo $tableheader;
			
			//Get Account groups/types under this group/type
			$typeresult = get_account_types(false, $class['cid'], -1);
			$k = 0; // row color
			while ($accounttype=db_fetch($typeresult)) {
				$TypeTotal = display_type($accounttype['id'], $accounttype['name'], $from, $to, $begin, $end, $compare, $convert, $dimension, $dimension2, $drilldown, $path_to_root);
				$class_per_total += $TypeTotal[0];
				$class_acc_total += $TypeTotal[1];

				if ($TypeTotal[0] != 0 || $TypeTotal[1] != 0 ) {
					$url = "<a href='".$path_to_root."/gl/inquiry/profit_loss.php?TransFromDate=".$from."&TransToDate=".$to."&Compare=".$compare."&Dimension=".$dimension."&Dimension2=".$dimension2."&AccGrp=".$accounttype['id']."'>".$accounttype['id']." ".$accounttype['name'].'</a>';
						
					alt_table_row_color($k);
					label_cell($url);
					amount_cell($TypeTotal[0] * $convert);
					amount_cell($TypeTotal[1] * $convert);
					amount_cell(Achieve($TypeTotal[0], $TypeTotal[1]));
					end_row();
				}
			}
			
			//Print Class Summary
			
			start_row("class='inquirybg' style='font-weight:bold'");
			label_cell(_('Total') . ' ' . $class['class_name']);
			amount_cell($class_per_total * $convert);
			amount_cell($class_acc_total * $convert);
			amount_cell(Achieve($class_per_total, $class_acc_total));
			end_row();			
			
			$salesper += $class_per_total;
			$salesacc += $class_acc_total;
		}
		
		start_row("class='inquirybg' style='font-weight:bold'");
		label_cell(_('Calculated Return'));
		amount_cell($salesper *-1);
		amount_cell($salesacc * -1);
		amount_cell(achieve($salesper, $salesacc));
		end_row();		

	}
	else {
		//Level Pointer : Global variable defined in order to control display of root 
		global $levelptr;
		$levelptr = 0;
		
		$accounttype = get_account_type($_POST['AccGrp']);
		$classid = $accounttype['class_id'];
		$class = get_account_class($classid);
		$convert = get_class_type_convert($class['ctype']); 
		
		//Print Class Name	
		table_section_title($_POST['AccGrp'].' '.get_account_type_name($_POST['AccGrp']),4);	
		echo $tableheader;
		
		$classtotal = display_type($accounttype['id'], $accounttype['name'], $from, $to, $begin, $end, $compare, $convert, $dimension, $dimension2, $drilldown, $path_to_root);
	}
	end_table(); // outer table
	hyperlink_params($_SERVER['PHP_SELF'], _('Back'), 'TransFromDate='. $from . '&TransToDate=' . $to . '&Dimension=' . $dimension . '&Dimension2=' . $dimension2);
	div_end();
}

//----------------------------------------------------------------------------------------------------

start_form();

inquiry_controls();

display_profit_and_loss(get_post('Compare'));

end_form();

end_page(false, true);