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
 * Warranty Claims Management.
 *
 * Features:
 *   - List warranty claims with filters (item, customer, status, date range)
 *   - Summary cards by status
 *   - Create/edit warranty claims with customer serial selection and warranty validity check
 *   - Status transitions: Open → Acknowledged → In Repair → Resolved/Replaced/Rejected → Closed
 *   - Repair parts tracking with cost calculation
 */
$page_security = 'SA_WARRANTY';
$path_to_root = '../..';
include_once($path_to_root . '/includes/session.inc');

$_SESSION['page_title'] = _($help_context = 'Warranty Claims');

$js = '';
if (user_use_date_picker())
	$js .= get_js_date_picker();

page($_SESSION['page_title'], false, false, '', $js);

include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/includes/date_functions.inc');
include_once($path_to_root . '/inventory/includes/db/warranty_recall_db.inc');
include_once($path_to_root . '/inventory/includes/db/serial_numbers_db.inc');
include_once($path_to_root . '/inventory/includes/serial_batch_ui.inc');
include_once($path_to_root . '/inventory/includes/inventory_db.inc');

simple_page_mode(true);

//----------------------------------------------------------------------
// Handle Delete
//----------------------------------------------------------------------
if ($Mode == 'Delete') {
	$can = can_delete_warranty_claim($selected_id);
	if ($can !== true) {
		display_error($can);
	} else {
		delete_warranty_claim($selected_id);
		display_notification(_('Warranty claim has been deleted.'));
	}
	$Mode = 'RESET';
}

//----------------------------------------------------------------------
// Handle Status Change
//----------------------------------------------------------------------
if (get_post('change_status')) {
	$claim_id = (int)get_post('status_claim_id');
	$new_status = get_post('new_status');
	$resolution_desc = get_post('resolution_description');
	$resolution_date = get_post('resolution_date');
	$repair_cost = get_post('repair_cost_input');

	$valid_statuses = array_keys(get_warranty_claim_statuses());
	if (in_array($new_status, $valid_statuses)) {
		update_warranty_claim_status($claim_id, $new_status,
			$resolution_desc, $resolution_date, $repair_cost);

		// If resolved/replaced/closed, update serial status
		$claim = get_warranty_claim($claim_id);
		if ($claim && $claim['serial_id']) {
			if ($new_status === 'in_repair') {
				update_serial_status($claim['serial_id'], 'in_repair', 0, 0,
					null, null, null, $claim['reference'], _('Warranty repair started'));
			} elseif ($new_status === 'resolved' || $new_status === 'closed') {
				update_serial_status($claim['serial_id'], 'delivered', 0, 0,
					null, null, null, $claim['reference'], _('Warranty claim resolved'));
			} elseif ($new_status === 'replaced') {
				update_serial_status($claim['serial_id'], 'scrapped', 0, 0,
					null, null, null, $claim['reference'], _('Replaced under warranty'));
			}
		}

		display_notification(sprintf(_('Warranty claim status changed to %s.'),
			get_warranty_claim_status_label($new_status)));
	}
	$Ajax->activate('_page_body');
}

//----------------------------------------------------------------------
// Handle Add Part
//----------------------------------------------------------------------
if (get_post('add_part')) {
	$claim_id = (int)get_post('part_claim_id');
	$part_stock_id = get_post('part_stock_id');
	$part_qty = (float)get_post('part_qty');
	$part_cost = (float)get_post('part_cost');

	if ($part_stock_id == '' || $part_qty <= 0) {
		display_error(_('Please select a part and enter a valid quantity.'));
	} else {
		add_warranty_claim_part($claim_id, $part_stock_id, $part_qty, $part_cost);
		recalculate_warranty_claim_cost($claim_id);
		display_notification(_('Repair part has been added.'));
	}
	$Ajax->activate('_page_body');
}

//----------------------------------------------------------------------
// Handle Delete Part
//----------------------------------------------------------------------
if (get_post('delete_part')) {
	$part_id = (int)get_post('delete_part_id');
	$claim_id = (int)get_post('part_claim_id_del');
	if ($part_id > 0) {
		delete_warranty_claim_part($part_id);
		recalculate_warranty_claim_cost($claim_id);
		display_notification(_('Repair part has been removed.'));
	}
	$Ajax->activate('_page_body');
}

//----------------------------------------------------------------------
// Handle Check Warranty
//----------------------------------------------------------------------
if (get_post('check_warranty')) {
	$serial_id = (int)get_post('serial_id');
	if ($serial_id > 0) {
		$validity = check_warranty_validity($serial_id);
		if ($validity['valid']) {
			$_POST['warranty_valid'] = 1;
			display_notification(sprintf(_('Warranty is VALID. Expires: %s (%d days remaining).'),
				$validity['warranty_end'], $validity['days_remaining']));
		} else {
			$_POST['warranty_valid'] = 0;
			if ($validity['warranty_end'])
				display_warning(sprintf(_('Warranty EXPIRED on %s (%d days ago).'),
					$validity['warranty_end'], abs($validity['days_remaining'])));
			else
				display_warning(_('No warranty information found for this serial number.'));
		}
	}
	$Ajax->activate('_page_body');
}

//----------------------------------------------------------------------
// Handle Add / Update
//----------------------------------------------------------------------
if ($Mode == 'ADD_ITEM' || $Mode == 'UPDATE_ITEM') {

	$input_error = 0;

	if (!get_post('stock_id')) {
		$input_error = 1;
		display_error(_('Please select an item.'));
	}
	if (!get_post('customer_id') || get_post('customer_id') <= 0) {
		$input_error = 1;
		display_error(_('Please select a customer.'));
	}
	if (strlen(trim(get_post('issue_description'))) == 0) {
		$input_error = 1;
		display_error(_('Please describe the issue.'));
	}

	if ($input_error != 1) {
		$serial_id = get_post('serial_id') ? (int)get_post('serial_id') : null;
		$batch_id = get_post('batch_id') ? (int)get_post('batch_id') : null;
		$assigned_to = get_post('assigned_to') ? (int)get_post('assigned_to') : null;

		if ($Mode == 'ADD_ITEM') {
			$reference = get_next_warranty_claim_reference();
			$id = add_warranty_claim(
				$reference,
				get_post('stock_id'),
				(int)get_post('customer_id'),
				get_post('claim_date'),
				get_post('issue_type'),
				get_post('issue_description'),
				$serial_id,
				$batch_id,
				check_value('warranty_valid'),
				check_value('is_chargeable'),
				$assigned_to,
				get_post('notes')
			);
			display_notification(sprintf(_('Warranty claim %s has been created.'), $reference));
		} else {
			update_warranty_claim(
				$selected_id,
				get_post('stock_id'),
				(int)get_post('customer_id'),
				get_post('claim_date'),
				get_post('issue_type'),
				get_post('issue_description'),
				$serial_id,
				$batch_id,
				check_value('warranty_valid'),
				check_value('is_chargeable'),
				$assigned_to,
				get_post('notes')
			);
			display_notification(_('Warranty claim has been updated.'));
		}
		$Mode = 'RESET';
	}
}

if ($Mode == 'RESET') {
	$selected_id = -1;
	unset($_POST['stock_id']);
	unset($_POST['customer_id']);
	unset($_POST['serial_id']);
	unset($_POST['batch_id']);
	unset($_POST['claim_date']);
	unset($_POST['issue_type']);
	unset($_POST['issue_description']);
	unset($_POST['warranty_valid']);
	unset($_POST['is_chargeable']);
	unset($_POST['assigned_to']);
	unset($_POST['notes']);
}

//----------------------------------------------------------------------
// View claim detail mode
//----------------------------------------------------------------------
$viewing_claim = null;
if (get_post('view_claim') || isset($_GET['id'])) {
	$view_id = get_post('view_claim') ? (int)get_post('view_claim_id') : (int)$_GET['id'];
	$viewing_claim = get_warranty_claim($view_id);
}

start_form();

//----------------------------------------------------------------------
// Claim Detail View
//----------------------------------------------------------------------
if ($viewing_claim) {
	$claim = $viewing_claim;

	echo '<div style="margin-bottom:15px;">';
	echo '<a href="' . $_SERVER['PHP_SELF'] . '">&laquo; ' . _('Back to Claims List') . '</a>';
	echo '</div>';

	// Header
	echo '<div style="margin-bottom:15px;">';
	echo '<h3 style="margin:0 0 5px 0;">' . $claim['reference'] . ' — '
		. warranty_claim_status_badge($claim['status']) . '</h3>';
	echo '<span style="color:#666;">' . $claim['item_description'] . ' (' . $claim['stock_id'] . ')</span>';
	echo '</div>';

	// Detail table
	start_table(TABLESTYLE, "width='80%'");
	$th = array(_('Field'), _('Value'));
	table_header($th);

	label_row(_('Reference'), $claim['reference']);
	label_row(_('Customer'), $claim['customer_name']);
	label_row(_('Item'), $claim['item_description'] . ' (' . $claim['stock_id'] . ')');
	if ($claim['serial_no'])
		label_row(_('Serial Number'), $claim['serial_no']);
	if ($claim['batch_no'])
		label_row(_('Batch Number'), $claim['batch_no']);
	label_row(_('Claim Date'), sql2date($claim['claim_date']));
	label_row(_('Issue Type'), get_warranty_issue_type_label($claim['issue_type']));
	label_row(_('Issue Description'), nl2br(htmlspecialchars($claim['issue_description'])));
	label_row(_('Warranty Valid'), $claim['warranty_valid'] ? _('Yes') : _('No'));
	label_row(_('Chargeable'), $claim['is_chargeable'] ? _('Yes') : _('No'));
	label_row(_('Status'), warranty_claim_status_badge($claim['status']));
	if ($claim['assigned_to_name'])
		label_row(_('Assigned To'), $claim['assigned_to_name']);
	if ($claim['resolution_description'])
		label_row(_('Resolution'), nl2br(htmlspecialchars($claim['resolution_description'])));
	if ($claim['resolution_date'])
		label_row(_('Resolution Date'), sql2date($claim['resolution_date']));
	label_row(_('Repair Cost'), price_format($claim['repair_cost']));
	if ($claim['replacement_serial_no'])
		label_row(_('Replacement Serial'), $claim['replacement_serial_no']);
	if ($claim['notes'])
		label_row(_('Notes'), nl2br(htmlspecialchars($claim['notes'])));
	label_row(_('Created'), sql2date($claim['created_at']) . ' ' . _('by') . ' ' . $claim['created_by_name']);
	end_table(1);

	// Repair Parts
	echo '<h3>' . _('Repair Parts') . '</h3>';
	start_table(TABLESTYLE, "width='80%'");
	$th = array(_('Part'), _('Description'), _('Qty'), _('Unit Cost'), _('Total'), '');
	table_header($th);

	$parts = get_warranty_claim_parts($claim['id']);
	$total_cost = 0;
	while ($part = db_fetch($parts)) {
		alt_table_row_color($k);
		label_cell($part['stock_id']);
		label_cell($part['part_description']);
		qty_cell($part['quantity']);
		amount_cell($part['unit_cost']);
		amount_cell($part['quantity'] * $part['unit_cost']);
		$total_cost += $part['quantity'] * $part['unit_cost'];
		// Delete part button (only if claim not closed)
		if (!in_array($claim['status'], array('closed', 'resolved', 'rejected'))) {
			echo '<td>';
			echo '<form method="post">';
			echo '<input type="hidden" name="delete_part_id" value="' . $part['id'] . '">';
			echo '<input type="hidden" name="part_claim_id_del" value="' . $claim['id'] . '">';
			submit('delete_part', _('Remove'), true, _('Remove this part'), 'fa-times');
			echo '</form>';
			echo '</td>';
		} else {
			label_cell('');
		}
		end_row();
	}
	// Total row
	label_cell('<b>' . _('Total') . '</b>', "colspan=4 align='right'");
	amount_cell($total_cost);
	label_cell('');
	end_row();
	end_table(1);

	// Add Part form (if claim is in_repair or acknowledged)
	if (in_array($claim['status'], array('open', 'acknowledged', 'in_repair'))) {
		echo '<h4>' . _('Add Repair Part') . '</h4>';
		start_table(TABLESTYLE2);
		stock_items_list_cells(_('Part Item'), 'part_stock_id', null, false, false, true);
		end_row();
		small_amount_row(_('Quantity'), 'part_qty', 1);
		small_amount_row(_('Unit Cost'), 'part_cost', 0);
		hidden('part_claim_id', $claim['id']);
		end_table(1);
		submit_center('add_part', _('Add Part'), true, '', 'fa-plus');
	}

	// Status Change form
	$allowed_transitions = array(
		'open'         => array('acknowledged', 'rejected', 'closed'),
		'acknowledged' => array('in_repair', 'rejected', 'closed'),
		'in_repair'    => array('resolved', 'replaced', 'rejected', 'closed'),
		'replaced'     => array('closed'),
		'resolved'     => array('closed'),
		'rejected'     => array('closed'),
	);

	$current = $claim['status'];
	if (isset($allowed_transitions[$current])) {
		echo '<h3>' . _('Change Status') . '</h3>';
		start_table(TABLESTYLE2);

		$options = array();
		foreach ($allowed_transitions[$current] as $st) {
			$options[$st] = get_warranty_claim_status_label($st);
		}
		array_selector_row(_('New Status'), 'new_status', null, $options);
		textarea_row(_('Resolution Notes'), 'resolution_description', '', 40, 3);
		date_row(_('Resolution Date'), 'resolution_date', '', true, 0, 0, 1001);
		small_amount_row(_('Override Repair Cost'), 'repair_cost_input', price_format($claim['repair_cost']));
		hidden('status_claim_id', $claim['id']);
		end_table(1);
		submit_center('change_status', _('Update Status'), true, '', 'fa-check');
	}

	end_form();
	end_page();
	exit;
}

//----------------------------------------------------------------------
// Filter Section
//----------------------------------------------------------------------
echo '<div style="margin-bottom:10px;">';
start_table(TABLESTYLE_NOBORDER);
start_row();
stock_items_list_cells(null, 'filter_stock_id', null, _('All Items'), true, true);
customer_list_cells(_('Customer:'), 'filter_customer_id', null, _('All'), true);
$status_options = array_merge(array('' => _('All Statuses')), get_warranty_claim_statuses());
label_cell(_('Status:'));
echo '<td>';
echo array_selector('filter_status', get_post('filter_status'), $status_options,
	array('select_submit' => true));
echo '</td>';
end_row();
end_table();

start_table(TABLESTYLE_NOBORDER);
start_row();
date_cells(_('From:'), 'filter_date_from', '', true, 0, 0, 1001);
date_cells(_('To:'), 'filter_date_to', '', true, 0, 0, 1001);
submit_cells('search', _('Search'), '', _('Search warranty claims'), 'default');
end_row();
end_table();
echo '</div>';

//----------------------------------------------------------------------
// Summary Cards
//----------------------------------------------------------------------
$summary = get_warranty_claim_summary();
$all_statuses = get_warranty_claim_statuses();

echo '<div style="display:flex;gap:10px;margin-bottom:15px;flex-wrap:wrap;">';
foreach ($all_statuses as $status_code => $status_label) {
	$cnt = isset($summary[$status_code]) ? $summary[$status_code] : 0;
	$color = get_warranty_claim_status_color($status_code);
	echo '<div style="flex:1;min-width:100px;padding:10px;border-radius:6px;text-align:center;'
		. 'background-color:' . $color . '15;border:1px solid ' . $color . '40;">';
	echo '<div style="font-size:20px;font-weight:bold;color:' . $color . ';">' . $cnt . '</div>';
	echo '<div style="font-size:11px;color:' . $color . ';">' . $status_label . '</div>';
	echo '</div>';
}
echo '</div>';

//----------------------------------------------------------------------
// Claims List
//----------------------------------------------------------------------
div_start('claims_list');

$filter_stock = get_post('filter_stock_id');
$filter_customer = get_post('filter_customer_id') ? (int)get_post('filter_customer_id') : 0;
$filter_status = get_post('filter_status');
$filter_from = get_post('filter_date_from');
$filter_to = get_post('filter_date_to');

$result = get_warranty_claims($filter_stock, $filter_customer, $filter_status, $filter_from, $filter_to);

start_table(TABLESTYLE, "width='100%'");
$th = array(_('#'), _('Reference'), _('Item'), _('Customer'), _('Serial/Batch'),
	_('Date'), _('Issue'), _('Status'), _('Cost'), _('Assigned'), '');
table_header($th);

$k = 0;
while ($row = db_fetch($result)) {
	alt_table_row_color($k);

	label_cell($row['id']);

	// Reference as link to detail view
	$link = '<a href="' . $_SERVER['PHP_SELF'] . '?id=' . $row['id'] . '">' . $row['reference'] . '</a>';
	label_cell($link);

	label_cell($row['item_description'] . ' (' . $row['stock_id'] . ')');
	label_cell($row['customer_name']);

	// Serial / Batch
	$tracking = '';
	if ($row['serial_no']) $tracking .= _('SN:') . ' ' . $row['serial_no'];
	if ($row['batch_no']) $tracking .= ($tracking ? ' / ' : '') . _('Batch:') . ' ' . $row['batch_no'];
	label_cell($tracking ? $tracking : '-');

	label_cell(sql2date($row['claim_date']));
	label_cell(get_warranty_issue_type_label($row['issue_type']));
	label_cell(warranty_claim_status_badge($row['status']));
	amount_cell($row['repair_cost']);
	label_cell($row['assigned_to_name'] ? $row['assigned_to_name'] : '-');

	// Actions
	echo '<td>';
	$edit_ok = in_array($row['status'], array('open', 'acknowledged'));
	if ($edit_ok) {
		edit_button_cell("Edit" . $row['id'], _('Edit'));
	}
	$del_ok = can_delete_warranty_claim($row['id']);
	if ($del_ok === true) {
		delete_button_cell("Delete" . $row['id'], _('Delete'));
	}
	echo '</td>';
	end_row();
}
end_table(1);

div_end();

//----------------------------------------------------------------------
// Add / Edit Form
//----------------------------------------------------------------------
start_table(TABLESTYLE2);

$is_edit = ($selected_id != -1 && $Mode == 'Edit');
if ($is_edit) {
	$claim = get_warranty_claim($selected_id);
	if ($claim) {
		$_POST['stock_id'] = $claim['stock_id'];
		$_POST['customer_id'] = $claim['customer_id'];
		$_POST['serial_id'] = $claim['serial_id'];
		$_POST['batch_id'] = $claim['batch_id'];
		$_POST['claim_date'] = sql2date($claim['claim_date']);
		$_POST['issue_type'] = $claim['issue_type'];
		$_POST['issue_description'] = $claim['issue_description'];
		$_POST['warranty_valid'] = $claim['warranty_valid'];
		$_POST['is_chargeable'] = $claim['is_chargeable'];
		$_POST['assigned_to'] = $claim['assigned_to'];
		$_POST['notes'] = $claim['notes'];
	}
}

if (!isset($_POST['claim_date']))
	$_POST['claim_date'] = Today();

label_cell(_('Select Item:'));
stock_items_list_cells(null, 'stock_id', null, false, false, true);

customer_list_row(_('Customer:'), 'customer_id', null, false, true);

// Serial number selector — manual text input with ID
text_row(_('Serial # ID (numeric):'), 'serial_id', get_post('serial_id'), 15, 11);
text_row(_('Batch ID (numeric):'), 'batch_id', get_post('batch_id'), 15, 11);

submit_row('check_warranty', _('Check Warranty Validity'), true, '', '', false);

date_row(_('Claim Date:'), 'claim_date');

$issue_types = get_warranty_issue_types();
array_selector_row(_('Issue Type:'), 'issue_type', get_post('issue_type'), $issue_types);

textarea_row(_('Issue Description:'), 'issue_description', get_post('issue_description'), 50, 4);

check_row(_('Warranty Valid:'), 'warranty_valid', get_post('warranty_valid'));
check_row(_('Chargeable to Customer:'), 'is_chargeable', get_post('is_chargeable'));

users_list_row(_('Assigned To:'), 'assigned_to', get_post('assigned_to'), true, true);

textarea_row(_('Notes:'), 'notes', get_post('notes'), 50, 3);

end_table(1);

submit_add_or_update_center($selected_id == -1, '', 'both');

end_form();
end_page();
