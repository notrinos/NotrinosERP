<?php

$widget = new Widget();
$widget->setTitle(_('Pending GRNs'));
$widget->Start();

if ($widget->checkSecurity('SA_PURCHDASHBOARD')) {
	$data = get_purchase_dashboard_data(begin_fiscalyear(), Today());
	render_dashboard_small_stat_card(_('Pending GRN'), (int)$data['pending_grn_count'], 'truck', 'warning');
}

$widget->End();
