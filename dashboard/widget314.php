<?php

$widget = new Widget();
$widget->setTitle(_('Below Reorder Level'));
$widget->Start();

if($widget->checkSecurity('SA_ITEMSTRANSVIEW')) {
	$value = dashboard_count_below_reorder_level_items();
	render_dashboard_small_stat_card(_('Below Reorder Level'), $value, 'alert-triangle', 'danger');
}

$widget->End();