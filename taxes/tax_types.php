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
$page_security = 'SA_TAXRATES';
$path_to_root = "..";

include($path_to_root . "/includes/session.inc");
page(_($help_context = "Tax Types"));

include_once($path_to_root . "/includes/ui.inc");
include_once($path_to_root . "/taxes/db/tax_types_db.inc");

simple_page_mode(true);
//-----------------------------------------------------------------------------------

function can_process()
{
	global $selected_id;
	
	if (strlen($_POST['name']) == 0)
	{
		display_error(_("The tax type name cannot be empty."));
		set_focus('name');
		return false;
	}
	elseif (!check_num('rate', 0))
	{
		display_error( _("The default tax rate must be numeric and not less than zero."));
		set_focus('rate');
		return false;
	}

	if (!is_tax_gl_unique(get_post('sales_gl_code'), get_post('purchasing_gl_code'), $selected_id)) {
		display_error( _("Selected GL Accounts cannot be used by another tax type."));
		set_focus('sales_gl_code');
		return false;
	}
	return true;
}

//-----------------------------------------------------------------------------------

if ($Mode=='ADD_ITEM' && can_process())
{

	add_tax_type($_POST['name'], $_POST['sales_gl_code'],
		$_POST['purchasing_gl_code'], input_num('rate', 0));
	display_notification(_('New tax type has been added'));
	$Mode = 'RESET';
}

//-----------------------------------------------------------------------------------

if ($Mode=='UPDATE_ITEM' && can_process())
{

	update_tax_type($selected_id, $_POST['name'],
    	$_POST['sales_gl_code'], $_POST['purchasing_gl_code'], input_num('rate'));
	display_notification(_('Selected tax type has been updated'));
	$Mode = 'RESET';
}

//-----------------------------------------------------------------------------------

function can_delete($selected_id)
{
	if (key_in_foreign_table($selected_id, 'tax_group_items', 'tax_type_id'))
	{
		display_error(_("Cannot delete this tax type because tax groups been created referring to it."));

		return false;
	}

	return true;
}


//-----------------------------------------------------------------------------------

if ($Mode == 'Delete')
{

	if (can_delete($selected_id))
	{
		delete_tax_type($selected_id);
		display_notification(_('Selected tax type has been deleted'));
	}
	$Mode = 'RESET';
}

if ($Mode == 'RESET')
{
	$selected_id = -1;
	$sav = get_post('show_inactive');
	unset($_POST);
	$_POST['show_inactive'] = $sav;
}
//-----------------------------------------------------------------------------------

$result = get_all_tax_types(check_value('show_inactive'));

start_form();

display_note(_("To avoid problems with manual journal entry all tax types should have unique Sales/Purchasing GL accounts."), 0, 1);
start_table(TABLESTYLE);

$th = array(_("Description"), _("Default Rate (%)"),
	_("Sales GL Account"), _("Purchasing GL Account"), "", "");
inactive_control_column($th);
table_header($th);

$k = 0;
while ($myrow = db_fetch($result))
{

	alt_table_row_color($k);

	label_cell($myrow["name"]);
	label_cell(percent_format($myrow["rate"]), "align=right");
	label_cell($myrow["sales_gl_code"] . "&nbsp;" . $myrow["SalesAccountName"]);
	label_cell($myrow["purchasing_gl_code"] . "&nbsp;" . $myrow["PurchasingAccountName"]);

	inactive_control_cell($myrow["id"], $myrow["inactive"], 'tax_types', 'id');
 	edit_button_cell("Edit".$myrow["id"], _("Edit"));
 	delete_button_cell("Delete".$myrow["id"], _("Delete"));

	end_row();
}

inactive_control_row($th);
end_table(1);
//-----------------------------------------------------------------------------------

start_table(TABLESTYLE2);

if ($selected_id != -1) 
{
 	if ($Mode == 'Edit') {
		//editing an existing status code

		$myrow = get_tax_type($selected_id);

		$_POST['name']  = $myrow["name"];
		$_POST['rate']  = percent_format($myrow["rate"]);
		$_POST['sales_gl_code']  = $myrow["sales_gl_code"];
		$_POST['purchasing_gl_code']  = $myrow["purchasing_gl_code"];
	}
	hidden('selected_id', $selected_id);
}
text_row_ex(_("Description:"), 'name', 50);
small_amount_row(_("Default Rate:"), 'rate', '', "", "%", user_percent_dec());

gl_all_accounts_list_row(_("Sales GL Account:"), 'sales_gl_code', null);
gl_all_accounts_list_row(_("Purchasing GL Account:"), 'purchasing_gl_code', null);

end_table(1);

submit_add_or_update_center($selected_id == -1, '', 'both');

end_form();

end_page();

