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
$page_security = 'SA_GLSETUP';
$path_to_root="..";
include($path_to_root . "/includes/session.inc");

$js = "";
if ($SysPrefs->use_popup_windows && $SysPrefs->use_popup_search)
	$js .= get_js_open_window(900, 500);

page(_($help_context = "System and General GL Setup"), false, false, "", $js);

include_once($path_to_root . "/includes/date_functions.inc");
include_once($path_to_root . "/includes/ui.inc");
include_once($path_to_root . "/includes/data_checks.inc");

include_once($path_to_root . "/admin/db/company_db.inc");

//-------------------------------------------------------------------------------------------------

function can_process()
{
    if (!check_num('past_due_days', 0, 100))
    {
        display_error(_("The past due days interval allowance must be between 0 and 100."));
        set_focus('past_due_days');
        return false;
    }

    if (!check_num('default_quote_valid_days', 0))
    {
        display_error(_("Quote Valid Days is not valid number."));
        set_focus('default_quote_valid_days');
        return false;
    }

    if (!check_num('default_delivery_required', 0))
    {
        display_error(_("Delivery Required By is not valid number."));
        set_focus('default_delivery_required');
        return false;
    }

    if (!check_num('default_receival_required', 0))
    {
        display_error(_("Receival Required By is not valid number."));
        set_focus('default_receival_required');
        return false;
    }

    if (!check_num('default_workorder_required', 0))
    {
        display_error(_("Work Order Required By After is not valid number."));
        set_focus('default_workorder_required');
        return false;
    }	

    if (!check_num('po_over_receive', 0, 100))
	{
		display_error(_("The delivery over-receive allowance must be between 0 and 100."));
		set_focus('po_over_receive');
		return false;
	}

	if (!check_num('po_over_charge', 0, 100))
	{
		display_error(_("The invoice over-charge allowance must be between 0 and 100."));
		set_focus('po_over_charge');
		return false;
	}

	if (!check_num('past_due_days', 0, 100))
	{
		display_error(_("The past due days interval allowance must be between 0 and 100."));
		set_focus('past_due_days');
		return false;
	}

	$grn_act = get_company_pref('grn_clearing_act');
	$post_grn_act = get_post('grn_clearing_act');
	if ($post_grn_act == null)
		$post_grn_act = 0;
	if (($post_grn_act != $grn_act) && db_num_rows(get_grn_items(0, '', true)))
	{
		display_error(_("Before GRN Clearing Account can be changed all GRNs have to be invoiced"));
		$_POST['grn_clearing_act'] = $grn_act;
		set_focus('grn_clearing_account');
		return false;
	}
	if (!is_account_balancesheet(get_post('retained_earnings_act')) || is_account_balancesheet(get_post('profit_loss_year_act')))
	{
		display_error(_("The Retained Earnings Account should be a Balance Account or the Profit and Loss Year Account should be an Expense Account (preferred the last one in the Expense Class)"));
		return false;
	}
	return true;
}

//-------------------------------------------------------------------------------------------------

if (isset($_POST['submit']) && can_process())
{
	update_company_prefs( get_post( array( 'retained_earnings_act', 'profit_loss_year_act',
		'debtors_act', 'pyt_discount_act', 'creditors_act', 'freight_act', 'deferred_income_act',
		'exchange_diff_act', 'bank_charge_act', 'default_sales_act', 'default_sales_discount_act',
		'default_prompt_payment_act', 'default_inventory_act', 'default_cogs_act', 'depreciation_period',
		'default_loss_on_asset_disposal_act', 'default_adj_act', 'default_inv_sales_act', 'default_wip_act', 'legal_text',
		'past_due_days', 'default_workorder_required', 'default_dim_required', 'default_receival_required',
		'default_delivery_required', 'default_quote_valid_days', 'grn_clearing_act', 'tax_algorithm',
		'no_zero_lines_amount', 'show_po_item_codes', 'accounts_alpha', 'loc_notification', 'print_invoice_no',
		'allow_negative_prices', 'print_item_images_on_quote', 
		'allow_negative_stock'=> 0, 'accumulate_shipping'=> 0,
		'po_over_receive' => 0.0, 'po_over_charge' => 0.0, 'default_credit_limit'=>0.0
)));

	display_notification(_("The general GL setup has been updated."));

} /* end of if submit */

//-------------------------------------------------------------------------------------------------

start_form();

start_outer_table(TABLESTYLE2);

table_section(1);

$myrow = get_company_prefs();

$_POST['retained_earnings_act']  = $myrow["retained_earnings_act"];
$_POST['profit_loss_year_act']  = $myrow["profit_loss_year_act"];
$_POST['debtors_act']  = $myrow["debtors_act"];
$_POST['creditors_act']  = $myrow["creditors_act"];
$_POST['freight_act'] = $myrow["freight_act"];
$_POST['deferred_income_act'] = $myrow["deferred_income_act"];
$_POST['pyt_discount_act']  = $myrow["pyt_discount_act"];

$_POST['exchange_diff_act'] = $myrow["exchange_diff_act"];
$_POST['bank_charge_act'] = $myrow["bank_charge_act"];
$_POST['tax_algorithm'] = $myrow["tax_algorithm"];
$_POST['default_sales_act'] = $myrow["default_sales_act"];
$_POST['default_sales_discount_act']  = $myrow["default_sales_discount_act"];
$_POST['default_prompt_payment_act']  = $myrow["default_prompt_payment_act"];

$_POST['default_inventory_act'] = $myrow["default_inventory_act"];
$_POST['default_cogs_act'] = $myrow["default_cogs_act"];
$_POST['default_adj_act'] = $myrow["default_adj_act"];
$_POST['default_inv_sales_act'] = $myrow['default_inv_sales_act'];
$_POST['default_wip_act'] = $myrow['default_wip_act'];

$_POST['allow_negative_stock'] = $myrow['allow_negative_stock'];

$_POST['po_over_receive'] = percent_format($myrow['po_over_receive']);
$_POST['po_over_charge'] = percent_format($myrow['po_over_charge']);
$_POST['past_due_days'] = $myrow['past_due_days'];

$_POST['grn_clearing_act'] = $myrow['grn_clearing_act'];

$_POST['default_credit_limit'] = price_format($myrow['default_credit_limit']);
$_POST['legal_text'] = $myrow['legal_text'];
$_POST['accumulate_shipping'] = $myrow['accumulate_shipping'];

$_POST['default_workorder_required'] = $myrow['default_workorder_required'];
$_POST['default_dim_required'] = $myrow['default_dim_required'];
$_POST['default_delivery_required'] = $myrow['default_delivery_required'];
$_POST['default_receival_required'] = $myrow['default_receival_required'];
$_POST['default_quote_valid_days'] = $myrow['default_quote_valid_days'];
$_POST['no_zero_lines_amount'] = $myrow['no_zero_lines_amount'];
$_POST['show_po_item_codes'] = $myrow['show_po_item_codes'];
$_POST['accounts_alpha'] = $myrow['accounts_alpha'];
$_POST['loc_notification'] = $myrow['loc_notification'];
$_POST['print_invoice_no'] = $myrow['print_invoice_no'];
$_POST['allow_negative_prices'] = $myrow['allow_negative_prices'];
$_POST['print_item_images_on_quote'] = $myrow['print_item_images_on_quote'];
$_POST['default_loss_on_asset_disposal_act'] = $myrow['default_loss_on_asset_disposal_act'];
$_POST['depreciation_period'] = $myrow['depreciation_period'];

//---------------


table_section_title(_("General GL"));

text_row(_("Past Due Days Interval:"), 'past_due_days', $_POST['past_due_days'], 6, 6, '', "", _("days"));

accounts_type_list_row(_("Accounts Type:"), 'accounts_alpha', $_POST['accounts_alpha']); 

gl_all_accounts_list_row(_("Retained Earnings:"), 'retained_earnings_act', $_POST['retained_earnings_act']);

gl_all_accounts_list_row(_("Profit/Loss Year:"), 'profit_loss_year_act', $_POST['profit_loss_year_act']);

gl_all_accounts_list_row(_("Exchange Variances Account:"), 'exchange_diff_act', $_POST['exchange_diff_act']);

gl_all_accounts_list_row(_("Bank Charges Account:"), 'bank_charge_act', $_POST['bank_charge_act']);

tax_algorithm_list_row(_("Tax Algorithm:"), 'tax_algorithm', $_POST['tax_algorithm']);

//---------------

table_section_title(_("Dimension Defaults"));

text_row(_("Dimension Required By After:"), 'default_dim_required', $_POST['default_dim_required'], 6, 6, '', "", _("days"));

//----------------

table_section_title(_("Customers and Sales"));

amount_row(_("Default Credit Limit:"), 'default_credit_limit', $_POST['default_credit_limit']);

yesno_list_row(_("Invoice Identification:"), 'print_invoice_no', $_POST['print_invoice_no'], $name_yes=_("Number"), $name_no=_("Reference"));

check_row(_("Accumulate batch shipping:"), 'accumulate_shipping', null);

check_row(_("Print Item Image on Quote:"), 'print_item_images_on_quote', null);

textarea_row(_("Legal Text on Invoice:"), 'legal_text', $_POST['legal_text'], 32, 4);

gl_all_accounts_list_row(_("Shipping Charged Account:"), 'freight_act', $_POST['freight_act']);

gl_all_accounts_list_row(_("Deferred Income Account:"), 'deferred_income_act', $_POST['deferred_income_act'], true, false,
	_("Not used"), false, false, false);

//---------------

table_section_title(_("Customers and Sales Defaults"));
// default for customer branch
gl_all_accounts_list_row(_("Receivable Account:"), 'debtors_act');

gl_all_accounts_list_row(_("Sales Account:"), 'default_sales_act', null,
	false, false, true);

gl_all_accounts_list_row(_("Sales Discount Account:"), 'default_sales_discount_act');

gl_all_accounts_list_row(_("Prompt Payment Discount Account:"), 'default_prompt_payment_act');

text_row(_("Quote Valid Days:"), 'default_quote_valid_days', $_POST['default_quote_valid_days'], 6, 6, '', "", _("days"));

text_row(_("Delivery Required By:"), 'default_delivery_required', $_POST['default_delivery_required'], 6, 6, '', "", _("days"));

//---------------

table_section(2);

table_section_title(_("Suppliers and Purchasing"));

percent_row(_("Delivery Over-Receive Allowance:"), 'po_over_receive');

percent_row(_("Invoice Over-Charge Allowance:"), 'po_over_charge');

table_section_title(_("Suppliers and Purchasing Defaults"));

gl_all_accounts_list_row(_("Payable Account:"), 'creditors_act', $_POST['creditors_act']);

gl_all_accounts_list_row(_("Purchase Discount Account:"), 'pyt_discount_act', $_POST['pyt_discount_act']);

gl_all_accounts_list_row(_("GRN Clearing Account:"), 'grn_clearing_act', get_post('grn_clearing_act'), true, false, _("No postings on GRN"));

text_row(_("Receival Required By:"), 'default_receival_required', $_POST['default_receival_required'], 6, 6, '', "", _("days"));

check_row(_("Show PO item codes:"), 'show_po_item_codes', null);

table_section_title(_("Inventory"));

check_row(_("Allow Negative Inventory:"), 'allow_negative_stock', null);
label_row(null, _("Warning:  This may cause a delay in GL postings"), "", "class='stockmankofg' colspan=2"); 

check_row(_("No zero-amounts (Service):"), 'no_zero_lines_amount', null);

check_row(_("Location Notifications:"), 'loc_notification', null);

check_row(_("Allow Negative Prices:"), 'allow_negative_prices', null);

table_section_title(_("Items Defaults"));
gl_all_accounts_list_row(_("Sales Account:"), 'default_inv_sales_act', $_POST['default_inv_sales_act']);

gl_all_accounts_list_row(_("Inventory Account:"), 'default_inventory_act', $_POST['default_inventory_act']);
// this one is default for items and suppliers (purchase account)
gl_all_accounts_list_row(_("C.O.G.S. Account:"), 'default_cogs_act', $_POST['default_cogs_act']);

gl_all_accounts_list_row(_("Inventory Adjustments Account:"), 'default_adj_act', $_POST['default_adj_act']);

gl_all_accounts_list_row(_("WIP Account:"), 'default_wip_act', $_POST['default_wip_act']);

//----------------

table_section_title(_("Fixed Assets Defaults"));

gl_all_accounts_list_row(_("Loss On Asset Disposal Account:"), 'default_loss_on_asset_disposal_act', $_POST['default_loss_on_asset_disposal_act']);

array_selector_row (_("Depreciation Period:"), 'depreciation_period', $_POST['depreciation_period'], array(FA_MONTHLY => _("Monthly"), FA_YEARLY => _("Yearly")));

//----------------

table_section_title(_("Manufacturing Defaults"));

text_row(_("Work Order Required By After:"), 'default_workorder_required', $_POST['default_workorder_required'], 6, 6, '', "", _("days"));

//----------------

end_outer_table(1);

submit_center('submit', _("Update"), true, '', 'default');

end_form(2);

//-------------------------------------------------------------------------------------------------

end_page();

