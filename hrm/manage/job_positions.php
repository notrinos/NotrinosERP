<?php
/**********************************************************************
	Copyright (C) NotrinosERP.
	Released under the terms of the GNU General Public License, GPL,
	as published by the Free Software Foundation, either version 3
	of the License, or (at your option) any later version.
	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
	See the License here <http://www.gnu.org/licenses/gpl-3.0.html>.
***********************************************************************/

$page_security = 'SA_POSITION';
$path_to_root  = '../..';

include_once($path_to_root.'/includes/session.inc');

include_once($path_to_root.'/includes/ui.inc');
include_once($path_to_root.'/hrm/includes/db/job_position_db.inc');
include_once($path_to_root.'/hrm/includes/db/job_classes_db.inc');

//--------------------------------------------------------------------------

page(_($help_context = 'Manage Job Positions'));

if(!db_has_job_classes()) {
	display_error(_('No Job Class found in the system, please define Job Classes first.'));
	display_footer_exit();
}

simple_page_mode(false);

if ($Mode=='ADD_ITEM' || $Mode=='UPDATE_ITEM') {

	if(empty(trim($_POST['position_name']))) {
		display_error(_('Position name cannot be empty.'));
		set_focus('position_name');
	}
	elseif(!check_num('basic_amount', 0)) {
		display_error(_('Amount field value must be a positive number.'));
		set_focus('basic_amount');
	}
	else {

		if ($selected_id != '') {
			update_job_position($selected_id, $_POST['position_name'], input_num('basic_amount'), $_POST['job_class_id']);
			display_notification(_('Selected job position has been updated'));
		}
		else {
			add_job_position($_POST['position_name'], input_num('basic_amount'), $_POST['job_class_id']);
			display_notification(_('New job position has been added'));
		}
		
		$Mode = 'RESET';
	}
}

if ($Mode == 'Delete') {

	if(key_in_foreign_table($selected_id, 'employees', 'position_id'))
		display_error(_('The Position cannot be deleted.'));
	else {
		delete_job_position($selected_id);
		display_notification(_('Selected job position has been deleted'));
	}
	$Mode = 'RESET';
}

if($Mode == 'RESET') {
	$selected_id = '';
	$_POST['selected_id'] = '';
	$_POST['position_name'] = '';
	$_POST['basic_amount'] = '';
}

//--------------------------------------------------------------------------

start_form();

start_table(TABLESTYLE, "width='40%'");

$th = array(_('Id'), _('Position Name'), _('Salary Basic Amount'), _('Class'), '', '');

inactive_control_column($th);
table_header($th);

$result = get_job_positions(check_value('show_inactive'));

$k = 0;
while ($myrow = db_fetch($result)) {
	$class_name = get_job_class($myrow['job_class_id'])['class_name'];
	alt_table_row_color($k);
	label_cell($myrow['position_id']);
	label_cell($myrow['position_name']);
	amount_cell($myrow['basic_amount']);
	label_cell($class_name);
	inactive_control_cell($myrow['position_id'], $myrow['inactive'], 'positions', 'position_id');
	edit_button_cell('Edit'.$myrow['position_id'], _('Edit'));
	delete_button_cell('Delete'.$myrow['position_id'], _('Delete'));
	end_row();
}
inactive_control_row($th);
end_table(1);

start_table(TABLESTYLE2);

if($selected_id != '') {
	
	if($Mode == 'Edit') {
		
		$myrow = get_job_position($selected_id);
		$_POST['position_name']  = $myrow['position_name'];
		$_POST['basic_amount'] = price_format($myrow['basic_amount']);
	}
	hidden('selected_id', $selected_id);
}

text_row_ex(_('Position Name:'), 'position_name', 50, 60);
amount_row(_('Salary Basic Amount:'), 'basic_amount', null, null, null, null, true);
job_classes_list_row(_('Job Class:'), 'job_class_id');

end_table(1);

submit_add_or_update_center($selected_id == '', '', 'both');

end_form();
end_page();
