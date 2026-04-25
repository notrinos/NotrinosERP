<?php

$widget = new Widget();
$widget->setTitle(_('Lead Time by Vendor'));
$widget->Start();

if ($widget->checkSecurity('SA_PURCHDASHBOARD')) {
	$rows = get_lead_time_analysis(begin_fiscalyear(), Today(), 0);
	start_table(TABLESTYLE, "width='100%'");
	table_header(array(_('Supplier'), _('Avg Days')));
	$k = 0;
	foreach ($rows as $row) {
		alt_table_row_color($k);
		label_cell($row['supp_name']);
		amount_cell((float)$row['avg_lead_days']);
		end_row();
	}
	if ($k == 0)
		label_row('', _('No lead time data found.'), 'colspan=2 align=center');
	end_table();
}

$widget->End();
