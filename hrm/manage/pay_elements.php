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

$page_security = 'SA_PAYELEMENT';
$path_to_root  = '../..';

include_once($path_to_root.'/includes/session.inc');

include_once($path_to_root.'/includes/ui.inc');
include_once($path_to_root.'/hrm/includes/db/pay_element_db.inc');

//--------------------------------------------------------------------------
	
page(_($help_context = 'Manage Pay Elements'));
simple_page_mode(false);

if ($Mode=='ADD_ITEM' || $Mode=='UPDATE_ITEM') {

	
	if(empty(trim($_POST['element_name']))) {
		display_error(_('Element Name cannot be empty.'));
		set_focus('element_name');
	}
	elseif(check_pay_element_duplicated($selected_id, $_POST['account_code'])) {
		display_error(_('Selected account is being used for another element.'));
		set_focus('account_code');
	}
	else {

		if($selected_id == '') {
			add_pay_element($_POST['element_name'], $_POST['account_code'], $_POST['is_deduction'], $_POST['amount_type']);
			display_notification(_('Pay element has been added.'));
		}
		else {
			update_pay_element($selected_id, $_POST['element_name'], $_POST['account_code'], $_POST['is_deduction'], $_POST['amount_type']);
			display_notification(_('The selected pay element has been updated.'));
		}
		
		$Mode = 'RESET';
	}
}

//--------------------------------------------------------------------------

if($Mode == 'Delete') {

	if(pay_element_used($selected_id))
		display_error(_('Cannot delete this account because payroll rules have been created using it.'));
	else {
		delete_pay_element($selected_id);
		display_notification(_('Selected account has been deleted'));
	}
	$Mode = 'RESET';
}

if($Mode == 'RESET') {
	$selected_id = '';
	$_POST['account_code'] = '';
	$_POST['element_name'] = '';
}

//--------------------------------------------------------------------------

$result = get_pay_elements();

start_form();
start_table(TABLESTYLE2);
$th = array(_('Element Name'), _('Element Type'), _('Amount Type'), _('Account Code'), _('Account Name'), '', '');

table_header($th);

$k = 0; 
while($myrow = db_fetch($result)) {

	alt_table_row_color($k);

	label_cell($myrow['element_name']);
	label_cell($myrow['is_deduction'] == 0 ? _('Earnings') : _('Deduction'));
	label_cell($myrow['amount_type'] == 0 ? _('Fixed Amount') : _('Percentage of Base Pay'));
	label_cell($myrow['account_code'], "align='center'");
	label_cell($myrow['account_name']);
	edit_button_cell('Edit'.$myrow['element_id'], _('Edit'));
	delete_button_cell('Delete'.$myrow['element_id'], _('Delete'));
	
	end_row();
}

end_table(1);

//--------------------------------------------------------------------------

start_table(TABLESTYLE2);

if($selected_id != '') {
	
	if($Mode == 'Edit') {
		$myrow = get_pay_element($selected_id);
		$_POST['element_name']  = $myrow['element_name'];
		$_POST['account_code']  = $myrow['account_code'];
		$_POST['is_deduction'] = $myrow['is_deduction'];
		$_POST['amount_type'] = $myrow['amount_type'];
	}
	hidden('selected_id', $selected_id);
}

text_row_ex(_('Element Name:'), 'element_name', 37, 50);
gl_all_accounts_list_row(_('Select Account:'), 'account_code', null, true);
label_row(_('Element Type:'), radio(_('Earnings'), 'is_deduction', 0, 1).'&nbsp;&nbsp;'.radio(_('Deduction'), 'is_deduction', 1));
label_row(_('Amount Type:'), radio(_('Fixed Amount'), 'amount_type', 0, 1).'&nbsp;&nbsp;'.radio(_('Percentage(%)'), 'amount_type', 1));

end_table(1);

submit_add_or_update_center($selected_id == '', '', 'both');

end_form();
end_page();
