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
include_once($path_to_root.'/hrm/includes/hrm_constants.inc');
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
	elseif((int)get_post('amount_type', 0) == HRM_AMTTYPE_FORMULA && empty(trim(get_post('formula')))) {
		display_error(_('Formula is required when Amount Type is Formula.'));
		set_focus('formula');
	}
	else {
		$extra = array(
			'element_code' => get_post('element_code', ''),
			'element_category' => get_post('element_category', ((int)get_post('is_deduction', 0) ? 2 : 1)),
			'default_amount' => input_num('default_amount'),
			'formula' => get_post('formula', ''),
			'employer_account' => get_post('employer_account', ''),
			'is_taxable' => get_post('is_taxable', 1),
			'affects_gross' => get_post('affects_gross', 1),
			'max_amount' => get_post('max_amount', ''),
			'min_amount' => get_post('min_amount', ''),
			'display_order' => get_post('display_order', 0),
			'description' => get_post('description', '')
		);

		if($selected_id == '') {
			add_pay_element($_POST['element_name'], $_POST['account_code'], $_POST['is_deduction'], $_POST['amount_type'], $extra);
			display_notification(_('Pay element has been added.'));
		}
		else {
			update_pay_element($selected_id, $_POST['element_name'], $_POST['account_code'], $_POST['is_deduction'], $_POST['amount_type'], $extra);
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
	$_POST['element_code'] = '';
	$_POST['element_name'] = '';
	$_POST['element_category'] = 1;
	$_POST['default_amount'] = '';
	$_POST['formula'] = '';
	$_POST['employer_account'] = '';
	$_POST['is_taxable'] = 1;
	$_POST['affects_gross'] = 1;
	$_POST['max_amount'] = '';
	$_POST['min_amount'] = '';
	$_POST['display_order'] = 0;
	$_POST['description'] = '';
}

//--------------------------------------------------------------------------

$result = get_pay_elements();
$categories = hrm_get_element_categories();
$amount_types = hrm_get_amount_types();

start_form();
start_table(TABLESTYLE);
$th = array(_('Code'), _('Element Name'), _('Category'), _('Element Type'), _('Amount Type'), _('Account Code'), _('Account Name'), '', '');

table_header($th);

$k = 0; 
while($myrow = db_fetch($result)) {

	alt_table_row_color($k);

	label_cell($myrow['element_code']);
	label_cell($myrow['element_name']);
	label_cell(isset($categories[$myrow['element_category']]) ? $categories[$myrow['element_category']] : '-');
	label_cell($myrow['is_deduction'] == 0 ? _('Earnings') : _('Deduction'));
	label_cell(isset($amount_types[$myrow['amount_type']]) ? $amount_types[$myrow['amount_type']] : _('Fixed Amount'));
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
		$_POST['element_code']  = @$myrow['element_code'];
		$_POST['element_name']  = $myrow['element_name'];
		$_POST['account_code']  = $myrow['account_code'];
		$_POST['is_deduction'] = $myrow['is_deduction'];
		$_POST['amount_type'] = $myrow['amount_type'];
		$_POST['element_category'] = @$myrow['element_category'];
		$_POST['default_amount'] = @$myrow['default_amount'];
		$_POST['formula'] = @$myrow['formula'];
		$_POST['employer_account'] = @$myrow['employer_account'];
		$_POST['is_taxable'] = isset($myrow['is_taxable']) ? $myrow['is_taxable'] : 1;
		$_POST['affects_gross'] = isset($myrow['affects_gross']) ? $myrow['affects_gross'] : 1;
		$_POST['max_amount'] = @$myrow['max_amount'];
		$_POST['min_amount'] = @$myrow['min_amount'];
		$_POST['display_order'] = @$myrow['display_order'];
		$_POST['description'] = @$myrow['description'];
	}
	hidden('selected_id', $selected_id);
}

text_row_ex(_('Element Code:'), 'element_code', 20, 20);
text_row_ex(_('Element Name:'), 'element_name', 37, 50);
label_row(_('Element Category:'), array_selector('element_category', get_post('element_category', 1), $categories));
gl_all_accounts_list_row(_('Select Account:'), 'account_code', null, true);
gl_all_accounts_list_row(_('Employer Account:'), 'employer_account', null, true, true, _('Optional'));
label_row(_('Element Type:'), radio(_('Earnings'), 'is_deduction', 0, 1).'&nbsp;&nbsp;'.radio(_('Deduction'), 'is_deduction', 1));
label_row(_('Amount Type:'),
	radio(_('Fixed Amount'), 'amount_type', 0, 1).'&nbsp;&nbsp;'
	.radio(_('Percentage of Basic'), 'amount_type', 1).'&nbsp;&nbsp;'
	.radio(_('Percentage of Gross'), 'amount_type', 2).'&nbsp;&nbsp;'
	.radio(_('Formula'), 'amount_type', 3).'&nbsp;&nbsp;'
	.radio(_('Attendance Based'), 'amount_type', 4));
amount_row(_('Default Amount:'), 'default_amount');
text_row(_('Formula:'), 'formula', null, 50, 255);
yesno_list_row(_('Taxable:'), 'is_taxable');
yesno_list_row(_('Affects Gross:'), 'affects_gross');
amount_row(_('Minimum Amount:'), 'min_amount');
amount_row(_('Maximum Amount:'), 'max_amount');
small_amount_row(_('Display Order:'), 'display_order', get_post('display_order', 0), 0, 9999);
textarea_row(_('Description:'), 'description', null, 50, 3);

end_table(1);

submit_add_or_update_center($selected_id == '', '', 'both');

end_form();
end_page();
