<?php

$widget = new Widget();
$widget->setTitle(_('Reorder and Risk Alerts'));
$widget->Start();

if ($widget->checkSecurity('SA_PURCHDASHBOARD')) {
	$data = get_purchase_dashboard_data(begin_fiscalyear(), Today());
	start_table(TABLESTYLE2, "width='100%'");
	label_row(_('Reorder Alerts'), (int)$data['reorder_alerts']);
	label_row(_('Matching Exceptions'), (int)$data['matching_exceptions']);
	label_row(_('Open PO Value'), price_format($data['open_po_value']));
	end_table();
}

$widget->End();
