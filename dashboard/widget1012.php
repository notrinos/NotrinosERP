<?php
/**
 * CRM Dashboard Widget 1012 — Active Opportunities Count (stat card).
 */
$widget = new Widget();
$widget->setTitle(_('Opportunities'));
$widget->Start();

if ($widget->checkSecurity('SA_CRM_PIPELINE')) {
	$value = dashboard_count_crm_opportunities();
	render_dashboard_small_stat_card(_('Opportunities'), $value, 'target', 'info');
}

$widget->End();
