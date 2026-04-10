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
 * Consignment & VMI Management — Receive, consume, return vendor-owned stock.
 * VMI: Set min/max levels, view alerts, export stock levels for vendor.
 *
 * @package NotrinosERP
 * @subpackage Inventory/Warehouse
 */

$page_security = 'SA_CONSIGNMENT';
$path_to_root = '../..';

include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/includes/date_functions.inc');
include_once($path_to_root . '/includes/data_checks.inc');
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/includes/ui/ui_lists.inc');
include_once($path_to_root . '/includes/db/inventory_db.inc');
include_once($path_to_root . '/inventory/includes/db/items_db.inc');
include_once($path_to_root . '/gl/includes/db/gl_db_trans.inc');
include_once($path_to_root . '/purchasing/includes/db/supp_trans_db.inc');
include_once($path_to_root . '/purchasing/includes/db/suppliers_db.inc');
include_once($path_to_root . '/inventory/warehouse/includes/db/warehouse_consignment_db.inc');
include_once($path_to_root . '/inventory/warehouse/includes/db/warehouse_locations_db.inc');
include_once($path_to_root . '/inventory/warehouse/includes/warehouse_ui.inc');
include_once($path_to_root . '/inventory/includes/db/serial_batch_db.inc');

$js = '';
if (user_use_date_picker())
	$js .= get_js_date_picker();

page(_($help_context = 'Consignment & VMI'), false, false, '', $js);

// =====================================================================
// TAB NAVIGATION
// =====================================================================

$tabs = array(
	'overview' => _('Stock Overview'),
	'receive'  => _('Receive'),
	'consume'  => _('Consume'),
	'return'   => _('Return to Vendor'),
	'vmi'      => _('VMI Settings'),
	'history'  => _('History'),
);

$current_tab = isset($_GET['tab']) ? $_GET['tab'] : 'overview';
if (!isset($tabs[$current_tab]))
	$current_tab = 'overview';

echo '<div style="margin-bottom:15px;border-bottom:2px solid #dee2e6;padding-bottom:0;">';
foreach ($tabs as $key => $label) {
	$active = ($key === $current_tab);
	$style = 'display:inline-block;padding:8px 16px;margin-bottom:-2px;text-decoration:none;'
		. 'border:1px solid ' . ($active ? '#dee2e6' : 'transparent') . ';'
		. 'border-bottom:2px solid ' . ($active ? '#fff' : 'transparent') . ';'
		. 'border-radius:4px 4px 0 0;'
		. 'background:' . ($active ? '#fff' : 'transparent') . ';'
		. 'color:' . ($active ? '#495057' : '#007bff') . ';'
		. 'font-weight:' . ($active ? '600' : '400') . ';';
	echo '<a href="?tab=' . $key . '" style="' . $style . '">' . $label . '</a>';
}
echo '</div>';

// =====================================================================
// HANDLE ACTIONS — RECEIVE
// =====================================================================

if (isset($_POST['ADD_CONSIGNMENT'])) {
	$input_error = 0;

	if (empty($_POST['recv_stock_id'])) {
		display_error(_('You must select an item.'));
		$input_error = 1;
	}

	if (empty($_POST['recv_supplier_id'])) {
		display_error(_('You must select a supplier.'));
		$input_error = 1;
	}

	$recv_qty = input_num('recv_qty', 0);
	if ($recv_qty <= 0) {
		display_error(_('Quantity must be greater than zero.'));
		$input_error = 1;
	}

	$recv_cost = input_num('recv_unit_cost', 0);
	if ($recv_cost <= 0) {
		display_error(_('Unit cost must be greater than zero.'));
		$input_error = 1;
	}

	if (empty($_POST['recv_loc_code'])) {
		display_error(_('You must select a warehouse location.'));
		$input_error = 1;
	}

	if ($input_error == 0) {
		$wh_loc_id = get_post('recv_bin_id') ? (int)get_post('recv_bin_id') : null;
		$batch_id = get_post('recv_batch_id') ? (int)get_post('recv_batch_id') : null;

		$consignment_id = receive_consignment_stock(
			$_POST['recv_stock_id'],
			$_POST['recv_supplier_id'],
			$recv_qty,
			$recv_cost,
			$_POST['recv_date'],
			$_POST['recv_loc_code'],
			$wh_loc_id,
			$batch_id,
			get_post('recv_memo')
		);

		if ($consignment_id) {
			display_notification(sprintf(_('Consignment stock #%s received successfully.'), $consignment_id));
			$current_tab = 'receive';
			// Clear form
			unset($_POST['recv_stock_id'], $_POST['recv_supplier_id'], $_POST['recv_qty'],
				$_POST['recv_unit_cost'], $_POST['recv_memo'], $_POST['recv_batch_id']);
		}
	}
}

// =====================================================================
// HANDLE ACTIONS — CONSUME
// =====================================================================

if (isset($_POST['CONSUME_CONSIGNMENT'])) {
	$input_error = 0;

	$consume_id = (int)get_post('consume_consignment_id');
	if ($consume_id <= 0) {
		display_error(_('You must select a consignment record to consume.'));
		$input_error = 1;
	}

	$consume_qty = input_num('consume_qty', 0);
	if ($consume_qty <= 0) {
		display_error(_('Consume quantity must be greater than zero.'));
		$input_error = 1;
	}

	if ($input_error == 0) {
		$result = consume_consignment_stock(
			$consume_id,
			$consume_qty,
			$_POST['consume_date'],
			get_post('consume_memo')
		);

		if ($result) {
			display_notification(sprintf(
				_('Consumed %s units from consignment #%s. AP Invoice #%s created for %s.'),
				number_format2($consume_qty, user_qty_dec()),
				$consume_id,
				$result['trans_no'],
				price_format($result['amount'])
			));
			$current_tab = 'consume';
			unset($_POST['consume_consignment_id'], $_POST['consume_qty'], $_POST['consume_memo']);
		}
	}
}

// =====================================================================
// HANDLE ACTIONS — RETURN
// =====================================================================

if (isset($_POST['RETURN_CONSIGNMENT'])) {
	$input_error = 0;

	$return_id = (int)get_post('return_consignment_id');
	if ($return_id <= 0) {
		display_error(_('You must select a consignment record to return.'));
		$input_error = 1;
	}

	$return_qty = input_num('return_qty', 0);
	if ($return_qty <= 0) {
		display_error(_('Return quantity must be greater than zero.'));
		$input_error = 1;
	}

	if ($input_error == 0) {
		$success = return_consignment_stock(
			$return_id,
			$return_qty,
			$_POST['return_date'],
			get_post('return_memo')
		);

		if ($success) {
			display_notification(sprintf(
				_('Returned %s units from consignment #%s to vendor.'),
				number_format2($return_qty, user_qty_dec()),
				$return_id
			));
			$current_tab = 'return';
			unset($_POST['return_consignment_id'], $_POST['return_qty'], $_POST['return_memo']);
		}
	}
}

// =====================================================================
// HANDLE ACTIONS — VMI LEVEL SAVE
// =====================================================================

if (isset($_POST['SAVE_VMI'])) {
	$input_error = 0;

	if (empty($_POST['vmi_stock_id'])) {
		display_error(_('You must select an item.'));
		$input_error = 1;
	}

	if (empty($_POST['vmi_supplier_id'])) {
		display_error(_('You must select a supplier.'));
		$input_error = 1;
	}

	$vmi_min = input_num('vmi_min_level', 0);
	$vmi_max = input_num('vmi_max_level', 0);
	$vmi_reorder = input_num('vmi_reorder_qty', 0);

	if ($vmi_min < 0 || $vmi_max < 0) {
		display_error(_('Level values cannot be negative.'));
		$input_error = 1;
	}

	if ($vmi_max > 0 && $vmi_min > $vmi_max) {
		display_error(_('Minimum level cannot exceed maximum level.'));
		$input_error = 1;
	}

	if ($input_error == 0) {
		set_vmi_level(
			$_POST['vmi_stock_id'],
			$_POST['vmi_supplier_id'],
			$vmi_min,
			$vmi_max,
			$vmi_reorder,
			get_post('vmi_loc_code')
		);

		display_notification(_('VMI level has been saved.'));
		$current_tab = 'vmi';
		unset($_POST['vmi_stock_id'], $_POST['vmi_supplier_id'], $_POST['vmi_min_level'],
			$_POST['vmi_max_level'], $_POST['vmi_reorder_qty']);
	}
}

// =====================================================================
// HANDLE ACTIONS — DELETE VMI LEVEL
// =====================================================================

if (isset($_GET['delete_vmi'])) {
	$vmi_id = (int)$_GET['delete_vmi'];
	delete_vmi_level($vmi_id);
	display_notification(_('VMI level has been deleted.'));
	$current_tab = 'vmi';
}

// =====================================================================
// HANDLE ACTIONS — GENERATE INVOICE
// =====================================================================

if (isset($_POST['GENERATE_INVOICE'])) {
	$input_error = 0;

	if (empty($_POST['inv_supplier_id'])) {
		display_error(_('You must select a supplier.'));
		$input_error = 1;
	}

	if ($input_error == 0) {
		$inv_result = generate_consignment_invoice(
			$_POST['inv_supplier_id'],
			$_POST['inv_from_date'],
			$_POST['inv_to_date']
		);

		if ($inv_result) {
			display_notification(sprintf(
				_('Consignment invoice #%s created for %s — %s lines, total %s'),
				$inv_result['trans_no'],
				get_supplier_name($inv_result['supplier_id']),
				$inv_result['line_count'],
				price_format($inv_result['total'])
			));
		} else {
			display_warning(_('No uninvoiced consumption found for this supplier in the selected period.'));
		}
		$current_tab = 'history';
	}
}

// =====================================================================
// HANDLE ACTIONS — VMI EXPORT
// =====================================================================

if (isset($_POST['EXPORT_VMI'])) {
	if (!empty($_POST['export_supplier_id'])) {
		$export_data = export_vmi_stock_levels($_POST['export_supplier_id']);
		$supplier_name = get_supplier_name($_POST['export_supplier_id']);

		if (!empty($export_data)) {
			// Generate CSV output
			header('Content-Type: text/csv; charset=utf-8');
			header('Content-Disposition: attachment; filename="vmi_stock_levels_' . date('Ymd') . '.csv"');
			$out = fopen('php://output', 'w');
			fputcsv($out, array('Item Code', 'Item Name', 'Unit', 'Min Level', 'Max Level',
				'Reorder Qty', 'Location', 'Current On Hand', 'Suggested Replenish', 'Status'));
			foreach ($export_data as $row) {
				fputcsv($out, array(
					$row['stock_id'], $row['item_name'], $row['units'],
					$row['min_level'], $row['max_level'], $row['reorder_qty'],
					$row['loc_code'], $row['current_on_hand'],
					$row['suggested_replenish'], $row['status']
				));
			}
			fclose($out);
			exit;
		} else {
			display_warning(_('No VMI levels configured for this supplier.'));
		}
	}
}

// =====================================================================
// TAB: STOCK OVERVIEW
// =====================================================================

if ($current_tab == 'overview') {

	// Summary cards
	$summary = get_consignment_summary();
	$vmi_alerts = get_vmi_alert_count();

	echo '<div style="display:flex;flex-wrap:wrap;gap:15px;margin-bottom:20px;">';

	// Card: Active Records
	echo '<div style="flex:1;min-width:180px;background:#fff;border:1px solid #dee2e6;border-radius:6px;padding:15px;text-align:center;">';
	echo '<div style="font-size:28px;font-weight:700;color:#28a745;">' . (int)$summary['total_records'] . '</div>';
	echo '<div style="color:#666;font-size:12px;">' . _('Active Consignments') . '</div>';
	echo '</div>';

	// Card: Total On-Hand Value
	echo '<div style="flex:1;min-width:180px;background:#fff;border:1px solid #dee2e6;border-radius:6px;padding:15px;text-align:center;">';
	echo '<div style="font-size:28px;font-weight:700;color:#007bff;">' . price_format($summary['total_on_hand_value']) . '</div>';
	echo '<div style="color:#666;font-size:12px;">' . _('On-Hand Value') . '</div>';
	echo '</div>';

	// Card: Total On-Hand Qty
	echo '<div style="flex:1;min-width:180px;background:#fff;border:1px solid #dee2e6;border-radius:6px;padding:15px;text-align:center;">';
	echo '<div style="font-size:28px;font-weight:700;color:#17a2b8;">' . number_format2($summary['total_on_hand_qty'], user_qty_dec()) . '</div>';
	echo '<div style="color:#666;font-size:12px;">' . _('Total Qty On Hand') . '</div>';
	echo '</div>';

	// Card: Suppliers
	echo '<div style="flex:1;min-width:180px;background:#fff;border:1px solid #dee2e6;border-radius:6px;padding:15px;text-align:center;">';
	echo '<div style="font-size:28px;font-weight:700;color:#6f42c1;">' . (int)$summary['supplier_count'] . '</div>';
	echo '<div style="color:#666;font-size:12px;">' . _('Suppliers') . '</div>';
	echo '</div>';

	// Card: VMI Alerts
	$alert_color = $vmi_alerts > 0 ? '#dc3545' : '#28a745';
	echo '<div style="flex:1;min-width:180px;background:#fff;border:1px solid #dee2e6;border-radius:6px;padding:15px;text-align:center;">';
	echo '<div style="font-size:28px;font-weight:700;color:' . $alert_color . ';">' . (int)$vmi_alerts . '</div>';
	echo '<div style="color:#666;font-size:12px;">' . _('VMI Alerts') . '</div>';
	echo '</div>';

	echo '</div>';

	// Supplier Balance Summary
	echo '<h3>' . _('Consignment Balance by Supplier') . '</h3>';
	$balances = get_consignment_balance_by_supplier();

	start_table(TABLESTYLE, "width='80%'");
	$th = array(_('Supplier'), _('Items'), _('Qty On Hand'), _('Value On Hand'),
		_('Qty Consumed'), _('Value Consumed'));
	table_header($th);

	$k = 0;
	$has_data = false;
	while ($row = db_fetch($balances)) {
		$has_data = true;
		alt_table_row_color($k);
		label_cell($row['supplier_name']);
		label_cell($row['item_count'], 'align="right"');
		qty_cell($row['total_on_hand']);
		amount_cell($row['total_value_on_hand']);
		qty_cell($row['total_consumed']);
		amount_cell($row['total_value_consumed']);
		end_row();
	}
	if (!$has_data) {
		label_row('', _('No consignment stock records found.'), 'colspan="6" align="center"');
	}
	end_table(1);

	// Filter bar for detailed list
	start_form();
	start_table(TABLESTYLE_NOBORDER);
	start_row();
	stock_items_list_cells(_('Item:'), 'filter_stock_id', null, _('All Items'), true);
	supplier_list_cells(_('Supplier:'), 'filter_supplier_id', null, _('All Suppliers'), true);
	locations_list_cells(_('Location:'), 'filter_loc_code', null, _('All Locations'), true);

	$status_options = array('' => _('All'), 'on_hand' => _('On Hand'), 'consumed' => _('Consumed'), 'returned' => _('Returned'));
	echo '<td class="label">' . _('Status:') . '</td><td>';
	echo array_selector('filter_status', get_post('filter_status'), $status_options,
		array('select_submit' => true));
	echo '</td>';

	check_cells(_('Show zero:'), 'filter_show_zero', null, true);
	end_row();
	end_table();

	// Consignment stock list
	$filters = array(
		'stock_id'   => get_post('filter_stock_id'),
		'supplier_id'=> get_post('filter_supplier_id'),
		'loc_code'   => get_post('filter_loc_code'),
		'status'     => get_post('filter_status'),
		'show_zero'  => check_value('filter_show_zero'),
	);

	div_start('consignment_list');

	$result = get_consignment_stocks($filters);

	start_table(TABLESTYLE, "width='95%'");
	$th = array(_('#'), _('Item'), _('Supplier'), _('Location'), _('Qty On Hand'),
		_('Qty Consumed'), _('Unit Cost'), _('Total Value'), _('Receipt Date'),
		_('Status'), _('Batch'), _('Memo'));
	table_header($th);

	$k = 0;
	$has_data = false;
	while ($row = db_fetch($result)) {
		$has_data = true;
		alt_table_row_color($k);
		label_cell($row['id']);
		label_cell($row['stock_id'] . ' - ' . $row['item_name']);
		label_cell($row['supplier_name']);
		label_cell($row['loc_code']);
		qty_cell($row['qty_on_hand']);
		qty_cell($row['qty_consumed']);
		amount_cell($row['unit_cost']);
		amount_cell($row['total_value']);
		label_cell(sql2date($row['receipt_date']));
		label_cell(consignment_status_badge($row['status']));
		label_cell($row['batch_id'] ? $row['batch_id'] : '-');
		label_cell($row['memo'] ? $row['memo'] : '-');
		end_row();
	}
	if (!$has_data) {
		label_row('', _('No consignment stock records found.'), 'colspan="12" align="center"');
	}
	end_table(1);

	div_end();
	end_form();
}

// =====================================================================
// TAB: RECEIVE CONSIGNMENT
// =====================================================================

if ($current_tab == 'receive') {
	echo '<h3>' . _('Receive Consignment Stock') . '</h3>';
	echo '<p style="color:#666;">' . _('Record vendor-owned stock received into your warehouse. This stock remains the property of the supplier until consumed.') . '</p>';

	start_form();

	start_table(TABLESTYLE2);

	echo '<tr><td class="label">' . _('Item:') . '</td>';
	stock_items_list_cells(null, 'recv_stock_id', null, false, false);
	echo '</tr>';

	echo '<tr><td class="label">' . _('Supplier:') . '</td>';
	supplier_list_cells(null, 'recv_supplier_id', null, false, false);
	echo '</tr>';

	small_amount_row(_('Quantity:'), 'recv_qty', null, null, null, user_qty_dec());
	small_amount_row(_('Unit Cost:'), 'recv_unit_cost', null, null, null, user_price_dec());
	date_row(_('Receipt Date:'), 'recv_date', '', true);
	locations_list_row(_('Warehouse:'), 'recv_loc_code', null, false, true);

	text_row(_('Memo / Reference:'), 'recv_memo', null, 40, 255);

	end_table(1);

	submit_center('ADD_CONSIGNMENT', _('Receive Consignment Stock'), true, '', 'default');

	end_form();

	// Recent receipts
	echo '<h3>' . _('Recent Consignment Receipts') . '</h3>';
	$recent = get_consignment_stocks(array('status' => 'on_hand'), 10);

	start_table(TABLESTYLE, "width='80%'");
	$th = array(_('#'), _('Item'), _('Supplier'), _('Location'), _('Qty'),
		_('Unit Cost'), _('Value'), _('Receipt Date'));
	table_header($th);

	$k = 0;
	$has_data = false;
	while ($row = db_fetch($recent)) {
		$has_data = true;
		alt_table_row_color($k);
		label_cell($row['id']);
		label_cell($row['stock_id'] . ' - ' . $row['item_name']);
		label_cell($row['supplier_name']);
		label_cell($row['loc_code']);
		qty_cell($row['qty_on_hand']);
		amount_cell($row['unit_cost']);
		amount_cell($row['total_value']);
		label_cell(sql2date($row['receipt_date']));
		end_row();
	}
	if (!$has_data) {
		label_row('', _('No consignment stock on hand.'), 'colspan="8" align="center"');
	}
	end_table(1);
}

// =====================================================================
// TAB: CONSUME CONSIGNMENT
// =====================================================================

if ($current_tab == 'consume') {
	echo '<h3>' . _('Consume Consignment Stock') . '</h3>';
	echo '<p style="color:#666;">' . _('Convert vendor-owned stock to company-owned. This creates a GL posting (Debit: Inventory, Credit: Supplier Payable) and an AP invoice.') . '</p>';

	// Show available consignment stock to consume
	$available = get_consignment_stocks(array('status' => 'on_hand'));

	start_table(TABLESTYLE, "width='90%'");
	$th = array(_('#'), _('Item'), _('Supplier'), _('Location'), _('Qty On Hand'),
		_('Unit Cost'), _('Value'), _('Receipt Date'), '');
	table_header($th);

	$k = 0;
	$has_data = false;
	while ($row = db_fetch($available)) {
		$has_data = true;
		alt_table_row_color($k);
		label_cell($row['id']);
		label_cell($row['stock_id'] . ' - ' . $row['item_name']);
		label_cell($row['supplier_name']);
		label_cell($row['loc_code']);
		qty_cell($row['qty_on_hand']);
		amount_cell($row['unit_cost']);
		amount_cell($row['total_value']);
		label_cell(sql2date($row['receipt_date']));
		// Select button
		echo '<td>';
		echo '<a href="?tab=consume&select_consume=' . $row['id'] . '" class="button">'
			. _('Select') . '</a>';
		echo '</td>';
		end_row();
	}
	if (!$has_data) {
		label_row('', _('No consignment stock available to consume.'), 'colspan="9" align="center"');
	}
	end_table(1);

	// Consume form (shown when a record is selected)
	$selected_id = isset($_GET['select_consume']) ? (int)$_GET['select_consume'] : 0;
	if ($selected_id > 0) {
		$selected = get_consignment_record($selected_id);
		if ($selected && $selected['qty_on_hand'] > 0) {
			echo '<hr>';
			echo '<h3>' . sprintf(_('Consume from Consignment #%s'), $selected_id) . '</h3>';
			echo '<p><strong>' . _('Item:') . '</strong> ' . $selected['stock_id'] . ' - ' . $selected['item_name']
				. ' | <strong>' . _('Supplier:') . '</strong> ' . $selected['supplier_name']
				. ' | <strong>' . _('On Hand:') . '</strong> ' . number_format2($selected['qty_on_hand'], user_qty_dec())
				. ' | <strong>' . _('Unit Cost:') . '</strong> ' . price_format($selected['unit_cost'])
				. '</p>';

			start_form();
			hidden('consume_consignment_id', $selected_id);

			start_table(TABLESTYLE2);
			small_amount_row(_('Quantity to Consume:'), 'consume_qty', null, null, null, user_qty_dec());
			date_row(_('Consumption Date:'), 'consume_date', '', true);
			text_row(_('Memo:'), 'consume_memo', null, 40, 255);
			end_table(1);

			submit_center('CONSUME_CONSIGNMENT', _('Consume — Create GL & AP Entry'), true, '', 'default');
			end_form();
		}
	}
}

// =====================================================================
// TAB: RETURN TO VENDOR
// =====================================================================

if ($current_tab == 'return') {
	echo '<h3>' . _('Return Consignment Stock to Vendor') . '</h3>';
	echo '<p style="color:#666;">' . _('Return unsold/unused vendor-owned stock. No GL impact — the stock was never owned by your company.') . '</p>';

	// Show available consignment stock to return
	$available = get_consignment_stocks(array('status' => 'on_hand'));

	start_table(TABLESTYLE, "width='90%'");
	$th = array(_('#'), _('Item'), _('Supplier'), _('Location'), _('Qty On Hand'),
		_('Unit Cost'), _('Value'), _('Receipt Date'), '');
	table_header($th);

	$k = 0;
	$has_data = false;
	while ($row = db_fetch($available)) {
		$has_data = true;
		alt_table_row_color($k);
		label_cell($row['id']);
		label_cell($row['stock_id'] . ' - ' . $row['item_name']);
		label_cell($row['supplier_name']);
		label_cell($row['loc_code']);
		qty_cell($row['qty_on_hand']);
		amount_cell($row['unit_cost']);
		amount_cell($row['total_value']);
		label_cell(sql2date($row['receipt_date']));
		echo '<td>';
		echo '<a href="?tab=return&select_return=' . $row['id'] . '" class="button">'
			. _('Select') . '</a>';
		echo '</td>';
		end_row();
	}
	if (!$has_data) {
		label_row('', _('No consignment stock available to return.'), 'colspan="9" align="center"');
	}
	end_table(1);

	// Return form (shown when a record is selected)
	$selected_id = isset($_GET['select_return']) ? (int)$_GET['select_return'] : 0;
	if ($selected_id > 0) {
		$selected = get_consignment_record($selected_id);
		if ($selected && $selected['qty_on_hand'] > 0) {
			echo '<hr>';
			echo '<h3>' . sprintf(_('Return from Consignment #%s'), $selected_id) . '</h3>';
			echo '<p><strong>' . _('Item:') . '</strong> ' . $selected['stock_id'] . ' - ' . $selected['item_name']
				. ' | <strong>' . _('Supplier:') . '</strong> ' . $selected['supplier_name']
				. ' | <strong>' . _('On Hand:') . '</strong> ' . number_format2($selected['qty_on_hand'], user_qty_dec())
				. '</p>';

			start_form();
			hidden('return_consignment_id', $selected_id);

			start_table(TABLESTYLE2);
			small_amount_row(_('Quantity to Return:'), 'return_qty', null, null, null, user_qty_dec());
			date_row(_('Return Date:'), 'return_date', '', true);
			text_row(_('Return Reason / Memo:'), 'return_memo', null, 40, 255);
			end_table(1);

			submit_center('RETURN_CONSIGNMENT', _('Return to Vendor'), true, '', 'default');
			end_form();
		}
	}
}

// =====================================================================
// TAB: VMI SETTINGS
// =====================================================================

if ($current_tab == 'vmi') {
	echo '<h3>' . _('Vendor-Managed Inventory (VMI) Levels') . '</h3>';
	echo '<p style="color:#666;">' . _('Set min/max stock levels per item-supplier pair. The vendor monitors these levels and replenishes when stock falls below minimum.') . '</p>';

	// VMI Alerts
	$alerts_result = get_vmi_alerts();
	$has_alerts = false;

	echo '<div style="margin-bottom:20px;">';
	echo '<h4 style="color:#dc3545;"><i class="fa fa-exclamation-triangle"></i> ' . _('VMI Alerts — Below Minimum') . '</h4>';

	start_table(TABLESTYLE, "width='80%'");
	$th = array(_('Item'), _('Supplier'), _('Min Level'), _('Current On Hand'), _('Deficit'), _('Reorder Qty'));
	table_header($th);

	$k = 0;
	while ($row = db_fetch($alerts_result)) {
		$has_alerts = true;
		alt_table_row_color($k);
		label_cell($row['stock_id'] . ' - ' . $row['item_name']);
		label_cell($row['supplier_name']);
		qty_cell($row['min_level']);
		qty_cell($row['current_on_hand']);
		echo '<td align="right" style="color:#dc3545;font-weight:600;">'
			. number_format2($row['deficit'], user_qty_dec()) . '</td>';
		qty_cell($row['reorder_qty']);
		end_row();
	}
	if (!$has_alerts) {
		label_row('', _('No VMI alerts — all items are above minimum levels.'),
			'colspan="6" align="center" style="color:#28a745;"');
	}
	end_table(1);
	echo '</div>';

	// VMI Levels List
	echo '<h4>' . _('Configured VMI Levels') . '</h4>';

	$vmi_levels = get_vmi_levels();

	start_table(TABLESTYLE, "width='90%'");
	$th = array(_('Item'), _('Supplier'), _('Min Level'), _('Max Level'),
		_('Reorder Qty'), _('Location'), _('Current On Hand'), _('Status'), '');
	table_header($th);

	$k = 0;
	$has_vmi = false;
	while ($row = db_fetch($vmi_levels)) {
		$has_vmi = true;
		alt_table_row_color($k);
		label_cell($row['stock_id'] . ' - ' . $row['item_name']);
		label_cell($row['supplier_name']);
		qty_cell($row['min_level']);
		qty_cell($row['max_level']);
		qty_cell($row['reorder_qty']);
		label_cell($row['loc_code'] ? $row['loc_code'] : '-');
		qty_cell($row['current_on_hand']);

		// Status badge
		$on_hand = (float)$row['current_on_hand'];
		$min = (float)$row['min_level'];
		$max = (float)$row['max_level'];
		if ($on_hand < $min) {
			$badge = '<span style="padding:2px 6px;border-radius:3px;background:#dc3545;color:#fff;font-size:11px;">BELOW MIN</span>';
		} elseif ($max > 0 && $on_hand > $max) {
			$badge = '<span style="padding:2px 6px;border-radius:3px;background:#ffc107;color:#000;font-size:11px;">ABOVE MAX</span>';
		} else {
			$badge = '<span style="padding:2px 6px;border-radius:3px;background:#28a745;color:#fff;font-size:11px;">OK</span>';
		}
		label_cell($badge);

		// Delete link
		echo '<td><a href="?tab=vmi&delete_vmi=' . $row['id']
			. '" onclick="return confirm(\'' . _('Delete this VMI level?') . '\');">'
			. _('Delete') . '</a></td>';
		end_row();
	}
	if (!$has_vmi) {
		label_row('', _('No VMI levels configured yet.'), 'colspan="9" align="center"');
	}
	end_table(1);

	// Add VMI Level form
	echo '<h4>' . _('Add / Update VMI Level') . '</h4>';

	start_form();
	start_table(TABLESTYLE2);

	echo '<tr><td class="label">' . _('Item:') . '</td>';
	stock_items_list_cells(null, 'vmi_stock_id', null, false, false);
	echo '</tr>';

	echo '<tr><td class="label">' . _('Supplier:') . '</td>';
	supplier_list_cells(null, 'vmi_supplier_id', null, false, false);
	echo '</tr>';
	small_amount_row(_('Minimum Level:'), 'vmi_min_level', null, null, null, user_qty_dec());
	small_amount_row(_('Maximum Level:'), 'vmi_max_level', null, null, null, user_qty_dec());
	small_amount_row(_('Reorder Quantity:'), 'vmi_reorder_qty', null, null, null, user_qty_dec());
	locations_list_row(_('Location:'), 'vmi_loc_code', null, _('Any'), false);

	end_table(1);
	submit_center('SAVE_VMI', _('Save VMI Level'), true, '', 'default');
	end_form();

	// VMI Export
	echo '<hr>';
	echo '<h4>' . _('Export Stock Levels for Vendor') . '</h4>';
	echo '<p style="color:#666;">' . _('Export current stock levels as CSV to share with the vendor for VMI replenishment planning.') . '</p>';

	start_form();
	start_table(TABLESTYLE2);
	echo '<tr><td class="label">' . _('Supplier:') . '</td>';
	supplier_list_cells(null, 'export_supplier_id', null, false, false);
	echo '</tr>';
	end_table(1);
	submit_center('EXPORT_VMI', _('Export CSV'), true, '', 'default');
	end_form();
}

// =====================================================================
// TAB: HISTORY
// =====================================================================

if ($current_tab == 'history') {
	echo '<h3>' . _('Consignment History & Invoicing') . '</h3>';

	// Generate invoice section
	echo '<div style="background:#f8f9fa;border:1px solid #dee2e6;border-radius:6px;padding:15px;margin-bottom:20px;">';
	echo '<h4>' . _('Generate Consignment Invoice') . '</h4>';
	echo '<p style="color:#666;margin-bottom:10px;">' . _('Create a consolidated AP invoice for all consumed consignment stock in a date range.') . '</p>';

	start_form();
	start_table(TABLESTYLE_NOBORDER);
	start_row();
	echo '<td class="label">' . _('Supplier:') . '</td>';
	supplier_list_cells(null, 'inv_supplier_id', null, false, false);
	date_cells(_('From:'), 'inv_from_date', '', true);
	date_cells(_('To:'), 'inv_to_date', '', true);
	submit_cells('GENERATE_INVOICE', _('Generate Invoice'), '', '', 'default');
	end_row();
	end_table();
	end_form();
	echo '</div>';

	// Movement history
	echo '<h4>' . _('Movement History') . '</h4>';

	$movements = get_consignment_movements(0, 50);

	start_table(TABLESTYLE, "width='95%'");
	$th = array(_('Date'), _('Consignment #'), _('Item'), _('Supplier'),
		_('Type'), _('Qty'), _('Description'), _('Invoice'), _('User'));
	table_header($th);

	$k = 0;
	$has_data = false;
	while ($row = db_fetch($movements)) {
		$has_data = true;
		alt_table_row_color($k);
		label_cell(sql2date($row['movement_date']));
		label_cell($row['consignment_id']);
		label_cell($row['stock_id'] . ' - ' . $row['item_name']);
		label_cell($row['supplier_name']);

		// Type badge
		$type_colors = array(
			'receive' => '#28a745',
			'consume' => '#007bff',
			'return'  => '#6c757d',
			'adjust'  => '#ffc107',
		);
		$type_color = isset($type_colors[$row['movement_type']]) ? $type_colors[$row['movement_type']] : '#999';
		label_cell('<span style="padding:2px 6px;border-radius:3px;background:'
			. $type_color . ';color:#fff;font-size:11px;">' . ucfirst($row['movement_type']) . '</span>');

		qty_cell($row['qty']);
		label_cell($row['description']);
		label_cell($row['invoiced'] ? sprintf(_('Inv #%s'), $row['invoice_trans_no']) : '-');
		label_cell($row['created_by']);
		end_row();
	}
	if (!$has_data) {
		label_row('', _('No consignment movements recorded yet.'), 'colspan="9" align="center"');
	}
	end_table(1);
}

// =====================================================================
// END PAGE
// =====================================================================

end_page();
