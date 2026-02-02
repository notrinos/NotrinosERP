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

$page_security = 'SA_DEPARTMENT';
$path_to_root  = '../..';

include_once($path_to_root.'/includes/session.inc');

include_once($path_to_root.'/includes/ui.inc');
include_once($path_to_root.'/hrm/includes/db/department_db.inc');

//--------------------------------------------------------------------------

page(_($help_context = 'Manage Departments'));

simple_page_mode(false);

if ($Mode=='ADD_ITEM' || $Mode=='UPDATE_ITEM') {

	if(empty(trim($_POST['department_name']))) {
		display_error(_('The Department name cannot be empty.'));
		set_focus('department_name');
	}
	elseif(empty($_POST['payroll_expense_account'])) {
		display_error(_('Please select basic account'));
		set_focus('payroll_expense_account');
	}
	elseif(is_account_balancesheet($_POST['payroll_expense_account'])) {
		display_error(_('Salary Expense Account should not be a balance account.'));
		set_focus('payroll_expense_account');
	}
	else {

		if ($selected_id != '') {
			update_department($selected_id, $_POST['department_name'], $_POST['payroll_expense_account']);
			display_notification(_('Selected department has been updated'));
		}
		else {
			add_department($_POST['department_name'], $_POST['payroll_expense_account']);
			display_notification(_('New department has been added'));
		}
		
		$Mode = 'RESET';
	}
}

if ($Mode == 'Delete') {

	if(key_in_foreign_table($selected_id, 'employees', 'department_id'))
		display_error(_('The Department cannot be deleted.'));
	else {
		delete_department($selected_id);
		display_notification(_('Selected department has been deleted'));
	}
	$Mode = 'RESET';
}

if($Mode == 'RESET') {
	$selected_id = '';
	$_POST['selected_id'] = '';
	$_POST['department_name'] = '';
	$_POST['payroll_expense_account'] = '';
}

//--------------------------------------------------------------------------

start_form();

start_table(TABLESTYLE, "width='40%'");

$th = array(_('Id'), _('Department Name'), _('Salary Expense Account'), '', '');

inactive_control_column($th);
table_header($th);

$result = get_departments(check_value('show_inactive'));

$k = 0;
while ($myrow = db_fetch($result)) {
	alt_table_row_color($k);

	label_cell($myrow['department_id']);
	label_cell($myrow['department_name']);
	$account_name = get_gl_account_name($myrow['payroll_expense_account']);
	label_cell($myrow['payroll_expense_account'].' - '.$account_name);
	inactive_control_cell($myrow['department_id'], $myrow['inactive'], 'departments', 'department_id');
	edit_button_cell('Edit'.$myrow['department_id'], _('Edit'));
	delete_button_cell('Delete'.$myrow['department_id'], _('Delete'));
	end_row();
}
inactive_control_row($th);
end_table(1);

start_table(TABLESTYLE2);

if($selected_id != '') {
	
	if($Mode == 'Edit') {
		
		$myrow = get_department($selected_id);
		$_POST['department_name']  = $myrow['department_name'];
		$_POST['payroll_expense_account'] = $myrow['payroll_expense_account'];
	}
	hidden('selected_id', $selected_id);
}

text_row_ex(_('Department Name:'), 'department_name', 50, 60);

gl_all_accounts_list_row(_('Salary Expense Account:'), 'payroll_expense_account', null, true, false, _('Select an expense account'));

end_table(1);

submit_add_or_update_center($selected_id == '', '', 'both');

end_form();
end_page();
