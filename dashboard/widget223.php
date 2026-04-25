<?php

$widget = new Widget();
$widget->setTitle(_('Pending Invoice Exposure'));
$widget->Start();

if ($widget->checkSecurity('SA_PURCHDASHBOARD')) {
	$data = get_purchase_dashboard_data(begin_fiscalyear(), Today());
	start_table(TABLESTYLE2, "width='100%'");
	label_row(_('Pending Invoice Lines'), (int)$data['pending_invoice_count']);
	label_row(_('Pending Invoice Value'), price_format($data['pending_invoice_value']));
	end_table();
}

$widget->End();
