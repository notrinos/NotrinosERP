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

//--------------------------------------------------------------------------------

function add_to_issue(&$order, $new_item, $new_item_qty, $standard_cost) {
	if ($order->find_cart_item($new_item))
		 display_error(_('For Part :').$new_item.' '.'This item is already on this issue.  You can change the quantity issued of the existing line if necessary.');
	else
		 $order->add_to_cart (count($order->line_items), $new_item, $new_item_qty, $standard_cost);
}

//---------------------------------------------------------------------------------

function display_issue_items($title, &$order) {
	global $path_to_root;

	display_heading($title);
	div_start('items_table');
	start_table(TABLESTYLE, "width='80%'");
	$th = array(_('Item Code'), _('Item Description'), _('Quantity'), _('Unit'), _('Unit Cost'), '');
	if ( count($order->line_items)) $th[] = '';

	table_header($th);
	// $total = 0;
	$k = 0;  //row colour counter

	if (count($order->line_items))
		$low_stock = $order->check_qoh($_POST['Location'], $_POST['date_'], !$_POST['IssueType']);
	$id = find_submit('Edit');
	foreach ($order->line_items as $line_no=>$stock_item) {

		if ($id != $line_no) {
			if (in_array($stock_item->stock_id, $low_stock))
				start_row("class='stockmankobg'");	// notice low stock status
			else 
				alt_table_row_color($k);

			view_stock_status_cell($stock_item->stock_id);
			label_cell($stock_item->item_description);
			qty_cell($stock_item->quantity, false, get_qty_dec($stock_item->stock_id));
			label_cell($stock_item->units);
			amount_cell($stock_item->standard_cost);
			// amount_cell($stock_item->standard_cost * $stock_item->quantity);

			edit_button_cell('Edit'.$line_no, _('Edit'), _('Edit document line'));
			delete_button_cell('Delete'.$line_no, _('Delete'), _('Remove line from document'));
			end_row();
		}
		else
			issue_edit_item_controls($order, $line_no);
	}

	if ($id == -1)
		issue_edit_item_controls($order);

	end_table();
	if (@$low_stock)
		display_note(_('Marked items have insufficient quantities in stock as on day of issue.'), 0, 1, "class='stockmankofg'");

	// --- Serial/Batch tracking panels for component issues ---
	if (function_exists('get_item_tracking_mode') && count($order->line_items) > 0) {
		$has_tracking_items = false;
		foreach ($order->line_items as $ln => $si) {
			$tm = get_item_tracking_mode($si->stock_id);
			if ($tm !== 'none') { $has_tracking_items = true; break; }
		}
		if ($has_tracking_items) {
			echo '<br>';
			display_heading(_('Serial/Batch Tracking for Components'));
			$location = isset($_POST['Location']) ? $_POST['Location'] : '';
			$num_cols = 3; // for table layout
			start_table(TABLESTYLE, "width='80%'");
			foreach ($order->line_items as $ln => $si) {
				$tm = get_item_tracking_mode($si->stock_id);
				if ($tm === 'none') continue;

				start_row();
				echo '<td colspan="'.$num_cols.'" style="padding:8px;">';
				echo '<div style="border-left:3px solid '
					. (item_has_serial_tracking($si->stock_id) ? '#2196F3' : '#FF9800')
					. '; background:#f7f9fc; padding:8px; margin:4px 0;">';
				echo '<b>' . $si->stock_id . '</b> — ' . $si->item_description
					. ' (' . _('Qty:') . ' ' . number_format2($si->quantity, get_qty_dec($si->stock_id)) . ')';

				if (item_has_serial_tracking($si->stock_id)) {
					echo '<br><small style="color:#666;">' . _('Available serials at location:') . '</small> ';
					$avail = get_serial_numbers_at_location($si->stock_id, $location, 'available');
					$sn_list = array();
					while ($srow = db_fetch($avail)) {
						$sn_list[] = $srow['serial_no'];
					}
					if (!empty($sn_list)) {
						echo '<small>' . implode(', ', array_slice($sn_list, 0, 20))
							. (count($sn_list) > 20 ? ' ...' : '') . '</small>';
					} else {
						echo '<small style="color:#999;">' . _('None available') . '</small>';
					}
					echo '<br><label>' . _('Serial Numbers (one per line or comma-separated):') . '</label><br>';
					$key = 'issue_serial_' . $ln;
					echo '<textarea name="' . $key . '" rows="3" cols="50" style="width:90%;">'
						. (isset($_POST[$key]) ? htmlspecialchars($_POST[$key]) : '')
						. '</textarea>';
				}

				if (item_has_batch_tracking($si->stock_id)) {
					echo '<br><small style="color:#666;">' . _('Available batches (FEFO order):') . '</small>';
					$avail_batches = array();
					if (function_exists('get_fefo_batches')) {
						$avail_batches = get_fefo_batches($si->stock_id, $location, 0);
					}
					if (!empty($avail_batches)) {
						echo '<table style="margin:4px 0; font-size:0.9em; border-collapse:collapse;">';
						echo '<tr style="background:#eee;">'
							. '<th style="padding:2px 8px;">' . _('ID') . '</th>'
							. '<th style="padding:2px 8px;">' . _('Batch #') . '</th>'
							. '<th style="padding:2px 8px;">' . _('Available') . '</th>'
							. '<th style="padding:2px 8px;">' . _('Expiry') . '</th>'
							. '</tr>';
						foreach ($avail_batches as $ab) {
							echo '<tr>'
								. '<td style="padding:2px 8px;">' . $ab['batch_id'] . '</td>'
								. '<td style="padding:2px 8px;">' . $ab['batch_no'] . '</td>'
								. '<td style="padding:2px 8px; text-align:right;">' . number_format2($ab['qty_available'], get_qty_dec($si->stock_id)) . '</td>'
								. '<td style="padding:2px 8px;">' . ($ab['expiry_date'] ? sql2date($ab['expiry_date']) : '-') . '</td>'
								. '</tr>';
						}
						echo '</table>';
					} else {
						echo ' <small style="color:#999;">' . _('None available') . '</small>';
					}
					echo '<br><label>' . _('Batch allocations (batch_id:qty per line, blank = auto FEFO):') . '</label><br>';
					$key = 'issue_batch_' . $ln;
					echo '<textarea name="' . $key . '" rows="3" cols="50" style="width:90%;">'
						. (isset($_POST[$key]) ? htmlspecialchars($_POST[$key]) : '')
						. '</textarea>';
				}

				echo '</div></td>';
				end_row();
			}
			end_table();
		}
	}

	div_end();
}

//---------------------------------------------------------------------------------

function issue_edit_item_controls(&$order, $line_no=-1) {
	global $Ajax;
	start_row();

	$id = find_submit('Edit');
	if ($line_no != -1 && $line_no == $id) {
		$_POST['stock_id'] = $order->line_items[$id]->stock_id;
		$_POST['qty'] = qty_format($order->line_items[$id]->quantity, $order->line_items[$id]->stock_id, $dec);
		$std_cost = $order->line_items[$id]->standard_cost;
		$_POST['units'] = $order->line_items[$id]->units;

		hidden('stock_id', $_POST['stock_id']);
		label_cell($_POST['stock_id']);
		label_cell($order->line_items[$id]->item_description);
		$Ajax->activate('items_table');
	}
	else {
		$wo_details = get_work_order($_SESSION['issue_items']->order_id);
		echo stock_items_list('stock_id', null, false, true,
			array(
				'where'=>array("stock_id != ".db_escape($wo_details['stock_id'])),
				'cells'=>true,
				'parent'=> $wo_details['stock_id']
			), false, 'component');
		if (list_updated('stock_id')) {
			$Ajax->activate('units');
			$Ajax->activate('qty');
			$Ajax->activate('std_cost');
		}

		$item_info = get_item_edit_info($_POST['stock_id']);

		$dec = $item_info['decimals'];
		$_POST['qty'] =	number_format2(0, $dec);
		$std_cost = $item_info['material_cost'];
		$_POST['units'] = $item_info['units'];
	}

	qty_cells(null, 'qty', $_POST['qty'], null, null, $dec);
	label_cell($_POST['units'], '', 'units');

	//amount_cells(null, 'std_cost', $_POST['std_cost']);
	hidden('std_cost', $std_cost);
	amount_cell($std_cost);

	if ($id != -1) {
		button_cell('UpdateItem', _('Update'), _('Confirm changes'), ICON_UPDATE);
		button_cell('CancelItemChanges', _('Cancel'), _('Cancel changes'), ICON_CANCEL);
		hidden('LineNo', $line_no);
		set_focus('qty');
	}
	else
		submit_cells('AddItem', _('Add Item'), 'colspan=2', _('Add new item to document'), true);

	end_row();
}


//---------------------------------------------------------------------------------

function issue_options_controls() {
	global $Refs;

	echo '<br>';
	start_table();
	date_row(_('Issue Date:'), 'date_');
	locations_list_row(_('From Location:'), 'Location');
	workcenter_list_row(_('To Work Centre:'), 'WorkCentre');

	ref_row(_('Reference:'), 'ref', '', $Refs->get_next(ST_MANUISSUE, null, array('date'=> get_post('date_'), 'location' => get_post('Location'))));

	if (!isset($_POST['IssueType']))
		$_POST['IssueType'] = 0;

	yesno_list_row(_('Type:'), 'IssueType', $_POST['IssueType'], _('Return Items to Location'), _('Issue Items to Work order'));

	textarea_row(_('Memo'), 'memo_', null, 50, 3, 40); // stock move reference is 40 max

	end_table(1);
}
