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
$path_to_root='../..';

include_once($path_to_root . '/includes/session.inc');

include_once($path_to_root . '/includes/date_functions.inc');
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/includes/data_checks.inc');

include_once($path_to_root . '/gl/includes/gl_db.inc');

$js = '';
if (user_use_date_picker())
	$js = get_js_date_picker();

page(_($help_context = 'Balance Sheet Drilldown'), false, false, '', $js);

//----------------------------------------------------------------------------------------------------
// Ajax updates

if (get_post('Show')) 
	$Ajax->activate('balance_tbl');

if (isset($_GET['TransFromDate']))
	$_POST['TransFromDate'] = $_GET['TransFromDate'];	
if (isset($_GET['TransToDate']))
	$_POST['TransToDate'] = $_GET['TransToDate'];
if (isset($_GET['Dimension']))
	$_POST['Dimension'] = $_GET['Dimension'];
if (isset($_GET['Dimension2']))
	$_POST['Dimension2'] = $_GET['Dimension2'];
if (isset($_GET['AccGrp']))
	$_POST['AccGrp'] = $_GET['AccGrp'];	

//----------------------------------------------------------------------------------------------------

function display_type ($type, $typename, $from, $to, $convert, $dimension, $dimension2, $drilldown) {
	global $path_to_root, $levelptr, $k;
	
	$acctstotal = 0;
	$typestotal = 0;
	
	//Get Accounts directly under this group/type
	$result = get_gl_accounts(null, null, $type);	
		
	while ($account=db_fetch($result)) {
		$net_balance = get_gl_trans_from_to('', $to, $account['account_code'], $dimension, $dimension2);
		if (!$net_balance)
			continue;
		
		if ($drilldown && $levelptr == 0) {
			$url = "<a href='".$path_to_root."/gl/inquiry/gl_account_inquiry.php?TransFromDate=".$from."&TransToDate=".$to."&Dimension=".$dimension."&Dimension2=".$dimension2."&account=".$account['account_code']."'>".$account['account_code']." ".$account['account_name'].'</a>';                
				
			start_row("class='stockmankobg'");
			label_cell($url);
			amount_cell(($net_balance) * $convert);
			end_row();
		}
		
		$acctstotal += $net_balance;
	}	
	
	$levelptr = 1;

	//Get Account groups/types under this group/type
	$result = get_account_types(false, false, $type);
	while ($accounttype=db_fetch($result)) {
		$typestotal += display_type($accounttype['id'], $accounttype['name'], $from, $to, 
			$convert, $dimension, $dimension2, $drilldown);
	}

	//Display Type Summary if total is != 0  
	if (($acctstotal + $typestotal) != 0) {
		if ($drilldown && $type == $_POST['AccGrp']) {
			start_row("class='inquirybg' style='font-weight:bold'");
			label_cell(_('Total') . ' ' . $typename);
			amount_cell(($acctstotal + $typestotal) * $convert);
			end_row();
		}
		//START Patch#1 : Display  only direct child types
		$acctype1 = get_account_type($type);
		$parent1 = $acctype1['parent'];
		if ($drilldown && $parent1 == $_POST['AccGrp']) {
		//END Patch#2
			$url = "<a href='".$path_to_root."/gl/inquiry/balance_sheet.php?TransFromDate=".$from."&TransToDate=".$to."&Dimension=".$dimension."&Dimension2=".$dimension2."&AccGrp=".$type."'>".$type." ".$typename.'</a>';
				
			alt_table_row_color($k);
			label_cell($url);
			amount_cell(($acctstotal + $typestotal) * $convert);
			end_row();
		}
	}
	return ($acctstotal + $typestotal);
}	
	
function inquiry_controls() {
	$dim = get_company_pref('use_dimension');
	start_table(TABLESTYLE_NOBORDER);
	date_cells(_('As at:'), 'TransToDate');
	if ($dim >= 1)
		dimensions_list_cells(_('Dimension').' 1:', 'Dimension', null, true, ' ', false, 1);
	if ($dim > 1)
		dimensions_list_cells(_('Dimension').' 2:', 'Dimension2', null, true, ' ', false, 2);
	submit_cells('Show',_('Show'),'','', 'default');
	end_table();

	hidden('TransFromDate');
	hidden('AccGrp');
}

function display_balance_sheet() {
	global $path_to_root;
	
	$to = $_POST['TransToDate'];
	$from = get_fiscalyear_begin_for_date($to);

	if (!isset($_POST['Dimension']))
		$_POST['Dimension'] = 0;
	if (!isset($_POST['Dimension2']))
		$_POST['Dimension2'] = 0;
	$dimension = $_POST['Dimension'];
	$dimension2 = $_POST['Dimension2'];
	$lconvert = $econvert = 1;
	if (isset($_POST['AccGrp']) && (strlen($_POST['AccGrp']) > 0))
		$drilldown = 1; // Deeper Level
	else
		$drilldown = 0; // Root level	

	div_start('balance_tbl');
	
	start_table(TABLESTYLE, "width='30%'");
		
	if (!$drilldown) {//Root Level
		$equityclose = 0.0;
		$lclose = 0.0; 
		$calculateclose = 0.0;		

		//Get classes for BS
		$classresult = get_account_classes(false, 1);
	
		while ($class = db_fetch($classresult)) {
			$classclose = 0.0;
			$convert = get_class_type_convert($class['ctype']);
			$ctype = $class['ctype'];

			//Print Class Name	
			table_section_title($class['class_name']);
			
			//Get Account groups/types under this group/type
			$typeresult = get_account_types(false, $class['cid'], -1);
			
			$k = 0;
			while ($accounttype=db_fetch($typeresult)) {
				$TypeTotal = display_type($accounttype['id'], $accounttype['name'], $from, $to, 
						$convert, $dimension, $dimension2, $drilldown);	
				//Print Summary 
				if ($TypeTotal != 0 ) {
					$url = "<a href='".$path_to_root."/gl/inquiry/balance_sheet.php?TransFromDate=".$from."&TransToDate=".$to."&Dimension=".$dimension."&Dimension2=".$dimension2."&AccGrp=".$accounttype['id']."'>".$accounttype['id']." ".$accounttype['name'].'</a>';	
					alt_table_row_color($k);
					label_cell($url);
					amount_cell($TypeTotal * $convert);
					end_row();
				}
				$classclose += $TypeTotal;
			}				

			//Print Class Summary
			start_row("class='inquirybg' style='font-weight:bold'");
			label_cell(_('Total') . ' ' . $class['class_name']);
			amount_cell($classclose * $convert);
			end_row();		
			
			if ($ctype == CL_EQUITY) {
				$equityclose += $classclose;
				$econvert = $convert;
			}
			if ($ctype == CL_LIABILITIES) {
				$lclose += $classclose;
				$lconvert = $convert;
			}
	
			$calculateclose += $classclose;
		}
		
		if ($lconvert == 1)
			$calculateclose *= -1;
		//Final Report Summary
		$url = "<a href='$path_to_root/gl/inquiry/profit_loss.php?TransFromDate=".$from."&TransToDate=".$to."&Dimension=".$dimension."&Dimension2=".$dimension2."&Compare=0'>"._('Calculated Return').'</a>';		
		
		start_row("class='inquirybg' style='font-weight:bold'");
		label_cell($url);
		amount_cell($calculateclose);
		end_row();		
		
		start_row("class='inquirybg' style='font-weight:bold'");
		label_cell(_('Total')." "._('Liabilities')._(' and ')._('Equities'));
		amount_cell($lclose * $lconvert + $equityclose * $econvert + $calculateclose);
		end_row();
	}
	else { //Drill Down
		//Level Pointer : Global variable defined in order to control display of root 
		global $levelptr;
		$levelptr = 0;
		
		$accounttype = get_account_type($_POST['AccGrp']);
		$classid = $accounttype['class_id'];
		$class = get_account_class($classid);
		$convert = get_class_type_convert($class['ctype']); 
		
		//Print Class Name	
		table_section_title($_POST['AccGrp']. ' ' . get_account_type_name($_POST['AccGrp']));	
		
		$classclose = display_type($accounttype['id'], $accounttype['name'], $from, $to, 
			$convert, $dimension, $dimension2, $drilldown);
	}
	
	end_table(); // outer table
	hyperlink_params($_SERVER['PHP_SELF'], _('Back'), 'TransFromDate='.$from.'&TransToDate='.$to.'&Dimension='.$dimension.'&Dimension2='.$dimension2);
	div_end();
}

//----------------------------------------------------------------------------------------------------

start_form();

inquiry_controls();

display_balance_sheet();

end_form();

end_page(false, true);