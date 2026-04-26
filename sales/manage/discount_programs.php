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
$page_security = 'SA_SALESDISCOUNT';
$path_to_root = '../..';
include_once($path_to_root . '/includes/session.inc');

page(_($help_context = 'Discount Programs'));

include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/sales/includes/db/sales_discount_db.inc');

simple_page_mode(true);

// ============================================================================
// VALIDATION HELPERS
// ============================================================================

/**
 * Validate the discount program form fields.
 *
 * @return bool True if all required fields are valid
 */
function can_process_discount_program() {
	if (strlen(trim(get_post('prog_name'))) == 0) {
		display_error(_('Program name cannot be empty.'));
		set_focus('prog_name');
		return false;
	}
	if (!check_num('reward_value', 0)) {
		display_error(_('Reward value must be a valid number >= 0.'));
		set_focus('reward_value');
		return false;
	}
	if (get_post('reward_type') === 'percentage_discount' && input_num('reward_value') > 100) {
		display_error(_('Percentage discount cannot exceed 100%.'));
		set_focus('reward_value');
		return false;
	}
	return true;
}

/**
 * Validate coupon generation form fields.
 *
 * @return bool True if valid
 */
function can_process_coupon_generation() {
	if (!check_num('coupon_count', 1)) {
		display_error(_('Number of coupons must be at least 1.'));
		set_focus('coupon_count');
		return false;
	}
	return true;
}

// ============================================================================
// CRUD OPERATIONS
// ============================================================================

if ($Mode == 'ADD_ITEM' && can_process_discount_program()) {
	$options = array(
		'applicable_items'      => get_post('applicable_items', ''),
		'applicable_categories' => get_post('applicable_categories', ''),
		'applicable_customers'  => get_post('applicable_customers', ''),
		'applicable_sales_types'=> get_post('applicable_sales_types', ''),
		'reward_max_amount'     => input_num('reward_max_amount'),
		'usage_limit'           => (int)get_post('usage_limit', 0),
		'per_customer_limit'    => (int)get_post('per_customer_limit', 0),
		'stackable'             => check_value('stackable') ? 1 : 0,
		'priority'              => (int)get_post('priority', 10),
	);
	add_discount_program(
		trim(get_post('prog_name')),
		get_post('program_type', 'automatic'),
		get_post('date_start', ''),
		get_post('date_end', ''),
		input_num('min_order_amount'),
		input_num('min_quantity'),
		get_post('reward_type', 'percentage_discount'),
		input_num('reward_value'),
		$options
	);
	display_notification(_('New discount program has been added.'));
	$Mode = 'RESET';
}

if ($Mode == 'UPDATE_ITEM' && can_process_discount_program()) {
	$update_data = array(
		'name'                  => trim(get_post('prog_name')),
		'program_type'          => get_post('program_type'),
		'date_start'            => get_post('date_start', ''),
		'date_end'              => get_post('date_end', ''),
		'min_order_amount'      => input_num('min_order_amount'),
		'min_quantity'          => input_num('min_quantity'),
		'applicable_items'      => get_post('applicable_items', ''),
		'applicable_categories' => get_post('applicable_categories', ''),
		'applicable_customers'  => get_post('applicable_customers', ''),
		'applicable_sales_types'=> get_post('applicable_sales_types', ''),
		'reward_type'           => get_post('reward_type'),
		'reward_value'          => input_num('reward_value'),
		'reward_max_amount'     => input_num('reward_max_amount'),
		'usage_limit'           => (int)get_post('usage_limit', 0),
		'per_customer_limit'    => (int)get_post('per_customer_limit', 0),
		'stackable'             => check_value('stackable') ? 1 : 0,
		'priority'              => (int)get_post('priority', 10),
	);
	update_discount_program($selected_id, $update_data);
	display_notification(_('Discount program has been updated.'));
	$Mode = 'RESET';
}

if ($Mode == 'Delete') {
	if (!delete_discount_program($selected_id))
		display_error(_('Only draft programs can be deleted. Use Cancel status to disable active programs.'));
	else
		display_notification(_('Discount program and its coupons have been deleted.'));
	$Mode = 'RESET';
}

// Activate / cancel program status changes
if (isset($_POST['ActivateProgram']) && $selected_id > 0) {
	update_discount_program($selected_id, array('status' => 'active'));
	display_notification(_('Discount program has been activated.'));
	$Mode = 'RESET';
}

if (isset($_POST['CancelProgram']) && $selected_id > 0) {
	update_discount_program($selected_id, array('status' => 'cancelled'));
	display_notification(_('Discount program has been cancelled.'));
	$Mode = 'RESET';
}

// Generate coupons sub-action
if (isset($_POST['GenerateCoupons']) && $selected_id > 0 && can_process_coupon_generation()) {
	$count = max(1, (int)get_post('coupon_count', 1));
	$prefix = trim(get_post('coupon_prefix', ''));
	$cust_id = (int)get_post('coupon_customer', 0);
	$valid_from  = get_post('coupon_valid_from', '');
	$valid_until = get_post('coupon_valid_until', '');
	$usage_limit = max(1, (int)get_post('coupon_usage_limit', 1));
	$created = generate_coupons($selected_id, $count, $prefix, $cust_id, $valid_from, $valid_until, $usage_limit);
	display_notification(sprintf(_('%d coupon(s) generated successfully.'), $created));
}

if ($Mode == 'RESET') {
	$selected_id = -1;
	$sav = get_post('show_inactive');
	unset($_POST);
	$_POST['show_inactive'] = $sav;
}

// ============================================================================
// PROGRAM LIST
// ============================================================================

start_form();
start_table(TABLESTYLE, "width='100%'");

$th = array(_('Name'), _('Type'), _('Status'), _('Date Start'), _('Date End'),
	_('Reward'), _('Uses'), _('Priority'), '', '');
inactive_control_column($th);
table_header($th);

$result = get_discount_programs(false); // show all including inactive
$k = 0;

while ($myrow = db_fetch($result)) {
	alt_table_row_color($k);

	label_cell($myrow['name']);
	label_cell(ucfirst($myrow['program_type']));

	$status_color = array(
		'draft'     => 'style="color:gray"',
		'active'    => 'style="color:green;font-weight:bold"',
		'expired'   => 'style="color:orange"',
		'cancelled' => 'style="color:red"',
	);
	$color = isset($status_color[$myrow['status']]) ? $status_color[$myrow['status']] : '';
	echo "<td $color>" . ucfirst($myrow['status']) . '</td>';

	label_cell($myrow['date_start'] ? sql2date($myrow['date_start']) : '-');
	label_cell($myrow['date_end']   ? sql2date($myrow['date_end'])   : '-');

	if ($myrow['reward_type'] === 'percentage_discount')
		label_cell(number_format2($myrow['reward_value'], 1) . '%');
	elseif ($myrow['reward_type'] === 'fixed_discount')
		amount_cell($myrow['reward_value']);
	else
		label_cell(ucfirst(str_replace('_', ' ', $myrow['reward_type'])));

	label_cell($myrow['usage_limit'] > 0
		? $myrow['usage_count'] . ' / ' . $myrow['usage_limit']
		: $myrow['usage_count'] . ' / &infin;');
	label_cell($myrow['priority']);

	inactive_control_cell($myrow['id'], $myrow['inactive'], 'sales_discount_programs', 'id');
	edit_button_cell('Edit' . $myrow['id'], _('Edit'));
	delete_button_cell('Delete' . $myrow['id'], _('Delete'));

	end_row();
}

inactive_control_row($th);
end_table(1);

// ============================================================================
// PROGRAM FORM
// ============================================================================

display_heading($selected_id == -1 ? _('New Discount Program') : _('Edit Discount Program'));

start_table(TABLESTYLE2);

if ($selected_id != -1 && $Mode == 'Edit') {
	$myrow = get_discount_program($selected_id);
	$_POST['prog_name']             = $myrow['name'];
	$_POST['program_type']          = $myrow['program_type'];
	$_POST['date_start']            = $myrow['date_start'] ? sql2date($myrow['date_start']) : '';
	$_POST['date_end']              = $myrow['date_end']   ? sql2date($myrow['date_end'])   : '';
	$_POST['min_order_amount']      = price_format($myrow['min_order_amount']);
	$_POST['min_quantity']          = qty_format($myrow['min_quantity']);
	$_POST['applicable_items']      = $myrow['applicable_items'];
	$_POST['applicable_categories'] = $myrow['applicable_categories'];
	$_POST['applicable_customers']  = $myrow['applicable_customers'];
	$_POST['applicable_sales_types']= $myrow['applicable_sales_types'];
	$_POST['reward_type']           = $myrow['reward_type'];
	$_POST['reward_value']          = price_format($myrow['reward_value']);
	$_POST['reward_max_amount']     = price_format($myrow['reward_max_amount']);
	$_POST['usage_limit']           = $myrow['usage_limit'];
	$_POST['per_customer_limit']    = $myrow['per_customer_limit'];
	$_POST['stackable']             = $myrow['stackable'];
	$_POST['priority']              = $myrow['priority'];
	hidden('selected_id', $selected_id);
}

// Set defaults for new form
if (!isset($_POST['priority']))        $_POST['priority']        = 10;
if (!isset($_POST['program_type']))    $_POST['program_type']    = 'automatic';
if (!isset($_POST['reward_type']))     $_POST['reward_type']     = 'percentage_discount';
if (!isset($_POST['min_order_amount']))$_POST['min_order_amount']= price_format(0);
if (!isset($_POST['min_quantity']))    $_POST['min_quantity']    = qty_format(0);
if (!isset($_POST['reward_value']))    $_POST['reward_value']    = price_format(0);
if (!isset($_POST['reward_max_amount']))$_POST['reward_max_amount'] = price_format(0);
if (!isset($_POST['usage_limit']))     $_POST['usage_limit']     = 0;
if (!isset($_POST['per_customer_limit'])) $_POST['per_customer_limit'] = 0;

text_row_ex(_('Program Name') . ':', 'prog_name', 60);

$prog_types = array(
	'automatic' => _('Automatic (applies without coupon code)'),
	'volume'    => _('Volume Discount (qty-based automatic)'),
	'coupon'    => _('Coupon (requires coupon code entry)'),
	'loyalty'   => _('Loyalty (tracked customer points)'),
);
array_selector_row(_('Program Type') . ':', 'program_type', get_post('program_type', 'automatic'), $prog_types);

date_row(_('Start Date (empty = any)') . ':', 'date_start', '', true);
date_row(_('End Date (empty = no expiry)') . ':', 'date_end', '', true);

amount_row(_('Minimum Order Amount (0 = any)') . ':', 'min_order_amount');
amount_row(_('Minimum Quantity (0 = any)') . ':', 'min_quantity', null, null, null, user_qty_dec());

$reward_types = array(
	'percentage_discount' => _('Percentage Discount (%)'),
	'fixed_discount'      => _('Fixed Amount Discount'),
	'free_product'        => _('Free Product'),
	'free_shipping'       => _('Free Shipping'),
);
array_selector_row(_('Reward Type') . ':', 'reward_type', get_post('reward_type', 'percentage_discount'), $reward_types);

amount_row(_('Reward Value (% or amount)') . ':', 'reward_value');
amount_row(_('Maximum Discount Amount (0 = unlimited)') . ':', 'reward_max_amount');

small_amount_row(_('Usage Limit (0 = unlimited)') . ':', 'usage_limit', null, null, null, 0);
small_amount_row(_('Per Customer Limit (0 = unlimited)') . ':', 'per_customer_limit', null, null, null, 0);
small_amount_row(_('Priority (lower = higher priority)') . ':', 'priority', null, null, null, 0);

check_row(_('Stackable (can be combined with other discounts)') . ':', 'stackable', get_post('stackable', 0));

start_row();
label_cell(_('Applicable Items (comma-separated stock IDs, empty = all)') . ':');
text_cells_ex(null, 'applicable_items', 70, 500, null, null);
end_row();

start_row();
label_cell(_('Applicable Categories (comma-separated category IDs, empty = all)') . ':');
text_cells_ex(null, 'applicable_categories', 40, 200, null, null);
end_row();

start_row();
label_cell(_('Applicable Customers (comma-separated debtor IDs, empty = all)') . ':');
text_cells_ex(null, 'applicable_customers', 40, 200, null, null);
end_row();

start_row();
label_cell(_('Applicable Sales Types (comma-separated type IDs, empty = all)') . ':');
text_cells_ex(null, 'applicable_sales_types', 40, 100, null, null);
end_row();

end_table(1);

submit_add_or_update_center($selected_id == -1, '', 'both');

// Activate/Cancel buttons for edit mode
if ($selected_id != -1) {
	$prog_row = get_discount_program($selected_id);
	echo '<br />';
	if ($prog_row['status'] !== 'active')
		submit_center('ActivateProgram', _('Activate Program'), true, _('Set status to Active'), 'default');
	if ($prog_row['status'] === 'active')
		submit_center('CancelProgram', _('Cancel Program'), true, _('Set status to Cancelled'), false);
}

end_form();

// ============================================================================
// COUPON MANAGEMENT (visible when editing a coupon-type program)
// ============================================================================

if ($selected_id != -1) {
	$prog_row = get_discount_program($selected_id);
	if (in_array($prog_row['program_type'], array('coupon', 'loyalty'))) {
		display_heading(_('Coupons'));
		echo '<p>' . sprintf(_('Program: <strong>%s</strong>'), htmlspecialchars($prog_row['name'])) . '</p>';

		// Coupon list
		$coupon_result = get_coupons_for_program($selected_id);
		if (db_num_rows($coupon_result) > 0) {
			start_table(TABLESTYLE, "width='80%'");
			$cth = array(_('Code'), _('Customer'), _('Valid From'), _('Valid Until'), _('Used'), _('Limit'), _('Active'));
			table_header($cth);
			$kk = 0;
			while ($crow = db_fetch($coupon_result)) {
				alt_table_row_color($kk);
				label_cell('<strong>' . htmlspecialchars($crow['code']) . '</strong>');
				label_cell($crow['debtor_no'] > 0 ? htmlspecialchars($crow['customer_name']) : _('All'));
				label_cell($crow['valid_from']  ? sql2date($crow['valid_from'])  : '-');
				label_cell($crow['valid_until'] ? sql2date($crow['valid_until']) : '-');
				label_cell($crow['usage_count']);
				label_cell($crow['usage_limit'] > 0 ? $crow['usage_limit'] : '&infin;');
				label_cell($crow['is_active'] ? _('Yes') : _('No'));
				end_row();
			}
			end_table(1);
		} else {
			display_note(_('No coupons generated yet.'));
		}

		// Coupon generation form
		display_heading2(_('Generate New Coupons'));
		start_form();
		hidden('selected_id', $selected_id);
		start_table(TABLESTYLE2);

		small_amount_row(_('Number of Coupons') . ':', 'coupon_count', 10, null, null, 0);
		text_row_ex(_('Code Prefix (optional)') . ':', 'coupon_prefix', 10);
		small_amount_row(_('Usage Limit per Coupon') . ':', 'coupon_usage_limit', 1, null, null, 0);
		customer_list_row(_('Restrict to Customer (optional)') . ':', 'coupon_customer', 0, true);
		date_row(_('Valid From (optional)') . ':', 'coupon_valid_from', '', true);
		date_row(_('Valid Until (optional)') . ':', 'coupon_valid_until', '', true);

		end_table(1);

		submit_center('GenerateCoupons', _('Generate Coupons'), true, _('Generate the specified number of coupon codes'), 'default');
		end_form();
	}

	// ============================================================================
	// USAGE REPORT for this program
	// ============================================================================
	display_heading2(_('Usage Report'));

	$today = today();
	$from_date = get_post('usage_from', add_months($today, -1));
	$to_date   = get_post('usage_to',   $today);

	start_form();
	hidden('selected_id', $selected_id);
	start_table(TABLESTYLE_NOBORDER);
	start_row();
	label_cell(_('From:'));
	date_cells(null, 'usage_from', '', $from_date);
	label_cell('&nbsp;' . _('To:'));
	date_cells(null, 'usage_to', '', $to_date);
	echo '<td>';
	submit('ShowUsage', _('Show'), true, false, 'default');
	echo '</td>';
	end_row();
	end_table(1);

	if (isset($_POST['ShowUsage'])) {
		$usage_result = get_discount_usage_report($from_date, $to_date, $selected_id);
		if (db_num_rows($usage_result) > 0) {
			start_table(TABLESTYLE, "width='90%'");
			$uth = array(_('Date'), _('Customer'), _('Trans Type'), _('Trans No'), _('Coupon'), _('Amount'));
			table_header($uth);
			$ku = 0;
			$grand_total = 0;
			while ($urow = db_fetch($usage_result)) {
				alt_table_row_color($ku);
				label_cell(date_format2($urow['applied_date'], user_date_format()));
				label_cell(htmlspecialchars($urow['customer_name']));
				label_cell(systypes::name($urow['trans_type']));
				label_cell($urow['trans_no']);
				label_cell($urow['coupon_code'] ? htmlspecialchars($urow['coupon_code']) : '-');
				amount_cell($urow['discount_amount']);
				end_row();
				$grand_total += $urow['discount_amount'];
			}
			start_row();
			label_cell('<strong>' . _('Total Discount Given') . '</strong>', 'colspan=5 align=right');
			amount_cell($grand_total, true);
			end_row();
			end_table(1);
		} else {
			display_note(_('No usage found in the selected period.'));
		}
	}

	end_form();
}

end_page();
