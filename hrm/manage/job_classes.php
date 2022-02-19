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

$page_security = 'SA_JOBCLASS';
$path_to_root  = '../..';

include_once($path_to_root.'/includes/session.inc');

include_once($path_to_root.'/includes/ui.inc');
include_once($path_to_root.'/hrm/includes/db/job_classes_db.inc');

//--------------------------------------------------------------------------

page(_($help_context = 'Manage Job Classes'));

simple_page_mode(false);

if ($Mode=='ADD_ITEM' || $Mode=='UPDATE_ITEM') {

	if(empty(trim($_POST['class_name']))) {
		display_error(_('Class name cannot be empty.'));
		set_focus('class_name');
	}
	else {

		if ($selected_id != '') {
			update_job_class($selected_id, $_POST['class_name'], $_POST['pay_basis']);
			display_notification(_('Selected job class has been updated'));
		}
		else {
			add_job_class($_POST['class_name'], $_POST['pay_basis']);
			display_notification(_('New job class has been added'));
		}
		
		$Mode = 'RESET';
	}
}

if ($Mode == 'Delete') {

	if(key_in_foreign_table($selected_id, 'positions', 'job_class_id'))
		display_error(_('The selected job class cannot be deleted.'));
	else {
		delete_job_class($selected_id);
		display_notification(_('Selected job class has been deleted'));
	}
	$Mode = 'RESET';
}

if($Mode == 'RESET') {
	$selected_id = '';
	$_POST['selected_id'] = '';
	$_POST['class_name'] = '';
}

//--------------------------------------------------------------------------

start_form();

start_table(TABLESTYLE, "width='40%'");

$th = array(_('Class Id'), _('Class Name'), _('Pay Basis'), '', '');

inactive_control_column($th);
table_header($th);

$result = get_job_classes(check_value('show_inactive'));

$k = 0;
while ($myrow = db_fetch($result)) {
	alt_table_row_color($k);

	label_cell($myrow['job_class_id']);
	label_cell($myrow['class_name']);
	label_cell($myrow['pay_basis'] === '0' ? _('Monthly') : _('Daily'));
	inactive_control_cell($myrow['job_class_id'], $myrow['inactive'], 'job_classes', 'job_class_id');
	edit_button_cell('Edit'.$myrow['job_class_id'], _('Edit'));
	delete_button_cell('Delete'.$myrow['job_class_id'], _('Delete'));
	end_row();
}
inactive_control_row($th);
end_table(1);

start_table(TABLESTYLE2);

if($selected_id != '') {
	
	if($Mode == 'Edit') {
		
		$myrow = get_job_class($selected_id);
		$_POST['class_name']  = $myrow['class_name'];
		$_POST['pay_basis']  = $myrow['pay_basis'];
	}
	hidden('selected_id', $selected_id);
}

text_row_ex(_('Class Name:'), 'class_name', 50, 60);
label_row(_('Pay Basis:'), radio(_('Monthly salary'), 'pay_basis', 0, 1).'&nbsp;&nbsp;'.radio(_('Daily wage'), 'pay_basis', 1));

end_table(1);

submit_add_or_update_center($selected_id == '', '', 'both');

end_form();
end_page();
