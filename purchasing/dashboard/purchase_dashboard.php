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

$page_security = 'SA_PURCHDASHBOARD';
$path_to_root = '../..';

include_once($path_to_root . '/includes/session.inc');
include_once($path_to_root . '/includes/ui.inc');
include_once($path_to_root . '/includes/date_functions.inc');
include_once($path_to_root . '/purchasing/includes/purchasing_db.inc');

page(_($help_context = 'Purchase Dashboard'));

/**
 * Render one KPI card for the dashboard.
 *
 * @param string $label
 * @param string $value
 * @param string $subtext
 * @param string $tone
 * @return void
 */
function render_purchase_kpi_card($label, $value, $subtext = '', $tone = 'default')
{
	$border = '#dbe4ee';
	$bg = '#f8fafc';
	if ($tone === 'warning') {
		$border = '#f8d7a3';
		$bg = '#fff8ef';
	} elseif ($tone === 'danger') {
		$border = '#f1c2c2';
		$bg = '#fff3f3';
	} elseif ($tone === 'success') {
		$border = '#bfe6d0';
		$bg = '#f2fbf6';
	}

	echo '<div style="min-width:180px;padding:12px 14px;background:' . $bg . ';border:1px solid ' . $border . ';border-radius:4px;">';
	echo '<div style="font-size:12px;color:#64748b;">' . $label . '</div>';
	echo '<div style="font-size:22px;font-weight:bold;color:#0f172a;line-height:1.35;">' . $value . '</div>';
	if ($subtext !== '')
		echo '<div style="font-size:11px;color:#64748b;margin-top:4px;">' . $subtext . '</div>';
	echo '</div>';
}

$date_from = get_post('date_from', begin_fiscalyear());
$date_to = get_post('date_to', Today());

$dashboard_data = get_purchase_dashboard_data($date_from, $date_to);

start_form();

start_table(TABLESTYLE_NOBORDER);
start_row();
date_cells(_('From:'), 'date_from', $date_from);
date_cells(_('To:'), 'date_to', $date_to);
submit_cells('RefreshDashboard', _('Apply Filter'), '', _('Refresh purchase dashboard metrics'), 'default');
end_row();
end_table(1);

echo '<div style="display:flex;gap:10px;flex-wrap:wrap;margin-bottom:15px;">';
render_purchase_kpi_card(
	_('Total Spend'),
	price_format($dashboard_data['total_spend']),
	sprintf(_('Vs previous period: %s%%'), number_format2($dashboard_data['spend_change_pct'], 2)),
	$dashboard_data['spend_change_pct'] <= 0 ? 'success' : 'warning'
);
render_purchase_kpi_card(_('Open POs'), price_format($dashboard_data['open_po_value']), _('Outstanding ordered value'), 'warning');
render_purchase_kpi_card(_('Pending GRNs'), (int)$dashboard_data['pending_grn_count'], price_format($dashboard_data['pending_grn_value']) . ' ' . _('value'), 'warning');
render_purchase_kpi_card(_('Pending Invoices'), (int)$dashboard_data['pending_invoice_count'], price_format($dashboard_data['pending_invoice_value']) . ' ' . _('value'), 'warning');
render_purchase_kpi_card(_('Matching Exceptions'), (int)$dashboard_data['matching_exceptions'], _('Open exception records'), $dashboard_data['matching_exceptions'] > 0 ? 'danger' : 'success');
render_purchase_kpi_card(_('Reorder Alerts'), (int)$dashboard_data['reorder_alerts'], _('Items currently flagged'), $dashboard_data['reorder_alerts'] > 0 ? 'warning' : 'success');
render_purchase_kpi_card(_('Average Lead Time'), number_format2($dashboard_data['lead_time_average_days'], 2) . ' ' . _('days'), _('PO to GRN cycle average'));
render_purchase_kpi_card(_('Average PO Value'), price_format($dashboard_data['avg_po_value']), sprintf(_('%d purchase orders in period'), (int)$dashboard_data['po_count']));
echo '</div>';

display_heading(_('Spend Trend (Last 12 Months)'));
start_table(TABLESTYLE, "width='100%'");
table_header(array(_('Period'), _('Spend'), _('Visual')));
$max_spend = 0;
foreach ($dashboard_data['spend_trend'] as $trend_row) {
	if ((float)$trend_row['period_spend'] > $max_spend)
		$max_spend = (float)$trend_row['period_spend'];
}

$k = 0;
foreach ($dashboard_data['spend_trend'] as $trend_row) {
	$period_spend = (float)$trend_row['period_spend'];
	$bar_pct = $max_spend > 0 ? (($period_spend / $max_spend) * 100) : 0;
	alt_table_row_color($k);
	label_cell($trend_row['period_key']);
	amount_cell($period_spend);
	label_cell('<div style="height:12px;background:#e8eef4;border-radius:2px;overflow:hidden;"><div style="height:12px;width:' . round($bar_pct, 2) . '%;background:#3a7bd5;"></div></div>');
	end_row();
}
if ($k == 0)
	label_row('', _('No spend trend data is available for the selected period.'), 'colspan=3 align=center');
end_table(1);

display_heading(_('Top Vendors by Spend'));
start_table(TABLESTYLE, "width='100%'");
table_header(array(_('Supplier'), _('Invoices'), _('Spend')));
$k = 0;
foreach ($dashboard_data['top_vendors'] as $vendor_row) {
	alt_table_row_color($k);
	label_cell($vendor_row['supp_name']);
	label_cell((int)$vendor_row['invoice_count'], 'align=right');
	amount_cell((float)$vendor_row['total_spend']);
	end_row();
}
if ($k == 0)
	label_row('', _('No vendor spend data is available for the selected period.'), 'colspan=3 align=center');
end_table(1);

display_heading(_('Top Items by Spend'));
start_table(TABLESTYLE, "width='100%'");
table_header(array(_('Item'), _('Quantity'), _('Spend')));
$k = 0;
foreach ($dashboard_data['top_items'] as $item_row) {
	alt_table_row_color($k);
	label_cell($item_row['stock_id'] . ' - ' . $item_row['item_description']);
	amount_cell((float)$item_row['total_quantity']);
	amount_cell((float)$item_row['total_spend']);
	end_row();
}
if ($k == 0)
	label_row('', _('No item spend data is available for the selected period.'), 'colspan=3 align=center');
end_table(1);

display_heading(_('Quick Actions'));
start_table(TABLESTYLE_NOBORDER);
start_row();
echo '<td>';
hyperlink_params($path_to_root . '/purchasing/purch_requisition_entry.php', _('New Requisition'), 'sel_app=AP&New=1');
echo '</td>';
echo '<td>';
hyperlink_params($path_to_root . '/purchasing/purch_rfq_entry.php', _('New RFQ'), 'sel_app=AP&New=1');
echo '</td>';
echo '<td>';
hyperlink_params($path_to_root . '/purchasing/po_entry_items.php', _('New Purchase Order'), 'sel_app=AP&NewOrder=Yes');
echo '</td>';
echo '<td>';
hyperlink_params($path_to_root . '/purchasing/inquiry/reorder_status.php', _('View Reorder Status'), 'sel_app=AP');
echo '</td>';
end_row();
end_table(1);

end_form();
end_page();
