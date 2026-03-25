<?php

$widget = new Widget();
$widget->setTitle(_('Overdue Invoices'));
$widget->Start();

if($widget->checkSecurity('SA_SUPPTRANSVIEW')) {
	$value = dashboard_count_overdue_purchase_invoices();
	render_dashboard_small_stat_card(_('Overdue Invoices'), $value, 'alert-triangle', 'danger');
}

$widget->End();