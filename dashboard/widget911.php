<?php

$widget = new Widget();
$widget->setTitle(_('Employees'));
$widget->Start();

if($widget->checkSecurity('SA_EMPLOYEE')) {
	$value = dashboard_count_employees();
	render_dashboard_small_stat_card(_('Employees'), $value, 'users', 'primary');
}

$widget->End();