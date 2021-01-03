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
$page_security = 'SA_DIMENSION';
$path_to_root = "..";
include_once($path_to_root . "/includes/session.inc");

include_once($path_to_root . "/includes/date_functions.inc");
include_once($path_to_root . "/includes/data_checks.inc");

include_once($path_to_root . "/admin/db/tags_db.inc");
include_once($path_to_root . "/dimensions/includes/dimensions_db.inc");
include_once($path_to_root . "/dimensions/includes/dimensions_ui.inc");

$js = "";
if (user_use_date_picker())
	$js .= get_js_date_picker();
page(_($help_context = "Dimension Entry"), false, false, "", $js);

//---------------------------------------------------------------------------------------

if (isset($_GET['trans_no']))
{
	$selected_id = $_GET['trans_no'];
} 
elseif(isset($_POST['selected_id']))
{
	$selected_id = $_POST['selected_id'];
}
else
	$selected_id = -1;
//---------------------------------------------------------------------------------------

if (isset($_GET['AddedID'])) 
{
	$id = $_GET['AddedID'];

	display_notification_centered(_("The dimension has been entered."));

	safe_exit();
}

//---------------------------------------------------------------------------------------

if (isset($_GET['UpdatedID'])) 
{
	$id = $_GET['UpdatedID'];

	display_notification_centered(_("The dimension has been updated."));
	safe_exit();
}

//---------------------------------------------------------------------------------------

if (isset($_GET['DeletedID'])) 
{
	$id = $_GET['DeletedID'];

	display_notification_centered(_("The dimension has been deleted."));
	safe_exit();
}

//---------------------------------------------------------------------------------------

if (isset($_GET['ClosedID'])) 
{
	$id = $_GET['ClosedID'];

	display_notification_centered(_("The dimension has been closed. There can be no more changes to it.") . " #$id");
	safe_exit();
}

//---------------------------------------------------------------------------------------

if (isset($_GET['ReopenedID'])) 
{
	$id = $_GET['ReopenedID'];

	display_notification_centered(_("The dimension has been re-opened. ") . " #$id");
	safe_exit();
}

//-------------------------------------------------------------------------------------------------

function safe_exit()
{
	global $path_to_root, $id;

	hyperlink_no_params("", _("Enter a &new dimension"));
	hyperlink_no_params($path_to_root . "/dimensions/inquiry/search_dimensions.php", _("&Select an existing dimension"));
    hyperlink_no_params($path_to_root . "/admin/attachments.php?filterType=40&trans_no=$id", _("&Add Attachment"));

	display_footer_exit();
}

//-------------------------------------------------------------------------------------

function can_process()
{
	global $selected_id, $Refs;

	if ($selected_id == -1) 
	{
    	if (!check_reference($_POST['ref'], ST_DIMENSION))
    	{
			set_focus('ref');
    		return false;
    	}
	}

	if (strlen($_POST['name']) == 0) 
	{
		display_error( _("The dimension name must be entered."));
		set_focus('name');
		return false;
	}

	if (!is_date($_POST['date_']))
	{
		display_error( _("The date entered is in an invalid format."));
		set_focus('date_');
		return false;
	}

	if (!is_date($_POST['due_date']))
	{
		display_error( _("The required by date entered is in an invalid format."));
		set_focus('due_date');
		return false;
	}

	return true;
}

//-------------------------------------------------------------------------------------

if (isset($_POST['ADD_ITEM']) || isset($_POST['UPDATE_ITEM'])) 
{
	if (!isset($_POST['dimension_tags']))
		$_POST['dimension_tags'] = array();
		
	if (can_process()) 
	{

		if ($selected_id == -1) 
		{
			$id = add_dimension($_POST['ref'], $_POST['name'], $_POST['type_'], $_POST['date_'], $_POST['due_date'], $_POST['memo_']);
			add_tag_associations($id, $_POST['dimension_tags']);
			meta_forward($_SERVER['PHP_SELF'], "AddedID=$id");
		} 
		else 
		{

			update_dimension($selected_id, $_POST['name'], $_POST['type_'], $_POST['date_'], $_POST['due_date'], $_POST['memo_']);
			update_tag_associations(TAG_DIMENSION, $selected_id, $_POST['dimension_tags']);

			meta_forward($_SERVER['PHP_SELF'], "UpdatedID=$selected_id");
		}
	}
}

//--------------------------------------------------------------------------------------

if (isset($_POST['delete'])) 
{

	$cancel_delete = false;

	// can't delete it there are productions or issues
	if (dimension_has_payments($selected_id) || dimension_has_deposits($selected_id))
	{
		display_error(_("This dimension cannot be deleted because it has already been processed."));
		set_focus('ref');
		$cancel_delete = true;
	}

	if ($cancel_delete == false) 
	{ //ie not cancelled the delete as a result of above tests

		// delete
		delete_dimension($selected_id);
		delete_tag_associations(TAG_DIMENSION,$selected_id, true);
		meta_forward($_SERVER['PHP_SELF'], "DeletedID=$selected_id");
	}
}

//-------------------------------------------------------------------------------------

if (isset($_POST['close'])) 
{

	// update the closed flag
	close_dimension($selected_id);
	meta_forward($_SERVER['PHP_SELF'], "ClosedID=$selected_id");
}

if (isset($_POST['reopen'])) 
{

	// update the closed flag
	reopen_dimension($selected_id);
	meta_forward($_SERVER['PHP_SELF'], "ReopenedID=$selected_id");
}
//-------------------------------------------------------------------------------------

start_form();

start_table(TABLESTYLE2);

if ($selected_id != -1)
{
	$myrow = get_dimension($selected_id, true);

	if ($myrow === false) 
	{
		display_error(_("The dimension sent is not valid."));
		display_footer_exit();
	}

	// if it's a closed dimension can't edit it
	//if ($myrow["closed"] == 1) 
	//{
	//	display_error(_("This dimension is closed and cannot be edited."));
	//	display_footer_exit();
	//}

	$_POST['ref'] = $myrow["reference"];
	$_POST['closed'] = $myrow["closed"];
	$_POST['name'] = $myrow["name"];
	$_POST['type_'] = $myrow["type_"];
	$_POST['date_'] = sql2date($myrow["date_"]);
	$_POST['due_date'] = sql2date($myrow["due_date"]);
	$_POST['memo_'] = get_comments_string(ST_DIMENSION, $selected_id);
	
 	$tags_result = get_tags_associated_with_record(TAG_DIMENSION, $selected_id);
 	$tagids = array();
 	while ($tag = db_fetch($tags_result)) 
 	 	$tagids[] = $tag['id'];
 	$_POST['dimension_tags'] = $tagids;	

	hidden('ref', $_POST['ref']);

	label_row(_("Dimension Reference:"), $_POST['ref']);

	hidden('selected_id', $selected_id);
} 
else 
{
	$_POST['dimension_tags'] = array();
	ref_row(_("Dimension Reference:"), 'ref', '', $Refs->get_next(ST_DIMENSION), false, ST_DIMENSION);
}

text_row_ex(_("Name") . ":", 'name', 50, 75);

$dim = get_company_pref('use_dimension');

number_list_row(_("Type"), 'type_', null, 1, $dim);

date_row(_("Start Date") . ":", 'date_');

date_row(_("Date Required By") . ":", 'due_date', '', null, $SysPrefs->default_dimension_required_by());

tag_list_row(_("Tags:"), 'dimension_tags', 5, TAG_DIMENSION, true);

textarea_row(_("Memo:"), 'memo_', null, 40, 5);

end_table(1);

if (isset($_POST['closed']) && $_POST['closed'] == 1)
	display_note(_("This Dimension is closed."), 0, 0, "class='currentfg'");

if ($selected_id != -1) 
{
	echo "<br>";
	submit_center_first('UPDATE_ITEM', _("Update"), _('Save changes to dimension'), 'default');
	if ($_POST['closed'] == 1)
		submit('reopen', _("Re-open This Dimension"), true, _('Mark this dimension as re-opened'), true);
	else	
		submit('close', _("Close This Dimension"), true, _('Mark this dimension as closed'), true);
	submit_center_last('delete', _("Delete This Dimension"), _('Delete unused dimension'), true);
}
else
{
	submit_center('ADD_ITEM', _("Add"), true, '', 'default');
}
end_form();

//--------------------------------------------------------------------------------------------

end_page();

