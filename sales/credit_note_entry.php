<?php
/**********************************************************************
    Copyright (C) FrontAccounting, LLC.
	Released under the terms of the GNU General Public License, GPL, 
	as published by the Free Software Foundation, either version 3 
	of the License, or (at your option) any later version.
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  
    See the License here <http://www.gnu.org/licenses/gpl-3.0.html>.
***********************************************************************/
//---------------------------------------------------------------------------
//
//	Entry/Modify free hand Credit Note
//
$page_security = 'SA_SALESCREDIT';
$path_to_root = "..";
include_once($path_to_root . "/sales/includes/cart_class.inc");
include_once($path_to_root . "/includes/session.inc");
include_once($path_to_root . "/includes/data_checks.inc");
include_once($path_to_root . "/sales/includes/sales_db.inc");
include_once($path_to_root . "/sales/includes/sales_ui.inc");
include_once($path_to_root . "/sales/includes/db/sales_types_db.inc");
include_once($path_to_root . "/sales/includes/ui/sales_credit_ui.inc");
include_once($path_to_root . "/sales/includes/ui/sales_order_ui.inc");
include_once($path_to_root . "/reporting/includes/reporting.inc");

$js = "";
if ($SysPrefs->use_popup_windows) {
	$js .= get_js_open_window(900, 500);
}
if (user_use_date_picker()) {
	$js .= get_js_date_picker();
}

if(isset($_GET['NewCredit'])) {
	$_SESSION['page_title'] = _($help_context = "Customer Credit Note");
	handle_new_credit(0);
} elseif (isset($_GET['ModifyCredit'])) {
	$_SESSION['page_title'] = sprintf(_("Modifying Customer Credit Note #%d"), $_GET['ModifyCredit']);
	handle_new_credit($_GET['ModifyCredit']);
	$help_context = "Modifying Customer Credit Note";
}

page($_SESSION['page_title'],false, false, "", $js);

//-----------------------------------------------------------------------------

check_db_has_stock_items(_("There are no items defined in the system."));

check_db_has_customer_branches(_("There are no customers, or there are no customers with branches. Please define customers and customer branches."));

//-----------------------------------------------------------------------------

if (list_updated('branch_id')) {
	// when branch is selected via external editor also customer can change
	$br = get_branch(get_post('branch_id'));
	$_POST['customer_id'] = $br['debtor_no'];
	$Ajax->activate('customer_id');
}

if (isset($_GET['AddedID'])) {
	$credit_no = $_GET['AddedID'];
	$trans_type = ST_CUSTCREDIT;

	display_notification_centered(sprintf(_("Credit Note # %d has been processed"),$credit_no));

	display_note(get_customer_trans_view_str($trans_type, $credit_no, _("&View this credit note")), 0, 1);

	display_note(print_document_link($credit_no."-".$trans_type, _("&Print This Credit Invoice"), true, ST_CUSTCREDIT),0, 1);
	display_note(print_document_link($credit_no."-".$trans_type, _("&Email This Credit Invoice"), true, ST_CUSTCREDIT, false, "printlink", "", 1),0, 1);

	display_note(get_gl_view_str($trans_type, $credit_no, _("View the GL &Journal Entries for this Credit Note")));

	hyperlink_params($_SERVER['PHP_SELF'], _("Enter Another &Credit Note"), "NewCredit=yes");

	hyperlink_params("$path_to_root/admin/attachments.php", _("Add an Attachment"), "filterType=$trans_type&trans_no=$credit_no");

	display_footer_exit();
} else
	check_edit_conflicts(get_post('cart_id'));

//--------------------------------------------------------------------------------

function line_start_focus() {
  	global $Ajax;
  	$Ajax->activate('items_table');
  	set_focus('_stock_id_edit');
}

//-----------------------------------------------------------------------------

function copy_to_cn()
{
	$cart = &$_SESSION['Items'];
	$cart->Comments = $_POST['CreditText'];
	$cart->document_date = $_POST['OrderDate'];
	$cart->freight_cost = input_num('ChargeFreightCost');
	$cart->Location = (isset($_POST["Location"]) ? $_POST["Location"] : "");
	$cart->sales_type = $_POST['sales_type_id'];
	if ($cart->trans_no == 0)
		$cart->reference = $_POST['ref'];
	$cart->ship_via = $_POST['ShipperID'];
	$cart->dimension_id = $_POST['dimension_id'];
	$cart->dimension2_id = $_POST['dimension2_id'];
}

//-----------------------------------------------------------------------------

function copy_from_cn()
{
	$cart = &$_SESSION['Items'];
	$_POST['CreditText'] = $cart->Comments;
	$_POST['OrderDate'] = $cart->document_date;
	$_POST['ChargeFreightCost'] = price_format($cart->freight_cost);
	$_POST['Location'] = $cart->Location;
	$_POST['sales_type_id'] = $cart->sales_type;
	if ($cart->trans_no == 0)
		$_POST['ref'] = $cart->reference;
	$_POST['ShipperID'] = $cart->ship_via;
	$_POST['dimension_id'] = $cart->dimension_id;
	$_POST['dimension2_id'] = $cart->dimension2_id;
	$_POST['cart_id'] = $cart->cart_id;
}

//-----------------------------------------------------------------------------

function handle_new_credit($trans_no)
{
	processing_start();
	$_SESSION['Items'] = new Cart(ST_CUSTCREDIT,$trans_no);
	copy_from_cn();
}

//-----------------------------------------------------------------------------

function can_process()
{
	global $Refs;

	$input_error = 0;

	if ($_SESSION['Items']->count_items() == 0 && (!check_num('ChargeFreightCost',0)))
		return false;
	if($_SESSION['Items']->trans_no == 0) {
	    if (!$Refs->is_valid($_POST['ref'], ST_CUSTCREDIT)) {
			display_error( _("You must enter a reference."));
			set_focus('ref');
			$input_error = 1;
		}
	}
	if (!is_date($_POST['OrderDate'])) {
		display_error(_("The entered date for the credit note is invalid."));
		set_focus('OrderDate');
		$input_error = 1;
	} elseif (!is_date_in_fiscalyear($_POST['OrderDate'])) {
		display_error(_("The entered date is out of fiscal year or is closed for further data entry."));
		set_focus('OrderDate');
		$input_error = 1;
	}
	return ($input_error == 0);
}

//-----------------------------------------------------------------------------

if (isset($_POST['ProcessCredit']) && can_process()) {
	copy_to_cn();
	if ($_POST['CreditType'] == "WriteOff" && (!isset($_POST['WriteOffGLCode']) ||
		$_POST['WriteOffGLCode'] == '')) {
		display_note(_("For credit notes created to write off the stock, a general ledger account is required to be selected."), 1, 0);
		display_note(_("Please select an account to write the cost of the stock off to, then click on Process again."), 1, 0);
		exit;
	}
	if (!isset($_POST['WriteOffGLCode'])) {
		$_POST['WriteOffGLCode'] = 0;
	}
	copy_to_cn();
	$credit_no = $_SESSION['Items']->write($_POST['WriteOffGLCode']);
	if ($credit_no == -1)
	{
		display_error(_("The entered reference is already in use."));
		set_focus('ref');
	}
	else
	{
		new_doc_date($_SESSION['Items']->document_date);
		processing_end();
		meta_forward($_SERVER['PHP_SELF'], "AddedID=$credit_no");
	}
} /*end of process credit note */

  //-----------------------------------------------------------------------------

function check_item_data()
{
	if (!check_num('qty',0)) {
		display_error(_("The quantity must be greater than zero."));
		set_focus('qty');
		return false;
	}
	if (!check_num('price',0)) {
		display_error(_("The entered price is negative or invalid."));
		set_focus('price');
		return false;
	}
	if (!check_num('Disc', 0, 100)) {
		display_error(_("The entered discount percent is negative, greater than 100 or invalid."));
		set_focus('Disc');
		return false;
	}
	return true;
}

//-----------------------------------------------------------------------------

function handle_update_item()
{
	if ($_POST['UpdateItem'] != "" && check_item_data()) {
		$_SESSION['Items']->update_cart_item($_POST['line_no'], input_num('qty'),
			input_num('price'), input_num('Disc') / 100);
	}
    line_start_focus();
}

//-----------------------------------------------------------------------------

function handle_delete_item($line_no)
{
	$_SESSION['Items']->remove_from_cart($line_no);
    line_start_focus();
}

//-----------------------------------------------------------------------------

function handle_new_item()
{

	if (!check_item_data())
		return;

	add_to_order($_SESSION['Items'], $_POST['stock_id'], input_num('qty'),
		input_num('price'), input_num('Disc') / 100);
    line_start_focus();
}
//-----------------------------------------------------------------------------
$id = find_submit('Delete');
if ($id!=-1)
	handle_delete_item($id);

if (isset($_POST['AddItem']))
	handle_new_item();

if (isset($_POST['UpdateItem']))
	handle_update_item();

if (isset($_POST['CancelItemChanges']))
	line_start_focus();

//-----------------------------------------------------------------------------

if (!processing_active()) {
	handle_new_credit(0);
}

//-----------------------------------------------------------------------------

start_form();
hidden('cart_id');

$customer_error = display_credit_header($_SESSION['Items']);

if ($customer_error == "") {
	start_table(TABLESTYLE, "width='80%'", 10);
	echo "<tr><td>";
	display_credit_items(_("Credit Note Items"), $_SESSION['Items']);
	credit_options_controls($_SESSION['Items']);
	echo "</td></tr>";
	end_table();
} else {
	display_error($customer_error);
}

echo "<br><center><table><tr>";
submit_cells('Update', _("Update"));
submit_cells('ProcessCredit', _("Process Credit Note"), '', false, 'default');
echo "</tr></table></center>";

end_form();
end_page();

