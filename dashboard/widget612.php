<?php

include_once(dirname(__DIR__).'/tracking/includes/tracking_category_policy.inc');

// DIM-NAM-002: the widget counts the tracking values of category 2 and is named by the company label of that category.
$widget = new Widget();
$widget->setTitle(htmlspecialchars(dimension_category_policy_label(2), ENT_QUOTES, 'UTF-8'));
$widget->Start();

if($widget->checkSecurity('SA_DIMTRANSVIEW')) {
	$value = dashboard_count_dimensions_type2();
	render_dashboard_small_stat_card(dimension_category_policy_label(2), $value, 'map', 'info');
}

$widget->End();