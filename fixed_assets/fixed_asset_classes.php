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
$page_security = 'SA_ASSETCLASS';
$path_to_root = "..";
include($path_to_root . "/includes/session.inc");

include_once($path_to_root . "/includes/ui.inc");

include_once($path_to_root . "/fixed_assets/includes/fixed_assets_db.inc");
include_once($path_to_root . "/fixed_assets/includes/fa_classes_db.inc");

page(_($help_context = "Fixed asset classes"));

simple_page_mode(true);

if ($Mode=='ADD_ITEM' || $Mode=='UPDATE_ITEM') 
{

	//initialise no input errors assumed initially before we test
	$input_error = 0;

	/* actions to take once the user has clicked the submit button
	ie the page has called itself with some user input */

	if ($input_error != 1) 
	{
    	if ($selected_id != -1) 
    	{
    		update_fixed_asset_class($selected_id, $_POST['parent_id'], $_POST['description'], $_POST['long_description'],
    			$_POST['depreciation_rate']);
			  display_notification(_('Selected fixed asset class has been updated'));
    	} 
    	else 
    	{
    		add_fixed_asset_class($_POST['fa_class_id'], $_POST['parent_id'], $_POST['description'], $_POST['long_description'],
    			$_POST['depreciation_rate']);
			  display_notification(_('New fixed asset class has been added'));
    	}

		$Mode = 'RESET';
	}
} 

function can_delete($selected_id)
{
	if (key_in_foreign_table($selected_id, 'stock_master', 'fa_class_id'))
	{
		display_error(_("Cannot delete this class because it is used by some fixed asset items."));
		return false;
	}
	return true;
}

//----------------------------------------------------------------------------------

if ($Mode == 'Delete')
{

	if (can_delete($selected_id)) 
	{
		delete_fixed_asset_class($selected_id);
		display_notification(_('Selected fixed asset class has been deleted'));
	} //end if Delete Location
	$Mode = 'RESET';
}

if ($Mode == 'RESET')
{
	$selected_id = -1;
  unset($_POST);
}

$result = get_fixed_asset_classes();

start_form();
start_table(TABLESTYLE);
$th = array(_("Fixed asset class"), _("Description"), _("Basic Depreciation Rate"), "", "");
inactive_control_column($th);
table_header($th);
$k = 0; //row colour counter
while ($myrow = db_fetch($result)) 
{
	alt_table_row_color($k);
	
	label_cell($myrow["fa_class_id"]);
	label_cell($myrow["description"]);
	label_cell($myrow["depreciation_rate"].'%');
	inactive_control_cell($myrow["fa_class_id"], $myrow["inactive"], 'stock_fa_classes', 'fa_class_id');
 	edit_button_cell("Edit".$myrow["fa_class_id"], _("Edit"));
 	delete_button_cell("Delete".$myrow["fa_class_id"], _("Delete"));
	end_row();
}
inactive_control_row($th);
end_table(1);

echo '<br>';

start_form(true);
div_start('par_tbl');
start_table(TABLESTYLE2);

if ($selected_id != -1) 
{
 	if ($Mode == 'Edit') {
		$myrow = get_fixed_asset_class($selected_id);

		$_POST['fa_class_id'] = $myrow["fa_class_id"];
		$_POST['parent_id'] = $myrow["parent_id"];
		$_POST['description']  = $myrow["description"];
		$_POST['long_description']  = $myrow["long_description"];
		$_POST['depreciation_rate'] = $myrow["depreciation_rate"];
	}
	hidden("selected_id", $selected_id);
	hidden("fa_class_id");
  hidden('parent_id');
  label_row(_("Parent class:"), $_POST['parent_id']);
  label_row(_("Fixed asset class:"), $_POST['fa_class_id']);
} 
else 
{
  text_row(_("Parent class:"), 'parent_id', null, 3, 3);
  text_row(_("Fixed asset class:"), 'fa_class_id', null, 3, 3);
}

text_row(_("Description:"), 'description', null, 42, 200);
textarea_row(_('Long description:'), 'long_description', null, 42, 3);
small_amount_row(_("Basic Depreciation Rate").':', 'depreciation_rate', null, null, '%', user_percent_dec());
//text_row(_("Parent id:"), 'parent_id', null, 3, 3);

end_table(1);
div_end();
//if ($selected_id != -1) 
submit_add_or_update_center($selected_id == -1, '', 'both');

end_form();

end_page();
