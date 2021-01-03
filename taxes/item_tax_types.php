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
$page_security = 'SA_ITEMTAXTYPE';
$path_to_root = "..";

include($path_to_root . "/includes/session.inc");

page(_($help_context = "Item Tax Types")); 

include_once($path_to_root . "/taxes/db/item_tax_types_db.inc");
include_once($path_to_root . "/taxes/db/tax_types_db.inc");

include($path_to_root . "/includes/ui.inc");

simple_page_mode(true);
//-----------------------------------------------------------------------------------

if ($Mode=='ADD_ITEM' || $Mode=='UPDATE_ITEM') 
{

	$input_error = 0;

	if (strlen($_POST['name']) == 0) 
	{
		$input_error = 1;
		display_error(_("The item tax type description cannot be empty."));
		set_focus('name');
	}

	if ($input_error != 1) 
	{
		
		// create an array of the exemptions
    	$exempt_from = array();
    	
        $tax_types = get_all_tax_types_simple();
        $i = 0;    	
        
        while ($myrow = db_fetch($tax_types)) 
        {
        	if (check_value('ExemptTax' . $myrow["id"]))
        	{
        		$exempt_from[$i] = $myrow["id"];
        		$i++;
        	}
        }  
        
    	if ($selected_id != -1) 
    	{    		
    		update_item_tax_type($selected_id, $_POST['name'], $_POST['exempt'], $exempt_from);
			display_notification(_('Selected item tax type has been updated'));
    	} 
    	else 
    	{
    		add_item_tax_type($_POST['name'], $_POST['exempt'], $exempt_from);
			display_notification(_('New item tax type has been added'));
    	}
		$Mode = 'RESET';
	}
} 

//-----------------------------------------------------------------------------------

function can_delete($selected_id)
{
	if (key_in_foreign_table($selected_id, 'stock_master', 'tax_type_id'))
	{
		display_error(_("Cannot delete this item tax type because items have been created referring to it."));
		return false;
	}
	if (key_in_foreign_table($selected_id, 'stock_category', 'dflt_tax_type'))
	{
		display_error(_("Cannot delete this item tax type because item categories have been created referring to it."));
		return false;
	}
	
	return true;
}


//-----------------------------------------------------------------------------------

if ($Mode == 'Delete')
{

	if (can_delete($selected_id))
	{
		delete_item_tax_type($selected_id);
		display_notification(_('Selected item tax type has been deleted'));
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


$result2 = $result = get_all_item_tax_types(check_value('show_inactive'));

start_form();
start_table(TABLESTYLE, "width='30%'");
$th = array(_("Name"), _("Tax exempt"),'','');
inactive_control_column($th);
table_header($th);

$k = 0;
while ($myrow = db_fetch($result2)) 
{
	
	alt_table_row_color($k);	

	if ($myrow["exempt"] == 0) 
	{
		$disallow_text = _("No");
	} 
	else 
	{
		$disallow_text = _("Yes");
	}
	
	label_cell($myrow["name"]);
	label_cell($disallow_text);
	inactive_control_cell($myrow["id"], $myrow["inactive"], 'item_tax_types', 'id');
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
   		$myrow = get_item_tax_type($selected_id);
   		unset($_POST); // clear exemption checkboxes
   		$_POST['name']  = $myrow["name"];
   		$_POST['exempt']  = $myrow["exempt"];
    	
   		// read the exemptions and check the ones that are on
   		$exemptions = get_item_tax_type_exemptions($selected_id);
    	
   		if (db_num_rows($exemptions) > 0)
   		{
   			while ($exmp = db_fetch($exemptions)) 
   			{
   				$_POST['ExemptTax' . $exmp["tax_type_id"]] = 1;
   			}
   		}	
	}

	hidden('selected_id', $selected_id);
} 

text_row_ex(_("Description:"), 'name', 50);

yesno_list_row(_("Is Fully Tax-exempt:"), 'exempt', null, "", "", true);

end_table(1);

if (!isset($_POST['exempt']) || $_POST['exempt'] == 0) 
{

    display_note(_("Select which taxes this item tax type is exempt from."), 0, 1);
    
    start_table(TABLESTYLE2);
    $th = array(_("Tax Name"), _("Rate"), _("Is exempt"));
    table_header($th);
    	
    $tax_types = get_all_tax_types_simple();    	
    
    while ($myrow = db_fetch($tax_types)) 
    {
    	
    	alt_table_row_color($k);	
    
    	label_cell($myrow["name"]);
		label_cell(percent_format($myrow["rate"])." %", "nowrap align=right");
    	check_cells("", 'ExemptTax' . $myrow["id"], null);
    	end_row();
    }
    
    end_table(1);
}

submit_add_or_update_center($selected_id == -1, '', 'both');

end_form();

//------------------------------------------------------------------------------------

end_page();

