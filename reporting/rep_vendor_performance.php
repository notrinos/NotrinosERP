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

page(_($help_context = 'Vendor Performance'));

$date_from = get_post('date_from', begin_fiscalyear());
$date_to = get_post('date_to', Today());
$supplier_id = (int)get_post('supplier_id', 0);
$stock_id = get_post('stock_id', '');

$lead_time_rows = get_lead_time_analysis($date_from, $date_to, $supplier_id);
$vendor_rows = get_vendor_spend_analysis($date_from, $date_to, 50);
$comparison_rows = $stock_id !== '' ? get_vendor_comparison_report($stock_id, $date_from, $date_to) : array();

start_form();

start_table(TABLESTYLE_NOBORDER);
start_row();
date_cells(_('From:'), 'date_from', $date_from);
date_cells(_('To:'), 'date_to', $date_to);
supplier_list_cells(_('Supplier:'), 'supplier_id', $supplier_id, true, true);
stock_items_list_cells(_('Item for Vendor Comparison:'), 'stock_id', $stock_id, true);
submit_cells('RefreshPerformance', _('Apply Filter'), '', _('Refresh vendor performance report'), 'default');
end_row();
end_table(1);

display_heading(_('Lead Time Performance'));
start_table(TABLESTYLE, "width='100%'");
table_header(array(_('Supplier'), _('Receipts'), _('Avg Lead Days'), _('Min Lead Days'), _('Max Lead Days')));
$k = 0;
foreach ($lead_time_rows as $row) {
	alt_table_row_color($k);
	label_cell($row['supp_name']);
	label_cell((int)$row['receipt_count'], 'align=right');
	amount_cell((float)$row['avg_lead_days']);
	amount_cell((float)$row['min_lead_days']);
	amount_cell((float)$row['max_lead_days']);
	end_row();
}
if ($k == 0)
	label_row('', _('No lead time data was found for the selected criteria.'), 'colspan=5 align=center');
end_table(1);

display_heading(_('Spend Contribution by Vendor'));
start_table(TABLESTYLE, "width='100%'");
table_header(array(_('Supplier'), _('Invoices'), _('Total Spend')));
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

display_heading(_('Vendor Comparison for Selected Item'));
start_table(TABLESTYLE, "width='100%'");
table_header(array(_('Supplier'), _('Invoice Lines'), _('Quantity'), _('Avg Price'), _('Min Price'), _('Max Price')));
$k = 0;
foreach ($comparison_rows as $row) {
	alt_table_row_color($k);
	label_cell($row['supp_name']);
	label_cell((int)$row['invoice_line_count'], 'align=right');
	amount_cell((float)$row['total_quantity']);
	amount_cell((float)$row['avg_price']);
	amount_cell((float)$row['min_price']);
	amount_cell((float)$row['max_price']);
	end_row();
}
if ($k == 0)
	label_row('', $stock_id === '' ? _('Select an item to compare vendor pricing.') : _('No vendor comparison rows were found for this item.'), 'colspan=6 align=center');
end_table(1);

end_form();
end_page();
