<?php

$widget = new Widget();
$widget->setTitle(_('Locations'));
$widget->Start();

if($widget->checkSecurity('SA_ASSETSANALYTIC')) {
	$value = dashboard_count_fixed_asset_locations();
	render_dashboard_small_stat_card(_('Locations'), $value, 'map', 'info');
}

$widget->End();