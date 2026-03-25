<?php

$widget = new Widget();
$widget->setTitle(_('Languages'));
$widget->Start();

if($widget->checkSecurity('SA_SETUPDISPLAY')) {
	$value = dashboard_count_installed_languages();
	render_dashboard_small_stat_card(_('Languages'), $value, 'map', 'success');
}

$widget->End();