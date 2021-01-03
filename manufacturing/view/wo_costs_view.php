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
$page_security = 'SA_MANUFTRANSVIEW';
$path_to_root = "../..";

include_once($path_to_root . "/includes/session.inc");

$js = "";
if ($SysPrefs->use_popup_windows)
	$js .= get_js_open_window(900, 500);
page(_($help_context = "View Work Order Costs"), true, false, "", $js);

include_once($path_to_root . "/includes/date_functions.inc");
include_once($path_to_root . "/includes/data_checks.inc");

include_once($path_to_root . "/manufacturing/includes/manufacturing_db.inc");
include_once($path_to_root . "/manufacturing/includes/manufacturing_ui.inc");

//-------------------------------------------------------------------------------------------------

if ($_GET['trans_no'] != "")
{
	$wo_id = $_GET['trans_no'];
}

//-------------------------------------------------------------------------------------------------
function print_gl_rows($result, $title)
{
	global $systypes_array;

    if (db_num_rows($result))
    {
		table_section_title($title, 7);
		while($myrow = db_fetch($result)) {
			start_row();
			label_cell(sql2date($myrow["tran_date"]));
			label_cell(get_trans_view_str($myrow['type'],$myrow["type_no"], $systypes_array[$myrow['type']]. ' '.$myrow['type_no']));
		    label_cell($myrow['account']);
			label_cell($myrow['account_name']);
			display_debit_or_credit_cells($myrow['amount']);
			label_cell($myrow['memo_']);
			end_row();
		}
	}
}
function display_wo_costs($prod_id)
{
	br(1);
    start_table(TABLESTYLE);

	$th = array(_("Date"), _("Transaction"), _("Account Code"), _("Account Name"),
		_("Debit"), _("Credit"), _("Memo"));

   	table_header($th);

	$productions = get_gl_wo_productions($prod_id, true);
	print_gl_rows($productions, _("Finished Product Requirements"));

	$issues = get_gl_wo_issue_trans($prod_id, -1, true);
	print_gl_rows($issues, _("Additional Material Issues"));

    $costs = get_gl_wo_cost_trans($prod_id, -1, true);
	print_gl_rows($costs, _("Additional Costs"));

	$wo = get_gl_trans(ST_WORKORDER, $prod_id);
	print_gl_rows($wo, _("Finished Product Receival"));
	end_table(1);
}

//-------------------------------------------------------------------------------------------------
display_heading(sprintf(_("Production Costs for Work Order # %d"), $wo_id));

display_wo_details($wo_id, true);

display_wo_costs($wo_id);

//-------------------------------------------------------------------------------------------------

br(2);

end_page(true, false, false, ST_WORKORDER, $wo_id);

