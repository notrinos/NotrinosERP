<?php

$widget = new Widget();
$widget->setTitle(_('Company P&L Result (Fiscal YTD)'));
$widget->Start();

if($widget->checkSecurity('SA_DIMTRANSVIEW')) {
	$value = dashboard_dimensions_total_result_text();
	render_dashboard_small_stat_card(_('Company P&L Result (Fiscal YTD)'), $value, 'activity', 'danger');
}

$widget->End();
