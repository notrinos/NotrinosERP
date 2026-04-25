<?php

$widget = new Widget();
$widget->setTitle(_('Spend This Fiscal Year'));
$widget->Start();

if ($widget->checkSecurity('SA_PURCHDASHBOARD')) {
	$data = get_purchase_dashboard_data(begin_fiscalyear(), Today());
	render_dashboard_small_stat_card(_('Spend'), price_format($data['total_spend']), 'trending-up', 'primary');
}

$widget->End();
