<?php

$widget = new Widget();
$widget->setTitle(_('Salesmen'));
$widget->Start();

if($widget->checkSecurity('SA_SALESTRANSVIEW')) {
	$value = dashboard_count_salesmen();
	render_dashboard_small_stat_card(_('Salesmen'), $value, 'user', 'success');
}

$widget->End();