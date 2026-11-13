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
$page_security = 'SA_ACCRUALS';
$path_to_root = '..';

include_once($path_to_root.'/includes/session.inc');
include_once($path_to_root.'/includes/ui/items_cart.inc');
include_once($path_to_root.'/gl/includes/accrual_schedule.inc');

$js = get_js_open_window(800, 500);
if (user_use_date_picker())
	$js .= get_js_date_picker();

include_once($path_to_root.'/includes/ui.inc');

$_SESSION['page_title'] = _($help_context = 'Revenue / Cost Accruals');

page($_SESSION['page_title'], false, false, '', $js);

//--------------------------------------------------------------------------------------------------

if (!isset($_POST['freq']))
	$_POST['freq'] = 3;
// If the import button was selected, we'll process the form here.  (If not, skip to actual content below.)
if (isset($_POST['go']) || isset($_POST['show'])) {
	// DIM-GL-002: one shared planner validates the period split, the revenue/cost classification and
	// the non-P&L accrued-balance control leg before the first period is written. The posting itself
	// is one atomic command, so a refused or concurrent-closed value cannot leave a partial schedule.
	$plan = accrual_schedule_plan(array(
		'date_' => get_post('date_'),
		'amount' => input_num('amount'),
		'periods' => input_num('periods'),
		'freq' => get_post('freq'),
		'acc_act' => get_post('acc_act'),
		'res_act' => get_post('res_act'),
		'dimension_id' => get_post('dimension_id'),
		'dimension2_id' => get_post('dimension2_id'),
		'memo_' => get_post('memo_')));
	if (!$plan['ok']) {
		foreach ($plan['errors'] as $plan_error)
			display_error($plan_error);
		set_focus('date_');
	}
	elseif (isset($_POST['go'])) {
		$posted = accrual_schedule_post($plan);
		if (!$posted['ok']) {
			foreach ($posted['errors'] as $post_error)
				display_error($post_error);
		}
		else {
			display_notification_centered(_('Revenue / Cost Accruals have been processed.'));
			$_POST['date_'] = '';
			$_POST['amount'] = '';
			$_POST['periods'] = '';
		}
	}
	else {
		start_table(TABLESTYLE);
		$dim = get_company_pref('use_dimension');

		$first_cols = array(_('Date'), _('Account'));
		if ($dim == 2)
			$dim_cols = array(_('Dimension').' 1', _('Dimension').' 2');
		elseif ($dim == 1)
			$dim_cols = array(_('Dimension'));
		else
			$dim_cols = array();

		$remaining_cols = array(_('Debit'), _('Credit'), _('Memo'));

		$th = array_merge($first_cols, $dim_cols, $remaining_cols);
		table_header($th);
		$k = 0;
		$acc_act = $plan['accounts']['acc_act'];
		$res_act = $plan['accounts']['res_act'];
		$dimension_id = $plan['classification']['economic']['values'][1];
		$dimension2_id = $plan['classification']['economic']['values'][2];
		foreach ($plan['periods'] as $period) {
			alt_table_row_color($k);
			label_cell($period['date']);
			label_cell($acc_act.' '.get_gl_account_name($acc_act));
			if ($dim > 0)
				label_cell('');
			if ($dim > 1)
				label_cell('');
			display_debit_or_credit_cells($period['amount'] * -1);
			label_cell($plan['memo']);
			alt_table_row_color($k);
			label_cell($period['date']);
			label_cell($res_act.' '.get_gl_account_name($res_act));
			if ($dim > 0)
				label_cell(get_dimension_string($dimension_id, true));
			if ($dim > 1)
				label_cell(get_dimension_string($dimension2_id, true));
			display_debit_or_credit_cells($period['amount']);
			label_cell($plan['memo']);
		}
		end_table(1);
		display_notification_centered(_('Showing GL Transactions.'));
	}
}

function frequency_list_row($label, $name, $selected=null) {
	echo "<tr>\n";
	label_cell($label, "class='label'");
	echo "<td>\n";
	$freq = array(
		'1'=> _('Weekly'),
		'2'=> _('Bi-weekly'),
		'3' => _('Monthly'),
		'4' => _('Quarterly'),
	);
	echo array_selector($name, $selected, $freq);
	echo "</td>\n";
	echo "</tr\n";
}

$dim = get_company_pref('use_dimension');

start_form(false, '', 'accrual');
start_table(TABLESTYLE2);

date_row(_('Date').':', 'date_', _('First date of Accruals'), true, 0, 0, 0, null, true);
start_row();
label_cell(_('Accrued Balance Account').':', "class='label'");
gl_all_accounts_list_cells(null, 'acc_act', null, true, false, false, true);
end_row();
gl_all_accounts_list_row(_('Revenue / Cost Account').':', 'res_act', null, true);

if ($dim >= 1)
	dimensions_list_row(_('Dimension').':', 'dimension_id', null, true, ' ', false, 1);
if ($dim > 1)
	dimensions_list_row(_('Dimension').' 2'.':', 'dimension2_id', null, true, ' ', false, 2);

$url = 'gl/view/accrual_trans.php?act='.get_post('acc_act').'&date='.get_post('date_');
amount_row(_('Amount').':', 'amount', null, null, viewer_link(_('Search Amount'), $url, '', '', ICON_VIEW));

frequency_list_row(_('Frequency').':', 'freq', null);

text_row(_('Periods').':', 'periods', null, 3, 3);
textarea_row(_('Memo').':', 'memo_', null, 35, 3);

end_table(1);
submit_center_first('show', _('Show GL Rows'));
submit_center_last('go', _('Process Accruals'));
submit_js_confirm('go', _('Are you sure you want to post accruals?'));

end_form();

end_page();
