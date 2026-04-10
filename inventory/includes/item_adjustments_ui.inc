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
include_once($path_to_root.'/includes/ui.inc');
include_once($path_to_root.'/includes/ui/items_cart.inc');
include_once($path_to_root.'/inventory/includes/db/serial_batch_db.inc');

//--------------------------------------------------------------------------------

function add_to_order(&$order, $new_item, $new_item_qty, $standard_cost) {
	if ($order->find_cart_item($new_item))
		display_error(_('For Part :').$new_item.' '.'This item is already on this document. You can change the quantity on the existing line if necessary.');
	else
		$order->add_to_cart(count($order->line_items), $new_item, $new_item_qty, $standard_cost);
}

function display_order_header(&$order) {
	global $Refs;

	start_outer_table(TABLESTYLE2, "data-order-header='1'");
	table_section(1);

	locations_list_row(_('Location:'), 'StockLocation', null, false, false, $order->fixed_asset);
	date_row(_('Date:'), 'AdjDate', '', true);

	table_section(2);

	ref_row(_('Reference:'), 'ref', '', $Refs->get_next(ST_INVADJUST, null, array('location'=>get_post('StockLocation'), 'date'=>get_post('AdjDate'))), false, ST_INVADJUST);

	end_outer_table(1);
}

//---------------------------------------------------------------------------------

function display_adjustment_items($title, &$order) {
	global $path_to_root;

	display_heading($title);
	div_start('items_table');
	start_table(TABLESTYLE, "width='90%'");

	if ($order->fixed_asset)
		$th = array(_('Item Code'), _('Item Description'), _('Quantity'), _('Unit'), _('Unit Cost'), _('Initial Value'), '');
	else {
		$th = array(_('Item Code'), _('Item Description'), _('QOH'), _('Quantity'), _('Unit'), _('Unit Cost'), _('Total'), '');
		if ( count($order->line_items))
			$th[] = '';
	}

	table_header($th);
	$total = 0;
	$k = 0;  //row colour counter

	$low_stock = $order->check_qoh($_POST['StockLocation'], $_POST['AdjDate']);
	$id = get_post('selected_id', find_submit('Edit'));

	foreach ($order->line_items as $line_no=>$stock_item) {
		$total += ($stock_item->standard_cost * $stock_item->quantity);

		if ($id != $line_no) {
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
			if (!$order->fixed_asset) {
				$qoh = get_qoh_on_date($stock_item->stock_id, $_POST['StockLocation']);
				qty_cell($qoh, false, $dec);
			}
			qty_cell($stock_item->quantity, false, $dec);
			label_cell($stock_item->units);
			amount_decimal_cell($stock_item->standard_cost);

			if ($order->fixed_asset) {
				$price = get_purchase_value($stock_item->stock_id);
				amount_cell($price);
			}
			else
				amount_cell($stock_item->standard_cost * $stock_item->quantity);

			if (!$order->fixed_asset) {
				if ($id == -1)
					edit_button_cell('Edit'.$line_no, _('Edit'), _('Edit document line'));
				else
					label_cell('');
			}
			if ($id == -1)
				delete_button_cell('Delete'.$line_no, _('Delete'), _('Remove line from document'));
			else
				label_cell('');
			
			end_row();

			// --- Serial/batch tracking panel for tracked items ---
			if (!$order->fixed_asset) {
				$tracking_mode = get_item_tracking_mode($stock_item->stock_id);
				$has_serial = ($tracking_mode === 'serial' || $tracking_mode === 'serial_batch');
				$has_batch = ($tracking_mode === 'batch' || $tracking_mode === 'serial_batch');
				$is_positive = ($stock_item->quantity > 0);

				if ($has_serial || $has_batch) {
					$track_colspan = count($th) - 1;
					echo '<tr class="tracking_panel">';
					echo '<td></td>';
					echo '<td colspan="' . $track_colspan . '" style="padding:4px 8px; background:' 
						. ($is_positive ? '#f0f9f0' : '#fff5f5') . '; border-left:3px solid '
						. ($is_positive ? '#28a745' : '#dc3545') . ';">';

					if ($is_positive) {
						// POSITIVE: Enter new serial numbers / batch info
						if ($has_serial) {
							$serial_key = 'adj_serial_' . $line_no;
							$serial_val = isset($_POST[$serial_key]) ? $_POST[$serial_key] : '';
							echo '<div style="margin-bottom:4px;">';
							echo '<label style="font-weight:bold; color:#28a745;"><i class="fa fa-barcode"></i> ';
							echo _('New Serial Numbers') . ' <small>(' . _('one per line or comma-separated') . ')</small></label><br>';
							echo '<textarea name="' . $serial_key . '" id="' . $serial_key . '" '
								. 'rows="3" cols="50" style="width:400px; font-family:monospace; font-size:12px;" '
								. 'placeholder="' . _('Enter new serial numbers to create') . '">'
								. htmlspecialchars($serial_val, ENT_QUOTES) . '</textarea>';
							echo '</div>';
						}

						if ($has_batch) {
							$batch_key = 'adj_batch_no_' . $line_no;
							$batch_val = isset($_POST[$batch_key]) ? $_POST[$batch_key] : '';
							$expiry_key = 'adj_batch_expiry_' . $line_no;
							$expiry_val = isset($_POST[$expiry_key]) ? $_POST[$expiry_key] : '';
							$mfg_key = 'adj_batch_mfg_' . $line_no;
							$mfg_val = isset($_POST[$mfg_key]) ? $_POST[$mfg_key] : '';

							echo '<div style="margin-bottom:4px;">';
							echo '<label style="font-weight:bold; color:#28a745;"><i class="fa fa-cubes"></i> ';
							echo _('Batch/Lot Info') . '</label><br>';
							echo '<table style="margin:2px 0;">';
							echo '<tr><td>' . _('Batch No') . ':</td>';
							echo '<td><input type="text" name="' . $batch_key . '" id="' . $batch_key . '" '
								. 'value="' . htmlspecialchars($batch_val, ENT_QUOTES) . '" size="20" maxlength="60" '
								. 'style="font-family:monospace;"></td>';
							echo '<td style="padding-left:10px;">' . _('Expiry') . ':</td>';
							echo '<td><input type="text" name="' . $expiry_key . '" id="' . $expiry_key . '" '
								. 'value="' . htmlspecialchars($expiry_val, ENT_QUOTES) . '" size="12" '
								. 'class="date_entry" placeholder="' . _('dd/mm/yyyy') . '"></td>';
							echo '<td style="padding-left:10px;">' . _('Mfg Date') . ':</td>';
							echo '<td><input type="text" name="' . $mfg_key . '" id="' . $mfg_key . '" '
								. 'value="' . htmlspecialchars($mfg_val, ENT_QUOTES) . '" size="12" '
								. 'class="date_entry" placeholder="' . _('dd/mm/yyyy') . '"></td>';
							echo '</tr></table>';
							echo '</div>';
						}
					} else {
						// NEGATIVE: Select existing serials to scrap / batches to reduce
						if ($has_serial) {
							$serial_key = 'adj_scrap_serial_' . $line_no;
							$serial_val = isset($_POST[$serial_key]) ? $_POST[$serial_key] : '';
							echo '<div style="margin-bottom:4px;">';
							echo '<label style="font-weight:bold; color:#dc3545;"><i class="fa fa-barcode"></i> ';
							echo _('Serials to Scrap') . ' <small>(' . _('enter serial numbers to mark as scrapped') . ')</small></label><br>';
							echo '<textarea name="' . $serial_key . '" id="' . $serial_key . '" '
								. 'rows="3" cols="50" style="width:400px; font-family:monospace; font-size:12px;" '
								. 'placeholder="' . _('Enter serial numbers to scrap (one per line)') . '">'
								. htmlspecialchars($serial_val, ENT_QUOTES) . '</textarea>';
							echo '</div>';
						}

						if ($has_batch) {
							$batch_key = 'adj_scrap_batch_' . $line_no;
							$batch_val = isset($_POST[$batch_key]) ? $_POST[$batch_key] : '';
							echo '<div style="margin-bottom:4px;">';
							echo '<label style="font-weight:bold; color:#dc3545;"><i class="fa fa-cubes"></i> ';
							echo _('Batch/Lot to Reduce') . ' <small>(' . _('format: batch_id:qty per line') . ')</small></label><br>';
							echo '<textarea name="' . $batch_key . '" id="' . $batch_key . '" '
								. 'rows="2" cols="50" style="width:400px; font-family:monospace; font-size:12px;" '
								. 'placeholder="' . _('e.g. 5:100 or batch_id:qty per line') . '">'
								. htmlspecialchars($batch_val, ENT_QUOTES) . '</textarea>';
							echo '</div>';
						}
					}

					echo '</td></tr>';
				}
			}
			// --- End tracking panel ---
		}
		else
			adjustment_edit_item_controls($order, $line_no);
	}

	if ($id == -1)
		adjustment_edit_item_controls($order);
	$row = !$order->fixed_asset ? 6 : 5;
	label_row(_('Total'), number_format2($total,user_price_dec()), 'align=right colspan='.$row, 'align=right', 2);

	end_table();

	if ($id != -1)
		hidden('selected_id', $id);
	if ($low_stock)
		display_note(_('Marked items have insufficient quantities in stock as on day of adjustment.'), 0, 1, "class='stockmankofg'");
	div_end();
}

function adjustment_edit_item_controls(&$order, $line_no=-1) {
	global $Ajax;

	start_row();
	$dec2 = 0;

	if ($line_no != -1) {
		if (find_submit('Edit') != -1) {
			$_POST['stock_id'] = $order->line_items[$line_no]->stock_id;
			$_POST['qty'] = qty_format($order->line_items[$line_no]->quantity, $order->line_items[$line_no]->stock_id, $dec);
			$_POST['std_cost'] = price_decimal_format($order->line_items[$line_no]->standard_cost, $dec2);
		}
		$_POST['units'] = $order->line_items[$line_no]->units;
		hidden('stock_id', $_POST['stock_id']);
		label_cell($_POST['stock_id']);
		label_cell($order->line_items[$line_no]->item_description, 'nowrap');
		$Ajax->activate('items_table');
	}
	else {
		if ($order->fixed_asset)
			stock_disposable_fa_list_cells(null, 'stock_id', null, false, true, $order->line_items);
		else
			stock_costable_items_list_cells(null, 'stock_id', null, false, true);

		if (list_updated('stock_id'))
			$Ajax->activate('items_table');
	}

	$item_info = get_item_edit_info(get_post('stock_id'));
	if ($item_info) {
		$dec = $item_info['decimals'];
		$_POST['std_cost'] = price_decimal_format($item_info['material_cost'], $dec2);
		$_POST['units'] = $item_info['units'];
	}
	else
		$dec = 0;
	if ($order->fixed_asset) {
		hidden('qty', -1);
		qty_cell(1, false, 0);
	}
	else {
		$qoh = get_qoh_on_date(get_post('stock_id'), $_POST['StockLocation']);
		qty_cell($qoh, false, $dec);
		label_cell( text_input('qty', get_post('qty', '0'), 15, 15, '',  'class="amount active" dec='.$dec), 'nowrap align=right');
	}
	label_cell(get_post('units'), '', 'units');

	if ($order->fixed_asset) {
		hidden('std_cost');
		label_cell($_POST['std_cost'], 'nowrap align=right ', 'std_cost_view');
		$price = get_purchase_value($_POST['stock_id']);
		amount_cell($price, false, '', 'initial_price');
	}
	else {
		if (get_post('_qty_changed')) {
			$Ajax->activate('items_table');
			set_focus(input_num('qty') >= 0 ? 'std_cost' : 'AddItem');
		}
		if (input_num('qty') >= 0)
			amount_cells(null, 'std_cost', null, null, null, $dec2);
		else {
			hidden('std_cost');
			label_cell($_POST['std_cost'], 'nowrap align=right');
		}
		label_cell('&nbsp;');
	}

	if ($line_no != -1) {
		button_cell('UpdateItem', _('Update'), _('Confirm changes'), ICON_UPDATE);
		button_cell('CancelItemChanges', _('Cancel'), _('Cancel changes'), ICON_CANCEL);
		hidden('LineNo', $line_no);
		if(empty($_POST['_qty_changed']))
			set_focus('qty');
	}
	else
		submit_cells('AddItem', _('Add Item'), 'colspan=2', _('Add new item to document'), true);

	end_row();
}

function adjustment_options_controls() {
	echo '<br>';
	start_table();
	textarea_row(_('Memo'), 'memo_', null, 50, 3);
	end_table(1);
}
