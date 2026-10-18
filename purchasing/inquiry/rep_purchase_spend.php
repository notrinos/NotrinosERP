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

$page_security = 'SA_PURCHREPORT';
$path_to_root = '../..';

include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/purchasing/includes/purchasing_db.inc');

$js = '';
if (user_use_date_picker())
	$js .= get_js_date_picker();

page(_($help_context = 'Purchase Spend Analysis'), false, false, '', $js);

$date_from = get_post('date_from', begin_fiscalyear());
$date_to = get_post('date_to', Today());
$dimension_id = get_post('dimension_id', '0');
$dimension2_id = get_post('dimension2_id', '0');
// Fail before analytics reads; a forged axis must not silently expand to All.
if (!is_scalar($date_from) || !is_scalar($date_to)
	|| !is_date($date_from) || !is_date($date_to) || date2sql($date_from) > date2sql($date_to)
	|| !is_scalar($dimension_id) || !ctype_digit((string)$dimension_id)
	|| !is_scalar($dimension2_id) || !ctype_digit((string)$dimension2_id)) {
	display_error(_('Invalid dates or tracking category filters.'));
	end_page();
	exit;
}
$dimension_id = (int)$dimension_id;
$dimension2_id = (int)$dimension2_id;
if (get_post('WholeMonths')) {
	$date_from = $_POST['date_from'] = begin_month($date_from);
	$date_to = $_POST['date_to'] = end_month($date_to);
}
foreach (array(1 => $dimension_id, 2 => $dimension2_id) as $axis => $value) {
	if ($value && !db_num_rows(db_query("SELECT id FROM " . TB_PREF . "dimensions WHERE id="
		. db_escape($value) . " AND type_=" . db_escape($axis), 'could not validate tracking filter'))) {
		display_error(_('Invalid tracking category for this axis.'));
		end_page();
		exit;
	}
}

$dashboard_data = get_purchase_dashboard_data($date_from, $date_to);
$vendor_rows = get_vendor_spend_analysis($date_from, $date_to, 25);
$item_rows = get_category_spend_analysis($date_from, $date_to);
$budget_variance = get_purchase_budget_variance($date_from, $date_to, $dimension_id, $dimension2_id);

start_form();

start_table(TABLESTYLE_NOBORDER);
start_row();
date_cells(_('From:'), 'date_from', $date_from);
date_cells(_('To:'), 'date_to', $date_to);
dimensions_list_cells(_('Category 1:'), 'dimension_id', $dimension_id, true, _('All'), true, 1);
dimensions_list_cells(_('Category 2:'), 'dimension2_id', $dimension2_id, true, _('All'), true, 2);
submit_cells('RefreshSpend', _('Apply Filter'), '', _('Refresh spend analysis'), 'default');
submit_cells('WholeMonths', _('Compare Whole Months'), '', _('Expand dates to complete calendar months'), 'default');
end_row();
end_table(1);

display_heading(_('Posted GL cost budget comparison'));
display_note(_('Company base currency; signed COGS and expense from all posting sources. Both category filters intersect. Recoverable tax, inventory and asset capitalization are excluded by account class.'));
display_note(_('Budget basis: exact selected coordinate; All uses coordinate 0 (aggregate). Other detailed or aggregate budget coordinates are not added. Use complete calendar months for budget and favorable variance.'));
start_outer_table();
table_section(1);
label_row(_('Posted GL Costs:'), price_format($budget_variance['actual_amount']));
table_section(2);
$budget_label = $budget_variance['budget_status'] === 'partial_month' ? _('N/A - select complete calendar months') : _('Not set');
label_row(_('Budget Amount:'), $budget_variance['budget_amount'] === null ? $budget_label : price_format($budget_variance['budget_amount']));
label_row(_('Favorable Variance:'), $budget_variance['variance_amount'] === null ? _('N/A') : price_format($budget_variance['variance_amount']));
label_row(_('Favorable Variance %:'), $budget_variance['variance_pct'] === null ? _('N/A') : number_format2($budget_variance['variance_pct'], 2) . '%');
end_outer_table(2);

display_heading(_('Company procurement activity - separate scope'));
display_note(_('Category filters apply only to the GL budget comparison above. Invoice and item rankings below cover the whole company and selected dates; invoice values include tax, stock and capital purchases, exclude credits, and remain in source currencies. They are not comparable GL expense. Open PO value is the current outstanding balance across all dates.'));
start_table(TABLESTYLE_NOBORDER);
label_row(_('Gross Supplier Invoices:'), price_format($dashboard_data['total_spend']));
label_row(_('Current Open PO Value:'), price_format($dashboard_data['open_po_value']));
label_row(_('Average PO Value in Period:'), price_format($dashboard_data['avg_po_value']));
end_table(1);
display_heading(_('Company Vendor Invoice Ranking (gross, including tax)'));
start_table(TABLESTYLE, "width='100%'");
table_header(array(_('Supplier'), _('Invoice Count'), _('Total Spend')));
$k = 0;
foreach ($vendor_rows as $row) {
	alt_table_row_color($k);
	label_cell($row['supp_name']);
	label_cell((int)$row['invoice_count'], 'align=right');
	amount_cell((float)$row['total_spend']);
	end_row();
}
if (empty($vendor_rows))
	label_row('', _('No vendor spend rows were found for this period.'), 'colspan=3 align=center');
end_table(1);

display_heading(_('Company Item Invoice Ranking (line values, excluding tax)'));
start_table(TABLESTYLE, "width='100%'");
table_header(array(_('Item'), _('Quantity'), _('Total Spend')));
$k = 0;
foreach ($item_rows as $row) {
	alt_table_row_color($k);
	label_cell($row['stock_id'] . ' - ' . $row['item_description']);
	amount_cell((float)$row['total_quantity']);
	amount_cell((float)$row['total_spend']);
	end_row();
}
if (empty($item_rows))
	label_row('', _('No item spend rows were found for this period.'), 'colspan=3 align=center');
end_table(1);

end_form();
end_page();
