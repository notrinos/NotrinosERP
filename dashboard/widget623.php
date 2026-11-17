<?php
/**
 * DIM-BUD-004: dashboard list of the cost budgets closest to or over their limit, with a link to the alerts page.
 */
include_once dirname(__DIR__).'/tracking/includes/tracking_budget_alert.inc';

$widget = new Widget();
$widget->setTitle(_('Budget overrun alerts'));
$widget->Start();

if ($widget->checkSecurity('SA_DIMTRANSVIEW')) {
	$alerts = dimension_alert_evaluate(0);
	if ($alerts['state'] !== 'available')
		echo '<p>'.dimension_alert_h(dimension_alert_state_note($alerts)).'</p>';
	else {
		$flagged = array();
		foreach ($alerts['rows'] as $alert_row)
			if ($alert_row['state'] === 'limit' || $alert_row['state'] === 'warning')
				$flagged[] = $alert_row;
		if (!$flagged)
			echo '<p>'.dimension_alert_h(_('No cost budget has reached its warning threshold.')).'</p>';
		else
			echo dimension_alert_rows_html($flagged, 10);
		echo '<p style="font-size:90%;">'.dimension_alert_h(dimension_alert_threshold_text($alerts['thresholds'])).' <a href="'
			.dimension_alert_h($path_to_root.'/tracking/inquiry/budget_alerts.php').'">'.dimension_alert_h(_('All budget alerts')).'</a></p>';
	}
}

$widget->End();
