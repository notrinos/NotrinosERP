<?php

$widget = new Widget();
$widget->setTitle(_('Unpaid Salaries'));
$widget->Start();

if($widget->checkSecurity('SA_EMPLOYEE')) {
	$sql = "SELECT COUNT(*) FROM ".TB_PREF."payroll_periods
		WHERE status IN (1,2,3)
			OR status IN ('open','locked','posted','approved')";
	$result = db_query($sql, _('Could not count unpaid salary periods'));
	$row = db_fetch_row($result);
	$value = $row ? (int)$row[0] : 0;
	render_dashboard_small_stat_card(_('Unpaid Salaries'), $value, 'alert-triangle', 'danger');
}

$widget->End();