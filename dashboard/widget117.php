<?php

include_once($path_to_root.'/sales/includes/db/sales_analytics_db.inc');

$widget = new Widget();
$widget->setTitle(_('Avg Order Value'));
$widget->Start();

if ($widget->checkSecurity('SA_SALESDASHBOARD')) {
    $month_start = date('Y-m-01');
    $month_end   = date('Y-m-t');
    $value = get_avg_order_value($month_start, $month_end);
    render_dashboard_small_stat_card(_('Avg Order Value'), price_format($value), 'trending-up', 'success');
}

$widget->End();
