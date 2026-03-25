<?php

$widget = new Widget();
$widget->setTitle(_('Dimensions Total Balance'));
$widget->Start();

if($widget->checkSecurity('SA_DIMTRANSVIEW')) {
	$value = dashboard_dimensions_total_balance_text();
	render_dashboard_small_stat_card(_('Dimensions Total Balance'), $value, 'database', 'success');
}

$widget->End();