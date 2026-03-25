<?php

$widget = new Widget();
$widget->setTitle(_('Items'));
$widget->Start();

if($widget->checkSecurity('SA_ITEMSTRANSVIEW')) {
	$value = dashboard_count_inventory_items();
	render_dashboard_small_stat_card(_('Items'), $value, 'box', 'primary');
}

$widget->End();