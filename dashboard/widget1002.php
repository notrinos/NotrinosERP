<?php
/**
 * CRM Dashboard Widget 1002 — Pipeline by Stage (chart).
 */
$pg = new graph();
$result = dashboard_crm_pipeline_by_stage();
$title = _('Pipeline by Stage');
$i = 0;

if ($result) {
	while ($myrow = db_fetch($result)) {
		if ($pg != null) {
			$pg->x[$i] = $myrow['stage_name'];
			$pg->y[$i] = abs($myrow['total']);
		}
		$i++;
	}
}

$widget = new Widget();
$widget->setTitle($title);
$widget->Start();

if ($widget->checkSecurity('SA_CRM_PIPELINE')) {
	if ($i > 0)
		source_graphic($title, _('Stage'), $pg, _('Deals'), null, 5);
	else
		display_note(_('No pipeline data to display.'));
}

$widget->End();
