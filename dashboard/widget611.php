<?php

$widget = new Widget();
$widget->setTitle(_('Dimensions'));
$widget->Start();

if($widget->checkSecurity('SA_DIMTRANSVIEW')) {
	$value = dashboard_count_dimensions_type1();
	render_dashboard_small_stat_card(_('Dimensions'), $value, 'map', 'primary');
}

$widget->End();