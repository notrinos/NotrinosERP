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
$page_security = 'SA_CRMCATEGORY';
$path_to_root = '..';
include($path_to_root.'/includes/session.inc');
include_once($path_to_root.'/includes/db/crm_contacts_db.inc');

page(_($help_context = 'Contact Categories'));

include($path_to_root.'/includes/ui.inc');

simple_page_mode(true);

if ($Mode=='ADD_ITEM' || $Mode=='UPDATE_ITEM') {

	$input_error = 0;

	if (strlen($_POST['description']) == 0) {
		$input_error = 1;
		display_error(_('Category description cannot be empty.'));
		set_focus('description');
	}

	if ($input_error != 1) {
		if ($selected_id != -1) {
			update_crm_category($selected_id, get_post('type'), get_post('subtype'), 
				get_post('name'), get_post('description'));
			$note = _('Selected contact category has been updated');
		} 
		else {
			add_crm_category(get_post('type'), get_post('subtype'), get_post('name'), get_post('description'));
			$note = _('New contact category has been added');
		}

		display_notification($note);
		$Mode = 'RESET';
	}
} 

if ($Mode == 'Delete') {
	$cancel_delete = 0;

	if (is_crm_category_used($selected_id)) {
		$cancel_delete = 1;
		display_error(_('Cannot delete this category because there are contacts related to it.'));
	} 
	if ($cancel_delete == 0) {
		delete_crm_category($selected_id);
		display_notification(_('Category has been deleted'));
	}
	$Mode = 'RESET';
}

if ($Mode == 'RESET') {
	$selected_id = -1;
	$sav = get_post('show_inactive');
	unset($_POST);
	$_POST['show_inactive'] = $sav;
}

//-------------------------------------------------------------------------------------------------

$result = get_crm_categories(check_value('show_inactive'));

start_form();
start_table(TABLESTYLE, "width='70%'");

$th = array(_('Category Type'), _('Category Subtype'), _('Short Name'), _('Description'),  '', '&nbsp;');
inactive_control_column($th);

table_header($th);
$k = 0; 

while ($myrow = db_fetch($result)) {
	
	alt_table_row_color($k);
		
	label_cell($myrow['type']);
	label_cell($myrow['action']);
	label_cell($myrow['name']);
	label_cell($myrow['description']);
	
	inactive_control_cell($myrow['id'], $myrow['inactive'], 'crm_categories', 'id');

	edit_button_cell('Edit'.$myrow['id'], _('Edit'));
	if ($myrow['system'])
		label_cell('');
	else
		delete_button_cell('Delete'.$myrow['id'], _('Delete'));
	end_row();
}
	
inactive_control_row($th);
end_table(1);

//-------------------------------------------------------------------------------------------------

start_table(TABLESTYLE2);

if ($selected_id != -1) {
	if ($Mode == 'Edit') {
		//editing an existing area
		$myrow = get_crm_category($selected_id);

		$_POST['name']  = $myrow['name'];
		$_POST['type']  = $myrow['type'];
		$_POST['subtype']  = $myrow['action'];
		$_POST['description']  = $myrow['description'];
	}
	hidden('selected_id', $selected_id);
} 

if ($Mode == 'Edit' && $myrow['system']) {
	label_row(_('Contact Category Type:'), $_POST['type']);
	label_row(_('Contact Category Subtype:'), $_POST['subtype']);
}
else {
//	crm_category_type_list_row(_("Contact Category Type:"), 'type', null, _('Other'));
	text_row_ex(_('Contact Category Type:'), 'type', 30); 
	text_row_ex(_('Contact Category Subtype:'), 'subtype', 30); 
}

text_row_ex(_('Category Short Name:'), 'name', 30); 
textarea_row(_('Category Description:'), 'description', null, 60, 4);

end_table(1);

submit_add_or_update_center($selected_id == -1, '', 'both');

end_form();

end_page();