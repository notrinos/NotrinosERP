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
include_once($path_to_root.'/purchasing/includes/purchasing_db.inc');

//--------------------------------------------------------------------------------------------------

function copy_from_cart() {
	$cart = &$_SESSION['PO'];

	$_POST['supplier_id'] = $cart->supplier_id;
	$_POST['OrderDate'] = $cart->orig_order_date;
	if ($cart->trans_type == ST_SUPPINVOICE)
		$_POST['due_date'] = $cart->due_date;
	$_POST['supp_ref'] = $cart->supp_ref;
	$_POST['ref'] = $cart->reference;
	$_POST['Comments'] = $cart->Comments;
	$_POST['StkLocation'] = $cart->Location;
	$_POST['delivery_address'] = $cart->delivery_address;
	$_POST['prep_amount'] = price_format($cart->prep_amount);
	$_POST['_ex_rate'] = $cart->ex_rate;
	$_POST['cash_account'] = $cart->cash_account;
	$_POST['dimension'] = $cart->dimension;
	$_POST['dimension2'] = $cart->dimension2;
	foreach($cart->tax_overrides as $id => $value)
		$_POST['mantax'][$id] = price_format($value);
}

function copy_to_cart() {
	$cart = &$_SESSION['PO'];

	$cart->supplier_id = $_POST['supplier_id'];	
	$cart->orig_order_date = $_POST['OrderDate'];
	if ($cart->trans_type == ST_SUPPINVOICE) {
		$cart->due_date = $_POST['due_date'];
		$cart->cash_account = get_post('cash_account');
	}
	$cart->reference = $_POST['ref'];
	$cart->supp_ref = $_POST['supp_ref'];
	$cart->Comments = $_POST['Comments'];	
	$cart->Location = $_POST['StkLocation'];
	$cart->delivery_address = $_POST['delivery_address'];
	$cart->dimension = @$_POST['dimension'];
	$cart->dimension2 =  @$_POST['dimension2'];
	$cart->prep_amount = input_num('prep_amount', 0);
	$cart->ex_rate = input_num('_ex_rate', null);

	if (isset($_POST['mantax'])) {
		foreach($_POST['mantax'] as $id => $tax) {
			$cart->tax_overrides[$id] = user_numeric($_POST['mantax'][$id]); }
	}
}

//---------------------------------------------------------------------------------------------------

function create_new_po($trans_type, $trans_no) {
	global $Refs;

	if (isset($_SESSION['PO']))
		unset ($_SESSION['PO']->line_items, $_SESSION['PO']);

	$cart = new purch_order;
	$_POST['OrderDate'] = new_doc_date();
	if (!is_date_in_fiscalyear($_POST['OrderDate']))
		$_POST['OrderDate'] = end_fiscalyear();
	$cart->due_date = $cart->orig_order_date = $_POST['OrderDate'];

	$cart->trans_type = $trans_type;
	$cart->order_no = $trans_no;
	/*read in all the selected order into the Items cart  */
	if ($trans_no) {
		read_po($trans_no, $cart);
		$cart->order_no = $trans_no;
	}
	else
		$cart->reference = $Refs->get_next($trans_type, null, array('supplier_id' => $cart->supplier_id, 'date' => get_post('OrderDate')));
	$_SESSION['PO'] = &$cart;
}

//---------------------------------------------------------------------------------------------------

function display_po_header(&$order) {
	global $Ajax, $Refs;

	$editable = ($order->order_no == 0);

	start_outer_table(TABLESTYLE2, "width='80%'");

	table_section(1);
	if ($editable) {
		if (!isset($_POST['supplier_id']) && (get_global_supplier() != ALL_TEXT))
			$_POST['supplier_id'] = get_global_supplier();

		supplier_list_row(_('Supplier:'), 'supplier_id', null, false, true, false, true);
	}
	else {
		hidden('supplier_id', $order->supplier_id);
		label_row(_('Supplier:'), $order->supplier_name);
	}

	if ($order->supplier_id != get_post('supplier_id', -1)) {
		$old_supp = $order->supplier_id;
		get_supplier_details_to_order($order, $_POST['supplier_id']); 
		get_duedate_from_terms($order);
		$_POST['due_date'] = $order->due_date;

		// supplier default price update
		foreach ($order->line_items as $line_no=>$item) {
			$line = &$order->line_items[$line_no];
			$line->price =  get_purchase_price ($order->supplier_id, $line->stock_id);
			$line->quantity = $line->quantity/get_purchase_conversion_factor($old_supp, $line->stock_id) * get_purchase_conversion_factor($order->supplier_id, $line->stock_id);
		}
		$Ajax->activate('items_table');
		$Ajax->activate('due_date');
	}
	set_global_supplier($_POST['supplier_id']);

	date_row($order->trans_type==ST_PURCHORDER ? _('Order Date:') :
		($order->trans_type==ST_SUPPRECEIVE ? _('Delivery Date:') : _('Invoice Date:')),
		'OrderDate', '', true, 0, 0, 0, null, true);

	if (isset($_POST['_OrderDate_changed'])) {
		$order->orig_order_date = $_POST['OrderDate'];
		get_duedate_from_terms($order);
		$_POST['due_date'] = $order->due_date;
		$Ajax->activate('due_date');
	}
	supplier_credit_row($order->supplier_id, $order->credit);

	if (!is_company_currency($order->curr_code)) {
		label_row(_('Supplier Currency:'), $order->curr_code);
		exchange_rate_display(get_company_currency(), $order->curr_code, $_POST['OrderDate']);
	}

	if ($editable)
		ref_row(_('Reference:'), 'ref', '', null, false, $order->trans_type, array('date'=> @$_POST['OrderDate']));
	else {
		hidden('ref', $order->reference);
		label_row(_('Reference:'), $order->reference);
	}

	table_section(2);

	if ($order->trans_type==ST_SUPPINVOICE)
		date_row(_('Due Date:'), 'due_date', '', false, 0, 0, 0, null, true);

	text_row(_("Supplier's Reference:"), 'supp_ref', null, 20, 60);

	if (get_company_pref('use_dimension'))
		dimensions_list_row(_('Dimension').':', 'dimension', null, true, _('Default'), false, 1);
	if (get_company_pref('use_dimension') == 2)
		dimensions_list_row(_('Dimension 2').':', 'dimension2', null, true, _('Default'), false, 2);
	locations_list_row(_('Receive Into:'), 'StkLocation', null, false, true, $order->fixed_asset); 

	table_section(3);

	if (!isset($_POST['StkLocation']) || $_POST['StkLocation'] == '' || isset($_POST['_StkLocation_update']) || !isset($_POST['delivery_address']) || $_POST['delivery_address'] == '') {
		// If this is the first time the form loaded set up defaults

		$loc_row = get_item_location(get_post('StkLocation'));
		if ($loc_row) {
			$_POST['delivery_address'] = $loc_row['delivery_address'];
			$Ajax->activate('delivery_address');
			$_SESSION['PO']->Location = $_POST['StkLocation'];
			$_SESSION['PO']->delivery_address = $_POST['delivery_address'];
		}
		else //The default location of the user is crook
			display_error(_('The default stock location set up for this user is not a currently defined stock location. Your system administrator needs to amend your user record.'));
	}

	textarea_row(_('Deliver to:'), 'delivery_address', $_POST['delivery_address'], 35, 4);

	end_outer_table();
}

//---------------------------------------------------------------------------------------------------

function display_po_items(&$order, $editable=true) {
	display_heading(_('Order Items'));

	div_start('items_table');
	start_table(TABLESTYLE, "width='80%'");

	$new = $order->order_no == 0;

	$th = array(_('Item Code'), _('Item Description'), _('Quantity'), _('Received'),
		_('Unit'), _('Required Delivery Date'), $order->tax_included ? _('Price after Tax') : _('Price before Tax'), _('Line Total'), '');

	if ($order->trans_type != ST_PURCHORDER)
		array_remove($th, 5);
	if ($new)
		array_remove($th, 3);
	if (count($order->line_items))
		$th[] = '';

	table_header($th);

	$id = find_submit('Edit');
	$total = 0;
	$k = 0;

	if ($id == -1 && $editable)
		po_item_controls($order, $k);
	else {
		start_row();
		label_cell('&nbsp;', "colspan=".count($th));
		end_row();
	}

	foreach ($order->line_items as $line_no => $po_line) {
		$line_total =	round($po_line->quantity * $po_line->price,  user_price_dec());
		if (!$editable || ($id != $line_no)) {
			alt_table_row_color($k);
			label_cell($po_line->stock_id);
			label_cell($po_line->item_description);
			if ($order->fixed_asset)
				label_cell(1);
			else
				qty_cell($po_line->quantity, false, get_qty_dec($po_line->stock_id));
			if (!$new)
				qty_cell($po_line->qty_received, false, get_qty_dec($po_line->stock_id));
			label_cell($po_line->units);
			if ($order->trans_type == ST_PURCHORDER)
				label_cell($po_line->req_del_date);
			amount_decimal_cell($po_line->price);
			amount_cell($line_total);

			if ($editable) {
				edit_button_cell('Edit'.$line_no, _('Edit'), _('Edit document line'));
				delete_button_cell('Delete'.$line_no, _('Delete'), _('Remove line from document'));
			}
			end_row();
		}
		else
			po_item_controls($order, $k, $line_no);
		
		$total += $line_total;
	}

	$colspan = count($th) - 2;
	if (count($order->line_items))
		$colspan--;

	$display_sub_total = price_format($total);

	label_row(_('Sub-total'), $display_sub_total, 'colspan='.$colspan.' align=right','align=right', 2);

	$taxes = $order->get_taxes(input_num('freight_cost'));
	
	$tax_total = display_edit_tax_items($taxes, $colspan, $order->tax_included, 2, $order->trans_type==ST_SUPPINVOICE);

	$display_total = price_format(($total + input_num('freight_cost') + $tax_total));

	start_row();
	label_cells(_('Amount Total'), $display_total, "colspan=$colspan align='right'","align='right'");
	$order->trans_type == ST_SUPPINVOICE ? submit_cells('update', _("Update"), "colspan=2 align='center'", _('Refresh'), true) : label_cell('', 'colspan=2');
	end_row();

	end_table(1);
	div_end();
}

//---------------------------------------------------------------------------------------------------

function display_po_summary(&$po, $is_self=false, $editable=false) {
	start_table(TABLESTYLE, "width='90%'");

	start_row();
	label_cells(_('Reference'), $po->reference, "class='tableheader2'");

	label_cells(_('Supplier'), $po->supplier_name, "class='tableheader2'");

	if (!is_company_currency($po->curr_code))
		label_cells(_('Order Currency'), $po->curr_code, "class='tableheader2'");

	if (!$is_self)
		label_cells(_('Purchase Order'), get_trans_view_str(ST_PURCHORDER, $po->order_no), "class='tableheader2'");
	
	end_row();
	start_row();
	label_cells(_('Date'), $po->orig_order_date, "class='tableheader2'");

	if ($editable) {
		if (!isset($_POST['Location']))
			$_POST['Location'] = $po->Location;
		label_cell(_('Deliver Into Location'), "class='tableheader2'");
		locations_list_cells(null, 'Location', $_POST['Location']);
	}
	else
		label_cells(_('Deliver Into Location'), get_location_name($po->Location), "class='tableheader2'");

	if ($po->supp_ref != "")
		label_cells(_("Supplier's Reference"), $po->supp_ref, "class='tableheader2'");
	end_row();

	if (!$editable)
		label_row(_('Delivery Address'), $po->delivery_address, "class='tableheader2'", "colspan=9");

	if (!$editable && ($po->prep_amount > 0)) {
		start_row();
		label_cells(_('Required Pre-Payment'), price_format($po->prep_amount), "class='tableheader2'");
		label_cells(_('Pre-Payments Allocated'), price_format($po->alloc), "class='tableheader2'");
		end_row();
	}

	if ($po->Comments != '')
		label_row(_('Order Comments'), $po->Comments, "class='tableheader2'", 'colspan=9');
	end_table(1);
}

//--------------------------------------------------------------------------------

function po_item_controls(&$order, &$rowcounter, $line_no=-1) {
	global $Ajax, $SysPrefs;

	alt_table_row_color($rowcounter);
	$new = $order->order_no == 0;

	$dec2 = 6;
	$id = find_submit('Edit');

	if (($id != -1) && $line_no == $id) {

		$_POST['stock_id'] = $order->line_items[$id]->stock_id;
		$dec = get_qty_dec($_POST['stock_id']);
		$_POST['qty'] = qty_format($order->line_items[$id]->quantity, $_POST['stock_id'], $dec);
		$_POST['price'] = price_decimal_format($order->line_items[$id]->price, $dec2);
		if ($order->trans_type == ST_PURCHORDER)
			$_POST['req_del_date'] = $order->line_items[$id]->req_del_date;

		$_POST['units'] = $order->line_items[$id]->units;
		$_POST['item_description'] = $order->line_items[$id]->item_description;

		hidden('stock_id', $_POST['stock_id']);
		label_cell($_POST['stock_id']);

		if ($order->line_items[$id]->descr_editable)
			text_cells(null,'item_description', null, 45, 150);
		else {
			hidden('item_description', $_POST['item_description']);
			label_cell($order->line_items[$id]->item_description); 
		}

		$Ajax->activate('items_table');
		$qty_rcvd = $order->line_items[$id]->qty_received;
	}
	else {

		// Manufactured item can be purchased
		if ($order->fixed_asset)
			stock_purchasable_fa_list_cells(null, 'stock_id', null, false, true, false, true, $order->line_items);
		else
			stock_items_list_cells(null, 'stock_id', null, false, true, false, true, array('editable' => 30, 'where'=>array('NOT no_purchase')));

		if (list_updated('stock_id')) {
			$Ajax->activate('price');
			$Ajax->activate('units');
			$Ajax->activate('qty');
			$Ajax->activate('req_del_date');
			$Ajax->activate('line_total');
		}

		$dec = 0;
		$_POST['units'] = '';
		$item_info = get_item_edit_info($_POST['stock_id']);
		if ($item_info) {
			$_POST['units'] = $item_info['units'];
			$dec = $item_info['decimals'];
		}
		$_POST['qty'] =	number_format2(get_purchase_conversion_factor ($order->supplier_id, $_POST['stock_id']), $dec);
		$_POST['price'] = price_decimal_format(get_purchase_price ($order->supplier_id, $_POST['stock_id']), $dec2);
		if ($order->trans_type == ST_PURCHORDER)
			$_POST['req_del_date'] = add_days(Today(), $SysPrefs->default_receival_required_by());
		$qty_rcvd = '';
	}

	if ($order->fixed_asset) {
		label_cell(1, '', 'qty');
		hidden('qty', 1);
	}
	else
		qty_cells(null, 'qty', null, null, null, $dec);

	if (!$new)
		qty_cell($qty_rcvd, false, $dec);

	label_cell($_POST['units'], '', 'units');
	if ($order->trans_type == ST_PURCHORDER)
		date_cells(null, 'req_del_date', '', null, 0, 0, 0);
	if ($qty_rcvd > 0) {
		amount_decimal_cell($_POST['price']);
		hidden('price', $_POST['price']);
	}	
	else	
		amount_cells(null, 'price', null, null, null, $dec2);

	$line_total = round(input_num('qty') * input_num('price'),  user_price_dec());
	amount_cell($line_total, false, '','line_total');

	if ($id!=-1) {
		button_cell('UpdateLine', _('Update'), _('Confirm changes'), ICON_UPDATE);
		button_cell('CancelUpdate', _('Cancel'), _('Cancel changes'), ICON_CANCEL);
		hidden('line_no', $line_no);
		set_focus('qty');
	}
	else
		submit_cells('EnterLine', _('Add Item'), "colspan=2 align='center'", _('Add new item to document'), true);

	end_row();
}

//---------------------------------------------------------------------------------------------------

function display_po_footer(&$trans) {
	global $Ajax;

	start_table(TABLESTYLE2);

	if ($trans->trans_type == ST_SUPPINVOICE)
		cash_accounts_list_row(_('Payment:'), 'cash_account', null, false, _('Delayed'));
	elseif ($trans->trans_type == ST_PURCHORDER)
		amount_row(_('Pre-Payment Required:'), 'prep_amount');

	textarea_row(_('Memo:'), 'Comments', null, 70, 4);

	end_table();
}
