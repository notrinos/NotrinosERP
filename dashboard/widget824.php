<?php

$widget = new Widget();
$widget->setTitle(_('Database Size'));
$widget->Start();

if($widget->checkSecurity('SA_SETUPDISPLAY')) {
	$value = dashboard_get_database_size_text();
	render_dashboard_small_stat_card(_('Database Size'), $value, 'database', 'danger');
}

$widget->End();