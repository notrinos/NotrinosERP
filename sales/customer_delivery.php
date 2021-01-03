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
//-----------------------------------------------------------------------------
//
//	Entry/Modify Delivery Note against Sales Order
//
$page_security = 'SA_SALESDELIVERY';
$path_to_root = "..";

include_once($path_to_root . "/sales/includes/cart_class.inc");
include_once($path_to_root . "/includes/session.inc");
include_once($path_to_root . "/includes/data_checks.inc");
include_once($path_to_root . "/sales/includes/sales_db.inc");
include_once($path_to_root . "/sales/includes/sales_ui.inc");
include_once($path_to_root . "/reporting/includes/reporting.inc");
include_once($path_to_root . "/taxes/tax_calc.inc");

$js = "";
if ($SysPrefs->use_popup_windows) {
	$js .= get_js_open_window(900, 500);
}
if (user_use_date_picker()) {
	$js .= get_js_date_picker();
}

if (isset($_GET['ModifyDelivery'])) {
	$_SESSION['page_title'] = sprintf(_("Modifying Delivery Note # %d."), $_GET['ModifyDelivery']);
	$help_context = "Modifying Delivery Note";
	processing_start();
} elseif (isset($_GET['OrderNumber'])) {
	$_SESSION['page_title'] = _($help_context = "Deliver Items for a Sales Order");
	processing_start();
}

page($_SESSION['page_title'], false, false, "", $js);

if (isset($_GET['AddedID'])) {
	$dispatch_no = $_GET['AddedID'];

	display_notification_centered(sprintf(_("Delivery # %d has been entered."),$dispatch_no));

	display_note(get_customer_trans_view_str(ST_CUSTDELIVERY, $dispatch_no, _("&View This Delivery")), 0, 1);

	display_note(print_document_link($dispatch_no, _("&Print Delivery Note"), true, ST_CUSTDELIVERY));
	display_note(print_document_link($dispatch_no, _("&Email Delivery Note"), true, ST_CUSTDELIVERY, false, "printlink", "", 1), 1, 1);
	display_note(print_document_link($dispatch_no, _("P&rint as Packing Slip"), true, ST_CUSTDELIVERY, false, "printlink", "", 0, 1));
	display_note(print_document_link($dispatch_no, _("E&mail as Packing Slip"), true, ST_CUSTDELIVERY, false, "printlink", "", 1, 1), 1);

	display_note(get_gl_view_str(13, $dispatch_no, _("View the GL Journal Entries for this Dispatch")),1);

	if (!isset($_GET['prepaid']))
		hyperlink_params("$path_to_root/sales/customer_invoice.php", _("Invoice This Delivery"), "DeliveryNumber=$dispatch_no");

	hyperlink_params("$path_to_root/sales/inquiry/sales_orders_view.php", _("Select Another Order For Dispatch"), "OutstandingOnly=1");

	display_footer_exit();

} elseif (isset($_GET['UpdatedID'])) {

	$delivery_no = $_GET['UpdatedID'];

	display_notification_centered(sprintf(_('Delivery Note # %d has been updated.'),$delivery_no));

	display_note(get_trans_view_str(ST_CUSTDELIVERY, $delivery_no, _("View this delivery")), 0, 1);

	display_note(print_document_link($delivery_no, _("&Print Delivery Note"), true, ST_CUSTDELIVERY));
	display_note(print_document_link($delivery_no, _("&Email Delivery Note"), true, ST_CUSTDELIVERY, false, "printlink", "", 1), 1, 1);
	display_note(print_document_link($delivery_no, _("P&rint as Packing Slip"), true, ST_CUSTDELIVERY, false, "printlink", "", 0, 1));
	display_note(print_document_link($delivery_no, _("E&mail as Packing Slip"), true, ST_CUSTDELIVERY, false, "printlink", "", 1, 1), 1);

	if (!isset($_GET['prepaid']))
		hyperlink_params($path_to_root . "/sales/customer_invoice.php", _("Confirm Delivery and Invoice"), "DeliveryNumber=$delivery_no");

	hyperlink_params($path_to_root . "/sales/inquiry/sales_deliveries_view.php", _("Select A Different Delivery"), "OutstandingOnly=1");

	display_footer_exit();
}
//-----------------------------------------------------------------------------

if (isset($_GET['OrderNumber']) && $_GET['OrderNumber'] > 0) {

	$ord = new Cart(ST_SALESORDER, $_GET['OrderNumber'], true);
	if ($ord->is_prepaid())
		check_deferred_income_act(_("You have to set Deferred Income Account in GL Setup to entry prepayment invoices."));

	if ($ord->count_items() == 0) {
		hyperlink_params($path_to_root . "/sales/inquiry/sales_orders_view.php",
			_("Select a different sales order to delivery"), "OutstandingOnly=1");
		echo "<br><center><b>" . _("This order has no items. There is nothing to delivery.") .
			"</center></b>";
		display_footer_exit();
	} else if (!$ord->is_released()) {
		hyperlink_params($path_to_root . "/sales/inquiry/sales_orders_view.php",_("Select a different sales order to delivery"),
			"OutstandingOnly=1");
		echo "<br><center><b>"._("This prepayment order is not yet ready for delivery due to insufficient amount received.")
			."</center></b>";
		display_footer_exit();
	}
 	// Adjust Shipping Charge based upon previous deliveries TAM
	adjust_shipping_charge($ord, $_GET['OrderNumber']);
 
	$_SESSION['Items'] = $ord;
	copy_from_cart();

} elseif (isset($_GET['ModifyDelivery']) && $_GET['ModifyDelivery'] > 0) {

	check_is_editable(ST_CUSTDELIVERY, $_GET['ModifyDelivery']);
	$_SESSION['Items'] = new Cart(ST_CUSTDELIVERY,$_GET['ModifyDelivery']);

	if (!$_SESSION['Items']->prepaid && $_SESSION['Items']->count_items() == 0) {
		hyperlink_params($path_to_root . "/sales/inquiry/sales_orders_view.php",
			_("Select a different delivery"), "OutstandingOnly=1");
		echo "<br><center><b>" . _("This delivery has all items invoiced. There is nothing to modify.") .
			"</center></b>";
		display_footer_exit();
	}

	copy_from_cart();
	
} elseif ( !processing_active() ) {
	/* This page can only be called with an order number for invoicing*/

	display_error(_("This page can only be opened if an order or delivery note has been selected. Please select it first."));

	hyperlink_params("$path_to_root/sales/inquiry/sales_orders_view.php", _("Select a Sales Order to Delivery"), "OutstandingOnly=1");

	end_page();
	exit;

} else {
	check_edit_conflicts(get_post('cart_id'));

	if (!check_quantities()) {
		display_error(_("Selected quantity cannot be less than quantity invoiced nor more than quantity	not dispatched on sales order."));

	} elseif(!check_num('ChargeFreightCost', 0)) {
		display_error(_("Freight cost cannot be less than zero"));
		set_focus('ChargeFreightCost');
	}
}

//-----------------------------------------------------------------------------

function check_data()
{
	global $Refs, $SysPrefs;

	if (!isset($_POST['DispatchDate']) || !is_date($_POST['DispatchDate']))	{
		display_error(_("The entered date of delivery is invalid."));
		set_focus('DispatchDate');
		return false;
	}

	if (!is_date_in_fiscalyear($_POST['DispatchDate'])) {
		display_error(_("The entered date is out of fiscal year or is closed for further data entry."));
		set_focus('DispatchDate');
		return false;
	}

	if (!isset($_POST['due_date']) || !is_date($_POST['due_date']))	{
		display_error(_("The entered dead-line for invoice is invalid."));
		set_focus('due_date');
		return false;
	}

	if ($_SESSION['Items']->trans_no==0) {
		if (!$Refs->is_valid($_POST['ref'], ST_CUSTDELIVERY)) {
			display_error(_("You must enter a reference."));
			set_focus('ref');
			return false;
		}
	}
	if ($_POST['ChargeFreightCost'] == "") {
		$_POST['ChargeFreightCost'] = price_format(0);
	}

	if (!check_num('ChargeFreightCost',0)) {
		display_error(_("The entered shipping value is not numeric."));
		set_focus('ChargeFreightCost');
		return false;
	}

	if ($_SESSION['Items']->has_items_dispatch() == 0 && input_num('ChargeFreightCost') == 0) {
		display_error(_("There are no item quantities on this delivery note."));
		return false;
	}

	if (!check_quantities()) {
		return false;
	}

	copy_to_cart();

	if (!$SysPrefs->allow_negative_stock() && ($low_stock = $_SESSION['Items']->check_qoh()))
	{
		display_error(_("This document cannot be processed because there is insufficient quantity for items marked."));
		return false;
	}

	return true;
}
//------------------------------------------------------------------------------
function copy_to_cart()
{
	$cart = &$_SESSION['Items'];
	$cart->ship_via = $_POST['ship_via'];
	$cart->freight_cost = input_num('ChargeFreightCost');
	$cart->document_date = $_POST['DispatchDate'];
	$cart->due_date =  $_POST['due_date'];
	$cart->Location = $_POST['Location'];
	$cart->Comments = $_POST['Comments'];
	$cart->dimension_id = $_POST['dimension_id'];
	$cart->dimension2_id = $_POST['dimension2_id'];
	if ($cart->trans_no == 0)
		$cart->reference = $_POST['ref'];

}
//------------------------------------------------------------------------------

function copy_from_cart()
{
	$cart = &$_SESSION['Items'];
	$_POST['ship_via'] = $cart->ship_via;
	$_POST['ChargeFreightCost'] = price_format($cart->freight_cost);
	$_POST['DispatchDate'] = $cart->document_date;
	$_POST['due_date'] = $cart->due_date;
	$_POST['Location'] = $cart->Location;
	$_POST['Comments'] = $cart->Comments;
	$_POST['dimension_id'] = $cart->dimension_id;
	$_POST['dimension2_id'] = $cart->dimension2_id;
	$_POST['cart_id'] = $cart->cart_id;
	$_POST['ref'] = $cart->reference;
}
//------------------------------------------------------------------------------

function check_quantities()
{
	$ok =1;
	// Update cart delivery quantities/descriptions
	foreach ($_SESSION['Items']->line_items as $line=>$itm) {
		if (isset($_POST['Line'.$line])) {
			if($_SESSION['Items']->trans_no) {
				$min = $itm->qty_done;
				$max = $itm->quantity;
			} else {
				$min = 0;
				$max = $itm->quantity - $itm->qty_done;
			}

			if (check_num('Line'.$line, $min, $max)) {
				$_SESSION['Items']->line_items[$line]->qty_dispatched =
				  input_num('Line'.$line);
			} else {
				set_focus('Line'.$line);
				$ok = 0;
			}

		}

		if (isset($_POST['Line'.$line.'Desc'])) {
			$line_desc = $_POST['Line'.$line.'Desc'];
			if (strlen($line_desc) > 0) {
				$_SESSION['Items']->line_items[$line]->item_description = $line_desc;
			}
		}
	}
	return $ok;
}

//------------------------------------------------------------------------------

if (isset($_POST['process_delivery']) && check_data()) {
	$dn = &$_SESSION['Items'];

	if ($_POST['bo_policy']) {
		$bo_policy = 0;
	} else {
		$bo_policy = 1;
	}
	$newdelivery = ($dn->trans_no == 0);

	if ($newdelivery)
		new_doc_date($dn->document_date);

	$delivery_no = $dn->write($bo_policy);

	if ($delivery_no == -1)
	{
		display_error(_("The entered reference is already in use."));
		set_focus('ref');
	}
	else
	{
		$is_prepaid = $dn->is_prepaid() ? "&prepaid=Yes" : '';

		processing_end();
		if ($newdelivery) {
			meta_forward($_SERVER['PHP_SELF'], "AddedID=$delivery_no$is_prepaid");
		} else {
			meta_forward($_SERVER['PHP_SELF'], "UpdatedID=$delivery_no$is_prepaid");
		}
	}
}

if (isset($_POST['Update']) || isset($_POST['_Location_update']) || isset($_POST['qty']) || isset($_POST['process_delivery'])) {
	$Ajax->activate('Items');
}
//------------------------------------------------------------------------------
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
	ref_cells(_("Reference"), 'ref', '', null, "class='tableheader2'", false, ST_CUSTDELIVERY,
	array('customer' => $_SESSION['Items']->customer_id,
			'branch' => $_SESSION['Items']->Branch,
			'date' => get_post('DispatchDate')));
} else {
	label_cells(_("Reference"), $_SESSION['Items']->reference, "class='tableheader2'");
}

label_cells(_("For Sales Order"), get_customer_trans_view_str(ST_SALESORDER, $_SESSION['Items']->order_no), "class='tableheader2'");

label_cells(_("Sales Type"), $_SESSION['Items']->sales_type_name, "class='tableheader2'");
end_row();
start_row();

if (!isset($_POST['Location'])) {
	$_POST['Location'] = $_SESSION['Items']->Location;
}
label_cell(_("Delivery From"), "class='tableheader2'");
locations_list_cells(null, 'Location', null, false, true);

if (!isset($_POST['ship_via'])) {
	$_POST['ship_via'] = $_SESSION['Items']->ship_via;
}
label_cell(_("Shipping Company"), "class='tableheader2'");
shippers_list_cells(null, 'ship_via', $_POST['ship_via']);

// set this up here cuz it's used to calc qoh
if (!isset($_POST['DispatchDate']) || !is_date($_POST['DispatchDate'])) {
	$_POST['DispatchDate'] = new_doc_date();
	if (!is_date_in_fiscalyear($_POST['DispatchDate'])) {
		$_POST['DispatchDate'] = end_fiscalyear();
	}
}
date_cells(_("Date"), 'DispatchDate', '', $_SESSION['Items']->trans_no==0, 0, 0, 0, "class='tableheader2'");
end_row();

end_table();

echo "</td><td>";// outer table

start_table(TABLESTYLE, "width='90%'");

if (!isset($_POST['due_date']) || !is_date($_POST['due_date'])) {
	$_POST['due_date'] = get_invoice_duedate($_SESSION['Items']->payment, $_POST['DispatchDate']);
}
customer_credit_row($_SESSION['Items']->customer_id, $_SESSION['Items']->credit, "class='tableheader2'");

$dim = get_company_pref('use_dimension');
if ($dim > 0) {
	start_row();
	label_cell(_("Dimension").":", "class='tableheader2'");
	dimensions_list_cells(null, 'dimension_id', null, true, ' ', false, 1, false);
	end_row();
}		
else
	hidden('dimension_id', 0);
if ($dim > 1) {
	start_row();
	label_cell(_("Dimension")." 2:", "class='tableheader2'");
	dimensions_list_cells(null, 'dimension2_id', null, true, ' ', false, 2, false);
	end_row();
}		
else
	hidden('dimension2_id', 0);
//---------
start_row();
date_cells(_("Invoice Dead-line"), 'due_date', '', null, 0, 0, 0, "class='tableheader2'");
end_row();
end_table();

echo "</td></tr>";
end_table(1); // outer table

$row = get_customer_to_order($_SESSION['Items']->customer_id);
if ($row['dissallow_invoices'] == 1)
{
	display_error(_("The selected customer account is currently on hold. Please contact the credit control personnel to discuss."));
	end_form();
	end_page();
	exit();
}	
display_heading(_("Delivery Items"));
div_start('Items');
start_table(TABLESTYLE, "width='80%'");

$new = $_SESSION['Items']->trans_no==0;
$th = array(_("Item Code"), _("Item Description"), 
	$new ? _("Ordered") : _("Max. delivery"), _("Units"), $new ? _("Delivered") : _("Invoiced"),
	_("This Delivery"), _("Price"), _("Tax Type"), _("Discount"), _("Total"));

table_header($th);
$k = 0;
$has_marked = false;

foreach ($_SESSION['Items']->line_items as $line=>$ln_itm) {
	if ($ln_itm->quantity==$ln_itm->qty_done) {
		continue; //this line is fully delivered
	}
	if(isset($_POST['_Location_update']) || isset($_POST['clear_quantity']) || isset($_POST['reset_quantity'])) {
		// reset quantity
		$ln_itm->qty_dispatched = $ln_itm->quantity-$ln_itm->qty_done;
	}
	// if it's a non-stock item (eg. service) don't show qoh
	$row_classes = null;
	if (has_stock_holding($ln_itm->mb_flag) && $ln_itm->qty_dispatched) {
		// It's a stock : call get_dispatchable_quantity hook  to get which quantity to preset in the
		// quantity input box. This allows for example a hook to modify the default quantity to what's dispatchable
		// (if there is not enough in hand), check at other location or other order people etc ...
		// This hook also returns a 'reason' (css classes) which can be used to theme the row.
		//
		// FIXME: hook_get_dispatchable definition does not allow qoh checks on transaction level
		// (but anyway dispatch is checked again later before transaction is saved)

		$qty = $ln_itm->qty_dispatched;
		if ($check = check_negative_stock($ln_itm->stock_id, $ln_itm->qty_done-$ln_itm->qty_dispatched, $_POST['Location'], $_POST['DispatchDate']))
			$qty = $check['qty'];

		$q_class =  hook_get_dispatchable_quantity($ln_itm, $_POST['Location'], $_POST['DispatchDate'], $qty);

		// Skip line if needed
		if($q_class === 'skip')  continue;
		if(is_array($q_class)) {
		  list($ln_itm->qty_dispatched, $row_classes) = $q_class;
			$has_marked = true;
		}
	}

	alt_table_row_color($k, $row_classes);
	view_stock_status_cell($ln_itm->stock_id);

	if ($ln_itm->descr_editable)
		text_cells(null, 'Line'.$line.'Desc', $ln_itm->item_description, 30, 50);
	else
		label_cell($ln_itm->item_description);

	$dec = get_qty_dec($ln_itm->stock_id);
	qty_cell($ln_itm->quantity, false, $dec);
	label_cell($ln_itm->units);
	qty_cell($ln_itm->qty_done, false, $dec);

	if(isset($_POST['clear_quantity'])) {
		$ln_itm->qty_dispatched = 0;
	}
	$_POST['Line'.$line]=$ln_itm->qty_dispatched; /// clear post so value displayed in the fiel is the 'new' quantity
	small_qty_cells(null, 'Line'.$line, qty_format($ln_itm->qty_dispatched, $ln_itm->stock_id, $dec), null, null, $dec);

	$display_discount_percent = percent_format($ln_itm->discount_percent*100) . "%";

	$line_total = ($ln_itm->qty_dispatched * $ln_itm->price * (1 - $ln_itm->discount_percent));

	amount_cell($ln_itm->price);
	label_cell($ln_itm->tax_type_name);
	label_cell($display_discount_percent, "nowrap align=right");
	amount_cell($line_total);

	end_row();
}

$_POST['ChargeFreightCost'] =  get_post('ChargeFreightCost', 
	price_format($_SESSION['Items']->freight_cost));

$colspan = 9;

start_row();
label_cell(_("Shipping Cost"), "colspan=$colspan align=right");
small_amount_cells(null, 'ChargeFreightCost', $_SESSION['Items']->freight_cost);
end_row();

$inv_items_total = $_SESSION['Items']->get_items_total_dispatch();

$display_sub_total = price_format($inv_items_total + input_num('ChargeFreightCost'));

label_row(_("Sub-total"), $display_sub_total, "colspan=$colspan align=right","align=right");

$taxes = $_SESSION['Items']->get_taxes(input_num('ChargeFreightCost'));
$tax_total = display_edit_tax_items($taxes, $colspan, $_SESSION['Items']->tax_included);

$display_total = price_format(($inv_items_total + input_num('ChargeFreightCost') + $tax_total));

label_row(_("Amount Total"), $display_total, "colspan=$colspan align=right","align=right");

end_table(1);

if ($has_marked) {
	display_note(_("Marked items have insufficient quantities in stock as on day of delivery."), 0, 1, "class='stockmankofg'");
}
start_table(TABLESTYLE2);

policy_list_row(_("Action For Balance"), "bo_policy", null);

textarea_row(_("Memo"), 'Comments', null, 50, 4);

end_table(1);
div_end();
submit_center_first('Update', _("Update"),
	_('Refresh document page'), true);
if(isset($_POST['clear_quantity'])) {
	submit('reset_quantity', _('Reset quantity'), true, _('Refresh document page'));
}
else  {
	submit('clear_quantity', _('Clear quantity'), true, _('Refresh document page'));
}
submit_center_last('process_delivery', _("Process Dispatch"),
	_('Check entered data and save document'), 'default');

end_form();


end_page();

