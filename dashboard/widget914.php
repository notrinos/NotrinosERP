<?php

$widget = new Widget();
$widget->setTitle(_('Unpaid Salaries'));
$widget->Start();

if($widget->checkSecurity('SA_EMPLOYEE')) {
	$value = dashboard_count_unpaid_salaries_periods();
	render_dashboard_small_stat_card(_('Unpaid Salaries'), $value, 'alert-triangle', 'danger');
}

$widget->End();