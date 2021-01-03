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
$page_security = 'SA_GLACCOUNTGROUP';
$path_to_root = "../..";
include($path_to_root . "/includes/session.inc");

page(_($help_context = "GL Account Groups"));

include($path_to_root . "/gl/includes/gl_db.inc");

include($path_to_root . "/includes/ui.inc");

if (isset($_GET["cid"]))
	$_POST["cid"] = $_GET["cid"];	

simple_page_mode(false);
//-----------------------------------------------------------------------------------

function can_process($selected_id) 
{
	if (strlen(trim($_POST['id'])) == 0) 
	{
	    display_error( _("The account group id cannot be empty."));
	    set_focus('id');
	    return false;
	}
	if (strlen(trim($_POST['name'])) == 0) 
	{
		display_error( _("The account group name cannot be empty."));
		set_focus('name');
		return false;
	}
	$type = get_account_type(trim($_POST['id']));
	if ($type && ($type['id'] != $selected_id)) 
	{
		display_error( _("This account group id is already in use."));
		set_focus('id');
		return false;
	}

	if ($_POST['id'] === $_POST['parent']) 
	{
		display_error(_("You cannot set an account group to be a subgroup of itself."));
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------------

if ($Mode=='ADD_ITEM' || $Mode=='UPDATE_ITEM') 
{

	if (can_process($selected_id)) 
	{

    	if ($selected_id != "") 
    	{
    		if (update_account_type($_POST['id'], $_POST['name'], $_POST['class_id'], $_POST['parent'], $_POST['old_id']))
				display_notification(_('Selected account type has been updated'));
    	} 
    	else 
    	{
    		if (add_account_type($_POST['id'], $_POST['name'], $_POST['class_id'], $_POST['parent'])) {
				display_notification(_('New account type has been added'));
			}
    	}
		$Mode = 'RESET';
	}
}

//-----------------------------------------------------------------------------------

function can_delete($type)
{
	if ($type == "")
		return false;

	if (key_in_foreign_table($type, 'chart_master', 'account_type'))
	{
		display_error(_("Cannot delete this account group because GL accounts have been created referring to it."));
		return false;
	}

	if (key_in_foreign_table($type, 'chart_types', 'parent'))
	{
		display_error(_("Cannot delete this account group because GL account groups have been created referring to it."));
		return false;
	}

	return true;
}


//-----------------------------------------------------------------------------------

if ($Mode == 'Delete')
{

	if (can_delete($selected_id))
	{
		delete_account_type($selected_id);
		display_notification(_('Selected account group has been deleted'));
	}
	$Mode = 'RESET';
}
if ($Mode == 'RESET')
{
 	$selected_id = "";
	$_POST['id']  = $_POST['name']  = '';
	unset($_POST['parent']);
	unset($_POST['class_id']);
}
//-----------------------------------------------------------------------------------
$filter_cid = (isset($_POST["cid"]));
if ($filter_cid)
	$result = get_account_types(check_value('show_inactive'), $_POST["cid"]);
else
	$result = get_account_types(check_value('show_inactive'));

start_form();
start_table(TABLESTYLE);
$th = array(_("Group ID"), _("Group Name"), _("Subgroup Of"), _("Class"), "", "");
inactive_control_column($th);
table_header($th);

$k = 0;
while ($myrow = db_fetch($result)) 
{

	alt_table_row_color($k);

	$bs_text = get_account_class_name($myrow["class_id"]);

	if ($myrow["parent"] == '-1') 
	{
		$parent_text = "";
	} 
	else 
	{
		$parent_text = get_account_type_name($myrow["parent"]);
	}

	label_cell($myrow["id"]);
	label_cell('<a href="./gl_accounts.php?id='.$myrow["id"].'">'.$myrow["name"].'</a>');
	label_cell($parent_text);
	label_cell($bs_text);
	inactive_control_cell($myrow["id"], $myrow["inactive"], 'chart_types', 'id');
	edit_button_cell("Edit".$myrow["id"], _("Edit"));
	delete_button_cell("Delete".$myrow["id"], _("Delete"));
	end_row();
}

inactive_control_row($th);
end_table(1);
//-----------------------------------------------------------------------------------

start_table(TABLESTYLE2);

if ($selected_id != "")
{
	if ($Mode == 'Edit') 
	{
		//editing an existing status code
		$myrow = get_account_type($selected_id);
	
		$_POST['id']  = $myrow["id"];
		$_POST['name']  = $myrow["name"];
		$_POST['parent']  = $myrow["parent"];
		if ($_POST['parent'] == '-1')
			$_POST['parent'] == "";
		$_POST['class_id']  = $myrow["class_id"];
		hidden('selected_id', $myrow['id']);
		hidden('old_id', $myrow["id"]);
 	}
 	else
 	{
		hidden('selected_id', $selected_id);
		hidden('old_id', $_POST["old_id"]);
	}	
}
text_row_ex(_("ID:"), 'id', 10);
text_row_ex(_("Name:"), 'name', 50);

gl_account_types_list_row(_("Subgroup Of:"), 'parent', null, _("None"), true);

if ($filter_cid)
	class_list_row(_("Class:"), 'class_id', $_POST['cid']);
else
	class_list_row(_("Class:"), 'class_id', null);

end_table(1);

submit_add_or_update_center($selected_id == "", '', 'both');

end_form();

//------------------------------------------------------------------------------------

end_page();

