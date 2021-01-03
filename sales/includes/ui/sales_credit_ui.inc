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
include_once($path_to_root . "/includes/ui.inc");

// ------------------------------------------------------------------------------

function display_credit_header(&$order)
{
	global $Ajax, $Refs;

	start_outer_table(TABLESTYLE, "width='80%'");
	table_section(1);

	$customer_error = "";
	$change_prices = 0;

    if (!isset($_POST['customer_id']) && (get_global_customer() != ALL_TEXT))
    	$_POST['customer_id'] = get_global_customer();

	customer_list_row(_("Customer:"), 'customer_id', null, false, true, false, true);

	if ($order->customer_id != $_POST['customer_id'] /*|| $order->sales_type != $_POST['sales_type_id']*/)
	{
		// customer has changed
		$Ajax->activate('branch_id');
	}

	customer_branches_list_row(_("Branch:"), $_POST['customer_id'], 
	  	'branch_id', null, false, true, true, true);

	if (($order->customer_id != $_POST['customer_id']) ||
		($order->Branch != $_POST['branch_id']))
	{

		$old_order = (PHP_VERSION<5) ? $order : clone $order;
		$customer_error = get_customer_details_to_order($order, $_POST['customer_id'], $_POST['branch_id']);

		$_POST['Location'] = $order->Location;
		$_POST['deliver_to'] = $order->deliver_to;
		$_POST['delivery_address'] = $order->delivery_address;
		$_POST['phone'] = $order->phone;
		$Ajax->activate('Location');
		$Ajax->activate('deliver_to');
		$Ajax->activate('phone');
		$Ajax->activate('delivery_address');
		// change prices if necessary
		// what about discount in template case?
		if ($old_order->customer_currency != $order->customer_currency) {
			$change_prices = 1;
		}
		if ($old_order->sales_type != $order->sales_type) {
			$_POST['sales_type_id'] = $order->sales_type;
			$Ajax->activate('sales_type_id');
			$change_prices = 1;
		}
		if ($old_order->dimension_id != $order->dimension_id) {
			$_POST['dimension_id'] = $order->dimension_id;
			$Ajax->activate('dimension_id');
		}
		if ($old_order->dimension2_id != $order->dimension2_id) {
			$_POST['dimension2_id'] = $order->dimension2_id;
			$Ajax->activate('dimension2_id');
		}
		unset($old_order);
	}
	set_global_customer($_POST['customer_id']);

	if (!isset($_POST['ref']))
		$_POST['ref'] = $Refs->get_next(ST_CUSTCREDIT, null, array('customer' => get_post('customer_id'),
			'branch' => get_post('branch_id'), 'date' => get_post('OrderDate')));
	if ($order->trans_no==0)
	    ref_row(_("Reference").':', 'ref');
	else
	    label_row(_("Reference").':', $order->reference);


	if (!is_company_currency($order->customer_currency))
	{
		table_section(2);
		
		label_row(_("Customer Currency:"), $order->customer_currency);
		exchange_rate_display(get_company_currency(), $order->customer_currency,
			$_POST['OrderDate']);
	}
	table_section(3);

    if (!isset($_POST['sales_type_id']))
    	$_POST['sales_type_id'] = $order->sales_type;
    sales_types_list_row(_("Sales Type"), 'sales_type_id', $_POST['sales_type_id'], true);
	
	if ($order->sales_type != $_POST['sales_type_id']) {
		$myrow = get_sales_type($_POST['sales_type_id']);
		$order->set_sales_type($myrow['id'], $myrow['sales_type'],
		$myrow['tax_included'], $myrow['factor']);
		$Ajax->activate('sales_type_id');
		$change_prices = 1;
	}

    shippers_list_row(_("Shipping Company:"), 'ShipperID', $order->ship_via);

	label_row(_("Customer Discount:"), ($order->default_discount * 100) . "%");

	table_section(4);

	if (!isset($_POST['OrderDate']) || $_POST['OrderDate'] == "")
		$_POST['OrderDate'] = $order->document_date;

	date_row(_("Date:"), 'OrderDate', '', $order->trans_no==0, 0, 0, 0, null, true);
	if (isset($_POST['_OrderDate_changed'])) {
		if (!is_company_currency($order->customer_currency) 
			&& (get_base_sales_type()>0)) {
				$change_prices = 1;
		}
		$Ajax->activate('_ex_rate');
	}
	// added dimensions
	$dim = get_company_pref('use_dimension');
	if ($dim > 0)
		dimensions_list_row(_("Dimension").":", 'dimension_id', 
			null, true, ' ', false, 1, false);
	else
		hidden('dimension_id', 0);
	if ($dim > 1)
		dimensions_list_row(_("Dimension")." 2:", 'dimension2_id', 
			null, true, ' ', false, 2, false);
	else
		hidden('dimension2_id', 0);

	end_outer_table(1); // outer table

	if ($change_prices != 0) {
		foreach ($order->line_items as $line_no=>$item) {
			$line = &$order->line_items[$line_no];
			$line->price = get_price($line->stock_id, $order->customer_currency,
				$order->sales_type, $order->price_factor, get_post('OrderDate'));
		}
	    $Ajax->activate('items_table');
	}


	return $customer_error;
}

//---------------------------------------------------------------------------------

function display_credit_items($title, &$order)
{
    global $path_to_root;

    display_heading($title);
    div_start('items_table');
    start_table(TABLESTYLE, "width='90%'");
    $th = array(_("Item Code"), _("Item Description"), _("Quantity"), _("Unit"),
		_("Price"), _("Discount %"), _("Total"),'');

    if (count($order->line_items)) $th[]= '';

    table_header($th);

    $subtotal = 0;
    $k = 0;  //row colour counter

    $id = find_submit('Edit');

    foreach ($order->get_items() as $line_no=>$line)
    {
	$line_total =	round($line->qty_dispatched * $line->price * (1 - $line->discount_percent),
	   user_price_dec());

	if ( $id != $line_no)
	{
	    alt_table_row_color($k);

    	    label_cell("<a target='_blank' href='$path_to_root/inventory/inquiry/stock_status.php?stock_id=" . $line->stock_id . "'>$line->stock_id</a>");
    	    label_cell($line->item_description, "nowrap");
    	    qty_cell($line->qty_dispatched, false, get_qty_dec($line->stock_id));
    	    label_cell($line->units);
    	    amount_cell($line->price);

	    	percent_cell($line->discount_percent * 100);
    	    amount_cell($line_total);

    	    edit_button_cell("Edit$line_no", _('Edit'),
				_('Edit document line'));
    	    delete_button_cell("Delete$line_no", _('Delete'),
				_('Remove line from document'));

    	    end_row();
        }
        else
        {
	    credit_edit_item_controls($order, $k, $line_no);
	}

	$subtotal += $line_total;
    }

    if ($id==-1)
        credit_edit_item_controls($order, $k);

	$colspan = 6;
    $display_sub_total = price_format($subtotal);
    label_row(_("Sub-total"), $display_sub_total, "colspan=$colspan align=right", "align=right", 2);

    if (!isset($_POST['ChargeFreightCost']) OR ($_POST['ChargeFreightCost'] == ""))
		$_POST['ChargeFreightCost'] = 0;
	start_row();
	label_cell(_("Shipping"), "colspan=$colspan align=right");
	small_amount_cells(null, 'ChargeFreightCost', price_format(get_post('ChargeFreightCost',0)));
    label_cell('', 'colspan=2');
	end_row();

    $taxes = $order->get_taxes($_POST['ChargeFreightCost']);

    $tax_total = display_edit_tax_items($taxes, $colspan, $order->tax_included, 2);

    $display_total = price_format(($subtotal + $_POST['ChargeFreightCost'] + $tax_total));

    label_row(_("Credit Note Total"), $display_total, "colspan=$colspan align=right","class='amount'", 2);

    end_table();
    div_end();
}

//---------------------------------------------------------------------------------

function credit_edit_item_controls(&$order, $rowcounter, $line_no=-1)
{
	global $Ajax;
	alt_table_row_color($rowcounter);
	$id = find_submit('Edit');

	if ($line_no!=-1 && $line_no == $id)
	{
		$_POST['stock_id'] = $order->line_items[$id]->stock_id;
		$dec = get_qty_dec($_POST['stock_id']);
		$_POST['qty'] = qty_format($order->line_items[$id]->qty_dispatched, $_POST['stock_id'], $dec);
		$_POST['price'] = price_format($order->line_items[$id]->price);
		$_POST['Disc'] = percent_format(($order->line_items[$id]->discount_percent)*100);
		$units = $order->line_items[$id]->units;
		hidden('stock_id', $_POST['stock_id']);
		label_cell($_POST['stock_id']);
		label_cell($order->line_items[$id]->item_description, "nowrap");
	    $Ajax->activate('items_table');
	}
	else
	{
		stock_items_list_cells(null,'stock_id', null, false, true);
		if (list_updated('stock_id')) {
			    $Ajax->activate('price');
			    $Ajax->activate('qty');
			    $Ajax->activate('units');
			    $Ajax->activate('line_total');
		}
  		$item_info = get_item_edit_info($_POST['stock_id']);

		$dec = $item_info['decimals'];
   		$_POST['qty'] = number_format2(0, $dec);
		$units = $item_info["units"];
		$price = get_price($_POST['stock_id'],
			$order->customer_currency, $order->sales_type,
			$order->price_factor, get_post('OrderDate'));
		$_POST['price'] = price_format($price);
		
		// default to the customer's discount %
		$_POST['Disc'] = percent_format($order->default_discount * 100);
	}

	qty_cells(null, 'qty', $_POST['qty'], null, null, $dec);

	label_cell($units, '', 'units');
	amount_cells(null, 'price');
	small_amount_cells(null, 'Disc', percent_format($_POST['Disc']), null, null, user_percent_dec());

	amount_cell(input_num('qty') * input_num('price') * (1 - input_num('Disc')/100), false, '', 'line_total');

	if ($id!=-1)
	{
		button_cell('UpdateItem', _("Update"),
				_('Confirm changes'), ICON_UPDATE);
		button_cell('CancelItemChanges', _("Cancel"),
				_('Cancel changes'), ICON_CANCEL);
		hidden('line_no', $line_no);
		set_focus('qty');
	}
	else
	{
		submit_cells('AddItem', _("Add Item"), "colspan=2",
		    _('Add new item to document'), true);
	}

	end_row();
}


//---------------------------------------------------------------------------------

function credit_options_controls($credit)
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
			$_POST['Location'] = $credit->Location;
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


//---------------------------------------------------------------------------------

