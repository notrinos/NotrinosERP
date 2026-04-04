<?php
/**
 * CRM Dashboard Widget 1004 — Leads by Source (chart).
 */
$pg = new graph();
$result = dashboard_crm_leads_by_source();
$title = _('Leads by Source');
$i = 0;

if ($result) {
	while ($myrow = db_fetch($result)) {
		if ($pg != null) {
			$pg->x[$i] = $myrow['source_name'];
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
		source_graphic($title, _('Source'), $pg, _('Leads'), null, 5);
	else
		display_note(_('No lead source data to display.'));
}

$widget->End();
