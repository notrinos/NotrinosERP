<?php

$widget = new Widget();
$widget->setTitle(_('Departments'));
$widget->Start();

if($widget->checkSecurity('SA_EMPLOYEE')) {
	$sql = "SELECT COUNT(*) FROM ".TB_PREF."departments";
	$result = db_query($sql, _('Could not count departments'));
	$row = db_fetch_row($result);
	$value = $row ? (int)$row[0] : 0;
	render_dashboard_small_stat_card(_('Departments'), $value, 'building', 'success');
}

$widget->End();