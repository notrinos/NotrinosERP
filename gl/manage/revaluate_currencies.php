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
include_once($path_to_root . "/includes/session.inc");

include_once($path_to_root . "/includes/date_functions.inc");
include_once($path_to_root . "/includes/ui.inc");
include_once($path_to_root . "/includes/banking.inc");

$js = "";
if (user_use_date_picker())
	$js .= get_js_date_picker();
page(_($help_context = "Revaluation of Currency Accounts"), false, false, "", $js);

if (isset($_GET['BA'])) 
{
	$BA = $_GET['BA'];
	$JE = $_GET['JE'];

	if ($BA != 0 || $JE !=0)
	{
		display_notification_centered(sprintf(_("%d Journal Entries for Bank Accounts have been added"), $BA));
		display_notification_centered(sprintf(_("%d Journal Entries for AR/AP accounts have been added"), $JE));
	}
	else
   		display_notification_centered( _("No revaluation was needed."));
}


//---------------------------------------------------------------------------------------------
function check_data()
{
	if (!is_date($_POST['date']))
	{
		display_error( _("The entered date is invalid."));
		set_focus('date');
		return false;
	}
	if (!is_date_in_fiscalyear($_POST['date']))
	{
		display_error(_("The entered date is out of fiscal year or is closed for further data entry."));
		set_focus('date');
		return false;
	}

	return true;
}

//---------------------------------------------------------------------------------------------

function handle_submit()
{
	if (!check_data())
		return;

	$trans = add_exchange_variation_all($_POST['date'], $_POST['memo_']);

	meta_forward($_SERVER['PHP_SELF'], "BA=".$trans[0]."&JE=".$trans[1]);
	//clear_data();
}


//---------------------------------------------------------------------------------------------

function display_reval()
{
	start_form();
	start_table(TABLESTYLE2);

	if (!isset($_POST['date']))
		$_POST['date'] = Today();
    date_row(_("Date for Revaluation:"), 'date', '', null, 0, 0, 0, null, true);
    textarea_row(_("Memo:"), 'memo_', null, 40,4);
	end_table(1);

	submit_center('submit', _("Revaluate Currencies"), true, false);
	end_form();
}

//---------------------------------------------------------------------------------------------

function clear_data()
{
	unset($_POST['date_']);
	unset($_POST['memo_']);
}

//---------------------------------------------------------------------------------------------

if (get_post('submit'))
	handle_submit();

//---------------------------------------------------------------------------------------------

display_reval();

end_page();

