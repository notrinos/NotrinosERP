<?php

$widget = new Widget();
$widget->setTitle(_('Type 2 Dimensions'));
$widget->Start();

if($widget->checkSecurity('SA_DIMTRANSVIEW')) {
	$value = dashboard_count_dimensions_type2();
	render_dashboard_small_stat_card(_('Type 2 Dimensions'), $value, 'map', 'info');
}

$widget->End();