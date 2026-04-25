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

$page_security = 'SA_PURCHRFQ';
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

page(_($help_context = 'Purchase RFQ Entry'), false, false, '', $js);

/**
 * Validate the RFQ header form.
 *
 * @return bool
 */
function can_save_purchase_rfq_header()
{
	if (!is_date(get_post('created_date'))) {
		display_error(_('The RFQ date is invalid.'));
		set_focus('created_date');
		return false;
	}

	if (get_post('deadline_date') !== '' && !is_date(get_post('deadline_date'))) {
		display_error(_('The deadline date is invalid.'));
		set_focus('deadline_date');
		return false;
	}

	if (get_post('validity_date') !== '' && !is_date(get_post('validity_date'))) {
		display_error(_('The validity date is invalid.'));
		set_focus('validity_date');
		return false;
	}

	if (get_post('required_delivery_date') !== '' && !is_date(get_post('required_delivery_date'))) {
		display_error(_('The required delivery date is invalid.'));
		set_focus('required_delivery_date');
		return false;
	}

	if (trim(get_post('rfq_description')) === '') {
		display_error(_('Enter a short RFQ description.'));
		set_focus('rfq_description');
		return false;
	}

	return true;
}

/**
 * Validate the RFQ item form.
 *
 * @return bool
 */
function can_save_purchase_rfq_item()
{
	if (get_post('rfq_item_stock_id') === '') {
		display_error(_('You must select an item.'));
		set_focus('rfq_item_stock_id');
		return false;
	}

	if (!check_num('rfq_item_quantity', 0)) {
		display_error(_('The quantity must be greater than zero.'));
		set_focus('rfq_item_quantity');
		return false;
	}

	if (!check_num('rfq_item_target_price', 0)) {
		display_error(_('The target price must be numeric.'));
		set_focus('rfq_item_target_price');
		return false;
	}

	return true;
}

/**
 * Parse the posted RFQ response lines for one vendor.
 *
 * @param int $rfq_vendor_id
 * @return array
 */
function get_posted_rfq_response_lines($rfq_vendor_id)
{
	$lines = array();
	$item_ids_csv = get_post('response_item_ids_' . $rfq_vendor_id);
	$item_ids = $item_ids_csv !== '' ? explode(',', $item_ids_csv) : array();

	foreach ($item_ids as $rfq_item_id) {
		$rfq_item_id = (int)$rfq_item_id;
		if ($rfq_item_id <= 0)
			continue;

		$lines[] = array(
			'rfq_item_id' => $rfq_item_id,
			'quoted_price' => input_num('response_price_' . $rfq_vendor_id . '_' . $rfq_item_id),
			'quoted_quantity' => input_num('response_qty_' . $rfq_vendor_id . '_' . $rfq_item_id),
			'delivery_lead_days' => (int)get_post('response_lead_' . $rfq_vendor_id . '_' . $rfq_item_id),
			'notes' => trim(get_post('response_note_' . $rfq_vendor_id . '_' . $rfq_item_id)),
		);
	}

	return $lines;
}

/**
 * Get preferred supplier options from a requisition.
 *
 * @param int $requisition_id
 * @return array
 */
function get_bridge_requisition_supplier_options($requisition_id)
{
	$options = array();
	if ((int)$requisition_id <= 0)
		return $options;

	$lines = get_requisition_lines($requisition_id);
	while ($line = db_fetch($lines)) {
		$supplier_id = (int)$line['preferred_supplier_id'];
		if ($supplier_id <= 0 || isset($options[$supplier_id]))
			continue;

		$options[$supplier_id] = $line['preferred_supplier_name'] ? $line['preferred_supplier_name'] : get_supplier_name($supplier_id);
	}

	return $options;
}

$selected_id = get_post('selected_id', 0);
if (!$selected_id && isset($_GET['rfq_id']))
	$selected_id = (int)$_GET['rfq_id'];
if (isset($_GET['New']))
	$selected_id = 0;

$bridge_requisition_id = (int)get_post('bridge_requisition_id', isset($_GET['requisition_id']) ? (int)$_GET['requisition_id'] : 0);

if (isset($_POST['CreateFromRequisition'])) {
	$bridge_supplier_ids = array();
	if (!empty($_POST['bridge_supplier_id']) && is_array($_POST['bridge_supplier_id'])) {
		foreach ($_POST['bridge_supplier_id'] as $supplier_id) {
			if ((int)$supplier_id > 0)
				$bridge_supplier_ids[] = (int)$supplier_id;
		}
	}

	if ($bridge_requisition_id <= 0) {
		display_error(_('Enter a valid requisition ID to bridge.'));
	} else {
		$created_rfq_id = create_rfq_from_requisition($bridge_requisition_id, $bridge_supplier_ids);
		if ($created_rfq_id) {
			meta_forward($_SERVER['PHP_SELF'], 'rfq_id=' . $created_rfq_id);
		} else {
			display_error(_('The selected requisition could not be converted into an RFQ.'));
		}
	}
}

if ((isset($_POST['ADD_RFQ']) || isset($_POST['UPDATE_RFQ'])) && can_save_purchase_rfq_header()) {
	$created_date = date2sql(get_post('created_date'));
	$deadline_date = get_post('deadline_date') !== '' ? date2sql(get_post('deadline_date')) : null;
	$validity_date = get_post('validity_date') !== '' ? date2sql(get_post('validity_date')) : null;
	$required_delivery_date = get_post('required_delivery_date') !== '' ? date2sql(get_post('required_delivery_date')) : null;
	$requisition_id = get_post('linked_requisition_id') !== '' ? (int)get_post('linked_requisition_id') : 0;
	$rfq_type = get_post('rfq_type') !== '' ? get_post('rfq_type') : 'standard';
	$description = trim(get_post('rfq_description'));
	$notes = trim(get_post('rfq_notes'));
	$terms_and_conditions = trim(get_post('terms_and_conditions'));
	$delivery_location = get_post('delivery_location') === ALL_TEXT ? '' : get_post('delivery_location');
	$evaluation_criteria = trim(get_post('evaluation_criteria'));
	$dimension_id = (int)get_post('dimension_id');
	$dimension2_id = (int)get_post('dimension2_id');
	$reference = trim(get_post('reference'));

	if (isset($_POST['ADD_RFQ'])) {
		$selected_id = add_purch_rfq(
			$rfq_type,
			$description,
			$deadline_date,
			$requisition_id,
			$notes,
			$terms_and_conditions,
			$delivery_location,
			$required_delivery_date,
			$evaluation_criteria,
			$validity_date,
			$dimension_id,
			$dimension2_id,
			$reference,
			$created_date
		);

		display_notification(_('Purchase RFQ has been created.'));
		meta_forward($_SERVER['PHP_SELF'], 'rfq_id=' . $selected_id);
	} else {
		if (update_purch_rfq(
			$selected_id,
			$rfq_type,
			$description,
			$deadline_date,
			$requisition_id,
			$notes,
			$terms_and_conditions,
			$delivery_location,
			$required_delivery_date,
			$evaluation_criteria,
			$validity_date,
			$dimension_id,
			$dimension2_id,
			$reference,
			$created_date
		)) {
			display_notification(_('Purchase RFQ has been updated.'));
		} else {
			display_error(_('The purchase RFQ could not be updated.'));
		}
	}
}

if (isset($_POST['AddRfqItem']) && $selected_id > 0 && can_save_purchase_rfq_item()) {
	$item_id = add_rfq_item(
		$selected_id,
		get_post('rfq_item_stock_id'),
		input_num('rfq_item_quantity'),
		input_num('rfq_item_target_price'),
		trim(get_post('rfq_item_specifications')),
		trim(get_post('rfq_item_description')),
		trim(get_post('rfq_item_uom'))
	);

	if ($item_id)
		display_notification(_('RFQ item has been added.'));
	else
		display_error(_('The RFQ item could not be added.'));
}

$update_item_id = find_submit('UpdateRfqItem');
if ($update_item_id > 0 && $selected_id > 0) {
	$updated = update_rfq_item(
		$update_item_id,
		get_post('edit_rfq_stock_' . $update_item_id),
		input_num('edit_rfq_qty_' . $update_item_id),
		input_num('edit_rfq_price_' . $update_item_id),
		trim(get_post('edit_rfq_specs_' . $update_item_id)),
		trim(get_post('edit_rfq_description_' . $update_item_id)),
		trim(get_post('edit_rfq_uom_' . $update_item_id))
	);

	if ($updated)
		display_notification(_('RFQ item has been updated.'));
	else
		display_error(_('The RFQ item could not be updated.'));
}

$delete_item_id = find_submit('DeleteRfqItem');
if ($delete_item_id > 0 && $selected_id > 0) {
	if (delete_rfq_item($delete_item_id))
		display_notification(_('RFQ item has been deleted.'));
	else
		display_error(_('The RFQ item could not be deleted.'));
}

if (isset($_POST['AddVendor']) && $selected_id > 0) {
	$supplier_id = get_post('add_supplier_id') == ALL_TEXT ? 0 : (int)get_post('add_supplier_id');
	$rfq_vendor_id = add_rfq_vendor($selected_id, $supplier_id);

	if ($rfq_vendor_id)
		display_notification(_('Vendor has been added to the RFQ.'));
	else
		display_error(_('The vendor could not be added to the RFQ.'));
}

$remove_vendor_id = find_submit('RemoveVendor');
if ($remove_vendor_id > 0 && $selected_id > 0) {
	$rfq_vendor = get_purch_rfq_vendor($remove_vendor_id);
	if ($rfq_vendor && remove_rfq_vendor($selected_id, (int)$rfq_vendor['supplier_id']))
		display_notification(_('Vendor has been removed from the RFQ.'));
	else
		display_error(_('The vendor could not be removed from the RFQ.'));
}

$record_vendor_response_id = find_submit('RecordVendorResponse');
if ($record_vendor_response_id > 0 && $selected_id > 0) {
	$rfq_vendor = get_purch_rfq_vendor($record_vendor_response_id);
	if (!$rfq_vendor || (int)$rfq_vendor['rfq_id'] !== (int)$selected_id) {
		display_error(_('The selected vendor response could not be found.'));
	} else {
		$response_saved = record_vendor_response(
			$selected_id,
			(int)$rfq_vendor['supplier_id'],
			get_posted_rfq_response_lines($record_vendor_response_id),
			input_num('response_total_' . $record_vendor_response_id),
			(int)get_post('response_vendor_lead_' . $record_vendor_response_id),
			trim(get_post('response_vendor_notes_' . $record_vendor_response_id)),
			trim(get_post('response_payment_terms_' . $record_vendor_response_id))
		);

		if ($response_saved)
			display_notification(_('Vendor response has been saved.'));
		else
			display_error(_('The vendor response could not be saved.'));
	}
}

if (isset($_POST['SendRFQ']) && $selected_id > 0) {
	if (send_rfq_to_vendors($selected_id))
		display_notification(_('RFQ has been marked as sent to the selected vendors.'));
	else
		display_error(_('The RFQ could not be sent.'));
}

$create_po_vendor_id = find_submit('CreatePOFromVendor');
if ($create_po_vendor_id > 0 && $selected_id > 0) {
	$po_number = create_po_from_rfq($selected_id, $create_po_vendor_id);
	if ($po_number) {
		display_notification(_('Purchase order has been created from the awarded RFQ vendor.'));
		display_note(get_trans_view_str(ST_PURCHORDER, $po_number, _('View Purchase Order') . ' #' . $po_number), 0, 1);
	} else {
		display_error(_('The purchase order could not be created from this RFQ.'));
	}
}

$rfq = $selected_id > 0 ? get_purch_rfq($selected_id) : false;
$editable = !$rfq || in_array($rfq['status'], get_purch_rfq_editable_statuses());

$rfq_items = array();
$rfq_vendors = array();
$rfq_item_ids = array();
if ($rfq) {
	$rfq_items_result = get_rfq_items($selected_id);
	while ($rfq_item = db_fetch($rfq_items_result)) {
		$rfq_items[] = $rfq_item;
		$rfq_item_ids[] = (int)$rfq_item['id'];
	}

	$rfq_vendors_result = get_rfq_vendors($selected_id);
	while ($rfq_vendor = db_fetch($rfq_vendors_result))
		$rfq_vendors[] = $rfq_vendor;
}

$bridge_requisition = $bridge_requisition_id > 0 ? get_purch_requisition($bridge_requisition_id) : false;
$bridge_supplier_options = $bridge_requisition ? get_bridge_requisition_supplier_options($bridge_requisition_id) : array();

if ($rfq) {
	$_POST['created_date'] = sql2date($rfq['created_date']);
	$_POST['deadline_date'] = $rfq['deadline_date'] ? sql2date($rfq['deadline_date']) : '';
	$_POST['validity_date'] = $rfq['validity_date'] ? sql2date($rfq['validity_date']) : '';
	$_POST['required_delivery_date'] = $rfq['required_delivery_date'] ? sql2date($rfq['required_delivery_date']) : '';
	$_POST['rfq_type'] = $rfq['rfq_type'];
	$_POST['rfq_description'] = $rfq['description'];
	$_POST['rfq_notes'] = $rfq['notes'];
	$_POST['terms_and_conditions'] = $rfq['terms_and_conditions'];
	$_POST['delivery_location'] = $rfq['delivery_location'];
	$_POST['evaluation_criteria'] = $rfq['evaluation_criteria'];
	$_POST['dimension_id'] = $rfq['dimension_id'];
	$_POST['dimension2_id'] = $rfq['dimension2_id'];
	$_POST['reference'] = $rfq['reference'];
	$_POST['linked_requisition_id'] = $rfq['requisition_id'];
} else {
	if (!isset($_POST['created_date']))
		$_POST['created_date'] = Today();
	if (!isset($_POST['deadline_date']))
		$_POST['deadline_date'] = add_days(Today(), (int)get_company_pref('rfq_default_deadline_days') > 0 ? (int)get_company_pref('rfq_default_deadline_days') : 14);
	if (!isset($_POST['rfq_type']))
		$_POST['rfq_type'] = 'standard';
	if (!isset($_POST['dimension_id']))
		$_POST['dimension_id'] = 0;
	if (!isset($_POST['dimension2_id']))
		$_POST['dimension2_id'] = 0;
	if (!isset($_POST['linked_requisition_id']))
		$_POST['linked_requisition_id'] = $bridge_requisition_id;
}

$rfq_types = get_purch_rfq_types();
$can_record_vendor_responses = $rfq && in_array($rfq['status'], array('sent', 'received', 'evaluated')) && !empty($rfq_items) && !empty($rfq_vendors);

start_form();

echo '<div style="display:flex;justify-content:space-between;align-items:center;gap:12px;flex-wrap:wrap;">';
echo '<div>';
echo '<h2 style="margin:0;">' . ($rfq ? htmlspecialchars($rfq['reference']) : _('New Purchase RFQ')) . '</h2>';
if ($rfq) {
	echo '<div style="margin-top:6px;">' . purch_rfq_status_badge($rfq['status']) . '</div>';
	if ((int)$rfq['requisition_id'] > 0)
		echo '<div style="margin-top:8px;color:#666;">' . _('Linked Requisition: ') . htmlspecialchars($rfq['requisition_reference']) . '</div>';
	if ((int)$rfq['responded_count'] > 0 || (int)$rfq['winner_count'] > 0)
		echo '<div style="margin-top:8px;color:#666;">' . _('Vendor comparison is available once responses are recorded.') . '</div>';
}
echo '</div>';
echo '<div>';
if ($rfq && ((int)$rfq['responded_count'] > 0 || (int)$rfq['winner_count'] > 0))
	hyperlink_params($path_to_root . '/purchasing/purch_rfq_comparison.php', _('Comparison'), 'rfq_id=' . (int)$selected_id);
echo '&nbsp;';
hyperlink_no_params('purchasing/inquiry/purch_rfq_view.php', _('Back to RFQ Inquiry'));
echo '</div>';
echo '</div>';

br();

if (!$rfq) {
	display_note(_('Save the RFQ header first, then add items, invite vendors, and record responses.'), 0, 1);

	display_heading(_('Create from Approved Requisition'));
	start_table(TABLESTYLE2, "width='60%'");
	text_row(_('Requisition ID:'), 'bridge_requisition_id', $bridge_requisition_id ? $bridge_requisition_id : null, 10, 10);
	if ($bridge_requisition) {
		label_row(_('Requisition Reference:'), $bridge_requisition['reference']);
		label_row(_('Requisition Status:'), purch_requisition_status_badge($bridge_requisition['status']));
		label_row(_('Outstanding Items:'), count($bridge_supplier_options) > 0 ? sprintf(_('%d preferred supplier(s) found'), count($bridge_supplier_options)) : _('No preferred suppliers found on requisition lines.'));

		echo '<tr><td class="label">' . _('Invite Preferred Suppliers:') . '</td><td>';
		if (!empty($bridge_supplier_options)) {
			foreach ($bridge_supplier_options as $supplier_id => $supplier_name) {
				echo '<label style="display:block;margin:2px 0;">';
				echo '<input type="checkbox" name="bridge_supplier_id[]" value="' . (int)$supplier_id . '" checked> ' . htmlspecialchars($supplier_name);
				echo '</label>';
			}
		} else {
			echo '<span>' . _('The RFQ can still be created without preselected vendors.') . '</span>';
		}
		echo '</td></tr>';
	}
	end_table(1);
	submit_center('CreateFromRequisition', _('Create RFQ from Requisition'), true, '', 'default');
	br();
}

start_table(TABLESTYLE2, "width='100%'");

if ($editable) {
	echo '<tr><td class="label">' . _('RFQ Type:') . '</td><td>';
	echo array_selector('rfq_type', get_post('rfq_type'), $rfq_types, array('class' => array('nosearch')));
	echo '</td></tr>';
	date_row(_('RFQ Date:'), 'created_date');
	date_row(_('Deadline Date:'), 'deadline_date');
	date_row(_('Validity Date:'), 'validity_date');
	date_row(_('Required Delivery Date:'), 'required_delivery_date');
	locations_list_row(_('Delivery Location:'), 'delivery_location', get_post('delivery_location'), true);
	text_row(_('Reference:'), 'reference', get_post('reference'), 20, 60);
	text_row(_('Description:'), 'rfq_description', get_post('rfq_description'), 50, 255);
	text_row(_('Linked Requisition ID:'), 'linked_requisition_id', get_post('linked_requisition_id'), 10, 10);
	dimensions_list_row(_('Dimension:'), 'dimension_id', get_post('dimension_id'), true);
	dimensions_list_row(_('Dimension 2:'), 'dimension2_id', get_post('dimension2_id'), true);
	textarea_row(_('Evaluation Criteria:'), 'evaluation_criteria', get_post('evaluation_criteria'), 35, 3);
	textarea_row(_('Terms and Conditions:'), 'terms_and_conditions', get_post('terms_and_conditions'), 35, 4);
	textarea_row(_('Notes:'), 'rfq_notes', get_post('rfq_notes'), 35, 4);
} else {
	label_row(_('RFQ Type:'), $rfq_types[$rfq['rfq_type']]);
	label_row(_('RFQ Date:'), sql2date($rfq['created_date']));
	label_row(_('Deadline Date:'), $rfq['deadline_date'] ? sql2date($rfq['deadline_date']) : '-');
	label_row(_('Validity Date:'), $rfq['validity_date'] ? sql2date($rfq['validity_date']) : '-');
	label_row(_('Required Delivery Date:'), $rfq['required_delivery_date'] ? sql2date($rfq['required_delivery_date']) : '-');
	label_row(_('Delivery Location:'), $rfq['delivery_location_name'] ? $rfq['delivery_location_name'] : '-');
	label_row(_('Reference:'), $rfq['reference']);
	label_row(_('Description:'), $rfq['description'] ? $rfq['description'] : '-');
	if ((int)$rfq['requisition_id'] > 0)
		label_row(_('Linked Requisition:'), '<a href="' . $path_to_root . '/purchasing/purch_requisition_entry.php?requisition_id=' . (int)$rfq['requisition_id'] . '">' . htmlspecialchars($rfq['requisition_reference']) . '</a>');
	label_row(_('Evaluation Criteria:'), $rfq['evaluation_criteria'] ? nl2br(htmlspecialchars($rfq['evaluation_criteria'])) : '-');
	label_row(_('Terms and Conditions:'), $rfq['terms_and_conditions'] ? nl2br(htmlspecialchars($rfq['terms_and_conditions'])) : '-');
	label_row(_('Notes:'), $rfq['notes'] ? nl2br(htmlspecialchars($rfq['notes'])) : '-');
	label_row(_('Target Total:'), price_format($rfq['target_total']));
}

end_table(1);

hidden('selected_id', $selected_id);

if ($editable) {
	if ($rfq)
		submit_center('UPDATE_RFQ', _('Update RFQ'), true, '', 'default');
	else
		submit_center('ADD_RFQ', _('Create RFQ'), true, '', 'default');
}

if ($rfq) {
	display_heading(_('RFQ Items'));
	start_table(TABLESTYLE, "width='100%'");
	$th = array(_('Item'), _('Description'), _('Quantity'), _('Unit'), _('Target Price'), _('Responses'), _('Best Quote'), _('Specifications'));
	if ($editable)
		$th[] = '';
	table_header($th);

	$k = 0;
	foreach ($rfq_items as $rfq_item) {
		alt_table_row_color($k);

		if ($editable) {
			label_cell($rfq_item['stock_id']);
			echo '<td><input type="text" name="edit_rfq_description_' . $rfq_item['id'] . '" value="' . htmlspecialchars($rfq_item['line_description'], ENT_QUOTES, 'UTF-8') . '" size="24"></td>';
			echo '<td><input type="text" name="edit_rfq_qty_' . $rfq_item['id'] . '" value="' . number_format2($rfq_item['quantity'], get_qty_dec($rfq_item['stock_id'])) . '" size="8" class="amount"></td>';
			echo '<td><input type="text" name="edit_rfq_uom_' . $rfq_item['id'] . '" value="' . htmlspecialchars($rfq_item['line_unit_of_measure'], ENT_QUOTES, 'UTF-8') . '" size="10"></td>';
			echo '<td><input type="text" name="edit_rfq_price_' . $rfq_item['id'] . '" value="' . price_decimal_format($rfq_item['target_price'], user_price_dec()) . '" size="8" class="amount"></td>';
			label_cell((int)$rfq_item['response_count'], 'align=right');
			label_cell($rfq_item['best_quoted_price'] !== null ? price_format($rfq_item['best_quoted_price']) : '-');
			echo '<td><input type="text" name="edit_rfq_specs_' . $rfq_item['id'] . '" value="' . htmlspecialchars($rfq_item['specifications'], ENT_QUOTES, 'UTF-8') . '" size="24"></td>';
			echo '<td nowrap>';
			hidden('edit_rfq_stock_' . $rfq_item['id'], $rfq_item['stock_id']);
			submit('UpdateRfqItem' . $rfq_item['id'], _('Update'), true, _('Update this RFQ item'));
			echo '&nbsp;';
			submit('DeleteRfqItem' . $rfq_item['id'], _('Delete'), true, _('Delete this RFQ item'));
			echo '</td>';
		} else {
			label_cell($rfq_item['stock_id']);
			label_cell($rfq_item['line_description']);
			qty_cell($rfq_item['quantity']);
			label_cell($rfq_item['line_unit_of_measure']);
			amount_cell($rfq_item['target_price']);
			label_cell((int)$rfq_item['response_count'], 'align=right');
			label_cell($rfq_item['best_quoted_price'] !== null ? price_format($rfq_item['best_quoted_price']) : '-');
			label_cell($rfq_item['specifications'] ? $rfq_item['specifications'] : '-');
		}

		end_row();
	}

	if ($k == 0)
		label_row('', _('No RFQ items have been added yet.'), 'colspan=' . ($editable ? 9 : 8) . ' align=center');

	end_table(1);

	if ($editable) {
		display_heading(_('Add RFQ Item'));
		start_table(TABLESTYLE2);
		start_row();
		stock_costable_items_list_cells(_('Item:'), 'rfq_item_stock_id', null, false, true);
		end_row();
		text_row(_('Description:'), 'rfq_item_description', null, 40, 255);
		qty_row(_('Quantity:'), 'rfq_item_quantity', null, null, null, get_qty_dec(''));
		amount_row(_('Target Price:'), 'rfq_item_target_price');
		text_row(_('Unit of Measure:'), 'rfq_item_uom', null, 20, 20);
		text_row(_('Specifications:'), 'rfq_item_specifications', null, 50, 255);
		end_table(1);
		submit_center('AddRfqItem', _('Add RFQ Item'), true, '', 'default');
	}

	br();

	display_heading(_('Invited Vendors'));
	start_table(TABLESTYLE, "width='100%'");
	$vendor_header = array(_('Vendor'), _('Status'), _('Sent Date'), _('Response Date'), _('Quoted Total'), _('Lead Days'), _('Score'), '');
	table_header($vendor_header);

	$k = 0;
	foreach ($rfq_vendors as $rfq_vendor) {
		alt_table_row_color($k);
		label_cell($rfq_vendor['supp_name']);
		label_cell(purch_rfq_vendor_status_badge($rfq_vendor['status']));
		label_cell($rfq_vendor['sent_date'] ? $rfq_vendor['sent_date'] : '-');
		label_cell($rfq_vendor['response_date'] ? $rfq_vendor['response_date'] : '-');
		amount_cell($rfq_vendor['total_quoted']);
		label_cell((int)$rfq_vendor['delivery_lead_days'] > 0 ? (int)$rfq_vendor['delivery_lead_days'] : '-');
		label_cell((float)$rfq_vendor['evaluator_score'] != 0 ? number_format((float)$rfq_vendor['evaluator_score'], 2) : '-');
		echo '<td nowrap>';
		if ($editable && !in_array($rfq_vendor['status'], array('responded', 'awarded')))
			submit('RemoveVendor' . $rfq_vendor['id'], _('Remove'), true, _('Remove this vendor from the RFQ'));
		if ((int)$rfq_vendor['is_winner'] === 1) {
			if ($editable && !in_array($rfq_vendor['status'], array('responded', 'awarded')))
				echo '&nbsp;';
			submit('CreatePOFromVendor' . $rfq_vendor['id'], _('Create PO'), true, _('Create purchase order from this awarded vendor'));
		}
		echo '</td>';
		end_row();
	}

	if ($k == 0)
		label_row('', _('No vendors have been invited yet.'), 'colspan=8 align=center');

	end_table(1);

	if ($editable) {
		start_table(TABLESTYLE2, "width='50%'");
		supplier_list_row(_('Add Vendor:'), 'add_supplier_id', null, true, false, true);
		end_table(1);
		submit_center('AddVendor', _('Add Vendor'), true, '', 'default');
	}

	if ($can_record_vendor_responses) {
		br();
		display_heading(_('Vendor Responses'));
		foreach ($rfq_vendors as $rfq_vendor) {
			$vendor_line_map = get_rfq_vendor_line_map((int)$rfq_vendor['id']);
			echo '<div style="border:1px solid #ddd;border-radius:4px;padding:14px;margin-bottom:16px;background:#fff;">';
			echo '<div style="display:flex;justify-content:space-between;gap:12px;align-items:center;flex-wrap:wrap;">';
			echo '<h3 style="margin:0;">' . htmlspecialchars($rfq_vendor['supp_name']) . '</h3>';
			echo '<div>' . purch_rfq_vendor_status_badge($rfq_vendor['status']) . '</div>';
			echo '</div>';

			hidden('response_item_ids_' . $rfq_vendor['id'], implode(',', $rfq_item_ids));

			start_table(TABLESTYLE2, "width='100%'");
			start_row();
			label_cell(_('Quoted Total:'), 'class="label"');
			echo '<td><input type="text" name="response_total_' . $rfq_vendor['id'] . '" value="' . price_decimal_format($rfq_vendor['total_quoted'], user_price_dec()) . '" size="10" class="amount"></td>';
			label_cell(_('Lead Days:'), 'class="label"');
			echo '<td><input type="text" name="response_vendor_lead_' . $rfq_vendor['id'] . '" value="' . (int)$rfq_vendor['delivery_lead_days'] . '" size="6"></td>';
			end_row();
			start_row();
			label_cell(_('Payment Terms:'), 'class="label"');
			echo '<td><input type="text" name="response_payment_terms_' . $rfq_vendor['id'] . '" value="' . htmlspecialchars($rfq_vendor['payment_terms'], ENT_QUOTES, 'UTF-8') . '" size="30"></td>';
			label_cell(_('Vendor Notes:'), 'class="label"');
			echo '<td><input type="text" name="response_vendor_notes_' . $rfq_vendor['id'] . '" value="' . htmlspecialchars($rfq_vendor['vendor_notes'], ENT_QUOTES, 'UTF-8') . '" size="40"></td>';
			end_row();
			end_table(1);

			start_table(TABLESTYLE, "width='100%'");
			table_header(array(_('Item'), _('Quoted Price'), _('Quoted Qty'), _('Lead Days'), _('Notes')));
			$item_row_color = 0;
			foreach ($rfq_items as $rfq_item) {
				$vendor_line = isset($vendor_line_map[(int)$rfq_item['id']]) ? $vendor_line_map[(int)$rfq_item['id']] : false;
				alt_table_row_color($item_row_color);
				label_cell($rfq_item['stock_id'] . ' - ' . $rfq_item['line_description']);
				echo '<td><input type="text" name="response_price_' . $rfq_vendor['id'] . '_' . $rfq_item['id'] . '" value="' . ($vendor_line ? price_decimal_format($vendor_line['quoted_price'], user_price_dec()) : '') . '" size="8" class="amount"></td>';
				echo '<td><input type="text" name="response_qty_' . $rfq_vendor['id'] . '_' . $rfq_item['id'] . '" value="' . ($vendor_line ? number_format2($vendor_line['quoted_quantity'], get_qty_dec($rfq_item['stock_id'])) : number_format2($rfq_item['quantity'], get_qty_dec($rfq_item['stock_id']))) . '" size="8" class="amount"></td>';
				echo '<td><input type="text" name="response_lead_' . $rfq_vendor['id'] . '_' . $rfq_item['id'] . '" value="' . ($vendor_line ? (int)$vendor_line['delivery_lead_days'] : 0) . '" size="6"></td>';
				echo '<td><input type="text" name="response_note_' . $rfq_vendor['id'] . '_' . $rfq_item['id'] . '" value="' . htmlspecialchars($vendor_line ? $vendor_line['notes'] : '', ENT_QUOTES, 'UTF-8') . '" size="30"></td>';
				end_row();
			}
			end_table(1);

			submit_center('RecordVendorResponse' . $rfq_vendor['id'], _('Save Vendor Response'), true, '', 'default');
			echo '</div>';
		}
	}

	echo '<div style="text-align:center;margin-top:18px;">';
	if ($rfq['status'] === 'draft' && count($rfq_items) > 0 && count($rfq_vendors) > 0)
		submit('SendRFQ', _('Send RFQ to Vendors'), true, _('Mark this RFQ as sent'));
	if ((int)$rfq['responded_count'] > 0 || (int)$rfq['winner_count'] > 0) {
		echo '&nbsp;';
		hyperlink_params($path_to_root . '/purchasing/purch_rfq_comparison.php', _('Open Comparison'), 'rfq_id=' . (int)$selected_id);
	}
	echo '</div>';
}

end_form();
end_page();