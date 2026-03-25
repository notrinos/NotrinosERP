<?php

$widget = new Widget();
$widget->setTitle(_('Receivables'));
$widget->Start();

if($widget->checkSecurity('SA_GLANALYTIC')) {
	$value = dashboard_count_receivables();
	render_dashboard_small_stat_card(_('Receivables'), $value, 'book', 'primary');
}

$widget->End();