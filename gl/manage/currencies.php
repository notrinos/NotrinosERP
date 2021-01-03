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
$page_security = 'SA_CURRENCY';
$path_to_root = "../..";
include_once($path_to_root . "/includes/session.inc");

page(_($help_context = "Currencies"));

include_once($path_to_root . "/includes/ui.inc");
include_once($path_to_root . "/includes/banking.inc");

simple_page_mode(false);

//---------------------------------------------------------------------------------------------

function check_data()
{
	if (strlen($_POST['Abbreviation']) == 0) 
	{
		display_error( _("The currency abbreviation must be entered."));
		set_focus('Abbreviation');
		return false;
	} 
	elseif (strlen($_POST['CurrencyName']) == 0) 
	{
		display_error( _("The currency name must be entered."));
		set_focus('CurrencyName');
		return false;		
	} 
	elseif (strlen($_POST['Symbol']) == 0) 
	{
		display_error( _("The currency symbol must be entered."));
		set_focus('Symbol');
		return false;		
	} 
	elseif (strlen($_POST['hundreds_name']) == 0) 
	{
		display_error( _("The hundredths name must be entered."));
		set_focus('hundreds_name');
		return false;		
	}  	
	
	return true;
}

//---------------------------------------------------------------------------------------------

function handle_submit()
{
	global $selected_id, $Mode;
	
	if (!check_data())
		return false;
		
	if ($selected_id != "") 
	{

		update_currency($_POST['Abbreviation'], $_POST['Symbol'], $_POST['CurrencyName'], 
			$_POST['country'], $_POST['hundreds_name'], check_value('auto_update'));
		display_notification(_('Selected currency settings has been updated'));
	} 
	else 
	{

		add_currency($_POST['Abbreviation'], $_POST['Symbol'], $_POST['CurrencyName'], 
			$_POST['country'], $_POST['hundreds_name'], check_value('auto_update'));
		display_notification(_('New currency has been added'));
	}	
	$Mode = 'RESET';
}

//---------------------------------------------------------------------------------------------

function check_can_delete($curr)
{

	if ($curr == "")
		return false;

	// PREVENT DELETES IF DEPENDENT RECORDS IN debtors_master
	if (key_in_foreign_table($curr, 'debtors_master', 'curr_code'))
	{
		display_error(_("Cannot delete this currency, because customer accounts have been created referring to this currency."));
		return false;
	}

	if (key_in_foreign_table($curr, 'suppliers', 'curr_code'))
	{
		display_error(_("Cannot delete this currency, because supplier accounts have been created referring to this currency."));
		return false;
	}

	if ($curr == get_company_pref('curr_default'))
	{
		display_error(_("Cannot delete this currency, because the company preferences uses this currency."));
		return false;
	}
	
	// see if there are any bank accounts that use this currency
	if (key_in_foreign_table($curr, 'bank_accounts', 'bank_curr_code'))
	{
		display_error(_("Cannot delete this currency, because thre are bank accounts that use this currency."));
		return false;
	}
	
	return true;
}

//---------------------------------------------------------------------------------------------

function handle_delete()
{
	global $selected_id, $Mode;
	if (check_can_delete($selected_id)) {
	//only delete if used in neither customer or supplier, comp prefs, bank trans accounts
		delete_currency($selected_id);
		display_notification(_('Selected currency has been deleted'));
	}
	$Mode = 'RESET';
}

//---------------------------------------------------------------------------------------------

function display_currencies()
{
	$company_currency = get_company_currency();
	
    $result = get_currencies(check_value('show_inactive'));
    start_table(TABLESTYLE);
    $th = array(_("Abbreviation"), _("Symbol"), _("Currency Name"),
    	_("Hundredths name"), _("Country"), _("Auto update"), "", "");
	inactive_control_column($th);
    table_header($th);	
    
    $k = 0; //row colour counter
    
    while ($myrow = db_fetch($result)) 
    {
    	
    	if ($myrow[1] == $company_currency) 
    	{
    		start_row("class='currencybg'");
    	} 
    	else
    		alt_table_row_color($k);
    		
    	label_cell($myrow["curr_abrev"]);
		label_cell($myrow["curr_symbol"]);
		label_cell($myrow["currency"]);
		label_cell($myrow["hundreds_name"]);
		label_cell($myrow["country"]);
		label_cell(	$myrow[1] == $company_currency ? '-' : 
			($myrow["auto_update"] ? _('Yes') :_('No')), "align='center'");
		inactive_control_cell($myrow["curr_abrev"], $myrow["inactive"], 'currencies', 'curr_abrev');
 		edit_button_cell("Edit".$myrow["curr_abrev"], _("Edit"));
		if ($myrow["curr_abrev"] != $company_currency)
 			delete_button_cell("Delete".$myrow["curr_abrev"], _("Delete"));
		else
			label_cell('');
		end_row();
		
    } //END WHILE LIST LOOP
    
	inactive_control_row($th);
    end_table();
    display_note(_("The marked currency is the home currency which cannot be deleted."), 0, 0, "class='currentfg'");
}

//---------------------------------------------------------------------------------------------

function display_currency_edit($selected_id)
{
	global $Mode;
	
	start_table(TABLESTYLE2);

	if ($selected_id != '') 
	{
		if ($Mode == 'Edit') {
			//editing an existing currency
			$myrow = get_currency($selected_id);

			$_POST['Abbreviation'] = $myrow["curr_abrev"];
			$_POST['Symbol'] = $myrow["curr_symbol"];
			$_POST['CurrencyName']  = $myrow["currency"];
			$_POST['country']  = $myrow["country"];
			$_POST['hundreds_name']  = $myrow["hundreds_name"];
			$_POST['auto_update']  = $myrow["auto_update"];
		}
		hidden('Abbreviation');
		hidden('selected_id', $selected_id);
		label_row(_("Currency Abbreviation:"), $_POST['Abbreviation']);
	} 
	else 
	{ 
		$_POST['auto_update']  = 1;
		text_row_ex(_("Currency Abbreviation:"), 'Abbreviation', 4, 3);
	}

	text_row_ex(_("Currency Symbol:"), 'Symbol', 10);
	text_row_ex(_("Currency Name:"), 'CurrencyName', 20);
	text_row_ex(_("Hundredths Name:"), 'hundreds_name', 15);	
	text_row_ex(_("Country:"), 'country', 40);	
	check_row(_("Automatic exchange rate update:"), 'auto_update', get_post('auto_update'));
	end_table(1);

	submit_add_or_update_center($selected_id == '', '', 'both');
}

//---------------------------------------------------------------------------------------------

if ($Mode=='ADD_ITEM' || $Mode=='UPDATE_ITEM') 
	handle_submit();

//--------------------------------------------------------------------------------------------- 

if ($Mode == 'Delete')
	handle_delete();

//---------------------------------------------------------------------------------------------
if ($Mode == 'RESET')
{
 		$selected_id = '';
		$_POST['Abbreviation'] = $_POST['Symbol'] = '';
		$_POST['CurrencyName'] = $_POST['country']  = '';
		$_POST['hundreds_name']  = '';
}

start_form();
display_currencies();

display_currency_edit($selected_id);
end_form();
//---------------------------------------------------------------------------------------------

end_page();

