<?php

$widget = new Widget();
$widget->setTitle(_('Todays Payments'));
$widget->Start();

if($widget->checkSecurity('SA_GLANALYTIC')) {
	$value = dashboard_count_todays_payments();
	render_dashboard_small_stat_card(_('Todays Payments'), $value, 'tag', 'danger');
}

$widget->End();