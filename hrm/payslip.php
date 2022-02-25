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

$page_security = 'SA_PAYSLIP';
$path_to_root = '..';

include_once($path_to_root.'/hrm/includes/hrm_class.inc');
include_once($path_to_root.'/includes/session.inc');

$js = '';
if ($SysPrefs->use_popup_windows)
	$js .= get_js_open_window(900, 500);
if (user_use_date_picker())
	$js .= get_js_date_picker();

include_once($path_to_root.'/includes/ui.inc');
include_once($path_to_root.'/hrm/includes/ui/payslip_ui.inc');
include_once($path_to_root.'/hrm/includes/db/payslip_db.inc');

if (isset($_GET['ModifyPaySlip'])) {
	$_SESSION['page_title'] = sprintf(_('Modifying Payslip #%d.'), $_GET['trans_no']);
	$help_context = _('Modifying Payslip');
}
else
	$_SESSION['page_title'] = _($help_context = 'Payslip Entry');

page($_SESSION['page_title'], false, false, '', $js);

//--------------------------------------------------------------------------

function line_start_focus() {
	global $Ajax;
	$Ajax->activate('items_table');
	set_focus('_code_id_edit');
}

//--------------------------------------------------------------------------

if (isset($_GET['AddedID'])) {
	
	$trans_no = $_GET['AddedID'];
	$trans_type = ST_PAYSLIP;

	if($payslip_no) {
		display_notification_centered( sprintf(_('Payslip #%d has been entered'), $trans_no));
		display_note(get_gl_view_str($trans_type, $trans_no, _('&View this Transaction')));

		reset_focus();
		hyperlink_params($path_to_root.'/hrm/employee_bank_entry.php', _('Make Payment &Advice for this Payslip'), 'PayslipNo='.$trans_no);
		hyperlink_params($_SERVER['PHP_SELF'], _('Enter &New Payslip'), 'NewPayslip=Yes');

		hyperlink_params($path_to_root.'/admin/attachments.php', _('Add an Attachment'), 'filterType='.$trans_type.'&trans_no='.$trans_no);
	}
	else
		display_error(_('Payslip number does not exist'));
	
	display_footer_exit();
} 
elseif(isset($_GET['UpdatedID'])) {
	
	$trans_no = $_GET['UpdatedID'];
	$trans_type = ST_PAYSLIP;

	display_notification_centered( _('Employee Payslip has been updated #') . $trans_no);
	display_note(get_gl_view_str($trans_type, $trans_no, _('&View this Journal Entry')));

	hyperlink_no_params($path_to_root.'/gl/inquiry/payslip_inquiry.php', _('Return to Payslip &Inquiry'));

	display_footer_exit();
}

//--------------------------------------------------------------------------

if(isset($_GET['NewPayslip']))
	create_cart(ST_PAYSLIP, 0);
elseif(isset($_GET['ModifyPaySlip'])) {
	check_is_editable($_GET['trans_type'], $_GET['trans_no']);
	
	if(!isset($_GET['trans_type']) || $_GET['trans_type'] != 0) {
		
	}
	create_cart($_GET['trans_type'], $_GET['trans_no']);
}

//--------------------------------------------------------------------------

function create_cart($type=80, $trans_no=0) {
	global $Refs;

	if (isset($_SESSION['hrm_items']))
		unset($_SESSION['hrm_items']);

	check_is_closed($type, $trans_no);
	$cart = new hrm_cart($type);
	$cart->trans_no = $trans_no;

	if($trans_no != 0) {

	}
	else {
		$cart->tran_date = new_doc_date();
		if (!is_date_in_fiscalyear($cart->tran_date))
			$cart->tran_date = end_fiscalyear();
		$cart->reference = $Refs->get_next(ST_PAYSLIP, null, $cart->tran_date);
	}

	$_POST['Comments'] = $cart->Comments;
	$_POST['reference'] = $cart->reference;
	$_POST['tran_date'] = $cart->tran_date;

	$_SESSION['hrm_items'] = &$cart;
}

//--------------------------------------------------------------------------

function validate_payslip_generation() {

	if(!$_POST['person_id']) {
		display_error(_('Employee not selected'));
		set_focus('person_id');
		return false;
	} 
	if(!is_date($_POST['from_date'])) {
		display_error(_('The entered date is invalid.'));
		set_focus('from_date');
		return false;
	}
	if(!is_date($_POST['to_date'])) {
		display_error(_('The entered date is invalid.'));
		set_focus('to_date');
		return false;
	}
	if(payslip_generated_for_date($_POST['from_date'], $_POST['person_id'])) {
		display_error(_('Selected date has already paid for this person'));
		set_focus('from_date');
		return false;
	}
	if(payslip_generated_for_date($_POST['to_date'], $_POST['person_id'])) {
		display_error(_('Selected date has already paid for this person'));
		set_focus('to_date');
		return false;
	}
	if(payslip_generated_for_period($_POST['from_date'], $_POST['to_date'], $_POST['person_id'])) {
		display_error(_('Selected period contains a period that has already been paid for this person'));
		set_focus('from_date');
		return false;
	}
	if(date_comp($_POST['from_date'], $_POST['to_date']) > 0) {
		display_error(_('End date cannot be before the start date'));
		set_focus('from_date');
		return false;
	}
	if(date_comp($_POST['from_date'], Today()) > 0) {
		display_error(_('Cannot pay for the date in the future.'));
		set_focus('from_date');
		return false;
	}
	if(date_comp($_POST['to_date'], Today()) > 0) {
		display_error(_('Cannot pay for the date in the future.'));
		set_focus('to_date');
		return false;
	}
	if(!check_employee_hired($_POST['person_id'], $_POST['from_date'])) {
		display_error(_('Cannot pay for the date before hired date'));
		set_focus('from_date');
		return false;
	}
	// The following two cases need to be set in correct order
	if(!employee_has_position($_POST['person_id'])) {
		display_error(_('Selected Employee does not have a Job Position, please define it first.'));
		set_focus('person_id');
		return false;
	}
	elseif(!emp_position_has_structure($_POST['person_id'])) {
		display_error(_("the Employee's Job Position does not have a structure, please define Salary Structure"));
		set_focus('person_id');
		return false;
	}
	return true;
}

//--------------------------------------------------------------------------

if(isset($_POST['GeneratePayslip']) && validate_payslip_generation())
	generate_gl_items($_SESSION['hrm_items']);
	
//--------------------------------------------------------------------------



if(isset($_POST['Process'])) {
	
}

//--------------------------------------------------------------------------



//--------------------------------------------------------------------------

function check_item_data() {
	if(isset($_POST['dimension_id']) && $_POST['dimension_id'] != 0 && dimension_is_closed($_POST['dimension_id'])) {
		display_error(_('Dimension is closed.'));
		set_focus('dimension_id');
		return false;
	}
	if(isset($_POST['dimension2_id']) && $_POST['dimension2_id'] != 0 && dimension_is_closed($_POST['dimension2_id'])) {
		display_error(_('Dimension is closed.'));
		set_focus('dimension2_id');
		return false;
	}
	if(!(input_num('AmountDebit')!=0 ^ input_num('AmountCredit')!=0) ) {
		display_error(_('You must enter either a debit amount or a credit amount.'));
		set_focus('AmountDebit');
			return false;
	}
	if(strlen($_POST['AmountDebit']) && !check_num('AmountDebit', 0)) {
		display_error(_('The debit amount entered is not a valid number or is less than zero.'));
		set_focus('AmountDebit');
		return false;
	}
	elseif(strlen($_POST['AmountCredit']) && !check_num('AmountCredit', 0)) {
		display_error(_('The credit amount entered is not a valid number or is less than zero.'));
		set_focus('AmountCredit');
		return false;
	}
	if(!is_tax_gl_unique(get_post('code_id'))) {
		display_error(_('Cannot post to GL account used by more than one tax type.'));
		set_focus('code_id');
		return false;
	}
	if(!$_SESSION['wa_current_user']->can_access('SA_BANKJOURNAL') && is_bank_account($_POST['code_id'])) {
		display_error(_('Cannot make a journal entry for a bank account. Use banking functions for bank transactions.'));
		set_focus('code_id');
		return false;
	}
	return true;
}

//--------------------------------------------------------------------------

function handle_update_item() {
	
	if($_POST['UpdateItem'] != '' && check_item_data()) {
		if (input_num('AmountDebit') > 0)
			$amount = input_num('AmountDebit');
		else
			$amount = -input_num('AmountCredit');

		$_SESSION['hrm_items']->update_gl_item($_POST['Index'], $_POST['code_id'], $_POST['dimension_id'], $_POST['dimension2_id'], $amount, $_POST['LineMemo'], '', get_post('person_id'));
	}
	line_start_focus();
}

function handle_delete_item($id) {
	$_SESSION['hrm_items']->remove_gl_item($id);
	line_start_focus();
}

function handle_new_item() {

	if(!check_item_data())
		return;

	if(input_num('AmountDebit') > 0)
		$amount = input_num('AmountDebit');
	else
		$amount = -input_num('AmountCredit');
	
	$_SESSION['hrm_items']->add_gl_item($_POST['code_id'], $_POST['dimension_id'], $_POST['dimension2_id'], $amount, $_POST['LineMemo'], '', get_post('person_id'));
	line_start_focus();
}

//--------------------------------------------------------------------------



//--------------------------------------------------------------------------

start_form();

$emp_error = display_payslip_header($_SESSION['hrm_items']);

if(empty($emp_error)) {
	display_order_summary(_('Payslip Elements'), $_SESSION['hrm_items']);
}
else
	display_error($emp_error);

start_table();
textarea_row(_('Comments:'), 'Comments', null, 50, 5);
end_table();

var_dump($_SESSION['hrm_items']);
end_form();
end_page();
