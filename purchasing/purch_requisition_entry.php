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

$page_security = 'SA_PURCHREQUISITION';
$path_to_root = '..';

include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/includes/date_functions.inc');
include_once($path_to_root . '/includes/data_checks.inc');
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/includes/ui/ui_lists.inc');
include_once($path_to_root . '/purchasing/includes/purchasing_db.inc');

$js = '';
if ($SysPrefs->use_popup_windows && $SysPrefs->use_popup_search)
	$js .= get_js_open_window(900, 600);
if (user_use_date_picker())
	$js .= get_js_date_picker();

page(_($help_context = 'Purchase Requisition Entry'), false, false, '', $js);

/**
 * Validate the requisition header form.
 *
 * @return bool
 */
function can_save_purchase_requisition_header()
{
	if (!is_date(get_post('request_date'))) {
		display_error(_('The request date is invalid.'));
		set_focus('request_date');
		return false;
	}

	if (get_post('required_date') !== '' && !is_date(get_post('required_date'))) {
		display_error(_('The required date is invalid.'));
		set_focus('required_date');
		return false;
	}

	if (get_post('requester_id') === '' || get_post('requester_id') == ALL_TEXT) {
		display_error(_('You must select a requester.'));
		set_focus('requester_id');
		return false;
	}

	if (get_post('location') === '' || get_post('location') == ALL_TEXT) {
		display_error(_('You must select a location.'));
		set_focus('location');
		return false;
	}

	return true;
}

/**
 * Validate the requisition line form.
 *
 * @return bool
 */
function can_save_purchase_requisition_line()
{
	if (get_post('line_stock_id') === '') {
		display_error(_('You must select an item.'));
		set_focus('line_stock_id');
		return false;
	}

	if (!check_num('line_quantity', 0)) {
		display_error(_('The quantity must be greater than zero.'));
		set_focus('line_quantity');
		return false;
	}

	if (!check_num('line_estimated_price', 0)) {
		display_error(_('The estimated unit price must be numeric.'));
		set_focus('line_estimated_price');
		return false;
	}

	return true;
}

$selected_id = get_post('selected_id', 0);
if (!$selected_id && isset($_GET['requisition_id']))
	$selected_id = (int)$_GET['requisition_id'];
if (isset($_GET['New']))
	$selected_id = 0;

if (isset($_POST['CreateFromMaterialRequest'])) {
	$material_request_id = (int)get_post('material_request_id');
	if ($material_request_id <= 0) {
		display_error(_('Enter a valid material request ID to bridge.'));
	} else {
		$bridged_requisition_id = create_requisition_from_material_request($material_request_id);
		if ($bridged_requisition_id) {
			meta_forward($_SERVER['PHP_SELF'], 'requisition_id=' . $bridged_requisition_id . get_sel_app_param());
		} else {
			display_error(_('The selected material request could not be converted into a purchase requisition.'));
		}
	}
}

if ((isset($_POST['ADD_ITEM']) || isset($_POST['UPDATE_ITEM'])) && can_save_purchase_requisition_header()) {
	$request_date = date2sql(get_post('request_date'));
	$required_date = get_post('required_date') !== '' ? date2sql(get_post('required_date')) : null;
	$requester_id = (int)get_post('requester_id');
	$department_id = (int)get_post('department_id');
	$priority = get_post('priority') !== '' ? get_post('priority') : 'normal';
	$purpose = get_post('purpose');
	$notes = get_post('notes');
	$location = get_post('location');
	$reference = trim(get_post('reference'));
	$dimension_id = (int)get_post('dimension_id');
	$dimension2_id = (int)get_post('dimension2_id');

	if (isset($_POST['ADD_ITEM'])) {
		$selected_id = add_purch_requisition(
			$requester_id,
			$department_id,
			$required_date,
			$purpose,
			$location,
			$priority,
			$notes,
			$dimension_id,
			$dimension2_id,
			$reference,
			$request_date
		);

		display_notification(_('Purchase requisition has been created.'));
		meta_forward($_SERVER['PHP_SELF'], 'requisition_id=' . $selected_id . get_sel_app_param());
	} else {
		if (update_purch_requisition(
			$selected_id,
			$department_id,
			$required_date,
			$purpose,
			$location,
			$priority,
			$notes,
			$dimension_id,
			$dimension2_id,
			$reference,
			$request_date
		)) {
			display_notification(_('Purchase requisition has been updated.'));
		} else {
			display_error(_('The purchase requisition could not be updated.'));
		}
	}
}

if (isset($_POST['AddLine']) && $selected_id > 0 && can_save_purchase_requisition_line()) {
	$preferred_supplier_id = get_post('line_preferred_supplier_id') == ALL_TEXT ? 0 : (int)get_post('line_preferred_supplier_id');
	$line_error_message = '';
	$line_id = add_requisition_line(
		$selected_id,
		get_post('line_stock_id'),
		input_num('line_quantity'),
		input_num('line_estimated_price'),
		$preferred_supplier_id,
		trim(get_post('line_description')),
		trim(get_post('line_unit_of_measure')),
		trim(get_post('line_notes')),
		array(),
		$line_error_message
	);

	if ($line_id) {
		display_notification(_('Line has been added to the purchase requisition.'));
	} else {
		display_error($line_error_message !== ''
			? $line_error_message
			: _('The purchase requisition line could not be added.'));
	}
}

$update_line_id = find_submit('UpdateLine');
if ($update_line_id > 0 && $selected_id > 0) {
	$preferred_supplier_id = get_post('edit_supplier_' . $update_line_id) == ALL_TEXT ? 0 : (int)get_post('edit_supplier_' . $update_line_id);
	$updated = update_requisition_line(
		$update_line_id,
		get_post('edit_stock_' . $update_line_id),
		input_num('edit_quantity_' . $update_line_id),
		input_num('edit_estimated_price_' . $update_line_id),
		$preferred_supplier_id,
		trim(get_post('edit_description_' . $update_line_id)),
		trim(get_post('edit_uom_' . $update_line_id)),
		trim(get_post('edit_notes_' . $update_line_id))
	);

	if ($updated)
		display_notification(_('Purchase requisition line has been updated.'));
	else
		display_error(_('The purchase requisition line could not be updated.'));
}

$delete_line_id = find_submit('DeleteLine');
if ($delete_line_id > 0 && $selected_id > 0) {
	if (delete_requisition_line($delete_line_id))
		display_notification(_('Purchase requisition line has been deleted.'));
	else
		display_error(_('The purchase requisition line could not be deleted.'));
}

if (isset($_POST['SubmitRequisition']) && $selected_id > 0) {
	$result = submit_requisition($selected_id);
	if ($result === false) {
		display_error(_('The purchase requisition could not be submitted.'));
	} elseif (is_array($result) && isset($result['status']) && $result['status'] === 'pending') {
		display_notification_centered(sprintf(
			_('Purchase requisition has been submitted for approval. Draft #%d, Reference: %s'),
			$result['draft_id'],
			$result['reference']
		));
		hyperlink_no_params('admin/approval_dashboard.php', _('Go to Approval Dashboard'));
	} elseif (is_array($result) && isset($result['status']) && $result['status'] === 'auto_approved') {
		display_notification(_('Purchase requisition has been auto-approved.'));
	} else {
		display_notification(_('Purchase requisition has been submitted.'));
	}
}

if (isset($_POST['ApproveRequisition']) && $selected_id > 0) {
	if (approve_requisition($selected_id, (int)$_SESSION['wa_current_user']->user))
		display_notification(_('Purchase requisition has been approved.'));
	else
		display_error(_('The purchase requisition could not be approved.'));
}

if (isset($_POST['RejectRequisition']) && $selected_id > 0) {
	$rejection_reason = trim(get_post('rejection_reason'));
	if ($rejection_reason === '') {
		display_error(_('Enter a rejection reason.'));
		set_focus('rejection_reason');
	} elseif (reject_requisition($selected_id, (int)$_SESSION['wa_current_user']->user, $rejection_reason)) {
		display_notification(_('Purchase requisition has been rejected.'));
	} else {
		display_error(_('The purchase requisition could not be rejected.'));
	}
}

if (isset($_POST['DeleteRequisition']) && $selected_id > 0) {
	if (delete_purch_requisition($selected_id)) {
		display_notification(_('Purchase requisition has been deleted.'));
		$selected_id = 0;
	} else {
		display_error(_('Only draft purchase requisitions can be deleted.'));
	}
}

if (isset($_POST['CreatePO']) && $selected_id > 0) {
	$supplier_id = get_post('create_po_supplier') == ALL_TEXT ? 0 : (int)get_post('create_po_supplier');
	$po_numbers = create_po_from_requisition($selected_id, $supplier_id);
	if (!$po_numbers) {
		display_error(_('No purchase order could be created from this requisition.'));
	} else {
		display_notification(sprintf(_('Created %d purchase order(s) from this requisition.'), count($po_numbers)));
		foreach ($po_numbers as $po_number)
			display_note(get_trans_view_str(ST_PURCHORDER, $po_number, _('View Purchase Order') . ' #' . $po_number), 0, 1);
	}
}

if (isset($_POST['CreateRFQ']) && $selected_id > 0) {
	$rfq_id = create_rfq_from_requisition($selected_id);
	if ($rfq_id) {
		display_notification(_('Purchase RFQ has been created from this requisition.'));
		meta_forward($path_to_root . '/purchasing/purch_rfq_entry.php', 'rfq_id=' . $rfq_id);
	} else {
		display_error(_('No RFQ could be created from this requisition.'));
	}
}

$requisition = $selected_id > 0 ? get_purch_requisition($selected_id) : false;
$editable = !$requisition || in_array($requisition['status'], get_purch_requisition_editable_statuses());

if ($requisition) {
	$_POST['request_date'] = sql2date($requisition['request_date']);
	$_POST['required_date'] = $requisition['required_date'] ? sql2date($requisition['required_date']) : '';
	$_POST['requester_id'] = $requisition['requester_id'];
	$_POST['department_id'] = $requisition['department_id'];
	$_POST['priority'] = $requisition['priority'];
	$_POST['purpose'] = $requisition['purpose'];
	$_POST['notes'] = $requisition['notes'];
	$_POST['location'] = $requisition['location'];
	$_POST['reference'] = $requisition['reference'];
	$_POST['dimension_id'] = $requisition['dimension_id'];
	$_POST['dimension2_id'] = $requisition['dimension2_id'];
} else {
	if (!isset($_POST['request_date']))
		$_POST['request_date'] = Today();
	if (!isset($_POST['requester_id']))
		$_POST['requester_id'] = $_SESSION['wa_current_user']->user;
	if (!isset($_POST['priority']))
		$_POST['priority'] = 'normal';
	if (!isset($_POST['dimension_id']))
		$_POST['dimension_id'] = 0;
	if (!isset($_POST['dimension2_id']))
		$_POST['dimension2_id'] = 0;
}

$priorities = get_purch_requisition_priorities();

start_form();

echo '<div style="display:flex;justify-content:space-between;align-items:center;gap:12px;flex-wrap:wrap;">';
echo '<div>';
echo '<h2 style="margin:0;">' . ($requisition ? htmlspecialchars($requisition['reference']) : _('New Purchase Requisition')) . '</h2>';
if ($requisition) {
	echo '<div style="margin-top:6px;">' . purch_requisition_status_badge($requisition['status']) . ' ';
	echo purch_requisition_priority_badge($requisition['priority']) . '</div>';
	if (!empty($requisition['rejection_reason']))
		echo '<div style="margin-top:8px;color:#dc3545;">' . _('Rejection Reason: ') . htmlspecialchars($requisition['rejection_reason']) . '</div>';
}
echo '</div>';
echo '<div>';
hyperlink_params($path_to_root . '/purchasing/inquiry/purch_requisitions_view.php', _('Back to Requisition Inquiry'), ltrim(get_sel_app_param(), '&'));
echo '</div>';
echo '</div>';

br();

if (!$requisition) {
	display_note(_('Save the requisition header first, then add line items and submit it for approval.'), 0, 1);
}

start_table(TABLESTYLE2, "width='100%'");

if ($editable) {
	users_list_row(_('Requester:'), 'requester_id', get_post('requester_id'), false, false);
	departments_list_row(_('Department:'), 'department_id', get_post('department_id'), false, _('Select department'));
	date_row(_('Request Date:'), 'request_date');
	date_row(_('Required Date:'), 'required_date');
	locations_list_row(_('Deliver To Location:'), 'location', get_post('location'));
	echo '<tr><td class="label">' . _('Priority:') . '</td><td>';
	echo array_selector('priority', get_post('priority'), $priorities, array('class' => array('nosearch')));
	echo '</td></tr>';
	text_row(_('Reference:'), 'reference', get_post('reference'), 20, 60);
	dimensions_list_row(_('Dimension:'), 'dimension_id', get_post('dimension_id'), true);
	dimensions_list_row(_('Dimension 2:'), 'dimension2_id', get_post('dimension2_id'), true);
	text_row(_('Purpose:'), 'purpose', get_post('purpose'), 50, 255);
	textarea_row(_('Notes:'), 'notes', get_post('notes'), 35, 4);
} else {
	label_row(_('Requester:'), $requisition['requester_name']);
	label_row(_('Department:'), $requisition['department_name'] ? $requisition['department_name'] : '-');
	label_row(_('Request Date:'), sql2date($requisition['request_date']));
	label_row(_('Required Date:'), $requisition['required_date'] ? sql2date($requisition['required_date']) : '-');
	label_row(_('Location:'), $requisition['location_name']);
	label_row(_('Priority:'), purch_requisition_priority_badge($requisition['priority']));
	label_row(_('Reference:'), $requisition['reference']);
	label_row(_('Purpose:'), $requisition['purpose'] ? $requisition['purpose'] : '-');
	label_row(_('Notes:'), $requisition['notes'] ? nl2br(htmlspecialchars($requisition['notes'])) : '-');
	label_row(_('Estimated Total:'), price_format($requisition['total_estimated']));
	if (!empty($requisition['material_request_id']))
		label_row(_('Linked Material Request:'), format_material_request_no($requisition['material_request_no']));
}

end_table(1);

hidden('selected_id', $selected_id);

if ($editable) {
	if ($requisition)
		submit_center('UPDATE_ITEM', _('Update Purchase Requisition'), true, '', 'default');
	else
		submit_center('ADD_ITEM', _('Create Purchase Requisition'), true, '', 'default');
}

if ($requisition) {
	$user_price_decimal_places = user_price_dec();
	display_heading(_('Line Items'));
	start_table(TABLESTYLE, "width='100%'");
	$th = array(_('Item'), _('Description'), _('Quantity'), _('Qty Ordered'), _('Estimated Price'), _('Preferred Supplier'), _('Status'), _('Notes'));
	if ($editable)
		$th[] = '';
	table_header($th);

	$k = 0;
	$lines_result = get_requisition_lines($selected_id);
	while ($line = db_fetch($lines_result)) {
		alt_table_row_color($k);

		if ($editable) {
			$edit_preferred_supplier_id = (int)$line['preferred_supplier_id'];
			label_cell($line['stock_id']);
			echo '<td><input type="text" name="edit_description_' . $line['id'] . '" value="'
				. htmlspecialchars($line['line_description'], ENT_QUOTES, 'UTF-8') . '" size="24"></td>';
			echo '<td><input type="text" name="edit_quantity_' . $line['id'] . '" value="'
				. number_format2($line['quantity'], get_qty_dec($line['stock_id'])) . '" size="8" class="amount"></td>';
			qty_cell($line['qty_ordered']);
			echo '<td><input type="text" name="edit_estimated_price_' . $line['id'] . '" value="'
				. price_decimal_format($line['estimated_unit_price'], $user_price_decimal_places) . '" size="8" class="amount"></td>';
			echo '<td>' . supplier_list('edit_supplier_' . $line['id'], $edit_preferred_supplier_id, true, false, true) . '</td>';
			label_cell(purch_requisition_status_badge($line['status']));
			echo '<td><input type="text" name="edit_notes_' . $line['id'] . '" value="'
				. htmlspecialchars($line['notes'] !== null ? $line['notes'] : '', ENT_QUOTES, 'UTF-8') . '" size="20"></td>';
			echo '<td nowrap>';
			hidden('edit_stock_' . $line['id'], $line['stock_id']);
			hidden('edit_uom_' . $line['id'], $line['line_unit_of_measure']);
			submit('UpdateLine' . $line['id'], _('Update'), true, _('Update this line'));
			echo '&nbsp;';
			submit('DeleteLine' . $line['id'], _('Delete'), true, _('Delete this line'));
			echo '</td>';
		} else {
			label_cell($line['stock_id']);
			label_cell($line['line_description']);
			qty_cell($line['quantity']);
			qty_cell($line['qty_ordered']);
			amount_cell($line['estimated_unit_price']);
			label_cell($line['preferred_supplier_name'] ? $line['preferred_supplier_name'] : '-');
			label_cell(purch_requisition_status_badge($line['status']));
			label_cell($line['notes'] ? $line['notes'] : '-');
		}

		end_row();
	}

	if ($k == 0)
		label_row('', _('No line items have been added yet.'), 'colspan=' . ($editable ? 9 : 8) . ' align=center');

	end_table(1);

	if ($editable) {
		display_heading(_('Add Line'));
		start_table(TABLESTYLE2);
		start_row();
		label_cell(_('Select Item:'));
		stock_costable_items_list_cells(null, 'line_stock_id', null, false, true);
		end_row();
		text_row(_('Description:'), 'line_description', null, 40, 255);
		qty_row(_('Quantity:'), 'line_quantity', null, null, null, get_qty_dec(''));
		amount_row(_('Estimated Unit Price:'), 'line_estimated_price');
		supplier_list_row(_('Preferred Supplier:'), 'line_preferred_supplier_id', null, true, false, true);
		text_row(_('Unit of Measure:'), 'line_unit_of_measure', null, 20, 20);
		text_row(_('Notes:'), 'line_notes', null, 40, 255);
		end_table(1);
		submit_center('AddLine', _('Add Line'), true, '', 'default');
	}

	br();

	echo '<div style="text-align:center;margin-top:18px;">';
	if ($requisition['status'] === 'draft' || $requisition['status'] === 'rejected') {
		submit('SubmitRequisition', _('Submit for Approval'), true, _('Submit this requisition'));
		echo '&nbsp;';
		if ($requisition['status'] === 'draft')
			submit('DeleteRequisition', _('Delete Requisition'), true, _('Delete this requisition'));
	} elseif ($requisition['status'] === 'submitted') {
		submit('ApproveRequisition', _('Approve'), true, _('Approve this requisition'));
		echo '&nbsp;';
		submit('RejectRequisition', _('Reject'), true, _('Reject this requisition'));
		echo '<div style="max-width:420px;margin:12px auto 0;">';
		text_row(_('Rejection Reason:'), 'rejection_reason', null, 40, 255);
		echo '</div>';
	} elseif (in_array($requisition['status'], array('approved', 'partially_ordered'))) {
		start_table(TABLESTYLE_NOBORDER, "style='margin:0 auto;' width='420'");
		supplier_list_row(_('Create PO For Supplier:'), 'create_po_supplier', null, true, false, true);
		end_table(1);
		submit('CreatePO', _('Create Purchase Order(s)'), true, _('Create purchase order from this requisition'));
		echo '&nbsp;';
		submit('CreateRFQ', _('Create RFQ'), true, _('Create RFQ from this requisition'));
	}
	echo '</div>';
}

if (!$requisition) {
	br();
	display_heading(_('Bridge Existing Material Request'));
	start_table(TABLESTYLE2, "width='50%'");
	text_row(_('Material Request ID:'), 'material_request_id', null, 10, 10);
	end_table(1);
	submit_center('CreateFromMaterialRequest', _('Create from Material Request'), true, '', 'default');
}

end_form();
end_page();