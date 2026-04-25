<?php

$widget = new Widget();
$widget->setTitle(_('Top Items by Spend'));
$widget->Start();

if ($widget->checkSecurity('SA_PURCHDASHBOARD')) {
	$rows = get_category_spend_analysis(begin_fiscalyear(), Today());
	start_table(TABLESTYLE, "width='100%'");
	table_header(array(_('Item'), _('Spend')));
	$k = 0;
	foreach ($rows as $row) {
		alt_table_row_color($k);
		label_cell($row['stock_id']);
		amount_cell((float)$row['total_spend']);
		end_row();
	}
	if ($k == 0)
		label_row('', _('No item spend data found.'), 'colspan=2 align=center');
	end_table();
}

$widget->End();
