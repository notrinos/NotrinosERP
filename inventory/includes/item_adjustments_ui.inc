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
include_once($path_to_root . "/includes/ui/items_cart.inc");

//--------------------------------------------------------------------------------

function add_to_order(&$order, $new_item, $new_item_qty, $standard_cost)
{
    if ($order->find_cart_item($new_item))
         display_error(_("For Part :") . $new_item . " " . "This item is already on this document. You can change the quantity on the existing line if necessary.");
	else
         $order->add_to_cart (count($order->line_items), $new_item, $new_item_qty, $standard_cost);
}

//--------------------------------------------------------------------------------

function display_order_header(&$order)
{
	global $Refs;

	start_outer_table(TABLESTYLE2, "width='70%'"); // outer table
	table_section(1);

	locations_list_row(_("Location:"), 'StockLocation', null, false, false, $order->fixed_asset);
    date_row(_("Date:"), 'AdjDate', '', true);

	table_section(2, "50%");

	ref_row(_("Reference:"), 'ref', '', $Refs->get_next(ST_INVADJUST, null, array('location'=>get_post('StockLocation'), 'date'=>get_post('AdjDate'))),
		 false, ST_INVADJUST);

	end_outer_table(1); // outer table
}

//---------------------------------------------------------------------------------

function display_adjustment_items($title, &$order)
{
	global $path_to_root;

	display_heading($title);
    div_start('items_table');
	start_table(TABLESTYLE, "width='90%'");

	if ($order->fixed_asset) {
		$th = array(_("Item Code"), _("Item Description"), _("Quantity"),
		_("Unit"), _("Unit Cost"), _("Initial Value"), "");
	} else {
		$th = array(_("Item Code"), _("Item Description"), _("QOH"), _("Quantity"),
		_("Unit"), _("Unit Cost"), _("Total"), "");
		if ( count($order->line_items)) $th[] = '';
	}

	table_header($th);
	$total = 0;
	$k = 0;  //row colour counter

	$low_stock = $order->check_qoh($_POST['StockLocation'], $_POST['AdjDate']);
	$id = get_post('selected_id', find_submit('Edit'));
	foreach ($order->line_items as $line_no=>$stock_item)
	{

		$total += ($stock_item->standard_cost * $stock_item->quantity);

		if ($id != $line_no)
		{
			if (in_array($stock_item->stock_id, $low_stock))
				start_row("class='stockmankobg'");	// notice low stock status
			else 
				alt_table_row_color($k);

    		if ($order->fixed_asset)
				label_cell($stock_item->stock_id);
      		else
				view_stock_status_cell($stock_item->stock_id);

			label_cell($stock_item->item_description);
			$dec = get_qty_dec($stock_item->stock_id);
    		if (!$order->fixed_asset)
			{
				$qoh = get_qoh_on_date($stock_item->stock_id, $_POST['StockLocation']);
				qty_cell($qoh, false, $dec);
			}
    		qty_cell($stock_item->quantity, false, $dec);
			label_cell($stock_item->units);
			amount_decimal_cell($stock_item->standard_cost);

			if ($order->fixed_asset) {
    	   		$price = get_purchase_value($stock_item->stock_id);
				amount_cell($price);
      		} else
				amount_cell($stock_item->standard_cost * $stock_item->quantity);

      		if (!$order->fixed_asset) {
      			if ($id == -1)
	        		edit_button_cell("Edit$line_no", _("Edit"),	_('Edit document line'));
	        	else
	        		label_cell('');
      		}
    		if ($id == -1)
				delete_button_cell("Delete$line_no", _("Delete"), _('Remove line from document'));
        	else
        		label_cell('');
			
			end_row();
		}
		else
		{
			adjustment_edit_item_controls($order, $line_no);
		}
	}

	if ($id == -1)
		adjustment_edit_item_controls($order);
	$row = !$order->fixed_asset ? 6 : 5;
	label_row(_("Total"), number_format2($total,user_price_dec()), "align=right colspan=$row", "align=right", 2);

    end_table();

	if ($id != -1)
	    hidden('selected_id', $id);
	if ($low_stock)
		display_note(_("Marked items have insufficient quantities in stock as on day of adjustment."), 0, 1, "class='stockmankofg'");
	div_end();
}

//---------------------------------------------------------------------------------

function adjustment_edit_item_controls(&$order, $line_no=-1)
{
	global $Ajax;
	start_row();

	$dec2 = 0;

	if ($line_no != -1)
	{
		if (find_submit('Edit') != -1) {
			$_POST['stock_id'] = $order->line_items[$line_no]->stock_id;
			$_POST['qty'] = qty_format($order->line_items[$line_no]->quantity, 
				$order->line_items[$line_no]->stock_id, $dec);
		//$_POST['std_cost'] = price_format($order->line_items[$line_no]->standard_cost);
			$_POST['std_cost'] = price_decimal_format($order->line_items[$line_no]->standard_cost, $dec2);
		}
		$_POST['units'] = $order->line_items[$line_no]->units;
		hidden('stock_id', $_POST['stock_id']);
		label_cell($_POST['stock_id']);
		label_cell($order->line_items[$line_no]->item_description, 'nowrap');
	    $Ajax->activate('items_table');
	}
	else
	{
		if ($order->fixed_asset)
			stock_disposable_fa_list_cells(null, 'stock_id', null, false, true, $order->line_items);
		else
			stock_costable_items_list_cells(null, 'stock_id', null, false, true);

		if (list_updated('stock_id')) {
			$Ajax->activate('items_table');
		}

	}

   	$item_info = get_item_edit_info(get_post('stock_id'));
   	if ($item_info) {
		$dec = $item_info['decimals'];
		$_POST['std_cost'] = price_decimal_format($item_info["material_cost"], $dec2);
		$_POST['units'] = $item_info["units"];
	} else
		$dec = 0;
	if ($order->fixed_asset) {
		hidden('qty', -1);
		qty_cell(1, false, 0);
	} else {
		$qoh = get_qoh_on_date(get_post('stock_id'), $_POST['StockLocation']);
		qty_cell($qoh, false, $dec);
		label_cell( text_input('qty', get_post('qty', '0'), 15, 15, '',  'class="amount active" dec='.$dec), "nowrap align=right");
	}
	label_cell(get_post('units'), '', 'units');

	if ($order->fixed_asset) {
		hidden('std_cost');
		label_cell($_POST['std_cost'], "nowrap align=right ", 'std_cost_view');
		$price = get_purchase_value($_POST['stock_id']);
		amount_cell($price, false, '', 'initial_price');
	} else {
		if (get_post('_qty_changed')) {
			$Ajax->activate('items_table');
			set_focus(input_num('qty') >= 0 ? 'std_cost' : 'AddItem');
		}
		if (input_num('qty') >= 0)
		    amount_cells(null, 'std_cost', null, null, null, $dec2);
		else {
		    hidden('std_cost');
			label_cell($_POST['std_cost'], "nowrap align=right");
		}
		label_cell("&nbsp;");
	}

	if ($line_no != -1)
	{
		button_cell('UpdateItem', _("Update"),
				_('Confirm changes'), ICON_UPDATE);
		button_cell('CancelItemChanges', _("Cancel"),
				_('Cancel changes'), ICON_CANCEL);
		hidden('LineNo', $line_no);
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

function adjustment_options_controls()
{
	  echo "<br>";
	  start_table();

	  textarea_row(_("Memo"), 'memo_', null, 50, 3);

	  end_table(1);
}


//---------------------------------------------------------------------------------

