<?php

$widget = new Widget();
$widget->setTitle(_('Price Variance Snapshot'));
$widget->Start();

if ($widget->checkSecurity('SA_PURCHDASHBOARD')) {
	$summary = get_purchase_savings_report(begin_fiscalyear(), Today());
	start_table(TABLESTYLE2, "width='100%'");
	label_row(_('Savings'), price_format($summary['total_savings']));
	label_row(_('Overspend'), price_format($summary['total_overspend']));
	label_row(_('Net Variance'), price_format($summary['total_variance']));
	end_table();
}

$widget->End();
