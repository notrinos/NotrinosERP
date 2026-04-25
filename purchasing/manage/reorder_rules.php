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

$page_security = 'SA_REORDERRULES';
$path_to_root = '../..';

include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/includes/ui/ui_lists.inc');
include_once($path_to_root . '/purchasing/includes/purchasing_db.inc');

page(_($help_context = 'Purchasing Reorder Rules Overlay'));

/**
 * Reset the reorder overlay form fields.
 *
 * @return void
 */
function reset_reorder_overlay_form()
{
	$_POST['selected_rule_id'] = 0;
	$_POST['overlay_supplier_id'] = 0;
	$_POST['overlay_auto_create_rfq'] = 0;
	$_POST['overlay_auto_create_po'] = 0;
	$_POST['overlay_lead_time_days'] = 0;
}

/**
 * Populate overlay form fields from one rule row.
 *
 * @param array $rule
 * @return void
 */
function load_reorder_overlay_form($rule)
{
	$_POST['selected_rule_id'] = (int)$rule['rule_id'];
	$_POST['overlay_supplier_id'] = (int)$rule['effective_supplier_id'];
	$_POST['overlay_auto_create_rfq'] = (int)$rule['auto_create_rfq'];
	$_POST['overlay_auto_create_po'] = (int)$rule['auto_create_po'];
	$_POST['overlay_lead_time_days'] = (int)$rule['lead_time_days'];
}

/**
 * Validate reorder overlay inputs.
 *
 * @return bool
 */
function can_save_reorder_overlay()
{
	if ((int)get_post('selected_rule_id') <= 0) {
		display_error(_('Select a replenishment rule first.'));
		return false;
	}

	if (!check_num('overlay_lead_time_days', 0)) {
		display_error(_('Lead time days must be zero or greater.'));
		set_focus('overlay_lead_time_days');
		return false;
	}

	return true;
}

if (!isset($_POST['selected_rule_id']))
	reset_reorder_overlay_form();

$selected_rule_id = (int)get_post('selected_rule_id', 0);
$edit_rule_id = find_submit('EditRule');

if (!empty($_POST))
	$Ajax->activate('_page_body');

if ($edit_rule_id > 0)
	$selected_rule_id = $edit_rule_id;

if (isset($_POST['SaveOverlay']) && can_save_reorder_overlay()) {
	set_preferred_supplier_for_rule(
		(int)get_post('selected_rule_id'),
		(int)get_post('overlay_supplier_id'),
		check_value('overlay_auto_create_rfq') ? 1 : 0,
		check_value('overlay_auto_create_po') ? 1 : 0,
		(int)input_num('overlay_lead_time_days')
	);
	display_notification(_('Reorder rule purchasing overlay has been updated.'));
	$selected_rule_id = (int)get_post('selected_rule_id');
}

if (isset($_POST['ResetOverlay'])) {
	$selected_rule_id = 0;
	reset_reorder_overlay_form();
}

$filter_stock = get_post('filter_stock', '');
$filter_location = get_post('filter_location', '');
$filter_supplier = (int)get_post('filter_supplier', 0);
$show_inactive = check_value('show_inactive');

if ($selected_rule_id > 0) {
	$rule_result = get_purchasing_reorder_rules('', '', 0, 1);
	while ($rule = db_fetch($rule_result)) {
		if ((int)$rule['rule_id'] === $selected_rule_id) {
			load_reorder_overlay_form($rule);
			break;
		}
	}
}

start_form(true);

start_table(TABLESTYLE2, "width='100%'");
start_row();
	stock_costable_items_list_cells(_('Item:'), 'filter_stock', $filter_stock, true, true);
	locations_list_cells(_('Warehouse:'), 'filter_location', $filter_location, true, true);
	supplier_list_cells(_('Preferred Supplier:'), 'filter_supplier', $filter_supplier, true, true);
	check_cells(_('Show Inactive Rules:'), 'show_inactive', $show_inactive);
	submit_cells('SearchRules', _('Apply Filter'), '', '', 'default');
end_row();
end_table(1);

$rules = get_purchasing_reorder_rules($filter_stock, $filter_location, $filter_supplier, $show_inactive ? 1 : 0);

display_heading(_('Purchasing Reorder Rule Overlay List'));
start_table(TABLESTYLE, "width='100%'");
$th = array(
	_('Rule'),
	_('Type'),
	_('Item'),
	_('Warehouse'),
	_('Stock vs Trigger'),
	_('Preferred Supplier'),
	_('Auto PO'),
	_('Auto RFQ'),
	_('Lead Time'),
	'',
);
table_header($th);

$k = 0;
while ($rule = db_fetch($rules)) {
	$current_stock = 0;
	$trigger_label = '-';
	if ($rule['stock_id'] !== '' && $rule['warehouse_loc_code'] !== '') {
		$current_stock = get_replenishment_on_hand_qty($rule['stock_id'], $rule['warehouse_loc_code']);
		if ($rule['rule_type'] === 'min_max')
			$trigger_label = number_format2($current_stock, get_qty_dec($rule['stock_id'])) . ' / ' . number_format2((float)$rule['min_qty'], get_qty_dec($rule['stock_id']));
		elseif ($rule['rule_type'] === 'reorder_point')
			$trigger_label = number_format2($current_stock, get_qty_dec($rule['stock_id'])) . ' / ' . number_format2((float)$rule['reorder_qty'], get_qty_dec($rule['stock_id']));
		else
			$trigger_label = number_format2($current_stock, get_qty_dec($rule['stock_id']));
	}

	alt_table_row_color($k);
	label_cell($rule['rule_name']);
	label_cell(get_replenishment_rule_type_label($rule['rule_type']));
	label_cell($rule['stock_id'] !== '' ? $rule['stock_id'] : _('Category/Global'));
	label_cell($rule['warehouse_name'] ? $rule['warehouse_name'] : '-');
	label_cell($trigger_label);
	label_cell($rule['supplier_name'] ? $rule['supplier_name'] : '-');
	label_cell((int)$rule['auto_create_po'] ? _('Yes') : _('No'));
	label_cell((int)$rule['auto_create_rfq'] ? _('Yes') : _('No'));
	label_cell((int)$rule['lead_time_days'] . ' ' . _('days'));
	edit_button_cell('EditRule' . $rule['rule_id'], _('Edit'));
	end_row();
}

if ($k == 0)
	label_row('', _('No replenishment rules matched the selected filters.'), 'colspan=10 align=center');

end_table(2);

if ((int)get_post('selected_rule_id') > 0) {
	display_heading(_('Edit Purchasing Overlay Fields'));
	start_table(TABLESTYLE2, "width='70%'");
	start_row();
		label_cell(_('Selected Rule:'));
		label_cell('#' . (int)get_post('selected_rule_id'));
	end_row();
	start_row();
		supplier_list_cells(_('Preferred Supplier:'), 'overlay_supplier_id', get_post('overlay_supplier_id'), true, true);
		text_cells(_('Lead Time Days:'), 'overlay_lead_time_days', get_post('overlay_lead_time_days'), 8, 8);
	end_row();
	start_row();
		check_cells(_('Auto Create RFQ:'), 'overlay_auto_create_rfq', get_post('overlay_auto_create_rfq'));
		check_cells(_('Auto Create PO:'), 'overlay_auto_create_po', get_post('overlay_auto_create_po'));
	end_row();
	end_table(1);
	hidden('selected_rule_id', get_post('selected_rule_id'));
	submit_center_first('SaveOverlay', _('Save Overlay'));
	submit_center_last('ResetOverlay', _('Reset'));
}

end_form();
end_page();
