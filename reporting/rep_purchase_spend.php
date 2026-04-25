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
$path_to_root = '..';

include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/purchasing/includes/purchasing_db.inc');

page(_($help_context = 'Purchase Spend Analysis'));

$date_from = get_post('date_from', begin_fiscalyear());
$date_to = get_post('date_to', Today());
$dimension_id = (int)get_post('dimension_id', 0);

$dashboard_data = get_purchase_dashboard_data($date_from, $date_to);
$vendor_rows = get_vendor_spend_analysis($date_from, $date_to, 25);
$item_rows = get_category_spend_analysis($date_from, $date_to);
$budget_variance = get_purchase_budget_variance($date_from, $date_to, $dimension_id);

start_form();

start_table(TABLESTYLE_NOBORDER);
start_row();
date_cells(_('From:'), 'date_from', $date_from);
date_cells(_('To:'), 'date_to', $date_to);
dimensions_list_cells(_('Dimension:'), 'dimension_id', $dimension_id, true, ' ', false, 1);
submit_cells('RefreshSpend', _('Apply Filter'), '', _('Refresh spend analysis'), 'default');
end_row();
end_table(1);

start_table(TABLESTYLE2, "width='100%'");
label_row(_('Total Spend:'), price_format($dashboard_data['total_spend']));
label_row(_('Open PO Value:'), price_format($dashboard_data['open_po_value']));
label_row(_('Average PO Value:'), price_format($dashboard_data['avg_po_value']));
label_row(_('Budget Amount:'), price_format($budget_variance['budget_amount']));
label_row(_('Actual Amount:'), price_format($budget_variance['actual_amount']));
label_row(_('Budget Variance:'), price_format($budget_variance['variance_amount']) . ' (' . number_format2($budget_variance['variance_pct'], 2) . '%)');
end_table(2);

display_heading(_('Vendor Spend Ranking'));
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
if ($k == 0)
	label_row('', _('No vendor spend rows were found for this period.'), 'colspan=3 align=center');
end_table(1);

display_heading(_('Item Spend Ranking'));
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
if ($k == 0)
	label_row('', _('No item spend rows were found for this period.'), 'colspan=3 align=center');
end_table(1);

end_form();
end_page();
