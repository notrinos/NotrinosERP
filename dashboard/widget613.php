<?php

$widget = new Widget();
$widget->setTitle(_('Tagged Balance-Sheet Net (Fiscal YTD)'));
$widget->Start();

if($widget->checkSecurity('SA_DIMTRANSVIEW')) {
	$value = dashboard_dimensions_total_balance_text();
	render_dashboard_small_stat_card(_('Tagged Balance-Sheet Net (Fiscal YTD)'), $value, 'database', 'success');
}

$widget->End();
