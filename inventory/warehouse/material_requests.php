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

$selected_id = get_post('selected_id', -1);
if ($selected_id == '') $selected_id = -1;

// --- Submit action ---
if (isset($_POST['Submit']) && $selected_id > 0) {
	if (submit_material_request($selected_id)) {
		display_notification(_('Material request has been submitted for approval.'));
	} else {
		display_error(_('Cannot submit this material request. It must be in Draft status and have at least one line.'));
	}
	$Ajax->activate('mr_list');
	$Ajax->activate('mr_detail');
}

// --- Approve action ---
if (isset($_POST['Approve']) && $selected_id > 0) {
	if (approve_material_request($selected_id)) {
		display_notification(_('Material request has been approved.'));
	} else {
		display_error(_('Cannot approve this material request. It must be in Submitted status.'));
	}
	$Ajax->activate('mr_list');
	$Ajax->activate('mr_detail');
}

// --- Reject action ---
if (isset($_POST['Reject']) && $selected_id > 0) {
	if (reject_material_request($selected_id)) {
		display_notification(_('Material request has been rejected and returned to Draft status.'));
	} else {
		display_error(_('Cannot reject this material request.'));
	}
	$Ajax->activate('mr_list');
	$Ajax->activate('mr_detail');
}

// --- Cancel action ---
if (isset($_POST['CancelMR']) && $selected_id > 0) {
	if (cancel_material_request($selected_id)) {
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
		display_error(_('Cannot delete this material request. Only Draft or Cancelled requests can be deleted.'));
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
		$mr = get_material_request($selected_id);
		$po_no = convert_material_request_to_po($selected_id, $supplier_id, $mr['warehouse_loc_code']);
		if ($po_no) {
			display_notification(sprintf(_('Purchase Order #%d has been created from this material request.'), $po_no));
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

	if (empty(get_post('request_type'))) {
		display_error(_('You must select a request type.'));
		$input_error = 1;
	}
	if (empty(get_post('warehouse_loc')) || get_post('warehouse_loc') == 'all') {
		display_error(_('You must select a requesting warehouse.'));
		$input_error = 1;
	}

	if ($input_error == 0) {
		$request_date = date2sql(get_post('request_date'));
		$required_date = get_post('required_date') ? date2sql(get_post('required_date')) : null;
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
			display_notification(sprintf(_('Material request #%d has been created.'), $new_id));
			$selected_id = $new_id;
		} else {
			update_material_request(
				$selected_id,
				$request_type, $warehouse_loc,
				$request_date, $required_date,
				$reference, $memo
			);
			display_notification(_('Material request has been updated.'));
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
		add_material_request_line(
			$selected_id,
			$line_stock,
			$line_qty,
			$line_date,
			$line_memo
		);
		display_notification(_('Line added to material request.'));
	}
	$Ajax->activate('mr_detail');
}

// --- Update line ---
$update_line_id = find_submit('UpdateLine');
if ($update_line_id > 0) {
	$upd_qty = input_num('edit_line_qty_' . $update_line_id);
	$upd_date = get_post('edit_line_date_' . $update_line_id) ? date2sql(get_post('edit_line_date_' . $update_line_id)) : null;
	$upd_memo = get_post('edit_line_memo_' . $update_line_id);

	if ($upd_qty <= 0) {
		display_error(_('Quantity must be positive.'));
	} else {
		update_material_request_line($update_line_id, $upd_qty, $upd_date, $upd_memo);
		display_notification(_('Line has been updated.'));
	}
	$Ajax->activate('mr_detail');
}

// --- Delete line ---
$del_line = find_submit('DeleteLine');
if ($del_line > 0) {
	delete_material_request_line($del_line);
	display_notification(_('Line removed from material request.'));
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
if (isset($_POST['New'])) {
	$selected_id = -1;
	$Ajax->activate('mr_detail');
}

// =====================================================================
// DISPLAY
// =====================================================================

start_form();

// --- FILTERS ---
start_table(TABLESTYLE_NOBORDER);
start_row();

$filter_status = get_post('filter_status', '');
$statuses = array('' => _('All Statuses')) + get_material_request_statuses();
echo '<td>' . _('Status:') . ' ';
echo array_selector('filter_status', $filter_status, $statuses, array('select_submit' => true));
echo '</td>';

$filter_type = get_post('filter_type', '');
$types = array('' => _('All Types')) + get_material_request_types();
echo '<td>' . _('Type:') . ' ';
echo array_selector('filter_type', $filter_type, $types, array('select_submit' => true));
echo '</td>';

locations_list_cells(_('Warehouse:'), 'filter_warehouse', null, true);

submit_cells('SearchMR', _('Search'), '', _('Search material requests'), true);
submit_cells('New', _('New Material Request'), '', _('Create a new material request'), 'default');
end_row();
end_table();

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
start_table(TABLESTYLE, "width='100%'");
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
if ($selected_id > 0 || isset($_POST['New'])) {
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

		start_table(TABLESTYLE2);
		label_row(_('Type:'), material_request_type_badge($mr['request_type']));
		label_row(_('Warehouse:'), $mr['warehouse_name']);
		label_row(_('Request Date:'), sql2date($mr['request_date']));
		if ($mr['required_date'])
			label_row(_('Required Date:'), sql2date($mr['required_date']));
		if ($mr['reference'])
			label_row(_('Reference:'), $mr['reference']);
		if ($mr['memo'])
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
		end_table(1);

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
				label_cell($line['stock_id']);
				label_cell($line['item_description']);
				echo '<td>';
				echo '<input type="text" name="edit_line_qty_' . $line['line_id']
					. '" value="' . number_format2($line['qty_requested'], get_qty_dec($line['stock_id']))
					. '" size="8" class="amount">';
				echo '</td>';
				qty_cell($line['qty_fulfilled']);
				qty_cell($remaining);
				label_cell($line['item_units']);
				echo '<td>';
				echo '<input type="text" name="edit_line_date_' . $line['line_id']
					. '" value="' . ($line['required_date'] ? sql2date($line['required_date']) : '')
					. '" size="12">';
				echo '</td>';
				echo '<td>';
				echo '<input type="text" name="edit_line_memo_' . $line['line_id']
					. '" value="' . htmlspecialchars($line['memo'] ? $line['memo'] : '', ENT_QUOTES, 'UTF-8')
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
			stock_costable_items_list_cells(_('Item:'), 'line_stock_id', null, false, true);
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

		if ($mr['status'] === 'draft') {
			submit('Submit', _('Submit for Approval'), true, _('Submit this request for approval'));
			echo ' ';
			submit('Delete', _('Delete'), true, _('Delete this material request'));
		}

		if ($mr['status'] === 'submitted') {
			submit('Approve', _('Approve'), true, _('Approve this material request'));
			echo ' ';
			submit('Reject', _('Reject'), true, _('Reject and return to Draft'));
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

	} elseif (isset($_POST['New']) || $selected_id == -1 && !isset($_POST['SearchMR'])) {
		// --- New request form ---
		display_heading(_('New Material Request'));
		start_table(TABLESTYLE2);

		$types = get_material_request_types();
		array_selector_row(_('Request Type:'), 'request_type', null, $types);
		locations_list_row(_('Requesting Warehouse:'), 'warehouse_loc', null);
		date_row(_('Request Date:'), 'request_date', '', true);
		date_row(_('Required Date:'), 'required_date');
		text_row(_('Reference:'), 'reference', null, 30, 60);
		textarea_row(_('Memo:'), 'memo', null, 50, 3);

		end_table(1);
		submit_center('ADD_ITEM', _('Create Material Request'), true, '', 'default');
	}
}
div_end();

end_form();
end_page();
