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
$page_security = 'SA_BUDGETENTRY';
$path_to_root = "..";
include_once($path_to_root . "/includes/session.inc");

add_js_file('budget.js');

page(_($help_context = "Budget Entry"));

include_once($path_to_root . "/includes/ui.inc");
include_once($path_to_root . "/gl/includes/gl_db.inc");
include_once($path_to_root . "/includes/data_checks.inc");
include_once($path_to_root . "/admin/db/fiscalyears_db.inc");


check_db_has_gl_account_groups(_("There are no account groups defined. Please define at least one account group before entering accounts."));

//-------------------------------------------------------------------------------------

if (isset($_POST['add']) || isset($_POST['delete']))
{
	begin_transaction();

	for ($i = 0, $da = $_POST['begin']; date1_greater_date2($_POST['end'], $da); $i++)
	{
		if (isset($_POST['add']))
			add_update_gl_budget_trans($da, $_POST['account'], $_POST['dim1'], $_POST['dim2'], input_num('amount'.$i));
		else
			delete_gl_budget_trans($da, $_POST['account'], $_POST['dim1'], $_POST['dim2']);
		$da = add_months($da, 1);
	}
	commit_transaction();

	if (isset($_POST['add']))
		display_notification_centered(_("The Budget has been saved."));
	else
		display_notification_centered(_("The Budget has been deleted."));

	$Ajax->activate('budget_tbl');
}
if (isset($_POST['submit']) || isset($_POST['update']))
	$Ajax->activate('budget_tbl');

//-------------------------------------------------------------------------------------

start_form();

if (db_has_gl_accounts())
{
	$dim = get_company_pref('use_dimension');
	start_table(TABLESTYLE2);
	fiscalyears_list_row(_("Fiscal Year:"), 'fyear', null);
	gl_all_accounts_list_row(_("Account Code:"), 'account', null);
	if (!isset($_POST['dim1']))
		$_POST['dim1'] = 0;
	if (!isset($_POST['dim2']))
		$_POST['dim2'] = 0;
    if ($dim == 2)
    {
		dimensions_list_row(_("Dimension")." 1", 'dim1', $_POST['dim1'], true, null, false, 1);
		dimensions_list_row(_("Dimension")." 2", 'dim2', $_POST['dim2'], true, null, false, 2);
	}
	elseif ($dim == 1)
	{
		dimensions_list_row(_("Dimension"), 'dim1', $_POST['dim1'], true, null, false, 1);
		hidden('dim2', 0);
	}
	else
	{
		hidden('dim1', 0);
		hidden('dim2', 0);
	}
	submit_row('submit', _("Get"), true, '', '', true);
	end_table(1);
	div_start('budget_tbl');
	start_table(TABLESTYLE2);
	$showdims = (($dim == 1 && $_POST['dim1'] == 0) ||
		($dim == 2 && $_POST['dim1'] == 0 && $_POST['dim2'] == 0));
	if ($showdims)
		$th = array(_("Period"), _("Amount"), _("Dim. incl."), _("Last Year"));
	else
		$th = array(_("Period"), _("Amount"), _("Last Year"));
	table_header($th);
	$year = $_POST['fyear'];
	if (get_post('update') == '') {
		$fyear = get_fiscalyear($year);
		$_POST['begin'] = sql2date($fyear['begin']);
		$_POST['end'] = sql2date($fyear['end']);
	}
	hidden('begin');
	hidden('end');
	$total = $btotal = $ltotal = 0;
	for ($i = 0, $date_ = $_POST['begin']; date1_greater_date2($_POST['end'], $date_); $i++)
	{
		start_row();
		if (get_post('update') == '')
			$_POST['amount'.$i] = number_format2(get_only_budget_trans_from_to(
				$date_, $date_, $_POST['account'], $_POST['dim1'], $_POST['dim2']), 0);

		label_cell($date_);
		amount_cells(null, 'amount'.$i, null, 15, null, 0);
		if ($showdims)
		{
			$d = get_budget_trans_from_to($date_, $date_, $_POST['account'], $_POST['dim1'], $_POST['dim2']);
			label_cell(number_format2($d, 0), "nowrap align=right");
			$btotal += $d;
		}
		$lamount = get_gl_trans_from_to(add_years($date_, -1), add_years(end_month($date_), -1), $_POST['account'], $_POST['dim1'], $_POST['dim2']);
		$total += input_num('amount'.$i);
		$ltotal += $lamount;
		label_cell(number_format2($lamount, 0), "nowrap align=right");
		$date_ = add_months($date_, 1);
		end_row();
	}
	start_row();
	label_cell("<b>"._("Total")."</b>");
	label_cell(number_format2($total, 0), 'align=right style="font-weight:bold"', 'Total');
	if ($showdims)
		label_cell("<b>".number_format2($btotal, 0)."</b>", "nowrap align=right");
	label_cell("<b>".number_format2($ltotal, 0)."</b>", "nowrap align=right");
	end_row();
	end_table(1);
	div_end();
	submit_center_first('update', _("Update"), '', null);
	submit('add', _("Save"), true, '', 'default');
	submit_center_last('delete', _("Delete"), '', true);
}
end_form();

end_page();

