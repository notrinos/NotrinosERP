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

$page_security = 'SA_PROCUREMENTPLAN';
$path_to_root = '..';

include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/includes/date_functions.inc');
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/includes/ui/ui_lists.inc');
include_once($path_to_root . '/purchasing/includes/purchasing_db.inc');

$js = '';
if (user_use_date_picker())
	$js .= get_js_date_picker();

page(_($help_context = 'Procurement Plan'), false, false, '', $js);

/**
 * Validate procurement plan generation form fields.
 *
 * @return bool
 */
function can_generate_procurement_plan()
{
	$plan_type = get_post('plan_type', 'auto_reorder');
	if (!in_array($plan_type, array('auto_reorder', 'demand_based', 'manual'))) {
		display_error(_('Select a valid procurement plan type.'));
		return false;
	}

	return true;
}

/**
 * Validate approve line fields.
 *
 * @param int $line_id
 * @return bool
 */
function can_approve_plan_line($line_id)
{
	if ((int)$line_id <= 0)
		return false;

	if (input_num('approve_qty_' . $line_id) < 0) {
		display_error(_('Approved quantity must be zero or greater.'));
		set_focus('approve_qty_' . $line_id);
		return false;
	}

	return true;
}

$generate_plan = isset($_POST['GeneratePlan']);
$create_pos = isset($_POST['CreatePOs']);
$approve_line_id = find_submit('ApproveLine');
$skip_line_id = find_submit('SkipLine');

if ($generate_plan && can_generate_procurement_plan()) {
	$generated_plan_id = generate_procurement_plan(
		get_post('plan_type', 'auto_reorder'),
		get_post('filter_location', '')
	);

	if ($generated_plan_id)
		display_notification(sprintf(_('Procurement plan %s has been generated.'), format_procurement_plan_reference($generated_plan_id)));
	else
		display_warning(_('No replenishment suggestions were available to generate a procurement plan.'));

	$_POST['selected_plan_id'] = $generated_plan_id ? $generated_plan_id : 0;
}

if ($approve_line_id > 0 && can_approve_plan_line($approve_line_id)) {
	approve_plan_line(
		$approve_line_id,
		input_num('approve_qty_' . $approve_line_id),
		(int)get_post('approve_supplier_' . $approve_line_id)
	);
	display_notification(_('Plan line has been approved.'));
}

if ($skip_line_id > 0) {
	skip_plan_line($skip_line_id, trim(get_post('skip_reason_' . $skip_line_id)));
	display_notification(_('Plan line has been skipped.'));
}

if ($create_pos) {
	$selected_plan_id = (int)get_post('selected_plan_id', 0);
	if ($selected_plan_id <= 0)
		display_error(_('Select a procurement plan before creating purchase orders.'));
	else {
		$po_numbers = create_pos_from_plan($selected_plan_id);
		if (empty($po_numbers))
			display_warning(_('No approved lines were available to create purchase orders.'));
		else
			display_notification(sprintf(_('Created purchase orders: %s'), implode(', ', $po_numbers)));
	}
}

$selected_plan_id = (int)get_post('selected_plan_id', 0);
$open_plan_id = find_submit('OpenPlan');
if ($open_plan_id > 0)
	$selected_plan_id = $open_plan_id;

$filter_status = get_post('filter_status', '');
$filter_date_from = get_post('filter_date_from', begin_month(Today()));
$filter_date_to = get_post('filter_date_to', Today());
$filter_location = get_post('filter_location', '');

$plans = get_procurement_plans(
	$filter_status,
	$filter_date_from !== '' ? date2sql($filter_date_from) : '',
	$filter_date_to !== '' ? date2sql($filter_date_to) : ''
);

$selected_plan = $selected_plan_id > 0 ? get_procurement_plan($selected_plan_id) : false;

start_form();

start_table(TABLESTYLE2, "width='100%'");
start_row();
	label_cell(_('Plan Type:'));
	echo "<td>" . array_selector('plan_type', get_post('plan_type', 'auto_reorder'), array(
		'auto_reorder' => _('Auto Reorder'),
		'demand_based' => _('Demand Based'),
		'manual' => _('Manual')
	), array('class' => array('nosearch'))) . "</td>";
	locations_list_cells(_('Warehouse:'), 'filter_location', $filter_location, true, true);
	submit_cells('GeneratePlan', _('Generate Plan'), '', _('Build a procurement plan from replenishment suggestions'), 'default');
end_row();
start_row();
	label_cell(_('Status:'));
	echo "<td>" . array_selector('filter_status', $filter_status, array('' => _('All Statuses')) + get_procurement_plan_statuses(), array('class' => array('nosearch'))) . "</td>";
	date_cells(_('From:'), 'filter_date_from', $filter_date_from);
	date_cells(_('To:'), 'filter_date_to', $filter_date_to);
	submit_cells('SearchPlans', _('Search Plans'), '', '', 'default');
end_row();
end_table(1);

hidden('selected_plan_id', $selected_plan_id);

display_heading(_('Procurement Plans'));
start_table(TABLESTYLE, "width='100%'");
$th = array(
	_('Reference'),
	_('Date'),
	_('Type'),
	_('Status'),
	_('Lines'),
	_('Estimated Total'),
	''
);
table_header($th);

$k = 0;
while ($plan = db_fetch($plans)) {
	alt_table_row_color($k);
	label_cell($plan['reference']);
	label_cell(sql2date($plan['plan_date']));
	label_cell(ucwords(str_replace('_', ' ', $plan['plan_type'])));
	label_cell(isset(get_procurement_plan_statuses()[$plan['status']]) ? get_procurement_plan_statuses()[$plan['status']] : $plan['status']);
	label_cell((int)$plan['line_count'], 'align=right');
	amount_cell((float)$plan['estimated_total']);
	submit_cells('OpenPlan' . $plan['id'], _('Open'));
	end_row();
}

if ($k == 0)
	label_row('', _('No procurement plans matched the selected filters.'), 'colspan=7 align=center');

end_table(2);

if ($selected_plan) {
	display_heading(sprintf(_('Plan Details: %s'), $selected_plan['reference']));
	start_table(TABLESTYLE2, "width='100%'");
	start_row();
		label_cell(_('Created By:'));
		label_cell($selected_plan['created_by_name'] ? $selected_plan['created_by_name'] : '-');
		label_cell(_('Line Count:'));
		label_cell((int)$selected_plan['line_count']);
		label_cell(_('Estimated Total:'));
		amount_cell((float)$selected_plan['estimated_total']);
	end_row();
	start_row();
		label_cell(_('Pending / Approved / Ordered / Skipped:'));
		label_cell(
			(int)$selected_plan['pending_count'] . ' / ' .
			(int)$selected_plan['approved_count'] . ' / ' .
			(int)$selected_plan['ordered_count'] . ' / ' .
			(int)$selected_plan['skipped_count'],
			"colspan='5'"
		);
	end_row();
	end_table(1);

	$lines = get_plan_lines($selected_plan_id);
	start_table(TABLESTYLE, "width='100%'");
	$th = array(
		_('Item'),
		_('Location'),
		_('Current Stock'),
		_('Suggested Qty'),
		_('Supplier'),
		_('Estimated Price'),
		_('Priority'),
		_('Status'),
		_('Actions')
	);
	table_header($th);

	$k = 0;
	while ($line = db_fetch($lines)) {
		alt_table_row_color($k);
		label_cell($line['stock_id'] . ' - ' . ($line['stock_description'] ? $line['stock_description'] : ''));
		label_cell($line['location_name'] ? $line['location_name'] : $line['location']);
		amount_cell((float)$line['current_stock']);
		amount_cell((float)$line['suggested_order_qty']);

		if ($line['status'] === 'pending' || $line['status'] === 'approved')
			supplier_list_cells(null, 'approve_supplier_' . $line['id'], (int)$line['supplier_id'], true, true);
		else
			label_cell($line['supplier_name'] ? $line['supplier_name'] : '-');

		amount_cell((float)$line['estimated_price']);
		label_cell(ucfirst($line['priority']));
		label_cell(isset(get_procurement_plan_line_statuses()[$line['status']]) ? get_procurement_plan_line_statuses()[$line['status']] : $line['status']);

		echo '<td nowrap>';
		if ($line['status'] === 'pending' || $line['status'] === 'approved') {
			amount_cells(null, 'approve_qty_' . $line['id'], (float)$line['suggested_order_qty'], null, null, 7);
			submit('ApproveLine' . $line['id'], _('Approve'), false, _('Approve this line'));
			echo '&nbsp;';
			text_cells(null, 'skip_reason_' . $line['id'], '', 10, 40);
			submit('SkipLine' . $line['id'], _('Skip'), false, _('Skip this line'));
		} else {
			label_cell($line['po_number'] > 0 ? sprintf(_('PO #%s'), $line['po_number']) : '-');
		}
		echo '</td>';

		end_row();
	}
	end_table(1);

	submit_center('CreatePOs', _('Create Purchase Orders From Approved Lines'));
}

end_form();
end_page();
