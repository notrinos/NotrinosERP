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
 * Sales Agreement Entry
 *
 * Full CRUD for sales agreements (blanket orders, framework agreements, and
 * contracts) with inline line item editor.  Supports Draft → Confirmed →
 * Active lifecycle and "Create Sales Order" action for active agreements.
 *
 * @package NotrinosERP
 * @subpackage Sales
 */
$page_security = 'SA_SALESAGREEMENT';
$path_to_root  = '..';

include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/includes/date_functions.inc');
include_once($path_to_root . '/includes/data_checks.inc');
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/includes/ui/ui_lists.inc');
include_once($path_to_root . '/sales/includes/db/sales_agreement_db.inc');

$js = '';
if ($SysPrefs->use_popup_windows && $SysPrefs->use_popup_search)
	$js .= get_js_open_window(900, 600);
if (user_use_date_picker())
	$js .= get_js_date_picker();

page(_($help_context = 'Sales Agreement Entry'), false, false, '', $js);

// ============================================================================
// VALIDATION
// ============================================================================

/**
 * Validate the agreement header form data before save.
 *
 * @return bool
 */
function can_save_sales_agreement_header()
{
	if (!get_post('debtor_no') || get_post('debtor_no') == ALL_TEXT) {
		display_error(_('You must select a customer.'));
		set_focus('debtor_no');
		return false;
	}

	if (!is_date(get_post('date_start'))) {
		display_error(_('The agreement start date is invalid.'));
		set_focus('date_start');
		return false;
	}

	if (get_post('date_end') !== '' && !is_date(get_post('date_end'))) {
		display_error(_('The agreement end date is invalid.'));
		set_focus('date_end');
		return false;
	}

	if (get_post('date_end') !== '' && is_date(get_post('date_end'))
		&& date1_greater_date2(get_post('date_start'), get_post('date_end')))
	{
		display_error(_('The agreement end date cannot be earlier than the start date.'));
		set_focus('date_end');
		return false;
	}

	return true;
}

/**
 * Validate one agreement line form submission.
 *
 * @return bool
 */
function can_save_sales_agreement_line()
{
	if (get_post('line_stock_id') === '') {
		display_error(_('You must select an item.'));
		set_focus('line_stock_id');
		return false;
	}

	if (!check_num('line_committed_qty', 0.001)) {
		display_error(_('The committed quantity must be greater than zero.'));
		set_focus('line_committed_qty');
		return false;
	}

	if (!check_num('line_unit_price', 0)) {
		display_error(_('The unit price must be numeric and not less than zero.'));
		set_focus('line_unit_price');
		return false;
	}

	if (!check_num('line_discount_percent', 0, 100)) {
		display_error(_('The discount percent must be between 0 and 100.'));
		set_focus('line_discount_percent');
		return false;
	}

	if (get_post('line_price_valid_until') !== '' && !is_date(get_post('line_price_valid_until'))) {
		display_error(_('The line price validity date is invalid.'));
		set_focus('line_price_valid_until');
		return false;
	}

	return true;
}

// ============================================================================
// REQUEST HANDLING
// ============================================================================

$selected_id = get_post('selected_id', 0);
if (!$selected_id && isset($_GET['agreement_id']))
	$selected_id = (int)$_GET['agreement_id'];
if (isset($_GET['New']))
	$selected_id = 0;

// Notice display after redirect
if (isset($_GET['notice'])) {
	$notice_map = array(
		'confirmed' => _('Sales agreement has been confirmed.'),
		'activated' => _('Sales agreement has been activated.'),
		'expired'   => _('Sales agreement has been expired.'),
		'cancelled' => _('Sales agreement has been cancelled.'),
		'deleted'   => _('Sales agreement has been deleted.'),
	);

	if (isset($notice_map[$_GET['notice']]))
		display_notification($notice_map[$_GET['notice']]);
}

if (isset($_GET['created_so']) && (int)$_GET['created_so'] > 0) {
	$so_number = (int)$_GET['created_so'];
	display_notification(_('Sales order has been created from this agreement.'));
	display_note(get_trans_view_str(ST_SALESORDER, $so_number, _('View Sales Order') . ' #' . $so_number), 0, 1);
}

// Auto-fill currency and payment terms when customer changes on new agreement
if (list_updated('debtor_no') && !$selected_id) {
	$debtor_no = (int)get_post('debtor_no');
	if ($debtor_no > 0) {
		$customer = get_customer($debtor_no);
		if ($customer) {
			$_POST['currency']      = $customer['curr_code'];
			$_POST['payment_terms'] = $customer['payment_terms'];
		}
	}
}

// ---- Save header ----
if ((isset($_POST['ADD_AGREEMENT']) || isset($_POST['UPDATE_AGREEMENT'])) && can_save_sales_agreement_header()) {
	$debtor_no     = (int)get_post('debtor_no');
	$branch_code   = get_post('branch_code') == ALL_TEXT ? 0 : (int)get_post('branch_code');
	$salesman_id   = get_post('salesman_id') == ALL_TEXT ? 0 : (int)get_post('salesman_id');
	$date_start    = date2sql(get_post('date_start'));
	$date_end      = get_post('date_end') !== '' ? date2sql(get_post('date_end')) : null;
	$agreement_type = get_post('agreement_type', 'blanket_order');
	$currency      = trim(get_post('currency'));
	$payment_terms = get_post('payment_terms') == ALL_TEXT ? 0 : (int)get_post('payment_terms');
	$auto_renew    = check_value('auto_renew') ? 1 : 0;
	$renewal_period_months = max(1, (int)get_post('renewal_period_months'));
	$terms_and_conditions  = trim(get_post('terms_and_conditions'));
	$notes         = trim(get_post('notes'));
	$dimension_id  = (int)get_post('dimension_id');
	$dimension2_id = (int)get_post('dimension2_id');
	$reference     = trim(get_post('reference'));

	if (isset($_POST['ADD_AGREEMENT'])) {
		$selected_id = add_sales_agreement(
			$agreement_type, $debtor_no, $branch_code, $date_start, $date_end,
			$currency, $payment_terms, $auto_renew, $renewal_period_months,
			$terms_and_conditions, $notes, $dimension_id, $dimension2_id,
			$reference, $salesman_id
		);

		if ($selected_id) {
			display_notification(_('Sales agreement has been created.'));
			meta_forward($_SERVER['PHP_SELF'], 'agreement_id=' . $selected_id);
		} else {
			display_error(_('The sales agreement could not be created.'));
		}
	} else {
		if (update_sales_agreement(
			$selected_id, $agreement_type, $debtor_no, $branch_code, $date_start, $date_end,
			$currency, $payment_terms, $auto_renew, $renewal_period_months,
			$terms_and_conditions, $notes, $dimension_id, $dimension2_id,
			$reference, $salesman_id
		)) {
			display_notification(_('Sales agreement has been updated.'));
		} else {
			display_error(_('The sales agreement could not be updated (only draft agreements can be edited).'));
		}
	}
}

// ---- Add line ----
if (isset($_POST['AddLine']) && $selected_id > 0 && can_save_sales_agreement_line()) {
	$line_id = add_sales_agreement_line(
		$selected_id,
		get_post('line_stock_id'),
		input_num('line_committed_qty'),
		input_num('line_unit_price'),
		input_num('line_discount_percent'),
		trim(get_post('line_description')),
		input_num('line_min_qty', 0),
		get_post('line_price_valid_until') !== '' ? date2sql(get_post('line_price_valid_until')) : null
	);

	if ($line_id)
		display_notification(_('Agreement line has been added.'));
	else
		display_error(_('The agreement line could not be added.'));
}

// ---- Update existing line ----
$update_line_id = find_submit('UpdateLine');
if ($update_line_id > 0 && $selected_id > 0) {
	$updated = update_sales_agreement_line(
		$update_line_id,
		get_post('edit_stock_' . $update_line_id),
		input_num('edit_committed_qty_' . $update_line_id),
		input_num('edit_unit_price_' . $update_line_id),
		input_num('edit_discount_' . $update_line_id),
		trim(get_post('edit_description_' . $update_line_id)),
		input_num('edit_min_qty_' . $update_line_id, 0),
		get_post('edit_price_valid_until_' . $update_line_id) !== ''
			? date2sql(get_post('edit_price_valid_until_' . $update_line_id))
			: null
	);

	if ($updated)
		display_notification(_('Agreement line has been updated.'));
	else
		display_error(_('The agreement line could not be updated.'));
}

// ---- Delete line ----
$delete_line_id = find_submit('DeleteLine');
if ($delete_line_id > 0 && $selected_id > 0) {
	if (delete_sales_agreement_line($delete_line_id))
		display_notification(_('Agreement line has been deleted.'));
	else
		display_error(_('The agreement line could not be deleted.'));
}

// ---- Status transitions ----
if (isset($_POST['DeleteAgreement']) && $selected_id > 0) {
	if (delete_sales_agreement($selected_id)) {
		meta_forward($_SERVER['PHP_SELF'], 'notice=deleted');
	} else {
		display_error(_('Only draft sales agreements can be deleted.'));
	}
}

if (isset($_POST['ConfirmAgreement']) && $selected_id > 0) {
	if (confirm_sales_agreement($selected_id)) {
		meta_forward($_SERVER['PHP_SELF'], 'agreement_id=' . $selected_id . '&notice=confirmed');
	} else {
		display_error(_('The agreement could not be confirmed. Ensure at least one line has been added.'));
	}
}

if (isset($_POST['ActivateAgreement']) && $selected_id > 0) {
	if (activate_sales_agreement($selected_id))
		meta_forward($_SERVER['PHP_SELF'], 'agreement_id=' . $selected_id . '&notice=activated');
	else
		display_error(_('The agreement could not be activated.'));
}

if (isset($_POST['ExpireAgreement']) && $selected_id > 0) {
	if (expire_sales_agreement($selected_id))
		meta_forward($_SERVER['PHP_SELF'], 'agreement_id=' . $selected_id . '&notice=expired');
	else
		display_error(_('The agreement could not be expired.'));
}

if (isset($_POST['CancelAgreement']) && $selected_id > 0) {
	$cancellation_reason = trim(get_post('cancellation_reason'));
	if ($cancellation_reason === '') {
		display_error(_('Please enter a cancellation reason.'));
		set_focus('cancellation_reason');
	} elseif (cancel_sales_agreement($selected_id, $cancellation_reason)) {
		meta_forward($_SERVER['PHP_SELF'], 'agreement_id=' . $selected_id . '&notice=cancelled');
	} else {
		display_error(_('The agreement could not be cancelled.'));
	}
}

// ---- Create SO from agreement ----
if (isset($_POST['CreateSO']) && $selected_id > 0) {
	$so_number = create_order_from_agreement($selected_id);
	if ($so_number > 0) {
		meta_forward($_SERVER['PHP_SELF'], 'agreement_id=' . $selected_id . '&created_so=' . $so_number);
	} else {
		display_error(_('No sales order could be created from this agreement. Ensure the agreement is active and has valid lines.'));
	}
}

// ============================================================================
// DISPLAY SETUP
// ============================================================================

$agreement = $selected_id > 0 ? get_sales_agreement($selected_id) : false;
$editable  = !$agreement || in_array($agreement['status'], get_sales_agreement_editable_statuses());

if ($agreement) {
	$_POST['agreement_type']        = $agreement['agreement_type'];
	$_POST['debtor_no']             = $agreement['debtor_no'];
	$_POST['branch_code']           = $agreement['branch_code'];
	$_POST['salesman_id']           = $agreement['salesman_id'];
	$_POST['date_start']            = sql2date($agreement['date_start']);
	$_POST['date_end']              = $agreement['date_end'] ? sql2date($agreement['date_end']) : '';
	$_POST['currency']              = $agreement['currency'];
	$_POST['payment_terms']         = $agreement['payment_terms'];
	$_POST['auto_renew']            = $agreement['auto_renew'];
	$_POST['renewal_period_months'] = $agreement['renewal_period_months'];
	$_POST['terms_and_conditions']  = $agreement['terms_and_conditions'];
	$_POST['notes']                 = $agreement['notes'];
	$_POST['dimension_id']          = $agreement['dimension_id'];
	$_POST['dimension2_id']         = $agreement['dimension2_id'];
	$_POST['reference']             = $agreement['reference'];
} else {
	if (!isset($_POST['agreement_type']))    $_POST['agreement_type']        = 'blanket_order';
	if (!isset($_POST['date_start']))        $_POST['date_start']            = Today();
	if (!isset($_POST['currency']))          $_POST['currency']              = get_company_pref('curr_default');
	if (!isset($_POST['payment_terms']))     $_POST['payment_terms']         = 0;
	if (!isset($_POST['renewal_period_months'])) $_POST['renewal_period_months'] = 12;
	if (!isset($_POST['dimension_id']))      $_POST['dimension_id']          = 0;
	if (!isset($_POST['dimension2_id']))     $_POST['dimension2_id']         = 0;
}

$agreement_types = get_sales_agreement_types();

// ============================================================================
// PAGE RENDERING
// ============================================================================

start_form();
echo '<input type="hidden" name="selected_id" value="' . (int)$selected_id . '">';

// Page header row
echo '<div style="display:flex;justify-content:space-between;align-items:center;gap:12px;flex-wrap:wrap;">';
echo '<div>';
echo '<h2 style="margin:0;">';
echo $agreement
	? htmlspecialchars($agreement['reference'])
	: _('New Sales Agreement');
echo '</h2>';
if ($agreement) {
	echo '<div style="margin-top:6px;">' . sales_agreement_status_badge($agreement['status']) . '</div>';
}
echo '</div>';
echo '<div>';
hyperlink_params($path_to_root . '/sales/inquiry/sales_agreements_view.php', _('Back to Agreement Inquiry'), '');
echo '</div>';
echo '</div>';

br();

if (!$agreement)
	display_note(_('Save the agreement header first, then add line items.'), 0, 1);

start_outer_table();
table_section(1);

if ($editable) {
	echo '<tr><td class="label">' . _('Agreement Type:') . '</td><td>';
	echo array_selector('agreement_type', get_post('agreement_type'), $agreement_types,
		array('class' => array('nosearch')));
	echo '</td></tr>';
	customer_list_row(_('Customer:'), 'debtor_no', get_post('debtor_no'), true, true);
	customer_branches_list_row(_('Branch:'), get_post('debtor_no'), 'branch_code', get_post('branch_code'), true, true);
	sales_persons_list_row(_('Salesman:'), 'salesman_id', get_post('salesman_id'), true);
	date_row(_('Start Date:'), 'date_start');
	date_row(_('End Date:'), 'date_end');
	text_row(_('Currency:'), 'currency', get_post('currency'), 8, 8);
	payment_terms_list_row(_('Payment Terms:'), 'payment_terms', get_post('payment_terms'));

	table_section(2);
	
	check_row(_('Auto Renew:'), 'auto_renew', get_post('auto_renew'));
	text_row(_('Renewal Period (Months):'), 'renewal_period_months',
		get_post('renewal_period_months'), 6, 6);
	text_row(_('Reference:'), 'reference', get_post('reference'), 20, 60);
	dimensions_list_row(_('Dimension:'), 'dimension_id', get_post('dimension_id'), true);
	dimensions_list_row(_('Dimension 2:'), 'dimension2_id', get_post('dimension2_id'), true);
	textarea_row(_('Terms and Conditions:'), 'terms_and_conditions',
		get_post('terms_and_conditions'), 35, 4);
	textarea_row(_('Notes:'), 'notes', get_post('notes'), 35, 4);
} else {
	label_row(_('Agreement Type:'), $agreement_types[$agreement['agreement_type']]);
	label_row(_('Customer:'), $agreement['customer_name']);
	label_row(_('Branch:'), $agreement['branch_name'] ? $agreement['branch_name'] : '-');
	label_row(_('Salesman:'), $agreement['salesman_name'] ? $agreement['salesman_name'] : '-');
	label_row(_('Start Date:'), sql2date($agreement['date_start']));
	label_row(_('End Date:'), $agreement['date_end'] ? sql2date($agreement['date_end']) : '-');
	label_row(_('Currency:'), $agreement['currency']);
	label_row(_('Payment Terms:'), $agreement['payment_terms_name'] ? $agreement['payment_terms_name'] : '-');

	table_section(2);
	
	label_row(_('Reference:'), $agreement['reference']);
	label_row(_('Auto Renew:'), $agreement['auto_renew'] ? _('Yes') : _('No'));
	label_row(_('Renewal Period:'), (int)$agreement['renewal_period_months'] . ' ' . _('months'));
	label_row(_('Terms and Conditions:'),
		$agreement['terms_and_conditions']
			? nl2br(htmlspecialchars($agreement['terms_and_conditions']))
			: '-');
	label_row(_('Notes:'), $agreement['notes'] ? nl2br(htmlspecialchars($agreement['notes'])) : '-');

	// Totals
	label_row(_('Committed Total:'), price_format($agreement['total_committed']));
	label_row(_('Ordered Total:'),   price_format($agreement['total_ordered']));
	label_row(_('Delivered Total:'), price_format($agreement['total_delivered']));
	label_row(_('Invoiced Total:'),  price_format($agreement['total_invoiced']));
}

end_outer_table();

if ($editable) {
	if ($agreement)
		submit_center('UPDATE_AGREEMENT', _('Update Agreement'), true, '', 'default');
	else
		submit_center('ADD_AGREEMENT', _('Create Agreement'), true, '', 'default');
}

// Progress cards (for non-draft agreements)
if ($agreement && !in_array($agreement['status'], array('draft'))) {
	echo '<div style="display:grid;grid-template-columns:repeat(auto-fit,minmax(200px,1fr));gap:12px;margin:16px 0;">';
	$cards = array(
		array(_('Ordered Progress'),   $agreement['ordered_progress_pct'],   '#0d6efd'),
		array(_('Delivered Progress'), $agreement['delivered_progress_pct'], '#28a745'),
		array(_('Invoiced Progress'),  $agreement['invoiced_progress_pct'],  '#fd7e14'),
	);
	foreach ($cards as $card) {
		echo '<div style="background:#fff;border:1px solid #ddd;border-radius:6px;padding:10px 14px;">';
		echo '<div style="font-size:13px;color:#666;">' . $card[0] . '</div>';
		echo '<div style="font-size:20px;font-weight:bold;margin:6px 0;">' . number_format2($card[1], 2) . '%</div>';
		echo '<div style="height:8px;background:#e9ecef;border-radius:999px;overflow:hidden;">';
		echo '<div style="width:' . min(100, (float)$card[1]) . '%;height:8px;background:' . $card[2] . ';"></div>';
		echo '</div></div>';
	}
	echo '</div>';
}

// ---- Agreement lines grid ----
if ($agreement) {
	display_heading(_('Agreement Lines'));
	start_table(TABLESTYLE, "width='100%'");

	$th = array(
		_('Item'), _('Description'), _('Committed Qty'), _('Min Qty / Order'),
		_('Unit Price'), _('Discount %'), _('Remaining'), _('Ordered'),
		_('Delivered'), _('Invoiced'), _('Price Valid Until'),
	);
	if ($editable) $th[] = '';
	table_header($th);

	$k = 0;
	$agreement_lines = get_sales_agreement_lines($selected_id);
	while ($line = db_fetch($agreement_lines)) {
		$remaining_qty     = max(0, (float)$line['committed_qty'] - (float)$line['ordered_qty']);
		$effective_price   = round(
			(float)$line['unit_price'] * (1 - ((float)$line['discount_percent'] / 100)),
			user_price_dec()
		);
		alt_table_row_color($k);

		if ($editable) {
			label_cell($line['stock_id']);
			echo '<td><input type="text" name="edit_description_' . $line['id'] . '" value="'
				. htmlspecialchars($line['description'] ? $line['description'] : $line['item_description'], ENT_QUOTES, 'UTF-8')
				. '" size="22"></td>';
			echo '<td><input type="text" name="edit_committed_qty_' . $line['id'] . '" value="'
				. number_format2($line['committed_qty'], get_qty_dec($line['stock_id']))
				. '" size="8" class="amount"></td>';
			echo '<td><input type="text" name="edit_min_qty_' . $line['id'] . '" value="'
				. number_format2($line['min_qty_per_order'], get_qty_dec($line['stock_id']))
				. '" size="8" class="amount"></td>';
			echo '<td><input type="text" name="edit_unit_price_' . $line['id'] . '" value="'
				. number_format2($line['unit_price'], user_price_dec())
				. '" size="8" class="amount"></td>';
			echo '<td><input type="text" name="edit_discount_' . $line['id'] . '" value="'
				. number_format2($line['discount_percent'], 2)
				. '" size="6" class="amount"></td>';
			label_cell(number_format2($remaining_qty, get_qty_dec($line['stock_id'])));
			label_cell(render_sales_agreement_line_progress(
				(float)$line['ordered_qty'],   (float)$line['committed_qty'], $line['stock_id'], '#0d6efd'));
			label_cell(render_sales_agreement_line_progress(
				(float)$line['delivered_qty'], (float)$line['committed_qty'], $line['stock_id'], '#28a745'));
			label_cell(render_sales_agreement_line_progress(
				(float)$line['invoiced_qty'],  (float)$line['committed_qty'], $line['stock_id'], '#fd7e14'));
			echo '<td><input type="text" name="edit_price_valid_until_' . $line['id'] . '" value="'
				. ($line['price_valid_until'] ? sql2date($line['price_valid_until']) : '')
				. '" size="10"></td>';
			echo '<td nowrap>';
			echo '<input type="hidden" name="edit_stock_' . $line['id'] . '" value="'
				. htmlspecialchars($line['stock_id'], ENT_QUOTES, 'UTF-8') . '">';
			echo '<input type="submit" class="inputsubmit" name="UpdateLine' . $line['id']
				. '" value="' . _('Update') . '">';
			echo '&nbsp;';
			echo '<input type="submit" class="inputsubmit" name="DeleteLine' . $line['id']
				. '" value="' . _('Delete') . '">';
			echo '</td>';
		} else {
			label_cell($line['stock_id']);
			label_cell($line['description'] ? $line['description'] : $line['item_description']);
			qty_cell($line['committed_qty']);
			qty_cell($line['min_qty_per_order']);
			amount_cell($effective_price);
			label_cell(number_format2($line['discount_percent'], 2));
			qty_cell($remaining_qty);
			label_cell(render_sales_agreement_line_progress(
				(float)$line['ordered_qty'],   (float)$line['committed_qty'], $line['stock_id'], '#0d6efd'));
			label_cell(render_sales_agreement_line_progress(
				(float)$line['delivered_qty'], (float)$line['committed_qty'], $line['stock_id'], '#28a745'));
			label_cell(render_sales_agreement_line_progress(
				(float)$line['invoiced_qty'],  (float)$line['committed_qty'], $line['stock_id'], '#fd7e14'));
			label_cell($line['price_valid_until'] ? sql2date($line['price_valid_until']) : '-');
		}

		$k++;
		end_row();
	}

	if ($k == 0) {
		$colspan = $editable ? 12 : 11;
		label_row('', _('No agreement lines have been added yet.'), "colspan=$colspan align='center'");
	}

	end_table(1);

	// ---- Add line form ----
	if ($editable) {
		display_heading(_('Add Line'));
		start_table(TABLESTYLE2);
		start_row();
		label_cell(_('Select Item:'));
		stock_costable_items_list_cells(null, 'line_stock_id', null, false, true);
		end_row();
		text_row(_('Description:'), 'line_description', null, 40, 255);
		qty_row(_('Committed Quantity:'), 'line_committed_qty', null, null, null, get_qty_dec(''));
		qty_row(_('Min Qty / Order:'), 'line_min_qty', null, null, null, get_qty_dec(''));
		amount_row(_('Unit Price:'), 'line_unit_price');
		amount_row(_('Discount Percent:'), 'line_discount_percent');
		date_row(_('Price Valid Until:'), 'line_price_valid_until');
		end_table(1);
		submit_center('AddLine', _('Add Line'), true, '', 'default');
	}

	br();

	// ---- Action buttons ----
	echo '<div style="text-align:center;margin-top:18px;">';
	if ($agreement['status'] === 'draft') {
		submit('ConfirmAgreement', _('Confirm Agreement'), true, _('Confirm this agreement'));
		echo '&nbsp;';
		submit('DeleteAgreement', _('Delete Agreement'), true, _('Delete this draft agreement'));
	} elseif ($agreement['status'] === 'confirmed') {
		submit('ActivateAgreement', _('Activate Agreement'), true, _('Activate this agreement'));
		echo '&nbsp;';
		submit('CancelAgreement', _('Cancel Agreement'), true, _('Cancel this agreement'));
	} elseif ($agreement['status'] === 'active') {
		submit('CreateSO', _('Create Sales Order'), true, _('Create sales order from this agreement'));
		echo '&nbsp;';
		submit('ExpireAgreement', _('Expire Agreement'), true, _('Manually expire this agreement'));
		echo '&nbsp;';
		submit('CancelAgreement', _('Cancel Agreement'), true, _('Cancel this agreement'));
	}
	if (in_array($agreement['status'], array('confirmed', 'active'))) {
		echo '<div style="max-width:420px;margin:12px auto 0;">';
		textarea_row(_('Cancellation Reason:'), 'cancellation_reason', null, 300, 5);
		echo '</div>';
	}
	echo '</div>';
}

end_form();
end_page();
