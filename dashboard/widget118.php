<?php

include_once($path_to_root.'/sales/includes/db/sales_analytics_db.inc');

$widget = new Widget();
$widget->setTitle(_('Return Rate'));
$widget->Start();

if ($widget->checkSecurity('SA_SALESDASHBOARD')) {
    $month_start = date('Y-m-01');
    $month_end   = date('Y-m-t');
    $return_data = get_return_rate($month_start, $month_end);
    render_dashboard_small_stat_card(
        _('Return Rate'),
        number_format($return_data['rate'], 1).'%',
        'rotate-ccw',
        $return_data['rate'] > 5 ? 'danger' : 'default'
    );
}

$widget->End();
