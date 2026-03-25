<?php

$widget = new Widget();
$widget->setTitle(_('Suppliers'));
$widget->Start();

if($widget->checkSecurity('SA_SUPPTRANSVIEW')) {
	$value = dashboard_count_suppliers();
	render_dashboard_small_stat_card(_('Suppliers'), $value, 'users', 'primary');
}

$widget->End();