<?php

$widget = new Widget();
$widget->setTitle(_('New Orders'));
$widget->Start();

if($widget->checkSecurity('SA_SUPPTRANSVIEW')) {
	$value = dashboard_count_open_purchase_orders();
	render_dashboard_small_stat_card(_('New Orders'), $value, 'cart', 'info');
}

$widget->End();