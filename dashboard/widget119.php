<?php

include_once($path_to_root.'/sales/includes/db/sales_analytics_db.inc');

$widget = new Widget();
$widget->setTitle(_('Top Customers (MTD)'));
$widget->Start();

if ($widget->checkSecurity('SA_SALESDASHBOARD')) {
    $month_start = date('Y-m-01');
    $month_end   = date('Y-m-t');
    $top_customers = get_top_customers($month_start, $month_end, 5);

    if (count($top_customers) > 0) {
        echo "<table class='dashboard-mini-table'>";
        echo "<thead><tr><th>"._('Customer')."</th><th>"._('Revenue')."</th></tr></thead>";
        echo "<tbody>";
        foreach ($top_customers as $cust) {
            echo "<tr>";
            echo "<td>".htmlspecialchars($cust['name'], ENT_QUOTES, 'UTF-8')."</td>";
            echo "<td class='right'>".price_format($cust['revenue'])."</td>";
            echo "</tr>";
        }
        echo "</tbody></table>";
    } else {
        echo "<div class='no-data'>"._('No revenue data this month.')."</div>";
    }
}

$widget->End();
