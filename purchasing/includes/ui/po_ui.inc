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
include_once($path_to_root.'/purchasing/includes/purchasing_db.inc');

$serial_batch_db_path = $path_to_root . '/inventory/includes/db/serial_batch_db.inc';
if (file_exists($serial_batch_db_path))
	include_once($serial_batch_db_path);

//--------------------------------------------------------------------------------------------------

function copy_from_cart() {
	$cart = &$_SESSION['PO'];

	$_POST['supplier_id'] = $cart->supplier_id;
	$_POST['purch_template_id'] = isset($cart->purch_template_id) ? (int)$cart->purch_template_id : 0;
	$_POST['agreement_id'] = isset($cart->agreement_id) ? (int)$cart->agreement_id : 0;
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
	$_POST['requisition_id'] = isset($cart->requisition_id) ? (int)$cart->requisition_id : 0;
	$_POST['rfq_id'] = isset($cart->rfq_id) ? (int)$cart->rfq_id : 0;
	foreach($cart->tax_overrides as $id => $value)
		$_POST['mantax'][$id] = price_format($value);
}

function copy_to_cart() {
	$cart = &$_SESSION['PO'];

	$cart->supplier_id = $_POST['supplier_id'];	
	$cart->purch_template_id = isset($_POST['purch_template_id']) ? (int)$_POST['purch_template_id'] : (isset($cart->purch_template_id) ? (int)$cart->purch_template_id : 0);
	$cart->agreement_id = isset($_POST['agreement_id']) ? (int)$_POST['agreement_id'] : (isset($cart->agreement_id) ? (int)$cart->agreement_id : 0);
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
	$cart->requisition_id = isset($_POST['requisition_id']) ? (int)$_POST['requisition_id'] : (isset($cart->requisition_id) ? (int)$cart->requisition_id : 0);
	$cart->rfq_id = isset($_POST['rfq_id']) ? (int)$_POST['rfq_id'] : (isset($cart->rfq_id) ? (int)$cart->rfq_id : 0);
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

/**
 * Build the vendor comparison tooltip text for one item row.
 *
 * @param string $stock_id
 * @param int    $current_supplier_id
 * @param float  $quantity
 * @param string $currency
 * @param string $date
 * @param int    $agreement_id
 * @return string
 */
function get_vendor_price_comparison_text($stock_id, $current_supplier_id, $quantity, $currency, $date, $agreement_id = 0)
{
	if (!function_exists('get_item_vendors') || $stock_id === '')
		return '';

	$vendors = get_item_vendors($stock_id, true, $quantity, $currency, $date);
	if (!count($vendors))
		return '';

	$parts = array();
	foreach ($vendors as $vendor) {
		$label = $vendor['supplier_name'] . ': ' . price_format($vendor['price']) . ' [' . get_purchase_price_source_label($vendor['source']) . ']';
		if (!empty($vendor['lead_time_days']))
			$label .= ' ' . sprintf(_('(%s days)'), (int)$vendor['lead_time_days']);
		if ((int)$vendor['supplier_id'] === (int)$current_supplier_id)
			$label = '* ' . $label;
		$parts[] = $label;
	}

	if ((int)$current_supplier_id > 0 && (int)$agreement_id > 0 && function_exists('get_supplier_pricing_context')) {
		$current_context = get_supplier_pricing_context($current_supplier_id, $stock_id, $quantity, $currency, $date, $agreement_id, true);
		if ($current_context['source'] === 'agreement' && $current_context['price'] > 0)
			array_unshift($parts, '* ' . get_supplier_name($current_supplier_id) . ': ' . price_format($current_context['price']) . ' [' . get_purchase_price_source_label('agreement') . ']');
	}

	return implode(' | ', array_slice($parts, 0, 4));
}

/**
 * Render a helper row with vendor price comparison information.
 *
 * @param purch_order $order
 * @param string      $stock_id
 * @param float       $quantity
 * @param int         $column_count
 * @return void
 */
function display_vendor_price_comparison_hint($order, $stock_id, $quantity, $column_count)
{
	$comparison_text = get_vendor_price_comparison_text(
		$stock_id,
		(int)$order->supplier_id,
		(float)$quantity,
		$order->curr_code,
		date2sql(get_post('OrderDate', Today())),
		isset($order->agreement_id) ? (int)$order->agreement_id : 0
	);
	if ($comparison_text === '')
		return;

	echo '<tr class="vendor_price_hint">';
	echo '<td></td>';
	echo '<td colspan="' . max(1, (int)$column_count - 1) . '" style="padding:4px 8px;background:#fff8e7;border-left:3px solid #f59e0b;font-size:12px;color:#7c5d12;">';
	echo '<strong>' . _('Vendor Price Comparison:') . '</strong> ' . htmlspecialchars($comparison_text, ENT_QUOTES, 'UTF-8');
	echo '</td>';
	echo '</tr>';
}

//---------------------------------------------------------------------------------------------------

function display_po_header(&$order) {
	global $Ajax, $Refs;

	$editable = ($order->order_no == 0);

	if ($order->trans_type == ST_PURCHORDER && $editable && list_updated('purch_template_id')) {
		$order->purch_template_id = (int)get_post('purch_template_id');
		if ($order->purch_template_id > 0 && function_exists('apply_template_to_po')) {
			apply_template_to_po($order, $order->purch_template_id);
			$_POST['supplier_id'] = $order->supplier_id;
			$_POST['StkLocation'] = $order->Location;
			$_POST['delivery_address'] = $order->delivery_address;
			$_POST['Comments'] = $order->Comments;
			$Ajax->activate('items_table');
			$Ajax->activate('delivery_address');
		}
	}

	start_outer_table(TABLESTYLE2, "data-order-header='1'");

	table_section(1);
	if ($editable) {
		if (!isset($_POST['supplier_id']) && (get_global_supplier() != ALL_TEXT))
			$_POST['supplier_id'] = get_global_supplier();

		supplier_list_row(_('Supplier:'), 'supplier_id', null, false, true, false, true);

		if ($order->trans_type == ST_PURCHORDER) {
				$selected_template_id = (int)get_post('purch_template_id', isset($order->purch_template_id) ? $order->purch_template_id : 0);
				$template_options = array('' => _('Manual Entry / No Template')) + get_purch_template_options((int)get_post('supplier_id'));
				echo "<tr><td class='label'>" . _('Use Template:') . "</td><td>";
				echo array_selector('purch_template_id', $selected_template_id, $template_options, array(
					'select_submit' => true,
					'class' => array('nosearch')
				));
				echo "</td></tr>\n";

			$selected_supplier_id = (int)get_post('supplier_id');
			$selected_agreement_id = (int)get_post('agreement_id', isset($order->agreement_id) ? $order->agreement_id : 0);
			$agreement_options = array('' => _('Manual Pricing / No Agreement'));
			if ($selected_supplier_id > 0)
				$agreement_options += get_active_purch_agreement_options($selected_supplier_id);

			echo "<tr><td class='label'>" . _('From Agreement:') . "</td><td>";
			echo array_selector('agreement_id', $selected_agreement_id, $agreement_options, array(
				'select_submit' => true,
				'class' => array('nosearch')
			));
			echo "</td></tr>\n";
		}
	}
	else {
		hidden('supplier_id', $order->supplier_id);
		label_row(_('Supplier:'), $order->supplier_name);
		if ($order->trans_type == ST_PURCHORDER) {
			hidden('purch_template_id', isset($order->purch_template_id) ? (int)$order->purch_template_id : 0);
			$template_name = '-';
			if (!empty($order->purch_template_id)) {
				$template = get_purch_template((int)$order->purch_template_id);
				if ($template)
					$template_name = $template['name'];
			}
			label_row(_('Use Template:'), $template_name);

			hidden('agreement_id', isset($order->agreement_id) ? (int)$order->agreement_id : 0);
			$agreement_reference = '-';
			if (!empty($order->agreement_id)) {
				$agreement = get_purch_agreement((int)$order->agreement_id);
				if ($agreement)
					$agreement_reference = $agreement['reference'];
			}
			label_row(_('From Agreement:'), $agreement_reference);
		}
	}

	if ($order->supplier_id != get_post('supplier_id', -1)) {
		$old_supp = $order->supplier_id;
		get_supplier_details_to_order($order, $_POST['supplier_id']); 
		get_duedate_from_terms($order);
		$_POST['due_date'] = $order->due_date;

		if ((int)get_post('purch_template_id') > 0) {
			$template = get_purch_template((int)get_post('purch_template_id'));
			if ($template && (int)$template['supplier_id'] > 0 && (int)$template['supplier_id'] !== (int)$_POST['supplier_id']) {
				$_POST['purch_template_id'] = 0;
				$order->purch_template_id = 0;
			}
		}

		if ((int)get_post('agreement_id') > 0) {
			$agreement = get_purch_agreement((int)get_post('agreement_id'));
			if (!$agreement || (int)$agreement['supplier_id'] !== (int)$_POST['supplier_id']) {
				$_POST['agreement_id'] = 0;
				$order->agreement_id = 0;
			}
		}

		// supplier default price update
		foreach ($order->line_items as $line_no=>$item) {
			$line = &$order->line_items[$line_no];
			$line->price = get_purchase_price(
				$order->supplier_id,
				$line->stock_id,
				$line->quantity,
				date2sql(get_post('OrderDate', Today())),
				(int)get_post('agreement_id')
			);
			$line->quantity = $line->quantity/get_purchase_conversion_factor($old_supp, $line->stock_id) * get_purchase_conversion_factor($order->supplier_id, $line->stock_id);
		}
		$Ajax->activate('items_table');
		$Ajax->activate('due_date');
	}

	if ($order->trans_type == ST_PURCHORDER && list_updated('agreement_id')) {
		$order->agreement_id = (int)get_post('agreement_id');
		if ($order->agreement_id > 0) {
			$agreement = get_purch_agreement($order->agreement_id);
			if ($agreement && $agreement['status'] === 'active') {
				if ($agreement['delivery_location'] !== '') {
					$order->Location = $agreement['delivery_location'];
					$_POST['StkLocation'] = $agreement['delivery_location'];
				}
				$order->delivery_address = $agreement['delivery_location_name'] ? $agreement['delivery_location_name'] : $agreement['delivery_location'];
				$_POST['delivery_address'] = $order->delivery_address;
				$order->dimension = (int)$agreement['dimension_id'];
				$order->dimension2 = (int)$agreement['dimension2_id'];
				$_POST['dimension'] = $order->dimension;
				$_POST['dimension2'] = $order->dimension2;
			}
		} else {
			$order->agreement_id = 0;
		}

		foreach ($order->line_items as $line_no => $item) {
			$line = &$order->line_items[$line_no];
			$line->price = get_purchase_price(
				$order->supplier_id,
				$line->stock_id,
				$line->quantity,
				date2sql(get_post('OrderDate', Today())),
				$order->agreement_id
			);
		}

		$Ajax->activate('items_table');
		$Ajax->activate('delivery_address');
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

	table_section(2);

	if ($editable)
		ref_row(_('Reference:'), 'ref', '', null, false, $order->trans_type, array('date'=> @$_POST['OrderDate']));
	else {
		hidden('ref', $order->reference);
		label_row(_('Reference:'), $order->reference);
	}

	if ($order->trans_type==ST_SUPPINVOICE)
		date_row(_('Due Date:'), 'due_date', '', false, 0, 0, 0, null, true);

	text_row(_("Supplier's Reference:"), 'supp_ref', null, 20, 60);

	if (get_company_pref('use_dimension'))
		dimensions_list_row(_('Dimension').':', 'dimension', null, true, _('Default'), false, 1);
	if (get_company_pref('use_dimension') == 2)
		dimensions_list_row(_('Dimension 2').':', 'dimension2', null, true, _('Default'), false, 2);
	locations_list_row(_('Receive Into:'), 'StkLocation', null, false, true, $order->fixed_asset); 

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
			$comparison_text = get_vendor_price_comparison_text(
				$po_line->stock_id,
				(int)$order->supplier_id,
				(float)$po_line->quantity,
				$order->curr_code,
				date2sql(get_post('OrderDate', Today())),
				isset($order->agreement_id) ? (int)$order->agreement_id : 0
			);
			$description_attributes = $comparison_text !== ''
				? 'title="' . htmlspecialchars($comparison_text, ENT_QUOTES, 'UTF-8') . '"'
				: '';
			label_cell($po_line->stock_id);
			label_cell($po_line->item_description, $description_attributes);
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

		if ($order->trans_type == ST_SUPPRECEIVE)
			display_direct_grn_tracking_inputs($line_no, $po_line, count($th));
		
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

/**
 * Render serial/batch tracking input panels for direct GRN rows.
 *
 * This supports direct GRN shortcut flow in po_entry_items.php and posts keys
 * consumed by process_grn_tracking_and_wms():
 * - serial_{line_no}
 * - batch_no_{line_no}
 * - batch_expiry_{line_no}
 * - batch_mfg_{line_no}
 *
 * @param int   $line_no             GRN line number
 * @param mixed $purchase_order_line Purchase order line object
 * @param int   $column_span         Number of table columns to span
 *
 * @return void
 */
function display_direct_grn_tracking_inputs($line_no, $purchase_order_line, $column_span) {
	if (!function_exists('item_has_serial_tracking') || !function_exists('item_has_batch_tracking'))
		return;

	$has_serial_tracking = item_has_serial_tracking($purchase_order_line->stock_id);
	$has_batch_tracking = item_has_batch_tracking($purchase_order_line->stock_id);

	if (!$has_serial_tracking && !$has_batch_tracking)
		return;

	echo '<tr class="tracking_panel">';
	echo '<td></td>';
	echo '<td colspan="' . max(1, (int)$column_span - 1) . '" style="padding:4px 8px; background:#f0f9eb; border-left:3px solid #67c23a;">';

	if ($has_serial_tracking) {
		$serial_input_name = 'serial_' . $line_no;
		$serial_input_value = get_post($serial_input_name);
		echo '<div style="margin-bottom:4px;">';
		echo '<label style="font-weight:bold; color: #67c23a;"><i class="fa fa-barcode"></i> ';
		echo _('Serial Numbers') . ' <small style="color: #888;">(' . _('one per line or comma-separated') . ')</small></label><br>';
		echo '<textarea name="' . $serial_input_name . '" id="' . $serial_input_name . '" rows="3" cols="50" style="width:400px; font-family:monospace; font-size:12px;">';
		echo htmlspecialchars($serial_input_value, ENT_QUOTES);
		echo '</textarea>';
		echo '</div>';
	}

	if ($has_batch_tracking) {
		$batch_number_input_name = 'batch_no_' . $line_no;
		$batch_number_value = get_post($batch_number_input_name);
		$batch_expiry_input_name = 'batch_expiry_' . $line_no;
		$batch_expiry_value = get_post($batch_expiry_input_name);
		$batch_mfg_input_name = 'batch_mfg_' . $line_no;
		$batch_mfg_value = get_post($batch_mfg_input_name);

		echo '<div style="margin-bottom:4px;">';
		echo '<label style="font-weight:bold; color: #67c23a;"><i class="fa fa-cubes"></i> ';
		echo _('Batch/Lot Info') . '</label><br>';
		echo '<table style="margin:2px 0;">';
		echo '<tr><td>' . _('Batch No') . ':</td>';
		echo '<td><input type="text" name="' . $batch_number_input_name . '" id="' . $batch_number_input_name . '" value="' . htmlspecialchars($batch_number_value, ENT_QUOTES) . '" size="20" maxlength="60" style="font-family:monospace;"></td>';
		echo '<td style="padding-left:10px;">' . _('Expiry') . ':</td>';
		echo '<td><input type="text" name="' . $batch_expiry_input_name . '" id="' . $batch_expiry_input_name . '" value="' . htmlspecialchars($batch_expiry_value, ENT_QUOTES) . '" size="12" class="date_entry" placeholder="' . _('dd/mm/yyyy') . '"></td>';
		echo '<td style="padding-left:10px;">' . _('Mfg Date') . ':</td>';
		echo '<td><input type="text" name="' . $batch_mfg_input_name . '" id="' . $batch_mfg_input_name . '" value="' . htmlspecialchars($batch_mfg_value, ENT_QUOTES) . '" size="12" class="date_entry" placeholder="' . _('dd/mm/yyyy') . '"></td>';
		echo '</tr></table>';
		echo '</div>';
	}

	echo '</td></tr>';
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
		if ($order->fixed_asset) {
			// Render direct table cells to keep line controls aligned with header columns.
			$where_opts = array();
			$where_opts[] = "stock_id NOT IN
				( SELECT stock_id FROM ".TB_PREF."stock_moves WHERE type=".ST_SUPPRECEIVE." AND qty!=0 )";
			foreach($order->line_items as $item) {
				$where_opts[] = "stock_id != ".db_escape($item->stock_id);
			}
			$where_opts[] = "mb_flag='F'";
			echo stock_items_list('stock_id', null, false, true,
				array('fixed_asset' => true, 'cells' => true, 'where' => $where_opts), true);
		}
		else
			echo stock_items_list('stock_id', null, false, true,
				array('cells' => true, 'editable' => 30, 'where'=>array('NOT no_purchase')), true);

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
		$_POST['qty'] = number_format2(get_purchase_conversion_factor($order->supplier_id, $_POST['stock_id']), $dec);
		$_POST['price'] = price_decimal_format(get_purchase_price(
			$order->supplier_id,
			$_POST['stock_id'],
			input_num('qty'),
			date2sql(get_post('OrderDate', Today())),
			isset($order->agreement_id) ? (int)$order->agreement_id : 0
		), $dec2);
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

	$column_count = 9;
	if ($order->trans_type != ST_PURCHORDER)
		$column_count--;
	if ($new)
		$column_count--;
	if (count($order->line_items))
		$column_count++;

	if (get_post('stock_id'))
		display_vendor_price_comparison_hint($order, get_post('stock_id'), input_num('qty', 0), $column_count);
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
