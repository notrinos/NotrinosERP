<?php

$widget = new Widget();
$widget->setTitle(_('Customers'));
$widget->Start();

if($widget->checkSecurity('SA_SALESTRANSVIEW')) {
	$value = dashboard_count_customers();
	render_dashboard_small_stat_card(_('Customers'), $value, 'users', 'primary');
}

$widget->End();