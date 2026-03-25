<?php

$widget = new Widget();
$widget->setTitle(_('Overdue Invoices'));
$widget->Start();

if($widget->checkSecurity('SA_SALESTRANSVIEW')) {
	$value = dashboard_count_overdue_sales_invoices();
	render_dashboard_small_stat_card(_('Overdue Invoices'), $value, 'alert-triangle', 'danger');
}

$widget->End();