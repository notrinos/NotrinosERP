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
$page_security = 'SA_SALESPRICELIST';
$path_to_root = '../..';
include_once($path_to_root . '/includes/session.inc');

page(_($help_context = 'Sales Pricelists'));

include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/sales/includes/db/sales_pricelist_db.inc');

simple_page_mode(true);
$selected_rule_id = get_post('selected_rule_id', -1);

// ============================================================================
// PRICELIST VALIDATION AND PROCESSING
// ============================================================================

function can_process_pricelist() {
	if (strlen($_POST['pricelist_name']) == 0) {
		display_error(_('Pricelist name cannot be empty.'));
		set_focus('pricelist_name');
		return false;
	}
	
	if ($_POST['applicable_to'] !== 'all' && $_POST['applicable_id'] == 0) {
		display_error(_('Please select an applicable entity.'));
		set_focus('applicable_id');
		return false;
	}
	
	return true;
}

function can_process_rule() {
	if (!isset($_POST['rule_min_qty']) || $_POST['rule_min_qty'] == '') {
		display_error(_('Minimum quantity cannot be empty.'));
		set_focus('rule_min_qty');
		return false;
	}
	
	if (!check_num('rule_min_qty', 0)) {
		display_error(_('Minimum quantity must be a valid number.'));
		set_focus('rule_min_qty');
		return false;
	}
	
	if ($_POST['rule_computation_type'] == 'fixed' && $_POST['rule_fixed_price'] == '') {
		display_error(_('Fixed price cannot be empty for fixed computation type.'));
		set_focus('rule_fixed_price');
		return false;
	}
	
	if ($_POST['rule_computation_type'] == 'percentage' && $_POST['rule_percentage'] == '') {
		display_error(_('Percentage cannot be empty for percentage computation type.'));
		set_focus('rule_percentage');
		return false;
	}
	
	if (!check_num('rule_fixed_price') || !check_num('rule_percentage') || !check_num('rule_surcharge')) {
		display_error(_('Price amounts must be valid numbers.'));
		return false;
	}
	
	return true;
}

// ============================================================================
// PRICELIST CRUD OPERATIONS
// ============================================================================

if ($Mode=='ADD_ITEM' && can_process_pricelist()) {
	$date_start = $_POST['date_start'] ? $_POST['date_start'] : null;
	$date_end = $_POST['date_end'] ? $_POST['date_end'] : null;
	$applicable_id = get_post('applicable_id', 0);
	
	add_sales_pricelist(
		$_POST['pricelist_name'],
		$_POST['currency'],
		$date_start,
		$date_end,
		input_num('priority'),
		$_POST['applicable_to'],
		$applicable_id,
		$_POST['pricelist_description']
	);
	display_notification(_('New pricelist has been added'));
	$Mode = 'RESET';
}

if ($Mode=='UPDATE_ITEM' && can_process_pricelist()) {
	$applicable_id = get_post('applicable_id', 0);
	$update_data = array(
		'name' => $_POST['pricelist_name'],
		'description' => $_POST['pricelist_description'],
		'currency' => $_POST['currency'],
		'date_start' => $_POST['date_start'] ? $_POST['date_start'] : null,
		'date_end' => $_POST['date_end'] ? $_POST['date_end'] : null,
		'priority' => input_num('priority'),
		'applicable_to' => $_POST['applicable_to'],
		'applicable_id' => $applicable_id
	);
	
	update_sales_pricelist($selected_id, $update_data);
	display_notification(_('Pricelist has been updated'));
	$Mode = 'RESET';
}

if ($Mode == 'Delete') {
	delete_sales_pricelist($selected_id);
	display_notification(_('Pricelist and all its rules have been deleted'));
	$Mode = 'RESET';
}

if ($Mode == 'RESET') {
	$selected_id = -1;
	$selected_rule_id = -1;
	$sav = get_post('show_inactive');
	unset($_POST);
	$_POST['show_inactive'] = $sav;
}

// ============================================================================
// RULE OPERATIONS (within pricelist)
// ============================================================================

if (isset($_POST['cancel_rule'])) {
	$selected_rule_id = -1;
	unset($_POST['selected_rule_id']);
	unset($_POST['rule_stock_id'], $_POST['rule_category_id'], $_POST['rule_min_qty'],
		$_POST['rule_computation_type'], $_POST['rule_fixed_price'], $_POST['rule_percentage'],
		$_POST['rule_base_price_type'], $_POST['rule_base_pricelist_id'], $_POST['rule_surcharge'],
		$_POST['rule_rounding'], $_POST['rule_priority'], $_POST['rule_date_start'], $_POST['rule_date_end']);
}

if (isset($_POST['rule_edit']) && $selected_id > 0) {
	$selected_rule_id = (int) key($_POST['rule_edit']);
	$rule_result = db_query(
		"SELECT * FROM ".TB_PREF."sales_pricelist_rules WHERE id = ".db_escape($selected_rule_id)." AND pricelist_id = ".db_escape($selected_id),
		'Pricelist rule cannot be retrieved'
	);
	if (db_num_rows($rule_result) > 0) {
		$rule_row = db_fetch_assoc($rule_result);
		$_POST['rule_stock_id'] = $rule_row['stock_id'];
		$_POST['rule_category_id'] = $rule_row['stock_category_id'];
		$_POST['rule_min_qty'] = price_format($rule_row['min_quantity']);
		$_POST['rule_computation_type'] = $rule_row['computation_type'];
		$_POST['rule_fixed_price'] = price_format($rule_row['fixed_price']);
		$_POST['rule_percentage'] = percent_format($rule_row['percentage']);
		$_POST['rule_base_price_type'] = $rule_row['base_price_type'];
		$_POST['rule_base_pricelist_id'] = $rule_row['base_pricelist_id'];
		$_POST['rule_surcharge'] = price_format($rule_row['surcharge']);
		$_POST['rule_rounding'] = price_format($rule_row['rounding']);
		$_POST['rule_priority'] = $rule_row['priority'];
		$_POST['rule_date_start'] = $rule_row['date_start'] ? sql2date($rule_row['date_start']) : '';
		$_POST['rule_date_end'] = $rule_row['date_end'] ? sql2date($rule_row['date_end']) : '';
	}
}

if (isset($_POST['add_rule']) && $selected_id > 0 && can_process_rule()) {
	add_pricelist_rule(
		$selected_id,
		$_POST['rule_stock_id'],
		input_num('rule_category_id'),
		input_num('rule_min_qty'),
		$_POST['rule_computation_type'],
		input_num('rule_fixed_price'),
		input_num('rule_percentage'),
		$_POST['rule_base_price_type'],
		input_num('rule_base_pricelist_id'),
		input_num('rule_surcharge'),
		input_num('rule_rounding'),
		input_num('rule_priority'),
		$_POST['rule_date_start'] ? $_POST['rule_date_start'] : null,
		$_POST['rule_date_end'] ? $_POST['rule_date_end'] : null
	);
	display_notification(_('Rule has been added to the pricelist'));
	unset($_POST['add_rule']);
	unset($_POST['rule_stock_id']);
	unset($_POST['rule_min_qty']);
	unset($_POST['rule_fixed_price']);
	unset($_POST['rule_percentage']);
	$selected_rule_id = -1;
}

if (isset($_POST['update_rule']) && $selected_id > 0 && $selected_rule_id > 0 && can_process_rule()) {
	update_pricelist_rule($selected_rule_id, array(
		'stock_id' => $_POST['rule_stock_id'],
		'stock_category_id' => input_num('rule_category_id'),
		'min_quantity' => input_num('rule_min_qty'),
		'computation_type' => $_POST['rule_computation_type'],
		'fixed_price' => input_num('rule_fixed_price'),
		'percentage' => input_num('rule_percentage'),
		'base_price_type' => $_POST['rule_base_price_type'],
		'base_pricelist_id' => input_num('rule_base_pricelist_id'),
		'surcharge' => input_num('rule_surcharge'),
		'rounding' => input_num('rule_rounding'),
		'priority' => input_num('rule_priority'),
		'date_start' => $_POST['rule_date_start'] ? $_POST['rule_date_start'] : null,
		'date_end' => $_POST['rule_date_end'] ? $_POST['rule_date_end'] : null
	));
	display_notification(_('Rule has been updated.'));
	$selected_rule_id = -1;
	unset($_POST['selected_rule_id']);
	unset($_POST['rule_stock_id'], $_POST['rule_category_id'], $_POST['rule_min_qty'],
		$_POST['rule_computation_type'], $_POST['rule_fixed_price'], $_POST['rule_percentage'],
		$_POST['rule_base_price_type'], $_POST['rule_base_pricelist_id'], $_POST['rule_surcharge'],
		$_POST['rule_rounding'], $_POST['rule_priority'], $_POST['rule_date_start'], $_POST['rule_date_end']);
}

if (isset($_POST['rule_delete']) && $selected_id > 0) {
	$rule_id = (int) key($_POST['rule_delete']);
	delete_pricelist_rule($rule_id);
	display_notification(_('Rule has been deleted from the pricelist'));
	if ($selected_rule_id == $rule_id) {
		$selected_rule_id = -1;
		unset($_POST['selected_rule_id']);
	}
	unset($_POST['rule_delete']);
}

// ============================================================================
// PRICELIST LIST
// ============================================================================

start_form();
start_table(TABLESTYLE, "width='100%'");

$th = array(_('Pricelist Name'), _('Currency'), _('Applicable To'), _('Date Start'), _('Date End'), _('Priority'), '', '');
inactive_control_column($th);
table_header($th);

$pricelists = get_sales_pricelists(check_value('show_inactive'));
$k = 0;


foreach ($pricelists as $myrow) {
	alt_table_row_color($k);
	
	label_cell($myrow['name']);
	label_cell($myrow['currency'] ? $myrow['currency'] : _('All'));
	label_cell($myrow['applicable_to']);
	label_cell($myrow['date_start'] ? sql2date($myrow['date_start']) : '');
	label_cell($myrow['date_end'] ? sql2date($myrow['date_end']) : '');
	label_cell($myrow['priority']);
	
	inactive_control_cell($myrow['id'], $myrow['inactive'], 'sales_pricelists', 'id');
	edit_button_cell('Edit'.$myrow['id'], _('Edit'));
	delete_button_cell('Delete'.$myrow['id'], _('Delete'));
	
	end_row();
}

inactive_control_row($th);
end_table(1);

// ============================================================================
// PRICELIST FORM
// ============================================================================

start_table(TABLESTYLE2);

if ($selected_id != -1 && $Mode == 'Edit') {
	$myrow = get_sales_pricelist($selected_id);
	
	$_POST['pricelist_name'] = $myrow['name'];
	$_POST['pricelist_description'] = $myrow['description'];
	$_POST['currency'] = $myrow['currency'];
	$_POST['date_start'] = $myrow['date_start'] ? sql2date($myrow['date_start']) : '';
	$_POST['date_end'] = $myrow['date_end'] ? sql2date($myrow['date_end']) : '';
	$_POST['priority'] = $myrow['priority'];
	$_POST['applicable_to'] = $myrow['applicable_to'];
	$_POST['applicable_id'] = $myrow['applicable_id'];
	
	hidden('selected_id', $selected_id);
}

if (!isset($_POST['priority']))
	$_POST['priority'] = 10;
if (!isset($_POST['applicable_to']))
	$_POST['applicable_to'] = 'all';
if (!isset($_POST['applicable_id']))
	$_POST['applicable_id'] = 0;

text_row_ex(_('Pricelist Name') . ':', 'pricelist_name', 40);
textarea_row(_('Description') . ':', 'pricelist_description', null, 40, 4);

currencies_list_row(_('Currency (empty = all)') . ':', 'currency', null);
amount_row(_('Priority (lower = higher priority)') . ':', 'priority', null, null, null, 0);

$applicable_options = array(
	'all' => _('All Customers'),
	'customer' => _('Specific Customer'),
	'sales_type' => _('Sales Type'),
	'customer_group' => _('Customer Group'),
	'branch' => _('Branch')
);

array_selector_row(_('Applicable To') . ':', 'applicable_to', null, $applicable_options);

if ($_POST['applicable_to'] !== 'all') {
	if ($_POST['applicable_to'] === 'customer') {
		customer_list_row(_('Customer') . ':', 'applicable_id', $_POST['applicable_id']);
	} elseif ($_POST['applicable_to'] === 'sales_type') {
		sales_types_list_row(_('Sales Type') . ':', 'applicable_id', $_POST['applicable_id']);
	} elseif ($_POST['applicable_to'] === 'customer_group') {
		sales_groups_list_row(_('Customer Group') . ':', 'applicable_id', $_POST['applicable_id']);
	} elseif ($_POST['applicable_to'] === 'branch') {
		label_row(
			_('Branch') . ':',
			combo_input(
				'applicable_id',
				$_POST['applicable_id'],
				"SELECT branch_code, CONCAT(branch_ref, ' - ', br_name) AS branch_name, inactive FROM ".TB_PREF."cust_branch",
				'branch_code',
				'branch_name',
				array(
					'where' => array('inactive = 0'),
					'order' => array('br_name'),
					'spec_option' => false,
					'sel_hint' => _('Select branch')
				)
			)
		);
	}
}

date_row(_('Date Start') . ':', 'date_start');
date_row(_('Date End') . ':', 'date_end');

end_table(1);

submit_add_or_update_center($selected_id == -1, '', 'both');

end_form();

// ============================================================================
// RULES MANAGEMENT (if editing a pricelist)
// ============================================================================

if ($selected_id != -1) {
	display_heading(_('Pricelist Rules'));
	
	start_form();
	
	start_table(TABLESTYLE, "width='100%'");
	
	$th = array(
		_('Min Qty'), _('Stock ID'), _('Computation'), _('Value'),
		_('Base Price'), _('Surcharge'), _('Rounding'), _('Priority'),
		_('Date Start'), _('Date End'), '', ''
	);
	table_header($th);
	
	$rules = get_pricelist_rules($selected_id);
	$k = 0;
	
	if (count($rules) > 0) {
		foreach ($rules as $rule) {
			alt_table_row_color($k);
			
			label_cell($rule['min_quantity']);
			label_cell($rule['stock_id'] ? $rule['stock_id'] : _('All'));
			label_cell($rule['computation_type']);
			
			if ($rule['computation_type'] == 'fixed') {
				label_cell(number_format2($rule['fixed_price'], user_price_dec()));
			} else if ($rule['computation_type'] == 'percentage') {
				label_cell(number_format2($rule['percentage'], 2) . '%');
			} else {
				label_cell(_('Formula'));
			}
			
			label_cell($rule['base_price_type']);
			label_cell(number_format2($rule['surcharge'], user_price_dec()));
			label_cell(number_format2($rule['rounding'], user_price_dec()));
			label_cell($rule['priority']);
			label_cell($rule['date_start'] ? sql2date($rule['date_start']) : '');
			label_cell($rule['date_end'] ? sql2date($rule['date_end']) : '');
			
			edit_button_cell('rule_edit['.$rule['id'].']', _('Edit'));
			delete_button_cell('rule_delete['.$rule['id'].']', _('Delete'));
			
			end_row();
		}
	} else {
		start_row();
		label_cell(_('No rules defined'), 'colspan=12 align=center');
		end_row();
	}
	
	end_table();
	hidden('selected_id', $selected_id);
	hidden('selected_rule_id', $selected_rule_id);
	
	// Rule addition form
	start_table(TABLESTYLE2);
	
	text_row_ex(_('Stock Item (empty = all)') . ':', 'rule_stock_id', 20);
	amount_row(_('Minimum Quantity') . ':', 'rule_min_qty', null, null, null, 4);
	
	$comp_types = array('fixed' => _('Fixed Price'), 'percentage' => _('Percentage'), 'formula' => _('Formula'));
	array_selector_row(_('Computation Type') . ':', 'rule_computation_type', null, $comp_types);
	
	amount_row(_('Fixed Price') . ':', 'rule_fixed_price');
	amount_row(_('Percentage Discount/Markup') . ':', 'rule_percentage', null, null, null, 2);
	
	$base_types = array('list_price' => _('List Price'), 'cost' => _('Cost'), 'other_pricelist' => _('Other Pricelist'));
	array_selector_row(_('Base Price Type') . ':', 'rule_base_price_type', null, $base_types);
	
	amount_row(_('Surcharge') . ':', 'rule_surcharge');
	amount_row(_('Rounding') . ':', 'rule_rounding', null, null, null, 4);
	amount_row(_('Priority (lower = higher priority)') . ':', 'rule_priority', null, null, null, 0);
	
	date_row(_('Date Start') . ':', 'rule_date_start');
	date_row(_('Date End') . ':', 'rule_date_end');
	
	end_table(1);

	if ($selected_rule_id == -1) {
		submit_center('add_rule', _('Add Rule'), true, '', 'both');
	} else {
		submit_center_first('update_rule', _('Update Rule'), '', 'both');
		submit_center_last('cancel_rule', _('Cancel Edit'));
	}
	
	end_form();
}

end_page();
