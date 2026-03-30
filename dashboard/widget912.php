<?php

$widget = new Widget();
$widget->setTitle(_('Doc Expiry'));
$widget->Start();

if($widget->checkSecurity('SA_EMPLOYEE')) {
	$today = date2sql(Today());
	$sql = "SELECT COUNT(*)
		FROM ".TB_PREF."employee_documents
		WHERE expiry_date IS NOT NULL
			AND expiry_date <> '0000-00-00'
			AND expiry_date >= '".$today."'
			AND expiry_date <= DATE_ADD('".$today."', INTERVAL 30 DAY)";
	$result = db_query($sql, _('Could not count expiring documents'));
	$row = db_fetch_row($result);
	$value = $row ? (int)$row[0] : 0;
	render_dashboard_small_stat_card(_('Doc Expiry'), $value, 'clock', 'info');
}

$widget->End();