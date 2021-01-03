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
$page_security = 'SA_SUPPTRANSVIEW';
$path_to_root = "../..";

include($path_to_root . "/includes/session.inc");

include_once($path_to_root . "/includes/ui.inc");
include_once($path_to_root . "/purchasing/includes/purchasing_db.inc");
$js = "";
if ($SysPrefs->use_popup_windows)
	$js .= get_js_open_window(900, 500);
page(_($help_context = "View Payment to Supplier"), true, false, "", $js);

if (isset($_GET["trans_no"]))
{
	$trans_no = $_GET["trans_no"];
}

$receipt = get_supp_trans($trans_no, ST_SUPPAYMENT);

$company_currency = get_company_currency();

$show_currencies = false;
$show_both_amounts = false;

if (($receipt['bank_curr_code'] != $company_currency) || ($receipt['curr_code'] != $company_currency))
	$show_currencies = true;

if ($receipt['bank_curr_code'] != $receipt['curr_code']) 
{
	$show_currencies = true;
	$show_both_amounts = true;
}

echo "<center>";

display_heading(_("Payment to Supplier") . " #$trans_no");

echo "<br>";
start_table(TABLESTYLE2, "width='80%'");

start_row();
label_cells(_("To Supplier"), $receipt['supplier_name'], "class='tableheader2'");
label_cells(_("From Bank Account"), $receipt['bank_account_name'], "class='tableheader2'");
label_cells(_("Date Paid"), sql2date($receipt['tran_date']), "class='tableheader2'");
end_row();
start_row();
if ($show_currencies)
	label_cells(_("Payment Currency"), $receipt['bank_curr_code'], "class='tableheader2'");
label_cells(_("Amount"), number_format2(-$receipt['bank_amount'], user_price_dec()), "class='tableheader2'");
if ($receipt['ov_discount'] != 0)
	label_cells(_("Discount"), number_format2(-$receipt['ov_discount']*$receipt['rate'], user_price_dec()), "class='tableheader2'");
else
	label_cells(_("Payment Type"), $bank_transfer_types[$receipt['BankTransType']], "class='tableheader2'");
end_row();
start_row();
if ($show_currencies) 
{
	label_cells(_("Supplier's Currency"), $receipt['curr_code'], "class='tableheader2'");
}
if ($show_both_amounts)
	label_cells(_("Amount"), number_format2(-$receipt['Total'], user_price_dec()), "class='tableheader2'");
label_cells(_("Reference"), $receipt['ref'], "class='tableheader2'");
end_row();
if ($receipt['ov_discount'] != 0)
{
	start_row();
	label_cells(_("Payment Type"), $bank_transfer_types[$receipt['BankTransType']], "class='tableheader2'");
	end_row();
}
comments_display_row(ST_SUPPAYMENT, $trans_no);

end_table(1);

$voided = is_voided_display(ST_SUPPAYMENT, $trans_no, _("This payment has been voided."));

// now display the allocations for this payment
if (!$voided) 
{
	display_allocations_from(PT_SUPPLIER, $receipt['supplier_id'], ST_SUPPAYMENT, $trans_no, -$receipt['Total']);
}

end_page(true, false, false, ST_SUPPAYMENT, $trans_no);
