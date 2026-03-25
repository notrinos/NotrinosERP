<?php

$widget = new Widget();
$widget->setTitle(_('Dimensions Total Result'));
$widget->Start();

if($widget->checkSecurity('SA_DIMTRANSVIEW')) {
	$value = dashboard_dimensions_total_result_text();
	render_dashboard_small_stat_card(_('Dimensions Total Result'), $value, 'activity', 'danger');
}

$widget->End();