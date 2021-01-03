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
$page_security = 'SA_INVENTORYLOCATION';
$path_to_root = "../..";
include($path_to_root . "/includes/session.inc");


include_once($path_to_root . "/includes/ui.inc");

include_once($path_to_root . "/inventory/includes/inventory_db.inc");

if (isset($_GET['FixedAsset'])) {
	$help_context = "Fixed Assets Locations";
	$_POST['fixed_asset'] = 1;
} else
	$help_context = "Inventory Locations";

page(_($help_context));

simple_page_mode(true);

if ($Mode=='ADD_ITEM' || $Mode=='UPDATE_ITEM') 
{

	//initialise no input errors assumed initially before we test
	$input_error = 0;

	/* actions to take once the user has clicked the submit button
	ie the page has called itself with some user input */

	//first off validate inputs sensible
	$_POST['loc_code'] = strtoupper($_POST['loc_code']);

	if ((strlen(db_escape($_POST['loc_code'])) > 7) || empty($_POST['loc_code'])) //check length after conversion
	{
		$input_error = 1;
		display_error( _("The location code must be five characters or less long (including converted special chars)."));
		set_focus('loc_code');
	} 
	elseif (strlen($_POST['location_name']) == 0) 
	{
		$input_error = 1;
		display_error( _("The location name must be entered."));		
		set_focus('location_name');
	}

	if ($input_error != 1) 
	{
    	if ($selected_id != -1) 
    	{
    
    		update_item_location($selected_id, $_POST['location_name'], $_POST['delivery_address'],
				$_POST['phone'], $_POST['phone2'], $_POST['fax'], $_POST['email'], $_POST['contact'], check_value('fixed_asset'));
			display_notification(_('Selected location has been updated'));
    	} 
    	else 
    	{
    
    	/*selected_id is null cos no item selected on first time round so must be adding a	record must be submitting new entries in the new Location form */
    	
    		add_item_location($_POST['loc_code'], $_POST['location_name'], $_POST['delivery_address'], 
				$_POST['phone'], $_POST['phone2'], $_POST['fax'], $_POST['email'], $_POST['contact'], check_value('fixed_asset'));
			display_notification(_('New location has been added'));
    	}
		
		$Mode = 'RESET';
	}
} 

function can_delete($selected_id)
{
	if (key_in_foreign_table($selected_id, 'stock_moves', 'loc_code'))
	{
		display_error(_("Cannot delete this location because item movements have been created using this location."));
		return false;
	}

	if (key_in_foreign_table($selected_id, 'workorders', 'loc_code'))
	{
		display_error(_("Cannot delete this location because it is used by some work orders records."));
		return false;
	}

	if (key_in_foreign_table($selected_id, 'cust_branch', 'default_location'))
	{
		display_error(_("Cannot delete this location because it is used by some branch records as the default location to deliver from."));
		return false;
	}
	
	if (key_in_foreign_table($selected_id, 'bom', 'loc_code'))
	{
		display_error(_("Cannot delete this location because it is used by some related records in other tables."));
		return false;
	}
	
	if (key_in_foreign_table($selected_id, 'grn_batch', 'loc_code'))
	{
		display_error(_("Cannot delete this location because it is used by some related records in other tables."));
		return false;
	}
	if (key_in_foreign_table($selected_id, 'purch_orders', 'into_stock_location'))
	{
		display_error(_("Cannot delete this location because it is used by some related records in other tables."));
		return false;
	}
	if (key_in_foreign_table($selected_id, 'sales_orders', 'from_stk_loc'))
	{
		display_error(_("Cannot delete this location because it is used by some related records in other tables."));
		return false;
	}
	if (key_in_foreign_table($selected_id, 'sales_pos', 'pos_location'))
	{
		display_error(_("Cannot delete this location because it is used by some related records in other tables."));
		return false;
	}
	return true;
}

//----------------------------------------------------------------------------------

if ($Mode == 'Delete')
{

	if (can_delete($selected_id)) 
	{
		delete_item_location($selected_id);
		display_notification(_('Selected location has been deleted'));
	} //end if Delete Location
	$Mode = 'RESET';
}

if ($Mode == 'RESET')
{
	$selected_id = -1;
	$sav = get_post('show_inactive');
	$sav2 = get_post('fixed_asset');
	unset($_POST);
	$_POST['show_inactive'] = $sav;
	$_POST['fixed_asset'] = $sav2;
}

$result = get_item_locations(check_value('show_inactive'), get_post('fixed_asset', 0));

start_form();
start_table(TABLESTYLE);
$th = array(_("Location Code"), _("Location Name"), _("Address"), _("Phone"), _("Secondary Phone"), "", "");
inactive_control_column($th);
table_header($th);
$k = 0; //row colour counter
while ($myrow = db_fetch($result)) 
{

	alt_table_row_color($k);
	
	label_cell($myrow["loc_code"]);
	label_cell($myrow["location_name"]);
	label_cell($myrow["delivery_address"]);
	label_cell($myrow["phone"]);
	label_cell($myrow["phone2"]);
	inactive_control_cell($myrow["loc_code"], $myrow["inactive"], 'locations', 'loc_code');
 	edit_button_cell("Edit".$myrow["loc_code"], _("Edit"));
 	delete_button_cell("Delete".$myrow["loc_code"], _("Delete"));
	end_row();
}
	//END WHILE LIST LOOP
inactive_control_row($th);
end_table();

echo '<br>';

start_table(TABLESTYLE2);
hidden("fixed_asset");

$_POST['email'] = "";
if ($selected_id != -1) 
{
	//editing an existing Location

 	if ($Mode == 'Edit') {
		$myrow = get_item_location($selected_id);

		$_POST['loc_code'] = $myrow["loc_code"];
		$_POST['location_name']  = $myrow["location_name"];
		$_POST['delivery_address'] = $myrow["delivery_address"];
		$_POST['contact'] = $myrow["contact"];
		$_POST['phone'] = $myrow["phone"];
		$_POST['phone2'] = $myrow["phone2"];
		$_POST['fax'] = $myrow["fax"];
		$_POST['email'] = $myrow["email"];
	}
	hidden("selected_id", $selected_id);
	hidden("loc_code");
	label_row(_("Location Code:"), $_POST['loc_code']);
} 
else 
{ //end of if $selected_id only do the else when a new record is being entered
	text_row(_("Location Code:"), 'loc_code', null, 5, 5);
}

text_row_ex(_("Location Name:"), 'location_name', 50, 50);
text_row_ex(_("Contact for deliveries:"), 'contact', 30, 30);

textarea_row(_("Address:"), 'delivery_address', null, 34, 5);	

text_row_ex(_("Telephone No:"), 'phone', 32, 30);
text_row_ex(_("Secondary Phone Number:"), 'phone2', 32, 30);
text_row_ex(_("Facsimile No:"), 'fax', 32, 30);
email_row_ex(_("E-mail:"), 'email', 50);

end_table(1);
submit_add_or_update_center($selected_id == -1, '', 'both');

end_form();

end_page();

