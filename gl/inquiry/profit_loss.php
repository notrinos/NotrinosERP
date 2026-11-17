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
$page_security = 'SA_GLANALYTIC';
$path_to_root = '../..';

include_once($path_to_root.'/includes/session.inc');

include_once($path_to_root.'/includes/date_functions.inc');
include_once($path_to_root.'/includes/ui.inc');
include_once($path_to_root.'/includes/data_checks.inc');

include_once($path_to_root.'/gl/includes/gl_db.inc');
include_once($path_to_root.'/tracking/includes/tracking_budget_basis.inc');

$js = '';
if (user_use_date_picker())
	$js = get_js_date_picker();

page(_($help_context = 'Profit & Loss Drilldown'), false, false, '', $js);

$compare_types = array(_('Accumulated'), _('Period Y-1'), _('Budget'));

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
if (isset($_GET['FullMonth']))
	$_POST['FullMonth'] = $_GET['FullMonth'];

//----------------------------------------------------------------------------------------------------

function display_type ($type, $typename, $from, $to, $begin, $end, $compare, $convert, $dimension, $dimension2, $drilldown) {
	global $path_to_root, $levelptr, $k;
		
	$code_na = 0;
	$na_total = 0;
	$code_per_balance = 0;
	$code_acc_balance = 0;
	$per_balance_total = 0;
	$acc_balance_total = 0;
	unset($totals_arr);
	$totals_arr = array();
	
	//Get Accounts directly under this group/type
	$accounts = chart_master_entity::all_with_types(null, null, $type);

	foreach ($accounts as $account) {
		$per_balance = get_gl_trans_from_to($from, $to, $account['account_code'], $dimension, $dimension2);

		$budget_na = false;
		if ($compare == 2) {
			// DIM-BUD-001: whole-month comparable budget, never a raw date-range sum.
			$budget = dimension_budget_report_amount($account['account_code'], $dimension, $dimension2, date2sql($begin), date2sql($end), !empty($_POST['FullMonth']));
			$acc_balance = $budget['amount'];
			$budget_na = !$budget['available'];
		}
		else
			$acc_balance = get_gl_trans_from_to($begin, $end, $account['account_code'], $dimension, $dimension2);
		if (!$per_balance && !$acc_balance)
			continue;
		if ($budget_na)
			$code_na++;
		
		if ($drilldown && $levelptr == 0) {
			$url = "<a href='".$path_to_root.'/gl/inquiry/gl_account_inquiry.php?TransFromDate='.$from.'&TransToDate='.$to.'&Dimension='.$dimension.'&Dimension2='.$dimension2.'&account='.$account['account_code']."'>".$account['account_code'].' '. $account['account_name'].'</a>';
			// DIM-BUD-001: same-scope budget drilldown for users who may enter budgets.
			if ($compare == 2 && $_SESSION['wa_current_user']->can_access('SA_BUDGETENTRY'))
				$url .= " <a href='".$path_to_root.'/gl/gl_budget.php?account='.urlencode($account['account_code']).'&dim1='.max(0, (int)$dimension).'&dim2='.max(0, (int)$dimension2)."'>"._('Budget').'</a>';
				
			start_row("class='stockmankobg'");
			label_cell($url);
			amount_cell($per_balance * $convert);
			budget_amount_cell($acc_balance * $convert, $budget_na);
			budget_achieved_cell($per_balance, $acc_balance, $budget_na);
			end_row();
		}
			
		$code_per_balance += $per_balance;
		$code_acc_balance += $acc_balance;
	}

	$levelptr = 1;
	
	//Get Account groups/types under this group/type
	$result = get_account_types(false, false, $type);
	while ($accounttype=db_fetch($result)) {	
		$totals_arr = display_type($accounttype['id'], $accounttype['name'], $from, $to, $begin, $end, $compare, $convert, $dimension, $dimension2, $drilldown);
		$per_balance_total += $totals_arr[0];
		$acc_balance_total += $totals_arr[1];
		$na_total += $totals_arr[2];
	}

	//Display Type Summary if total is != 0 
	if (($code_per_balance + $per_balance_total + $code_acc_balance + $acc_balance_total) != 0) {
		if ($drilldown && $type == $_POST['AccGrp']) {
			start_row("class='inquirybg' style='font-weight:bold'");
			label_cell(_('Total').' '.$typename);
			amount_cell(($code_per_balance + $per_balance_total) * $convert);
			budget_amount_cell(($code_acc_balance + $acc_balance_total) * $convert, ($code_na + $na_total) > 0);
			budget_achieved_cell(($code_per_balance + $per_balance_total), ($code_acc_balance + $acc_balance_total), ($code_na + $na_total) > 0);
			end_row();
		}
		//START Patch#1 : Display  only direct child types
		$acctype1 = get_account_type($type);
		$parent1 = $acctype1['parent'];
		if ($drilldown && $parent1 == $_POST['AccGrp'])
		//END Patch#2		
		//elseif ($drilldown && $type != $_POST['AccGrp'])
		{
			$url = "<a href='".$path_to_root."/gl/inquiry/profit_loss.php?TransFromDate=".$from."&TransToDate=".$to."&Compare=".$compare.budget_view_query()."&Dimension=".$dimension."&Dimension2=".$dimension2."&AccGrp=".$type."'>".$type." ".$typename.'</a>';
				
			alt_table_row_color($k);
			label_cell($url);
			amount_cell(($code_per_balance + $per_balance_total) * $convert);
			budget_amount_cell(($code_acc_balance + $acc_balance_total) * $convert, ($code_na + $na_total) > 0);
			budget_achieved_cell(($code_per_balance + $per_balance_total), ($code_acc_balance + $acc_balance_total), ($code_na + $na_total) > 0);
			end_row();
		}
	}
	
	$totals_arr[0] = $code_per_balance + $per_balance_total;
	$totals_arr[1] = $code_acc_balance + $acc_balance_total;
	$totals_arr[2] = $code_na + $na_total;
	return $totals_arr;
}	

/**
 * Query-string fragment that keeps the full-month budget view across drilldown links.
 *
 * @return string
 */
function budget_view_query() {
	return empty($_POST['FullMonth']) ? '' : '&FullMonth=1';
}

/**
 * Explain, above the table, why the monthly budget is N/A or which basis is shown (DIM-BUD-001).
 *
 * @param string $begin Budget period begin (user date)
 * @param string $end Budget period end (user date)
 * @param bool $full_month Full-month budget view requested
 * @return void
 */
function display_budget_period_note($begin, $end, $full_month) {
	$period = dimension_budget_period(date2sql($begin), date2sql($end));
	// DIM-BUD-002: name the approved budget version when a fiscal year compares against one.
	foreach (dimension_budget_version_source_labels(date2sql($begin), date2sql($end)) as $label)
		display_note($label, 0, 1);
	if ($period['comparable'])
		return;
	if ($full_month && $period['reason'] !== 'invalid') {
		display_note(_('Budget shows every month the period touches; the actual amounts cover only the selected dates, so the achieved percentage is not a like-for-like comparison.'), 0, 1);
		return;
	}
	display_note(dimension_budget_reason_text($period['reason']).($period['whole_from'] !== null ? ' '.sprintf(_('Whole months available: %s to %s.'), sql2date($period['whole_from']), sql2date($period['whole_to'])) : ''), 0, 1);
}

/**
 * Budget amount cell: N/A when the comparable budget is unavailable (partial period, no or incomplete months).
 *
 * @param float $amount Budget amount
 * @param bool $na True when the budget cannot be compared
 * @return void
 */
function budget_amount_cell($amount, $na) {
	if ($na)
		label_cell(_('N/A'), "align='right'");
	else
		amount_cell($amount);
}

/**
 * Achieved percentage cell: N/A whenever the budget side is not comparable.
 *
 * @param float $actual Actual
 * @param float $budget Budget
 * @param bool $na True when the budget cannot be compared
 * @return void
 */
function budget_achieved_cell($actual, $budget, $na) {
	if ($na)
		label_cell(_('N/A'), "align='right'");
	else
		amount_cell(Achieve($actual, $budget));
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
	start_row();
	
	$date = today();
	if (!isset($_POST['TransToDate']))
		$_POST['TransToDate'] = end_month($date);
	if (!isset($_POST['TransFromDate']))
		$_POST['TransFromDate'] = add_days(end_month($date), -user_transaction_days());
	date_cells(_('From:'), 'TransFromDate');
	date_cells(_('To:'), 'TransToDate');
	
	filter_cell_open(_('Compare to'));
	echo array_selector('Compare', null, $compare_types);
	filter_cell_close();
	check_cells(_('Full-month budget:'), 'FullMonth', null, false, _('Show the budget of every month the period touches (actual covers only the selected dates)'));

	// DIM-LFC-002: historical axis filters are independent of the current posting preference.
	dimensions_history_list_cells(dimension_axis_label_html(1, ':'), 'Dimension', null, true, sprintf(_('%s: all'), dimension_axis_label_html(1)));
	dimensions_history_list_cells(dimension_axis_label_html(2, ':'), 'Dimension2', null, true, sprintf(_('%s: all'), dimension_axis_label_html(2)));
	
	submit_cells('Show',_('Show'), '', '', 'default');
	end_row();
	end_table();

	hidden('AccGrp');
}

//----------------------------------------------------------------------------------------------------

function display_profit_and_loss($compare) {
	global $path_to_root, $compare_types;

	$dimension = empty($_POST['Dimension']) ? 0 : $_POST['Dimension'];
	$dimension2 = empty($_POST['Dimension2']) ? 0 : $_POST['Dimension2'];

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
	if ($compare == 2)
		display_budget_period_note($begin, $end, !empty($_POST['FullMonth']));

	start_table(TABLESTYLE, "width='50%'");

	$tableheader = "<tr>
		<td class='tableheader'>"._('Group/Account Name')."</td>
		<td class='tableheader'>"._('Period') . "</td>
		<td class='tableheader'>".$compare_types[$compare]."</td>
		<td class='tableheader'>"._('Achieved %')."</td>
	</tr>";	
	
	if (!$drilldown) {//Root Level
		$salesper = 0.0;
		$salesacc = 0.0;	
		$sales_na = 0;
	
		//Get classes for PL
		$classresult = get_account_classes(false, 0);
		while ($class = db_fetch($classresult)) {
			$class_per_total = 0;
			$class_acc_total = 0;
			$class_na = 0;
			$convert = get_class_type_convert($class['ctype']); 		
			
			//Print Class Name	
			table_section_title($class['class_name'],4);	
			echo $tableheader;
			
			//Get Account groups/types under this group/type
			$typeresult = get_account_types(false, $class['cid'], -1);
			$k = 0; // row color
			while ($accounttype=db_fetch($typeresult)) {
				$TypeTotal = display_type($accounttype['id'], $accounttype['name'], $from, $to, $begin, $end, $compare, $convert, $dimension, $dimension2, $drilldown);
				$class_per_total += $TypeTotal[0];
				$class_acc_total += $TypeTotal[1];
				$class_na += $TypeTotal[2];

				if ($TypeTotal[0] != 0 || $TypeTotal[1] != 0 ) {
					$url = "<a href='".$path_to_root.'/gl/inquiry/profit_loss.php?TransFromDate='.$from.'&TransToDate='.$to.'&Compare='.$compare.budget_view_query().'&Dimension='.$dimension.'&Dimension2='.$dimension2.'&AccGrp='.$accounttype['id']."'>".$accounttype['id'].' '.$accounttype['name'].'</a>';
						
					alt_table_row_color($k);
					label_cell($url);
					amount_cell($TypeTotal[0] * $convert);
					budget_amount_cell($TypeTotal[1] * $convert, $TypeTotal[2] > 0);
					budget_achieved_cell($TypeTotal[0], $TypeTotal[1], $TypeTotal[2] > 0);
					end_row();
				}
			}
			
			//Print Class Summary
			
			start_row("class='inquirybg' style='font-weight:bold'");
			label_cell(_('Total').' '.$class['class_name']);
			amount_cell($class_per_total * $convert);
			budget_amount_cell($class_acc_total * $convert, $class_na > 0);
			budget_achieved_cell($class_per_total, $class_acc_total, $class_na > 0);
			end_row();			
			
			$salesper += $class_per_total;
			$salesacc += $class_acc_total;
			$sales_na += $class_na;
		}
		
		start_row("class='inquirybg' style='font-weight:bold'");
		label_cell(_('Calculated Return'));
		amount_cell($salesper *-1);
		budget_amount_cell($salesacc * -1, $sales_na > 0);
		budget_achieved_cell($salesper, $salesacc, $sales_na > 0);
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
		
		$classtotal = display_type($accounttype['id'], $accounttype['name'], $from, $to, $begin, $end, $compare, $convert, $dimension, $dimension2, $drilldown);
	}
	end_table(); // outer table
	hyperlink_params($_SERVER['PHP_SELF'], _('Back'), 'TransFromDate='.$from.'&TransToDate='.$to.'&Dimension='.$dimension.'&Dimension2='.$dimension2);
	div_end();
}

//----------------------------------------------------------------------------------------------------

start_form();

inquiry_controls();

display_profit_and_loss(get_post('Compare'));

end_form();

end_page(false, true);
