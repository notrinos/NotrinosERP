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

$page_security = 'SA_OVERTIME';
$path_to_root  = '../..';

include_once($path_to_root.'/includes/session.inc');

include_once($path_to_root.'/includes/ui.inc');
include_once($path_to_root.'/hrm/includes/db/overtime_db.inc');

//--------------------------------------------------------------------------

page(_($help_context = 'Manage Overtime'));

simple_page_mode(false);

if ($Mode=='ADD_ITEM' || $Mode=='UPDATE_ITEM') {

	if(empty(trim($_POST['overtime_name']))) {
		display_error(_('The overtime name cannot be empty.'));
		set_focus('overtime_name');
	}
	elseif(!check_num('pay_rate', 0.0)) {
		display_error(_('Pay rate field value must be a positive number.'));
		set_focus('pay_rate');
	}
	else {

		if ($selected_id != '') {
			update_overtime($selected_id, $_POST['overtime_name'], input_num('pay_rate'));
			display_notification(_('Selected overtime has been updated'));
		}
		else {
			add_overtime($_POST['overtime_name'], input_num('pay_rate'));
			display_notification(_('New overtime has been added'));
		}
		
		$Mode = 'RESET';
	}
}

if ($Mode == 'Delete') {

	if(key_in_foreign_table($selected_id, 'attendance', 'overtime_id'))
		display_error(_('The selected overtime cannot be deleted.'));
	else {
		delete_overtime($selected_id);
		display_notification(_('Selected overtime has been deleted'));
	}
	$Mode = 'RESET';
}

if($Mode == 'RESET') {
	$selected_id = '';
	$_POST['selected_id'] = '';
	$_POST['overtime_name'] = '';
	$_POST['pay_rate'] = '';
}

//--------------------------------------------------------------------------

start_form();

start_table(TABLESTYLE, "width='40%'");

$th = array(_('Id'), _('Overtime Name'), _('Pay rate').'(%)', '', '');

inactive_control_column($th);
table_header($th);

$result = get_all_overtime(check_value('show_inactive'));

$k = 0;
while ($myrow = db_fetch($result)) {
	alt_table_row_color($k);

	label_cell($myrow['overtime_id']);
	label_cell($myrow['overtime_name']);
	percent_cell($myrow['pay_rate']);
	inactive_control_cell($myrow['overtime_id'], $myrow['inactive'], 'overtime', 'overtime_id');
	edit_button_cell('Edit'.$myrow['overtime_id'], _('Edit'));
	delete_button_cell('Delete'.$myrow['overtime_id'], _('Delete'));
	end_row();
}
inactive_control_row($th);
end_table(1);

start_table(TABLESTYLE2);

if($selected_id != '') {
	
	if($Mode == 'Edit') {
		
		$myrow = get_overtime($selected_id);
		$_POST['overtime_name']  = $myrow['overtime_name'];
		$_POST['pay_rate'] = percent_format($myrow['pay_rate']);
	}
	hidden('selected_id', $selected_id);
}

text_row_ex(_('Overtime Name:'), 'overtime_name', 50, 60);
percent_row(_('Pay Rate:'), 'pay_rate');

end_table(1);

submit_add_or_update_center($selected_id == '', '', 'both');

end_form();
end_page();
