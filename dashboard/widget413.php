<?php

$widget = new Widget();
$widget->setTitle(_('Work Centres'));
$widget->Start();

if($widget->checkSecurity('SA_WORKORDERANALYTIC')) {
	$value = dashboard_count_work_centres();
	render_dashboard_small_stat_card(_('Work Centres'), $value, 'building', 'success');
}

$widget->End();