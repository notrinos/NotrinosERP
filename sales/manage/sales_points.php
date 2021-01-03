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
$page_security = 'SA_POSSETUP';
$path_to_root = "../..";
include_once($path_to_root . "/includes/session.inc");

page(_($help_context = "POS settings"));

include_once($path_to_root . "/includes/ui.inc");
include_once($path_to_root . "/sales/includes/db/sales_points_db.inc");

simple_page_mode(true);
//----------------------------------------------------------------------------------------------------

function can_process()
{
	if (strlen($_POST['name']) == 0)
	{
		display_error(_("The POS name cannot be empty."));
		set_focus('pos_name');
		return false;
	}
	return true;
}

//----------------------------------------------------------------------------------------------------

if ($Mode=='ADD_ITEM' && can_process())
{
	add_sales_point($_POST['name'], $_POST['location'], $_POST['account'],
		check_value('cash'), check_value('credit'));
	display_notification(_('New point of sale has been added'));
	$Mode = 'RESET';
}

//----------------------------------------------------------------------------------------------------

if ($Mode=='UPDATE_ITEM' && can_process())
{

	update_sales_point($selected_id, $_POST['name'], $_POST['location'],
		$_POST['account'], check_value('cash'), check_value('credit'));
	display_notification(_('Selected point of sale has been updated'));
	$Mode = 'RESET';
}

//----------------------------------------------------------------------------------------------------

if ($Mode == 'Delete')
{
	if (key_in_foreign_table($selected_id, 'users', 'pos'))
	{
		display_error(_("Cannot delete this POS because it is used in users setup."));
	} else {
		delete_sales_point($selected_id);
		display_notification(_('Selected point of sale has been deleted'));
		$Mode = 'RESET';
	}
}

if ($Mode == 'RESET')
{
	$selected_id = -1;
	$sav = get_post('show_inactive');
	unset($_POST);
	$_POST['show_inactive'] = $sav;
}
//----------------------------------------------------------------------------------------------------

$result = get_all_sales_points(check_value('show_inactive'));

start_form();
start_table(TABLESTYLE);

$th = array (_('POS Name'), _('Credit sale'), _('Cash sale'), _('Location'), _('Default account'), 
	 '','');
inactive_control_column($th);
table_header($th);
$k = 0;

while ($myrow = db_fetch($result))
{
    alt_table_row_color($k);
	label_cell($myrow["pos_name"], "nowrap");
	label_cell($myrow['credit_sale'] ? _('Yes') : _('No'));
	label_cell($myrow['cash_sale'] ? _('Yes') : _('No'));
	label_cell($myrow["location_name"], "");
	label_cell($myrow["bank_account_name"], "");
	inactive_control_cell($myrow["id"], $myrow["inactive"], "sales_pos", 'id');
 	edit_button_cell("Edit".$myrow['id'], _("Edit"));
 	delete_button_cell("Delete".$myrow['id'], _("Delete"));
	end_row();
}

inactive_control_row($th);
end_table(1);
//----------------------------------------------------------------------------------------------------

$cash = db_has_cash_accounts();

if (!$cash) display_note(_("To have cash POS first define at least one cash bank account."));

start_table(TABLESTYLE2);

if ($selected_id != -1)
{

 	if ($Mode == 'Edit') {
		$myrow = get_sales_point($selected_id);

		$_POST['name']  = $myrow["pos_name"];
		$_POST['location']  = $myrow["pos_location"];
		$_POST['account']  = $myrow["pos_account"];
		if ($myrow["credit_sale"]) $_POST['credit_sale']  = 1;
		if ($myrow["cash_sale"]) $_POST['cash_sale'] = 1;
	}
	hidden('selected_id', $selected_id);
} 

text_row_ex(_("Point of Sale Name").':', 'name', 20, 30);
if($cash) {
	check_row(_('Allowed credit sale terms selection:'), 'credit', check_value('credit_sale'));
	check_row(_('Allowed cash sale terms selection:'), 'cash',  check_value('cash_sale'));
	cash_accounts_list_row(_("Default cash account").':', 'account');
} else {
	hidden('credit', 1);
	hidden('account', 0);
}

locations_list_row(_("POS location").':', 'location');
end_table(1);

submit_add_or_update_center($selected_id == -1, '', 'both');

end_form();

end_page();

