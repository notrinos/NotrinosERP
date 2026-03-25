<?php

$widget = new Widget();
$widget->setTitle(_('Branches'));
$widget->Start();

if($widget->checkSecurity('SA_SALESTRANSVIEW')) {
	$value = dashboard_count_customer_branches();
	render_dashboard_small_stat_card(_('Branches'), $value, 'map', 'info');
}

$widget->End();