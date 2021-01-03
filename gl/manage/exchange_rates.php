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
$page_security = 'SA_EXCHANGERATE';
$path_to_root = "../..";
include($path_to_root . "/includes/db_pager.inc");
include_once($path_to_root . "/includes/session.inc");

include_once($path_to_root . "/includes/date_functions.inc");
include_once($path_to_root . "/includes/ui.inc");
include_once($path_to_root . "/includes/banking.inc");

$js = "";
if (user_use_date_picker())
	$js .= get_js_date_picker();
page(_($help_context = "Exchange Rates"), false, false, "", $js);

simple_page_mode(false);

//---------------------------------------------------------------------------------------------
function check_data($selected_id)
{
	if (!is_date($_POST['date_']))
	{
		display_error( _("The entered date is invalid."));
		set_focus('date_');
		return false;
	}
	if (input_num('BuyRate') <= 0)
	{
		display_error( _("The exchange rate cannot be zero or a negative number."));
		set_focus('BuyRate');
		return false;
	}
	if (!$selected_id && get_date_exchange_rate($_POST['curr_abrev'], $_POST['date_']))
	{
		display_error( _("The exchange rate for the date is already there."));
		set_focus('date_');
		return false;
	}
	return true;
}

//---------------------------------------------------------------------------------------------

function handle_submit()
{
	global $selected_id;

	if (!check_data($selected_id))
		return false;

	if ($selected_id != "")
	{

		update_exchange_rate($_POST['curr_abrev'], $_POST['date_'],
		input_num('BuyRate'), input_num('BuyRate'));
	}
	else
	{

		add_exchange_rate($_POST['curr_abrev'], $_POST['date_'],
		    input_num('BuyRate'), input_num('BuyRate'));
	}

	$selected_id = '';
	clear_data();
}

//---------------------------------------------------------------------------------------------

function handle_delete()
{
	global $selected_id;

	if ($selected_id == "")
		return;
	delete_exchange_rate($selected_id);
	$selected_id = '';
	clear_data();
}

//---------------------------------------------------------------------------------------------
function edit_link($row) 
{
  return button('Edit'.$row["id"], _("Edit"), true, ICON_EDIT);
}

function del_link($row) 
{
  return button('Delete'.$row["id"], _("Delete"), true, ICON_DELETE);
}

function display_rates($curr_code)
{

}

//---------------------------------------------------------------------------------------------

function display_rate_edit()
{
	global $selected_id, $Ajax, $SysPrefs;
	$xchg_rate_provider = ((isset($SysPrefs->xr_providers) && isset($SysPrefs->dflt_xr_provider))
		? $SysPrefs->xr_providers[$SysPrefs->dflt_xr_provider] : 'ECB');
	start_table(TABLESTYLE2);

	if ($selected_id != "")
	{
		//editing an existing exchange rate

		$myrow = get_exchange_rate($selected_id);

		$_POST['date_'] = sql2date($myrow["date_"]);
		$_POST['BuyRate'] = maxprec_format($myrow["rate_buy"]);

		hidden('selected_id', $selected_id);
		hidden('date_', $_POST['date_']);

		label_row(_("Date to Use From:"), $_POST['date_']);
	}
	else
	{
		$_POST['date_'] = Today();
		$_POST['BuyRate'] = '';
		date_row(_("Date to Use From:"), 'date_');
	}
	if (isset($_POST['get_rate']))
	{
		$_POST['BuyRate'] = 
			maxprec_format(retrieve_exrate($_POST['curr_abrev'], $_POST['date_']));
		$Ajax->activate('BuyRate');
	}
	amount_row(_("Exchange Rate:"), 'BuyRate', null, '',
	  	submit('get_rate',_("Get"), false, _('Get current rate from') . ' ' . $xchg_rate_provider , true), 'max');

	end_table(1);

	submit_add_or_update_center($selected_id == '', '', 'both');

	display_note(_("Exchange rates are entered against the company currency."), 1);
}

//---------------------------------------------------------------------------------------------

function clear_data()
{
	unset($_POST['selected_id']);
	unset($_POST['date_']);
	unset($_POST['BuyRate']);
}

//---------------------------------------------------------------------------------------------

if ($Mode=='ADD_ITEM' || $Mode=='UPDATE_ITEM') 
	handle_submit();

//---------------------------------------------------------------------------------------------

if ($Mode == 'Delete')
	handle_delete();


//---------------------------------------------------------------------------------------------

start_form();

if (!isset($_POST['curr_abrev']))
	$_POST['curr_abrev'] = get_global_curr_code();

echo "<center>";
echo _("Select a currency :") . "  ";
echo currencies_list('curr_abrev', null, true, true);
echo "</center>";

// if currency sel has changed, clear the form
if ($_POST['curr_abrev'] != get_global_curr_code())
{
	clear_data();
	$selected_id = "";
}

set_global_curr_code(get_post('curr_abrev'));

$sql = get_sql_for_exchange_rates(get_post('curr_abrev'));

$cols = array(
	_("Date to Use From") => 'date', 
	_("Exchange Rate") => 'rate',
	array('insert'=>true, 'fun'=>'edit_link'),
	array('insert'=>true, 'fun'=>'del_link'),
);
$table =& new_db_pager('orders_tbl', $sql, $cols);

if (is_company_currency(get_post('curr_abrev')))
{

	display_note(_("The selected currency is the company currency."), 2);
	display_note(_("The company currency is the base currency so exchange rates cannot be set for it."), 1);
}
else
{

	br(1);
	$table->width = "40%";
	if ($table->rec_count == 0)
		$table->ready = false;
	display_db_pager($table);
   	br(1);
    display_rate_edit();
}

end_form();

end_page();

