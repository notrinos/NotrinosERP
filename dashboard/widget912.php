<?php

$widget = new Widget();
$widget->setTitle(_('Doc Expiry'));
$widget->Start();

if($widget->checkSecurity('SA_EMPLOYEE')) {
	$value = dashboard_count_doc_expiry_soon();
	render_dashboard_small_stat_card(_('Doc Expiry'), $value, 'clock', 'info');
}

$widget->End();