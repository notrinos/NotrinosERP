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
//	Entry/Modify Credit Note for selected Sales Invoice
//

$page_security = 'SA_SALESCREDITINV';
$path_to_root = "..";

include_once($path_to_root . "/sales/includes/cart_class.inc");
include_once($path_to_root . "/includes/session.inc");
include_once($path_to_root . "/includes/data_checks.inc");
include_once($path_to_root . "/sales/includes/sales_db.inc");
include_once($path_to_root . "/sales/includes/sales_ui.inc");
include_once($path_to_root . "/reporting/includes/reporting.inc");

$js = "";
if ($SysPrefs->use_popup_windows) {
	$js .= get_js_open_window(900, 500);
}

if (user_use_date_picker()) {
	$js .= get_js_date_picker();
}

if (isset($_GET['ModifyCredit'])) {
	$_SESSION['page_title'] = sprintf(_("Modifying Credit Invoice # %d."), $_GET['ModifyCredit']);
	$help_context = "Modifying Credit Invoice";
	processing_start();
} elseif (isset($_GET['InvoiceNumber'])) {
	$_SESSION['page_title'] = _($help_context = "Credit all or part of an Invoice");
	processing_start();
}
page($_SESSION['page_title'], false, false, "", $js);

//-----------------------------------------------------------------------------

if (isset($_GET['AddedID'])) {
	$credit_no = $_GET['AddedID'];
	$trans_type = ST_CUSTCREDIT;

	display_notification_centered(_("Credit Note has been processed"));

	display_note(get_customer_trans_view_str($trans_type, $credit_no, _("&View This Credit Note")), 0, 0);

	display_note(print_document_link($credit_no."-".$trans_type, _("&Print This Credit Note"), true, $trans_type),1);
	display_note(print_document_link($credit_no."-".$trans_type, _("&Email This Credit Note"), true, $trans_type, false, "printlink", "", 1),1);

 	display_note(get_gl_view_str($trans_type, $credit_no, _("View the GL &Journal Entries for this Credit Note")),1);

	hyperlink_params("$path_to_root/admin/attachments.php", _("Add an Attachment"), "filterType=$trans_type&trans_no=$credit_no");

	display_footer_exit();

} elseif (isset($_GET['UpdatedID'])) {
	$credit_no = $_GET['UpdatedID'];
	$trans_type = ST_CUSTCREDIT;

	display_notification_centered(_("Credit Note has been updated"));

	display_note(get_customer_trans_view_str($trans_type, $credit_no, _("&View This Credit Note")), 0, 0);

	display_note(print_document_link($credit_no."-".$trans_type, _("&Print This Credit Note"), true, $trans_type),1);
	display_note(print_document_link($credit_no."-".$trans_type, _("&Email This Credit Note"), true, $trans_type, false, "printlink", "", 1),1);

 	display_note(get_gl_view_str($trans_type, $credit_no, _("View the GL &Journal Entries for this Credit Note")),1);

	display_footer_exit();
} else
	check_edit_conflicts(get_post('cart_id'));


//-----------------------------------------------------------------------------

function can_process()
{
	global $Refs;

	if (!is_date($_POST['CreditDate'])) {
		display_error(_("The entered date is invalid."));
		set_focus('CreditDate');
		return false;
	} elseif (!is_date_in_fiscalyear($_POST['CreditDate']))	{
		display_error(_("The entered date is out of fiscal year or is closed for further data entry."));
		set_focus('CreditDate');
		return false;
	}

    if ($_SESSION['Items']->trans_no==0) {
		if (!$Refs->is_valid($_POST['ref'], ST_CUSTCREDIT)) {
			display_error(_("You must enter a reference."));
			set_focus('ref');
			return false;
		}

    }
	if (!check_num('ChargeFreightCost', 0)) {
		display_error(_("The entered shipping cost is invalid or less than zero."));
		set_focus('ChargeFreightCost');
		return false;
	}
	if (!check_quantities()) {
		display_error(_("Selected quantity cannot be less than zero nor more than quantity not credited yet."));
		return false;
	}
	return true;
}

//-----------------------------------------------------------------------------

if (isset($_GET['InvoiceNumber']) && $_GET['InvoiceNumber'] > 0) {

    $_SESSION['Items'] = new Cart(ST_SALESINVOICE, $_GET['InvoiceNumber'], true);
	copy_from_cart();

} elseif ( isset($_GET['ModifyCredit']) && $_GET['ModifyCredit']>0) {

	$_SESSION['Items'] = new Cart(ST_CUSTCREDIT, $_GET['ModifyCredit']);
	copy_from_cart();

} elseif (!processing_active()) {
	/* This page can only be called with an invoice number for crediting*/
	die (_("This page can only be opened if an invoice has been selected for crediting."));
} elseif (!check_quantities()) {
	display_error(_("Selected quantity cannot be less than zero nor more than quantity not credited yet."));
}

function check_quantities()
{
	$ok =1;
	foreach ($_SESSION['Items']->line_items as $line_no=>$itm) {
		if ($itm->quantity == $itm->qty_done) {
			continue; // this line was fully credited/removed
		}
		if (isset($_POST['Line'.$line_no])) {
			if (check_num('Line'.$line_no, 0, $itm->quantity)) {
				$_SESSION['Items']->line_items[$line_no]->qty_dispatched =
				  input_num('Line'.$line_no);
			}
			else {
				$ok = 0;
			}
	  	}

		if (isset($_POST['Line'.$line_no.'Desc'])) {
			$line_desc = $_POST['Line'.$line_no.'Desc'];
			if (strlen($line_desc) > 0) {
				$_SESSION['Items']->line_items[$line_no]->item_description = $line_desc;
			}
	  	}
	}
	return $ok;
}
//-----------------------------------------------------------------------------

function copy_to_cart()
{
	$cart = &$_SESSION['Items'];
	$cart->ship_via = $_POST['ShipperID'];
	$cart->freight_cost = input_num('ChargeFreightCost');
	$cart->document_date =  $_POST['CreditDate'];
	$cart->Location = (isset($_POST['Location']) ? $_POST['Location'] : "");
	$cart->Comments = $_POST['CreditText'];
	if ($_SESSION['Items']->trans_no == 0)
		$cart->reference = $_POST['ref'];
}
//-----------------------------------------------------------------------------

function copy_from_cart()
{
	$cart = &$_SESSION['Items'];
	$_POST['ShipperID'] = $cart->ship_via;
	$_POST['ChargeFreightCost'] = price_format($cart->freight_cost);
	$_POST['CreditDate']= $cart->document_date;
	$_POST['Location']= $cart->Location;
	$_POST['CreditText']= $cart->Comments;
	$_POST['cart_id'] = $cart->cart_id;
	$_POST['ref'] = $cart->reference;
}
//-----------------------------------------------------------------------------

if (isset($_POST['ProcessCredit']) && can_process()) {
	$new_credit = ($_SESSION['Items']->trans_no == 0);

	if (!isset($_POST['WriteOffGLCode']))
		$_POST['WriteOffGLCode'] = 0;

	copy_to_cart();
	if ($new_credit) 
		new_doc_date($_SESSION['Items']->document_date);
	$credit_no = $_SESSION['Items']->write($_POST['WriteOffGLCode']);
	if ($credit_no == -1)
	{
		display_error(_("The entered reference is already in use."));
		set_focus('ref');
	} elseif($credit_no) {
		processing_end();
		if ($new_credit) {
			meta_forward($_SERVER['PHP_SELF'], "AddedID=$credit_no");
		} else {
			meta_forward($_SERVER['PHP_SELF'], "UpdatedID=$credit_no");
		}
	}
}

//-----------------------------------------------------------------------------

if (isset($_POST['Location'])) {
	$_SESSION['Items']->Location = $_POST['Location'];
}

//-----------------------------------------------------------------------------

function display_credit_items()
{
    start_form();
	hidden('cart_id');

	start_table(TABLESTYLE2, "width='80%'", 5);
	echo "<tr><td>"; // outer table

    start_table(TABLESTYLE, "width='100%'");
    start_row();
    label_cells(_("Customer"), $_SESSION['Items']->customer_name, "class='tableheader2'");
	label_cells(_("Branch"), get_branch_name($_SESSION['Items']->Branch), "class='tableheader2'");
    label_cells(_("Currency"), $_SESSION['Items']->customer_currency, "class='tableheader2'");
    end_row();
    start_row();

    if ($_SESSION['Items']->trans_no==0) {
		ref_cells(_("Reference"), 'ref', '', null, "class='tableheader2'", false, ST_CUSTCREDIT,
		array('customer' => $_SESSION['Items']->customer_id,
			'branch' => $_SESSION['Items']->Branch,
			'date' => get_post('CreditDate')));
	} else {
		label_cells(_("Reference"), $_SESSION['Items']->reference, "class='tableheader2'");
	}
    label_cells(_("Crediting Invoice"), get_customer_trans_view_str(ST_SALESINVOICE, array_keys($_SESSION['Items']->src_docs)), "class='tableheader2'");

	if (!isset($_POST['ShipperID'])) {
		$_POST['ShipperID'] = $_SESSION['Items']->ship_via;
	}
	label_cell(_("Shipping Company"), "class='tableheader2'");
	shippers_list_cells(null, 'ShipperID', $_POST['ShipperID']);

	end_row();
	end_table();

    echo "</td><td>";// outer table

    start_table(TABLESTYLE, "width='100%'");

    label_row(_("Invoice Date"), $_SESSION['Items']->src_date, "class='tableheader2'");

    date_row(_("Credit Note Date"), 'CreditDate', '', $_SESSION['Items']->trans_no==0, 0, 0, 0, "class='tableheader2'");

    end_table();

	echo "</td></tr>";

	end_table(1); // outer table

	div_start('credit_items');
    start_table(TABLESTYLE, "width='80%'");
    $th = array(_("Item Code"), _("Item Description"), _("Invoiced Quantity"), _("Units"),
    	_("Credit Quantity"), _("Price"), _("Discount %"), _("Total"));
    table_header($th);

    $k = 0; //row colour counter

    foreach ($_SESSION['Items']->line_items as $line_no=>$ln_itm) {
		if ($ln_itm->quantity == $ln_itm->qty_done) {
			continue; // this line was fully credited/removed
		}
		alt_table_row_color($k);


		//	view_stock_status_cell($ln_itm->stock_id); alternative view
    	label_cell($ln_itm->stock_id);

		text_cells(null, 'Line'.$line_no.'Desc', $ln_itm->item_description, 30, 50);
		$dec = get_qty_dec($ln_itm->stock_id);
    	qty_cell($ln_itm->quantity, false, $dec);
    	label_cell($ln_itm->units);
		amount_cells(null, 'Line'.$line_no, number_format2($ln_itm->qty_dispatched, $dec),
			null, null, $dec);
    	$line_total =($ln_itm->qty_dispatched * $ln_itm->price * (1 - $ln_itm->discount_percent));

    	amount_cell($ln_itm->price);
    	percent_cell($ln_itm->discount_percent*100);
    	amount_cell($line_total);
    	end_row();
    }

    if (!check_num('ChargeFreightCost')) {
    	$_POST['ChargeFreightCost'] = price_format($_SESSION['Items']->freight_cost);
    }
	$colspan = 7;
	start_row();
	label_cell(_("Credit Shipping Cost"), "colspan=$colspan align=right");
	small_amount_cells(null, "ChargeFreightCost", price_format(get_post('ChargeFreightCost',0)));
	end_row();

    $inv_items_total = $_SESSION['Items']->get_items_total_dispatch();

    $display_sub_total = price_format($inv_items_total + input_num($_POST['ChargeFreightCost']));
    label_row(_("Sub-total"), $display_sub_total, "colspan=$colspan align=right", "align=right");

    $taxes = $_SESSION['Items']->get_taxes(input_num($_POST['ChargeFreightCost']));

    $tax_total = display_edit_tax_items($taxes, $colspan, $_SESSION['Items']->tax_included);

    $display_total = price_format(($inv_items_total + input_num('ChargeFreightCost') + $tax_total));

    label_row(_("Credit Note Total"), $display_total, "colspan=$colspan align=right", "align=right");

    end_table();
	div_end();
}

//-----------------------------------------------------------------------------
function display_credit_options()
{
	global $Ajax;
	br();

	if (isset($_POST['_CreditType_update']))
		$Ajax->activate('options');

 	div_start('options');
	start_table(TABLESTYLE2);

	credit_type_list_row(_("Credit Note Type"), 'CreditType', null, true);

	if ($_POST['CreditType'] == "Return")
	{

		/*if the credit note is a return of goods then need to know which location to receive them into */
		if (!isset($_POST['Location']))
			$_POST['Location'] = $_SESSION['Items']->Location;
	   	locations_list_row(_("Items Returned to Location"), 'Location', $_POST['Location']);
	}
	else
	{
		/* the goods are to be written off to somewhere */
		gl_all_accounts_list_row(_("Write off the cost of the items to"), 'WriteOffGLCode', null);
	}

	textarea_row(_("Memo"), "CreditText", null, 51, 3);
	echo "</table>";
 div_end();
}

//-----------------------------------------------------------------------------
if (get_post('Update'))
{
	copy_to_cart();
	$Ajax->activate('credit_items');
}
//-----------------------------------------------------------------------------

display_credit_items();
display_credit_options();

echo "<br><center>";
submit('Update', _("Update"), true, _('Update credit value for quantities entered'), true);
echo "&nbsp";
submit('ProcessCredit', _("Process Credit Note"), true, '', 'default');
echo "</center>";

end_form();


end_page();

