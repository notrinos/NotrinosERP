<?php

$widget = new Widget();
$widget->setTitle(_('Invoices'));
$widget->Start();

if($widget->checkSecurity('SA_SUPPTRANSVIEW')) {
	$value = dashboard_count_purchase_invoices();
	render_dashboard_small_stat_card(_('Invoices'), $value, 'file', 'success');
}

$widget->End();