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

$page_security = 'SA_PAYGRADE';
$path_to_root  = '../..';

include_once($path_to_root.'/includes/session.inc');

include_once($path_to_root.'/includes/ui.inc');
include_once($path_to_root.'/hrm/includes/db/grade_db.inc');
include_once($path_to_root.'/hrm/includes/db/job_position_db.inc');

//--------------------------------------------------------------------------

page(_($help_context = 'Manage Pay Grades'));

simple_page_mode(false);

if ($Mode=='ADD_ITEM' || $Mode=='UPDATE_ITEM') {

	if(empty(trim($_POST['grade_name']))) {
		display_error(_('Grade name cannot be empty.'));
		set_focus('grade_name');
	}
	elseif(get_post('position_id') == 0) {
		display_error(_('Please select a job position.'));
		set_focus('position_id');
	}
	elseif(!check_num('pay_amount', 0.0)) {
		display_error(_('Pay amount field value must be a positive number.'));
		set_focus('pay_amount');
	}
	else {

		if ($selected_id != '') {
			update_pay_grade($selected_id, $_POST['grade_name'], $_POST['position_id'], input_num('pay_amount'));
			display_notification(_('Selected pay grade has been updated'));
		}
		else {
			add_pay_grade($_POST['grade_name'], $_POST['position_id'], input_num('pay_amount'));
			display_notification(_('New pay grade has been added'));
		}
		
		$Mode = 'RESET';
	}
}

if ($Mode == 'Delete') {

	if(key_in_foreign_table($selected_id, 'employees', 'grade_id'))
		display_error(_('The selected pay grade cannot be deleted.'));
	else {
		delete_pay_grade($selected_id);
		display_notification(_('Selected pay grade has been deleted'));
	}
	$Mode = 'RESET';
}

if($Mode == 'RESET') {
	$selected_id = '';
	$_POST['selected_id'] = '';
	$_POST['grade_name'] = '';
	$_POST['position_id'] = 0;
	$_POST['pay_amount'] = '';
}

//--------------------------------------------------------------------------

start_form();

start_table(TABLESTYLE, "width='40%'");

$result = get_pay_grades(check_value('show_inactive'));

$th = array(_('Position'), _('Basic Amount'), _('Grade ID'), _('Grade Name'), _('Pay Amount'), '', '');
$dec = user_price_dec();

inactive_control_column($th);
table_header($th);

$k = 0;
while ($myrow = db_fetch($result)) {
	alt_table_row_color($k);

	$position = get_job_position($myrow['position_id']);

	label_cell($position['position_name']);
	amount_cell($position['basic_amount'], $dec);
	label_cell($myrow['grade_id']);
	label_cell($myrow['grade_name']);
	amount_cell($myrow['pay_amount'], $dec);
	inactive_control_cell($myrow['grade_id'], $myrow['inactive'], 'pay_grades', 'grade_id');
	edit_button_cell('Edit'.$myrow['grade_id'], _('Edit'));
	delete_button_cell('Delete'.$myrow['grade_id'], _('Delete'));
	end_row();
}
inactive_control_row($th);
end_table(1);

start_table(TABLESTYLE2);

if($selected_id != '') {
	
	if($Mode == 'Edit') {
		
		$myrow = get_pay_grade($selected_id);
		$_POST['grade_name']  = $myrow['grade_name'];
		$_POST['position_id'] = $myrow['position_id'];
		$_POST['pay_amount'] = price_format($myrow['pay_amount'], $dec);
	}
	hidden('selected_id', $selected_id);
}

positions_list_row(_('Job position:'), 'position_id', null, false, _('Select a job position'));
text_row_ex(_('Grade Name:'), 'grade_name', 50, 60);
amount_row(_('Pay Amount:'), 'pay_amount');

end_table(1);

submit_add_or_update_center($selected_id == '', '', 'both');

end_form();
end_page();
