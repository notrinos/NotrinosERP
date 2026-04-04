<?php
/**
 * CRM Dashboard Widget 1011 — Active Leads Count (stat card).
 */
$widget = new Widget();
$widget->setTitle(_('Active Leads'));
$widget->Start();

if ($widget->checkSecurity('SA_CRM_PIPELINE')) {
	$value = dashboard_count_crm_leads();
	render_dashboard_small_stat_card(_('Active Leads'), $value, 'user-plus', 'primary');
}

$widget->End();
