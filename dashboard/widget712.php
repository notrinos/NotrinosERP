<?php

$widget = new Widget();
$widget->setTitle(_('Payables'));
$widget->Start();

if($widget->checkSecurity('SA_GLANALYTIC')) {
	$value = dashboard_count_payables();
	render_dashboard_small_stat_card(_('Payables'), $value, 'book', 'info');
}

$widget->End();