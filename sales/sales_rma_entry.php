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
 * Sales RMA Entry
 *
 * Customer-facing RMA authorization form.  Handles:
 *   - New RMA creation (optionally pre-populated from an invoice or delivery)
 *   - Edit while status = 'pending'
 *   - Authorize / Reject actions
 *   - "Create WH Return" button (after authorization, delegates to existing WH system)
 *   - "Create Credit Note" button (after WH processing or upon authorization)
 *   - "Create Replacement Order" button (when return_method = replacement)
 *
 * @package NotrinosERP
 * @subpackage Sales
 */
$page_security = 'SA_SALESRETURN';
$path_to_root  = '..';

include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/includes/date_functions.inc');
include_once($path_to_root . '/includes/data_checks.inc');
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/includes/ui/ui_lists.inc');
include_once($path_to_root . '/sales/includes/db/sales_rma_db.inc');
include_once($path_to_root . '/sales/includes/db/customers_db.inc');

$js = '';
if ($SysPrefs->use_popup_windows && $SysPrefs->use_popup_search)
	$js .= get_js_open_window(900, 600);
if (user_use_date_picker())
	$js .= get_js_date_picker();

page(_($help_context = 'Sales Return Material Authorization Entry'), false, false, '', $js);

// ============================================================================
// INITIALISE FROM URL PARAMS (pre-populate from invoice/delivery)
// ============================================================================

$rma_id        = get_post('selected_id', -1);
if ($rma_id === '') $rma_id = -1;
$rma_id = (int)$rma_id;
if ($rma_id <= 0 && isset($_GET['selected_id']))
	$rma_id = (int)$_GET['selected_id'];

$source_type_init = isset($_GET['source_type']) ? (int)$_GET['source_type'] : 0;
$source_no_init   = isset($_GET['source_no'])   ? (int)$_GET['source_no']   : 0;
$new_rma          = isset($_GET['New']) && $_GET['New'] == 1;

// ============================================================================
// VALIDATION
// ============================================================================

/**
 * Validate the RMA header form before saving.
 *
 * @return bool
 */
function can_save_rma_header()
{
	if (!get_post('debtor_no') || get_post('debtor_no') == ALL_TEXT) {
		display_error(_('You must select a customer.'));
		set_focus('debtor_no');
		return false;
	}
	if (!is_date(get_post('request_date'))) {
		display_error(_('The request date is invalid.'));
		set_focus('request_date');
		return false;
	}
	if (!get_post('return_reason_id')) {
		display_error(_('You must select a return reason.'));
		set_focus('return_reason_id');
		return false;
	}
	return true;
}

// ============================================================================
// ACTION HANDLERS
// ============================================================================

// --- Save new RMA ---
if (isset($_POST['SAVE_RMA_NEW'])) {
	if (can_save_rma_header()) {
		$rma_id = add_sales_rma(
			(int)get_post('debtor_no'),
			(int)get_post('branch_code'),
			get_post('request_date'),
			(int)get_post('source_type'),
			(int)get_post('source_no'),
			(int)get_post('return_reason_id'),
			get_post('return_method', 'credit_note'),
			get_post('customer_notes', ''),
			get_post('internal_notes', ''),
			(float)get_post('restocking_fee_percent', 0)
		);

		// Add lines from the source document
		$src_type = (int)get_post('source_type');
		$src_no   = (int)get_post('source_no');
		if ($src_no > 0) {
			if ($src_type == ST_SALESINVOICE)
				$items_result = get_invoice_items_for_rma($src_no);
			elseif ($src_type == ST_CUSTDELIVERY)
				$items_result = get_delivery_items_for_rma($src_no);
			else
				$items_result = false;

			if ($items_result) {
				while ($item = db_fetch($items_result)) {
					// If user selected individual items via checkboxes, only add those
					$chk_key = 'select_item_' . $item['stock_id'];
					if (!empty($_POST) && !isset($_POST[$chk_key]))
						continue;
					$qty_key = 'qty_' . $item['stock_id'];
					$qty = isset($_POST[$qty_key]) ? (float)$_POST[$qty_key] : (float)$item['quantity'];
					if ($qty <= 0) continue;
					$price = (float)$item['unit_price'] * (1 - (float)$item['discount_percent']);
					add_rma_line($rma_id, $item['stock_id'],
						$item['description'] ? $item['description'] : $item['item_name'],
						$qty, $price);
				}
			}
		}

		display_notification(sprintf(_('RMA #%d has been created.'), $rma_id));
		$Ajax->activate('_page_body');
	} else {
		$Ajax->activate('_page_body');
	}
}

// --- Update existing RMA ---
if (isset($_POST['SAVE_RMA_UPDATE']) && $rma_id > 0) {
	if (can_save_rma_header()) {
		update_sales_rma(
			$rma_id,
			(int)get_post('return_reason_id'),
			get_post('return_method', 'credit_note'),
			get_post('customer_notes', ''),
			get_post('internal_notes', ''),
			(float)get_post('restocking_fee_percent', 0)
		);
		display_notification(_('RMA has been updated.'));
		$Ajax->activate('_page_body');
	} else {
		$Ajax->activate('_page_body');
	}
}

// --- Authorize ---
if (isset($_POST['AUTHORIZE_RMA']) && $rma_id > 0) {
	if (authorize_rma($rma_id, get_post('auth_notes', ''))) {
		display_notification(_('RMA has been authorized.'));
	} else {
		display_error(_('Could not authorize this RMA. It may no longer be in pending status.'));
	}
	$Ajax->activate('_page_body');
}

// --- Reject ---
if (isset($_POST['REJECT_RMA']) && $rma_id > 0) {
	$reason = get_post('rejection_reason', '');
	if (empty($reason)) {
		display_error(_('You must provide a rejection reason.'));
	} elseif (reject_rma($rma_id, $reason)) {
		display_notification(_('RMA has been rejected.'));
	} else {
		display_error(_('Could not reject this RMA.'));
	}
	$Ajax->activate('_page_body');
}

// --- Create WH Return ---
if (isset($_POST['CREATE_WH_RETURN']) && $rma_id > 0) {
	$wh_loc = get_post('wh_location', '');
	if (empty($wh_loc)) {
		display_error(_('Please select a warehouse location for the return.'));
	} else {
		$wh_id = create_wh_return_from_rma($rma_id, $wh_loc);
		if ($wh_id) {
			display_notification(sprintf(_('WH Return Order #%d has been created.'), $wh_id));
		} else {
			display_error(_('Could not create WH Return Order. Make sure the RMA is authorized.'));
		}
	}
	$Ajax->activate('_page_body');
}

// --- Create Credit Note ---
if (isset($_POST['CREATE_CREDIT']) && $rma_id > 0) {
	$credit_no = create_credit_from_rma($rma_id);
	if ($credit_no) {
		display_notification(sprintf(_('Credit Note #%d has been created.'), $credit_no));
	} else {
		display_error(_('Could not create credit note. Ensure the RMA is authorized and has lines.'));
	}
	$Ajax->activate('_page_body');
}

// --- Create Replacement Order ---
if (isset($_POST['CREATE_REPLACEMENT']) && $rma_id > 0) {
	$order_no = create_replacement_from_rma($rma_id);
	if ($order_no) {
		display_notification(sprintf(_('Replacement Sales Order #%d has been created.'), $order_no));
	} else {
		display_error(_('Could not create replacement order.'));
	}
	$Ajax->activate('_page_body');
}

// --- Add manual line ---
if (isset($_POST['ADD_RMA_LINE']) && $rma_id > 0) {
	$stock_id = get_post('line_stock_id', '');
	$qty      = (float)get_post('line_qty', 0);
	$price    = (float)get_post('line_price', 0);
	$cond     = get_post('line_condition', 'good');
	if (!$stock_id) {
		display_error(_('Please select an item.'));
	} elseif ($qty <= 0) {
		display_error(_('Quantity must be greater than zero.'));
	} else {
		$item_row = db_fetch(db_query("SELECT description FROM " . TB_PREF . "stock_master WHERE stock_id = " . db_escape($stock_id), 'get item'));
		add_rma_line($rma_id, $stock_id,
			$item_row ? $item_row['description'] : $stock_id,
			$qty, $price, $cond);
		display_notification(_('Line added.'));
	}
	$Ajax->activate('_page_body');
}

// --- Delete line ---
if (isset($_POST['DELETE_LINE'])) {
	$line_id = (int)get_post('DELETE_LINE');
	if ($line_id > 0) {
		delete_rma_line($line_id);
		display_notification(_('Line removed.'));
	}
	$Ajax->activate('_page_body');
}

// Reload RMA data after any action
if ($rma_id > 0)
	$rma = get_sales_rma($rma_id);
else
	$rma = null;

// Pre-load source document items if coming from invoice/delivery link
$source_items = array();
if ($rma_id < 0 && $source_no_init > 0) {
	if ($source_type_init == ST_SALESINVOICE)
		$src_result = get_invoice_items_for_rma($source_no_init);
	elseif ($source_type_init == ST_CUSTDELIVERY)
		$src_result = get_delivery_items_for_rma($source_no_init);
	else
		$src_result = false;

	if ($src_result)
		while ($row = db_fetch($src_result)) $source_items[] = $row;
}

// ============================================================================
// DISPLAY HELPERS
// ============================================================================

/**
 * Render an inline dropdown list of return reasons.
 *
 * @param string $name    Field name
 * @param int    $selected Currently selected ID
 * @return void
 */
function rma_reason_list($name, $selected = 0)
{
	$result = get_rma_reasons();
	$items = array(0 => _('-- Select Reason --'));
	while ($row = db_fetch($result))
		$items[$row['id']] = $row['description'];
	return array_selector($name, $selected, $items);
}

// ============================================================================
// MAIN FORM
// ============================================================================
start_form();
hidden('selected_id', $rma_id);

// Status banner for existing RMA
if ($rma) {
	$status_color = get_rma_status_color($rma['status']);
	$statuses = get_rma_statuses();
	$status_label = isset($statuses[$rma['status']]) ? $statuses[$rma['status']] : $rma['status'];
	echo '<div style="background:' . $status_color . '; color:#fff; padding:6px 14px; border-radius:4px; margin-bottom:10px; font-weight:bold;">'
		. sprintf(_('RMA #%d — %s — %s'), $rma['id'], $rma['reference'], $status_label)
		. '</div>';
}

// ============ HEADER SECTION ============
start_table(TABLESTYLE2);
$th = array(_('RMA Details'), '');
table_header($th);

if ($rma) {
	// View mode: show customer/branch read-only
	label_row(_('Customer'), $rma['customer_name']);
	label_row(_('Branch'), $rma['branch_name']);
	label_row(_('Request Date'), sql2date($rma['request_date']));
	if ($rma['source_no'] > 0) {
		$src_types = array(ST_SALESINVOICE => _('Invoice'), ST_CUSTDELIVERY => _('Delivery'));
		$src_label = isset($src_types[$rma['source_type']]) ? $src_types[$rma['source_type']] : _('Document');
		label_row(_('Source Document'), $src_label . ' #' . $rma['source_no']);
	}
	// Editable fields if still pending
	if ($rma['status'] === 'pending') {
		label_cells(_('Return Reason'), '', '', '');
		start_row(); label_cell(_('Return Reason'));
		hidden('return_reason_id_view', $rma['return_reason_id']);
		echo '<td>' . rma_reason_list('return_reason_id', (int)$rma['return_reason_id']) . '</td>'; end_row();

		start_row(); label_cell(_('Return Method'));
		echo '<td>';
		echo array_selector('return_method', $rma['return_method'], get_rma_return_methods());
		echo '</td>'; end_row();

		start_row(); label_cell(_('Restocking Fee %'));
		echo '<td>'; text_cells_ex(null, 'restocking_fee_percent', 8, 8, $rma['restocking_fee_percent']); echo '</td>'; end_row();

		start_row(); label_cell(_('Customer Notes'));
		echo '<td><textarea name="customer_notes" rows="2" style="width:350px">' . htmlspecialchars($rma['customer_notes']) . '</textarea></td>'; end_row();

		start_row(); label_cell(_('Internal Notes'));
		echo '<td><textarea name="internal_notes" rows="2" style="width:350px">' . htmlspecialchars($rma['internal_notes']) . '</textarea></td>'; end_row();
	} else {
		// Read-only display for non-pending RMAs
		$methods = get_rma_return_methods();
		label_row(_('Return Reason'), $rma['reason_description']);
		label_row(_('Return Method'), isset($methods[$rma['return_method']]) ? $methods[$rma['return_method']] : $rma['return_method']);
		label_row(_('Restocking Fee %'), number_format2((float)$rma['restocking_fee_percent'], 2) . '%');
		if ($rma['customer_notes'])
			label_row(_('Customer Notes'), nl2br(htmlspecialchars($rma['customer_notes'])));
		if ($rma['internal_notes'])
			label_row(_('Internal Notes'), nl2br(htmlspecialchars($rma['internal_notes'])));
	}

	// Totals
	label_row(_('Total Amount'), price_format((float)$rma['total_amount']));
	if ((float)$rma['restocking_fee_amount'] > 0)
		label_row(_('Restocking Fee'), price_format((float)$rma['restocking_fee_amount']));
	label_row(_('<b>Refund Amount</b>'), '<b>' . price_format((float)$rma['refund_amount']) . '</b>');

	// Linked documents
	if ($rma['wh_return_order_id'] > 0)
		label_row(_('WH Return Order'), '#' . $rma['wh_return_order_id']
			. ' &nbsp;<a href="' . $path_to_root . '/inventory/warehouse/returns.php?selected_id=' . (int)$rma['wh_return_order_id'] . '">' . _('View') . '</a>');
	if ($rma['credit_note_no'] > 0)
		label_row(_('Credit Note'), '#' . $rma['credit_note_no']
			. ' &nbsp;<a href="' . $path_to_root . '/sales/view/view_credit_note.php?trans_no=' . (int)$rma['credit_note_no'] . '">' . _('View') . '</a>');
	if ($rma['replacement_order_no'] > 0)
		label_row(_('Replacement Order'), '#' . $rma['replacement_order_no']
			. ' &nbsp;<a href="' . $path_to_root . '/sales/sales_order_entry.php?OrderNumber=' . (int)$rma['replacement_order_no'] . '">' . _('View') . '</a>');

} else {
	// New RMA entry
	start_row();
	label_cell(_('Customer'));
	echo '<td>'.customer_list('debtor_no', get_post('debtor_no', 0), false, true).'</td>';
	end_row();

	start_row();
	label_cell(_('Branch'));
	echo '<td>'.customer_branches_list('debtor_no', 'branch_code', get_post('branch_code', 0)).'</td>';
	end_row();

	date_row(_('Request Date'), 'request_date', '', null, 0, 0, 0, null, true);

	start_row();

	$src_types = array(0 => _('None'),
		ST_SALESINVOICE => _('Sales Invoice'),
		ST_CUSTDELIVERY => _('Customer Delivery'));
	label_cell(_('Source Document Type'));
	echo '<td>'.array_selector('source_type', $source_type_init ?: get_post('source_type', 0), $src_types).'</td>';
	end_row();

	start_row();
	label_cell(_('Source Document #'));
	text_cells_ex(null, 'source_no', 10, 10, $source_no_init ?: get_post('source_no', ''));
	end_row();

	start_row();
	label_cell(_('Return Reason'));
	echo '<td>'.rma_reason_list('return_reason_id', (int)get_post('return_reason_id', 0)).'</td>';
	end_row();

	start_row(); label_cell(_('Return Method'));
	echo '<td>'.array_selector('return_method', get_post('return_method', 'credit_note'), get_rma_return_methods()).'</td>';
	end_row();

	start_row();
	label_cell(_('Restocking Fee %'));
	text_cells_ex(null, 'restocking_fee_percent', 8, 8, get_post('restocking_fee_percent', 0));
	end_row();

	start_row();
	label_cell(_('Customer Notes'));
	echo '<td><textarea name="customer_notes" rows="2">' . htmlspecialchars(get_post('customer_notes', '')) . '</textarea></td>';
	end_row();

	start_row();
	label_cell(_('Internal Notes'));
	echo '<td><textarea name="internal_notes" rows="2">' . htmlspecialchars(get_post('internal_notes', '')) . '</textarea></td>';
	end_row();
}

end_table(1);

// ============ SOURCE ITEMS SELECTOR (new RMA from document) ============
if (!$rma && !empty($source_items)) {
	echo '<p><b>' . _('Select items to return:') . '</b></p>';
	start_table(TABLESTYLE);
	$th = array(_(''), _('Item Code'), _('Description'), _('Qty'), _('Return Qty'));
	table_header($th);
	$k = 0;
	foreach ($source_items as $item) {
		alt_table_row_color($k);
		echo '<td><input type="checkbox" name="select_item_' . htmlspecialchars($item['stock_id'])
			. '" value="1" checked></td>';
		label_cell($item['stock_id']);
		label_cell($item['item_name'] ? $item['item_name'] : $item['description']);
		label_cell(number_format2((float)$item['quantity'], get_qty_dec($item['stock_id'])));
		echo '<td><input type="text" name="qty_' . htmlspecialchars($item['stock_id'])
			. '" value="' . number_format2((float)$item['quantity'], get_qty_dec($item['stock_id']))
			. '" size="8"></td>';
		end_row();
	}
	end_table(1);
}

// ============ LINE ITEMS TABLE (existing RMA) ============
if ($rma) {
	echo '<h3 style="margin-top:16px;">' . _('Return Lines') . '</h3>';
	start_table(TABLESTYLE);
	$conditions = array('new' => _('New'), 'good' => _('Good'), 'damaged' => _('Damaged'), 'defective' => _('Defective'));
	$th = array(_('Item Code'), _('Description'), _('Requested'), _('Authorized'), _('Unit Price'), _('Condition'), _('Serial #'), _('Batch #'), _('Notes'), '');
	table_header($th);
	$k = 0;
	$lines_result = get_rma_lines($rma['id']);
	while ($line = db_fetch($lines_result)) {
		alt_table_row_color($k);
		label_cell($line['stock_id']);
		label_cell($line['description'] ? $line['description'] : $line['item_name']);
		label_cell(number_format2((float)$line['quantity_requested'], get_qty_dec($line['stock_id'])));
		label_cell(number_format2((float)$line['quantity_authorized'], get_qty_dec($line['stock_id'])));
		label_cell(price_format((float)$line['unit_price']));
		$cond_label = isset($conditions[$line['return_condition']]) ? $conditions[$line['return_condition']] : $line['return_condition'];
		label_cell($cond_label);
		label_cell($line['serial_number']);
		label_cell($line['batch_number']);
		label_cell($line['notes']);
		if ($rma['status'] === 'pending') {
			echo '<td>';
			echo '<button class="ajaxsubmit" type="submit" name="DELETE_LINE" value="'
				. (int)$line['id'] . '"><span>' . _('Remove') . '</span></button>';
			echo '</td>';
		} else {
			label_cell('');
		}
		end_row();
	}
	end_table(1);

	// Add line form (only in pending status)
	if ($rma['status'] === 'pending') {
		echo '<b>' . _('Add Line') . '</b><br>';
		start_table(TABLESTYLE2);
		start_row();
		echo '<td>' . _('Item') . ':</td><td>';
		echo stock_items_list('line_stock_id', get_post('line_stock_id', ''), false, true);
		echo '</td><td>' . _('Qty') . ':</td><td>';
		text_cells_ex(null, 'line_qty', 8, 8, get_post('line_qty', 1));
		echo '</td><td>' . _('Price') . ':</td><td>';
		text_cells_ex(null, 'line_price', 10, 10, get_post('line_price', 0));
		echo '</td><td>' . _('Condition') . ':</td><td>';
		echo array_selector('line_condition', get_post('line_condition', 'good'), $conditions);
		echo '</td><td>';
		submit('ADD_RMA_LINE', _('Add Line'), true, '', ICON_ADD);
		echo '</td>';
		end_row();
		end_table(1);
	}
}

// ============ ACTION BUTTONS ============
div_start('action_buttons');
start_table();
start_row();

if (!$rma) {
	submit_center('SAVE_RMA_NEW', _('Create RMA'), true, '', ICON_SUBMIT);
} elseif ($rma['status'] === 'pending') {
	submit('SAVE_RMA_UPDATE', _('Save Changes'), true, '', ICON_SUBMIT);
	echo '&nbsp;';
	// Authorize button
	echo '<input type="text" name="auth_notes" placeholder="' . _('Authorization note (optional)') . '" style="width:200px">&nbsp;';
	submit('AUTHORIZE_RMA', _('Authorize'), true, '', ICON_SUBMIT);
	echo '&nbsp;';
	// Reject button
	echo '<input type="text" name="rejection_reason" placeholder="' . _('Rejection reason') . '" style="width:200px">&nbsp;';
	submit('REJECT_RMA', _('Reject'), true, '', ICON_DELETE);
} elseif ($rma['status'] === 'authorized') {
	// WH Return creation
	if ($rma['wh_return_order_id'] == 0) {
		echo '<td>' . _('Warehouse') . ': ';
		echo locations_list('wh_location', get_post('wh_location', ''));
		echo '</td><td>&nbsp;';
		submit('CREATE_WH_RETURN', _('Create WH Return'), true, '', ICON_SUBMIT);
		echo '</td><td>&nbsp;</td>';
	}
	// Credit note / replacement
	if ($rma['credit_note_no'] == 0) {
		if ($rma['return_method'] === 'replacement') {
			if ($rma['replacement_order_no'] == 0)
				submit('CREATE_REPLACEMENT', _('Create Replacement Order'), true, '', ICON_SUBMIT);
		} else {
			submit('CREATE_CREDIT', _('Create Credit Note'), true, '', ICON_SUBMIT);
		}
	}
} elseif ($rma['status'] === 'wh_processing') {
	if ($rma['credit_note_no'] == 0) {
		if ($rma['return_method'] === 'replacement') {
			if ($rma['replacement_order_no'] == 0)
				submit('CREATE_REPLACEMENT', _('Create Replacement Order'), true, '', ICON_SUBMIT);
		} else {
			submit('CREATE_CREDIT', _('Create Credit Note'), true, '', ICON_SUBMIT);
		}
	}
}

end_row();
end_table(0);
div_end();

end_form();
end_page();
