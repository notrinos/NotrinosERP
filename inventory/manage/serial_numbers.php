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
 * Serial Number Management — List, Create, Edit, Bulk Generate, Import.
 *
 * Features:
 *   - Filterable list by item, status, location, search text
 *   - Create / Edit single serial number
 *   - Bulk generate serial numbers for an item
 *   - Import serial numbers from CSV text
 *   - Manual status change with movement logging
 */
$page_security = 'SA_SERIALNUMBER';
$path_to_root = '../..';
include_once($path_to_root . '/includes/session.inc');

$_SESSION['page_title'] = _($help_context = 'Serial Numbers');

$js = '';
if (user_use_date_picker())
	$js .= get_js_date_picker();

page($_SESSION['page_title'], false, false, '', $js);

include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/includes/date_functions.inc');
include_once($path_to_root . '/inventory/includes/db/serial_numbers_db.inc');
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

	if (strlen(get_post('serial_no')) == 0) {
		$input_error = 1;
		display_error(_('The serial number must be entered.'));
		set_focus('serial_no');
	}

	if (strlen(get_post('stock_id_edit')) == 0) {
		$input_error = 1;
		display_error(_('An item must be selected.'));
		set_focus('stock_id_edit');
	}

	// Check duplicate
	if ($input_error == 0) {
		$exclude = ($selected_id != -1) ? $selected_id : 0;
		if (is_serial_number_duplicate(get_post('serial_no'), get_post('stock_id_edit'), $exclude)) {
			$input_error = 1;
			display_error(_('This serial number already exists for the selected item.'));
			set_focus('serial_no');
		}
	}

	// Validate status
	if ($input_error == 0) {
		$valid_statuses = get_serial_statuses();
		if (!isset($valid_statuses[get_post('status')])) {
			$input_error = 1;
			display_error(_('Invalid status selected.'));
			set_focus('status');
		}
	}

	if ($input_error == 0) {
		if ($selected_id != -1) {
			// Get old serial to detect status change
			$old_serial = get_serial_number($selected_id);

			update_serial_number(
				$selected_id,
				get_post('serial_no'),
				get_post('status'),
				get_post('loc_code'),
				get_post('wh_loc_id', null),
				get_post('batch_id', null),
				get_post('supplier_id', null),
				get_post('customer_id', null),
				get_post('purchase_date'),
				get_post('delivery_date'),
				get_post('manufacturing_date'),
				get_post('expiry_date'),
				get_post('warranty_start'),
				get_post('warranty_end'),
				get_post('warranty_type'),
				get_post('purchase_cost', 0),
				get_post('grn_id', null),
				get_post('delivery_note_id', null),
				get_post('work_order_id', null),
				get_post('notes')
			);

			// Log status change if changed
			if ($old_serial && $old_serial['status'] !== get_post('status')) {
				add_serial_movement(
					$selected_id, 0, 0,
					$old_serial['loc_code'], get_post('loc_code'),
					$old_serial['status'], get_post('status'),
					Today(), '', _('Manual status change from serial management')
				);
			}

			display_notification(_('Serial number has been updated.'));
		} else {
			$new_id = add_serial_number(
				get_post('serial_no'),
				get_post('stock_id_edit'),
				get_post('status'),
				get_post('loc_code'),
				get_post('wh_loc_id', null),
				get_post('batch_id', null),
				get_post('supplier_id', null),
				get_post('purchase_date'),
				get_post('manufacturing_date'),
				get_post('expiry_date'),
				get_post('warranty_start'),
				get_post('warranty_end'),
				get_post('warranty_type'),
				get_post('purchase_cost', 0),
				get_post('grn_id', null),
				get_post('notes')
			);

			// Log initial creation movement
			add_serial_movement(
				$new_id, 0, 0,
				null, get_post('loc_code'),
				null, get_post('status'),
				Today(), '', _('Serial number created manually')
			);

			display_notification(_('New serial number has been added.'));
		}
		$Mode = 'RESET';
	}
}

//----------------------------------------------------------------------
// Handle DELETE
//----------------------------------------------------------------------
if ($Mode == 'Delete') {
	$can = can_delete_serial_number($selected_id);
	if ($can === true) {
		delete_serial_number($selected_id);
		display_notification(_('Selected serial number has been deleted.'));
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
		$generated = bulk_create_serial_numbers(
			get_post('bulk_stock_id'),
			$bulk_count,
			'available',
			get_post('bulk_loc_code', null),
			null,
			get_post('bulk_supplier_id', null),
			get_post('bulk_purchase_date'),
			get_post('bulk_notes')
		);
		display_notification(sprintf(_('%d serial numbers have been generated successfully.'), count($generated)));

		// Show the generated serials
		if (count($generated) <= 20) {
			display_note(_('Generated:') . ' ' . implode(', ', $generated));
		} else {
			display_note(sprintf(_('Generated %d serial numbers. First: %s, Last: %s'),
				count($generated), $generated[0], $generated[count($generated) - 1]));
		}
		$Ajax->activate('serial_list');
	}
}

//----------------------------------------------------------------------
// Handle IMPORT
//----------------------------------------------------------------------
if (isset($_POST['import_serials']) && $_POST['import_serials'] != '') {
	$input_error = 0;

	if (strlen(get_post('import_stock_id')) == 0) {
		$input_error = 1;
		display_error(_('An item must be selected for import.'));
		set_focus('import_stock_id');
	}

	$import_text = get_post('import_text');
	if (strlen(trim($import_text)) == 0) {
		$input_error = 1;
		display_error(_('Serial numbers text must not be empty.'));
		set_focus('import_text');
	}

	if ($input_error == 0) {
		$result = import_serial_numbers(
			get_post('import_stock_id'),
			$import_text,
			'available',
			get_post('import_loc_code', null),
			get_post('import_supplier_id', null)
		);

		if ($result['imported'] > 0)
			display_notification(sprintf(_('%d serial numbers imported successfully.'), $result['imported']));
		if (count($result['duplicates']) > 0)
			display_warning(sprintf(_('%d duplicate serial numbers skipped: %s'),
				count($result['duplicates']),
				implode(', ', array_slice($result['duplicates'], 0, 10))
				. (count($result['duplicates']) > 10 ? '...' : '')));
		if (count($result['errors']) > 0) {
			foreach ($result['errors'] as $err)
				display_error($err);
		}
		$Ajax->activate('serial_list');
	}
}

//----------------------------------------------------------------------
// RESET
//----------------------------------------------------------------------
if ($Mode == 'RESET') {
	$selected_id = -1;
	$sav_stock = get_post('filter_stock_id');
	$sav_status = get_post('filter_status');
	$sav_loc = get_post('filter_loc_code');
	$sav_search = get_post('filter_search');
	$sav_inactive = get_post('show_inactive');
	unset($_POST);
	$_POST['filter_stock_id'] = $sav_stock;
	$_POST['filter_status'] = $sav_status;
	$_POST['filter_loc_code'] = $sav_loc;
	$_POST['filter_search'] = $sav_search;
	$_POST['show_inactive'] = $sav_inactive;
}

//----------------------------------------------------------------------
// Refresh list on filter changes
//----------------------------------------------------------------------
if (list_updated('filter_stock_id') || list_updated('filter_status') || list_updated('filter_loc_code')
	|| isset($_POST['search_serials']))
	$Ajax->activate('serial_list');

//======================================================================
//  F I L T E R S
//======================================================================

start_form();

start_table(TABLESTYLE_NOBORDER);
start_row();

serial_tracked_items_list_cells(_('Item:'), 'filter_stock_id', get_post('filter_stock_id'), true, true);
serial_status_list_cells(_('Status:'), 'filter_status', get_post('filter_status'), true, true);
locations_list_cells(_('Location:'), 'filter_loc_code', null, true);

end_row();
start_row();

text_cells(_('Search:'), 'filter_search', get_post('filter_search'), 30, 100);
submit_cells('search_serials', _('Search'), '', _('Filter serial numbers'), 'default');
check_cells(_('Show inactive:'), 'show_inactive', null, true);

end_row();
end_table();

//======================================================================
//  L I S T   T A B L E
//======================================================================

div_start('serial_list');

$stock_id_filter = get_post('filter_stock_id', '');
$status_filter = get_post('filter_status', '');
$loc_filter = get_post('filter_loc_code', '');
$search_filter = get_post('filter_search', '');
$show_inactive = check_value('show_inactive');

$result = get_serial_numbers($stock_id_filter, $status_filter, $loc_filter,
	$search_filter, $show_inactive);

$total = count_serial_numbers($stock_id_filter, $status_filter, $loc_filter,
	$search_filter, $show_inactive);

// Summary line
display_note(sprintf(_('Showing %d serial number(s)'), $total), 0, 0, "style='margin:5px 0;'");

start_table(TABLESTYLE, "width='95%'");
$th = array(
	_('Serial Number'), _('Item'), _('Status'), _('Location'),
	_('Supplier'), _('Customer'), _('Warranty End'), _('Created'), '', ''
);
inactive_control_column($th);
table_header($th);

$k = 0;
while ($myrow = db_fetch($result)) {
	alt_table_row_color($k);

	// Serial number — link to inquiry
	$serial_link = '<a href="' . $path_to_root . '/inventory/inquiry/serial_inquiry.php?serial_id='
		. $myrow['id'] . '">' . $myrow['serial_no'] . '</a>';
	label_cell($serial_link);

	// Item
	label_cell($myrow['stock_id'] . ' - ' . $myrow['item_description']);

	// Status badge
	label_cell(serial_status_badge($myrow['status']));

	// Location
	label_cell($myrow['location_name'] ? $myrow['location_name'] : '-');

	// Supplier
	label_cell($myrow['supplier_name'] ? $myrow['supplier_name'] : '-');

	// Customer
	label_cell($myrow['customer_name'] ? $myrow['customer_name'] : '-');

	// Warranty end
	if ($myrow['warranty_end']) {
		$wend = sql2date($myrow['warranty_end']);
		if ($myrow['warranty_end'] < date('Y-m-d'))
			label_cell('<span style="color:red;">' . $wend . '</span>');
		else
			label_cell('<span style="color:green;">' . $wend . '</span>');
	} else {
		label_cell('-');
	}

	// Created date
	label_cell(sql2date($myrow['created_at']));

	inactive_control_cell($myrow['id'], $myrow['inactive'], 'serial_numbers', 'id');
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
		$myrow = get_serial_number($selected_id);
		$_POST['serial_no'] = $myrow['serial_no'];
		$_POST['stock_id_edit'] = $myrow['stock_id'];
		$_POST['status'] = $myrow['status'];
		$_POST['loc_code'] = $myrow['loc_code'];
		$_POST['wh_loc_id'] = $myrow['wh_loc_id'];
		$_POST['batch_id'] = $myrow['batch_id'];
		$_POST['supplier_id'] = $myrow['supplier_id'];
		$_POST['customer_id'] = $myrow['customer_id'];
		$_POST['purchase_date'] = $myrow['purchase_date'] ? sql2date($myrow['purchase_date']) : '';
		$_POST['delivery_date'] = $myrow['delivery_date'] ? sql2date($myrow['delivery_date']) : '';
		$_POST['manufacturing_date'] = $myrow['manufacturing_date'] ? sql2date($myrow['manufacturing_date']) : '';
		$_POST['expiry_date'] = $myrow['expiry_date'] ? sql2date($myrow['expiry_date']) : '';
		$_POST['warranty_start'] = $myrow['warranty_start'] ? sql2date($myrow['warranty_start']) : '';
		$_POST['warranty_end'] = $myrow['warranty_end'] ? sql2date($myrow['warranty_end']) : '';
		$_POST['warranty_type'] = $myrow['warranty_type'];
		$_POST['purchase_cost'] = $myrow['purchase_cost'];
		$_POST['grn_id'] = $myrow['grn_id'];
		$_POST['delivery_note_id'] = $myrow['delivery_id'];
		$_POST['work_order_id'] = $myrow['work_order_id'];
		$_POST['notes'] = $myrow['notes'];
	}
	hidden('selected_id', $selected_id);
}

$is_edit = ($selected_id != -1);

// Section header
echo "<tr><td colspan='2'><strong>" . ($is_edit ? _('Edit Serial Number') : _('Add New Serial Number')) . "</strong></td></tr>\n";

// Serial Number
text_row_ex(_('Serial Number:'), 'serial_no', 50, 100);

// Item selector
if ($is_edit) {
	// Item is locked when editing
	label_row(_('Item:'), get_post('stock_id_edit') . ' - ' .
		(isset($myrow['item_description']) ? $myrow['item_description'] : ''));
	hidden('stock_id_edit', get_post('stock_id_edit'));
} else {
	// For new serial: select from serial-tracked items
	echo "<tr><td class='label'>" . _('Item:') . "</td><td>";
	echo serial_tracked_items_list('stock_id_edit', get_post('stock_id_edit'), false, false);
	echo "</td></tr>\n";
}

// Auto-generate button (only for new)
if (!$is_edit) {
	echo "<tr><td></td><td>";
	submit('auto_generate', _('Auto-Generate Serial'), true, _('Generate serial number using format template'), true);
	echo "</td></tr>\n";
}

// Status
serial_status_list_row(_('Status:'), 'status', get_post('status', 'available'), false, false);

// Location
locations_list_row(_('Location:'), 'loc_code', get_post('loc_code'), true);

// Supplier
echo "<tr><td class='label'>" . _('Supplier:') . "</td><td>";
supplier_list('supplier_id', get_post('supplier_id'), _('None'), true);
echo "</td></tr>\n";

// Customer (only for edit)
if ($is_edit) {
	echo "<tr><td class='label'>" . _('Customer:') . "</td><td>";
	customer_list('customer_id', get_post('customer_id'), _('None'), true);
	echo "</td></tr>\n";
}

// Dates section
echo "<tr><td colspan='2' style='padding-top:10px;'><strong>" . _('Dates') . "</strong></td></tr>\n";

date_row(_('Purchase Date:'), 'purchase_date', '', null, 0, 0, 1001);
date_row(_('Manufacturing Date:'), 'manufacturing_date', '', null, 0, 0, 1001);
date_row(_('Expiry Date:'), 'expiry_date', '', null, 0, 0, 1001);

// Warranty section
echo "<tr><td colspan='2' style='padding-top:10px;'><strong>" . _('Warranty') . "</strong></td></tr>\n";

date_row(_('Warranty Start:'), 'warranty_start', '', null, 0, 0, 1001);
date_row(_('Warranty End:'), 'warranty_end', '', null, 0, 0, 1001);

$warranty_types = array('' => _('None'), 'standard' => _('Standard'), 'extended' => _('Extended'), 'limited' => _('Limited'));
array_selector_row(_('Warranty Type:'), 'warranty_type', get_post('warranty_type', ''), $warranty_types);

// Cost
small_amount_row(_('Purchase Cost:'), 'purchase_cost', get_post('purchase_cost', ''), null, null, 2);

// Notes
textarea_row(_('Notes:'), 'notes', get_post('notes', ''), 40, 3);

end_table(1);

submit_add_or_update_center($selected_id == -1, '', 'both');

//======================================================================
//  B U L K   G E N E R A T E   S E C T I O N
//======================================================================

if ($selected_id == -1) {

	br();
	echo "<fieldset style='border:1px solid #ccc;padding:10px;margin:10px 0;'>";
	echo "<legend><strong>" . _('Bulk Generate Serial Numbers') . "</strong></legend>";

	start_table(TABLESTYLE2);

	echo "<tr><td class='label'>" . _('Item:') . "</td><td>";
	echo serial_tracked_items_list('bulk_stock_id', get_post('bulk_stock_id'), false, false);
	echo "</td></tr>\n";

	text_row_ex(_('Quantity (1-1000):'), 'bulk_count', 10, 10);

	locations_list_row(_('Location:'), 'bulk_loc_code', null, true);

	echo "<tr><td class='label'>" . _('Supplier:') . "</td><td>";
	supplier_list('bulk_supplier_id', get_post('bulk_supplier_id'), _('None'), true);
	echo "</td></tr>\n";

	date_row(_('Purchase Date:'), 'bulk_purchase_date', '', null, 0, 0, 1001);

	textarea_row(_('Notes:'), 'bulk_notes', '', 40, 2);

	end_table(1);

	submit_center('bulk_generate', _('Generate Serials'), true, _('Bulk generate serial numbers'), 'default');

	echo "</fieldset>";

	//======================================================================
	//  I M P O R T   S E C T I O N
	//======================================================================

	echo "<fieldset style='border:1px solid #ccc;padding:10px;margin:10px 0;'>";
	echo "<legend><strong>" . _('Import Serial Numbers') . "</strong></legend>";

	start_table(TABLESTYLE2);

	echo "<tr><td class='label'>" . _('Item:') . "</td><td>";
	echo serial_tracked_items_list('import_stock_id', get_post('import_stock_id'), false, false);
	echo "</td></tr>\n";

	locations_list_row(_('Location:'), 'import_loc_code', null, true);

	echo "<tr><td class='label'>" . _('Supplier:') . "</td><td>";
	supplier_list('import_supplier_id', get_post('import_supplier_id'), _('None'), true);
	echo "</td></tr>\n";

	textarea_row(_('Serial Numbers (one per line or comma-separated):'), 'import_text', '', 60, 5);

	end_table(1);

	submit_center('import_serials', _('Import'), true, _('Import serial numbers from text'), 'default');

	echo "</fieldset>";
}

//----------------------------------------------------------------------
// Handle Auto-Generate button
//----------------------------------------------------------------------
if (isset($_POST['auto_generate']) && $_POST['auto_generate'] != '') {
	if (strlen(get_post('stock_id_edit')) > 0) {
		$_POST['serial_no'] = generate_serial_number(get_post('stock_id_edit'));
		$Ajax->activate('_page_body');
	} else {
		display_error(_('Select an item first before auto-generating.'));
	}
}

end_form();
end_page();
