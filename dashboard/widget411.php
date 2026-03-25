<?php

$widget = new Widget();
$widget->setTitle(_('Assembled Items'));
$widget->Start();

if($widget->checkSecurity('SA_WORKORDERANALYTIC')) {
	$value = dashboard_count_assembled_items();
	render_dashboard_small_stat_card(_('Assembled Items'), $value, 'factory', 'primary');
}

$widget->End();