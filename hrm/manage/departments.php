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
include_once($path_to_root.'/hrm/includes/db/departments_entity.inc');

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
	elseif (get_post('parent_department_id') && $Mode=='UPDATE_ITEM' && get_post('parent_department_id') == $selected_id) {
		display_error(_('A department cannot be parent of itself.'));
		set_focus('parent_department_id');
	}
	else {
		$data = array(
			'department_code' => get_post('department_code', ''),
			'department_name' => $_POST['department_name'],
			'parent_department_id' => (int)get_post('parent_department_id', 0) ?: null,
			'manager_employee_id' => get_post('manager_employee_id', '') ?: null,
			'cost_center_id' => (int)get_post('cost_center_id', 0),
			'payroll_expense_account' => $_POST['payroll_expense_account'],
			'payroll_liability_account' => get_post('payroll_liability_account', '') ?: null,
			'description' => get_post('description', ''),
		);

		if ($selected_id != '') {
			departments_entity::modify($selected_id, $data);
			display_notification(_('Selected department has been updated'));
		}
		else {
			departments_entity::create($data);
			display_notification(_('New department has been added'));
		}
		
		$Mode = 'RESET';
	}
}

if ($Mode == 'Delete') {

	if(key_in_foreign_table($selected_id, 'employees', 'department_id'))
		display_error(_('The Department cannot be deleted.'));
	else if (departments_entity::has_children($selected_id))
		display_error(_('The Department cannot be deleted.'));
	else if (!departments_entity::remove($selected_id))
		display_error(_('The Department cannot be deleted.'));
	else
		display_notification(_('Selected department has been deleted'));
	$Mode = 'RESET';
}

if($Mode == 'RESET') {
	$selected_id = '';
	$_POST['selected_id'] = '';
	$_POST['department_name'] = '';
	$_POST['payroll_expense_account'] = '';
	$_POST['department_code'] = '';
	$_POST['parent_department_id'] = 0;
	$_POST['manager_employee_id'] = '';
	$_POST['cost_center_id'] = 0;
	$_POST['payroll_liability_account'] = '';
	$_POST['description'] = '';
}

//--------------------------------------------------------------------------

start_form();

start_table(TABLESTYLE, "width='75%'");

$th = array(
	_('Id'),
	_('Code'),
	_('Department Name'),
	_('Parent'),
	_('Manager'),
	_('Salary Expense Account'),
	_('Payroll Liability Account'),
	'',
	''
);

inactive_control_column($th);
table_header($th);

$result = departments_entity::all_db_resource(check_value('show_inactive') ? '1' : '!inactive');

$k = 0;
while ($myrow = db_fetch($result)) {
	alt_table_row_color($k);

	label_cell($myrow['department_id']);
	label_cell($myrow['department_code']);
	label_cell($myrow['department_name']);
	label_cell(@$myrow['parent_department_name']);
	label_cell($myrow['manager_employee_id']);
	$account_name = get_gl_account_name($myrow['payroll_expense_account']);
	label_cell($myrow['payroll_expense_account'].' - '.$account_name);
	$liability_acc = $myrow['payroll_liability_account'];
	if (!empty($liability_acc))
		label_cell($liability_acc.' - '.get_gl_account_name($liability_acc));
	else
		label_cell('');
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
		
		$myrow = departments_entity::find($selected_id);
		$_POST['department_code'] = @$myrow['department_code'];
		$_POST['department_name']  = $myrow['department_name'];
		$_POST['payroll_expense_account'] = $myrow['payroll_expense_account'];
		$_POST['parent_department_id'] = (int)@$myrow['parent_department_id'];
		$_POST['manager_employee_id'] = @$myrow['manager_employee_id'];
		$_POST['cost_center_id'] = (int)@$myrow['cost_center_id'];
		$_POST['payroll_liability_account'] = @$myrow['payroll_liability_account'];
		$_POST['description'] = @$myrow['description'];
	}
	hidden('selected_id', $selected_id);
}

text_row_ex(_('Department Code:'), 'department_code', 20, 20);

text_row_ex(_('Department Name:'), 'department_name', 50, 60);

$parent_sql = "SELECT department_id, department_name FROM ".TB_PREF."departments";
if ($selected_id != '')
	$parent_sql .= " WHERE department_id <> ".db_escape($selected_id);
label_row(_('Parent Department:'), combo_input('parent_department_id', get_post('parent_department_id', 0), $parent_sql, 'department_id', 'department_name', array(
	'spec_option' => _('-- None --'), 'spec_id' => 0
)));

text_row(_('Manager Employee ID:'), 'manager_employee_id', null, 20, 20);

dimensions_list_row(_('Cost Center:'), 'cost_center_id', null, true, ' ', false, 1, false);

gl_all_accounts_list_row(_('Salary Expense Account:'), 'payroll_expense_account', null, true, false, _('Select an expense account'));

gl_all_accounts_list_row(_('Payroll Liability Account:'), 'payroll_liability_account', null, true, true, _('Select liability account'));

textarea_row(_('Description:'), 'description', null, 50, 3);

end_table(1);

submit_add_or_update_center($selected_id == '', '', 'both');

end_form();
end_page();
