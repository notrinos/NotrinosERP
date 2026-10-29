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
 * Material Requests — Internal demand workflow.
 *
 * Lifecycle: Draft → Submitted → Approved → Ordered → Fulfilled → (Cancelled)
 * Types: purchase, transfer, manufacturing, issue
 * Supports conversion to PO, Transfer Order, or Work Order.
 *
 * @package NotrinosERP
 * @subpackage Inventory/Warehouse
 */

$page_security = 'SA_MATERIALREQUEST';
$path_to_root = '../..';

include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/includes/date_functions.inc');
include_once($path_to_root . '/purchasing/includes/purchasing_db.inc');
include_once($path_to_root . '/includes/data_checks.inc');
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/includes/ui/ui_lists.inc');
include_once($path_to_root . '/inventory/warehouse/includes/db/warehouse_material_requests_db.inc');

$js = '';
if (user_use_date_picker())
	$js .= get_js_date_picker();

page(_($help_context = 'Material Requests'), false, false, '', $js);

// =====================================================================
// HANDLE ACTIONS
// =====================================================================

/** Bind approval commands to the actor/company which rendered this native form. */
function get_material_request_core_form_context()
{
	global $db_connections;
	$user = $_SESSION['wa_current_user'];
	return hash_hmac('sha256', serialize(array($user->user, $user->cur_con,
		$db_connections[$user->cur_con])), ensure_csrf_token());
}

$material_line_commands = array('AddLine', 'UpdateLine', 'DeleteLine');
$material_line_post = isset($_POST['AddLine']) || find_submit('UpdateLine') > 0 || find_submit('DeleteLine') > 0;
if ($material_line_post || isset($_POST['ADD_ITEM']) || isset($_POST['UPDATE_ITEM']) || isset($_POST['Submit']) || isset($_POST['CancelMR']) || isset($_POST['Delete']) || isset($_POST['ConvertPO'])) {
	$core_context = get_post('material_core_context', '');
	if (!is_string($core_context) || !hash_equals(get_material_request_core_form_context(), $core_context)) {
		unset($_POST['Submit'], $_POST['CancelMR'], $_POST['Delete'], $_POST['UPDATE_ITEM'], $_POST['ADD_ITEM'], $_POST['ConvertPO']);
		foreach (array_keys($_POST) as $field)
			foreach ($material_line_commands as $command)
				if ($field === $command || strpos($field, $command) === 0) unset($_POST[$field]);
		display_error(_('The material request form context has changed. Reload this page before changing this request.'));
	}
}

$selected_id = get_post('selected_id', null);
if ($selected_id === null || $selected_id === '')
	$selected_id = isset($_GET['selected_id']) ? $_GET['selected_id'] : -1;
if ($selected_id === '')
	$selected_id = -1;
$is_new_request = isset($_POST['New']) || isset($_GET['New']);

// --- Submit action ---
if (isset($_POST['Submit']) && $selected_id > 0) {
	$result = submit_material_request_for_core_approval($selected_id);
	if ($result !== false) {
		if (is_array($result) && isset($result['status']) && $result['status'] === 'pending') {
			display_notification(_('Material request has been submitted to the core approval workflow.'));
		} elseif (is_array($result) && isset($result['status']) && $result['status'] === 'auto_approved') {
			display_notification(_('Material request has been auto-approved by the core approval workflow.'));
		} else {
			display_notification(_('Material request has been submitted.'));
		}
	} else {
		display_error(_('Cannot submit this material request. It must be in Draft status, have at least one line, and pass approval setup validation.'));
	}
	$Ajax->activate('mr_list');
	$Ajax->activate('mr_detail');
}

// --- Cancel action ---
if (isset($_POST['CancelMR']) && $selected_id > 0) {
	if (cancel_material_request($selected_id, _('Cancelled from material request entry.'))) {
		display_notification(_('Material request has been cancelled.'));
	} else {
		display_error(_('Cannot cancel this material request.'));
	}
	$Ajax->activate('mr_list');
	$Ajax->activate('mr_detail');
}

// --- Delete action ---
if (isset($_POST['Delete']) && $selected_id > 0) {
	if (delete_material_request($selected_id)) {
		display_notification(_('Material request has been deleted.'));
		$selected_id = -1;
	} else {
		display_error(_('Cannot delete this material request. Reviewed, linked or retained source evidence must be preserved.'));
	}
	$Ajax->activate('mr_list');
	$Ajax->activate('mr_detail');
}

// --- Convert to PO ---
if (isset($_POST['ConvertPO']) && $selected_id > 0) {
	$supplier_id = get_post('convert_supplier');
	if (empty($supplier_id)) {
		display_error(_('Please select a supplier for the purchase order.'));
	} else {
		include_once($path_to_root.'/inventory/warehouse/includes/db/warehouse_material_po_approval_db.inc');
		$result = submit_material_request_purchase_order($selected_id, $supplier_id);
		if ($result && $result['status'] === 'pending') {
			display_notification(sprintf(_('Purchase order conversion submitted for approval. Draft #%d.'), $result['draft_id']));
		} elseif ($result && $result['status'] === 'posted') {
			display_notification(sprintf(_('Purchase Order #%d has been created from this material request.'), $result['trans_no']));
		} else {
			display_error(_('Could not create purchase order. The request may not be in Approved status or have no outstanding lines.'));
		}
	}
	$Ajax->activate('mr_list');
	$Ajax->activate('mr_detail');
}

// --- Convert to Transfer Order ---
if (isset($_POST['ConvertTO']) && $selected_id > 0) {
	$from_loc = get_post('convert_from_loc');
	if (empty($from_loc)) {
		display_error(_('Please select a source location for the transfer.'));
	} else {
		$to_id = convert_material_request_to_transfer($selected_id, $from_loc);
		if ($to_id) {
			display_notification(sprintf(_('Transfer Order #%d has been created from this material request.'), $to_id));
		} else {
			display_error(_('Could not create transfer order. The request may not be in Approved status or have no outstanding lines.'));
		}
	}
	$Ajax->activate('mr_list');
	$Ajax->activate('mr_detail');
}

// --- Save new/edit ---
if (isset($_POST['ADD_ITEM']) || isset($_POST['UPDATE_ITEM'])) {
	$input_error = 0;
	if (isset($_POST['ADD_ITEM']) && isset($_POST['UPDATE_ITEM'])) {
		display_error(_('Choose one material request command and retry.'));
		$input_error = 1;
	}

	if (empty(get_post('request_type'))) {
		display_error(_('You must select a request type.'));
		$input_error = 1;
	}
	if (empty(get_post('warehouse_loc')) || get_post('warehouse_loc') == 'all') {
		display_error(_('You must select a requesting warehouse.'));
		$input_error = 1;
	}

	$request_date_input = get_post('request_date', '');
	$required_date_input = get_post('required_date', '');
	if (!is_string($request_date_input) || !is_date($request_date_input)) {
		display_error(_('Enter a valid request date.'));
		$input_error = 1;
	}
	if (!is_string($required_date_input) || ($required_date_input !== '' && !is_date($required_date_input))) {
		display_error(_('Enter a valid required date, or leave it empty.'));
		$input_error = 1;
	}
	if ($input_error == 0) {
		$request_date = date2sql($request_date_input);
		$required_date = $required_date_input !== '' ? date2sql($required_date_input) : null;
		$reference = get_post('reference');
		$memo = get_post('memo');
		$request_type = get_post('request_type');
		$warehouse_loc = get_post('warehouse_loc');

		if (isset($_POST['ADD_ITEM'])) {
			$new_id = add_material_request(
				$request_type, $warehouse_loc,
				$request_date, $required_date,
				$reference, $memo
			);
			if ($new_id) {
				display_notification(sprintf(_('Material request #%d has been created.'), $new_id));
				$selected_id = $new_id;
			} else display_error(_('Cannot create this material request. Reload the form and retry.'));
		} else {
			$header_result = update_material_request(
				$selected_id,
				$request_type, $warehouse_loc,
				$request_date, $required_date,
				$reference, $memo
			);
			if ($header_result) display_notification(_('Material request has been updated.'));
			else display_error(_('Cannot update this material request. Reload the current standalone draft and retry.'));
		}
	}
	$Ajax->activate('mr_list');
	$Ajax->activate('mr_detail');
}

// --- Add line ---
if (isset($_POST['AddLine'])) {
	$line_stock = get_post('line_stock_id');
	$line_qty = input_num('line_qty');
	$line_date = get_post('line_required_date') ? date2sql(get_post('line_required_date')) : null;
	$line_memo = get_post('line_memo');

	if (empty($line_stock)) {
		display_error(_('You must select an item.'));
	} elseif ($line_qty <= 0) {
		display_error(_('Quantity must be positive.'));
	} elseif ($selected_id > 0) {
		$line_result = add_material_request_line(
			$selected_id,
			$line_stock,
			$line_qty,
			$line_date,
			$line_memo
		);
		if ($line_result !== false) display_notification(_('Line added to material request.'));
		else display_error(_('Cannot add this line. Reload the current draft and retry.'));
	}
	$Ajax->activate('mr_detail');
}

// --- Update line ---
$material_line_failed_id = 0;
$update_line_id = find_submit('UpdateLine');
if ($update_line_id > 0) {
	$upd_qty = input_num('edit_line_qty_' . $update_line_id);
	$upd_date = get_post('edit_line_date_' . $update_line_id) ? date2sql(get_post('edit_line_date_' . $update_line_id)) : null;
	$upd_memo = get_post('edit_line_memo_' . $update_line_id);

	if ($upd_qty <= 0) {
		$material_line_failed_id = $update_line_id;
		display_error(_('Quantity must be positive.'));
	} else {
		if (update_material_request_line($update_line_id, $upd_qty, $upd_date, $upd_memo, $selected_id))
			display_notification(_('Line has been updated.'));
		else {
			$material_line_failed_id = $update_line_id;
			display_error(_('Cannot update this line. Reload the current draft and retry.'));
		}
	}
	$Ajax->activate('mr_detail');
}

// --- Delete line ---
$del_line = find_submit('DeleteLine');
if ($del_line > 0) {
	if (delete_material_request_line($del_line, $selected_id)) display_notification(_('Line removed from material request.'));
	else display_error(_('Cannot remove this line. Reviewed, linked or retained source evidence must be preserved.'));
	$Ajax->activate('mr_detail');
}

// --- Edit button from list ---
$edit_id = find_submit('Edit');
if ($edit_id > 0) {
	$selected_id = $edit_id;
	$Ajax->activate('mr_detail');
}

// --- View button from list ---
$view_id = find_submit('View');
if ($view_id > 0) {
	$selected_id = $view_id;
	$Ajax->activate('mr_detail');
}

// --- New button ---
if ($is_new_request) {
	$selected_id = -1;
	$Ajax->activate('mr_detail');
}

// =====================================================================
// DISPLAY
// =====================================================================

start_form();
hidden('material_core_context', get_material_request_core_form_context());

// --- FILTERS ---
start_table(TABLESTYLE_NOBORDER);
start_row();

$filter_status = get_post('filter_status', '');
$statuses = array('' => _('All Statuses')) + get_material_request_statuses();
echo "<div class = 'filter-field'>";
echo array_selector('filter_status', $filter_status, $statuses, array('select_submit' => true));
echo '</div>';

$filter_type = get_post('filter_type', '');
$types = array('' => _('All Types')) + get_material_request_types();
echo "<div class = 'filter-field'>";
echo array_selector('filter_type', $filter_type, $types, array('select_submit' => true));
echo '</div>';

locations_list_cells(_('Warehouse:'), 'filter_warehouse', null, true);

submit_cells('SearchMR', _('Search'), '', _('Search material requests'), true);
submit_cells('New', _('New Material Request'), '', _('Create a new material request'), 'default');
end_row();
end_table(1);

// --- SUMMARY CARDS ---
$summary = get_material_request_summary();
echo '<div style="display:flex;gap:10px;margin:10px 0;flex-wrap:wrap;">';
$status_cards = array(
	'draft'     => array('icon' => 'fa-file-o',      'color' => '#6c757d'),
	'submitted' => array('icon' => 'fa-paper-plane',  'color' => '#17a2b8'),
	'approved'  => array('icon' => 'fa-check',        'color' => '#007bff'),
	'ordered'   => array('icon' => 'fa-shopping-cart', 'color' => '#fd7e14'),
	'fulfilled' => array('icon' => 'fa-check-circle',  'color' => '#28a745'),
);
$st_labels = get_material_request_statuses();
foreach ($status_cards as $st => $conf) {
	$cnt = isset($summary[$st]) ? $summary[$st] : 0;
	echo '<div style="background:#fff;border:1px solid #ddd;border-left:4px solid '
		. $conf['color'] . ';padding:8px 16px;border-radius:4px;min-width:120px;">'
		. '<div style="font-size:20px;font-weight:bold;color:' . $conf['color'] . ';">' . $cnt . '</div>'
		. '<div style="font-size:12px;color:#666;"><i class="fa ' . $conf['icon'] . '"></i> '
		. $st_labels[$st] . '</div></div>';
}
echo '</div>';

// --- LIST ---
$filters = array();
if (!empty(get_post('filter_status')))
	$filters['status'] = get_post('filter_status');
if (!empty(get_post('filter_type')))
	$filters['request_type'] = get_post('filter_type');
if (!empty(get_post('filter_warehouse')) && get_post('filter_warehouse') != 'all')
	$filters['warehouse_loc_code'] = get_post('filter_warehouse');

div_start('mr_list');
$requests = get_material_requests_filtered($filters, 100);
start_table(TABLESTYLE_DATA, "width='100%'");
$th = array('#', _('Request #'), _('Type'), _('Status'), _('Warehouse'),
	_('Lines'), _('Total Qty'), _('Fulfilled'), _('Request Date'), _('Required Date'),
	_('Requested By'), '');
table_header($th);

$k = 0;
while ($row = db_fetch($requests)) {
	alt_table_row_color($k);

	label_cell($row['request_id']);
	label_cell('<a href="' . $_SERVER['PHP_SELF'] . '?selected_id=' . $row['request_id'] . '">'
		. format_material_request_no($row['request_no']) . '</a>');
	label_cell(material_request_type_badge($row['request_type']));
	label_cell(material_request_status_badge($row['status']));
	label_cell($row['warehouse_name']);
	label_cell($row['line_count'], 'align=right');
	qty_cell($row['total_qty_requested'] ? $row['total_qty_requested'] : 0);
	qty_cell($row['total_qty_fulfilled'] ? $row['total_qty_fulfilled'] : 0);
	label_cell(sql2date($row['request_date']));
	label_cell($row['required_date'] ? sql2date($row['required_date']) : '-');
	label_cell($row['requested_by_name']);

	// Action buttons
	echo '<td nowrap>';
	if ($row['status'] === 'draft') {
		echo '<button class="ajaxsubmit" type="submit" name="Edit' . $row['request_id']
			. '" value="1" style="margin:1px;">' . _('Edit') . '</button> ';
	}
	echo '<button class="ajaxsubmit" type="submit" name="View' . $row['request_id']
		. '" value="1" style="margin:1px;">' . _('View') . '</button>';
	echo '</td>';
	end_row();
}

if ($k == 0) {
	label_row('', _('No material requests found.'), 'colspan=12 align=center');
}

end_table(1);
div_end();

// =====================================================================
// DETAIL VIEW / EDIT FORM
// =====================================================================

div_start('mr_detail');

if ($selected_id > 0 || $is_new_request) {
	$editing = false;
	$mr = null;

	if ($selected_id > 0) {
		$mr = get_material_request($selected_id);
		if (!$mr) {
			display_error(_('Material request not found.'));
			$selected_id = -1;
		}
	}

	if ($selected_id > 0 && $mr) {
		$editing = ($mr['status'] === 'draft');

		// Display request header info
		echo '<div style="background:#f8f9fa;border:1px solid #dee2e6;border-radius:4px;padding:15px;margin:10px 0;">';
		echo '<h3 style="margin:0 0 10px;">' . _('Material Request') . ': '
			. format_material_request_no($mr['request_no']) . ' '
			. material_request_status_badge($mr['status']) . ' '
			. material_request_type_badge($mr['request_type']) . '</h3>';

		start_outer_table();

		table_section(1);
		label_row(_('Type:'), material_request_type_badge($mr['request_type']));
		label_row(_('Warehouse:'), $mr['warehouse_name']);
		label_row(_('Request Date:'), sql2date($mr['request_date']));
		if ($mr['required_date'])
			label_row(_('Required Date:'), sql2date($mr['required_date']));

		table_section(2);

		if ($mr['reference'] !== null && $mr['reference'] !== '')
			label_row(_('Reference:'), $mr['reference']);
		if ($mr['memo'] !== null && $mr['memo'] !== '')
			label_row(_('Memo:'), $mr['memo']);
		label_row(_('Requested By:'), $mr['requested_by_name']);
		if ($mr['approved_by_name'])
			label_row(_('Approved By:'), $mr['approved_by_name']);

		// Linked document info
		if ($mr['linked_doc_type'] && $mr['linked_doc_no']) {
			$doc_types = array(
				ST_PURCHORDER  => _('Purchase Order'),
				ST_LOCTRANSFER => _('Transfer Order'),
				ST_WORKORDER   => _('Work Order'),
			);
			$doc_label = isset($doc_types[$mr['linked_doc_type']]) ? $doc_types[$mr['linked_doc_type']] : _('Document');
			label_row(_('Linked Document:'), $doc_label . ' #' . $mr['linked_doc_no']);
		}

		// Fulfillment progress
		$fulfillment_pct = get_material_request_fulfillment_pct($selected_id);
		$pct_color = '#6c757d'; // gray
		if ($fulfillment_pct >= 100) $pct_color = '#28a745'; // green
		elseif ($fulfillment_pct > 0) $pct_color = '#fd7e14'; // orange

		echo '<tr><td class="label">' . _('Fulfillment:') . '</td><td>';
		echo '<div style="display:inline-flex;align-items:center;gap:8px;">';
		echo '<div style="width:200px;height:16px;background:#e9ecef;border-radius:8px;overflow:hidden;">';
		echo '<div style="width:' . $fulfillment_pct . '%;height:100%;background:' . $pct_color . ';border-radius:8px;transition:width 0.3s;"></div>';
		echo '</div>';
		echo '<span style="font-weight:bold;color:' . $pct_color . ';">' . $fulfillment_pct . '%</span>';
		echo '</div></td></tr>';

		// Estimated value
		$total_value = get_material_request_total_value($selected_id);
		label_row(_('Estimated Value:'), price_format($total_value));
		end_outer_table(1);

		// --- Line items ---
		display_heading(_('Line Items'));

		start_table(TABLESTYLE, "width='100%'");
		$th = array(_('Item Code'), _('Description'), _('Qty Requested'), _('Qty Fulfilled'),
			_('Remaining'), _('Unit'), _('Required Date'), _('Memo'));
		if ($editing)
			$th[] = '';
		table_header($th);

		$k = 0;
		$lines = get_material_request_lines($selected_id);
		while ($line = db_fetch($lines)) {
			alt_table_row_color($k);

			$remaining = $line['qty_requested'] - $line['qty_fulfilled'];

			if ($editing) {
				// Editable row
				$line_quantity_value = number_format2($line['qty_requested'], max(4, get_qty_dec($line['stock_id'])));
				$line_date_value = $line['required_date'] ? sql2date($line['required_date']) : '';
				$line_memo_value = $line['memo'] === null ? '' : $line['memo'];
				if ((int)$line['line_id'] === (int)$material_line_failed_id) {
					$line_quantity_value = get_post('edit_line_qty_' . $line['line_id'], $line_quantity_value);
					$line_date_value = get_post('edit_line_date_' . $line['line_id'], $line_date_value);
					$line_memo_value = get_post('edit_line_memo_' . $line['line_id'], $line_memo_value);
				}
				label_cell($line['stock_id']);
				label_cell($line['item_description']);
				echo '<td>';
				echo '<input type="text" name="edit_line_qty_' . $line['line_id']
					. '" value="' . htmlspecialchars(is_scalar($line_quantity_value) ? (string)$line_quantity_value : '', ENT_QUOTES, 'UTF-8')
					. '" size="8" class="amount">';
				echo '</td>';
				qty_cell($line['qty_fulfilled']);
				qty_cell($remaining);
				label_cell($line['item_units']);
				echo '<td>';
				echo '<input type="text" name="edit_line_date_' . $line['line_id']
					. '" value="' . htmlspecialchars(is_scalar($line_date_value) ? (string)$line_date_value : '', ENT_QUOTES, 'UTF-8')
					. '" size="12">';
				echo '</td>';
				echo '<td>';
				echo '<input type="text" name="edit_line_memo_' . $line['line_id']
					. '" value="' . htmlspecialchars(is_scalar($line_memo_value) ? (string)$line_memo_value : '', ENT_QUOTES, 'UTF-8')
					. '" size="20">';
				echo '</td>';
				echo '<td nowrap>';
				echo '<button type="submit" name="UpdateLine' . $line['line_id']
					. '" value="1" class="ajaxsubmit" style="margin:1px;">' . _('Update') . '</button> ';
				echo '<button type="submit" name="DeleteLine' . $line['line_id']
					. '" value="1" class="ajaxsubmit" onclick="return confirm(\'' . _('Delete this line?') . '\');"'
					. ' style="margin:1px;">' . _('Delete') . '</button>';
				echo '</td>';
			} else {
				// Read-only row
				label_cell($line['stock_id']);
				label_cell($line['item_description']);
				qty_cell($line['qty_requested']);
				qty_cell($line['qty_fulfilled']);

				// Color-code remaining
				if ($remaining <= 0)
					label_cell('<span style="color:#28a745;font-weight:bold;">0</span>', 'align=right');
				else
					qty_cell($remaining);

				label_cell($line['item_units']);
				label_cell($line['required_date'] ? sql2date($line['required_date']) : '-');
				label_cell($line['memo'] ? $line['memo'] : '-');
			}

			end_row();
		}

		if ($k == 0) {
			$colspan = $editing ? 9 : 8;
			label_row('', _('No line items.'), 'colspan=' . $colspan . ' align=center');
		}

		end_table(1);

		// --- Add line form (draft only) ---
		if ($editing) {
			display_heading(_('Add Line'));
			start_table(TABLESTYLE2);
			label_cell(_('Item:'));
			stock_costable_items_list_cells(null, 'line_stock_id', null, false, true);
			qty_row(_('Quantity:'), 'line_qty', null, null, null, 4);
			date_row(_('Required Date:'), 'line_required_date');
			text_row(_('Memo:'), 'line_memo', null, 30, 255);
			end_table(1);

			hidden('selected_id', $selected_id);
			submit_center('AddLine', _('Add Line'), true, '', 'default');
			echo '<br>';
		}

		// --- Action buttons based on status ---
		echo '<div style="text-align:center;margin:15px 0;">';
		hidden('selected_id', $selected_id);

		if (in_array($mr['status'], array('draft', 'cancelled'))) {
			submit('Submit', _('Submit for Approval'), true, _('Submit this request for approval'));
			echo ' ';
			submit('Delete', _('Delete'), true, _('Delete this material request'));
		}

		if ($mr['status'] === 'submitted') {
			echo '<div style="background:#fff3cd;border:1px solid #ffeeba;border-radius:4px;padding:10px;margin:8px 0;">';
			echo '<strong>' . _('Pending Core Approval') . '</strong><br>';
			echo _('This material request is in the core approval queue. Approve or reject it from the Approval Dashboard.');
			echo '<br><a href="' . $path_to_root . '/admin/approval_dashboard.php?&sel_app=system" target="_blank">'
				. _('Open Approval Dashboard') . '</a>';
			echo '</div>';
		}

		if ($mr['status'] === 'approved') {
			// Conversion options based on request type
			echo '<div style="background:#e8f4fd;border:1px solid #bee5eb;border-radius:4px;padding:15px;margin:10px 0;text-align:left;">';
			echo '<h4 style="margin:0 0 10px;">' . _('Convert to Document') . '</h4>';

			if ($mr['request_type'] === 'purchase') {
				echo '<div style="margin-bottom:10px;">';
				start_table(TABLESTYLE_NOBORDER);
				start_row();
				supplier_list_cells(_('Supplier:'), 'convert_supplier', null);
				submit_cells('ConvertPO', _('Create Purchase Order'), '', _('Create a PO from this request'), true);
				end_row();
				end_table();
				echo '</div>';
			}

			if ($mr['request_type'] === 'transfer') {
				echo '<div style="margin-bottom:10px;">';
				start_table(TABLESTYLE_NOBORDER);
				start_row();
				locations_list_cells(_('From Location:'), 'convert_from_loc', null);
				submit_cells('ConvertTO', _('Create Transfer Order'), '', _('Create a TO from this request'), true);
				end_row();
				end_table();
				echo '</div>';
			}

			if ($mr['request_type'] === 'manufacturing') {
				echo '<div style="margin-bottom:10px;">';
				echo '<p style="color:#666;">' . _('Manufacturing requests should be converted to Work Orders in the Manufacturing module. Items to produce:') . '</p>';
				$mfg_lines = get_material_request_manufacturing_lines($selected_id);
				if (count($mfg_lines) > 0) {
					start_table(TABLESTYLE);
					$th = array(_('Item Code'), _('Description'), _('Qty to Produce'), _('Unit'));
					table_header($th);
					$mk = 0;
					foreach ($mfg_lines as $mline) {
						alt_table_row_color($mk);
						label_cell($mline['stock_id']);
						label_cell($mline['description']);
						qty_cell($mline['qty']);
						label_cell($mline['units']);
						end_row();
					}
					end_table();
				}
				echo '</div>';
			}

			if ($mr['request_type'] === 'issue') {
				echo '<p style="color:#666;">' . _('Issue requests are fulfilled by direct inventory adjustments. Process the items from the warehouse.') . '</p>';
			}

			echo '</div>';
		}

		if (in_array($mr['status'], array('draft', 'submitted', 'approved'))) {
			echo ' ';
			submit('CancelMR', _('Cancel Request'), true, _('Cancel this material request'));
		}

		echo '</div>';
		echo '</div>';

		// View link
		echo '<div style="text-align:center;margin:5px 0;">';
		echo '<a href="' . $path_to_root . '/inventory/warehouse/view/view_material_request.php?request_id='
			. $selected_id . '" target="_blank">' . _('Open printable view') . '</a>';
		echo '</div>';

	} elseif ($is_new_request || $selected_id == -1 && !isset($_POST['SearchMR'])) {
		// --- New request form ---
		display_heading(_('New Material Request'));
		start_outer_table();

		table_section(1);
		$types = get_material_request_types();
		array_selector_row(_('Request Type:'), 'request_type', null, $types);
		locations_list_row(_('Requesting Warehouse:'), 'warehouse_loc', null);
		date_row(_('Request Date:'), 'request_date', '', true);
		date_row(_('Required Date:'), 'required_date');

		table_section(2);
		text_row(_('Reference:'), 'reference', null, 30, 60);
		textarea_row(_('Memo:'), 'memo', null, 50, 5);

		end_outer_table(1);
		submit_center('ADD_ITEM', _('Create Material Request'), true, '', 'default');
	}
}
div_end();

end_form();
end_page();
