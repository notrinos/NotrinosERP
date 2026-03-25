<?php

$widget = new Widget();
$widget->setTitle(_('Extensions'));
$widget->Start();

if($widget->checkSecurity('SA_SETUPDISPLAY')) {
	$value = dashboard_count_active_extensions();
	render_dashboard_small_stat_card(_('Extensions'), $value, 'settings-list', 'info');
}

$widget->End();