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

include($path_to_root . "/includes/session.inc");

page(_($help_context = "View Bank Transfer"), true);

include_once($path_to_root . "/includes/date_functions.inc");
include_once($path_to_root . "/includes/ui.inc");
include_once($path_to_root . "/gl/includes/gl_db.inc");

if (isset($_GET["trans_no"])){

	$trans_no = $_GET["trans_no"];
}

$result = get_bank_trans(ST_BANKTRANSFER, $trans_no);

if (db_num_rows($result) != 2)
	display_db_error("Bank transfer does not contain two records");

$trans1 = db_fetch($result);
$trans2 = db_fetch($result);

if ($trans1["amount"] < 0) 
{
    $from_trans = $trans1; // from trans is the negative one
    $to_trans = $trans2;
} 
else 
{
	$from_trans = $trans2;
	$to_trans = $trans1;
}

$company_currency = get_company_currency();

$show_currencies = false;
$show_both_amounts = false;

if (($from_trans['bank_curr_code'] != $company_currency) || ($to_trans['bank_curr_code'] != $company_currency))
	$show_currencies = true;

if ($from_trans['bank_curr_code'] != $to_trans['bank_curr_code']) 
{
	$show_currencies = true;
	$show_both_amounts = true;
}

display_heading($systypes_array[ST_BANKTRANSFER] . " #$trans_no");

echo "<br>";
start_table(TABLESTYLE, "width='80%'");

start_row();
label_cells(_("From Bank Account"), $from_trans['bank_account_name'], "class='tableheader2'");
if ($show_currencies)
	label_cells(_("Currency"), $from_trans['bank_curr_code'], "class='tableheader2'");
label_cells(_("Amount"), number_format2(-$from_trans['amount'], user_price_dec()), "class='tableheader2'", "align=right");
if ($show_currencies)
{
	end_row();
	start_row();
}	
label_cells(_("To Bank Account"), $to_trans['bank_account_name'], "class='tableheader2'");
if ($show_currencies)
	label_cells(_("Currency"), $to_trans['bank_curr_code'], "class='tableheader2'");
if ($show_both_amounts)
	label_cells(_("Amount"), number_format2($to_trans['amount'], user_price_dec()), "class='tableheader2'", "align=right");
end_row();
start_row();
label_cells(_("Date"), sql2date($from_trans['trans_date']), "class='tableheader2'");
label_cells(_("Transfer Type"), $bank_transfer_types[$from_trans['account_type']],
	 "class='tableheader2'");
label_cells(_("Reference"), $from_trans['ref'], "class='tableheader2'");
end_row();
comments_display_row(ST_BANKTRANSFER, $trans_no);

end_table(1);

is_voided_display(ST_BANKTRANSFER, $trans_no, _("This transfer has been voided."));

end_page(true, false, false, ST_BANKTRANSFER, $trans_no);
