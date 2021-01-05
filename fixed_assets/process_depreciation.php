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
$page_security = 'SA_DEPRECIATION';
$path_to_root = '..';

include_once($path_to_root.'/includes/session.inc');

include_once($path_to_root.'/includes/date_functions.inc');
include_once($path_to_root.'/admin/db/fiscalyears_db.inc');
include_once($path_to_root.'/includes/ui.inc');
include_once($path_to_root.'/includes/data_checks.inc');
include_once($path_to_root.'/includes/ui/items_cart.inc');

include_once($path_to_root.'/fixed_assets/includes/depreciation.inc');
include_once($path_to_root.'/fixed_assets/includes/fixed_assets_db.inc');

$js = '';
if (user_use_date_picker())
  $js .= get_js_date_picker();

page(_($help_context = 'Process Depreciation'), false, false, '', $js);

//---------------------------------------------------------------------------------------------

function check_data() {
	$myrow = get_item($_POST['stock_id']);

	if ($_POST['months'] > depreciation_months($myrow['depreciation_date'])) {
		display_error(_('The number of months is greater than the timespan between the depreciation start and the end of the fiscal year.'));
		set_focus('months');
		return false;
	}

	return true;
}

//---------------------------------------------------------------------------------------------

function handle_submit() {
	if (!check_data())
		return;

	$item = get_item($_POST['stock_id']);
	$period = get_company_pref('depreciation_period'); 
	$gl_rows = compute_gl_rows_for_depreciation($item, $_POST['months'], $period);
	$trans_no = process_fixed_asset_depreciation($_POST['stock_id'], $gl_rows, $_POST['refline'], $_POST['memo_']);

	meta_forward($_SERVER['PHP_SELF'], 'AddedID='.$trans_no);
}

//---------------------------------------------------------------------------------------------

if (get_post('process'))
	handle_submit();

//---------------------------------------------------------------------------------------------

if (isset($_GET['AddedID'])) {
	$trans_no = $_GET['AddedID'];
	$trans_type = ST_JOURNAL;

	display_notification(_('The fixed asset has been depreciated for this year'));

	display_note(get_gl_view_str($trans_type, $trans_no, _('View the GL &Postings for this Depreciation')), 1, 0);

	hyperlink_no_params($_SERVER['PHP_SELF'], _('Depreciate &Another Fixed Asset'));

	display_footer_exit();
}

//--------------------------------------------------------------------------------------

check_db_has_depreciable_fixed_assets(_('There are no fixed assets that could be depreciated.'));

//---------------------------------------------------------------------------------------------

function show_gl_rows() {

	$item = get_item($_POST['stock_id']);

	hidden('stock_id');
	hidden('months');
	hidden('refline');
	hidden('memo_');

	start_table(TABLESTYLE, 'width=40%');
	$th = array(_('Item'), _('Date'), _('Account'), _('Debit'), _('Credit'));

	table_header($th);
	$k = 0; //row colour counter

	$period = get_company_pref('depreciation_period'); 
	$gl_rows = compute_gl_rows_for_depreciation($item, $_POST['months'], $period);

	foreach($gl_rows as $myrow) {
		alt_table_row_color($k);
		label_cell($item['stock_id']);
		label_cell($myrow['date']);
		label_cell($item['cogs_account'].' '.get_gl_account_name($item['cogs_account']));
		amount_cell($myrow['value']);
		label_cell('');
		end_row();

		alt_table_row_color($k);
		label_cell($item['stock_id']);
		label_cell($myrow['date']);
		label_cell($item['adjustment_account'].' '.get_gl_account_name($item['adjustment_account']));
		label_cell('');
		amount_cell($myrow['value']);
		end_row();
	}

	end_table(1);

	submit_center('process', _('Process Depreciation'), true, false);
}

function show_gl_controls() {
	global $Ajax;

	check_db_has_depreciable_fixed_assets('There are no active fixed asset defined in the system.');
 
	start_table(TABLESTYLE_NOBORDER);
	start_row();
	stock_depreciable_fa_list_cells(_('Select an item:'), 'stock_id', null, false, true);
	 end_row();
	end_table();

	$myrow = get_item($_POST['stock_id']);

	if (list_updated('stock_id') || !isset($_POST['months'])) {
		//$_POST['depreciation_start'] = sql2date($myrow['depreciation_start']);
		$_POST['months'] = depreciation_months($myrow['depreciation_date']);
		unset($_POST['memo_']);
	}

	$Ajax->activate('depreciation_date');
	$Ajax->activate('months');
	$Ajax->activate('memo_');

	start_table(TABLESTYLE2);

	if (!isset($_POST['date']))
		$_POST['date'] = Today();

	$start = next_depreciation_date($myrow['depreciation_date']);
	$start_text =  _(date('F', $start)).' '.date('Y', $start);

	//date_row(_('Starting from month').':', 'depreciation_start', '', null, 0, 0, 0, null, true);
	label_row(_('Starting from month').':', $start_text, null, null, 0, 'depreciation_date');
	$period = get_company_pref('depreciation_period'); 
	if ($period != FA_YEARLY)
		text_row(_('Period').':', 'months', null, 4, 3, null, null, _('months'));
	else {
		label_row(_('Period').':', '1 year');
		hidden ('months');
	}
	refline_list_row(_('Reference line:'), 'refline', ST_JOURNAL, null, false, true);
	textarea_row(_('Memo:'), 'memo_', null, 40, 4);

	end_table(1);

	submit_center_first('show', _('Show GL Rows'), true, false);
	submit_center_last('process', _('Process Depreciation'), true, false);
}

//---------------------------------------------------------------------------------------------

start_form();

if (isset($_POST['show']) && check_data())
	show_gl_rows();
else
	show_gl_controls();

end_form();

end_page();
