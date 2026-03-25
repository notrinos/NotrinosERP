<?php

$widget = new Widget();
$widget->setTitle(_('Open Workorders'));
$widget->Start();

if($widget->checkSecurity('SA_WORKORDERANALYTIC')) {
	$value = dashboard_count_open_workorders();
	render_dashboard_small_stat_card(_('Open Workorders'), $value, 'clock', 'danger');
}

$widget->End();