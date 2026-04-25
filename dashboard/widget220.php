<?php

$widget = new Widget();
$widget->setTitle(_('Top Vendors by Spend'));
$widget->Start();

if ($widget->checkSecurity('SA_PURCHDASHBOARD')) {
	$rows = get_vendor_spend_analysis(begin_fiscalyear(), Today(), 8);
	start_table(TABLESTYLE, "width='100%'");
	table_header(array(_('Supplier'), _('Spend')));
	$k = 0;
	foreach ($rows as $row) {
		alt_table_row_color($k);
		label_cell($row['supp_name']);
		amount_cell((float)$row['total_spend']);
		end_row();
	}
	if ($k == 0)
		label_row('', _('No vendor spend data found.'), 'colspan=2 align=center');
	end_table();
}

$widget->End();
