<?php

include_once($path_to_root.'/sales/includes/db/sales_analytics_db.inc');

$widget = new Widget();
$widget->setTitle(_('Revenue This Month'));
$widget->Start();

if ($widget->checkSecurity('SA_SALESDASHBOARD')) {
    $month_start = date('Y-m-01');
    $month_end   = date('Y-m-t');
    $value = get_total_revenue($month_start, $month_end);
    render_dashboard_small_stat_card(_('Revenue (MTD)'), price_format($value), 'dollar-sign', 'primary');
}

$widget->End();
