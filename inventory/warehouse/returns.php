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
 * Return Orders â€” Customer & Supplier return processing with inspection
 * and disposition routing (restock / refurbish / scrap / return to vendor).
 *
 * Lifecycle: Draft â†’ Received â†’ Inspected â†’ Completed (or Cancelled)
 *
 * @package NotrinosERP
 * @subpackage Inventory/Warehouse
 */

$page_security = 'SA_WAREHOUSE_RETURNS';
$path_to_root = '../..';

include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/includes/date_functions.inc');
include_once($path_to_root . '/includes/data_checks.inc');
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/includes/ui/ui_lists.inc');
include_once($path_to_root . '/includes/db/inventory_db.inc');
include_once($path_to_root . '/gl/includes/db/gl_db_trans.inc');
include_once($path_to_root . '/inventory/warehouse/includes/db/warehouse_returns_db.inc');
include_once($path_to_root . '/inventory/warehouse/includes/db/warehouse_locations_db.inc');
include_once($path_to_root . '/inventory/warehouse/includes/warehouse_ui.inc');
include_once($path_to_root . '/inventory/includes/db/serial_numbers_db.inc');
include_once($path_to_root . '/inventory/includes/db/stock_batches_db.inc');
include_once($path_to_root . '/inventory/includes/db/serial_batch_db.inc');

$js = '';
if (user_use_date_picker())
	$js .= get_js_date_picker();

page(_($help_context = 'Return Orders'), false, false, '', $js);

// =====================================================================
// HANDLE ACTIONS
// =====================================================================

$selected_id = get_post('selected_id', -1);
if ($selected_id == '') $selected_id = -1;

// --- Create new return order ---
if (isset($_POST['CREATE_RETURN'])) {
	$input_error = 0;

	if (empty($_POST['return_type'])) {
		display_error(_('You must select a return type.'));
		$input_error = 1;
	}
	if (empty($_POST['warehouse'])) {
		display_error(_('You must select a warehouse.'));
		$input_error = 1;
	}

	if ($input_error == 0) {
		$return_id = add_return_order(
			$_POST['return_type'],
			$_POST['warehouse'],
			$_POST['return_date'],
			get_post('customer_id') ? (int)get_post('customer_id') : null,
			get_post('supplier_id') ? (int)get_post('supplier_id') : null,
			null, null,
			get_post('reference'),
			get_post('memo')
		);

		display_notification(sprintf(_('Return order RO#%s has been created.'), get_next_return_order_no() - 1));
		$selected_id = $return_id;
	}
	$Ajax->activate('_page_body');
}

// --- Update return order ---
if (isset($_POST['UPDATE_RETURN']) && $selected_id > 0) {
	$input_error = 0;

	if (empty($_POST['return_type'])) {
		display_error(_('You must select a return type.'));
		$input_error = 1;
	}

	if ($input_error == 0) {
		update_return_order(
			$selected_id,
			$_POST['return_type'],
			$_POST['warehouse'],
			$_POST['return_date'],
			get_post('customer_id') ? (int)get_post('customer_id') : null,
			get_post('supplier_id') ? (int)get_post('supplier_id') : null,
			null, null,
			get_post('reference'),
			get_post('memo')
		);
		display_notification(_('Return order has been updated.'));
	}
	$Ajax->activate('_page_body');
}

// --- Add line item ---
if (isset($_POST['ADD_LINE']) && $selected_id > 0) {
	$input_error = 0;

	if (empty($_POST['line_stock_id'])) {
		display_error(_('You must select an item.'));
		$input_error = 1;
	}
	$line_qty = input_num('line_qty', 0);
	if ($line_qty <= 0) {
		display_error(_('Quantity must be greater than zero.'));
		$input_error = 1;
	}

	if ($input_error == 0) {
		add_return_order_line(
			$selected_id,
			$_POST['line_stock_id'],
			$line_qty,
			get_post('line_serial_id') ? (int)get_post('line_serial_id') : null,
			get_post('line_batch_id') ? (int)get_post('line_batch_id') : null,
			get_post('line_reason_code'),
			get_post('line_memo')
		);
		display_notification(_('Line item has been added.'));
		unset($_POST['line_stock_id'], $_POST['line_qty'], $_POST['line_serial_id'],
			$_POST['line_batch_id'], $_POST['line_reason_code'], $_POST['line_memo']);
	}
	$Ajax->activate('_page_body');
}

// --- Delete line item ---
if (isset($_POST['DELETE_LINE'])) {
	$line_id = (int)get_post('delete_line_id');
	if ($line_id > 0) {
		delete_return_order_line($line_id);
		display_notification(_('Line item has been deleted.'));
	}
	$Ajax->activate('_page_body');
}

// --- Receive return ---
if (isset($_POST['RECEIVE_RETURN']) && $selected_id > 0) {
	if (receive_return_order($selected_id, $_POST['return_date'])) {
		display_notification(_('Return order has been received. Items are in quarantine.'));
	} else {
		display_error(_('Cannot receive this return order. Check status and line items.'));
	}
	$Ajax->activate('_page_body');
}

// --- Inspect line ---
if (isset($_POST['INSPECT_LINE'])) {
	$line_id = (int)get_post('inspect_line_id');
	$qty_good = input_num('inspect_qty_good_' . $line_id, 0);
	$qty_damaged = input_num('inspect_qty_damaged_' . $line_id, 0);
	$qty_scrap = input_num('inspect_qty_scrap_' . $line_id, 0);
	$disposition = get_post('inspect_disposition_' . $line_id);

	if ($line_id > 0) {
		inspect_return_line($line_id, $qty_good, $qty_damaged, $qty_scrap, $disposition);
		display_notification(_('Line inspection has been saved.'));
	}
	$Ajax->activate('_page_body');
}

// --- Mark inspected ---
if (isset($_POST['MARK_INSPECTED']) && $selected_id > 0) {
	mark_return_inspected($selected_id);
	display_notification(_('Return order has been marked as inspected.'));
	$Ajax->activate('_page_body');
}

// --- Complete return ---
if (isset($_POST['COMPLETE_RETURN']) && $selected_id > 0) {
	if (complete_return_order($selected_id, $_POST['return_date'])) {
		display_notification(_('Return order has been completed. Disposition routing applied.'));
	} else {
		display_error(_('Cannot complete this return order.'));
	}
	$selected_id = -1;
	$Ajax->activate('_page_body');
}

// --- Cancel return ---
if (isset($_POST['CANCEL_RETURN']) && $selected_id > 0) {
	if (cancel_return_order($selected_id)) {
		display_notification(_('Return order has been cancelled.'));
	} else {
		display_error(_('Cannot cancel this return order.'));
	}
	$selected_id = -1;
	$Ajax->activate('_page_body');
}

// --- Delete return ---
if (isset($_POST['DELETE_RETURN']) && $selected_id > 0) {
	if (delete_return_order($selected_id)) {
		display_notification(_('Return order has been deleted.'));
		$selected_id = -1;
	} else {
		display_error(_('Cannot delete this return order. Only draft orders can be deleted.'));
	}
	$Ajax->activate('_page_body');
}

// --- New order ---
if (isset($_POST['NEW_RETURN'])) {
	$selected_id = -1;
	$Ajax->activate('_page_body');
}

// --- Edit existing ---
if (isset($_GET['return_id'])) {
	$selected_id = (int)$_GET['return_id'];
}

// =====================================================================
// START FORM
// =====================================================================

start_form();
hidden('selected_id', $selected_id);

// =====================================================================
// Summary Cards
// =====================================================================

$summary = get_return_order_summary(get_post('filter_loc'));
$statuses = get_return_order_statuses();

echo "<div style='display:flex;gap:12px;flex-wrap:wrap;margin-bottom:16px;'>";
foreach ($statuses as $code => $label) {
	$cnt = isset($summary['by_status'][$code]) ? $summary['by_status'][$code] : 0;
	$color = get_return_status_color($code);
	echo "<div style='flex:1;min-width:120px;padding:10px 14px;background:#f8f9fa;border-left:4px solid " . $color . ";border-radius:4px;'>";
	echo "<div style='font-size:11px;color:#6c757d;text-transform:uppercase;'>" . $label . "</div>";
	echo "<div style='font-size:20px;font-weight:600;color:" . $color . ";'>" . $cnt . "</div>";
	echo "</div>";
}
echo "</div>";

// =====================================================================
// If editing/viewing a specific return order
// =====================================================================

if ($selected_id > 0) {
	$order = get_return_order($selected_id);
	if (!$order) {
		display_error(_('Return order not found.'));
		$selected_id = -1;
	}
}

if ($selected_id > 0 && $order) {

	// --- Order header display ---
	$status_color = get_return_status_color($order['status']);
	$status_label = $statuses[$order['status']];
	$types = get_return_order_types();
	$type_label = isset($types[$order['return_type']]) ? $types[$order['return_type']] : $order['return_type'];

	echo "<h3>";
	echo sprintf(_('Return Order RO#%s'), $order['return_no']);
	echo " <span style='display:inline-block;padding:3px 10px;border-radius:10px;font-size:12px;"
		. "background:" . $status_color . ";color:#fff;'>" . $status_label . "</span>";
	echo " <span style='font-size:14px;color:#6c757d;'>" . $type_label . "</span>";
	echo "</h3>";

	// --- Header details ---
	start_table(TABLESTYLE2);
	if ($order['status'] === 'draft') {
		// Editable form
		$type_options = get_return_order_types();
		echo "<tr><td class='label'>" . _('Return Type:') . "</td><td>";
		echo array_selector('return_type', $order['return_type'], $type_options, array('select_submit' => true));
		echo "</td></tr>";

		locations_list_row(_('Warehouse:'), 'warehouse', $order['warehouse_loc_code'], false, false, true);
		date_row(_('Return Date:'), 'return_date', sql2date($order['return_date']), null, 0, 0, 0);

		$return_type = get_post('return_type', $order['return_type']);
		if ($return_type === 'customer') {
			customer_list_row(_('Customer:'), 'customer_id', $order['customer_id'], false, true);
		} else {
			supplier_list_row(_('Supplier:'), 'supplier_id', $order['supplier_id'], false, true);
		}

		text_row(_('Reference:'), 'reference', $order['reference'], 30, 60);
		textarea_row(_('Memo:'), 'memo', $order['memo'], 60, 3);
	} else {
		// Read-only display
		label_row(_('Return Type:'), $type_label);
		label_row(_('Warehouse:'), $order['warehouse_name']);
		label_row(_('Return Date:'), sql2date($order['return_date']));
		if ($order['received_date'])
			label_row(_('Received Date:'), sql2date($order['received_date']));
		if ($order['return_type'] === 'customer')
			label_row(_('Customer:'), $order['customer_name']);
		else
			label_row(_('Supplier:'), $order['supplier_name']);
		if ($order['reference'])
			label_row(_('Reference:'), $order['reference']);
		if ($order['memo'])
			label_row(_('Memo:'), $order['memo']);
		if ($order['disposition_code']) {
			$disp_codes = get_return_disposition_codes();
			$disp_label = isset($disp_codes[$order['disposition_code']]) ? $disp_codes[$order['disposition_code']] : $order['disposition_code'];
			label_row(_('Disposition:'), $disp_label);
		}
	}
	end_table(1);

	// --- Line Items ---
	echo "<h4>" . _('Line Items') . "</h4>";

	$lines = get_return_order_lines($selected_id);
	$line_count = 0;

	if ($order['status'] === 'draft') {
		// Editable line items table
		start_table(TABLESTYLE, "width='95%'");
		$th = array(_('Item'), _('Description'), _('Qty Expected'), _('Serial'), _('Batch'), _('Reason'), _('Memo'), _(''));
		table_header($th);

		$k = 0;
		while ($line = db_fetch($lines)) {
			alt_table_row_color($k);
			label_cell($line['stock_id']);
			label_cell($line['description']);
			qty_cell($line['qty_expected']);
			label_cell($line['serial_no'] ? $line['serial_no'] : 'â€”');
			label_cell($line['batch_no'] ? $line['batch_no'] : 'â€”');
			label_cell($line['reason_code'] ? $line['reason_code'] : 'â€”');
			label_cell($line['memo'] ? $line['memo'] : '');
			echo "<td>";
			hidden('delete_line_id', $line['line_id']);
			submit('DELETE_LINE', _('Delete'), true, _('Delete this line'), 'default');
			echo "</td>";
			end_row();
			$line_count++;
		}
		end_table(1);

		// Add line form
		echo "<h5>" . _('Add Line Item') . "</h5>";
		start_table(TABLESTYLE2);
		echo "<tr>";
		stock_items_list_cells(_('Item:'), 'line_stock_id', get_post('line_stock_id'), false, true);
		echo "</tr>";
		small_amount_row(_('Expected Qty:'), 'line_qty', get_post('line_qty', ''), null, null, user_qty_dec());

		// Serial/Batch selectors based on item tracking
		$line_stock = get_post('line_stock_id');
		if ($line_stock) {
			$tracking = get_item_tracking_mode($line_stock);
			if ($tracking === 'serial' || $tracking === 'both') {
				$serial_sql = "SELECT id, serial_no FROM " . TB_PREF . "serial_numbers"
					. " WHERE stock_id = " . db_escape($line_stock)
					. " AND status IN ('available', 'delivered', 'returned', 'quarantine')"
					. " ORDER BY serial_no";
				echo "<tr><td class='label'>" . _('Serial:') . "</td><td>";
				echo combo_input('line_serial_id', get_post('line_serial_id'), $serial_sql, 'id', 'serial_no',
					array('spec_option' => _('-- None --'), 'spec_id' => '', 'order' => false));
				echo "</td></tr>";
			}
			if ($tracking === 'batch' || $tracking === 'both') {
				$batch_sql = "SELECT id, batch_no FROM " . TB_PREF . "stock_batches"
					. " WHERE stock_id = " . db_escape($line_stock)
					. " ORDER BY batch_no";
				echo "<tr><td class='label'>" . _('Batch:') . "</td><td>";
				echo combo_input('line_batch_id', get_post('line_batch_id'), $batch_sql, 'id', 'batch_no',
					array('spec_option' => _('-- None --'), 'spec_id' => '', 'order' => false));
				echo "</td></tr>";
			}
		}

		text_row(_('Reason:'), 'line_reason_code', get_post('line_reason_code'), 30, 20);
		text_row(_('Memo:'), 'line_memo', get_post('line_memo'), 60, 255);
		end_table(1);

		submit_center('ADD_LINE', _('Add Line'), true, _('Add line item to return order'), 'default');

	} elseif (in_array($order['status'], array('received', 'inspected'))) {
		// Inspection table
		start_table(TABLESTYLE, "width='95%'");
		$th = array(_('Item'), _('Description'), _('Expected'), _('Received'),
			_('Good'), _('Damaged'), _('Scrap'), _('Disposition'), _(''));
		table_header($th);

		$k = 0;
		while ($line = db_fetch($lines)) {
			alt_table_row_color($k);
			label_cell($line['stock_id']);
			label_cell($line['description']);
			qty_cell($line['qty_expected']);
			qty_cell($line['qty_received']);

			if ($order['status'] === 'received') {
				// Editable inspection fields
				small_amount_cells(null, 'inspect_qty_good_' . $line['line_id'],
					price_format($line['qty_good']), null, null, user_qty_dec());
				small_amount_cells(null, 'inspect_qty_damaged_' . $line['line_id'],
					price_format($line['qty_damaged']), null, null, user_qty_dec());
				small_amount_cells(null, 'inspect_qty_scrap_' . $line['line_id'],
					price_format($line['qty_scrap']), null, null, user_qty_dec());
				// Disposition dropdown
				$disp_codes = get_return_disposition_codes();
				echo "<td>";
				echo array_selector('inspect_disposition_' . $line['line_id'],
					$line['disposition_code'] ? $line['disposition_code'] : 'restock', $disp_codes);
				echo "</td>";
				echo "<td>";
				hidden('inspect_line_id', $line['line_id']);
				submit('INSPECT_LINE', _('Save'), true, _('Save inspection for this line'), 'default');
				echo "</td>";
			} else {
				// Read-only inspection results
				qty_cell($line['qty_good']);
				qty_cell($line['qty_damaged']);
				qty_cell($line['qty_scrap']);
				$disp_codes = get_return_disposition_codes();
				$disp_label = $line['disposition_code'] && isset($disp_codes[$line['disposition_code']])
					? $disp_codes[$line['disposition_code']] : 'â€”';
				$disp_color = $line['disposition_code'] ? get_disposition_color($line['disposition_code']) : '#6c757d';
				label_cell("<span style='color:" . $disp_color . ";font-weight:600;'>" . $disp_label . "</span>");
				label_cell('');
			}
			end_row();
			$line_count++;
		}
		end_table(1);

	} else {
		// Completed/cancelled â€” read-only
		start_table(TABLESTYLE, "width='95%'");
		$th = array(_('Item'), _('Description'), _('Expected'), _('Received'),
			_('Good'), _('Damaged'), _('Scrap'), _('Disposition'));
		table_header($th);

		$k = 0;
		while ($line = db_fetch($lines)) {
			alt_table_row_color($k);
			label_cell($line['stock_id']);
			label_cell($line['description']);
			qty_cell($line['qty_expected']);
			qty_cell($line['qty_received']);
			qty_cell($line['qty_good']);
			qty_cell($line['qty_damaged']);
			qty_cell($line['qty_scrap']);
			$disp_codes = get_return_disposition_codes();
			$disp_label = $line['disposition_code'] && isset($disp_codes[$line['disposition_code']])
				? $disp_codes[$line['disposition_code']] : 'â€”';
			$disp_color = $line['disposition_code'] ? get_disposition_color($line['disposition_code']) : '#6c757d';
			label_cell("<span style='color:" . $disp_color . ";font-weight:600;'>" . $disp_label . "</span>");
			end_row();
			$line_count++;
		}
		end_table(1);
	}

	// --- Action Buttons ---
	echo "<div style='margin-top:12px;'>";
	if ($order['status'] === 'draft') {
		submit_center_first('UPDATE_RETURN', _('Save Changes'), true, _('Save return order'), 'default');
		submit('RECEIVE_RETURN', _('Receive'), true, _('Receive return and create stock moves'), 'default');
		submit('DELETE_RETURN', _('Delete'), true, _('Delete this return order'), 'cancel');
	} elseif ($order['status'] === 'received') {
		submit_center_first('MARK_INSPECTED', _('Mark Inspected'), true, _('Mark all lines as inspected'), 'default');
		submit('CANCEL_RETURN', _('Cancel'), true, _('Cancel this return order'), 'cancel');
	} elseif ($order['status'] === 'inspected') {
		submit_center_first('COMPLETE_RETURN', _('Complete & Route'), true, _('Complete return and apply disposition routing'), 'default');
		submit('CANCEL_RETURN', _('Cancel'), true, _('Cancel this return order'), 'cancel');
	}

	echo "&nbsp;&nbsp;";
	submit('NEW_RETURN', _('New Return Order'), true, _('Create a new return order'), 'default');
	echo "</div>";

} else {

	// =====================================================================
	// Filter Bar
	// =====================================================================

	echo "<h3>" . _('Return Orders') . "</h3>";

	start_table(TABLESTYLE2);
	start_row();

	$type_options = array_merge(array('' => _('-- All Types --')), get_return_order_types());
	echo "<td class='label'>" . _('Type:') . "</td><td>";
	echo array_selector('filter_type', get_post('filter_type'), $type_options);
	echo "</td>";

	$status_options = array_merge(array('' => _('-- All Statuses --')), get_return_order_statuses());
	echo "<td class='label'>" . _('Status:') . "</td><td>";
	echo array_selector('filter_status', get_post('filter_status'), $status_options);
	echo "</td>";

	locations_list_cells(_('Warehouse:'), 'filter_loc', get_post('filter_loc'), true, false, true);
	submit_cells('SearchOrders', _('Search'), '', _('Search return orders'), 'default');
	submit_cells('NEW_RETURN_BTN', _('+ New Return'), '', _('Create a new return order'), 'default');
	end_row();
	end_table(1);

	// --- Handle new return button from filter bar ---
	if (isset($_POST['NEW_RETURN_BTN'])) {
		// Show creation form below
		echo "<h3>" . _('New Return Order') . "</h3>";
		start_table(TABLESTYLE2);
		$type_options = get_return_order_types();
		echo "<tr><td class='label'>" . _('Return Type:') . "</td><td>";
		echo array_selector('return_type', get_post('return_type', 'customer'), $type_options, array('select_submit' => true));
		echo "</td></tr>";

		locations_list_row(_('Warehouse:'), 'warehouse', get_post('warehouse'), false, false, true);
		date_row(_('Return Date:'), 'return_date', '', null, 0, 0, 0);

		$return_type = get_post('return_type', 'customer');
		if ($return_type === 'customer') {
			customer_list_row(_('Customer:'), 'customer_id', null, false, true);
		} else {
			supplier_list_row(_('Supplier:'), 'supplier_id', null, false, true);
		}

		text_row(_('Reference:'), 'reference', '', 30, 60);
		textarea_row(_('Memo:'), 'memo', '', 60, 3);
		end_table(1);
		submit_center('CREATE_RETURN', _('Create Return Order'), true, _('Create a new return order'), 'default');

		$Ajax->activate('_page_body');
	}

	// =====================================================================
	// Return Orders List
	// =====================================================================

	$filters = array();
	if (get_post('filter_type')) $filters['return_type'] = get_post('filter_type');
	if (get_post('filter_status')) $filters['status'] = get_post('filter_status');
	if (get_post('filter_loc')) $filters['loc_code'] = get_post('filter_loc');

	$result = get_return_orders($filters);

	div_start('return_list');
	start_table(TABLESTYLE, "width='95%'");
	$th = array(_('RO#'), _('Type'), _('Status'), _('Date'), _('Customer/Supplier'),
		_('Warehouse'), _('Lines'), _('Expected'), _('Received'), _('Reference'), _(''));
	table_header($th);

	$k = 0;
	while ($row = db_fetch($result)) {
		alt_table_row_color($k);

		// RO# as clickable link
		$ro_link = "<a href='" . $_SERVER['PHP_SELF'] . "?return_id=" . $row['return_id'] . "'>"
			. "RO#" . $row['return_no'] . "</a>";
		label_cell($ro_link);

		// Type badge
		$types = get_return_order_types();
		$type_label = isset($types[$row['return_type']]) ? $types[$row['return_type']] : $row['return_type'];
		$type_color = ($row['return_type'] === 'customer') ? '#007bff' : '#fd7e14';
		label_cell("<span style='color:" . $type_color . ";font-weight:600;'>" . $type_label . "</span>");

		// Status badge
		$status_color = get_return_status_color($row['status']);
		$status_label = $statuses[$row['status']];
		label_cell("<span style='display:inline-block;padding:2px 8px;border-radius:10px;font-size:11px;"
			. "background:" . $status_color . ";color:#fff;'>" . $status_label . "</span>");

		label_cell(sql2date($row['return_date']));

		// Customer or supplier name
		$party_name = $row['return_type'] === 'customer' ? $row['customer_name'] : $row['supplier_name'];
		label_cell($party_name ? $party_name : 'â€”');

		label_cell($row['warehouse_name']);
		label_cell($row['line_count'], "align='center'");
		qty_cell($row['total_expected']);
		qty_cell($row['total_received']);
		label_cell($row['reference'] ? $row['reference'] : '');

		// Action link
		$edit_link = "<a href='" . $_SERVER['PHP_SELF'] . "?return_id=" . $row['return_id'] . "'>"
			. _('View/Edit') . "</a>";
		label_cell($edit_link);

		end_row();
	}
	end_table(1);
	div_end();
}

end_form();
end_page();
