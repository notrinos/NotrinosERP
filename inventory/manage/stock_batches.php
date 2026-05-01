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
 * Batch/Lot Management — List, Create, Edit, Bulk Generate, Import,
 * Split, Merge, Status management.
 *
 * Features:
 *   - Filterable list by item, status, expiry, search text
 *   - Create / Edit single batch
 *   - Bulk generate batch numbers for an item
 *   - Import batch numbers from text
 *   - Split a batch into a new batch
 *   - Merge multiple batches into one
 *   - Expiry auto-calculation from shelf life
 *   - Color-coded expiry indicators
 */
$page_security = 'SA_BATCHNUMBER';
$path_to_root = '../..';
include_once($path_to_root . '/includes/session.inc');

$_SESSION['page_title'] = _($help_context = 'Batch / Lot Numbers');

$js = '';
if (user_use_date_picker())
	$js .= get_js_date_picker();

page($_SESSION['page_title'], false, false, '', $js);

include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/includes/date_functions.inc');
include_once($path_to_root . '/inventory/includes/db/stock_batches_db.inc');
include_once($path_to_root . '/inventory/includes/db/serial_batch_db.inc');
include_once($path_to_root . '/inventory/includes/serial_batch_ui.inc');
include_once($path_to_root . '/inventory/includes/inventory_db.inc');

simple_page_mode(true);

//----------------------------------------------------------------------
// Handle GET parameter for pre-selected item
//----------------------------------------------------------------------
if (isset($_GET['stock_id']))
	$_POST['filter_stock_id'] = $_GET['stock_id'];

//----------------------------------------------------------------------
// Handle ADD / UPDATE
//----------------------------------------------------------------------
if ($Mode == 'ADD_ITEM' || $Mode == 'UPDATE_ITEM') {
	$input_error = 0;

	if (strlen(get_post('batch_no')) == 0) {
		$input_error = 1;
		display_error(_('The batch number must be entered.'));
		set_focus('batch_no');
	}

	if (strlen(get_post('stock_id_edit')) == 0) {
		$input_error = 1;
		display_error(_('An item must be selected.'));
		set_focus('stock_id_edit');
	}

	// Check duplicate
	if ($input_error == 0) {
		$exclude = ($selected_id != -1) ? $selected_id : 0;
		if (is_batch_number_duplicate(get_post('batch_no'), get_post('stock_id_edit'), $exclude)) {
			$input_error = 1;
			display_error(_('This batch number already exists for the selected item.'));
			set_focus('batch_no');
		}
	}

	// Validate status
	if ($input_error == 0) {
		$valid_statuses = get_batch_statuses();
		if (!isset($valid_statuses[get_post('status')])) {
			$input_error = 1;
			display_error(_('Invalid status selected.'));
			set_focus('status');
		}
	}

	if ($input_error == 0) {
		if ($selected_id != -1) {
			$old_batch = get_stock_batch($selected_id);

			update_stock_batch(
				$selected_id,
				get_post('batch_no'),
				get_post('status'),
				get_post('manufacturing_date'),
				get_post('expiry_date'),
				get_post('best_before_date'),
				get_post('retest_date'),
				get_post('shelf_life_days'),
				get_post('supplier_id', null),
				get_post('supplier_batch_no'),
				get_post('grn_id', null),
				get_post('work_order_id', null),
				get_post('initial_qty', 0),
				get_post('country_of_origin'),
				get_post('certification'),
				get_post('notes')
			);

			// Log status change if changed
			if ($old_batch && $old_batch['status'] !== get_post('status')) {
				add_batch_movement(
					$selected_id, 0, 0,
					'', null, 0,
					Today(), '', sprintf(_('Status changed from %s to %s'),
						get_batch_status_label($old_batch['status']),
						get_batch_status_label(get_post('status')))
				);
			}

			display_notification(_('Batch has been updated.'));
		} else {
			$new_id = add_stock_batch(
				get_post('batch_no'),
				get_post('stock_id_edit'),
				get_post('status'),
				get_post('manufacturing_date'),
				get_post('expiry_date'),
				get_post('best_before_date'),
				get_post('retest_date'),
				get_post('shelf_life_days'),
				get_post('supplier_id', null),
				get_post('supplier_batch_no'),
				get_post('grn_id', null),
				get_post('work_order_id', null),
				get_post('initial_qty', 0),
				get_post('country_of_origin'),
				get_post('certification'),
				get_post('notes')
			);

			// Log initial creation movement with initial_qty if > 0
			$init_qty = (float)get_post('initial_qty', 0);
			if ($init_qty > 0) {
				$init_loc = get_post('init_location', '');
				add_batch_movement(
					$new_id, 0, 0,
					$init_loc, null, $init_qty,
					Today(), '', _('Batch created manually — initial quantity')
				);
			}

			display_notification(_('New batch has been added.'));
		}
		$Mode = 'RESET';
	}
}

//----------------------------------------------------------------------
// Handle DELETE
//----------------------------------------------------------------------
if ($Mode == 'Delete') {
	$can = can_delete_stock_batch($selected_id);
	if ($can === true) {
		delete_stock_batch($selected_id);
		display_notification(_('Selected batch has been deleted.'));
	} else {
		display_error($can);
	}
	$Mode = 'RESET';
}

//----------------------------------------------------------------------
// Handle BULK GENERATE
//----------------------------------------------------------------------
if (isset($_POST['bulk_generate']) && $_POST['bulk_generate'] != '') {
	$input_error = 0;

	if (strlen(get_post('bulk_stock_id')) == 0) {
		$input_error = 1;
		display_error(_('An item must be selected for bulk generation.'));
		set_focus('bulk_stock_id');
	}

	$bulk_count = (int)get_post('bulk_count');
	if ($bulk_count < 1 || $bulk_count > 1000) {
		$input_error = 1;
		display_error(_('Quantity must be between 1 and 1000.'));
		set_focus('bulk_count');
	}

	if ($input_error == 0) {
		$generated = bulk_create_stock_batches(
			get_post('bulk_stock_id'),
			$bulk_count,
			'active',
			get_post('bulk_manufacturing_date'),
			get_post('bulk_expiry_date'),
			get_post('bulk_supplier_id', null),
			(float)get_post('bulk_initial_qty', 0),
			get_post('bulk_notes')
		);
		display_notification(sprintf(_('%d batch numbers have been generated successfully.'), count($generated)));

		if (count($generated) <= 20) {
			display_note(_('Generated:') . ' ' . implode(', ', $generated));
		} else {
			display_note(sprintf(_('Generated %d batch numbers. First: %s, Last: %s'),
				count($generated), $generated[0], $generated[count($generated) - 1]));
		}
		$Ajax->activate('batch_list');
	}
}

//----------------------------------------------------------------------
// Handle IMPORT
//----------------------------------------------------------------------
if (isset($_POST['import_batches']) && $_POST['import_batches'] != '') {
	$input_error = 0;

	if (strlen(get_post('import_stock_id')) == 0) {
		$input_error = 1;
		display_error(_('An item must be selected for import.'));
		set_focus('import_stock_id');
	}

	$import_text = get_post('import_text');
	if (strlen(trim($import_text)) == 0) {
		$input_error = 1;
		display_error(_('Batch numbers text must not be empty.'));
		set_focus('import_text');
	}

	if ($input_error == 0) {
		$result = import_stock_batches(
			get_post('import_stock_id'),
			$import_text,
			'active',
			get_post('import_expiry_date'),
			get_post('import_supplier_id', null),
			(float)get_post('import_initial_qty', 0)
		);

		if ($result['imported'] > 0)
			display_notification(sprintf(_('%d batch numbers imported successfully.'), $result['imported']));
		if (count($result['duplicates']) > 0)
			display_warning(sprintf(_('%d duplicate batch numbers skipped: %s'),
				count($result['duplicates']),
				implode(', ', array_slice($result['duplicates'], 0, 10))
				. (count($result['duplicates']) > 10 ? '...' : '')));
		if (count($result['errors']) > 0) {
			foreach ($result['errors'] as $err)
				display_error($err);
		}
		$Ajax->activate('batch_list');
	}
}

//----------------------------------------------------------------------
// Handle SPLIT
//----------------------------------------------------------------------
if (isset($_POST['do_split']) && $_POST['do_split'] != '') {
	$input_error = 0;

	$split_batch_id = (int)get_post('split_batch_id');
	$split_qty = (float)get_post('split_qty');
	$split_new_no = get_post('split_new_batch_no');
	$split_loc = get_post('split_loc_code', '');

	if ($split_batch_id <= 0) {
		$input_error = 1;
		display_error(_('No batch selected for split.'));
	}

	if ($split_qty <= 0) {
		$input_error = 1;
		display_error(_('Split quantity must be greater than zero.'));
		set_focus('split_qty');
	}

	if ($input_error == 0) {
		$new_id = split_batch($split_batch_id, $split_qty, $split_new_no ? $split_new_no : null,
			$split_loc, get_post('split_notes', ''));

		if ($new_id === false)
			display_error(_('Split failed. Check that the source batch has enough quantity.'));
		else {
			$new_batch = get_stock_batch($new_id);
			display_notification(sprintf(_('Batch split successful. New batch: %s (ID: %d)'),
				$new_batch['batch_no'], $new_id));
		}
		$Ajax->activate('batch_list');
	}
}

//----------------------------------------------------------------------
// Handle AUTO-EXPIRE
//----------------------------------------------------------------------
if (isset($_POST['auto_expire']) && $_POST['auto_expire'] != '') {
	$expired_count = auto_expire_batches();
	if ($expired_count > 0)
		display_notification(sprintf(_('%d batches have been marked as expired.'), $expired_count));
	else
		display_note(_('No active batches found with past expiry dates.'));
	$Ajax->activate('batch_list');
}

//----------------------------------------------------------------------
// RESET
//----------------------------------------------------------------------
if ($Mode == 'RESET') {
	$selected_id = -1;
	$sav_stock = get_post('filter_stock_id');
	$sav_status = get_post('filter_status');
	$sav_expiry = get_post('filter_expiry');
	$sav_search = get_post('filter_search');
	$sav_inactive = get_post('show_inactive');
	unset($_POST);
	$_POST['filter_stock_id'] = $sav_stock;
	$_POST['filter_status'] = $sav_status;
	$_POST['filter_expiry'] = $sav_expiry;
	$_POST['filter_search'] = $sav_search;
	$_POST['show_inactive'] = $sav_inactive;
}

//----------------------------------------------------------------------
// Handle Auto-Generate button
//----------------------------------------------------------------------
if (isset($_POST['auto_generate_batch']) && $_POST['auto_generate_batch'] != '') {
	if (strlen(get_post('stock_id_edit')) > 0) {
		$_POST['batch_no'] = generate_batch_number(get_post('stock_id_edit'));
		$Ajax->activate('_page_body');
	} else {
		display_error(_('Select an item first before auto-generating.'));
	}
}

//----------------------------------------------------------------------
// Handle Calculate Expiry button
//----------------------------------------------------------------------
if (isset($_POST['calc_expiry']) && $_POST['calc_expiry'] != '') {
	if (strlen(get_post('stock_id_edit')) > 0 && strlen(get_post('manufacturing_date')) > 0) {
		$calc = calculate_expiry_from_shelf_life(get_post('stock_id_edit'), get_post('manufacturing_date'));
		if ($calc) {
			$_POST['expiry_date'] = $calc;
			display_note(sprintf(_('Expiry date calculated: %s'), $calc));
		} else {
			display_warning(_('Could not calculate expiry. Check that the item has a shelf life configured.'));
		}
		$Ajax->activate('_page_body');
	} else {
		display_error(_('Select an item and enter a manufacturing date first.'));
	}
}

//----------------------------------------------------------------------
// Refresh list on filter changes
//----------------------------------------------------------------------
if (list_updated('filter_stock_id') || list_updated('filter_status') || list_updated('filter_expiry')
	|| isset($_POST['search_batches']))
	$Ajax->activate('batch_list');

//======================================================================
//  F I L T E R S
//======================================================================

start_form();

start_table(TABLESTYLE_NOBORDER);
start_row();

batch_tracked_items_list_cells(null, 'filter_stock_id', get_post('filter_stock_id'), true, true);
batch_status_list_cells(null, 'filter_status', get_post('filter_status'), true, true);
batch_expiry_filter_cells(null, 'filter_expiry', get_post('filter_expiry'), true);

end_row();
start_row();

ref_cells(_('Search:'), 'filter_search', '', null, _('Enter reference fragment or leave empty'));
check_cells(_('Show inactive:'), 'show_inactive', null, true);
submit_cells('search_batches', _('Search'), '', _('Filter batch numbers'), 'default');

// Auto-expire button
submit_cells('auto_expire', _('Auto-Expire'), '', _('Mark past-expiry active batches as expired'), true);

end_row();
end_table();

//======================================================================
//  L I S T   T A B L E
//======================================================================

div_start('batch_list');

$stock_id_filter = get_post('filter_stock_id', '');
$status_filter = get_post('filter_status', '');
$expiry_filter = get_post('filter_expiry', '');
$search_filter = get_post('filter_search', '');
$show_inactive = check_value('show_inactive');

$result = get_stock_batches($stock_id_filter, $status_filter, $search_filter,
	$show_inactive, $expiry_filter);

$total = count_stock_batches($stock_id_filter, $status_filter, $search_filter,
	$show_inactive, $expiry_filter);

// Summary line
display_note(sprintf(_('Showing %d batch(es)'), $total), 0, 0, "style='margin:5px 0;'");

// Expiry summary cards
$expiry_summary = get_batch_expiry_summary($stock_id_filter);
echo "<div style='margin:5px 0 10px 0;'>";
echo "<span style='display:inline-block;padding:3px 10px;margin:2px;border-radius:3px;color:#fff;background:#dc3545;'>"
	. sprintf(_('Expired: %d'), $expiry_summary['expired']) . "</span>";
echo "<span style='display:inline-block;padding:3px 10px;margin:2px;border-radius:3px;color:#fff;background:#fd7e14;'>"
	. sprintf(_('Critical (≤30d): %d'), $expiry_summary['critical']) . "</span>";
echo "<span style='display:inline-block;padding:3px 10px;margin:2px;border-radius:3px;color:#fff;background:#ffc107;'>"
	. sprintf(_('Warning: %d'), $expiry_summary['warning']) . "</span>";
echo "<span style='display:inline-block;padding:3px 10px;margin:2px;border-radius:3px;color:#fff;background:#28a745;'>"
	. sprintf(_('OK: %d'), $expiry_summary['ok']) . "</span>";
echo "<span style='display:inline-block;padding:3px 10px;margin:2px;border-radius:3px;color:#fff;background:#999;'>"
	. sprintf(_('No Expiry: %d'), $expiry_summary['no_expiry']) . "</span>";
echo "</div>";

start_table(TABLESTYLE, "width='98%'");
$th = array(
	_('Batch #'), _('Item'), _('Status'), _('Expiry Date'), _('Days'),
	_('Mfg Date'), _('Initial Qty'), _('Supplier'), _('Created'), '', ''
);
inactive_control_column($th);
table_header($th);

$k = 0;
while ($myrow = db_fetch($result)) {
	alt_table_row_color($k);

	// Batch number — link to inquiry
	$batch_link = '<a href="' . $path_to_root . '/inventory/inquiry/batch_inquiry.php?batch_id='
		. $myrow['id'] . '">' . $myrow['batch_no'] . '</a>';
	label_cell($batch_link);

	// Item
	label_cell($myrow['stock_id'] . ' - ' . $myrow['item_description']);

	// Status badge
	label_cell(batch_status_badge($myrow['status']));

	// Expiry date with color badge
	if ($myrow['expiry_date'])
		label_cell(batch_expiry_badge($myrow['expiry_date']));
	else
		label_cell('-');

	// Days until expiry
	if ($myrow['days_until_expiry'] !== null) {
		$days = (int)$myrow['days_until_expiry'];
		if ($days < 0)
			label_cell('<span style="color:red;font-weight:bold;">' . $days . '</span>', "align='right'");
		elseif ($days <= 30)
			label_cell('<span style="color:orange;font-weight:bold;">' . $days . '</span>', "align='right'");
		else
			label_cell($days, "align='right'");
	} else {
		label_cell('-', "align='right'");
	}

	// Manufacturing date
	label_cell($myrow['manufacturing_date'] ? sql2date($myrow['manufacturing_date']) : '-');

	// Initial qty
	label_cell($myrow['initial_qty'] > 0 ? number_format2($myrow['initial_qty'], 2) : '-', "align='right'");

	// Supplier
	label_cell($myrow['supplier_name'] ? $myrow['supplier_name'] : '-');

	// Created
	label_cell(sql2date($myrow['created_at']));

	inactive_control_cell($myrow['id'], $myrow['inactive'], 'stock_batches', 'id');
	edit_button_cell("Edit" . $myrow['id'], _('Edit'));
	delete_button_cell("Delete" . $myrow['id'], _('Delete'));

	end_row();
}

inactive_control_row($th);
end_table(1);

div_end();

//======================================================================
//  A D D  /  E D I T   F O R M
//======================================================================

start_table(TABLESTYLE2);

if ($selected_id != -1) {
	if ($Mode == 'Edit') {
		$myrow = get_stock_batch($selected_id);
		$_POST['batch_no'] = $myrow['batch_no'];
		$_POST['stock_id_edit'] = $myrow['stock_id'];
		$_POST['status'] = $myrow['status'];
		$_POST['manufacturing_date'] = $myrow['manufacturing_date'] ? sql2date($myrow['manufacturing_date']) : '';
		$_POST['expiry_date'] = $myrow['expiry_date'] ? sql2date($myrow['expiry_date']) : '';
		$_POST['best_before_date'] = $myrow['best_before_date'] ? sql2date($myrow['best_before_date']) : '';
		$_POST['retest_date'] = $myrow['retest_date'] ? sql2date($myrow['retest_date']) : '';
		$_POST['shelf_life_days'] = $myrow['shelf_life_days'];
		$_POST['supplier_id'] = $myrow['supplier_id'];
		$_POST['supplier_batch_no'] = $myrow['supplier_batch_no'];
		$_POST['grn_id'] = $myrow['grn_id'];
		$_POST['work_order_id'] = $myrow['work_order_id'];
		$_POST['initial_qty'] = $myrow['initial_qty'];
		$_POST['country_of_origin'] = $myrow['country_of_origin'];
		$_POST['certification'] = $myrow['certification'];
		$_POST['notes'] = $myrow['notes'];
	}
	hidden('selected_id', $selected_id);
}

$is_edit = ($selected_id != -1);

// Section header
echo "<tr><td colspan='2'><strong>" . ($is_edit ? _('Edit Batch') : _('Add New Batch')) . "</strong></td></tr>\n";

// Batch Number
text_row_ex(_('Batch Number:'), 'batch_no', 50, 100);

// Item selector
if ($is_edit) {
	label_row(_('Item:'), get_post('stock_id_edit') . ' - '
		. (isset($myrow['item_description']) ? $myrow['item_description'] : ''));
	hidden('stock_id_edit', get_post('stock_id_edit'));
} else {
	echo "<tr><td class='label'>" . _('Item:') . "</td><td>";
	echo batch_tracked_items_list('stock_id_edit', get_post('stock_id_edit'), false, false);
	echo "</td></tr>\n";
}

// Auto-generate button (only for new)
if (!$is_edit) {
	echo "<tr><td></td><td>";
	submit('auto_generate_batch', _('Auto-Generate Batch #'), true, _('Generate batch number using format template'), true);
	echo "</td></tr>\n";
}

// Status
batch_status_list_row(_('Status:'), 'status', get_post('status', 'active'), false, false);

// Initial Quantity
small_amount_row(_('Initial Quantity:'), 'initial_qty', get_post('initial_qty', ''), null, null, 2);

// Location for initial quantity (only for new batches)
if (!$is_edit) {
	locations_list_row(_('Initial Location:'), 'init_location', get_post('init_location'), false, false);
}

// Dates section
echo "<tr><td colspan='2' style='padding-top:10px;'><strong>" . _('Dates & Shelf Life') . "</strong></td></tr>\n";

date_row(_('Manufacturing Date:'), 'manufacturing_date', '', null, 0, 0, 1001);
date_row(_('Expiry Date:'), 'expiry_date', '', null, 0, 0, 1001);
date_row(_('Best Before Date:'), 'best_before_date', '', null, 0, 0, 1001);
date_row(_('Retest Date:'), 'retest_date', '', null, 0, 0, 1001);
text_row_ex(_('Shelf Life (days):'), 'shelf_life_days', 10, 10);

// Auto-calc expiry button
if (!$is_edit) {
	echo "<tr><td></td><td>";
	submit('calc_expiry', _('Calculate Expiry from Shelf Life'), true,
		_('Auto-calculate expiry date from manufacturing date + item shelf life'), true);
	echo "</td></tr>\n";
}

// Supplier section
echo "<tr><td colspan='2' style='padding-top:10px;'><strong>" . _('Source') . "</strong></td></tr>\n";

supplier_list_row(_('Supplier:'), 'supplier_id', get_post('supplier_id'), _('Select Supplier'), true);

text_row_ex(_('Supplier Batch #:'), 'supplier_batch_no', 50, 100);

// Origin & Certification
echo "<tr><td colspan='2' style='padding-top:10px;'><strong>" . _('Quality & Origin') . "</strong></td></tr>\n";

text_row_ex(_('Country of Origin:'), 'country_of_origin', 50, 60);
text_row_ex(_('Certification:'), 'certification', 50, 200);

// Notes
textarea_row(_('Notes:'), 'notes', get_post('notes', ''), 40, 3);

end_table(1);

submit_add_or_update_center($selected_id == -1, '', 'both');

//======================================================================
//  S P L I T   B A T C H   S E C T I O N
//======================================================================

if ($selected_id != -1 && $is_edit) {
	br();
	echo "<fieldset style='border:1px solid #ccc;padding:10px;margin:10px 0;'>";
	echo "<legend><strong>" . _('Split Batch') . "</strong></legend>";

	$current_qty = get_batch_quantity_on_hand($selected_id);
	echo "<p>" . sprintf(_('Current quantity on hand: <strong>%s</strong>'), number_format2($current_qty, 2)) . "</p>";

	start_table(TABLESTYLE2);
	hidden('split_batch_id', $selected_id);

	small_amount_row(_('Quantity to Split:'), 'split_qty', '', null, null, 2);
	text_row_ex(_('New Batch # (blank=auto):'), 'split_new_batch_no', 50, 100);
	locations_list_row(_('Location:'), 'split_loc_code', null, true);
	textarea_row(_('Notes:'), 'split_notes', '', 40, 2);

	end_table(1);
	submit_center('do_split', _('Split Batch'), true, _('Create a new batch from a portion of this batch'), 'default');

	echo "</fieldset>";
}

//======================================================================
//  B U L K   G E N E R A T E   S E C T I O N
//======================================================================

if ($selected_id == -1) {

	br();
	echo "<fieldset style='border:1px solid #ccc;padding:10px;margin:10px 0;'>";
	echo "<legend><strong>" . _('Bulk Generate Batch Numbers') . "</strong></legend>";

	start_table(TABLESTYLE2);

	echo "<tr><td class='label'>" . _('Item:') . "</td><td>";
	echo batch_tracked_items_list('bulk_stock_id', get_post('bulk_stock_id'), false, false);
	echo "</td></tr>\n";

	text_row_ex(_('Quantity (1-1000):'), 'bulk_count', 10, 10);
	small_amount_row(_('Initial Qty per Batch:'), 'bulk_initial_qty', '', null, null, 2);
	date_row(_('Manufacturing Date:'), 'bulk_manufacturing_date', '', null, 0, 0, 1001);
	date_row(_('Expiry Date:'), 'bulk_expiry_date', '', null, 0, 0, 1001);

	echo "<tr><td class='label'>" . _('Supplier:') . "</td><td>";
	supplier_list('bulk_supplier_id', get_post('bulk_supplier_id'), _('None'), true);
	echo "</td></tr>\n";

	textarea_row(_('Notes:'), 'bulk_notes', '', 40, 2);

	end_table(1);

	submit_center('bulk_generate', _('Generate Batches'), true, _('Bulk generate batch numbers'), 'default');

	echo "</fieldset>";

	//======================================================================
	//  I M P O R T   S E C T I O N
	//======================================================================

	echo "<fieldset style='border:1px solid #ccc;padding:10px;margin:10px 0;'>";
	echo "<legend><strong>" . _('Import Batch Numbers') . "</strong></legend>";

	start_table(TABLESTYLE2);

	echo "<tr><td class='label'>" . _('Item:') . "</td><td>";
	echo batch_tracked_items_list('import_stock_id', get_post('import_stock_id'), false, false);
	echo "</td></tr>\n";

	small_amount_row(_('Initial Qty per Batch:'), 'import_initial_qty', '', null, null, 2);
	date_row(_('Expiry Date:'), 'import_expiry_date', '', null, 0, 0, 1001);

	echo "<tr><td class='label'>" . _('Supplier:') . "</td><td>";
	supplier_list('import_supplier_id', get_post('import_supplier_id'), _('None'), true);
	echo "</td></tr>\n";

	textarea_row(_('Batch Numbers (one per line or comma-separated):'), 'import_text', '', 60, 5);

	end_table(1);

	submit_center('import_batches', _('Import'), true, _('Import batch numbers from text'), 'default');

	echo "</fieldset>";
}

end_form();
end_page();
