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

$page_security = 'SA_LEAVETYPE';
$path_to_root  = '../..';

include_once($path_to_root.'/includes/session.inc');

include_once($path_to_root.'/includes/ui.inc');
include_once($path_to_root.'/hrm/includes/db/leave_db.inc');

//--------------------------------------------------------------------------

page(_($help_context = 'Manage Leave Types'));

simple_page_mode(false);

if ($Mode=='ADD_ITEM' || $Mode=='UPDATE_ITEM') {

	if(empty(trim($_POST['leave_name']))) {
		display_error(_('Leave name cannot be empty.'));
		set_focus('leave_name');
	}
	elseif(empty(trim($_POST['leave_code'])) || !preg_match("/^[a-zA-Z]+$/", $_POST['leave_code'])) {
		display_error(_('The Leave type code cannot be empty and only allows alphabet letters.'));
		set_focus('leave_code');
	}
	elseif(!check_num('pay_rate', 0.0)) {
		display_error(_('Pay rate field value must be a positive number.'));
		set_focus('pay_rate');
	}
	else {

		if ($selected_id != '') {
			update_leave_type($selected_id, $_POST['leave_name'], $_POST['leave_code'], input_num('pay_rate'));
			display_notification(_('Selected leave type has been updated'));
		}
		else {
			add_leave_type($_POST['leave_name'], $_POST['leave_code'], input_num('pay_rate'));
			display_notification(_('New leave type has been added'));
		}
		
		$Mode = 'RESET';
	}
}

if ($Mode == 'Delete') {

	if(key_in_foreign_table($selected_id, 'attendance', 'leave_id'))
		display_error(_('The selected leave type cannot be deleted.'));
	else {
		delete_leave_type($selected_id);
		display_notification(_('Selected leave type has been deleted'));
	}
	$Mode = 'RESET';
}

if($Mode == 'RESET') {
	$selected_id = '';
	$_POST['selected_id'] = '';
	$_POST['leave_name'] = '';
	$_POST['leave_code'] = '';
	$_POST['pay_rate'] = '';
}

//--------------------------------------------------------------------------

start_form();

start_table(TABLESTYLE, "width='40%'");

$th = array(_('Id'), _('Leave Name'), _('Leave Code'), _('Pay Rate').'(%)', '', '');

inactive_control_column($th);
table_header($th);

$result = get_leave_types(check_value('show_inactive'));

$k = 0;
while ($myrow = db_fetch($result)) {
	alt_table_row_color($k);

	label_cell($myrow['leave_id']);
	label_cell($myrow['leave_name']);
	label_cell($myrow['leave_code']);
	percent_cell($myrow['pay_rate']);
	inactive_control_cell($myrow['leave_id'], $myrow['inactive'], 'leave_types', 'leave_id');
	edit_button_cell('Edit'.$myrow['leave_id'], _('Edit'));
	delete_button_cell('Delete'.$myrow['leave_id'], _('Delete'));
	end_row();
}
inactive_control_row($th);
end_table(1);

start_table(TABLESTYLE2);

if($selected_id != '') {
	
	if($Mode == 'Edit') {
		
		$myrow = get_leave_type($selected_id);
		$_POST['leave_name']  = $myrow['leave_name'];
		$_POST['leave_code']  = $myrow['leave_code'];
		$_POST['pay_rate'] = percent_format($myrow['pay_rate']);
	}
	hidden('selected_id', $selected_id);
}

text_row_ex(_('Leave Type Name:'), 'leave_name', 50, 60);
text_row_ex(_('Leave Type Code'), 'leave_code', 50, 60);
percent_row(_('Pay Rate:'), 'pay_rate');

end_table(1);

submit_add_or_update_center($selected_id == '', '', 'both');

end_form();
end_page();
