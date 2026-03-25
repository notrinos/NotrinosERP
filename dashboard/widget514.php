<?php

$widget = new Widget();
$widget->setTitle(_('Fixed Asset Classes'));
$widget->Start();

if($widget->checkSecurity('SA_ASSETSANALYTIC')) {
	$value = dashboard_count_fixed_asset_classes();
	render_dashboard_small_stat_card(_('Fixed Asset Classes'), $value, 'book', 'danger');
}

$widget->End();