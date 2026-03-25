<?php

$widget = new Widget();
$widget->setTitle(_('Manufactured Items'));
$widget->Start();

if($widget->checkSecurity('SA_WORKORDERANALYTIC')) {
	$value = dashboard_count_manufactured_items();
	render_dashboard_small_stat_card(_('Manufactured Items'), $value, 'box', 'info');
}

$widget->End();