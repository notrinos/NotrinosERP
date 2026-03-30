<?php

$widget = new Widget();
$widget->setTitle(_('Employees'));
$widget->Start();

if($widget->checkSecurity('SA_EMPLOYEE')) {
	$sql = "SELECT COUNT(*) FROM ".TB_PREF."employees";
	$result = db_query($sql, _('Could not count employees'));
	$row = db_fetch_row($result);
	$value = $row ? (int)$row[0] : 0;
	render_dashboard_small_stat_card(_('Employees'), $value, 'users', 'primary');
}

$widget->End();