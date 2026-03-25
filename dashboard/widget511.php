<?php

$widget = new Widget();
$widget->setTitle(_('Fixed Assets'));
$widget->Start();

if($widget->checkSecurity('SA_ASSETSANALYTIC')) {
	$value = dashboard_count_fixed_assets();
	render_dashboard_small_stat_card(_('Fixed Assets'), $value, 'building', 'primary');
}

$widget->End();