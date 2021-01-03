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
$page_security = 'SA_BANKTRANSVIEW';
$path_to_root="../..";
include_once($path_to_root . "/includes/session.inc");

include_once($path_to_root . "/includes/date_functions.inc");
include_once($path_to_root . "/includes/db_pager.inc");
include_once($path_to_root . "/includes/ui.inc");
include_once($path_to_root . "/includes/data_checks.inc");

include_once($path_to_root . "/gl/includes/gl_db.inc");
include_once($path_to_root . "/includes/banking.inc");

$js = "";
if ($SysPrefs->use_popup_windows)
	$js .= get_js_open_window(800, 500);
if (user_use_date_picker())
	$js .= get_js_date_picker();
page(_($help_context = "Bank Account Inquiry"), isset($_GET['bank_account']) && !isset($_GET['TransAfterDate']), false, "", $js, false, "", true);

check_db_has_bank_accounts(_("There are no bank accounts defined in the system."));

//-----------------------------------------------------------------------------------
// Ajax updates
//
if (get_post('Show'))
{
	$Ajax->activate('trans_tbl');
}
//------------------------------------------------------------------------------------------------

if (isset($_GET['bank_account']))
	$_POST['bank_account'] = $_GET['bank_account'];

start_form();
start_table(TABLESTYLE_NOBORDER);
start_row();
bank_accounts_list_cells(_("Account:"), 'bank_account', null);

date_cells(_("From:"), 'TransAfterDate', '', null, -user_transaction_days());
date_cells(_("To:"), 'TransToDate');

submit_cells('Show',_("Show"),'','', 'default');
end_row();
end_table();
end_form();

//------------------------------------------------------------------------------------------------

if (!isset($_POST['bank_account']))
	$_POST['bank_account'] = "";

$result = get_bank_trans_for_bank_account($_POST['bank_account'], $_POST['TransAfterDate'], $_POST['TransToDate']);	

div_start('trans_tbl');
$act = get_bank_account($_POST["bank_account"]);
display_heading($act['bank_account_name']." - ".$act['bank_curr_code']);

start_table(TABLESTYLE);

$th = array(_("Type"), _("#"), _("Reference"), _("Date"),
	_("Debit"), _("Credit"), _("Balance"), _("Person/Item"), _("Memo"), "", "");
table_header($th);

$bfw = get_balance_before_for_bank_account($_POST['bank_account'], $_POST['TransAfterDate']);

$credit = $debit = 0;
start_row("class='inquirybg' style='font-weight:bold'");
label_cell(_("Opening Balance")." - ".$_POST['TransAfterDate'], "colspan=4");
display_debit_or_credit_cells($bfw);
label_cell("");
label_cell("", "colspan=4");

end_row();
$running_total = $bfw;
if ($bfw > 0 ) 
	$debit += $bfw;
else 
	$credit += $bfw;
$j = 1;
$k = 0; //row colour counter
while ($myrow = db_fetch($result))
{

	alt_table_row_color($k);

	$running_total += $myrow["amount"];

	$trandate = sql2date($myrow["trans_date"]);
	label_cell($systypes_array[$myrow["type"]]);
	label_cell(get_trans_view_str($myrow["type"],$myrow["trans_no"]));
	label_cell(get_trans_view_str($myrow["type"],$myrow["trans_no"],$myrow['ref']));
	label_cell($trandate);
	display_debit_or_credit_cells($myrow["amount"]);
	amount_cell($running_total);

	label_cell(payment_person_name($myrow["person_type_id"],$myrow["person_id"]));

	label_cell(get_comments_string($myrow["type"], $myrow["trans_no"]));
	label_cell(get_gl_view_str($myrow["type"], $myrow["trans_no"]));

	label_cell(trans_editor_link($myrow["type"], $myrow["trans_no"]));

	end_row();
 	if ($myrow["amount"] > 0 ) 
 		$debit += $myrow["amount"];
 	else 
 		$credit += $myrow["amount"];

	if ($j == 12)
	{
		$j = 1;
		table_header($th);
	}
	$j++;
}
//end of while loop

start_row("class='inquirybg' style='font-weight:bold'");
label_cell(_("Ending Balance")." - ". $_POST['TransToDate'], "colspan=4");
amount_cell($debit);
amount_cell(-$credit);
//display_debit_or_credit_cells($running_total);
amount_cell($debit+$credit);
label_cell("", "colspan=4");
end_row();
end_table(2);
div_end();
//------------------------------------------------------------------------------------------------

end_page();

