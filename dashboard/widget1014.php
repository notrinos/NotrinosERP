<?php
/**
 * CRM Dashboard Widget 1014 — Won Opportunities Count (stat card).
 */
$widget = new Widget();
$widget->setTitle(_('Won'));
$widget->Start();

if ($widget->checkSecurity('SA_CRM_PIPELINE')) {
	$value = dashboard_count_crm_won();
	render_dashboard_small_stat_card(_('Won'), $value, 'check-circle', 'success');
}

$widget->End();
