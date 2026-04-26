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

/*
    Phase 7: Salesman Performance Report.
    Shows revenue, margin, commission, and targets per salesman.
    Uses HTML page pattern (consistent with other custom reports in this codebase).
*/

$page_security = 'SA_SALESREPORT';
$path_to_root = '..';

include_once($path_to_root.'/includes/session.inc');
include_once($path_to_root.'/includes/ui.inc');
include_once($path_to_root.'/sales/includes/db/sales_analytics_db.inc');

page(_($help_context = 'Salesman Performance'));

$date_from = get_post('date_from', begin_fiscalyear());
$date_to   = get_post('date_to', Today());

$data = get_salesman_performance_report($date_from, $date_to);

start_form();

start_table(TABLESTYLE_NOBORDER);
start_row();
date_cells(_('From:'), 'date_from', $date_from);
date_cells(_('To:'), 'date_to', $date_to);
submit_cells('RefreshPerformance', _('Apply Filter'), '', _('Refresh salesman performance'), 'default');
end_row();
end_table(1);

display_heading(_('Salesman Performance'));
start_table(TABLESTYLE, "width='100%'");
table_header(array(_('Salesman'), _('Revenue'), _('Orders'), _('Margin'), _('Margin %'), _('Commission')));

$total_revenue    = 0;
$total_orders     = 0;
$total_margin     = 0;
$total_commission = 0;
$k = 0;
foreach ($data as $row) {
    alt_table_row_color($k);
    label_cell($row['name']);
    amount_cell($row['revenue']);
    label_cell((int)$row['order_count'], 'align=right');
    amount_cell($row['margin']);
    $pct = $row['revenue'] > 0 ? round(($row['margin'] / $row['revenue']) * 100, 1) : 0;
    label_cell(number_format($pct, 1).' %', 'align=right');
    amount_cell($row['commission']);
    end_row();
    $total_revenue    += $row['revenue'];
    $total_orders     += $row['order_count'];
    $total_margin     += $row['margin'];
    $total_commission += $row['commission'];
}
if (empty($data))
    label_row('', _('No salesman performance data found for the selected criteria.'), 'colspan=6 align=center');

// Grand total row
start_row();
label_cell('<strong>'._('Grand Total').'</strong>');
amount_cell($total_revenue);
label_cell('<strong>'.(int)$total_orders.'</strong>', 'align=right');
amount_cell($total_margin);
$grand_pct = $total_revenue > 0 ? round(($total_margin / $total_revenue) * 100, 1) : 0;
label_cell('<strong>'.number_format($grand_pct, 1).' %</strong>', 'align=right');
amount_cell($total_commission);
end_row();

end_table(1);
end_form();
end_page();
