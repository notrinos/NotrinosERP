<?php

$widget = new Widget();
$widget->setTitle(_('Spend Trend (6 Months)'));
$widget->Start();

if ($widget->checkSecurity('SA_PURCHDASHBOARD')) {
	$rows = get_spend_trend('monthly', 6);
	start_table(TABLESTYLE, "width='100%'");
	table_header(array(_('Period'), _('Spend')));
	$k = 0;
	foreach ($rows as $row) {
		alt_table_row_color($k);
		label_cell($row['period_key']);
		amount_cell((float)$row['period_spend']);
		end_row();
	}
	if ($k == 0)
		label_row('', _('No spend trend data found.'), 'colspan=2 align=center');
	end_table();
}

$widget->End();
