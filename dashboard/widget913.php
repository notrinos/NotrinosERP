<?php

$widget = new Widget();
$widget->setTitle(_('Departments'));
$widget->Start();

if($widget->checkSecurity('SA_EMPLOYEE')) {
	$value = dashboard_count_departments();
	render_dashboard_small_stat_card(_('Departments'), $value, 'building', 'success');
}

$widget->End();