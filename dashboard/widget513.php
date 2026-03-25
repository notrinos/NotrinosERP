<?php

$widget = new Widget();
$widget->setTitle(_('Categories'));
$widget->Start();

if($widget->checkSecurity('SA_ASSETSANALYTIC')) {
	$value = dashboard_count_fixed_asset_categories();
	render_dashboard_small_stat_card(_('Categories'), $value, 'folder-plus', 'success');
}

$widget->End();