<?php
/**
 * CRM Dashboard Widget 1013 — Pipeline Value (stat card).
 */
$widget = new Widget();
$widget->setTitle(_('Pipeline Value'));
$widget->Start();

if ($widget->checkSecurity('SA_CRM_PIPELINE')) {
	$value = dashboard_crm_pipeline_value();
	$formatted = number_format2($value, user_price_dec());
	render_dashboard_small_stat_card(_('Pipeline Value'), $formatted, 'dollar-sign', 'success');
}

$widget->End();
