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
    Phase 7: Sales Margin Analysis Report.
    Shows margin breakdown by product, customer, or salesman.
    Uses HTML page pattern (consistent with other custom reports in this codebase).
*/

$page_security = 'SA_SALESREPORT';
$path_to_root = '..';

include_once($path_to_root.'/includes/session.inc');
include_once($path_to_root.'/includes/ui.inc');
include_once($path_to_root.'/sales/includes/db/sales_analytics_db.inc');

page(_($help_context = 'Sales Margin Analysis'));

$date_from = get_post('date_from', begin_fiscalyear());
$date_to   = get_post('date_to', Today());
$group_by  = get_post('group_by', 'product');

$group_options = array('product' => _('By Product'), 'customer' => _('By Customer'), 'salesman' => _('By Salesman'));

$data = get_margin_analysis($date_from, $date_to, $group_by);

start_form();

start_table(TABLESTYLE_NOBORDER);
start_row();
date_cells(_('From:'), 'date_from', $date_from);
date_cells(_('To:'), 'date_to', $date_to);
label_cell(_('Group By:'));
echo '<td><select name="group_by">';
foreach ($group_options as $val => $label) {
    $selected = ($val == $group_by) ? ' selected="selected"' : '';
    echo '<option value="'.htmlspecialchars($val).'"'.$selected.'>'.htmlspecialchars($label).'</option>';
}
echo '</select></td>';
submit_cells('RefreshMargin', _('Apply Filter'), '', _('Refresh margin analysis'), 'default');
end_row();
end_table(1);

display_heading(_('Sales Margin Analysis'));
start_table(TABLESTYLE, "width='100%'");
table_header(array(_('Name'), _('Revenue'), _('Cost'), _('Margin'), _('Margin %')));

$total_revenue = 0;
$total_cost    = 0;
$total_margin  = 0;
$k = 0;
foreach ($data as $row) {
    alt_table_row_color($k);
    label_cell($row['group_name']);
    amount_cell($row['revenue']);
    amount_cell($row['cost']);
    amount_cell($row['margin']);
    $pct = $row['margin_pct'];
    label_cell(number_format($pct, 1).' %', 'align=right');
    end_row();
    $total_revenue += $row['revenue'];
    $total_cost    += $row['cost'];
    $total_margin  += $row['margin'];
}
if ($k == 0 && empty($data))
    label_row('', _('No sales margin data found for the selected criteria.'), 'colspan=5 align=center');

// Grand total row
start_row();
label_cell('<strong>'._('Grand Total').'</strong>');
amount_cell($total_revenue);
amount_cell($total_cost);
amount_cell($total_margin);
$grand_pct = $total_revenue > 0 ? round(($total_margin / $total_revenue) * 100, 1) : 0;
label_cell('<strong>'.number_format($grand_pct, 1).' %</strong>', 'align=right');
end_row();

end_table(1);
end_form();
end_page();
