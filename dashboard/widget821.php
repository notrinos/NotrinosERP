<?php

$widget = new Widget();
$widget->setTitle(_('Users'));
$widget->Start();

if($widget->checkSecurity('SA_SETUPDISPLAY')) {
	$value = dashboard_count_users();
	render_dashboard_small_stat_card(_('Users'), $value, 'users', 'primary');
}

$widget->End();