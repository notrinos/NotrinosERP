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

/**
 * Transfer Orders — Formal inter-branch transfer management.
 *
 * Lifecycle: Draft → Approved → Shipped (In Transit) → Received
 * Supports partial receipt with automatic backorder creation.
 *
 * @package NotrinosERP
 * @subpackage Inventory/Warehouse
 */

$page_security = 'SA_TRANSFERORDERS';
$path_to_root = '../..';

include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/includes/date_functions.inc');
include_once($path_to_root . '/includes/data_checks.inc');
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/includes/ui/ui_lists.inc');
include_once($path_to_root . '/inventory/warehouse/includes/db/warehouse_transfers_db.inc');
include_once($path_to_root . '/inventory/includes/db/serial_batch_db.inc');

$js = '';
if (user_use_date_picker())
	$js .= get_js_date_picker();

page(_($help_context = 'Transfer Orders'), false, false, '', $js);

// =====================================================================
// HANDLE ACTIONS
// =====================================================================

$selected_id = get_post('selected_id', -1);
if ($selected_id == '') $selected_id = -1;

// --- Approve action ---
if (isset($_POST['Approve']) && $selected_id > 0) {
	if (approve_transfer_order($selected_id)) {
		display_notification(_('Transfer order has been approved.'));
	} else {
		display_error(_('Cannot approve this transfer order. It may not be in Draft status.'));
	}
	$selected_id = -1;
}

// --- Ship action ---
if (isset($_POST['Ship']) && $selected_id > 0) {
	$ship_date = get_post('ship_date');
	if (empty($ship_date)) $ship_date = Today();
	if (ship_transfer_order($selected_id, $ship_date)) {
		display_notification(_('Transfer order has been shipped and is now in transit.'));
	} else {
		display_error(_('Cannot ship this transfer order. It must be in Approved status.'));
	}
	$selected_id = -1;
}

// --- Receive action ---
if (isset($_POST['Receive']) && $selected_id > 0) {
	$recv_date = get_post('recv_date');
	if (empty($recv_date)) $recv_date = Today();

	// Collect received qty per line
	$received = array();
	$lines = get_transfer_order_lines($selected_id);
	while ($line = db_fetch($lines)) {
		$key = 'recv_qty_' . $line['line_id'];
		if (isset($_POST[$key])) {
			$received[(int)$line['line_id']] = input_num($key);
		}
	}

	$recv_result = receive_transfer_order($selected_id, $recv_date, $received);
	if ($recv_result['success']) {
		display_notification($recv_result['message']);
		if ($recv_result['backorder_id']) {
			display_notification(sprintf(_('Backorder Transfer Order ID: %d'), $recv_result['backorder_id']));
		}
	} else {
		display_error($recv_result['message']);
	}
	$selected_id = -1;
}

// --- Cancel action ---
if (isset($_POST['Cancel']) && $selected_id > 0) {
	if (cancel_transfer_order($selected_id)) {
		display_notification(_('Transfer order has been cancelled.'));
	} else {
		display_error(_('Cannot cancel this transfer order.'));
	}
	$selected_id = -1;
}

// --- Delete action ---
if (isset($_POST['Delete']) && $selected_id > 0) {
	if (delete_transfer_order($selected_id)) {
		display_notification(_('Transfer order has been deleted.'));
	} else {
		display_error(_('Cannot delete this transfer order. Only Draft or Cancelled orders can be deleted.'));
	}
	$selected_id = -1;
}

// --- Save new/edit ---
if (isset($_POST['ADD_ITEM']) || isset($_POST['UPDATE_ITEM'])) {
	$input_error = 0;

	if (get_post('from_loc') == get_post('to_loc')) {
		display_error(_('Source and destination locations must be different.'));
		$input_error = 1;
	}
	if (empty(get_post('from_loc')) || empty(get_post('to_loc'))) {
		display_error(_('You must select both source and destination locations.'));
		$input_error = 1;
	}

	if ($input_error == 0) {
		$request_date = date2sql(get_post('request_date'));
		$expected_ship = get_post('expected_ship') ? date2sql(get_post('expected_ship')) : null;
		$expected_recv = get_post('expected_recv') ? date2sql(get_post('expected_recv')) : null;
		$reference = get_post('reference');
		$memo = get_post('memo');
		$transfer_type = get_post('transfer_type', 'standard');

		if (isset($_POST['ADD_ITEM'])) {
			$new_id = add_transfer_order(
				get_post('from_loc'), get_post('to_loc'),
				$request_date, $transfer_type,
				$reference, $memo, $expected_ship, $expected_recv
			);
			display_notification(sprintf(_('Transfer order #%d has been created.'), $new_id));
			$selected_id = $new_id; // Stay on the new order to add lines
		} else {
			update_transfer_order(
				$selected_id,
				get_post('from_loc'), get_post('to_loc'),
				$request_date, $transfer_type,
				$reference, $memo, $expected_ship, $expected_recv
			);
			display_notification(_('Transfer order has been updated.'));
		}
	}
}

// --- Add line ---
if (isset($_POST['AddLine'])) {
	$line_stock = get_post('line_stock_id');
	$line_qty = input_num('line_qty');

	if (empty($line_stock)) {
		display_error(_('You must select an item.'));
	} elseif ($line_qty <= 0) {
		display_error(_('Quantity must be positive.'));
	} elseif ($selected_id > 0) {
		$item_info = get_item_edit_info($line_stock);
		$unit_cost = $item_info ? $item_info['material_cost'] : 0;

		add_transfer_order_line(
			$selected_id,
			$line_stock,
			$line_qty,
			null, null, // bin assignments (optional later)
			null, null, // batch/serial (assigned during ship)
			$unit_cost
		);
		display_notification(_('Line added to transfer order.'));
	}
}

// --- Delete line ---
$del_line = find_submit('DeleteLine');
if ($del_line > 0) {
	delete_transfer_order_line($del_line);
	display_notification(_('Line removed from transfer order.'));
}

// --- Edit button from list ---
$edit_id = find_submit('Edit');
if ($edit_id > 0) {
	$selected_id = $edit_id;
	$Ajax->activate('to_detail');
}

// --- View button from list ---
$view_id = find_submit('View');
if ($view_id > 0) {
	$selected_id = $view_id;
	$Ajax->activate('to_detail');
}

// --- New button ---
if (isset($_POST['New'])) {
	$selected_id = -1;
	$Ajax->activate('to_detail');
}

// =====================================================================
// DISPLAY
// =====================================================================

start_form();

// --- FILTERS ---
start_table(TABLESTYLE_NOBORDER);
start_row();

$filter_status = get_post('filter_status', '');
$statuses = array('' => _('All Statuses')) + get_transfer_order_statuses();
echo '<td>' . _('Status:') . ' ';
echo array_selector('filter_status', $filter_status, $statuses, array('select_submit' => true));
echo '</td>';

locations_list_cells(_('From:'), 'filter_from', null, true);
locations_list_cells(_('To:'), 'filter_to', null, true);

submit_cells('SearchOrders', _('Search'), '', _('Search transfer orders'), true);
submit_cells('New', _('New Transfer Order'), '', _('Create a new transfer order'), 'default');
end_row();
end_table();

// --- LIST ---
$filters = array();
if (!empty(get_post('filter_status')))
	$filters['status'] = get_post('filter_status');
if (!empty(get_post('filter_from')) && get_post('filter_from') != 'all')
	$filters['from_loc_code'] = get_post('filter_from');
if (!empty(get_post('filter_to')) && get_post('filter_to') != 'all')
	$filters['to_loc_code'] = get_post('filter_to');

// Summary cards
$summary = get_transfer_order_summary();
echo '<div style="display:flex;gap:10px;margin:10px 0;flex-wrap:wrap;">';
$status_cards = array(
	'draft'      => array('icon' => 'fa-file-o',     'color' => '#6c757d'),
	'approved'   => array('icon' => 'fa-check',       'color' => '#007bff'),
	'in_transit' => array('icon' => 'fa-truck',        'color' => '#fd7e14'),
	'received'   => array('icon' => 'fa-check-circle', 'color' => '#28a745'),
);
foreach ($status_cards as $st => $conf) {
	$cnt = isset($summary[$st]) ? $summary[$st] : 0;
	$st_labels = get_transfer_order_statuses();
	echo '<div style="background:#fff;border:1px solid #ddd;border-left:4px solid '
		. $conf['color'] . ';padding:8px 16px;border-radius:4px;min-width:120px;">'
		. '<div style="font-size:20px;font-weight:bold;color:' . $conf['color'] . ';">' . $cnt . '</div>'
		. '<div style="font-size:12px;color:#666;"><i class="fa ' . $conf['icon'] . '"></i> '
		. $st_labels[$st] . '</div></div>';
}
echo '</div>';

div_start('to_list');
$orders = get_transfer_orders_filtered($filters, 100);
start_table(TABLESTYLE, "width='100%'");
$th = array('#', _('Transfer #'), _('Type'), _('Status'), _('From'), _('To'),
	_('Lines'), _('Total Qty'), _('Request Date'), _('Requested By'), '');
table_header($th);

$k = 0;
while ($row = db_fetch($orders)) {
	alt_table_row_color($k);

	label_cell($row['transfer_id']);
	label_cell('<a href="' . $_SERVER['PHP_SELF'] . '?selected_id=' . $row['transfer_id'] . '">'
		. 'TO-' . str_pad($row['transfer_no'], 4, '0', STR_PAD_LEFT) . '</a>');
	label_cell(transfer_order_type_badge($row['transfer_type']));
	label_cell(transfer_order_status_badge($row['status']));
	label_cell($row['from_loc_name']);
	label_cell($row['to_loc_name']);
	label_cell($row['line_count'], 'align=right');
	qty_cell($row['total_qty'] ? $row['total_qty'] : 0);
	label_cell(sql2date($row['request_date']));
	label_cell($row['requested_by_name']);

	// Action buttons
	echo '<td nowrap>';
	if ($row['status'] === 'draft') {
		echo '<button class="ajaxsubmit" type="submit" name="Edit' . $row['transfer_id']
			. '" value="1" style="margin:1px;">' . _('Edit') . '</button> ';
	}
	echo '<button class="ajaxsubmit" type="submit" name="View' . $row['transfer_id']
		. '" value="1" style="margin:1px;">' . _('View') . '</button>';
	echo '</td>';
	end_row();
}
end_table(1);
div_end();

// =====================================================================
// DETAIL VIEW / EDIT FORM
// =====================================================================

div_start('to_detail');
if ($selected_id > 0 || isset($_POST['New'])) {
	$editing = false;
	$order = null;

	if ($selected_id > 0) {
		$order = get_transfer_order($selected_id);
		if (!$order) {
			display_error(_('Transfer order not found.'));
			$selected_id = -1;
		}
	}

	if ($selected_id > 0 && $order) {
		$editing = ($order['status'] === 'draft');

		// Display order header info
		echo '<div style="background:#f8f9fa;border:1px solid #dee2e6;border-radius:4px;padding:15px;margin:10px 0;">';
		echo '<h3 style="margin:0 0 10px;">' . _('Transfer Order') . ': TO-'
			. str_pad($order['transfer_no'], 4, '0', STR_PAD_LEFT) . ' '
			. transfer_order_status_badge($order['status']) . ' '
			. transfer_order_type_badge($order['transfer_type']) . '</h3>';

		start_table(TABLESTYLE2);
		label_row(_('From Location:'), $order['from_loc_name']);
		label_row(_('To Location:'), $order['to_loc_name']);
		label_row(_('Request Date:'), sql2date($order['request_date']));
		if ($order['expected_ship_date'])
			label_row(_('Expected Ship:'), sql2date($order['expected_ship_date']));
		if ($order['actual_ship_date'])
			label_row(_('Actual Ship:'), sql2date($order['actual_ship_date']));
		if ($order['expected_recv_date'])
			label_row(_('Expected Receive:'), sql2date($order['expected_recv_date']));
		if ($order['actual_recv_date'])
			label_row(_('Actual Receive:'), sql2date($order['actual_recv_date']));
		if ($order['reference'])
			label_row(_('Reference:'), $order['reference']);
		if ($order['memo'])
			label_row(_('Memo:'), $order['memo']);
		label_row(_('Requested By:'), $order['requested_by_name']);
		if ($order['approved_by_name'])
			label_row(_('Approved By:'), $order['approved_by_name']);
		if ($order['shipped_by_name'])
			label_row(_('Shipped By:'), $order['shipped_by_name']);
		if ($order['received_by_name'])
			label_row(_('Received By:'), $order['received_by_name']);
		end_table(1);

		// --- Line items ---
		display_heading(_('Line Items'));
		$show_receive_qty = in_array($order['status'], array('in_transit', 'shipped'));

		start_table(TABLESTYLE, "width='100%'");
		$th = array(_('Item Code'), _('Description'), _('Qty Requested'), _('Qty Shipped'),
			_('Qty Received'), _('Unit'), _('Unit Cost'));
		if ($show_receive_qty)
			$th[] = _('Receive Qty');
		if ($editing)
			$th[] = '';
		table_header($th);

		$k = 0;
		$lines = get_transfer_order_lines($selected_id);
		while ($line = db_fetch($lines)) {
			alt_table_row_color($k);

			label_cell($line['stock_id']);
			label_cell($line['item_description']);
			qty_cell($line['qty_requested']);
			qty_cell($line['qty_shipped']);
			qty_cell($line['qty_received']);
			label_cell($line['item_units']);
			amount_decimal_cell($line['unit_cost']);

			if ($show_receive_qty) {
				echo '<td>';
				$outstanding = $line['qty_shipped'] - $line['qty_received'];
				echo '<input type="text" name="recv_qty_' . $line['line_id']
					. '" value="' . number_format2($outstanding, get_qty_dec($line['stock_id']))
					. '" size="8" class="amount">';
				echo '</td>';
			}

			if ($editing) {
				echo '<td>';
				echo '<button type="submit" name="DeleteLine' . $line['line_id']
					. '" value="1" class="ajaxsubmit" onclick="return confirm(\'' . _('Delete this line?') . '\');">'
					. _('Delete') . '</button>';
				echo '</td>';
			}

			// Show tracking info
			if ($line['serial_no']) {
				end_row();
				echo '<tr><td></td><td colspan="6" style="padding:2px 10px;border-left:3px solid #5b9bd5;background:#f7f9fc;font-size:11px;">';
				echo '<i class="fa fa-barcode"></i> ' . _('Serial:') . ' <b>' . $line['serial_no'] . '</b>';
				echo '</td></tr>';
			}
			if ($line['batch_no']) {
				end_row();
				echo '<tr><td></td><td colspan="6" style="padding:2px 10px;border-left:3px solid #e8a838;background:#fffaf0;font-size:11px;">';
				echo '<i class="fa fa-cubes"></i> ' . _('Batch:') . ' <b>' . $line['batch_no'] . '</b>';
				echo '</td></tr>';
			}

			end_row();
		}
		end_table(1);

		// --- Add line form (draft only) ---
		if ($editing) {
			display_heading(_('Add Line'));
			start_table(TABLESTYLE2);
			stock_costable_items_list_cells(_('Item:'), 'line_stock_id', null, false, true);
			qty_row(_('Quantity:'), 'line_qty', null, null, null, 4);
			end_table(1);

			hidden('selected_id', $selected_id);
			submit_center('AddLine', _('Add Line'), true, '', 'default');
			echo '<br>';
		}

		// --- Action buttons based on status ---
		echo '<div style="text-align:center;margin:15px 0;">';
		hidden('selected_id', $selected_id);

		if ($order['status'] === 'draft') {
			submit('Approve', _('Approve'), true, _('Approve this transfer order'));
			echo ' ';
			submit('Delete', _('Delete'), true, _('Delete this transfer order'));
		}
		if ($order['status'] === 'approved') {
			echo '<br><br>';
			start_table(TABLESTYLE_NOBORDER);
			date_row(_('Ship Date:'), 'ship_date', '', true);
			end_table();
			submit('Ship', _('Ship Items'), true, _('Mark as shipped and create stock moves'));
		}
		if ($show_receive_qty) {
			echo '<br><br>';
			start_table(TABLESTYLE_NOBORDER);
			date_row(_('Receive Date:'), 'recv_date', '', true);
			end_table();
			submit('Receive', _('Receive Items'), true, _('Receive items at destination'));
		}
		if (in_array($order['status'], array('draft', 'approved', 'in_transit', 'shipped'))) {
			echo ' ';
			submit('Cancel', _('Cancel Order'), true, _('Cancel this transfer order'));
		}
		echo '</div>';
		echo '</div>';

	} elseif (isset($_POST['New']) || $selected_id == -1) {
		// --- New order form ---
		display_heading(_('New Transfer Order'));
		start_table(TABLESTYLE2);

		$types = array('standard' => _('Standard'), 'urgent' => _('Urgent'));
		array_selector_row(_('Transfer Type:'), 'transfer_type', null, $types);
		locations_list_row(_('From Location:'), 'from_loc', null);
		locations_list_row(_('To Location:'), 'to_loc', null);
		date_row(_('Request Date:'), 'request_date', '', true);
		date_row(_('Expected Ship Date:'), 'expected_ship');
		date_row(_('Expected Receive Date:'), 'expected_recv');
		text_row(_('Reference:'), 'reference', null, 30, 60);
		textarea_row(_('Memo:'), 'memo', null, 50, 3);

		end_table(1);
		submit_center('ADD_ITEM', _('Create Transfer Order'), true, '', 'default');
	}
}
div_end();

end_form();
end_page();
