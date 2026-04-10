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
 * Reusable UI components for serial number and batch/lot selection,
 * entry grids, and display helpers.
 *
 * Used by: serial_numbers.php, serial_inquiry.php, GRN forms, delivery forms.
 */

/**
 * Render a serial number status badge with color.
 *
 * @param string $status Status code
 * @return string HTML badge
 */
function serial_status_badge($status) {
	include_once(dirname(__FILE__) . '/db/serial_numbers_db.inc');
	$label = get_serial_status_label($status);
	$color = get_serial_status_color($status);
	return '<span style="display:inline-block;padding:2px 8px;border-radius:3px;font-size:11px;'
		. 'color:#fff;background-color:' . $color . ';">' . $label . '</span>';
}

/**
 * Render a serial status selector dropdown.
 *
 * @param string $name      Field name
 * @param string $selected  Currently selected status
 * @param bool   $all_option Whether to include an "All" option
 * @param bool   $submit_on_change Submit form on change
 */
function serial_status_list($name, $selected = '', $all_option = true, $submit_on_change = false) {
	include_once(dirname(__FILE__) . '/db/serial_numbers_db.inc');
	$statuses = get_serial_statuses();


	if ($all_option)
		$options = array('' => _('All Statuses'));
	else
		$options = array();

	$options = array_merge($options, $statuses);

	echo array_selector($name, $selected, $options,
		array('select_submit' => $submit_on_change));
}

/**
 * Render serial status list in a table cell.
 *
 * @param string $label         Cell label
 * @param string $name          Field name
 * @param string $selected      Current value
 * @param bool   $all_option    Include "All" option
 * @param bool   $submit_on_change Submit on change
 */
function serial_status_list_cells($label, $name, $selected = '', $all_option = true, $submit_on_change = false) {
	if ($label != null)
		echo "<td>$label</td>\n";
	echo "<td>";
	serial_status_list($name, $selected, $all_option, $submit_on_change);
	echo "</td>\n";
}

/**
 * Render serial status list as a table row.
 *
 * @param string $label         Row label
 * @param string $name          Field name
 * @param string $selected      Current value
 * @param bool   $all_option    Include "All" option
 * @param bool   $submit_on_change Submit on change
 */
function serial_status_list_row($label, $name, $selected = '', $all_option = true, $submit_on_change = false) {
	echo "<tr><td class='label'>$label</td>";
	serial_status_list_cells(null, $name, $selected, $all_option, $submit_on_change);
	echo "</tr>\n";
}

/**
 * Render a serial-tracked items selector (only items with serial tracking enabled).
 *
 * @param string $name          Field name
 * @param string $selected_id   Current value
 * @param bool   $all_option    Include "All Items" option
 * @param bool   $submit_on_change Submit on change
 * @return string HTML
 */
function serial_tracked_items_list($name, $selected_id = null, $all_option = true, $submit_on_change = false) {
	$sql = "SELECT stock_id, CONCAT(stock_id, ' - ', description) AS name, inactive
		FROM " . TB_PREF . "stock_master
		WHERE (track_by='serial' OR track_by='serial_batch')
		ORDER BY description";

	return combo_input($name, $selected_id, $sql, 'stock_id', 'name',
		array(
			'spec_option' => $all_option ? _('All Serial-Tracked Items') : false,
			'spec_id' => '',
			'select_submit' => $submit_on_change,
			'order' => false,
		));
}

/**
 * Render serial-tracked items list in table cells.
 *
 * @param string $label         Cell label
 * @param string $name          Field name
 * @param string $selected_id   Current value
 * @param bool   $all_option    Include "All" option
 * @param bool   $submit_on_change Submit on change
 */
function serial_tracked_items_list_cells($label, $name, $selected_id = null, $all_option = true, $submit_on_change = false) {
	if ($label != null)
		echo "<td>$label</td>\n";
	echo "<td>";
	echo serial_tracked_items_list($name, $selected_id, $all_option, $submit_on_change);
	echo "</td>\n";
}

/**
 * Display a serial number detail card (used in inquiry/view pages).
 *
 * @param array $serial Serial number record from get_serial_number()
 */
function display_serial_detail_card($serial) {
	start_table(TABLESTYLE2, "width='80%'");

	label_row(_('Serial Number:'), '<strong>' . $serial['serial_no'] . '</strong>');
	label_row(_('Item:'), $serial['stock_id'] . ' - ' . $serial['item_description']);
	label_row(_('Status:'), serial_status_badge($serial['status']));
	label_row(_('Location:'), $serial['location_name'] ? $serial['location_name'] . ' (' . $serial['loc_code'] . ')' : '-');

	if ($serial['supplier_name'])
		label_row(_('Supplier:'), $serial['supplier_name']);
	if ($serial['customer_name'])
		label_row(_('Customer:'), $serial['customer_name']);

	if ($serial['purchase_date'])
		label_row(_('Purchase Date:'), sql2date($serial['purchase_date']));
	if ($serial['delivery_date'])
		label_row(_('Delivery Date:'), sql2date($serial['delivery_date']));
	if ($serial['manufacturing_date'])
		label_row(_('Manufacturing Date:'), sql2date($serial['manufacturing_date']));
	if ($serial['expiry_date'])
		label_row(_('Expiry Date:'), sql2date($serial['expiry_date']));

	if ($serial['warranty_start'] || $serial['warranty_end']) {
		$warranty = '';
		if ($serial['warranty_start'])
			$warranty .= sql2date($serial['warranty_start']);
		if ($serial['warranty_end']) {
			$warranty .= ' — ' . sql2date($serial['warranty_end']);
			if ($serial['warranty_end'] < date('Y-m-d'))
				$warranty .= ' <span style="color:red;">(' . _('Expired') . ')</span>';
			else
				$warranty .= ' <span style="color:green;">(' . _('Active') . ')</span>';
		}
		if ($serial['warranty_type'])
			$warranty .= ' [' . $serial['warranty_type'] . ']';
		label_row(_('Warranty:'), $warranty);
	}

	if ($serial['purchase_cost'] > 0)
		label_row(_('Purchase Cost:'), price_format($serial['purchase_cost']));

	if ($serial['notes'])
		label_row(_('Notes:'), nl2br(htmlspecialchars($serial['notes'])));

	label_row(_('Created:'), sql2date($serial['created_at']));

	end_table();
}

/**
 * Display the serial movement timeline table.
 *
 * @param int $serial_id Serial record id
 */
function display_serial_movement_timeline($serial_id) {
	include_once(dirname(__FILE__) . '/db/serial_numbers_db.inc');
	$result = get_serial_movements($serial_id);

	$count = count_serial_movements($serial_id);
	if ($count == 0) {
		display_note(_('No movement history recorded for this serial number.'));
		return;
	}

	start_table(TABLESTYLE, "width='95%'");
	$th = array(_('Date'), _('Type'), _('Trans #'), _('Reference'), _('From'), _('To'),
		_('Status Change'), _('Notes'), _('By'));
	table_header($th);

	$k = 0;
	while ($row = db_fetch($result)) {
		alt_table_row_color($k);

		label_cell(sql2date($row['tran_date']));

		// Transaction type name
		if ($row['trans_type'] > 0 && isset($GLOBALS['systypes_array'][$row['trans_type']]))
			label_cell($GLOBALS['systypes_array'][$row['trans_type']]);
		else
			label_cell($row['trans_type'] > 0 ? '#' . $row['trans_type'] : _('Manual'));

		// Transaction number with link
		if ($row['trans_type'] > 0 && $row['trans_no'] > 0)
			label_cell(get_trans_view_str($row['trans_type'], $row['trans_no']));
		else
			label_cell('-');

		label_cell($row['reference'] ? $row['reference'] : '-');
		label_cell($row['from_location_name'] ? $row['from_location_name'] : '-');
		label_cell($row['to_location_name'] ? $row['to_location_name'] : '-');

		// Status change
		$status_change = '';
		if ($row['from_status'] || $row['to_status']) {
			if ($row['from_status'])
				$status_change .= serial_status_badge($row['from_status']);
			$status_change .= ' &rarr; ';
			if ($row['to_status'])
				$status_change .= serial_status_badge($row['to_status']);
		} else {
			$status_change = '-';
		}
		label_cell($status_change);

		label_cell($row['notes'] ? $row['notes'] : '-');

		// Created by user
		if ($row['created_by']) {
			$user_sql = "SELECT real_name FROM " . TB_PREF . "users WHERE id=" . (int)$row['created_by'];
			$user_res = db_query($user_sql, 'could not get user');
			$user_row = db_fetch($user_res);
			label_cell($user_row ? $user_row['real_name'] : '#' . $row['created_by']);
		} else {
			label_cell('-');
		}

		end_row();
	}

	end_table();
}

//======================================================================
//  B A T C H / L O T   U I   C O M P O N E N T S
//======================================================================

/**
 * Render a batch status badge with color.
 *
 * @param string $status Status code
 * @return string HTML badge
 */
function batch_status_badge($status) {
	include_once(dirname(__FILE__) . '/db/stock_batches_db.inc');
	$label = get_batch_status_label($status);
	$color = get_batch_status_color($status);
	return '<span style="display:inline-block;padding:2px 8px;border-radius:3px;font-size:11px;'
		. 'color:#fff;background-color:' . $color . ';">' . $label . '</span>';
}

/**
 * Render an expiry badge with color-coded urgency.
 *
 * @param string|null $expiry_date SQL date string (Y-m-d)
 * @return string HTML badge
 */
function batch_expiry_badge($expiry_date) {
	include_once(dirname(__FILE__) . '/db/stock_batches_db.inc');
	if (!$expiry_date)
		return '<span style="color:#999;">' . _('No expiry') . '</span>';

	$color = get_batch_expiry_color($expiry_date);
	$display_date = sql2date($expiry_date);

	$today = date('Y-m-d');
	if ($expiry_date < $today) {
		$days_past = (int)((strtotime($today) - strtotime($expiry_date)) / 86400);
		$label = sprintf(_('Expired %d days ago'), $days_past);
	} else {
		$days_left = (int)((strtotime($expiry_date) - strtotime($today)) / 86400);
		if ($days_left == 0)
			$label = _('Expires today');
		elseif ($days_left == 1)
			$label = _('Expires tomorrow');
		else
			$label = sprintf(_('%d days left'), $days_left);
	}

	return '<span style="display:inline-block;padding:2px 8px;border-radius:3px;font-size:11px;'
		. 'color:#fff;background-color:' . $color . ';" title="' . htmlspecialchars($label) . '">'
		. $display_date . '</span>';
}

/**
 * Render a batch status selector dropdown.
 *
 * @param string $name      Field name
 * @param string $selected  Currently selected status
 * @param bool   $all_option Whether to include an "All" option
 * @param bool   $submit_on_change Submit form on change
 */
function batch_status_list($name, $selected = '', $all_option = true, $submit_on_change = false) {
	include_once(dirname(__FILE__) . '/db/stock_batches_db.inc');
	$statuses = get_batch_statuses();

	if ($all_option)
		$options = array('' => _('All Statuses'));
	else
		$options = array();

	$options = array_merge($options, $statuses);

	echo array_selector($name, $selected, $options,
		array('select_submit' => $submit_on_change));
}

/**
 * Render batch status list in a table cell.
 *
 * @param string $label         Cell label
 * @param string $name          Field name
 * @param string $selected      Current value
 * @param bool   $all_option    Include "All" option
 * @param bool   $submit_on_change Submit on change
 */
function batch_status_list_cells($label, $name, $selected = '', $all_option = true, $submit_on_change = false) {
	if ($label != null)
		echo "<td>$label</td>\n";
	echo "<td>";
	batch_status_list($name, $selected, $all_option, $submit_on_change);
	echo "</td>\n";
}

/**
 * Render batch status list as a table row.
 *
 * @param string $label         Row label
 * @param string $name          Field name
 * @param string $selected      Current value
 * @param bool   $all_option    Include "All" option
 * @param bool   $submit_on_change Submit on change
 */
function batch_status_list_row($label, $name, $selected = '', $all_option = true, $submit_on_change = false) {
	echo "<tr><td class='label'>$label</td>";
	batch_status_list_cells(null, $name, $selected, $all_option, $submit_on_change);
	echo "</tr>\n";
}

/**
 * Render a batch-tracked items selector (only items with batch tracking enabled).
 *
 * @param string $name          Field name
 * @param string $selected_id   Current value
 * @param bool   $all_option    Include "All Items" option
 * @param bool   $submit_on_change Submit on change
 * @return string HTML
 */
function batch_tracked_items_list($name, $selected_id = null, $all_option = true, $submit_on_change = false) {
	$sql = "SELECT stock_id, CONCAT(stock_id, ' - ', description) AS name, inactive
		FROM " . TB_PREF . "stock_master
		WHERE (track_by='batch' OR track_by='serial_batch')
		ORDER BY description";

	return combo_input($name, $selected_id, $sql, 'stock_id', 'name',
		array(
			'spec_option' => $all_option ? _('All Batch-Tracked Items') : false,
			'spec_id' => '',
			'select_submit' => $submit_on_change,
			'order' => false,
		));
}

/**
 * Render batch-tracked items list in table cells.
 *
 * @param string $label         Cell label
 * @param string $name          Field name
 * @param string $selected_id   Current value
 * @param bool   $all_option    Include "All" option
 * @param bool   $submit_on_change Submit on change
 */
function batch_tracked_items_list_cells($label, $name, $selected_id = null, $all_option = true, $submit_on_change = false) {
	if ($label != null)
		echo "<td>$label</td>\n";
	echo "<td>";
	echo batch_tracked_items_list($name, $selected_id, $all_option, $submit_on_change);
	echo "</td>\n";
}

/**
 * Render an expiry filter dropdown.
 *
 * @param string $name     Field name
 * @param string $selected Current value
 * @param bool   $submit_on_change Submit on change
 */
function batch_expiry_filter_list($name, $selected = '', $submit_on_change = false) {
	$options = array(
		'' => _('All Expiry'),
		'expired' => _('Expired'),
		'expiring_soon' => _('Expiring Soon'),
		'ok' => _('OK / No Expiry'),
	);

	echo array_selector($name, $selected, $options,
		array('select_submit' => $submit_on_change));
}

/**
 * Render expiry filter in table cells.
 *
 * @param string $label Cell label
 * @param string $name  Field name
 * @param string $selected Current value
 * @param bool   $submit_on_change Submit on change
 */
function batch_expiry_filter_cells($label, $name, $selected = '', $submit_on_change = false) {
	if ($label != null)
		echo "<td>$label</td>\n";
	echo "<td>";
	batch_expiry_filter_list($name, $selected, $submit_on_change);
	echo "</td>\n";
}

/**
 * Display a batch detail card (used in inquiry/view pages).
 *
 * @param array $batch Batch record from get_stock_batch()
 */
function display_batch_detail_card($batch) {
	include_once(dirname(__FILE__) . '/db/stock_batches_db.inc');

	start_table(TABLESTYLE2, "width='80%'");

	label_row(_('Batch Number:'), '<strong>' . $batch['batch_no'] . '</strong>');
	label_row(_('Item:'), $batch['stock_id'] . ' - ' . $batch['item_description']);
	label_row(_('Status:'), batch_status_badge($batch['status']));

	if ($batch['manufacturing_date'])
		label_row(_('Manufacturing Date:'), sql2date($batch['manufacturing_date']));
	if ($batch['expiry_date'])
		label_row(_('Expiry Date:'), batch_expiry_badge($batch['expiry_date']));
	if ($batch['best_before_date'])
		label_row(_('Best Before Date:'), sql2date($batch['best_before_date']));
	if ($batch['retest_date'])
		label_row(_('Retest Date:'), sql2date($batch['retest_date']));
	if ($batch['shelf_life_days'])
		label_row(_('Shelf Life:'), sprintf(_('%d days'), $batch['shelf_life_days']));

	if ($batch['supplier_name'])
		label_row(_('Supplier:'), $batch['supplier_name']);
	if ($batch['supplier_batch_no'])
		label_row(_('Supplier Batch #:'), $batch['supplier_batch_no']);

	if ($batch['initial_qty'] > 0)
		label_row(_('Initial Quantity:'), number_format2($batch['initial_qty'], get_qty_dec($batch['stock_id'])));

	// Current quantity on hand
	$qty_on_hand = get_batch_quantity_on_hand($batch['id']);
	label_row(_('Current Qty on Hand:'), '<strong>' . number_format2($qty_on_hand, get_qty_dec($batch['stock_id'])) . '</strong>');

	if ($batch['country_of_origin'])
		label_row(_('Country of Origin:'), $batch['country_of_origin']);
	if ($batch['certification'])
		label_row(_('Certification:'), $batch['certification']);

	if ($batch['notes'])
		label_row(_('Notes:'), nl2br(htmlspecialchars($batch['notes'])));

	label_row(_('Created:'), sql2date($batch['created_at']));

	end_table();
}

/**
 * Display the batch movement timeline table.
 *
 * @param int $batch_id Batch record id
 */
function display_batch_movement_timeline($batch_id) {
	include_once(dirname(__FILE__) . '/db/stock_batches_db.inc');
	$result = get_batch_movements($batch_id);

	$count = count_batch_movements($batch_id);
	if ($count == 0) {
		display_note(_('No movement history recorded for this batch.'));
		return;
	}

	start_table(TABLESTYLE, "width='95%'");
	$th = array(_('Date'), _('Type'), _('Trans #'), _('Reference'), _('Location'),
		_('Quantity'), _('Notes'), _('By'));
	table_header($th);

	$k = 0;
	while ($row = db_fetch($result)) {
		alt_table_row_color($k);

		label_cell(sql2date($row['tran_date']));

		// Transaction type name
		if ($row['trans_type'] > 0 && isset($GLOBALS['systypes_array'][$row['trans_type']]))
			label_cell($GLOBALS['systypes_array'][$row['trans_type']]);
		else
			label_cell($row['trans_type'] > 0 ? '#' . $row['trans_type'] : _('Manual'));

		// Transaction number with link
		if ($row['trans_type'] > 0 && $row['trans_no'] > 0)
			label_cell(get_trans_view_str($row['trans_type'], $row['trans_no']));
		else
			label_cell('-');

		label_cell($row['reference'] ? $row['reference'] : '-');
		label_cell($row['location_name']);

		// Quantity with color
		$qty_str = number_format2($row['quantity'], 4);
		if ($row['quantity'] > 0)
			label_cell('<span style="color:green;">+' . $qty_str . '</span>', "align='right'");
		elseif ($row['quantity'] < 0)
			label_cell('<span style="color:red;">' . $qty_str . '</span>', "align='right'");
		else
			label_cell($qty_str, "align='right'");

		label_cell($row['notes'] ? $row['notes'] : '-');

		// Created by user
		if ($row['created_by']) {
			$user_sql = "SELECT real_name FROM " . TB_PREF . "users WHERE id=" . (int)$row['created_by'];
			$user_res = db_query($user_sql, 'could not get user');
			$user_row = db_fetch($user_res);
			label_cell($user_row ? $user_row['real_name'] : '#' . $row['created_by']);
		} else {
			label_cell('-');
		}

		end_row();
	}

	end_table();
}

/**
 * Display batch location breakdown table.
 *
 * @param int $batch_id Batch record id
 */
function display_batch_location_breakdown($batch_id) {
	include_once(dirname(__FILE__) . '/db/stock_batches_db.inc');
	$result = get_batch_location_breakdown($batch_id);

	$batch = get_stock_batch($batch_id);
	$dec = $batch ? get_qty_dec($batch['stock_id']) : 4;

	$has_rows = false;
	start_table(TABLESTYLE, "width='60%'");
	$th = array(_('Location'), _('Quantity'));
	table_header($th);

	$k = 0;
	$total = 0;
	while ($row = db_fetch($result)) {
		$has_rows = true;
		alt_table_row_color($k);
		label_cell($row['location_name']);
		qty_cell($row['qty'], false, $dec);
		end_row();
		$total += $row['qty'];
	}

	if ($has_rows) {
		// Total row
		label_row('<strong>' . _('Total') . '</strong>',
			'<strong>' . number_format2($total, $dec) . '</strong>',
			"align='right'", "align='right'");
	}

	end_table();

	if (!$has_rows)
		display_note(_('No stock in any location for this batch.'));
}
