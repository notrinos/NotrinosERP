<?php

$widget = new Widget();
$widget->setTitle(_('Kits'));
$widget->Start();

if($widget->checkSecurity('SA_ITEMSTRANSVIEW')) {
	$value = dashboard_count_item_kits();
	render_dashboard_small_stat_card(_('Kits'), $value, 'settings-list', 'success');
}

$widget->End();