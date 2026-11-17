<?php
/**
 * DIM-BUD-004: dashboard tile with the number of cost budgets at or over their limit threshold.
 */
include_once dirname(__DIR__).'/tracking/includes/tracking_budget_alert.inc';

$widget = new Widget();
$widget->setTitle(_('Budgets at the limit'));
$widget->Start();

if ($widget->checkSecurity('SA_DIMTRANSVIEW')) {
	$alerts = dimension_alert_evaluate(0);
	if ($alerts['state'] === 'available')
		render_dashboard_small_stat_card(_('Budgets at the limit'), (int)$alerts['counts']['limit'], 'activity', 'danger');
	else
		echo '<p>'.dimension_alert_h(dimension_alert_state_note($alerts)).'</p>';
}

$widget->End();
