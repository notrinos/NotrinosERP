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

page(_($help_context = 'Purchase Price Variance'));

$date_from = get_post('date_from', begin_fiscalyear());
$date_to = get_post('date_to', Today());
$variance_rows = get_price_variance_report($date_from, $date_to);
$savings_summary = get_purchase_savings_report($date_from, $date_to);

start_form();

start_table(TABLESTYLE_NOBORDER);
start_row();
date_cells(_('From:'), 'date_from', $date_from);
date_cells(_('To:'), 'date_to', $date_to);
submit_cells('RefreshVariance', _('Apply Filter'), '', _('Refresh purchase variance report'), 'default');
end_row();
end_table(1);

start_table(TABLESTYLE2, "width='100%'");
label_row(_('Total Savings:'), price_format($savings_summary['total_savings']));
label_row(_('Total Overspend:'), price_format($savings_summary['total_overspend']));
label_row(_('Net Variance:'), price_format($savings_summary['total_variance']));
label_row(_('Variance Lines:'), (int)$savings_summary['line_count']);
end_table(2);

display_heading(_('Price Variance Details'));
start_table(TABLESTYLE, "width='100%'");
table_header(array(
	_('Supplier'),
	_('Item'),
	_('Actual Avg Price'),
	_('Reference Avg Price'),
	_('Variance Amount'),
	_('Variance %'),
	_('Quantity')
));

$k = 0;
foreach ($variance_rows as $row) {
	alt_table_row_color($k);
	label_cell($row['supp_name']);
	label_cell($row['stock_id'] . ' - ' . $row['item_description']);
	amount_cell((float)$row['avg_actual_price']);
	amount_cell((float)$row['avg_reference_price']);
	amount_cell((float)$row['avg_variance_amount']);
	label_cell(number_format2((float)$row['avg_variance_pct'], 2) . '%', 'align=right');
	amount_cell((float)$row['total_quantity']);
	end_row();
}
if ($k == 0)
	label_row('', _('No price variance rows were found for the selected period.'), 'colspan=7 align=center');
end_table(1);

end_form();
end_page();
