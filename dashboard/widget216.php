<?php

$widget = new Widget();
$widget->setTitle(_('Open PO Value'));
$widget->Start();

if ($widget->checkSecurity('SA_PURCHDASHBOARD')) {
	$data = get_purchase_dashboard_data(begin_fiscalyear(), Today());
	render_dashboard_small_stat_card(_('Open POs'), price_format($data['open_po_value']), 'package', 'warning');
}

$widget->End();
