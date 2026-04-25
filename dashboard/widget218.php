<?php

$widget = new Widget();
$widget->setTitle(_('Matching Exceptions'));
$widget->Start();

if ($widget->checkSecurity('SA_PURCHDASHBOARD')) {
	$data = get_purchase_dashboard_data(begin_fiscalyear(), Today());
	$tone = (int)$data['matching_exceptions'] > 0 ? 'danger' : 'success';
	render_dashboard_small_stat_card(_('Exceptions'), (int)$data['matching_exceptions'], 'alert-triangle', $tone);
}

$widget->End();
