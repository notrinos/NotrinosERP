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
    Phase 7: Sales Analytics Dashboard.
    Shows KPIs, charts, and quick actions for sales performance.
*/

$page_security = 'SA_SALESDASHBOARD';
$path_to_root = '../..';

include_once($path_to_root.'/includes/session.inc');
include_once($path_to_root.'/includes/date_functions.inc');
include_once($path_to_root.'/includes/ui.inc');
include_once($path_to_root.'/sales/includes/db/sales_analytics_db.inc');

$js = '';
if (user_use_date_picker())
    $js .= get_js_date_picker();
if ($SysPrefs->use_popup_windows)
    $js .= get_js_open_window(800, 500);

page(_($help_context = 'Sales Dashboard'), false, false, '', $js);
echo "<link rel='stylesheet' type='text/css' href='".$path_to_root."/themes/default/local_style/sales_dashboard.css?v=20260426a'>";

// -----------------------------------------------------------------------
// Default date range: last 30 days
// -----------------------------------------------------------------------
$date_to = isset($_POST['date_to']) ? date2sql($_POST['date_to']) : date2sql(Today());
$date_from = isset($_POST['date_from']) ? date2sql($_POST['date_from']) : date('Y-m-d', strtotime($date_to . ' -30 days'));

if (strtotime($date_from) === false || strtotime($date_to) === false || $date_from > $date_to) {
    $date_to = date2sql(Today());
    $date_from = date('Y-m-d', strtotime($date_to . ' -30 days'));
}

// -----------------------------------------------------------------------
// Fetch dashboard data
// -----------------------------------------------------------------------
$data = get_sales_dashboard_data($date_from, $date_to);

$revenue_change_class = $data['revenue_change_pct'] >= 0 ? 'positive' : 'negative';
$revenue_change_icon  = $data['revenue_change_pct'] >= 0 ? 'trending-up' : 'trending-down';
$revenue_change_sign  = $data['revenue_change_pct'] >= 0 ? '+' : '';

echo "<div class='sales-dashboard'>";
echo "<div class='sales-dashboard-header'>";
echo "<div class='sales-dashboard-header-main'>";
echo "<h3 class='sales-dashboard-title'>"._('Sales Performance Overview')."</h3>";
echo "<p class='sales-dashboard-subtitle'>"._('Track revenue, conversion, receivables, and pipeline performance from one workspace.')."</p>";
echo "</div>";
echo "<div class='sales-dashboard-period-chip'>";
echo default_theme_icon('calendar')." ".sprintf(_('Period: %s to %s'), sql2date($date_from), sql2date($date_to));
echo "</div>";
echo "</div>";

// -----------------------------------------------------------------------
// Date filter form
// -----------------------------------------------------------------------
echo "<div class='sales-dashboard-filters'>";
echo "<form method='post' action='' class='sales-dashboard-filters-form'>";
echo "<label class='sales-dashboard-filter'>";
echo "<span class='sales-dashboard-filter-label'>"._('From')."</span>";
echo "<input type='text' name='date_from' class='datepicker' value='".sql2date($date_from)."' size='10'>";
echo "</label>";
echo "<label class='sales-dashboard-filter'>";
echo "<span class='sales-dashboard-filter-label'>"._('To')."</span>";
echo "<input type='text' name='date_to' class='datepicker' value='".sql2date($date_to)."' size='10'>";
echo "</label>";
echo "<button type='submit' class='sales-dashboard-refresh-btn'>".default_theme_icon('refresh-cw')." "._('Refresh Dashboard')."</button>";
echo "</form>";
echo "</div>";

// -----------------------------------------------------------------------
// KPI Cards Row
// -----------------------------------------------------------------------
echo "<div class='sales-dashboard-kpi-grid'>";

// Revenue KPI
echo "<div class='sales-dashboard-kpi-card'>";
echo "<div class='sales-dashboard-kpi-icon is-primary'>".default_theme_icon('dollar-sign')."</div>";
echo "<div class='sales-dashboard-kpi-content'>";
echo "<div class='sales-dashboard-kpi-value'>".price_format($data['current_revenue'])."</div>";
echo "<div class='sales-dashboard-kpi-label'>"._('Total Revenue')."</div>";
echo "<div class='sales-dashboard-kpi-change $revenue_change_class'>"
    .default_theme_icon($revenue_change_icon)." "
    .$revenue_change_sign.$data['revenue_change_pct'].'% '._('vs previous period')
    ."</div>";
echo "</div></div>";

// Orders KPI
echo "<div class='sales-dashboard-kpi-card'>";
echo "<div class='sales-dashboard-kpi-icon is-info'>".default_theme_icon('shopping-cart')."</div>";
echo "<div class='sales-dashboard-kpi-content'>";
echo "<div class='sales-dashboard-kpi-value'>".number_format($data['order_count'], 0)."</div>";
echo "<div class='sales-dashboard-kpi-label'>"._('Orders')."</div>";
echo "<div class='sales-dashboard-kpi-change neutral'>"._('Avg').': '.price_format($data['avg_order_value'])."</div>";
echo "</div></div>";

// Conversion Rate KPI
echo "<div class='sales-dashboard-kpi-card'>";
echo "<div class='sales-dashboard-kpi-icon is-success'>".default_theme_icon('check-circle')."</div>";
echo "<div class='sales-dashboard-kpi-content'>";
echo "<div class='sales-dashboard-kpi-value'>".$data['conversion_rate'].'%' ."</div>";
echo "<div class='sales-dashboard-kpi-label'>"._('Quote Conversion')."</div>";
echo "</div></div>";

// Overdue KPI
echo "<div class='sales-dashboard-kpi-card'>";
echo "<div class='sales-dashboard-kpi-icon is-danger'>".default_theme_icon('alert-triangle')."</div>";
echo "<div class='sales-dashboard-kpi-content'>";
echo "<div class='sales-dashboard-kpi-value'>".price_format($data['total_overdue'])."</div>";
echo "<div class='sales-dashboard-kpi-label'>"._('Overdue AR')."</div>";
echo "</div></div>";

// Pipeline Value
echo "<div class='sales-dashboard-kpi-card'>";
echo "<div class='sales-dashboard-kpi-icon is-warning'>".default_theme_icon('target')."</div>";
echo "<div class='sales-dashboard-kpi-content'>";
echo "<div class='sales-dashboard-kpi-value'>".price_format($data['pipeline']['total_value'])."</div>";
echo "<div class='sales-dashboard-kpi-label'>"._('Pipeline Value')."</div>";
echo "</div></div>";

// Commission Payable
echo "<div class='sales-dashboard-kpi-card'>";
echo "<div class='sales-dashboard-kpi-icon is-info'>".default_theme_icon('user-check')."</div>";
echo "<div class='sales-dashboard-kpi-content'>";
echo "<div class='sales-dashboard-kpi-value'>".price_format($data['commission_payable'])."</div>";
echo "<div class='sales-dashboard-kpi-label'>"._('Commission Payable')."</div>";
echo "</div></div>";

// Return Rate
echo "<div class='sales-dashboard-kpi-card'>";
echo "<div class='sales-dashboard-kpi-icon is-default'>".default_theme_icon('rotate-ccw')."</div>";
echo "<div class='sales-dashboard-kpi-content'>";
echo "<div class='sales-dashboard-kpi-value'>".$data['return_rate'].'%' ."</div>";
echo "<div class='sales-dashboard-kpi-label'>"._('Return Rate')."</div>";
echo "</div></div>";

echo "</div>";  // .sales-dashboard-kpi-grid

// -----------------------------------------------------------------------
// Row: Revenue Trend Chart + Top Customers
// -----------------------------------------------------------------------
echo "<div class='sales-dashboard-grid-2col'>";

// Revenue Trend
echo "<div class='sales-dashboard-panel'>";
echo "<div class='sales-dashboard-panel-header'>"._('Revenue Trend (6 Months)')."</div>";
echo "<div class='sales-dashboard-panel-body'>";
if (count($data['revenue_trend']) > 0) {
    echo "<table class='sales-dashboard-table'>";
    echo "<thead><tr><th>"._('Period')."</th><th>"._('Revenue')."</th><th>"._('Orders')."</th></tr></thead>";
    echo "<tbody>";
    foreach ($data['revenue_trend'] as $row) {
        echo "<tr>";
        echo "<td>".htmlspecialchars($row['period'], ENT_QUOTES, 'UTF-8')."</td>";
        echo "<td class='right'>".price_format($row['revenue'])."</td>";
        echo "<td class='right'>".number_format($row['count'], 0)."</td>";
        echo "</tr>";
    }
    echo "</tbody></table>";
} else {
    echo "<div class='sales-dashboard-no-data'>"._('No revenue data for this period.')."</div>";
}
echo "</div></div>";

// Top Customers
echo "<div class='sales-dashboard-panel'>";
echo "<div class='sales-dashboard-panel-header'>"._('Top 5 Customers')."</div>";
echo "<div class='sales-dashboard-panel-body'>";
if (count($data['top_customers']) > 0) {
    echo "<table class='sales-dashboard-table'>";
    echo "<thead><tr><th>"._('Customer')."</th><th>"._('Revenue')."</th><th>"._('Orders')."</th></tr></thead>";
    echo "<tbody>";
    foreach ($data['top_customers'] as $cust) {
        echo "<tr>";
        echo "<td>".htmlspecialchars($cust['name'], ENT_QUOTES, 'UTF-8')."</td>";
        echo "<td class='right'>".price_format($cust['revenue'])."</td>";
        echo "<td class='right'>".number_format($cust['order_count'], 0)."</td>";
        echo "</tr>";
    }
    echo "</tbody></table>";
} else {
    echo "<div class='sales-dashboard-no-data'>"._('No customer data for this period.')."</div>";
}
echo "</div></div>";

echo "</div>";  // .sales-dashboard-grid-2col

// -----------------------------------------------------------------------
// Row: Top Products + Pipeline Stages
// -----------------------------------------------------------------------
echo "<div class='sales-dashboard-grid-2col'>";

// Top Products
echo "<div class='sales-dashboard-panel'>";
echo "<div class='sales-dashboard-panel-header'>"._('Top 5 Products')."</div>";
echo "<div class='sales-dashboard-panel-body'>";
if (count($data['top_products']) > 0) {
    echo "<table class='sales-dashboard-table'>";
    echo "<thead><tr><th>"._('Product')."</th><th>"._('Revenue')."</th><th>"._('Qty')."</th></tr></thead>";
    echo "<tbody>";
    foreach ($data['top_products'] as $prod) {
        echo "<tr>";
        echo "<td>".htmlspecialchars($prod['name'], ENT_QUOTES, 'UTF-8')."</td>";
        echo "<td class='right'>".price_format($prod['revenue'])."</td>";
        echo "<td class='right'>".number_format((float)$prod['qty'], 2)."</td>";
        echo "</tr>";
    }
    echo "</tbody></table>";
} else {
    echo "<div class='sales-dashboard-no-data'>"._('No product data for this period.')."</div>";
}
echo "</div></div>";

// Pipeline Stages (if CRM enabled)
echo "<div class='sales-dashboard-panel'>";
echo "<div class='sales-dashboard-panel-header'>"._('Pipeline Stages')."</div>";
echo "<div class='sales-dashboard-panel-body'>";
$pipeline_stages = $data['pipeline']['stages'];
if (count($pipeline_stages) > 0) {
    echo "<table class='sales-dashboard-table'>";
    echo "<thead><tr><th>"._('Stage')."</th><th>"._('Value')."</th><th>"._('#')."</th></tr></thead>";
    echo "<tbody>";
    foreach ($pipeline_stages as $stage) {
        $stage_label = ucfirst($stage['stage']);
        echo "<tr>";
        echo "<td>".htmlspecialchars($stage_label, ENT_QUOTES, 'UTF-8')."</td>";
        echo "<td class='right'>".price_format($stage['value'])."</td>";
        echo "<td class='right'>".number_format($stage['count'], 0)."</td>";
        echo "</tr>";
    }
    echo "</tbody></table>";
} else {
    echo "<div class='sales-dashboard-no-data'>"._('No pipeline data available. Enable CRM module for pipeline tracking.')."</div>";
}
echo "</div></div>";

echo "</div>";  // .sales-dashboard-grid-2col

// -----------------------------------------------------------------------
// Row: AR Aging Summary
// -----------------------------------------------------------------------
echo "<div class='sales-dashboard-panel'>";
echo "<div class='sales-dashboard-panel-header'>"._('AR Aging Summary')."</div>";
echo "<div class='sales-dashboard-panel-body'>";
$aging = $data['aging'];
echo "<table class='sales-dashboard-table'>";
echo "<thead><tr>
    <th>"._('Current')."</th>
    <th>"._('1-30 Days')."</th>
    <th>"._('31-60 Days')."</th>
    <th>"._('61-90 Days')."</th>
    <th>"._('90+ Days')."</th>
    <th>"._('Total Overdue')."</th>
</tr></thead>";
echo "<tbody><tr>";
echo "<td class='right'>".price_format($aging['current'])."</td>";
echo "<td class='right ".(($aging['1_30'] > 0) ? 'text-warning' : '')."'>".price_format($aging['1_30'])."</td>";
echo "<td class='right ".(($aging['31_60'] > 0) ? 'text-warning' : '')."'>".price_format($aging['31_60'])."</td>";
echo "<td class='right ".(($aging['61_90'] > 0) ? 'text-danger' : '')."'>".price_format($aging['61_90'])."</td>";
echo "<td class='right ".(($aging['90_plus'] > 0) ? 'text-danger' : '')."'>".price_format($aging['90_plus'])."</td>";
echo "<td class='right font-bold'>".price_format($data['total_overdue'])."</td>";
echo "</tr></tbody></table>";
echo "</div></div>";

// -----------------------------------------------------------------------
// Quick Actions
// -----------------------------------------------------------------------
echo "<div class='sales-dashboard-panel'>";
echo "<div class='sales-dashboard-panel-header'>"._('Quick Actions')."</div>";
echo "<div class='sales-dashboard-panel-body sales-dashboard-quick-actions'>";
echo "<a href='".$path_to_root."/sales/sales_order_entry.php?NewQuotation=Yes' class='sales-dashboard-quick-action-btn'>"
    .default_theme_icon('file-text').' '._('New Quotation')."</a>";
echo "<a href='".$path_to_root."/sales/sales_order_entry.php?NewOrder=Yes' class='sales-dashboard-quick-action-btn'>"
    .default_theme_icon('shopping-cart').' '._('New Sales Order')."</a>";
echo "<a href='".$path_to_root."/sales/inquiry/sales_orders_view.php?type=32' class='sales-dashboard-quick-action-btn'>"
    .default_theme_icon('search').' '._('View Quotations')."</a>";
echo "<a href='".$path_to_root."/reporting/reports_main.php?Class=0' class='sales-dashboard-quick-action-btn'>"
    .default_theme_icon('bar-chart-2').' '._('Sales Reports')."</a>";
echo "</div></div>";

echo "</div>"; // .sales-dashboard

end_page();
