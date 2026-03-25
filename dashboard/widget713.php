<?php

$widget = new Widget();
$widget->setTitle(_('Todays Deposits'));
$widget->Start();

if($widget->checkSecurity('SA_GLANALYTIC')) {
	$value = dashboard_count_todays_deposits();
	render_dashboard_small_stat_card(_('Todays Deposits'), $value, 'tag', 'success');
}

$widget->End();